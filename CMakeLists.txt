cmake_minimum_required(VERSION 3.20)

project(TypingAssistant LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# GoogleTest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/googletest)

# Bundled llama.cpp (static, CPU-only with Apple Accelerate so GPU stays 100% idle)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/CMakeLists.txt")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(GGML_METAL OFF CACHE BOOL "" FORCE)
    set(GGML_ACCELERATE ON CACHE BOOL "" FORCE)
    set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/llama.cpp EXCLUDE_FROM_ALL)
    set(INLINE_HAS_LLAMA TRUE)
endif()

enable_testing()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/dictionaries DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(core)
add_subdirectory(platform/mac)

add_executable(InlinePredictor tools/predictor_service/main.cpp)
target_link_libraries(InlinePredictor PRIVATE core_lib)

add_subdirectory(gui)
add_subdirectory(tests)

add_executable(bench_corrector tools/bench_corrector.cpp)
target_link_libraries(bench_corrector PRIVATE core_lib)

add_executable(bench_predictor tools/bench_predictor.cpp)
target_link_libraries(bench_predictor PRIVATE core_lib)
