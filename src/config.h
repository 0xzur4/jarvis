// config.h - API config storage. API key is DPAPI-encrypted before hitting disk.
#pragma once
#include <string>

struct AppConfig {
    std::wstring api_key;      // plaintext in memory only
    std::string  base_url;     // e.g. https://api.atria-asi.ai/v1
    std::string  model;        // e.g. Atria-Dawn-Preview
    int          mic_device_id = -1;   // waveIn device id, -1 = not chosen yet
    std::wstring mic_device_name;      // display name, used to re-match devices
    std::string  stt_language = "id"; // "id" / "en" / "auto"
    std::wstring tts_voice_id;        // SAPI token id of manually chosen voice, empty = auto
};

bool config_file_path(std::wstring& out);   // %APPDATA%\Jarvis\config.ini
bool config_exists();
bool config_load(AppConfig& cfg);           // false if missing/corrupt
bool config_save(const AppConfig& cfg);
