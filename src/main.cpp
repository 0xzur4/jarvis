// main.cpp - Jarvis v2: borderless transparent floating pixel face.
// The window is layered with a magenta color key: only the white pixel
// face (and the status dot) is visible. Drag by the face, right-click
// the face for the menu. The mic is always listening; no buttons.
#include <windows.h>
#include "pixel_face.h"
#include "config.h"
#include "setup_dialog.h"
#include "mic_enum.h"
#include "brain.h"

namespace {

const wchar_t* kClass = L"JarvisMainClass";
const COLORREF kKey = RGB(255, 0, 255);  // transparency color key
const UINT WM_BRAIN_STATE = WM_APP + 201;
const UINT_PTR kTalkTimer = 1;
const int kFaceScale = 4;
const int kPad = 26;  // padding around the face inside the window

enum { IDM_API = 101, IDM_MIC, IDM_MIC_TOGGLE, IDM_EXIT };

struct App {
    HINSTANCE hInst;
    HWND hwnd = nullptr;
    AppConfig cfg;
    PixelFace face;
    Brain* brain = nullptr;
    AppState state = AppState::IDLE;
    bool talk_open = false;
    bool mic_muted = false;
};

std::wstring exe_dir_of() {
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::wstring d = exe_path;
    size_t bs = d.find_last_of(L"\\/");
    if (bs != std::wstring::npos) d = d.substr(0, bs);
    return d;
}

AtriaConfig make_atria(const AppConfig& cfg) {
    AtriaConfig ac;
    ac.base_url = cfg.base_url;
    ac.model = cfg.model;
    int n = WideCharToMultiByte(CP_UTF8, 0, cfg.api_key.c_str(), -1,
                                nullptr, 0, nullptr, nullptr);
    ac.api_key.assign(n > 1 ? n - 1 : 0, 0);
    if (n > 1)
        WideCharToMultiByte(CP_UTF8, 0, cfg.api_key.c_str(), -1,
                            ac.api_key.data(), n, nullptr, nullptr);
    return ac;
}

FaceId face_for_state(App* a) {
    switch (a->state) {
        case AppState::LISTENING: return FaceId::LISTEN;
        case AppState::THINKING:  return FaceId::THINK;
        case AppState::SPEAKING:  return a->talk_open ? FaceId::TALK_OPEN : FaceId::TALK_CLOSED;
        default:                  return FaceId::IDLE;
    }
}

COLORREF dot_color(AppState s, bool muted) {
    if (muted) return RGB(0xe0, 0x40, 0x40);       // red: mic off
    switch (s) {
        case AppState::LISTENING: return RGB(0x2e, 0xcc, 0x71);  // green: talk now
        case AppState::THINKING:  return RGB(0xf5, 0xa6, 0x23);  // amber: thinking
        case AppState::SPEAKING:  return RGB(0x4a, 0xa8, 0xff);  // blue: speaking
        default:                  return RGB(0x88, 0x88, 0x88);  // gray: idle
    }
}

void recreate_brain(App* a) {
    if (a->brain) { a->brain->shutdown(); delete a->brain; a->brain = nullptr; }
    UINT dev = 0;
    resolve_mic_device(a->cfg.mic_device_id, a->cfg.mic_device_name, dev);
    a->brain = new Brain(make_atria(a->cfg), exe_dir_of(),
        [a](AppState st, const std::wstring&) {
            // worker thread -> UI thread (info unused; face+dot show state)
            PostMessageW(a->hwnd, WM_BRAIN_STATE, (WPARAM)st, 0);
        }, dev, a->cfg.stt_language);
    a->brain->set_tts_voice(a->cfg.tts_voice_id);
    a->brain->set_mic_muted(a->mic_muted);
    a->brain->run();
    a->state = AppState::IDLE;
    InvalidateRect(a->hwnd, nullptr, TRUE);
}

void show_context_menu(App* a) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_API, L"Pengaturan API...");
    AppendMenuW(m, MF_STRING, IDM_MIC, L"Pilih Microphone...");
    AppendMenuW(m, MF_STRING | (a->mic_muted ? MF_CHECKED : MF_UNCHECKED),
                IDM_MIC_TOGGLE,
                a->mic_muted ? L"Mic: Mati (klik untuk menyalakan)"
                             : L"Mic: Nyala (klik untuk mematikan)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_EXIT, L"Keluar");
    SetForegroundWindow(a->hwnd);  // so the menu dismisses correctly
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, a->hwnd, nullptr);
    DestroyMenu(m);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* a = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        a = (App*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)a);
        a->hwnd = hwnd;
        // layered color-key transparency: magenta becomes see-through
        SetLayeredWindowAttributes(hwnd, kKey, 0, LWA_COLORKEY);
        SetTimer(hwnd, kTalkTimer, 125, nullptr);  // 8 fps mouth animation
        recreate_brain(a);
        return 0;
    }
    case WM_NCHITTEST:
        // whole visible face is draggable (transparent pixels are click-through)
        return HTCAPTION;
    case WM_RBUTTONUP:
    case WM_NCRBUTTONUP:
        if (a) show_context_menu(a);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDM_API) {
            AppConfig edited = a->cfg;
            if (show_setup_dialog(a->hInst, edited)) {
                a->cfg = edited;
                recreate_brain(a);
            }
        } else if (id == IDM_MIC) {
            AppConfig edited = a->cfg;
            if (show_mic_dialog(a->hInst, edited)) {
                a->cfg = edited;
                UINT dev = 0;
                if (a->brain &&
                    resolve_mic_device(a->cfg.mic_device_id, a->cfg.mic_device_name, dev))
                    a->brain->set_mic_device(dev);
            }
        } else if (id == IDM_MIC_TOGGLE) {
            a->mic_muted = !a->mic_muted;
            if (a->brain) a->brain->set_mic_muted(a->mic_muted);
            InvalidateRect(hwnd, nullptr, TRUE);
        } else if (id == IDM_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_BRAIN_STATE: {
        a->state = (AppState)wp;
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_TIMER:
        if (wp == kTalkTimer && a->state == AppState::SPEAKING) {
            a->talk_open = !a->talk_open;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;  // we paint everything in WM_PAINT
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        // key-color background -> transparent everywhere except the face
        HBRUSH key = CreateSolidBrush(kKey);
        FillRect(hdc, &rc, key);
        DeleteObject(key);
        // face: opaque white pixels alpha-blend cleanly over the key color,
        // so no magenta fringe appears at the edges
        a->face.draw(hdc, rc, face_for_state(a), kFaceScale);
        // status dot, bottom-right
        COLORREF dc = dot_color(a->state, a->mic_muted);
        HBRUSH b = CreateSolidBrush(dc);
        HPEN p = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HGDIOBJ ob = SelectObject(hdc, b);
        HGDIOBJ op = SelectObject(hdc, p);
        int r = 8, cx = rc.right - 22, cy = rc.bottom - 22;
        Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(b);
        DeleteObject(p);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kTalkTimer);
        if (a->brain) { a->brain->shutdown(); delete a->brain; a->brain = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    AppConfig cfg;
    if (!config_exists() || !config_load(cfg)) {
        AppConfig fresh;
        if (!show_setup_dialog(hInst, fresh)) return 0;  // user cancelled
        cfg = fresh;
    }

    // --- microphone must exist and be selected ---
    if (enum_microphones().empty()) {
        MessageBoxW(nullptr, L"Tidak ada microphone terdeteksi di PC ini. "
                             L"Jarvis butuh microphone untuk mendengarkan.",
                    L"Jarvis", MB_ICONERROR);
        return 1;
    }
    bool have_saved = cfg.mic_device_id >= 0 || !cfg.mic_device_name.empty();
    if (!have_saved || !mic_device_still_present(cfg.mic_device_id, cfg.mic_device_name)) {
        // no saved mic, or the saved one is gone: ask again
        if (!show_mic_dialog(hInst, cfg)) return 0;  // user cancelled
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClass;
    wc.hbrBackground = nullptr;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    App app;
    app.hInst = hInst;
    app.cfg = cfg;
    app.face.init();

    // window size = face bitmap at 4x + padding
    int fw = 0, fh = 0;
    for (int i = 0; i < (int)FaceId::COUNT; ++i) {
        int w, h;
        if (app.face.size((FaceId)i, w, h)) {
            if (w > fw) fw = w;
            if (h > fh) fh = h;
        }
    }
    int win_w = fw * kFaceScale + 2 * kPad;
    int win_h = fh * kFaceScale + 2 * kPad;

    // place bottom-right of the work area
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wa.right - win_w - 24;
    int y = wa.bottom - win_h - 24;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kClass, L"Jarvis", WS_POPUP,
        x, y, win_w, win_h,
        nullptr, nullptr, hInst, &app);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
