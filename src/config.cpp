// config.cpp - INI storage + DPAPI encryption for the API key + tiny base64.
#include "config.h"
#include <windows.h>
#include <dpapi.h>
#include <vector>

namespace {

const wchar_t* kAppDir  = L"Jarvis";
const wchar_t* kIniFile = L"config.ini";

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

static const char kB64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64_encode(const uint8_t* d, size_t n) {
    std::string o;
    o.reserve(((n + 2) / 3) * 4);
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)d[i] << 16;
        if (i + 1 < n) v |= (uint32_t)d[i + 1] << 8;
        if (i + 2 < n) v |= d[i + 2];
        o += kB64[(v >> 18) & 63];
        o += kB64[(v >> 12) & 63];
        o += (i + 1 < n) ? kB64[(v >> 6) & 63] : '=';
        o += (i + 2 < n) ? kB64[v & 63] : '=';
    }
    return o;
}

bool b64_decode(const std::string& s, std::vector<uint8_t>& out) {
    static int rev[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i) rev[i] = -1;
        for (int i = 0; kB64[i]; ++i) rev[(uint8_t)kB64[i]] = i;
        init = true;
    }
    out.clear();
    uint32_t v = 0;
    int bits = 0;
    for (char c : s) {
        if (c == '=') break;
        int x = rev[(uint8_t)c];
        if (x < 0) continue;  // skip whitespace
        v = (v << 6) | (uint32_t)x;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)(v >> bits)); v &= (1u << bits) - 1; }
    }
    return !out.empty();
}

bool dpapi_protect(const std::string& plain, std::string& b64out) {
    DATA_BLOB in{(DWORD)plain.size(), (BYTE*)plain.data()}, out{0, nullptr};
    if (!CryptProtectData(&in, L"jarvis-api-key", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out))
        return false;
    b64out = b64_encode(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return true;
}

bool dpapi_unprotect(const std::string& b64in, std::string& plain) {
    std::vector<uint8_t> blob;
    if (!b64_decode(b64in, blob)) return false;
    DATA_BLOB in{(DWORD)blob.size(), blob.data()}, out{0, nullptr};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out))
        return false;
    plain.assign((char*)out.pbData, out.cbData);
    LocalFree(out.pbData);
    return true;
}

}  // namespace

bool config_file_path(std::wstring& out) {
    wchar_t appdata[MAX_PATH];
    if (!GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) return false;
    out = std::wstring(appdata) + L"\\" + kAppDir;
    CreateDirectoryW(out.c_str(), nullptr);  // ensure dir exists
    out += L"\\";
    out += kIniFile;
    return true;
}

bool config_exists() {
    std::wstring p;
    if (!config_file_path(p)) return false;
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool config_load(AppConfig& cfg) {
    std::wstring p;
    if (!config_file_path(p)) return false;
    wchar_t buf[2048];
    GetPrivateProfileStringW(L"atria", L"base_url", L"https://api.atria-asi.ai/v1",
                             buf, 2048, p.c_str());
    cfg.base_url = narrow(buf);
    GetPrivateProfileStringW(L"atria", L"model", L"Atria-Dawn-Preview", buf, 2048, p.c_str());
    cfg.model = narrow(buf);
    GetPrivateProfileStringW(L"atria", L"api_key_enc", L"", buf, 2048, p.c_str());
    if (buf[0] == 0) return false;
    std::string plain;
    if (!dpapi_unprotect(narrow(buf), plain)) return false;
    cfg.api_key = widen(plain);
    if (cfg.api_key.empty()) return false;
    // audio section is optional (older configs won't have it)
    cfg.mic_device_id = (int)GetPrivateProfileIntW(L"audio", L"device_id", -1, p.c_str());
    GetPrivateProfileStringW(L"audio", L"device_name", L"", buf, 2048, p.c_str());
    cfg.mic_device_name = buf;
    GetPrivateProfileStringW(L"audio", L"language", L"id", buf, 2048, p.c_str());
    std::string lang = narrow(buf);
    cfg.stt_language = (lang == "en" || lang == "auto") ? lang : "id";
    GetPrivateProfileStringW(L"audio", L"voice_id", L"", buf, 2048, p.c_str());
    cfg.tts_voice_id = buf;  // empty = automatic voice selection
    return true;
}

bool config_save(const AppConfig& cfg) {
    std::wstring p;
    if (!config_file_path(p)) return false;
    std::string enc;
    if (!dpapi_protect(narrow(cfg.api_key), enc)) return false;
    WritePrivateProfileStringW(L"atria", L"base_url", widen(cfg.base_url).c_str(), p.c_str());
    WritePrivateProfileStringW(L"atria", L"model", widen(cfg.model).c_str(), p.c_str());
    WritePrivateProfileStringW(L"atria", L"api_key_enc", widen(enc).c_str(), p.c_str());
    WritePrivateProfileStringW(L"audio", L"device_id",
                               std::to_wstring(cfg.mic_device_id).c_str(), p.c_str());
    WritePrivateProfileStringW(L"audio", L"device_name",
                               cfg.mic_device_name.c_str(), p.c_str());
    WritePrivateProfileStringW(L"audio", L"language",
                               widen(cfg.stt_language).c_str(), p.c_str());
    WritePrivateProfileStringW(L"audio", L"voice_id",
                               cfg.tts_voice_id.c_str(), p.c_str());
    return true;
}
