# platform/mac/CMakeLists.txt

find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
find_library(CARBON_FRAMEWORK Carbon REQUIRED)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
find_library(CORETEXT_FRAMEWORK CoreText REQUIRED)
find_library(APPLICATIONSERVICES_FRAMEWORK ApplicationServices REQUIRED)
find_library(SERVICEMANAGEMENT_FRAMEWORK ServiceManagement REQUIRED)
find_library(IOKIT_FRAMEWORK IOKit REQUIRED)

add_library(mac_platform_lib STATIC
    permissions.mm
    hook/synthetic_filter.cpp
    hook/tap_recovery.cpp
    hook/mac_keyboard_hook.mm
    caret/secure_field_classifier.cpp
    caret/mac_caret_tracker.mm
    ghost/ghost_overlay.mm
    power/power_monitor.mm
    predict/predictor_client.mm
    inject/mac_injector.mm
    login/login_item.mm
    hotkey/global_hotkey.mm
)

target_compile_options(mac_platform_lib PRIVATE
    $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
    $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>
)

target_include_directories(mac_platform_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(mac_platform_lib PUBLIC
    core_lib
    ${COCOA_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${CARBON_FRAMEWORK}
    ${COREGRAPHICS_FRAMEWORK}
    ${CORETEXT_FRAMEWORK}
    ${APPLICATIONSERVICES_FRAMEWORK}
    ${SERVICEMANAGEMENT_FRAMEWORK}
    ${IOKIT_FRAMEWORK}
)
