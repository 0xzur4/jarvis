// json_mini.h - tiny JSON helpers for SSE delta extraction and Vosk results.
// Only handles what Jarvis needs: string lookup by key and JSON unescaping.
#pragma once
#include <cstdio>
#include <string>

// Unescape a JSON string body (without surrounding quotes).
inline std::string json_unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char e = s[++i];
            switch (e) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    // \uXXXX -> UTF-8 (handles BMP; surrogate pairs combined)
                    if (i + 4 < s.size()) {
                        unsigned cp = 0;
                        for (int k = 1; k <= 4; ++k) {
                            char h = s[i + k];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                        }
                        i += 4;
                        // combine surrogate pair
                        if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < s.size()
                            && s[i + 1] == '\\' && s[i + 2] == 'u') {
                            unsigned lo = 0;
                            for (int k = 3; k <= 6; ++k) {
                                char h = s[i + k];
                                lo <<= 4;
                                if (h >= '0' && h <= '9') lo |= (h - '0');
                                else if (h >= 'a' && h <= 'f') lo |= (h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F') lo |= (h - 'A' + 10);
                            }
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                i += 6;
                            }
                        }
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        } else if (cp < 0x10000) {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out += (char)(0xF0 | (cp >> 18));
                            out += (char)(0x80 | ((cp >> 12) & 0x3F));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                    }
                    break;
                }
                default: out += e; break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

// Find the raw value span of "key" inside a JSON object. On success sets
// val_begin/val_end to the value range (for strings: WITHOUT quotes, still escaped)
// and returns true. Handles nested objects/arrays by brace matching.
inline bool json_find_value(const std::string& obj, const std::string& key,
                            size_t& val_begin, size_t& val_end) {
    const std::string pat = "\"" + key + "\"";
    size_t pos = 0;
    bool in_str = false, esc = false;
    // Walk the object tracking string state so we only match keys outside strings.
    while (pos < obj.size()) {
        if (!in_str && obj.compare(pos, pat.size(), pat) == 0) {
            size_t p = pos + pat.size();
            while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' ||
                                      obj[p] == '\n' || obj[p] == '\r')) ++p;
            if (p < obj.size() && obj[p] == ':') {
                ++p;
                while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' ||
                                          obj[p] == '\n' || obj[p] == '\r')) ++p;
                if (p >= obj.size()) return false;
                if (obj[p] == '"') {
                    // string value: scan to closing quote
                    size_t q = p + 1;
                    bool e2 = false;
                    while (q < obj.size()) {
                        if (e2) e2 = false;
                        else if (obj[q] == '\\') e2 = true;
                        else if (obj[q] == '"') break;
                        ++q;
                    }
                    if (q >= obj.size()) return false;
                    val_begin = p + 1; val_end = q;
                    return true;
                } else if (obj[p] == '{' || obj[p] == '[') {
                    char open = obj[p], close = (open == '{') ? '}' : ']';
                    int depth = 0;
                    size_t q = p;
                    bool s2 = false, e3 = false;
                    while (q < obj.size()) {
                        char ch = obj[q];
                        if (s2) { if (e3) e3 = false; else if (ch == '\\') e3 = true; else if (ch == '"') s2 = false; }
                        else if (ch == '"') s2 = true;
                        else if (ch == open) ++depth;
                        else if (ch == close) { if (--depth == 0) break; }
                        ++q;
                    }
                    if (q >= obj.size()) return false;
                    val_begin = p; val_end = q + 1;
                    return true;
                } else {
                    // number / true / false / null
                    size_t q = p;
                    while (q < obj.size() && obj[q] != ',' && obj[q] != '}' &&
                           obj[q] != ']' && obj[q] != ' ' && obj[q] != '\n' &&
                           obj[q] != '\r' && obj[q] != '\t') ++q;
                    val_begin = p; val_end = q;
                    return true;
                }
            }
        }
        // advance string tracking
        char c = obj[pos];
        if (in_str) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
        } else if (c == '"') {
            in_str = true;
        }
        ++pos;
    }
    return false;
}

// Get unescaped string value of "key" from a JSON object. Returns false if missing.
inline bool json_get_string(const std::string& obj, const std::string& key, std::string& out) {
    size_t b, e;
    if (!json_find_value(obj, key, b, e)) return false;
    out = json_unescape(obj.substr(b, e - b));
    return true;
}

// Get raw (still-escaped) object/array substring of "key".
inline bool json_get_raw(const std::string& obj, const std::string& key, std::string& out) {
    size_t b, e;
    if (!json_find_value(obj, key, b, e)) return false;
    out = obj.substr(b, e - b);
    return true;
}

// Extract choices[0].delta.content from one SSE "data: {...}" payload line.
inline bool sse_extract_delta(const std::string& data_json, std::string& out) {
    std::string choices;
    if (!json_get_raw(data_json, "choices", choices)) return false;
    // choices is an array; take its first element object.
    size_t ob = choices.find('{');
    if (ob == std::string::npos) return false;
    int depth = 0;
    size_t oe = ob;
    bool s = false, e = false;
    for (; oe < choices.size(); ++oe) {
        char c = choices[oe];
        if (s) { if (e) e = false; else if (c == '\\') e = true; else if (c == '"') s = false; }
        else if (c == '"') s = true;
        else if (c == '{') ++depth;
        else if (c == '}') { if (--depth == 0) break; }
    }
    std::string first = choices.substr(ob, oe - ob + 1);
    std::string delta;
    if (!json_get_raw(first, "delta", delta)) return false;
    return json_get_string(delta, "content", out);
}

// Escape a UTF-8 string for embedding inside a JSON string literal.
inline std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else out += (char)c;
        }
    }
    return out;
}
