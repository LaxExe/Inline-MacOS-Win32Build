# core/CMakeLists.txt

add_library(core_lib STATIC
    buffers/buffer_gate.cpp
    buffers/word_boundary.cpp
    config/config_json.cpp
    injection/token_validation.cpp
    injection/priority_queue.cpp
    tier1/symspell_dict.cpp
    tier1/corrector.cpp
    undo/undo_state_machine.cpp
    reliability/circuit_breaker.cpp
    reliability/engine_mode.cpp
    reliability/privacy_logger.cpp
    ghost/ghost_layout.cpp
    ghost/ghost_session.cpp
    predict/token_healing.cpp
    predict/suggestion_filter.cpp
    predict/ngram_predictor.cpp
    predict/llm_predictor.cpp
)

target_include_directories(core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(INLINE_HAS_LLAMA)
    target_compile_definitions(core_lib PUBLIC INLINE_ENABLE_LLAMA=1)
    target_link_libraries(core_lib PUBLIC llama)
endif()
