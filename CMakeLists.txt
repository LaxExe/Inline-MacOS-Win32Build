cmake_minimum_required(VERSION 3.20)

option(BUILD_WINDOWS "Force building Windows platform code" OFF)

if(APPLE AND NOT BUILD_WINDOWS)
    project(TypingAssistant LANGUAGES C CXX OBJC OBJCXX)
else()
    project(TypingAssistant LANGUAGES C CXX)
    add_compile_definitions(BUILD_WINDOWS_PLATFORM)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE AND NOT BUILD_WINDOWS)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# GoogleTest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/googletest)

enable_testing()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/dictionaries DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(core)

if(APPLE AND NOT BUILD_WINDOWS)
    add_subdirectory(platform/mac)
else()
    add_subdirectory(platform/win)
endif()

add_subdirectory(gui)
add_subdirectory(tests)

add_executable(bench_corrector tools/bench_corrector.cpp)
target_link_libraries(bench_corrector PRIVATE core_lib)

