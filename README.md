# Jarvis

A native Windows desktop voice assistant in C++ — inspired by Iron Man's Jarvis. Talk to it, and it talks back: speech-to-speech, hands-free, with a floating pixel face and permission to control things on your PC.

![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue) ![Language](https://img.shields.io/badge/language-C%2B%2B17-orange) ![License](https://img.shields.io/badge/license-MIT-green)

> **Status:** v4 — working preview. Speech recognition (Indonesian/English), streaming AI brain, voice picker, local system commands. See [Roadmap](#roadmap).

---

## Features

- **Speech-to-speech loop** — always-on microphone with energy-based voice activity detection: you speak → it transcribes → it thinks → it speaks back. No buttons to press.
- **Multilingual STT** — [whisper.cpp](https://github.com/ggerganov/whisper.cpp) (`ggml-base`), with selectable language: Indonesian, English, or Auto-detect.
- **Streaming AI brain** — connects to any OpenAI-compatible `/v1/chat/completions` API (default: Atria ASI). Responses stream token-by-token so speech starts before the sentence finishes.
- **Voice picker** — choose any SAPI voice installed on your machine (e.g. a female voice), auto-matched to your language, overridable anytime.
- **Floating pixel face** — borderless transparent window; only the animated pixel face is visible. Mouth animates while speaking, expressions change per state (listening / thinking / speaking). Drag it anywhere, right-click for settings.
- **Microphone selection** — pick your input device once; remembered afterwards.
- **Local system commands** — say *"buka visual studio code"*, *"volume 50"*, *"kunci"* to open apps, set volume, or lock the workstation. App-name aliases + Windows App Paths lookup, with spoken success/failure feedback.
- **Private by design** — your API key is entered manually on first run and stored encrypted with Windows DPAPI. Nothing is hardcoded.

---

## Quick start (Windows)

1. Download the latest `jarvis-v*-windows.zip` from [Releases](https://github.com/0xzur4/jarvis/releases) and extract it.
2. Run `jarvis.exe`.
3. On first run a setup window appears:
   - **API Key** — your key for the AI API (see below).
   - **Base URL** — default `https://api.atria-asi.ai/v1` (any OpenAI-compatible endpoint works).
   - **Model** — default `Atria-Dawn-Preview`.
   - **Microphone / Language / Voice** — pick your devices and preferences.
   - Click **Test Connection**, then **Save & Start**.
4. Just talk. The dot on the face shows the state: 🟢 listening · 🟡 thinking · 🔵 speaking.

### Getting an Atria API key

1. Log in to the Atria console with your Google account.
2. Open the API Key page, create a key, and copy it (it is shown only once).
3. Paste it into Jarvis's setup window. The key is encrypted with DPAPI and stored under `%APPDATA%\Jarvis\`.

> Tip: for a natural Indonesian speaking voice, install the Bahasa Indonesia language pack in Windows Settings → Time & Language → Language (it brings its own TTS voice), then select it in Jarvis's voice dropdown.

---

## Voice commands

| Say (ID / EN) | Action |
|---|---|
| `buka <app>` / `open <app>` | Open an application (aliases: "visual studio code", "chrome", "notepad", "kalkulator", …) |
| `volume <0-100>` | Set system volume |
| `kunci` / `lock` | Lock the workstation |
| anything else | Sent to the AI brain, answered by voice |

---

## Building from source

**Requirements:** a C++17 compiler targeting Windows x86-64.

**On Windows (MSVC):** open a *x64 Native Tools* prompt and compile all `src/*.cpp` with `/std:c++17`, linking `winhttp.lib winmm.lib ole32.lib crypt32.lib msimg32.lib gdi32.lib uuid.lib`, plus whisper.cpp built from source (or use the prebuilt static libs under `tools/whisper/`).

**Cross-compile from Linux (how the releases are built):**
```bash
# 1. Get a MinGW-w64 toolchain, e.g. llvm-mingw (Linux-hosted):
#    https://github.com/mstorsjo/llvm-mingw/releases
# 2. Build whisper.cpp v1.9.x for Windows and place headers/libs in tools/whisper/
# 3. Download the STT model (~148 MB):
curl -L -o models/ggml-base.bin \
  https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin
# 4. Build:
./build.sh
# 5. Package: dist/jarvis.exe + models/ggml-base.bin
```

The model file is **not** in this repo (GitHub file-size limits) — download it from the link above.

---

## Project structure

```
jarvis/
├── src/
│   ├── main.cpp          # WinMain, window creation, message loop
│   ├── pixel_face.*      # Pixel-face renderer (1-bit bitmaps embedded in faces.h)
│   ├── faces.h           # Face bitmaps generated from 21 pixel-art PNGs
│   ├── setup_dialog.*    # First-run setup: API key, mic, language, voice
│   ├── config.*          # config.ini in %APPDATA%, API key encrypted via DPAPI
│   ├── atria_client.*    # HTTPS streaming client (WinHTTP + SSE) for chat/completions
│   ├── audio_in.*        # Microphone capture (WinMM waveIn) + energy VAD
│   ├── mic_enum.*        # Microphone device enumeration
│   ├── stt_whisper.*     # Speech-to-text via whisper.cpp
│   ├── tts_sapi.*       # Text-to-speech via Windows SAPI
│   ├── voice_enum.*      # Installed TTS voice enumeration
│   ├── syscontrol.*      # Local commands: open app, volume, lock
│   ├── brain.*           # Conversation state machine (listen→think→speak loop)
│   └── json_mini.*       # Tiny JSON string escaper/unescaper
├── tools/whisper/       # Prebuilt whisper.cpp headers + static libs (Windows x86-64)
├── models/              # ggml-base.bin goes here (downloaded separately, gitignored)
├── build.sh             # Cross-compile script (Linux → Windows via llvm-mingw)
├── README.md            # This file (English)
└── README-INDONESIA.txt # User guide in Indonesian (shipped with releases)
```

---

## Privacy

- The microphone is always on while Jarvis runs (that's the point), but audio is processed **locally** for speech detection and transcription. Only transcribed text is sent to the configured AI API.
- Your API key never leaves your machine except as an `Authorization` header to the API endpoint you configured. It is stored encrypted with DPAPI.

---

## Roadmap

- [ ] Wake-word activation ("Hey Jarvis") instead of pure VAD
- [ ] More local commands (media control, screenshots, custom shortcuts)
- [ ] Conversation memory across sessions
- [ ] Installer (.msi) and auto-update

---

## License

MIT — see [LICENSE](LICENSE).

*Not affiliated with Marvel. Just a fan build.*
