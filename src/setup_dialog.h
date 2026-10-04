// setup_dialog.h - first-run window for entering Atria API credentials.
#pragma once
#include <windows.h>
#include <string>
#include "config.h"

// Shows the setup window modally (own message loop). Returns true if the user
// saved a config (caller should then open the main window).
bool show_setup_dialog(HINSTANCE hInst, AppConfig& cfg /*in/out*/);

// Small modal dialog to (re)select the microphone. Returns true if the user
// picked one and it was saved to config.
bool show_mic_dialog(HINSTANCE hInst, AppConfig& cfg /*in/out*/);
