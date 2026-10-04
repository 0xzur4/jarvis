// voice_enum.cpp - enumerate installed SAPI5 voices via SPCAT_VOICES.
#include "voice_enum.h"
#include <sapi.h>

namespace {

std::wstring lang_display_name(WORD langid) {
    wchar_t buf[128];
    // VerLanguageNameW gives e.g. L"English (United States)".
    int n = VerLanguageNameW(langid, buf, 128);
    if (n > 0) return buf;
    wchar_t hex[16];
    swprintf(hex, 16, L"0x%04X", langid);
    return hex;
}

std::wstring short_lang(const std::wstring& full) {
    // "English (United States)" -> "English"; keep as-is when no parens.
    size_t p = full.find(L" (");
    return (p == std::wstring::npos) ? full : full.substr(0, p);
}

}  // namespace

std::vector<TtsVoice> enum_tts_voices() {
    std::vector<TtsVoice> out;
    // NOTE: SpEnumTokens() is not declared by MinGW's sapi.h, so enumerate
    // through the category object directly (same pattern as SapiTTS::set_language).
    ISpObjectTokenCategory* cat = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                  IID_ISpObjectTokenCategory, (void**)&cat);
    if (FAILED(hr) || !cat) return out;
    if (SUCCEEDED(cat->SetId(SPCAT_VOICES, FALSE))) {
        IEnumSpObjectTokens* en = nullptr;
        if (SUCCEEDED(cat->EnumTokens(nullptr, nullptr, &en)) && en) {
            ISpObjectToken* tok = nullptr;
            while (en->Next(1, &tok, nullptr) == S_OK) {
        TtsVoice v;
        LPWSTR id = nullptr;
        if (SUCCEEDED(tok->GetId(&id)) && id) {
            v.token_id = id;
            ::CoTaskMemFree(id);
        }
        LPWSTR s = nullptr;
        if (SUCCEEDED(tok->GetStringValue(L"Name", &s)) && s) {
            v.name = s;
            ::CoTaskMemFree(s);
            s = nullptr;
        }
        if (SUCCEEDED(tok->GetStringValue(L"Language", &s)) && s) {
            // Hex LANGID list, e.g. L"409" or L"409;809" - use the first.
            unsigned long raw = wcstoul(s, nullptr, 16);
            ::CoTaskMemFree(s);
            s = nullptr;
            v.lang_primary = PRIMARYLANGID((WORD)raw);
            v.language = lang_display_name((WORD)raw);
        }
        if (SUCCEEDED(tok->GetStringValue(L"Gender", &s)) && s) {
            v.gender = s;  // "Male" / "Female"
            ::CoTaskMemFree(s);
            s = nullptr;
        }
                if (!v.token_id.empty() && !v.name.empty())
                    out.push_back(std::move(v));
                tok->Release();
            }
            en->Release();
        }
    }
    cat->Release();
    return out;
}

std::wstring tts_voice_display(const TtsVoice& v) {
    std::wstring d = v.name + L" (" + short_lang(v.language);
    if (!v.gender.empty()) d += L", " + v.gender;
    d += L")";
    return d;
}

int tts_voice_for_lang(const std::vector<TtsVoice>& vs, WORD primary) {
    for (size_t i = 0; i < vs.size(); ++i)
        if (vs[i].lang_primary == primary) return (int)i;
    return -1;
}
