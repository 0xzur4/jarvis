// atria_client.h - OpenAI-compatible chat completions over WinHTTP, with SSE streaming.
#pragma once
#include <windows.h>
#include <functional>
#include <string>

struct AtriaConfig {
    std::string base_url;  // e.g. https://api.atria-asi.ai/v1
    std::string model;     // e.g. Atria-Dawn-Preview
    std::string api_key;   // UTF-8
};

class AtriaClient {
public:
    explicit AtriaClient(const AtriaConfig& cfg) : cfg_(cfg) {}

    // Streaming chat. on_token receives each delta chunk; return false to abort.
    // Returns true on clean completion ([DONE] or finish_reason), false on error.
    bool chat_stream(const std::string& system_prompt, const std::string& user_text,
                     const std::function<bool(const std::string&)>& on_token,
                     std::string& error);

    // Quick non-streaming probe: POST {"model":m,"messages":[{"role":"user","content":"hi"}]}
    // Success = HTTP 200 and body contains "choices".
    bool test_connection(std::string& error);

private:
    AtriaConfig cfg_;
    bool post(const std::string& path, const std::string& body, bool stream,
              const std::function<bool(const std::string&)>& on_token,
              std::string& full_body, std::string& error, DWORD& http_status);
};
