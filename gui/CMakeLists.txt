# gui/CMakeLists.txt

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(gui_lib STATIC
    tray_state.cpp
    settings_binding.cpp
    hotkey_blocklist.cpp
    settings_dialog.cpp
    permission_dialog.cpp
    main_tray_app.cpp
)

target_include_directories(gui_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(gui_lib PUBLIC
    core_lib
    mac_platform_lib
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# App Bundle executable
add_executable(Inline MACOSX_BUNDLE
    main.cpp
)

add_dependencies(Inline InlinePredictor)

set_target_properties(Inline PROPERTIES
    OUTPUT_NAME "Inline"
    MACOSX_BUNDLE_BUNDLE_NAME "Inline"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../Info.plist
)

target_link_libraries(Inline PRIVATE
    gui_lib
)

add_custom_command(TARGET Inline POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Inline>/../Resources/dictionaries"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Inline>/../Resources/models"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Inline>/../XPCServices/InlinePredictor.xpc/Contents/MacOS"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../dictionaries" "$<TARGET_FILE_DIR:Inline>/../Resources/dictionaries"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../resources/AppIcon.icns" "$<TARGET_FILE_DIR:Inline>/../Resources/AppIcon.icns"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:InlinePredictor>" "$<TARGET_FILE_DIR:Inline>/InlinePredictor"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:InlinePredictor>" "$<TARGET_FILE_DIR:Inline>/../XPCServices/InlinePredictor.xpc/Contents/MacOS/InlinePredictor"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/models/smollm2-135m.gguf" "$<TARGET_FILE_DIR:Inline>/../Resources/models/smollm2-135m.gguf"
    COMMAND /usr/bin/codesign --force --sign - "$<TARGET_FILE_DIR:Inline>/InlinePredictor"
    COMMAND /usr/bin/codesign --force --deep --sign - -i com.inline.mac "$<TARGET_FILE_DIR:Inline>/../.."
)
