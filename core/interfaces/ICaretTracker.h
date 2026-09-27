#pragma once

#include <optional>

struct CaretRect {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

class ICaretTracker {
public:
    virtual ~ICaretTracker() = default;
    virtual std::optional<CaretRect> getCaretScreenRect() = 0;
};
