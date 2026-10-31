#pragma once

#include <functional>
#include <cstdint>

struct RawKeyEvent {
    int64_t keyCode = 0;
    uint64_t flags = 0;
    uint32_t type = 0;
    uint64_t timestampNs = 0;
    bool isAutorepeat = false;
};

class IKeyboardHook {
public:
    virtual ~IKeyboardHook() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void setKeyCallback(std::function<void(const RawKeyEvent&)> cb) = 0;
};
