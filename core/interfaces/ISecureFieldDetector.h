#pragma once

class ISecureFieldDetector {
public:
    virtual ~ISecureFieldDetector() = default;
    virtual bool isFocusedFieldSecure() = 0;
};
