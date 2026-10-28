# tests/CMakeLists.txt

if(APPLE AND NOT BUILD_WINDOWS)
    set(PLATFORM_LIB mac_platform_lib)
else()
    set(PLATFORM_LIB win_platform_lib)
endif()

add_executable(test_smoke unit/test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE GTest::gtest_main)
add_test(NAME Smoke.HarnessRuns COMMAND test_smoke)

# Stage 1
add_executable(test_tier1 unit/test_tier1.cpp)
target_link_libraries(test_tier1 PRIVATE core_lib GTest::gtest_main)
add_test(NAME Tier1Test COMMAND test_tier1)

# Stage 2
add_executable(test_permission_flow unit/test_permission_flow.cpp)
target_link_libraries(test_permission_flow PRIVATE ${PLATFORM_LIB} GTest::gtest_main)
add_test(NAME PermissionFlow COMMAND test_permission_flow)

# Stage 3
add_executable(test_hook_filter unit/test_hook_filter.cpp)
target_link_libraries(test_hook_filter PRIVATE ${PLATFORM_LIB} GTest::gtest_main)
add_test(NAME HookFilter COMMAND test_hook_filter)

# Stage 4
add_executable(test_secure_field_classifier unit/test_secure_field_classifier.cpp)
target_link_libraries(test_secure_field_classifier PRIVATE ${PLATFORM_LIB} GTest::gtest_main)
add_test(NAME SecureFieldClassifier COMMAND test_secure_field_classifier)

add_executable(test_buffer_suppression unit/test_buffer_suppression.cpp)
target_link_libraries(test_buffer_suppression PRIVATE core_lib GTest::gtest_main)
add_test(NAME BufferGate COMMAND test_buffer_suppression)

# Stage 5
add_executable(test_word_boundary unit/test_word_boundary.cpp)
target_link_libraries(test_word_boundary PRIVATE core_lib GTest::gtest_main)
add_test(NAME WordBoundary COMMAND test_word_boundary)

# Stage 6
add_executable(test_injection_token unit/test_injection_token.cpp)
target_link_libraries(test_injection_token PRIVATE core_lib GTest::gtest_main)
add_test(NAME TokenValidation COMMAND test_injection_token)

add_executable(test_injection_priority unit/test_injection_priority.cpp)
target_link_libraries(test_injection_priority PRIVATE core_lib GTest::gtest_main)
add_test(NAME PriorityQueue COMMAND test_injection_priority)

# Stage 7
add_executable(test_undo_state_machine unit/test_undo_state_machine.cpp)
target_link_libraries(test_undo_state_machine PRIVATE core_lib GTest::gtest_main)
add_test(NAME UndoStateMachine COMMAND test_undo_state_machine)

# Stage 8
add_executable(test_config_persistence unit/test_config_persistence.cpp)
target_link_libraries(test_config_persistence PRIVATE core_lib GTest::gtest_main)
add_test(NAME ConfigPersistence COMMAND test_config_persistence)

# Stage 9
add_executable(test_tray_state unit/test_tray_state.cpp)
target_link_libraries(test_tray_state PRIVATE gui_lib GTest::gtest_main)
add_test(NAME TrayState COMMAND test_tray_state)

# Stage 10
add_executable(test_settings_binding unit/test_settings_binding.cpp)
target_link_libraries(test_settings_binding PRIVATE gui_lib GTest::gtest_main)
add_test(NAME SettingsBinding COMMAND test_settings_binding)

# Stage 11
add_executable(test_hotkey_blocklist unit/test_hotkey_blocklist.cpp)
target_link_libraries(test_hotkey_blocklist PRIVATE gui_lib GTest::gtest_main)
add_test(NAME HotkeyBlocklist COMMAND test_hotkey_blocklist)

# Stage 12
add_executable(test_circuit_breaker unit/test_circuit_breaker.cpp)
target_link_libraries(test_circuit_breaker PRIVATE core_lib GTest::gtest_main)
add_test(NAME CircuitBreaker COMMAND test_circuit_breaker)

add_executable(test_engine_mode unit/test_engine_mode.cpp)
target_link_libraries(test_engine_mode PRIVATE core_lib GTest::gtest_main)
add_test(NAME EngineMode COMMAND test_engine_mode)

# Integration tests
if(APPLE AND NOT BUILD_WINDOWS)
    add_executable(hook_synth_test integration/mac/hook_synth_test.mm)
    target_link_libraries(hook_synth_test PRIVATE mac_platform_lib GTest::gtest_main)
    add_test(NAME HookIntegration COMMAND hook_synth_test)

    add_executable(injector_synth_test integration/mac/injector_synth_test.mm)
    target_link_libraries(injector_synth_test PRIVATE mac_platform_lib GTest::gtest_main)
    add_test(NAME InjectorIntegration COMMAND injector_synth_test)
endif()

