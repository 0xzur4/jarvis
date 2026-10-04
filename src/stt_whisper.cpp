// stt_whisper.cpp - whisper.cpp wrapper. Uses the non-deprecated
// whisper_init_from_file_with_params API (v1.9.x).
#include "stt_whisper.h"
#include "whisper.h"
#include <vector>

namespace {

std::string trim_ws(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

}  // namespace

WhisperSTT::~WhisperSTT() { shutdown(); }

void WhisperSTT::shutdown() {
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

bool WhisperSTT::init(const std::string& model_path, const std::string& language,
                      std::string& error) {
    shutdown();
    set_language(language);
    whisper_context_params cparams = whisper_context_default_params();
    ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!ctx_) {
        error = "whisper model failed to load (missing/corrupt models/whisper-base.bin?)";
        return false;
    }
    return true;
}

void WhisperSTT::set_language(const std::string& lang) {
    // whisper treats null/empty/"auto" as auto-detect.
    if (lang == "id" || lang == "en" || lang == "auto") language_ = lang;
    else language_ = "id";
}

bool WhisperSTT::transcribe(const int16_t* pcm, size_t samples,
                            std::string& text_utf8_out) {
    text_utf8_out.clear();
    if (!ctx_ || !pcm || samples == 0) return false;

    // whisper expects 16kHz mono float in [-1, 1].
    std::vector<float> f32(samples);
    for (size_t i = 0; i < samples; ++i) f32[i] = pcm[i] / 32768.0f;

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.translate        = false;
    wparams.no_context       = true;   // independent utterances
    wparams.single_segment   = true;
    wparams.language         = language_.c_str();

    if (whisper_full(ctx_, wparams, f32.data(), (int)samples) != 0) return false;

    std::string out;
    const int n = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n; ++i) {
        const char* t = whisper_full_get_segment_text(ctx_, i);
        if (t) out += t;
    }
    text_utf8_out = trim_ws(out);
    return true;
}
