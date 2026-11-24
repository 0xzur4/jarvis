// brain.cpp - worker thread: LISTEN -> (local cmd | THINK streaming -> SPEAK) -> IDLE.
#include "brain.h"
#include "audio_in.h"
#include "stt_whisper.h"
#include "tts_sapi.h"
#include "syscontrol.h"
#include <windows.h>
#include <condition_variable>
#include <cwctype>
#include <queue>
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

// Strip optional leading wake/politeness prefixes so commands must appear at
// the start of the utterance (avoids false positives like "what is open source").
std::wstring strip_command_prefix(std::wstring s) {
    static const wchar_t* kPrefixes[] = {
        L"hey jarvis", L"hei jarvis", L"halo jarvis", L"hi jarvis", L"jarvis",
        L"tolong", L"coba", L"bisa tolong", L"bisa",
        L"please", L"can you", L"could you",
    };
    bool changed = true;
    while (changed) {
        changed = false;
        s = trim(s);
        while (!s.empty() && (s.front() == L',' || s.front() == L':' || s.front() == L'-')) {
            s.erase(0, 1);
            s = trim(s);
        }
        for (const wchar_t* p : kPrefixes) {
            size_t len = wcslen(p);
            if (s.size() > len && s.compare(0, len, p) == 0 &&
                (s[len] == L' ' || s[len] == L',' || s[len] == L':')) {
                s = trim(s.substr(len));
                changed = true;
                break;
            }
        }
    }
    return s;
}

int count_words(const std::wstring& s) {
    int count = 0;
    bool in_word = false;
    for (wchar_t c : s) {
        if (iswspace(c)) in_word = false;
        else if (!in_word) { in_word = true; ++count; }
    }
    return count;
}

bool file_exists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring resolve_whisper_model(const std::wstring& exe_dir) {
    std::wstring primary = exe_dir + L"\\models\\ggml-base.bin";
    if (file_exists(primary)) return primary;
    std::wstring legacy = exe_dir + L"\\models\\whisper-base.bin";
    if (file_exists(legacy)) return legacy;
    return primary;
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

void Brain::clear_history() {
    std::lock_guard<std::mutex> lk(history_mutex_);
    history_.clear();
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
    std::wstring orig = strip_trailing_punct(trim(transcript));
    std::wstring t = strip_command_prefix(lower(orig));
    bool indonesian = (current_lang() != "en");

    // "buka <app>" / "open <app>" - must be at the start of the command (after
    // optional wake/politeness prefix) and target at most 4 words.
    for (const wchar_t* kw : {L"buka ", L"open "}) {
        size_t kwlen = wcslen(kw);
        if (t.compare(0, kwlen, kw) != 0) continue;
        std::wstring target = strip_trailing_punct(trim(t.substr(kwlen)));
        if (target.empty() || count_words(target) > 4) continue;
        std::wstring app = resolve_app_name(target);
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

    // "volume <0-100>" / "volume ke <0-100>" / "set volume <0-100>"
    auto try_volume = [&](const std::wstring& kw) -> bool {
        if (t.compare(0, kw.size(), kw) != 0) return false;
        std::wstring rest = trim(t.substr(kw.size()));
        if (rest.compare(0, 3, L"ke ") == 0) rest = trim(rest.substr(3));
        else if (rest.compare(0, 3, L"to ") == 0) rest = trim(rest.substr(3));
        try {
            int v = std::stoi(rest);
            bool ok = sys_set_volume(v);
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            spoken = ok ? (indonesian ? L"Volume diatur ke " + std::to_wstring(v) + L" persen."
                                      : L"Volume set to " + std::to_wstring(v) + L" percent.")
                        : (indonesian ? L"Gagal mengatur volume." : L"Could not set volume.");
            return true;
        } catch (...) { return false; }
    };
    if (try_volume(L"volume") || try_volume(L"set volume") || try_volume(L"atur volume")) return true;

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
    std::wstring model_path_w = resolve_whisper_model(exe_dir_);
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

        // --- LLM streaming (producer thread) -> speak per sentence (COM STA thread) ---
        // Running WinHTTP SSE on a separate thread prevents blocking SAPI TTS
        // from stalling or timing out the HTTP stream.
        std::vector<ChatMessage> req_messages;
        {
            std::lock_guard<std::mutex> lk(history_mutex_);
            req_messages = history_;
        }
        req_messages.push_back({"user", text});

        std::mutex q_mtx;
        std::condition_variable q_cv;
        std::queue<std::wstring> tts_queue;
        bool stream_done = false;
        bool stream_ok = false;
        std::string stream_err;
        std::string full_reply_utf8;

        std::thread sse_thread([&] {
            std::wstring pending;
            auto on_token = [&](const std::string& tok) -> bool {
                if (stop_) return false;
                full_reply_utf8 += tok;
                pending += widen(tok);
                auto sentences = take_sentences(pending);
                if (!sentences.empty()) {
                    {
                        std::lock_guard<std::mutex> lk(q_mtx);
                        for (auto& s : sentences) tts_queue.push(std::move(s));
                    }
                    q_cv.notify_one();
                }
                return true;
            };

            stream_ok = client.chat_stream(kSystemPrompt, req_messages, on_token, stream_err);

            std::wstring tail = trim(pending);
            {
                std::lock_guard<std::mutex> lk(q_mtx);
                if (!tail.empty() && !stop_) tts_queue.push(std::move(tail));
                stream_done = true;
            }
            q_cv.notify_one();
        });

        std::wstring spoken_so_far;
        while (true) {
            std::wstring sentence;
            {
                std::unique_lock<std::mutex> lk(q_mtx);
                q_cv.wait(lk, [&] { return !tts_queue.empty() || stream_done || stop_.load(); });
                if (stop_) break;
                if (tts_queue.empty()) {
                    if (stream_done) break;
                    continue;
                }
                sentence = std::move(tts_queue.front());
                tts_queue.pop();
            }
            if (!spoken_so_far.empty()) spoken_so_far += L" ";
            spoken_so_far += sentence;
            cb_(AppState::SPEAKING, spoken_so_far);
            tts.speak(sentence);  // mic stays closed: no self-trigger
            if (stop_) break;
        }

        if (stop_) tts.stop();
        if (sse_thread.joinable()) sse_thread.join();

        if (!stream_ok && !stop_) {
            std::wstring msg = widen("Maaf, ada gangguan koneksi AI: " + stream_err);
            cb_(AppState::SPEAKING, msg);
            tts.speak(msg);
        } else if (stream_ok && !stop_ && !full_reply_utf8.empty()) {
            constexpr size_t kMaxHistoryMessages = 20;  // 10 user+assistant turns
            std::lock_guard<std::mutex> lk(history_mutex_);
            history_.push_back({"user", text});
            history_.push_back({"assistant", full_reply_utf8});
            if (history_.size() > kMaxHistoryMessages) {
                history_.erase(history_.begin(),
                               history_.begin() + (history_.size() - kMaxHistoryMessages));
            }
        }
        // loop back to LISTEN automatically - no button needed
    }
    cb_(AppState::IDLE, L"");
    CoUninitialize();
}
