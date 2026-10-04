// atria_client.cpp - WinHTTP HTTPS client with SSE streaming support.
#include "atria_client.h"
#include "json_mini.h"
#include <windows.h>
#include <winhttp.h>

namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

// Split "https://host[:port]/path..." into host and path. Returns false if not https http(s).
bool split_url(const std::string& url, bool& secure, std::string& host, std::string& path) {
    std::string u = url;
    secure = true;
    if (u.compare(0, 8, "https://") == 0) { u = u.substr(8); secure = true; }
    else if (u.compare(0, 7, "http://") == 0) { u = u.substr(7); secure = false; }
    else return false;
    size_t slash = u.find('/');
    if (slash == std::string::npos) { host = u; path = "/"; }
    else { host = u.substr(0, slash); path = u.substr(slash); }
    return !host.empty();
}

std::string trim_cr(const std::string& s) {
    if (!s.empty() && s.back() == '\r') return s.substr(0, s.size() - 1);
    return s;
}

}  // namespace

bool AtriaClient::post(const std::string& path, const std::string& body, bool stream,
                       const std::function<bool(const std::string&)>& on_token,
                       std::string& full_body, std::string& error, DWORD& http_status) {
    bool secure;
    std::string host, base_path;
    if (!split_url(cfg_.base_url, secure, host, base_path)) {
        error = "Bad base URL: " + cfg_.base_url;
        return false;
    }
    std::string full_path = base_path;
    if (!full_path.empty() && full_path.back() == '/' && !path.empty() && path.front() == '/')
        full_path.pop_back();
    full_path += path;

    HINTERNET hSession = WinHttpOpen(L"Jarvis/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { error = "WinHttpOpen failed"; return false; }

    bool ok = false;
    HINTERNET hConn = nullptr, hReq = nullptr;
    // Extract port if present in host.
    std::wstring whost = widen(host);
    std::wstring wserver = whost;
    INTERNET_PORT port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    size_t colon = whost.find(L':');
    if (colon != std::wstring::npos) {
        port = (INTERNET_PORT)_wtoi(whost.substr(colon + 1).c_str());
        wserver = whost.substr(0, colon);
    }

    hConn = WinHttpConnect(hSession, wserver.c_str(), port, 0);
    if (!hConn) { error = "WinHttpConnect failed"; goto done; }

    hReq = WinHttpOpenRequest(hConn, L"POST", widen(full_path).c_str(), nullptr,
                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                             secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) { error = "WinHttpOpenRequest failed"; goto done; }

    {
        std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " +
                               widen(cfg_.api_key) + L"\r\n";
        if (!WinHttpSendRequest(hReq, headers.c_str(), (DWORD)headers.size(),
                                (LPVOID)body.data(), (DWORD)body.size(),
                                (DWORD)body.size(), 0)) {
            error = "WinHttpSendRequest failed";
            goto done;
        }
    }
    if (!WinHttpReceiveResponse(hReq, nullptr)) { error = "WinHttpReceiveResponse failed"; goto done; }

    {
        DWORD sz = sizeof(http_status);
        if (!WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &http_status, &sz,
                                 WINHTTP_NO_HEADER_INDEX)) {
            error = "WinHttpQueryHeaders failed";
            goto done;
        }
    }

    {
        std::string carry;   // incomplete line from previous chunk
        char buf[8192];
        DWORD got = 0;
        bool aborted = false;
        while (WinHttpReadData(hReq, buf, sizeof(buf), &got) && got > 0 && !aborted) {
            std::string chunk(buf, got);
            full_body += chunk;
            if (!stream || !on_token) continue;
            chunk = carry + chunk;
            carry.clear();
            size_t start = 0;
            while (true) {
                size_t nl = chunk.find('\n', start);
                if (nl == std::string::npos) { carry = chunk.substr(start); break; }
                std::string line = trim_cr(chunk.substr(start, nl - start));
                start = nl + 1;
                if (line.compare(0, 5, "data:") == 0) {
                    std::string payload = line.substr(5);
                    // trim leading space
                    size_t ns = payload.find_first_not_of(' ');
                    if (ns != std::string::npos) payload = payload.substr(ns);
                    if (payload == "[DONE]") { aborted = true; break; }
                    std::string delta;
                    if (sse_extract_delta(payload, delta) && !delta.empty()) {
                        if (!on_token(delta)) { aborted = true; break; }
                    }
                }
            }
        }
    }
    ok = true;

done:
    if (hReq) WinHttpCloseHandle(hReq);
    if (hConn) WinHttpCloseHandle(hConn);
    if (hSession) WinHttpCloseHandle(hSession);
    return ok;
}

bool AtriaClient::chat_stream(const std::string& system_prompt, const std::string& user_text,
                              const std::function<bool(const std::string&)>& on_token,
                              std::string& error) {
    std::string body = "{\"model\":\"" + json_escape(cfg_.model) + "\",\"stream\":true,"
                       "\"messages\":["
                       "{\"role\":\"system\",\"content\":\"" + json_escape(system_prompt) + "\"},"
                       "{\"role\":\"user\",\"content\":\"" + json_escape(user_text) + "\"}"
                       "]}";
    std::string full;
    DWORD status = 0;
    if (!post("/chat/completions", body, true, on_token, full, error, status)) return false;
    if (status != 200) {
        error = "HTTP " + std::to_string(status) + ": " + full.substr(0, 300);
        return false;
    }
    return true;
}

bool AtriaClient::test_connection(std::string& error) {
    std::string body = "{\"model\":\"" + json_escape(cfg_.model) + "\","
                       "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}";
    std::string full;
    DWORD status = 0;
    if (!post("/chat/completions", body, false, nullptr, full, error, status)) return false;
    if (status != 200) {
        error = "HTTP " + std::to_string(status);
        if (status == 401) error += " - API key invalid or revoked";
        else if (status == 429) error += " - rate limited / quota exceeded";
        return false;
    }
    if (full.find("\"choices\"") == std::string::npos) {
        error = "Unexpected response (no 'choices' field)";
        return false;
    }
    return true;
}
