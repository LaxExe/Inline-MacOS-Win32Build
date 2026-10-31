#include "core/ghost/ghost_layout.h"
#include <algorithm>
#include <cmath>

static double defaultMeasureText(const std::wstring& text, const std::string&, double fontSize) {
    // Fallback proportional character width estimate (~0.54 * fontSize per glyph)
    return static_cast<double>(text.size()) * (fontSize * 0.54);
}

GhostLayoutResult computeGhostLayout(
    const std::wstring& suggestion,
    const CaretGeometry& geom,
    bool isMidWord,
    const TextWidthMeasureFn& measureFn,
    int styleMode
) {
    GhostLayoutResult res;
    res.styleMode = styleMode;
    if (suggestion.empty()) return res;

    const double caretH = (geom.caret.height >= 6.0 && geom.caret.height <= 200.0) ? geom.caret.height : 18.0;
    res.lineHeight = (geom.lineHeight >= 6.0 && geom.lineHeight <= 240.0) ? geom.lineHeight : caretH;
    res.fontSize = (geom.fontSize >= 6.0 && geom.fontSize <= 144.0)
                 ? geom.fontSize
                 : ((caretH <= 21.0) ? 13.0 : std::clamp(std::round(caretH * 0.72), 13.0, 48.0));
    res.fontName = geom.fontName;

    auto measure = [&](const std::wstring& s) -> double {
        if (measureFn) return measureFn(s, res.fontName, res.fontSize);
        return defaultMeasureText(s, res.fontName, res.fontSize);
    };

    // Place suggestion snug against the cursor for mid-word completions or tokens without leading space,
    // avoiding artificial gaps between the typed characters and the inline suggestion.
    const bool snugToCaret = isMidWord || (!suggestion.empty() && suggestion.front() != L' ');
    const double caretOffset = snugToCaret ? 0.5 : std::max(0.0, geom.caret.width);
    const double caretRightX = geom.caret.x + caretOffset;
    const double fieldLeft = (geom.field.width > 20.0) ? geom.field.x : (caretRightX - 40.0);
    const double fieldRight = (geom.field.width > 20.0) ? (geom.field.x + geom.field.width) : (caretRightX + 500.0);
    const double lineStartX = (geom.lineStartX >= fieldLeft && geom.lineStartX <= caretRightX + 4.0)
                            ? geom.lineStartX
                            : std::max(fieldLeft + 4.0, caretRightX - 200.0);

    const double padL = std::clamp(lineStartX - fieldLeft, 2.0, 24.0);
    const double rightEdge = std::max(caretRightX, fieldRight - padL);

    const double firstLineWidth = rightEdge - caretRightX;
    const double restLineWidth = std::max(40.0, rightEdge - lineStartX);

    const bool canWrap = !geom.isSingleLine && geom.caretAtEOF && (restLineWidth >= 40.0);
    int maxLines = 1;
    if (canWrap) {
        if (geom.field.height > res.lineHeight * 1.5) {
            double availH = (geom.field.y + geom.field.height) - geom.caret.y;
            maxLines = std::clamp(static_cast<int>(std::floor(availH / res.lineHeight)), 1, 4);
        } else {
            maxLines = 3;
        }
    }

    // Split suggestion into words while preserving leading space on the first word if present
    std::vector<std::wstring> words;
    {
        size_t i = 0;
        std::wstring leadingSpaces;
        while (i < suggestion.size() && suggestion[i] == L' ') {
            leadingSpaces.push_back(L' ');
            ++i;
        }
        bool first = true;
        while (i < suggestion.size()) {
            size_t start = i;
            while (i < suggestion.size() && suggestion[i] != L' ') ++i;
            if (i > start) {
                std::wstring w = suggestion.substr(start, i - start);
                if (first && !leadingSpaces.empty()) {
                    w = leadingSpaces + w;
                }
                words.push_back(std::move(w));
                first = false;
            }
            while (i < suggestion.size() && suggestion[i] == L' ') ++i;
        }
    }
    if (words.empty()) return res;

    int curLineIdx = 0;
    std::wstring curLineText;
    double curLineX = caretRightX;
    double curLineMaxW = firstLineWidth;

    auto flushLine = [&]() {
        if (curLineText.empty()) return;
        GhostLineRun run;
        run.x = curLineX;
        run.y = geom.caret.y + curLineIdx * res.lineHeight;
        run.width = std::max(4.0, measure(curLineText) + 2.0);
        run.height = res.lineHeight;
        run.text = curLineText;
        res.lines.push_back(std::move(run));
        curLineText.clear();
    };

    for (size_t wi = 0; wi < words.size(); ++wi) {
        const std::wstring& word = words[wi];
        std::wstring candidate = curLineText.empty() ? word : (curLineText + L" " + word);
        double candW = measure(candidate);

        if (candW <= curLineMaxW) {
            curLineText = std::move(candidate);
            if (!res.fittedText.empty()) res.fittedText += L" ";
            res.fittedText += word;
            continue;
        }

        // First token is mid-word suffix and doesn't fit on line 1 -> cannot wrap half a word cleanly
        if (wi == 0 && isMidWord) {
            return GhostLayoutResult{};
        }

        // Cannot wrap to next line -> stop at the words that already fit on the current line
        if (!canWrap || (curLineIdx + 1) >= maxLines) {
            break;
        }

        // Wrap to next line at lineStartX
        flushLine();
        curLineIdx++;
        curLineX = lineStartX;
        curLineMaxW = restLineWidth;

        // Strip leading space when a word starts a wrapped line visually
        std::wstring lineWord = word;
        while (!lineWord.empty() && lineWord.front() == L' ') {
            lineWord.erase(lineWord.begin());
        }
        if (lineWord.empty()) continue;

        double wWidth = measure(lineWord);
        if (wWidth > curLineMaxW) {
            break;
        }
        curLineText = lineWord;
        if (!res.fittedText.empty()) {
            res.fittedText += L" " + lineWord;
        } else {
            res.fittedText = word; // keep original leading space for Tab insertion if first word wrapped
        }
    }

    flushLine();

    if (res.lines.empty() || res.fittedText.empty()) {
        return GhostLayoutResult{};
    }

    // Compute union boundingBox across all line runs
    double minX = res.lines[0].x;
    double minY = res.lines[0].y;
    double maxX = res.lines[0].x + res.lines[0].width;
    double maxY = res.lines[0].y + res.lines[0].height;
    for (size_t i = 1; i < res.lines.size(); ++i) {
        minX = std::min(minX, res.lines[i].x);
        minY = std::min(minY, res.lines[i].y);
        maxX = std::max(maxX, res.lines[i].x + res.lines[i].width);
        maxY = std::max(maxY, res.lines[i].y + res.lines[i].height);
    }
    res.boundingBox = {minX, minY, maxX - minX, maxY - minY};
    res.valid = true;
    return res;
}
