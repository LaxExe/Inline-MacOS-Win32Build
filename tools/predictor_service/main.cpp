#include "core/predict/llm_predictor.h"
#include <unistd.h>
#include <poll.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

#ifdef __APPLE__
#include <pthread/qos.h>
#endif

#pragma pack(push, 1)
struct PredictorRequestHeader {
    uint32_t magic = 0x494E4C50; // 'INLP'
    uint8_t op = 1;              // 1 = Predict, 2 = Warmup, 3 = Shutdown
    uint8_t maxWords = 3;
    uint16_t reserved = 0;
    uint64_t generation = 0;
    uint32_t contextLen = 0;
    uint32_t partialLen = 0;
};

struct PredictorResponseHeader {
    uint32_t magic = 0x494E4C52; // 'INLR'
    uint64_t generation = 0;
    int64_t latencyUs = 0;
    float confidence = 0.0f;
    uint32_t textLen = 0;
};
#pragma pack(pop)

static bool readExact(int fd, void* buf, size_t len) {
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::read(fd, p + got, len - got);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

static bool writeExact(int fd, const void* buf, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::write(fd, p + sent, len - sent);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

int main(int argc, char* argv[]) {
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
#endif

    if (argc < 2) {
        return 1;
    }
    std::string modelPath = argv[1];
    if (!std::filesystem::exists(modelPath)) {
        return 2;
    }

    LlmPredictor predictor;
    if (!predictor.loadModel(modelPath, 2)) {
        return 3;
    }

    // Warmup pass so first user pause has zero graph-init overhead
    predictor.predict("Hello, I ", "wa", 2, 0.25f);

    // 10-minute idle timeout (600,000 ms) so helper exits and returns RAM when user isn't typing
    constexpr int kIdleTimeoutMs = 10 * 60 * 1000;

    while (true) {
        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int pr = ::poll(&pfd, 1, kIdleTimeoutMs);
        if (pr <= 0) {
            // Timeout (10 min idle) or poll error -> exit cleanly
            break;
        }
        if ((pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) && !(pfd.revents & POLLIN)) {
            break;
        }

        PredictorRequestHeader req;
        if (!readExact(STDIN_FILENO, &req, sizeof(req))) {
            break;
        }
        if (req.magic != 0x494E4C50 || req.contextLen > 4096 || req.partialLen > 256) {
            break;
        }

        std::string context(req.contextLen, '\0');
        std::string partial(req.partialLen, '\0');
        if (req.contextLen > 0 && !readExact(STDIN_FILENO, context.data(), req.contextLen)) break;
        if (req.partialLen > 0 && !readExact(STDIN_FILENO, partial.data(), req.partialLen)) break;

        if (req.op == 3) { // Shutdown
            break;
        }

        PredictorResponseHeader resp;
        resp.generation = req.generation;
        std::string outUTF8;

        if (req.op == 1) {
            PredictionResult r = predictor.predict(context, partial, std::max<int>(1, req.maxWords), 0.26f);
            resp.latencyUs = r.latencyUs;
            resp.confidence = static_cast<float>(r.confidence);
            outUTF8 = LlmPredictor::wstringToUtf8(r.text);
        }

        resp.textLen = static_cast<uint32_t>(outUTF8.size());
        if (!writeExact(STDOUT_FILENO, &resp, sizeof(resp))) break;
        if (!outUTF8.empty() && !writeExact(STDOUT_FILENO, outUTF8.data(), outUTF8.size())) break;
    }

    predictor.unload();
    return 0;
}
