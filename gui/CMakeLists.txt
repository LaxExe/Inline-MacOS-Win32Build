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
add_executable(TypingAssistant MACOSX_BUNDLE
    main.cpp
)

set_target_properties(TypingAssistant PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../Info.plist
)

target_link_libraries(TypingAssistant PRIVATE
    gui_lib
)

add_custom_command(TARGET TypingAssistant POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TypingAssistant>/../Resources/dictionaries"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../dictionaries" "$<TARGET_FILE_DIR:TypingAssistant>/../Resources/dictionaries"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../resources/AppIcon.icns" "$<TARGET_FILE_DIR:TypingAssistant>/../Resources/AppIcon.icns"
    COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_FILE_DIR:TypingAssistant>/../.."
)


