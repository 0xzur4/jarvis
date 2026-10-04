// voice_enum.h - list the TTS voices installed on this machine (SAPI5).
// COM must be initialized on the calling thread before calling these.
#pragma once
#include <string>
#include <vector>
#include <windows.h>

struct TtsVoice {
    std::wstring token_id;   // registry token id, stable across runs
    std::wstring name;       // e.g. L"Microsoft Zira Desktop"
    std::wstring language;   // e.g. L"English (United States)"
    std::wstring gender;     // L"Female" / L"Male" / empty if unknown
    WORD lang_primary = 0;   // PRIMARYLANGID of the first listed language
};

std::vector<TtsVoice> enum_tts_voices();

// Combo-box display text, e.g. "Microsoft Zira Desktop (English, Female)".
std::wstring tts_voice_display(const TtsVoice& v);

// Index of the first voice matching a primary language id
// (0x09 = English, 0x21 = Indonesian), or -1 when none match.
int tts_voice_for_lang(const std::vector<TtsVoice>& vs, WORD primary);
