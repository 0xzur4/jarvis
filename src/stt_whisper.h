// stt_whisper.h - speech recognition via whisper.cpp (multilingual).
// The model file (ggml-base.bin) ships next to the exe under models/.
// Language is a per-call parameter: "id", "en", or "auto" (auto-detect).
#pragma once
#include <cstdint>
#include <string>

struct whisper_context;

class WhisperSTT {
public:
    ~WhisperSTT();
    // model_path: UTF-8 path to the ggml model file. language: "id"/"en"/"auto".
    bool init(const std::string& model_path, const std::string& language,
              std::string& error);
    void set_language(const std::string& lang);  // takes effect on next transcribe
    bool transcribe(const int16_t* pcm, size_t samples, std::string& text_utf8_out);
    bool ready() const { return ctx_ != nullptr; }
    void shutdown();

private:
    whisper_context* ctx_ = nullptr;
    std::string language_ = "id";
};
