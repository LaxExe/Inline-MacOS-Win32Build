#include "platform/mac/predict/predictor_client.h"
#include "core/predict/llm_predictor.h"
#include "core/reliability/circuit_breaker.h"
#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <filesystem>
#include <chrono>
#include <vector>

extern char** environ;

#pragma pack(push, 1)
struct ClientReqHeader {
    uint32_t magic = 0x494E4C50;
    uint8_t op = 1;
    uint8_t maxWords = 3;
    uint16_t reserved = 0;
    uint64_t generation = 0;
    uint32_t contextLen = 0;
    uint32_t partialLen = 0;
};

struct ClientRespHeader {
    uint32_t magic = 0x494E4C52;
    uint64_t generation = 0;
    int64_t latencyUs = 0;
    float confidence = 0.0f;
    uint32_t textLen = 0;
};
#pragma pack(pop)

static dispatch_source_t gMemPressureSource = nil;

PredictorClient::PredictorClient() {
    resolvePaths();

    // Register macOS memory pressure handler to automatically unload helper under WARN/CRITICAL pressure
    gMemPressureSource = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_MEMORYPRESSURE,
        0,
        DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL,
        dispatch_get_global_queue(QOS_CLASS_UTILITY, 0)
    );
    if (gMemPressureSource) {
        PredictorClient* self = this;
        dispatch_source_set_event_handler(gMemPressureSource, ^{
            self->shutdownHelper();
        });
        dispatch_resume(gMemPressureSource);
    }
}

PredictorClient::~PredictorClient() {
    if (gMemPressureSource) {
        dispatch_source_cancel(gMemPressureSource);
        gMemPressureSource = nil;
    }
    shutdownHelper();
}

void PredictorClient::resolvePaths() {
    std::filesystem::path binDir = std::filesystem::current_path();
    char execPath[PATH_MAX] = {0};
    uint32_t sz = sizeof(execPath);
    if (_NSGetExecutablePath(execPath, &sz) == 0) {
        std::error_code ec;
        auto canon = std::filesystem::canonical(execPath, ec);
        binDir = ec ? std::filesystem::path(execPath).parent_path() : canon.parent_path();
    }

    // Search candidate paths for InlinePredictor helper binary
    std::vector<std::filesystem::path> helperCands = {
        binDir.parent_path() / "XPCServices" / "InlinePredictor.xpc" / "Contents" / "MacOS" / "InlinePredictor",
        binDir / "InlinePredictor",
        binDir.parent_path() / "InlinePredictor",
        std::filesystem::current_path() / "build_release" / "InlinePredictor",
        std::filesystem::current_path() / "InlinePredictor"
    };
    for (const auto& p : helperCands) {
        if (std::filesystem::exists(p)) {
            helperPath_ = p.string();
            break;
        }
    }

    // Search candidate paths for bundled SmolLM2-135M GGUF model
    std::vector<std::filesystem::path> modelCands = {
        binDir.parent_path() / "Resources" / "models" / "smollm2-135m.gguf",
        binDir / "models" / "smollm2-135m.gguf",
        std::filesystem::current_path() / "third_party" / "models" / "smollm2-135m.gguf",
        binDir.parent_path() / " third_party" / "models" / "smollm2-135m.gguf",
        binDir.parent_path().parent_path() / "third_party" / "models" / "smollm2-135m.gguf"
    };
    for (const auto& p : modelCands) {
        std::error_code ec;
        if (std::filesystem::exists(p, ec) && std::filesystem::file_size(p, ec) > 1024 * 1024) {
            modelPath_ = p.string();
            modelAvailable_.store(true, std::memory_order_relaxed);
            break;
        }
    }
}

bool PredictorClient::ensureHelperStartedLocked() {
    if (circuitTripped_.load(std::memory_order_relaxed)) return false;

    int pid = childPid_.load(std::memory_order_relaxed);
    if (pid > 0 && toChildFd_ >= 0 && fromChildFd_ >= 0) {
        int status = 0;
        pid_t w = ::waitpid(pid, &status, WNOHANG);
        if (w == 0) {
            return true; // Still alive
        }
        closeChildLocked(true);
        if (circuitTripped_.load(std::memory_order_relaxed)) return false;
    }

    if (helperPath_.empty() || modelPath_.empty()) {
        resolvePaths();
    }
    if (helperPath_.empty() || modelPath_.empty() || !modelAvailable_.load(std::memory_order_relaxed)) {
        return false;
    }

    int inPipe[2] = {-1, -1};
    int outPipe[2] = {-1, -1};
    if (::pipe(inPipe) != 0) return false;
    if (::pipe(outPipe) != 0) {
        ::close(inPipe[0]);
        ::close(inPipe[1]);
        return false;
    }

    // Prevent SIGPIPE if helper exits unexpectedly
    ::fcntl(inPipe[1], F_SETNOSIGPIPE, 1);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, inPipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, inPipe[0]);
    posix_spawn_file_actions_addclose(&actions, inPipe[1]);
    posix_spawn_file_actions_addclose(&actions, outPipe[0]);
    posix_spawn_file_actions_addclose(&actions, outPipe[1]);

    char* argv[3];
    argv[0] = const_cast<char*>(helperPath_.c_str());
    argv[1] = const_cast<char*>(modelPath_.c_str());
    argv[2] = nullptr;

    pid_t spawnedPid = 0;
    int rc = ::posix_spawn(&spawnedPid, helperPath_.c_str(), &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);

    ::close(inPipe[0]);
    ::close(outPipe[1]);

    if (rc != 0 || spawnedPid <= 0) {
        ::close(inPipe[1]);
        ::close(outPipe[0]);
        return false;
    }

    toChildFd_ = inPipe[1];
    fromChildFd_ = outPipe[0];
    childPid_.store(static_cast<int>(spawnedPid), std::memory_order_relaxed);
    return true;
}

void PredictorClient::closeChildLocked(bool recordCrash) {
    if (toChildFd_ >= 0) {
        ::close(toChildFd_);
        toChildFd_ = -1;
    }
    if (fromChildFd_ >= 0) {
        ::close(fromChildFd_);
        fromChildFd_ = -1;
    }
    int pid = childPid_.exchange(0, std::memory_order_relaxed);
    if (pid > 0) {
        int status = 0;
        pid_t w = ::waitpid(pid, &status, WNOHANG);
        if (w == 0) {
            ::kill(pid, SIGTERM);
            ::waitpid(pid, &status, 0);
        }
    }

    if (recordCrash) {
        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        recentCrashTimestampsMs_.push_back(nowMs);
        while (recentCrashTimestampsMs_.size() > 8) {
            recentCrashTimestampsMs_.pop_front();
        }
        // Trip circuit breaker if 3 crashes occur within 5 minutes (300,000 ms)
        if (circuitBreakerShouldTrip(recentCrashTimestampsMs_, nowMs, 2, 300000)) {
            circuitTripped_.store(true, std::memory_order_relaxed);
        }
    }
}

void PredictorClient::shutdownHelper() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (toChildFd_ >= 0) {
        ClientReqHeader req;
        req.op = 3; // Shutdown
        (void)::write(toChildFd_, &req, sizeof(req));
    }
    closeChildLocked(false);
}

static bool writeAll(int fd, const void* buf, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::write(fd, p + sent, len - sent);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

static bool readAllWithTimeout(int fd, void* buf, size_t len, int timeoutMs) {
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t got = 0;
    while (got < len) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pr = ::poll(&pfd, 1, timeoutMs);
        if (pr <= 0) return false;
        ssize_t n = ::read(fd, p + got, len - got);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

PredictionResult PredictorClient::predict(
    uint64_t generation,
    const std::wstring& contextBeforePartial,
    const std::wstring& partialWord,
    int maxWords
) {
    PredictionResult res;
    res.fromLLM = true;

    std::lock_guard<std::mutex> lock(mutex_);
    bool wasRunning = (childPid_.load(std::memory_order_relaxed) > 0);
    if (!ensureHelperStartedLocked()) {
        return res;
    }

    std::string ctxUTF8 = LlmPredictor::wstringToUtf8(contextBeforePartial);
    std::string partUTF8 = LlmPredictor::wstringToUtf8(partialWord);

    ClientReqHeader req;
    req.op = 1;
    req.maxWords = static_cast<uint8_t>(std::clamp(maxWords, 1, 6));
    req.generation = generation;
    req.contextLen = static_cast<uint32_t>(ctxUTF8.size());
    req.partialLen = static_cast<uint32_t>(partUTF8.size());

    if (!writeAll(toChildFd_, &req, sizeof(req)) ||
        (!ctxUTF8.empty() && !writeAll(toChildFd_, ctxUTF8.data(), ctxUTF8.size())) ||
        (!partUTF8.empty() && !writeAll(toChildFd_, partUTF8.data(), partUTF8.size()))) {
        closeChildLocked(true);
        return res;
    }

    // Allow longer timeout on first cold-start request (600 ms), 160 ms once warm
    int timeoutMs = wasRunning ? 160 : 600;
    ClientRespHeader resp;
    if (!readAllWithTimeout(fromChildFd_, &resp, sizeof(resp), timeoutMs) ||
        resp.magic != 0x494E4C52 || resp.textLen > 1024) {
        closeChildLocked(true);
        return res;
    }

    std::string outUTF8(resp.textLen, '\0');
    if (resp.textLen > 0 && !readAllWithTimeout(fromChildFd_, outUTF8.data(), resp.textLen, 100)) {
        closeChildLocked(true);
        return res;
    }

    res.text = LlmPredictor::utf8ToWstring(outUTF8);
    res.confidence = resp.confidence;
    res.latencyUs = resp.latencyUs;
    res.wordCount = 1;
    for (wchar_t c : res.text) if (c == L' ') res.wordCount++;
    if (resp.latencyUs > 0) {
        lastLatencyMs_.store(std::max<int64_t>(1, resp.latencyUs / 1000), std::memory_order_relaxed);
    }
    return res;
}
