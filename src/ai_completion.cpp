#include "ai_completion.h"

#include <winhttp.h>

namespace {

// ---------------------------------------------------------------- JSON model
// A small self-contained JSON reader. It only needs to understand the shapes
// returned by OpenAI-compatible completion APIs.
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::wstring text;
    std::vector<JsonValue> array;
    std::unordered_map<std::wstring, JsonValue> object;

    const JsonValue* Get(const wchar_t* key) const {
        if (type != Type::Object) return nullptr;
        auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::wstring& source) : s_(source) {}

    bool Parse(JsonValue& out) {
        SkipSpace();
        if (!ParseValue(out)) return false;
        SkipSpace();
        return pos_ == s_.size();
    }

private:
    const std::wstring& s_;
    size_t pos_ = 0;

    void SkipSpace() {
        while (pos_ < s_.size() && iswspace(s_[pos_])) ++pos_;
    }

    bool Consume(wchar_t c) {
        SkipSpace();
        if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; }
        return false;
    }

    static int Hex(wchar_t c) {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'a' && c <= L'f') return c - L'a' + 10;
        if (c >= L'A' && c <= L'F') return c - L'A' + 10;
        return -1;
    }

    bool ParseUnicode(std::wstring& out) {
        if (pos_ + 4 > s_.size()) return false;
        int code = 0;
        for (int i = 0; i < 4; ++i) {
            int h = Hex(s_[pos_++]);
            if (h < 0) return false;
            code = code * 16 + h;
        }
        if (code >= 0xD800 && code <= 0xDBFF && pos_ + 6 <= s_.size() &&
            s_[pos_] == L'\\' && s_[pos_ + 1] == L'u') {
            size_t save = pos_;
            pos_ += 2;
            int low = 0;
            bool ok = true;
            for (int i = 0; i < 4; ++i) {
                int h = Hex(s_[pos_++]);
                if (h < 0) { ok = false; break; }
                low = low * 16 + h;
            }
            if (ok && low >= 0xDC00 && low <= 0xDFFF) {
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
            } else {
                pos_ = save;
            }
        }
        if (code > 0xFFFF) {
            code -= 0x10000;
            out.push_back((wchar_t)(0xD800 + (code >> 10)));
            out.push_back((wchar_t)(0xDC00 + (code & 0x3FF)));
        } else {
            out.push_back((wchar_t)code);
        }
        return true;
    }

    bool ParseString(std::wstring& out) {
        if (!Consume(L'"')) return false;
        out.clear();
        while (pos_ < s_.size()) {
            wchar_t c = s_[pos_++];
            if (c == L'"') return true;
            if (c != L'\\') { out.push_back(c); continue; }
            if (pos_ >= s_.size()) return false;
            wchar_t esc = s_[pos_++];
            switch (esc) {
                case L'"': out.push_back(L'"'); break;
                case L'\\': out.push_back(L'\\'); break;
                case L'/': out.push_back(L'/'); break;
                case L'b': out.push_back(L'\b'); break;
                case L'f': out.push_back(L'\f'); break;
                case L'n': out.push_back(L'\n'); break;
                case L'r': out.push_back(L'\r'); break;
                case L't': out.push_back(L'\t'); break;
                case L'u': if (!ParseUnicode(out)) return false; break;
                default: out.push_back(esc); break;
            }
        }
        return false;
    }

    bool ParseValue(JsonValue& value) {
        SkipSpace();
        if (pos_ >= s_.size()) return false;
        wchar_t c = s_[pos_];
        if (c == L'"') {
            value.type = JsonValue::Type::String;
            return ParseString(value.text);
        }
        if (c == L'{') {
            value.type = JsonValue::Type::Object;
            ++pos_;
            if (Consume(L'}')) return true;
            while (pos_ < s_.size()) {
                std::wstring key;
                if (!ParseString(key)) return false;
                if (!Consume(L':')) return false;
                JsonValue child;
                if (!ParseValue(child)) return false;
                value.object[key] = std::move(child);
                if (Consume(L'}')) return true;
                if (!Consume(L',')) return false;
            }
            return false;
        }
        if (c == L'[') {
            value.type = JsonValue::Type::Array;
            ++pos_;
            if (Consume(L']')) return true;
            while (pos_ < s_.size()) {
                JsonValue child;
                if (!ParseValue(child)) return false;
                value.array.push_back(std::move(child));
                if (Consume(L']')) return true;
                if (!Consume(L',')) return false;
            }
            return false;
        }
        if (s_.compare(pos_, 4, L"true") == 0) {
            pos_ += 4; value.type = JsonValue::Type::Bool; value.boolean = true; return true;
        }
        if (s_.compare(pos_, 5, L"false") == 0) {
            pos_ += 5; value.type = JsonValue::Type::Bool; value.boolean = false; return true;
        }
        if (s_.compare(pos_, 4, L"null") == 0) {
            pos_ += 4; value.type = JsonValue::Type::Null; return true;
        }
        size_t start = pos_;
        if (c == L'-' || c == L'+' || iswdigit(c)) ++pos_;
        while (pos_ < s_.size() &&
               (iswdigit(s_[pos_]) || s_[pos_] == L'.' || s_[pos_] == L'e' ||
                s_[pos_] == L'E' || s_[pos_] == L'+' || s_[pos_] == L'-')) ++pos_;
        if (pos_ == start) return false;
        value.type = JsonValue::Type::Number;
        value.number = _wtof(s_.substr(start, pos_ - start).c_str());
        return true;
    }
};

std::wstring JsonEscape(const std::wstring& input) {
    std::wstring out;
    out.reserve(input.size() + 8);
    for (wchar_t c : input) {
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            case L'\b': out += L"\\b"; break;
            case L'\f': out += L"\\f"; break;
            default:
                if (c < 0x20) {
                    wchar_t buffer[8];
                    swprintf_s(buffer, L"\\u%04X", (unsigned)c);
                    out += buffer;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

std::wstring Quoted(const std::wstring& value) {
    return L"\"" + JsonEscape(value) + L"\"";
}

// ------------------------------------------------------------ response parse
void CollectTexts(const JsonValue& value, std::vector<std::wstring>& out) {
    if (value.type == JsonValue::Type::String) {
        out.push_back(value.text);
        return;
    }
    if (value.type == JsonValue::Type::Array) {
        for (const auto& item : value.array) CollectTexts(item, out);
        return;
    }
    if (value.type == JsonValue::Type::Object) {
        if (const JsonValue* text = value.Get(L"text")) {
            if (text->type == JsonValue::Type::String) out.push_back(text->text);
        }
        if (const JsonValue* content = value.Get(L"content")) CollectTexts(*content, out);
    }
}

std::vector<std::wstring> ExtractSuggestions(const JsonValue& root, AiProtocol protocol) {
    std::vector<std::wstring> texts;
    if (protocol == AiProtocol::Responses) {
        if (const JsonValue* output = root.Get(L"output")) CollectTexts(*output, texts);
        if (const JsonValue* outputText = root.Get(L"output_text")) {
            if (outputText->type == JsonValue::Type::String) texts.insert(texts.begin(), outputText->text);
        }
    } else if (const JsonValue* choices = root.Get(L"choices")) {
        if (choices->type == JsonValue::Type::Array) {
            for (const JsonValue& choice : choices->array) {
                std::wstring text;
                if (const JsonValue* t = choice.Get(L"text")) {
                    if (t->type == JsonValue::Type::String) text = t->text;
                }
                if (text.empty()) {
                    if (const JsonValue* message = choice.Get(L"message")) {
                        if (const JsonValue* content = message->Get(L"content")) {
                            if (content->type == JsonValue::Type::String) text = content->text;
                            else CollectTexts(*content, texts);
                        }
                    }
                }
                if (!text.empty()) texts.push_back(text);
            }
        }
    }
    return texts;
}

std::wstring ExtractRemainingTokens(const JsonValue& root) {
    static const wchar_t* keys[] = {
        L"remaining_tokens", L"remainingTokens", L"tokens_remaining",
        L"quota_remaining", L"remaining"
    };
    auto scan = [&](const JsonValue& object) -> std::wstring {
        for (const wchar_t* key : keys) {
            if (const JsonValue* value = object.Get(key)) {
                if (value->type == JsonValue::Type::Number) {
                    wchar_t buffer[32];
                    swprintf_s(buffer, L"%lld", (long long)value->number);
                    return buffer;
                }
                if (value->type == JsonValue::Type::String) return value->text;
            }
        }
        return {};
    };
    std::wstring result = scan(root);
    if (!result.empty()) return result;
    if (const JsonValue* usage = root.Get(L"usage")) return scan(*usage);
    return {};
}

std::wstring QueryHeader(HINTERNET request, const wchar_t* name) {
    wchar_t buffer[128]{};
    DWORD size = sizeof(buffer);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, name, buffer, &size, nullptr)) {
        return buffer;
    }
    return {};
}

} // namespace

const wchar_t* AiProtocolName(AiProtocol protocol) {
    switch (protocol) {
        case AiProtocol::Completion: return L"OpenAI Completion";
        case AiProtocol::ChatCompletions: return L"OpenAI Chat Completions";
        case AiProtocol::Responses: return L"OpenAI Responses";
        default: return L"OpenAI Completion";
    }
}

int AiPathProtocol(const std::wstring& baseUrl) {
    const std::wstring lower = ToLower(baseUrl);
    if (IEndsWith(lower, L"/v1/chat/completions")) return (int)AiProtocol::ChatCompletions;
    if (IEndsWith(lower, L"/v1/completions")) return (int)AiProtocol::Completion;
    if (IEndsWith(lower, L"/v1/responses")) return (int)AiProtocol::Responses;
    return -1;
}

std::wstring AiUrlRoot(const std::wstring& baseUrl) {
    std::wstring base = Trim(baseUrl);
    size_t at = base.find(L"://");
    size_t searchFrom = at == std::wstring::npos ? 0 : at + 3;
    size_t v1 = base.find(L"/v1", searchFrom);
    if (v1 != std::wstring::npos) base.resize(v1);
    while (!base.empty() && base.back() == L'/') base.pop_back();
    return base;
}

std::wstring BuildAiEndpoint(const std::wstring& baseUrl, AiProtocol protocol) {
    std::wstring base = Trim(baseUrl);
    if (base.empty()) return {};
    if (!IContains(base, L"://")) base = L"https://" + base;
    while (!base.empty() && base.back() == L'/') base.pop_back();

    const std::wstring lower = ToLower(base);
    const wchar_t* paths[] = {L"/v1/completions", L"/v1/chat/completions", L"/v1/responses"};
    // If the user already supplied a protocol-specific path, honour it as-is.
    for (const wchar_t* path : paths) {
        if (IEndsWith(lower, path)) return base;
    }

    const wchar_t* tails[] = {L"/completions", L"/chat/completions", L"/responses"};
    const wchar_t* tail = tails[(int)protocol];
    if (IEndsWith(lower, L"/v1")) return base + tail;
    return base + L"/v1" + tail;
}

AiCompletionResult RequestAiCompletion(const AiSettings& settings,
                                       const std::wstring& prefix,
                                       const std::wstring& suffix,
                                       Language language) {
    AiCompletionResult result;
    const std::wstring endpoint = BuildAiEndpoint(settings.baseUrl, settings.protocol);
    if (endpoint.empty() || settings.apiKey.empty() || settings.modelId.empty()) {
        result.error = L"AI completion is not configured.";
        return result;
    }

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    std::vector<wchar_t> host(endpoint.size() + 1), path(endpoint.size() + 1);
    parts.lpszHostName = host.data();
    parts.dwHostNameLength = (DWORD)host.size();
    parts.lpszUrlPath = path.data();
    parts.dwUrlPathLength = (DWORD)path.size();
    if (!WinHttpCrackUrl(endpoint.c_str(), (DWORD)endpoint.size(), 0, &parts)) {
        result.error = L"Invalid AI endpoint URL.";
        return result;
    }

    HINTERNET session = WinHttpOpen(L"NeoIDEs/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { result.error = L"Could not initialize HTTP."; return result; }
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 20000);

    HINTERNET connection = WinHttpConnect(session, host.data(), parts.nPort, 0);
    if (!connection) {
        result.error = L"Could not connect to the AI endpoint.";
        WinHttpCloseHandle(session);
        return result;
    }

    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    HINTERNET request = WinHttpOpenRequest(connection, L"POST", path.data(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        result.error = L"Could not create the HTTP request.";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    std::wstring auth = L"Authorization: Bearer " + settings.apiKey;
    WinHttpAddRequestHeaders(request, auth.c_str(), (DWORD)auth.size(), WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(request, L"Content-Type: application/json",
                             (DWORD)wcslen(L"Content-Type: application/json"), WINHTTP_ADDREQ_FLAG_ADD);

    const std::wstring model = Quoted(settings.modelId);
    const std::wstring prompt = Quoted(prefix);
    const std::wstring suffixJson = Quoted(suffix);
    const std::wstring system = Quoted(L"You are a code completion engine. Reply only with the code that completes the given text. Do not include explanations or markdown fences.");
    const std::wstring user = Quoted(L"Complete the following " + std::wstring(LanguageName(language)) +
                                     L" code:\n" + prefix);
    const std::wstring languageHint = Quoted(std::wstring(L"Language: ") + LanguageName(language));

    std::wstring body;
    switch (settings.protocol) {
        case AiProtocol::Completion:
            body = L"{\"model\":" + model + L",\"prompt\":" + prompt +
                   L",\"suffix\":" + suffixJson + L",\"max_tokens\":64," +
                   L"\"temperature\":0.2,\"n\":5}";
            break;
        case AiProtocol::ChatCompletions:
            body = L"{\"model\":" + model + L",\"messages\":[{\"role\":\"system\",\"content\":" +
                   system + L"},{\"role\":\"user\",\"content\":" + user + L"}]," +
                   L"\"max_tokens\":64,\"temperature\":0.2,\"n\":5}";
            break;
        case AiProtocol::Responses:
            body = L"{\"model\":" + model + L",\"instructions\":" + languageHint +
                   L",\"input\":" + prompt + L",\"max_output_tokens\":64}";
            break;
    }

    std::string bytes = WideToUtf8(body);
    BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   (LPVOID)bytes.data(), (DWORD)bytes.size(),
                                   (DWORD)bytes.size(), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        result.error = L"AI request failed.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (status < 200 || status >= 300) {
        result.error = L"AI API returned status " + std::to_wstring(status) + L".";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    std::string responseBytes;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        std::vector<char> chunk(available + 1, 0);
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
        responseBytes.append(chunk.data(), read);
    }

    // Remaining-token info may be provided as a response header.
    std::wstring remaining = QueryHeader(request, L"X-RateLimit-Remaining-Tokens");
    if (remaining.empty()) remaining = QueryHeader(request, L"x-ratelimit-remaining-tokens");

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    JsonValue root;
    JsonParser parser(Utf8ToWide(responseBytes));
    if (!parser.Parse(root)) {
        result.error = L"Could not parse the AI response.";
        return result;
    }

    std::vector<std::wstring> texts = ExtractSuggestions(root, settings.protocol);
    for (std::wstring& text : texts) text = Trim(text);
    std::sort(texts.begin(), texts.end());
    texts.erase(std::unique(texts.begin(), texts.end()), texts.end());
    for (const std::wstring& text : texts) {
        if (!text.empty() && result.suggestions.size() < 12) result.suggestions.push_back(text);
    }
    if (remaining.empty()) remaining = ExtractRemainingTokens(root);
    result.remainingTokens = remaining;
    result.ok = !result.suggestions.empty();
    if (!result.ok) result.error = L"AI returned no completions.";
    return result;
}
