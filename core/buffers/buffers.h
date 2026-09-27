#pragma once

#include <string>
#include <deque>
#include <vector>

struct CurrentWordBuffer {
    std::wstring chars;
};

struct ContextWindow {
    std::deque<std::wstring> words;
};

struct SentenceBuffer {
    std::vector<std::wstring> words;
    int generationToken = 0;
};
