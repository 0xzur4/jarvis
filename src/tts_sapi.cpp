// tts_sapi.cpp - SAPI5 wrapper. Caller owns COM init (see brain.cpp).
#define INITGUID  // instantiate CLSID_SpVoice / IID_ISpVoice in this TU
#include "tts_sapi.h"
#include <windows.h>
#include <sapi.h>

SapiTTS::~SapiTTS() { shutdown(); }

bool SapiTTS::init(std::string& error) {
    shutdown();
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&voice_);
    if (FAILED(hr) || !voice_) {
        error = "Could not create SAPI voice (no TTS voice installed?)";
        return false;
    }
    return true;
}

bool SapiTTS::speak(const std::wstring& text) {
    if (!voice_ || text.empty()) return false;
    return SUCCEEDED(voice_->Speak(text.c_str(), SPF_DEFAULT, nullptr));
}

bool SapiTTS::set_language(const std::string& lang) {
    if (!voice_) return false;
    WORD want = 0;
    if (lang == "id") want = 0x421;        // id-ID
    else if (lang == "en") want = 0x409;   // en-US
    else return true;                      // "auto": keep current voice
    const WORD want_primary = PRIMARYLANGID(want);

    ISpObjectTokenCategory* cat = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                  IID_ISpObjectTokenCategory, (void**)&cat);
    if (FAILED(hr) || !cat) return false;
    bool found = false;
    if (SUCCEEDED(cat->SetId(SPCAT_VOICES, FALSE))) {
        IEnumSpObjectTokens* en = nullptr;
        if (SUCCEEDED(cat->EnumTokens(nullptr, nullptr, &en)) && en) {
            ISpObjectToken* tok = nullptr;
            while (en->Next(1, &tok, nullptr) == S_OK) {
                LPWSTR lang_attr = nullptr;
                if (SUCCEEDED(tok->GetStringValue(L"Language", &lang_attr)) && lang_attr) {
                    // "Language" is hex, e.g. L"409" or L"421;409".
                    unsigned long v = wcstoul(lang_attr, nullptr, 16);
                    ::CoTaskMemFree(lang_attr);
                    if (PRIMARYLANGID((WORD)v) == want_primary) {
                        if (SUCCEEDED(voice_->SetVoice(tok))) found = true;
                        tok->Release();
                        break;
                    }
                }
                tok->Release();
            }
            en->Release();
        }
    }
    cat->Release();
    return found;  // false: no matching voice installed, keep previous
}

bool SapiTTS::set_voice_by_id(const std::wstring& token_id) {
    if (!voice_ || token_id.empty()) return false;
    // SpGetTokenFromId() is not declared by MinGW's sapi.h; find the token
    // by enumerating SPCAT_VOICES and comparing ids instead.
    ISpObjectToken* found = nullptr;
    ISpObjectTokenCategory* cat = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                    IID_ISpObjectTokenCategory, (void**)&cat)) && cat) {
        if (SUCCEEDED(cat->SetId(SPCAT_VOICES, FALSE))) {
            IEnumSpObjectTokens* en = nullptr;
            if (SUCCEEDED(cat->EnumTokens(nullptr, nullptr, &en)) && en) {
                ISpObjectToken* tok = nullptr;
                while (!found && en->Next(1, &tok, nullptr) == S_OK) {
                    LPWSTR id = nullptr;
                    if (SUCCEEDED(tok->GetId(&id)) && id) {
                        if (token_id == id) found = tok;  // keep the reference
                        else tok->Release();
                        ::CoTaskMemFree(id);
                    } else {
                        tok->Release();
                    }
                }
                en->Release();
            }
        }
        cat->Release();
    }
    if (!found) return false;  // token gone (voice was uninstalled)
    bool ok = SUCCEEDED(voice_->SetVoice(found));
    found->Release();
    return ok;
}

void SapiTTS::stop() {    if (voice_) voice_->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
}

void SapiTTS::shutdown() {
    if (voice_) { voice_->Release(); voice_ = nullptr; }
}
