# platform/win/CMakeLists.txt

add_library(win_platform_lib STATIC
    permissions.cpp
    hook/synthetic_filter.cpp
    hook/tap_recovery.cpp
    hook/win_keyboard_hook.cpp
    caret/secure_field_classifier.cpp
    inject/win_injector.cpp
    login/login_item.cpp
    hotkey/global_hotkey.cpp
)

target_include_directories(win_platform_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(win_platform_lib PUBLIC
    core_lib
)

if(WIN32)
    target_link_libraries(win_platform_lib PUBLIC
        user32
        shell32
        advapi32
    )
endif()
