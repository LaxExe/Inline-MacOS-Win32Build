#pragma once

#include <string>

class GhostOverlayWindow {
public:
    GhostOverlayWindow();
    ~GhostOverlayWindow();

    void showGhostText(const std::wstring& text, double screenX, double screenY, double height);
    void hide();
    bool isVisible() const;

private:
    void* nativeWindow_ = nullptr;
    void* pillView_ = nullptr;
};
