#!/usr/bin/env bash
# ==============================================================================
# Fetch bundled on-device language model and llama.cpp runtime for Inline
# ==============================================================================
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODELS_DIR="$ROOT_DIR/third_party/models"
LLAMA_DIR="$ROOT_DIR/third_party/llama.cpp"

mkdir -p "$MODELS_DIR"

# 1. Fetch llama.cpp (shallow clone if not already present)
if [ ! -f "$LLAMA_DIR/CMakeLists.txt" ]; then
    echo "[fetch_model] Cloning llama.cpp into third_party/llama.cpp..."
    rm -rf "$LLAMA_DIR"
    git clone --depth 1 https://github.com/ggml-org/llama.cpp.git "$LLAMA_DIR"
else
    echo "[fetch_model] llama.cpp already present."
fi

# 2. Fetch SmolLM2-135M GGUF (primary bundled model)
MODEL_135M="$MODELS_DIR/smollm2-135m.gguf"
if [ ! -s "$MODEL_135M" ]; then
    echo "[fetch_model] Downloading SmolLM2-135M GGUF..."
    if ! curl -fL --retry 3 -o "$MODEL_135M" \
        "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/resolve/main/SmolLM2-135M-Instruct-Q8_0.gguf"; then
        curl -fL --retry 3 -o "$MODEL_135M" \
            "https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct-GGUF/resolve/main/smollm2-135m-instruct-q8_0.gguf"
    fi
else
    echo "[fetch_model] SmolLM2-135M GGUF already cached."
fi

# 3. Optional: Fetch SmolLM2-360M GGUF when --benchmark is passed
if [ "${1:-}" = "--benchmark" ]; then
    MODEL_360M="$MODELS_DIR/smollm2-360m.gguf"
    if [ ! -s "$MODEL_360M" ]; then
        echo "[fetch_model] Downloading SmolLM2-360M GGUF for benchmark..."
        if ! curl -fL --retry 3 -o "$MODEL_360M" \
            "https://huggingface.co/bartowski/SmolLM2-360M-Instruct-GGUF/resolve/main/SmolLM2-360M-Instruct-Q4_K_M.gguf"; then
            curl -fL --retry 3 -o "$MODEL_360M" \
                "https://huggingface.co/HuggingFaceTB/SmolLM2-360M-Instruct-GGUF/resolve/main/smollm2-360m-instruct-q8_0.gguf"
        fi
    else
        echo "[fetch_model] SmolLM2-360M GGUF already cached."
    fi
fi

# 4. Write model SHA-256 manifest and license files
ACTIVE_MODEL="$MODELS_DIR/active_model.gguf"
if [ ! -f "$ACTIVE_MODEL" ]; then
    cp "$MODEL_135M" "$ACTIVE_MODEL"
fi
shasum -a 256 "$ACTIVE_MODEL" | awk '{print $1}' > "$MODELS_DIR/model.sha256"
echo "[fetch_model] Ready. Active model SHA-256: $(cat "$MODELS_DIR/model.sha256")"
