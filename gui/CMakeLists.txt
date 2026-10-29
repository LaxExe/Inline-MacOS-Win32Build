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

if(APPLE AND NOT BUILD_WINDOWS)
    set(PLATFORM_LIB mac_platform_lib)
else()
    set(PLATFORM_LIB win_platform_lib)
endif()

target_link_libraries(gui_lib PUBLIC
    core_lib
    ${PLATFORM_LIB}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(APPLE AND NOT BUILD_WINDOWS)
    # App Bundle executable
    add_executable(Inline MACOSX_BUNDLE
        main.cpp
    )

    set_target_properties(Inline PROPERTIES
        OUTPUT_NAME "Inline"
        MACOSX_BUNDLE_BUNDLE_NAME "Inline"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../Info.plist
    )
elseif(WIN32)
    add_executable(Inline WIN32
        main.cpp
    )
    set_target_properties(Inline PROPERTIES OUTPUT_NAME "Inline")
else()
    add_executable(Inline
        main.cpp
    )
    set_target_properties(Inline PROPERTIES OUTPUT_NAME "Inline")
endif()

target_link_libraries(Inline PRIVATE
    gui_lib
)

if(APPLE AND NOT BUILD_WINDOWS)
    add_custom_command(TARGET Inline POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Inline>/../Resources/dictionaries"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../dictionaries" "$<TARGET_FILE_DIR:Inline>/../Resources/dictionaries"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../resources/AppIcon.icns" "$<TARGET_FILE_DIR:Inline>/../Resources/AppIcon.icns"
        COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_FILE_DIR:Inline>/../.."
    )
else()
    add_custom_command(TARGET Inline POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Inline>/dictionaries"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../dictionaries" "$<TARGET_FILE_DIR:Inline>/dictionaries"
    )
endif()

