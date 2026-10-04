// tts_sapi.h - text-to-speech via built-in Windows SAPI (zero dependencies).
// NOTE: caller must have COM initialized on the calling thread (brain worker does).
#pragma once
#include <string>

class SapiTTS {
public:
    ~SapiTTS();
    bool init(std::string& error);
    // lang: "id" -> prefer an id-ID voice (0x421), "en" -> en-US (0x409),
    // "auto"/other -> keep current voice. Returns true if a matching voice
    // was found and selected; false keeps the previous voice.
    bool set_language(const std::string& lang);
    // Select a specific installed voice by its token id (from enum_tts_voices).
    // Returns false when the token no longer exists (voice was uninstalled).
    bool set_voice_by_id(const std::wstring& token_id);
    bool speak(const std::wstring& text);   // synchronous; false on failure
    void stop();                            // purge pending speech
    void shutdown();
private:
    struct ISpVoice* voice_ = nullptr;
};
