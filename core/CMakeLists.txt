# core/CMakeLists.txt

add_library(core_lib STATIC
    buffers/buffer_gate.cpp
    buffers/word_boundary.cpp
    config/config_json.cpp
    injection/token_validation.cpp
    injection/priority_queue.cpp
    tier1/symspell_dict.cpp
    tier1/corrector.cpp
    tier2/diff_safety.cpp
    tier2/supervisor_policy.cpp
    ghosttext/dismissal.cpp
    ghosttext/predictor.cpp
    undo/undo_state_machine.cpp
    reliability/circuit_breaker.cpp
    reliability/engine_mode.cpp
    reliability/privacy_logger.cpp
)

target_include_directories(core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
