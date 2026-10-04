// brain.cpp - worker thread: LISTEN -> (local cmd | THINK streaming -> SPEAK) -> IDLE.
#include "brain.h"
#include "audio_in.h"
#include "stt_whisper.h"
#include "tts_sapi.h"
#include "syscontrol.h"
#include <windows.h>
#include <cwctype>
#include <thread>
#include <vector>

namespace {

// Well-known app aliases (lowercase) -> executable name resolvable via
// App Paths registry / PATH. Case-insensitive match on the spoken target.
const std::pair<const wchar_t*, const wchar_t*> kAppAlias[] = {
    {L"visual studio code", L"Code"},
    {L"vs code",            L"Code"},
    {L"vscode",             L"Code"},
    {L"notepad",            L"notepad"},
    {L"chrome",             L"chrome"},
    {L"google chrome",      L"chrome"},
    {L"firefox",            L"firefox"},
    {L"edge",               L"msedge"},
    {L"microsoft edge",     L"msedge"},
    {L"calculator",         L"calc"},
    {L"kalkulator",         L"calc"},
    {L"paint",              L"mspaint"},
    {L"cmd",                L"cmd"},
    {L"command prompt",     L"cmd"},
    {L"powershell",         L"powershell"},
    {L"explorer",           L"explorer"},
    {L"file explorer",      L"explorer"},
    {L"word",               L"winword"},
    {L"excel",              L"excel"},
    {L"spotify",            L"spotify"},
    {L"telegram",           L"telegram"},
    {L"discord",            L"discord"},
};

std::wstring resolve_app_name(const std::wstring& target_lower) {
    for (const auto& a : kAppAlias) {
        if (target_lower == a.first) return a.second;
    }
    return target_lower;  // try as-is; ShellExecute + App Paths may resolve it
}

const char* kSystemPrompt =
    "You are Jarvis, a helpful desktop AI assistant like in Iron Man movies. "
    "The user speaks Indonesian and English - always reply in the same language the user used. "
    "Keep responses SHORT (1-3 sentences) because they will be spoken aloud. "
    "Be warm and concise.";

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring lower(std::wstring s) {
    for (auto& c : s) c = towlower(c);
    return s;
}
std::wstring trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Split accumulated text into speakable sentences. Returns complete sentences,
// leaving the incomplete tail in buf.
std::vector<std::wstring> take_sentences(std::wstring& buf) {
    std::vector<std::wstring> out;
    while (true) {
        size_t cut = std::wstring::npos;
        for (size_t i = 0; i < buf.size(); ++i) {
            wchar_t c = buf[i];
            if (c == L'\n' || ((c == L'.' || c == L'!' || c == L'?') &&
                (i + 1 == buf.size() || buf[i + 1] == L' ' || buf[i + 1] == L'\n' ||
                 buf[i + 1] == L'\r' || buf[i + 1] == L'"'))) {
                cut = i + 1;
                break;
            }
        }
        // latency guard: flush long buffers at a word boundary
        if (cut == std::wstring::npos && buf.size() > 160) {
            size_t sp = buf.find_last_of(L" ", 200);
            if (sp != std::wstring::npos && sp > 0) cut = sp + 1;
            else cut = 160;  // pathological: no space at all, hard cut
        }
        if (cut == std::wstring::npos) break;
        std::wstring s = trim(buf.substr(0, cut));
        buf.erase(0, cut);
        if (!s.empty()) out.push_back(s);
    }
    return out;
}

std::wstring strip_trailing_punct(const std::wstring& s) {
    size_t b = s.find_last_not_of(L".,!?\"'`:;");
    if (b == std::wstring::npos) return L"";
    return s.substr(0, b + 1);
}

}  // namespace

Brain::Brain(const AtriaConfig& cfg, const std::wstring& exe_dir, StateCb cb,
             UINT mic_device, const std::string& stt_lang)
    : cfg_(cfg), exe_dir_(exe_dir), cb_(std::move(cb)), mic_device_(mic_device) {
    set_stt_language(stt_lang);
}
Brain::~Brain() { shutdown(); }

void Brain::run() {
    std::lock_guard<std::mutex> lk(run_mutex_);
    if (running_) return;
    running_ = true;
    stop_ = false;
    worker_ = std::thread([this] { loop_main(); });
}

void Brain::shutdown() {
    stop_ = true;
    std::lock_guard<std::mutex> lk(run_mutex_);
    if (running_) {
        if (worker_.joinable()) worker_.join();
        running_ = false;
    }
}

void Brain::set_mic_device(UINT id) { mic_device_ = id; }
void Brain::set_mic_muted(bool m) { mic_muted_ = m; }

void Brain::set_stt_language(const std::string& lang) {
    std::lock_guard<std::mutex> lk(lang_mutex_);
    stt_lang_ = (lang == "en" || lang == "auto") ? lang : "id";
}

void Brain::set_tts_voice(const std::wstring& token_id) {
    std::lock_guard<std::mutex> lk(lang_mutex_);
    tts_voice_id_ = token_id;
}

std::string Brain::current_lang() {
    std::lock_guard<std::mutex> lk(lang_mutex_);
    return stt_lang_;
}

std::wstring Brain::current_voice() {
    std::lock_guard<std::mutex> lk(lang_mutex_);
    return tts_voice_id_;
}

bool Brain::handle_local_command(const std::wstring& transcript, std::wstring& spoken) {
    std::wstring orig = trim(transcript);
    std::wstring t = lower(orig);
    bool indonesian = (current_lang() != "en");

    // "buka <app>" / "open <app>" - tolerant: keyword may appear anywhere
    // ("tolong buka visual studio code"). Target = text after the keyword.
    for (const wchar_t* kw : {L"buka ", L"open "}) {
        size_t pos = t.find(kw);
        if (pos == std::wstring::npos) continue;
        std::wstring target = strip_trailing_punct(trim(orig.substr(pos + wcslen(kw))));
        if (target.empty()) continue;
        std::wstring app = resolve_app_name(lower(target));
        bool ok = sys_open_app(app);
        if (!ok) {
            // never fail silently: say so out loud
            spoken = indonesian ? L"Maaf, saya tidak bisa membuka " + target + L"."
                                : L"Sorry, I could not open " + target + L".";
        } else {
            spoken = indonesian ? L"Membuka " + target + L"."
                                : L"Opening " + target + L".";
        }
        return true;
    }

    // "volume <0-100>" / "volume ke <0-100>"
    auto try_volume = [&](const std::wstring& kw) -> bool {
        if (t.compare(0, kw.size(), kw) != 0) return false;
        std::wstring rest = trim(t.substr(kw.size()));
        if (rest.compare(0, 3, L"ke ") == 0) rest = trim(rest.substr(3));
        try {
            int v = std::stoi(rest);
            bool ok = sys_set_volume(v);
            if (v < 0) v = 0; if (v > 100) v = 100;
            spoken = ok ? (indonesian ? L"Volume diatur ke " + std::to_wstring(v) + L" persen."
                                      : L"Volume set to " + std::to_wstring(v) + L" percent.")
                        : (indonesian ? L"Gagal mengatur volume." : L"Could not set volume.");
            return true;
        } catch (...) { return false; }
    };
    if (try_volume(L"volume")) return true;

    // "kunci" / "kunci layar" / "lock"
    if (t == L"kunci" || t == L"kunci layar" || t == L"kunci komputer") {
        spoken = sys_lock_workstation() ? L"Mengunci layar." : L"Gagal mengunci layar.";
        return true;
    }
    if (t == L"lock" || t == L"lock screen" || t == L"lock workstation") {
        spoken = sys_lock_workstation() ? L"Locking the screen." : L"Could not lock the screen.";
        return true;
    }
    return false;
}

void Brain::loop_main() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // --- init engines once ---
    WhisperSTT stt;
    std::string err;
    std::wstring model_path_w = exe_dir_ + L"\\models\\whisper-base.bin";
    std::string lang = current_lang();
    bool stt_ok = stt.init(narrow(model_path_w), lang, err);
    SapiTTS tts;
    bool tts_ok = stt_ok && tts.init(err);
    std::string tts_lang;
    if (tts_ok) {
        // Manual voice choice wins; otherwise fall back to language matching.
        // (A stale token id, e.g. voice uninstalled, falls through too.)
        std::wstring manual = current_voice();
        bool voice_set = !manual.empty() && tts.set_voice_by_id(manual);
        if (!voice_set) tts.set_language(lang);
        tts_lang = lang;
    }

    if (!stt_ok || !tts_ok) {
        std::wstring msg = widen("Jarvis tidak bisa jalan: " + err);
        MessageBoxW(nullptr, msg.c_str(), L"Jarvis", MB_ICONERROR);
        cb_(AppState::IDLE, msg);
        CoUninitialize();
        return;
    }

    AtriaClient client(cfg_);
    AudioCapture audio;

    while (!stop_) {
        // --- mic muted: idle-wait, keep UI informed ---
        if (mic_muted_.load()) {
            cb_(AppState::IDLE, L"Mic dimatikan.");
            for (int i = 0; i < 10 && !stop_ && mic_muted_.load(); ++i)
                Sleep(200);
            continue;
        }

        // --- LISTEN: capture one utterance (mic open only here) ---
        cb_(AppState::LISTENING, L"");
        std::vector<int16_t> pcm;
        bool heard = audio.record_utterance(pcm, mic_device_.load(), 30000, &stop_);
        if (stop_) break;
        if (!heard || pcm.empty()) continue;  // silence timeout: loop quietly

        // --- speech to text (whisper; language may have changed in settings) ---
        std::string text;
        std::string cur = current_lang();
        stt.set_language(cur);
        // Manual voice choice always wins over automatic language matching.
        if (cur != tts_lang) {
            if (current_voice().empty()) tts.set_language(cur);
            tts_lang = cur;
        }
        if (!stt.transcribe(pcm.data(), pcm.size(), text) || text.empty())
            continue;  // unrecognized: back to listening
        std::wstring transcript = widen(text);
        cb_(AppState::THINKING, transcript);

        // --- local commands first (no LLM round-trip) ---
        // (mic is already closed here: record_utterance returned)
        std::wstring local_reply;
        if (handle_local_command(transcript, local_reply)) {
            cb_(AppState::SPEAKING, local_reply);
            tts.speak(local_reply);
            continue;  // back to LISTEN automatically
        }

        // --- LLM streaming -> speak per sentence ---
        std::wstring pending;
        std::wstring full_reply;
        bool stream_ok = false;
        std::string stream_err;

        auto on_token = [&](const std::string& tok) -> bool {
            if (stop_) return false;
            pending += widen(tok);
            for (auto& s : take_sentences(pending)) {
                if (stop_) return false;
                full_reply += s + L" ";
                cb_(AppState::SPEAKING, full_reply);
                tts.speak(s);  // mic stays closed: no self-trigger
                if (!stop_) cb_(AppState::THINKING, full_reply);
            }
            return true;
        };

        stream_ok = client.chat_stream(kSystemPrompt, text, on_token, stream_err);

        std::wstring tail = trim(pending);
        if (!tail.empty() && !stop_) {
            full_reply += tail;
            cb_(AppState::SPEAKING, full_reply);
            tts.speak(tail);
        }
        if (!stream_ok && !stop_) {
            std::wstring msg = widen("Maaf, ada gangguan koneksi AI: " + stream_err);
            cb_(AppState::SPEAKING, msg);
            tts.speak(msg);
        }
        // loop back to LISTEN automatically - no button needed
    }
    cb_(AppState::IDLE, L"");
    CoUninitialize();
}
