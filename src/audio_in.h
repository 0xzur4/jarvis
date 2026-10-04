// audio_in.h - microphone capture via WinMM waveIn, 16kHz mono 16-bit,
// with simple energy-based voice activity detection.
#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <vector>

class AudioCapture {
public:
    // Blocks until an utterance is captured (speech + ~0.8s trailing silence),
    // timeout_ms elapses, or *abort becomes true. Returns true if speech was
    // detected. device_id selects the waveIn input device.
    bool record_utterance(std::vector<int16_t>& pcm_out, UINT device_id,
                          int timeout_ms = 30000,
                          const std::atomic<bool>* abort = nullptr);
};
