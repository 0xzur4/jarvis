// syscontrol.cpp - UNTESTED: written against Win32 docs, cannot run on Linux.
#include "syscontrol.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <shellapi.h>

bool sys_set_volume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    HRESULT hr;
    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr) || !enumerator) return false;
    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    enumerator->Release();
    if (FAILED(hr) || !device) return false;
    IAudioEndpointVolume* vol = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&vol);
    device->Release();
    if (FAILED(hr) || !vol) return false;
    hr = vol->SetMasterVolumeLevelScalar(percent / 100.0f, nullptr);
    vol->Release();
    return SUCCEEDED(hr);
}

bool sys_open_app(const std::wstring& target) {
    if (target.empty()) return false;
    // 1) Direct: ShellExecute resolves via PATH and well-known names.
    HINSTANCE r = ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)r > 32) return true;
    // 2) With .exe suffix (covers "Code" -> "Code.exe" etc).
    std::wstring with_exe = target + L".exe";
    r = ShellExecuteW(nullptr, L"open", with_exe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)r > 32) return true;
    // 3) App Paths registry (where installers like VS Code register):
    //    HKCU/HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\<name>.exe
    const wchar_t* kAppPaths = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\";
    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        std::wstring key = std::wstring(kAppPaths) + with_exe;
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        if (RegGetValueW(root, key.c_str(), nullptr, RRF_RT_REG_SZ, nullptr,
                         path, &size) == ERROR_SUCCESS && path[0]) {
            r = ShellExecuteW(nullptr, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
            if ((INT_PTR)r > 32) return true;
        }
    }
    return false;
}

bool sys_lock_workstation() {
    return LockWorkStation() != FALSE;
}
