# Gridrunner rules and module-selected bindings over the shared engine.
set(NEON_RENDERER ${PROJECT_SOURCE_DIR}/engine/bindings/common)
add_subdirectory(${PROJECT_SOURCE_DIR}/reconstruction/gridrunner ${CMAKE_CURRENT_BINARY_DIR}/native_reconstruction)
add_library(mino_core_gridrunner STATIC
    gridrunner_core.cpp native_view.cpp controls.cpp original_stm.cpp original_frontend.cpp menu_focus.cpp original_ui.cpp title_preview.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/late/neon_sprite.cpp ${PROJECT_SOURCE_DIR}/engine/bindings/common/neon_synth.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_create.cpp ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_showjoy.cpp ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_mixer.cpp ${PROJECT_SOURCE_DIR}/engine/bindings/late/mod_cellgrid.cpp
    ${NEON_RENDERER}/neon_base.cpp
)
target_compile_definitions(mino_core_gridrunner PRIVATE NEON_GAME=gridrunner)
target_include_directories(mino_core_gridrunner PUBLIC ${PROJECT_SOURCE_DIR}/engine/bindings/late ${NEON_RENDERER})
target_include_directories(mino_core_gridrunner PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(mino_core_gridrunner PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
target_link_libraries(mino_core_gridrunner PUBLIC neon_renderer mino_glad mino_pugixml mino_shared mino_gridrunner_rules)
if(NOT MSVC)
    target_compile_options(mino_core_gridrunner PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Optional checks; normal builds do not build or run tests.
add_executable(mino_gridrunner_classic_checks EXCLUDE_FROM_ALL
    ${PROJECT_SOURCE_DIR}/reconstruction/gridrunner/native_classic_test.cpp)
target_link_libraries(mino_gridrunner_classic_checks PRIVATE mino_gridrunner_rules)
target_include_directories(mino_gridrunner_classic_checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(mino_display_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tests/display_layout.cpp
    ${PROJECT_SOURCE_DIR}/platform/settings.cpp)
target_include_directories(mino_display_checks PRIVATE ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/platform ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(mino_display_checks PRIVATE ${PROJECT_SOURCE_DIR}/engine/include)
target_link_libraries(mino_display_checks PRIVATE SDL3::SDL3)

add_executable(mino_gridrunner_frontend_checks EXCLUDE_FROM_ALL
    ${PROJECT_SOURCE_DIR}/tests/gridrunner_frontend.cpp original_frontend.cpp menu_focus.cpp)
target_compile_definitions(mino_gridrunner_frontend_checks PRIVATE NEON_GAME=gridrunner)
target_link_libraries(mino_gridrunner_frontend_checks PRIVATE neon_engine)
target_include_directories(mino_gridrunner_frontend_checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/engine/bindings/late ${NEON_RENDERER} ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/platform ${PROJECT_SOURCE_DIR}/third_party/glad/include)

add_executable(mino_gridrunner_profile_checks EXCLUDE_FROM_ALL
    ${PROJECT_SOURCE_DIR}/tests/gridrunner_profile.cpp controls.cpp)
target_include_directories(mino_gridrunner_profile_checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/shared ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/engine/include)
target_link_libraries(mino_gridrunner_profile_checks PRIVATE mino_shared neon_engine mino_gridrunner_rules)

# Match source floating-point operation boundaries in shared template consumers.
target_link_libraries(mino_core_gridrunner PRIVATE neon_source_numeric)

if(NOT MINO_STANDALONE_GAME)
    add_executable(mino_gridrunner_ux_checks EXCLUDE_FROM_ALL
        ${PROJECT_SOURCE_DIR}/tests/gridrunner_ux.cpp
        ${PROJECT_SOURCE_DIR}/platform/display.cpp)
    target_compile_definitions(mino_gridrunner_ux_checks PRIVATE NEON_GAME=gridrunner)
    target_include_directories(mino_gridrunner_ux_checks PRIVATE
        ${PROJECT_SOURCE_DIR}/platform ${PROJECT_SOURCE_DIR}/third_party/stb)
    target_link_libraries(mino_gridrunner_ux_checks PRIVATE mino_core_gridrunner mino_crt SDL3::SDL3)
    set_target_properties(mino_gridrunner_ux_checks PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
endif()
