// mic_enum.cpp
#include "mic_enum.h"
#include <mmsystem.h>

std::vector<MicDevice> enum_microphones() {
    std::vector<MicDevice> out;
    UINT n = waveInGetNumDevs();
    for (UINT i = 0; i < n; ++i) {
        WAVEINCAPSW caps{};
        if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
            out.push_back({i, caps.szPname});
    }
    return out;
}

bool mic_device_still_present(int saved_id, const std::wstring& saved_name) {
    auto devs = enum_microphones();
    if (!saved_name.empty()) {
        for (const auto& d : devs)
            if (d.name == saved_name) return true;
    }
    if (saved_id >= 0) {
        for (const auto& d : devs)
            if ((int)d.id == saved_id) return true;
    }
    return false;
}

bool resolve_mic_device(int saved_id, const std::wstring& saved_name, UINT& id_out) {
    auto devs = enum_microphones();
    if (devs.empty()) return false;
    if (mic_device_still_present(saved_id, saved_name)) {
        // prefer the name match for a stable id
        if (!saved_name.empty()) {
            for (const auto& d : devs)
                if (d.name == saved_name) { id_out = d.id; return true; }
        }
        for (const auto& d : devs)
            if ((int)d.id == saved_id) { id_out = d.id; return true; }
    }
    id_out = devs[0].id;  // fallback: default/first device
    return true;
}
