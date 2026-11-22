#pragma once

#include "common.h"
#include "syntax.h"

// Loads the built-in completion database embedded as an RCDATA resource.
// Completions are keyed by language; the resource is parsed once on first use.
class CompletionData {
public:
    static const std::vector<std::wstring>& ForLanguage(Language language);

    // Reload from the embedded resource (used by tests and after resource updates).
    static void Reload();

    // Number of languages that have at least one completion entry.
    static size_t LanguageCount();
};
