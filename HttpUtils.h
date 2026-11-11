#pragma once
#include "Common.h"

thread_local DWORD t_httpStatus = 0;
thread_local DWORD t_httpError = 0;

inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size, nullptr, nullptr);
    return str;
}

inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size);
    return wstr;
}

inline std::wstring UrlEncodeWide(const std::wstring& wstr) {
    std::string utf8 = WideToUtf8(wstr);
    std::wostringstream escaped;
    escaped.fill(L'0');
    escaped << std::hex << std::uppercase;
    for (unsigned char c : utf8) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << (wchar_t)c;
        }
        else {
            escaped << L'%' << std::setw(2) << (int)c;
        }
    }
    return escaped.str();
}

inline std::string HttpGet(const wchar_t* host, const std::wstring& path) {
    std::string result;
    t_httpStatus = 0;
    t_httpError = 0;

    HINTERNET hSession = WinHttpOpen(L"IslandOverlay/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        t_httpError = GetLastError();
        return result;
    }
    WinHttpSetTimeouts(hSession, 5000, 6000, 8000, 10000);

    HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        t_httpError = GetLastError();
    }
    else {
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            t_httpError = GetLastError();
        }
        else {
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, nullptr)) {
                DWORD status = 0;
                DWORD statusSize = sizeof(status);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
                    t_httpStatus = status;
                }

                DWORD dwSize = 0;
                do {
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                        t_httpError = GetLastError();
                        break;
                    }
                    if (dwSize == 0) break;
                    std::string buffer(dwSize, 0);
                    DWORD dwDownloaded = 0;
                    if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                        result.append(buffer, 0, dwDownloaded);
                    }
                    else {
                        t_httpError = GetLastError();
                        break;
                    }
                } while (dwSize > 0);
            }
            else {
                t_httpError = GetLastError();
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return result;
}

inline std::wstring HttpDiag() {
    if (t_httpError != 0) return L"ошибка WinHTTP " + std::to_wstring(static_cast<unsigned long>(t_httpError));
    if (t_httpStatus != 0 && t_httpStatus != 200) return L"HTTP " + std::to_wstring(static_cast<unsigned long>(t_httpStatus));
    return L"пустой ответ";
}

// --- Мини-парсер JSON ---
inline void AppendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

inline std::string JsonReadStringRaw(const std::string& j, size_t pos) {
    std::string out;
    for (size_t i = pos + 1; i < j.size(); ++i) {
        char c = j[i];
        if (c == '"') break;
        if (c == '\\' && i + 1 < j.size()) {
            char n = j[++i];
            if (n == 'u' && i + 4 < j.size()) {
                unsigned cp = static_cast<unsigned>(strtoul(j.substr(i + 1, 4).c_str(), nullptr, 16));
                i += 4;
                if (cp >= 0xD800 && cp < 0xDC00 && i + 6 < j.size() && j[i + 1] == '\\' && j[i + 2] == 'u') {
                    unsigned lo = static_cast<unsigned>(strtoul(j.substr(i + 3, 4).c_str(), nullptr, 16));
                    if (lo >= 0xDC00 && lo < 0xE000) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        i += 6;
                    }
                }
                AppendUtf8(out, cp);
            }
            else if (n == 'n') out.push_back('\n');
            else if (n == 't') out.push_back('\t');
            else out.push_back(n);
        }
        else {
            out.push_back(c);
        }
    }
    return out;
}

inline size_t JsonFindValue(const std::string& s, const char* key) {
    std::string pat = std::string("\"") + key + "\":";
    size_t p = s.find(pat);
    if (p == std::string::npos) return std::string::npos;
    p += pat.size();
    while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) ++p;
    return p;
}

inline std::string JsonObjectSlice(const std::string& j, const char* key) {
    std::string pat = std::string("\"") + key + "\":{";
    size_t p = j.find(pat);
    if (p == std::string::npos) return "";
    size_t start = p + pat.size();
    size_t end = j.find('}', start);
    if (end == std::string::npos) return "";
    return j.substr(start, end - start);
}

inline bool JsonGetNumber(const std::string& s, const char* key, double& out) {
    size_t p = JsonFindValue(s, key);
    if (p == std::string::npos || p >= s.size()) return false;
    const char* b = s.c_str() + p;
    char* e = nullptr;
    double v = strtod(b, &e);
    if (e == b) return false;
    out = v;
    return true;
}

inline bool JsonGetNumQ(const std::string& s, const char* key, double& out) {
    size_t p = JsonFindValue(s, key);
    if (p == std::string::npos || p >= s.size()) return false;
    if (s[p] == '"') ++p;
    const char* b = s.c_str() + p;
    char* e = nullptr;
    double v = strtod(b, &e);
    if (e == b) return false;
    out = v;
    return true;
}

inline std::string JsonGetStringA(const std::string& s, const char* key) {
    size_t p = JsonFindValue(s, key);
    if (p == std::string::npos || p >= s.size() || s[p] != '"') return "";
    return JsonReadStringRaw(s, p);
}

inline std::wstring JsonGetString(const std::string& s, const char* key) {
    return Utf8ToWide(JsonGetStringA(s, key));
}

inline std::vector<double> JsonGetNumArray(const std::string& s, const char* key) {
    std::vector<double> out;
    size_t p = JsonFindValue(s, key);
    if (p == std::string::npos || p >= s.size() || s[p] != '[') return out;
    ++p;
    while (p < s.size() && s[p] != ']') {
        while (p < s.size() && (s[p] == ' ' || s[p] == ',')) ++p;
        if (p >= s.size() || s[p] == ']') break;
        const char* b = s.c_str() + p;
        char* e = nullptr;
        double v = strtod(b, &e);
        if (e == b) {
            out.push_back(0.0);
            while (p < s.size() && s[p] != ',' && s[p] != ']') ++p;
        }
        else {
            out.push_back(v);
            p += static_cast<size_t>(e - b);
        }
    }
    return out;
}

inline std::vector<std::string> JsonGetStrArray(const std::string& s, const char* key) {
    std::vector<std::string> out;
    size_t p = JsonFindValue(s, key);
    if (p == std::string::npos || p >= s.size() || s[p] != '[') return out;
    ++p;
    while (p < s.size() && s[p] != ']') {
        while (p < s.size() && (s[p] == ' ' || s[p] == ',')) ++p;
        if (p >= s.size() || s[p] == ']') break;
        if (s[p] == '"') {
            out.push_back(JsonReadStringRaw(s, p));
            size_t q = p + 1;
            while (q < s.size() && s[q] != '"') {
                if (s[q] == '\\') ++q;
                ++q;
            }
            p = q + 1;
        }
        else {
            while (p < s.size() && s[p] != ',' && s[p] != ']') ++p;
        }
    }
    return out;
}

inline std::vector<std::string> JsonArrayObjects(const std::string& s, const char* key) {
    std::vector<std::string> out;
    size_t p = JsonFindValue(s, key);
    if (p == std::string::npos || p >= s.size() || s[p] != '[') return out;
    ++p;
    int depth = 0;
    size_t objStart = std::string::npos;
    bool inStr = false;
    for (; p < s.size(); ++p) {
        char c = s[p];
        if (inStr) {
            if (c == '\\') ++p;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') { inStr = true; continue; }
        if (c == '{') {
            if (depth == 0) objStart = p;
            ++depth;
        }
        else if (c == '}') {
            --depth;
            if (depth == 0 && objStart != std::string::npos) {
                out.push_back(s.substr(objStart, p - objStart + 1));
                objStart = std::string::npos;
            }
        }
        else if (c == ']' && depth == 0) {
            break;
        }
    }
    return out;
}