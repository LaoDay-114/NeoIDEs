#pragma once

#include "common.h"
#include "syntax.h"

// The three OpenAI-compatible completion protocols supported by the client.
enum class AiProtocol {
    Completion = 0,        // POST /v1/completions       (default)
    ChatCompletions = 1,   // POST /v1/chat/completions
    Responses = 2          // POST /v1/responses
};

const wchar_t* AiProtocolName(AiProtocol protocol);

struct AiSettings {
    bool enabled = false;
    std::wstring baseUrl;
    std::wstring apiKey;
    std::wstring modelId = L"gpt-3.5-turbo-instruct";
    AiProtocol protocol = AiProtocol::Completion;
};

struct AiCompletionResult {
    bool ok = false;
    std::vector<std::wstring> suggestions;
    // Set only when the API response reports remaining-token information.
    std::wstring remainingTokens;
    std::wstring error;
};

// Builds the full request URL from a base URL and the selected protocol.
// If the base URL already ends with a known protocol path, it is left intact;
// otherwise the protocol-specific path is appended.
std::wstring BuildAiEndpoint(const std::wstring& baseUrl, AiProtocol protocol);

// Returns the protocol whose path is already embedded in the URL, or -1 when
// no known path is present. Used to warn about a URL/protocol mismatch.
int AiPathProtocol(const std::wstring& baseUrl);

// Removes any embedded "/v1..." suffix so the selected protocol path can be
// appended cleanly, e.g. "https://x.com/v1/completions" -> "https://x.com".
std::wstring AiUrlRoot(const std::wstring& baseUrl);

// Performs one synchronous request. Blocking; callers run it on a worker thread.
AiCompletionResult RequestAiCompletion(const AiSettings& settings,
                                       const std::wstring& prefix,
                                       const std::wstring& suffix,
                                       Language language);
