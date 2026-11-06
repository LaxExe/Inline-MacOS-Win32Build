#pragma once

#include "core/interfaces/ICaretTracker.h"
#include <unordered_set>
#include <mutex>

class MacCaretTracker : public ICaretTracker {
public:
    MacCaretTracker();
    ~MacCaretTracker() override = default;

    std::optional<CaretRect> getCaretScreenRect() override;
    std::optional<CaretGeometry> getCaretGeometry() override;

    static void enableChromiumAccessibilityIfNeeded(int pid);

private:
    std::mutex pidMutex_;
    std::unordered_set<int> enhancedAxPids_;
};
