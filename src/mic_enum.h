// mic_enum.h - enumerate waveIn microphone input devices.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct MicDevice {
    UINT id;
    std::wstring name;
};

// All available input devices (may be empty if none present).
std::vector<MicDevice> enum_microphones();

// Resolve a saved (id, name) pair to a live device id.
// Match by name first (ids can shift between boots), then by id,
// then fall back to the first available device. Returns false only
// when no input devices exist at all.
bool resolve_mic_device(int saved_id, const std::wstring& saved_name, UINT& id_out);

// Strict check: does the saved (id, name) still match a live device?
bool mic_device_still_present(int saved_id, const std::wstring& saved_name);
