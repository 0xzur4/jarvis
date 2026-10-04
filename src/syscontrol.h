// syscontrol.h - v1 local system control. UNTESTED on real Windows (built on Linux).
#pragma once
#include <string>

// percent: 0..100 (clamped)
bool sys_set_volume(int percent);
// target: e.g. L"notepad", L"calc", L"C:\\path\\app.exe". Uses ShellExecuteW.
bool sys_open_app(const std::wstring& target);
bool sys_lock_workstation();
