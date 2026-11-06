#pragma once

#include <string>
#include <vector>

// Returns true if the candidate suggestion is safe to display (no unprompted profanity,
// no PII like emails/URLs/long digit strings, non-empty alphanumeric content).
bool isSuggestionSafe(const std::wstring& suggestion, const std::wstring& recentContext = L"");

// Truncates a suggestion to at most maxWords words and stops at sentence-ending punctuation or newlines.
std::wstring clampSuggestionWords(const std::wstring& suggestion, int maxWords);

// Extracts the portion to insert on Tab when ghostAcceptMode == 1 (One word) vs 0 (Whole suggestion).
// Also outputs the remaining suffix of the suggestion (empty if all consumed).
std::wstring extractAcceptChunk(const std::wstring& suggestion, int acceptMode, std::wstring& outRemaining);
