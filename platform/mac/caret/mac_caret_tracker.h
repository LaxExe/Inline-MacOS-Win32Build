#pragma once

#include "core/interfaces/ICaretTracker.h"
#include "core/interfaces/ISecureFieldDetector.h"
#include <ApplicationServices/ApplicationServices.h>

class MacCaretTracker : public ICaretTracker, public ISecureFieldDetector {
public:
    MacCaretTracker();
    ~MacCaretTracker() override;

    std::optional<CaretRect> getCaretScreenRect() override;
    bool isFocusedFieldSecure() override;

private:
    AXUIElementRef systemWide_ = nullptr;
};
