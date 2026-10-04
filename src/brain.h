// brain.h - conversation state machine. Runs a persistent always-listening
// loop on a worker thread: LISTEN -> STT -> THINK (streaming) -> SPEAK,
// then back to LISTEN. Reports state to the UI via callback.
// The microphone is only captured during LISTEN; it is closed while the
// assistant thinks/speaks so its own voice can't retrigger the VAD.
#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include "atria_client.h"

enum class AppState { IDLE, LISTENING, THINKING, SPEAKING };

class Brain {
public:
    // cb(state, info): invoked from worker thread; UI must marshal (PostMessage).
    using StateCb = std::function<void(AppState, const std::wstring&)>;
    Brain(const AtriaConfig& cfg, const std::wstring& exe_dir, StateCb cb,
          UINT mic_device, const std::string& stt_lang);
    ~Brain();

    void run();        // start the persistent loop (no-op if already running)
    void shutdown();   // stop the loop and join the worker thread

    void set_mic_device(UINT id);   // takes effect on the next listen cycle
    void set_mic_muted(bool m);
    bool mic_muted() const { return mic_muted_.load(); }
    void set_stt_language(const std::string& lang);  // "id"/"en"/"auto"
    void set_tts_voice(const std::wstring& token_id);  // manual SAPI voice override;
                                                      // empty = automatic

private:
    void loop_main();
    bool handle_local_command(const std::wstring& transcript, std::wstring& spoken_reply);

    AtriaConfig cfg_;
    std::wstring exe_dir_;
    StateCb cb_;
    std::atomic<UINT> mic_device_;
    std::atomic<bool> mic_muted_{false};
    std::atomic<bool> stop_{false};
    std::thread worker_;
    std::mutex run_mutex_;
    std::mutex lang_mutex_;          // guards stt_lang_ and tts_voice_id_
    std::string stt_lang_ = "id";    // "id" / "en" / "auto"
    std::wstring tts_voice_id_;      // manual voice override, empty = automatic
    bool running_ = false;

    std::string current_lang();  // mutex-protected copy
    std::wstring current_voice();  // mutex-protected copy
};
