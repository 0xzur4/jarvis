// setup_dialog.cpp - programmatic Win32 controls (no .rc / windres needed).
#include "setup_dialog.h"
#include "atria_client.h"
#include "mic_enum.h"
#include "voice_enum.h"
#include <windows.h>
#include <thread>

namespace {

const wchar_t* kClass = L"JarvisSetupClass";
const wchar_t* kMicClass = L"JarvisMicClass";
const UINT WM_TEST_DONE = WM_APP + 101;

enum { IDC_KEY = 101, IDC_URL, IDC_MODEL, IDC_MIC, IDC_LANG, IDC_SKIP, IDC_TEST, IDC_SAVE, IDC_STATUS,
       IDC_VOICE = 110 };

struct Ctx {
    HINSTANCE hInst;
    AppConfig cfg;
    bool saved = false;
    bool testing = false;
    bool voice_touched = false;  // user manually changed the voice combo
    HWND hKey, hUrl, hModel, hMic, hLang, hVoice, hSkip, hTest, hSave, hStatus;
    std::vector<MicDevice> mics;
    std::vector<TtsVoice> voices;
};

// Fill a combobox with the available microphones and select the saved one
// (by name, then id, else first). Returns false when no devices exist.
bool fill_mic_combo(HWND hCombo, const std::vector<MicDevice>& mics,
                    int saved_id, const std::wstring& saved_name) {
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    if (mics.empty()) return false;
    int sel = 0;
    for (size_t i = 0; i < mics.size(); ++i) {
        int idx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0,
                                    (LPARAM)mics[i].name.c_str());
        SendMessageW(hCombo, CB_SETITEMDATA, idx, (LPARAM)mics[i].id);
        if (!saved_name.empty() && mics[i].name == saved_name) sel = idx;
        else if (saved_name.empty() && saved_id >= 0 && (int)mics[i].id == saved_id) sel = idx;
    }
    SendMessageW(hCombo, CB_SETCURSEL, sel, 0);
    return true;
}

bool read_mic_combo(HWND hCombo, const std::vector<MicDevice>& mics,
                    int& id_out, std::wstring& name_out) {
    int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || mics.empty()) return false;
    UINT dev = (UINT)SendMessageW(hCombo, CB_GETITEMDATA, sel, 0);
    for (const auto& m : mics) {
        if (m.id == dev) { id_out = (int)m.id; name_out = m.name; return true; }
    }
    return false;
}

// Primary LANGID for a stt language code; 0 = no preference ("auto").
WORD lang_primary_of(const std::string& code) {
    if (code == "id") return 0x21;  // Indonesian
    if (code == "en") return 0x09;  // English
    return 0;
}

// Fill the voice combobox with installed TTS voices. Pre-selects the saved
// voice id when still installed, else the best match for lang_primary.
bool fill_voice_combo(HWND hCombo, const std::vector<TtsVoice>& vs,
                      const std::wstring& saved_id, WORD lang_primary) {
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    if (vs.empty()) return false;
    int sel = 0;
    for (size_t i = 0; i < vs.size(); ++i) {
        std::wstring d = tts_voice_display(vs[i]);
        int idx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)d.c_str());
        SendMessageW(hCombo, CB_SETITEMDATA, idx, (LPARAM)i);
    }
    int n = (int)SendMessageW(hCombo, CB_GETCOUNT, 0, 0);
    int saved_idx = -1, lang_idx = -1;
    for (int i = 0; i < n; ++i) {
        int vi = (int)SendMessageW(hCombo, CB_GETITEMDATA, i, 0);
        if (vi < 0 || (size_t)vi >= vs.size()) continue;
        if (!saved_id.empty() && vs[vi].token_id == saved_id) saved_idx = i;
        if (lang_idx < 0 && lang_primary != 0 &&
            vs[vi].lang_primary == lang_primary) lang_idx = i;
    }
    sel = saved_idx >= 0 ? saved_idx : (lang_idx >= 0 ? lang_idx : 0);
    SendMessageW(hCombo, CB_SETCURSEL, sel, 0);
    return true;
}

// Select the best voice for lang_primary in an already-filled combo.
void autoselect_voice_for_lang(HWND hCombo, const std::vector<TtsVoice>& vs,
                               WORD lang_primary) {
    if (lang_primary == 0) return;
    int vi = tts_voice_for_lang(vs, lang_primary);
    if (vi < 0) return;
    int n = (int)SendMessageW(hCombo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < n; ++i) {
        if ((int)SendMessageW(hCombo, CB_GETITEMDATA, i, 0) == vi) {
            SendMessageW(hCombo, CB_SETCURSEL, i, 0);
            return;
        }
    }
}

std::string narrow(const std::wstring& w) {    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

void set_status(Ctx* c, const std::wstring& t) { SetWindowTextW(c->hStatus, t.c_str()); }

AtriaConfig ctx_to_atria(Ctx* c) {
    wchar_t b[2048];
    GetWindowTextW(c->hKey, b, 2048);   std::string key = narrow(b);
    GetWindowTextW(c->hUrl, b, 2048);   std::string url = narrow(b);
    GetWindowTextW(c->hModel, b, 2048); std::string model = narrow(b);
    if (url.empty()) url = "https://api.atria-asi.ai/v1";
    if (model.empty()) model = "Atria-Dawn-Preview";
    return {url, model, key};
}

void run_test(Ctx* c, HWND hwnd) {
    if (c->testing) return;
    c->testing = true;
    EnableWindow(c->hTest, FALSE);
    EnableWindow(c->hSave, FALSE);
    set_status(c, L"Menghubungi API...");
    AtriaConfig ac = ctx_to_atria(c);
    std::thread([hwnd, ac] {
        std::string err;
        AtriaClient client(ac);
        bool ok = client.test_connection(err);
        std::wstring* msg = new std::wstring(ok ? L"OK - koneksi berhasil."
                                                : widen("Gagal: " + err));
        PostMessageW(hwnd, WM_TEST_DONE, ok ? 1 : 0, (LPARAM)msg);
    }).detach();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Ctx* c = (Ctx*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        c = (Ctx*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)c);
        HINSTANCE hi = c->hInst;
        auto label = [&](const wchar_t* t, int y) {
            CreateWindowW(L"STATIC", t, WS_CHILD | WS_VISIBLE, 20, y, 440, 20, hwnd,
                          nullptr, hi, nullptr);
        };
        auto edit = [&](int id, int y, DWORD style) -> HWND {
            return CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | style,
                                 20, y, 440, 26, hwnd, (HMENU)(INT_PTR)id, hi, nullptr);
        };
        label(L"API Key (dari console Atria - API Key page):", 14);
        c->hKey = edit(IDC_KEY, 38, ES_PASSWORD | ES_AUTOHSCROLL);
        label(L"Base URL:", 72);
        c->hUrl = edit(IDC_URL, 96, ES_AUTOHSCROLL);
        label(L"Model:", 130);
        c->hModel = edit(IDC_MODEL, 154, ES_AUTOHSCROLL);
        label(L"Microphone:", 188);
        c->hMic = CreateWindowW(L"COMBOBOX", L"",
                                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                20, 212, 440, 200, hwnd, (HMENU)(INT_PTR)IDC_MIC, hi, nullptr);
        c->mics = enum_microphones();
        if (!fill_mic_combo(c->hMic, c->mics, c->cfg.mic_device_id, c->cfg.mic_device_name)) {
            SendMessageW(c->hMic, CB_ADDSTRING, 0, (LPARAM)L"(tidak ada microphone terdeteksi)");
            SendMessageW(c->hMic, CB_SETCURSEL, 0, 0);
            EnableWindow(c->hMic, FALSE);
        }
        label(L"Bahasa bicara (speech recognition):", 246);
        c->hLang = CreateWindowW(L"COMBOBOX", L"",
                                 WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                 20, 270, 440, 120, hwnd, (HMENU)(INT_PTR)IDC_LANG, hi, nullptr);
        const wchar_t* kLangName[] = {L"Indonesia", L"English", L"Otomatis (deteksi)"};
        const char* kLangCode[] = {"id", "en", "auto"};
        int lang_sel = 0;
        for (int i = 0; i < 3; ++i) {
            int idx = (int)SendMessageW(c->hLang, CB_ADDSTRING, 0, (LPARAM)kLangName[i]);
            SendMessageW(c->hLang, CB_SETITEMDATA, idx, (LPARAM)i);
            if (c->cfg.stt_language == kLangCode[i]) lang_sel = idx;
        }
        SendMessageW(c->hLang, CB_SETCURSEL, lang_sel, 0);
        label(L"Suara (voice):", 304);
        c->hVoice = CreateWindowW(L"COMBOBOX", L"",
                                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  20, 328, 440, 200, hwnd, (HMENU)(INT_PTR)IDC_VOICE, hi, nullptr);
        c->voices = enum_tts_voices();  // needs COM (initialized by show_setup_dialog)
        if (!fill_voice_combo(c->hVoice, c->voices, c->cfg.tts_voice_id,
                              lang_primary_of(c->cfg.stt_language))) {
            SendMessageW(c->hVoice, CB_ADDSTRING, 0,
                         (LPARAM)L"(tidak ada voice TTS terdeteksi)");
            SendMessageW(c->hVoice, CB_SETCURSEL, 0, 0);
            EnableWindow(c->hVoice, FALSE);
        }
        c->hSkip = CreateWindowW(L"BUTTON", L"Lewati test koneksi",
                                 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                 20, 362, 440, 22, hwnd, (HMENU)(INT_PTR)IDC_SKIP, hi, nullptr);
        c->hTest = CreateWindowW(L"BUTTON", L"Test Koneksi",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 20, 390, 140, 32, hwnd, (HMENU)(INT_PTR)IDC_TEST, hi, nullptr);
        c->hSave = CreateWindowW(L"BUTTON", L"Simpan && Mulai",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                                 320, 390, 140, 32, hwnd, (HMENU)(INT_PTR)IDC_SAVE, hi, nullptr);
        c->hStatus = CreateWindowW(L"STATIC", L"",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   20, 432, 440, 56, hwnd, (HMENU)(INT_PTR)IDC_STATUS, hi, nullptr);

        // prefill with existing config or defaults
        SetWindowTextW(c->hKey, c->cfg.api_key.c_str());
        SetWindowTextW(c->hUrl, widen(c->cfg.base_url.empty()
            ? "https://api.atria-asi.ai/v1" : c->cfg.base_url).c_str());
        SetWindowTextW(c->hModel, widen(c->cfg.model.empty()
            ? "Atria-Dawn-Preview" : c->cfg.model).c_str());
        if (!c->cfg.api_key.empty()) {
            // editing existing config: allow save directly
            EnableWindow(c->hSave, TRUE);
            set_status(c, L"Config lama dimuat. Test ulang atau simpan langsung.");
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        if (id == IDC_VOICE && code == CBN_SELCHANGE) {
            c->voice_touched = true;  // manual choice always wins from now on
        }
        else if (id == IDC_LANG && code == CBN_SELCHANGE) {
            // Smart default: pre-select a voice matching the new language,
            // unless the user already picked a voice manually.
            if (!c->voice_touched && IsWindowEnabled(c->hVoice)) {
                int lsel = (int)SendMessageW(c->hLang, CB_GETCURSEL, 0, 0);
                if (lsel >= 0) {
                    int lidx = (int)SendMessageW(c->hLang, CB_GETITEMDATA, lsel, 0);
                    const char* kLangCode[] = {"id", "en", "auto"};
                    WORD prim = (lidx >= 0 && lidx < 3)
                        ? lang_primary_of(kLangCode[lidx]) : 0;
                    autoselect_voice_for_lang(c->hVoice, c->voices, prim);
                }
            }
        }
        else if (id == IDC_TEST) run_test(c, hwnd);
        else if (id == IDC_SKIP) {
            bool skip = SendMessageW(c->hSkip, BM_GETCHECK, 0, 0) == BST_CHECKED;
            EnableWindow(c->hSave, skip ? TRUE : FALSE);
            if (skip) set_status(c, L"Test dilewati - kamu bisa simpan langsung.");
        }
        else if (id == IDC_SAVE) {
            wchar_t b[2048];
            GetWindowTextW(c->hKey, b, 2048);
            if (b[0] == 0) { set_status(c, L"API Key tidak boleh kosong."); break; }
            GetWindowTextW(c->hUrl, b, 2048);
            c->cfg.base_url = narrow(b);
            if (c->cfg.base_url.empty()) c->cfg.base_url = "https://api.atria-asi.ai/v1";
            GetWindowTextW(c->hModel, b, 2048);
            c->cfg.model = narrow(b);
            if (c->cfg.model.empty()) c->cfg.model = "Atria-Dawn-Preview";
            GetWindowTextW(c->hKey, b, 2048);
            c->cfg.api_key = b;
            int mid; std::wstring mname;
            if (read_mic_combo(c->hMic, c->mics, mid, mname)) {
                c->cfg.mic_device_id = mid;
                c->cfg.mic_device_name = mname;
            }
            int lsel = (int)SendMessageW(c->hLang, CB_GETCURSEL, 0, 0);
            if (lsel >= 0) {
                int lidx = (int)SendMessageW(c->hLang, CB_GETITEMDATA, lsel, 0);
                const char* kLangCode[] = {"id", "en", "auto"};
                if (lidx >= 0 && lidx < 3) c->cfg.stt_language = kLangCode[lidx];
            }
            if (IsWindowEnabled(c->hVoice)) {
                int vsel = (int)SendMessageW(c->hVoice, CB_GETCURSEL, 0, 0);
                if (vsel >= 0) {
                    int vidx = (int)SendMessageW(c->hVoice, CB_GETITEMDATA, vsel, 0);
                    if (vidx >= 0 && (size_t)vidx < c->voices.size())
                        c->cfg.tts_voice_id = c->voices[vidx].token_id;
                }
            }
            if (config_save(c->cfg)) { c->saved = true; DestroyWindow(hwnd); }
            else set_status(c, L"Gagal menyimpan config.");
        }
        return 0;
    }
    case WM_TEST_DONE: {
        c->testing = false;
        EnableWindow(c->hTest, TRUE);
        std::wstring* m = (std::wstring*)lp;
        set_status(c, *m);
        delete m;
        if (wp == 1) EnableWindow(c->hSave, TRUE);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool show_setup_dialog(HINSTANCE hInst, AppConfig& cfg) {
    // COM is needed here for TTS voice enumeration (SAPI).
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool co_owned = (hrCo == S_OK);  // S_FALSE: already init on this thread

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClass;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    Ctx ctx;
    ctx.hInst = hInst;
    ctx.cfg = cfg;

    HWND hwnd = CreateWindowExW(0, kClass, L"Jarvis - Setup API",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 496, 528,
        nullptr, nullptr, hInst, &ctx);
    if (!hwnd) { if (co_owned) CoUninitialize(); return false; }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    UnregisterClassW(kClass, hInst);
    if (co_owned) CoUninitialize();
    if (ctx.saved) cfg = ctx.cfg;
    return ctx.saved;
}

namespace {

enum { MIDC_COMBO = 201, MIDC_OK, MIDC_CANCEL };

struct MicCtx {
    HINSTANCE hInst;
    AppConfig cfg;
    bool saved = false;
    HWND hCombo;
    std::vector<MicDevice> mics;
};

LRESULT CALLBACK MicWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MicCtx* c = (MicCtx*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        c = (MicCtx*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)c);
        HINSTANCE hi = c->hInst;
        CreateWindowW(L"STATIC", L"Pilih microphone yang dipakai Jarvis:",
                      WS_CHILD | WS_VISIBLE, 20, 18, 380, 20, hwnd, nullptr, hi, nullptr);
        c->hCombo = CreateWindowW(L"COMBOBOX", L"",
                                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  20, 44, 380, 200, hwnd, (HMENU)(INT_PTR)MIDC_COMBO, hi, nullptr);
        c->mics = enum_microphones();
        if (!fill_mic_combo(c->hCombo, c->mics, c->cfg.mic_device_id, c->cfg.mic_device_name)) {
            SendMessageW(c->hCombo, CB_ADDSTRING, 0, (LPARAM)L"(tidak ada microphone terdeteksi)");
            SendMessageW(c->hCombo, CB_SETCURSEL, 0, 0);
            EnableWindow(c->hCombo, FALSE);
        }
        CreateWindowW(L"BUTTON", L"Simpan",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                      220, 96, 90, 30, hwnd, (HMENU)(INT_PTR)MIDC_OK, hi, nullptr);
        CreateWindowW(L"BUTTON", L"Batal",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      318, 96, 90, 30, hwnd, (HMENU)(INT_PTR)MIDC_CANCEL, hi, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == MIDC_OK) {
            int mid; std::wstring mname;
            if (read_mic_combo(c->hCombo, c->mics, mid, mname)) {
                c->cfg.mic_device_id = mid;
                c->cfg.mic_device_name = mname;
                if (config_save(c->cfg)) { c->saved = true; DestroyWindow(hwnd); }
            } else {
                MessageBoxW(hwnd, L"Tidak ada microphone yang bisa dipilih.",
                            L"Jarvis", MB_ICONWARNING);
            }
        } else if (id == MIDC_CANCEL) {
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool show_mic_dialog(HINSTANCE hInst, AppConfig& cfg) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = MicWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kMicClass;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    MicCtx ctx;
    ctx.hInst = hInst;
    ctx.cfg = cfg;

    HWND hwnd = CreateWindowExW(0, kMicClass, L"Jarvis - Pilih Microphone",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 440, 180,
        nullptr, nullptr, hInst, &ctx);
    if (!hwnd) return false;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    UnregisterClassW(kMicClass, hInst);
    if (ctx.saved) cfg = ctx.cfg;
    return ctx.saved;
}
