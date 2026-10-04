// audio_in.cpp - waveIn capture with energy VAD.
// Format: 16000 Hz, mono, 16-bit PCM (matches Vosk expectations).
#include "audio_in.h"
#include <windows.h>
#include <mmsystem.h>
#include <cmath>

namespace {
const int kRate = 16000;
const int kBufMs = 200;                       // 200ms per buffer
const int kBufSamples = kRate * kBufMs / 1000;  // 3200 samples
const int kNumBufs = 4;
// VAD thresholds on mean-absolute amplitude (0..32768). Tuned conservatively;
// quiet rooms may need lowering. Marked UNTESTED on real hardware.
const double kStartThresh = 320.0;   // speech start
const double kEndThresh   = 220.0;   // speech end
const int kStartBufs = 2;            // consecutive hot buffers to start
const int kEndBufs   = 4;            // consecutive quiet buffers (0.8s) to stop
const int kMaxBufs   = 60;           // 12s hard cap per utterance
const int kPreRollBufs = 2;          // keep 0.4s before trigger

double mean_abs(const int16_t* p, int n) {
    // remove DC offset first for a stable energy estimate
    double sum = 0;
    for (int i = 0; i < n; ++i) sum += p[i];
    double dc = sum / n;
    double e = 0;
    for (int i = 0; i < n; ++i) e += fabs(p[i] - dc);
    return e / n;
}
}  // namespace

bool AudioCapture::record_utterance(std::vector<int16_t>& pcm_out, UINT device_id,
                                   int timeout_ms, const std::atomic<bool>* abort) {
    pcm_out.clear();

    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = kRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = kRate * 2;

    HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!hEvent) return false;

    HWAVEIN hWave = nullptr;
    bool ok = false;
    WAVEHDR* hdrs = nullptr;

    if (waveInOpen(&hWave, device_id, &fmt, (DWORD_PTR)hEvent, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR)
        goto cleanup;

    hdrs = new WAVEHDR[kNumBufs]();
    for (int i = 0; i < kNumBufs; ++i) {
        hdrs[i].lpData = new char[kBufSamples * 2]();
        hdrs[i].dwBufferLength = kBufSamples * 2;
        waveInPrepareHeader(hWave, &hdrs[i], sizeof(WAVEHDR));
        waveInAddBuffer(hWave, &hdrs[i], sizeof(WAVEHDR));
    }
    if (waveInStart(hWave) != MMSYSERR_NOERROR) goto cleanup;

    {
        int hot = 0, quiet = 0, total = 0;
        bool speaking = false;
        std::vector<int16_t> preroll;  // ring of pre-trigger audio
        DWORD t0 = GetTickCount();
        int buf_idx = 0;

        while (true) {
            if (abort && abort->load()) break;
            if ((int)(GetTickCount() - t0) > timeout_ms) break;
            DWORD w = WaitForSingleObject(hEvent, 500);
            if (w != WAIT_OBJECT_0) {
                if (abort && abort->load()) break;
                if ((int)(GetTickCount() - t0) > timeout_ms) break;
                continue;
            }
            // Find the done buffer (round-robin: we know the order).
            WAVEHDR* h = &hdrs[buf_idx];
            buf_idx = (buf_idx + 1) % kNumBufs;
            if (!(h->dwFlags & WHDR_DONE)) continue;

            int16_t* samples = (int16_t*)h->lpData;
            int n = h->dwBytesRecorded / 2;
            double e = (n > 0) ? mean_abs(samples, n) : 0.0;

            if (!speaking) {
                // keep pre-roll
                preroll.insert(preroll.end(), samples, samples + n);
                if ((int)preroll.size() > kPreRollBufs * kBufSamples)
                    preroll.erase(preroll.begin(),
                                  preroll.begin() + (preroll.size() - kPreRollBufs * kBufSamples));
                hot = (e > kStartThresh) ? hot + 1 : 0;
                if (hot >= kStartBufs) {
                    speaking = true;
                    pcm_out = preroll;
                    pcm_out.insert(pcm_out.end(), samples, samples + n);
                    total = 1;
                    quiet = 0;
                }
            } else {
                pcm_out.insert(pcm_out.end(), samples, samples + n);
                ++total;
                quiet = (e < kEndThresh) ? quiet + 1 : 0;
                if (quiet >= kEndBufs || total >= kMaxBufs) { ok = true; break; }
            }

            // recycle buffer
            h->dwFlags &= ~WHDR_DONE;
            waveInAddBuffer(hWave, h, sizeof(WAVEHDR));
        }
    }

cleanup:
    if (hWave) {
        waveInStop(hWave);
        waveInReset(hWave);
    }
    if (hdrs) {
        for (int i = 0; i < kNumBufs; ++i) {
            if (hWave) waveInUnprepareHeader(hWave, &hdrs[i], sizeof(WAVEHDR));
            delete[] hdrs[i].lpData;
        }
        delete[] hdrs;
    }
    if (hWave) waveInClose(hWave);
    CloseHandle(hEvent);
    if (!ok) pcm_out.clear();
    return ok && !pcm_out.empty();
}
