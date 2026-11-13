# Caverns of Minos game module (core id "minos").
add_subdirectory(${PROJECT_SOURCE_DIR}/reconstruction/minos ${CMAKE_CURRENT_BINARY_DIR}/reconstruction)
# Optional checks (normal builds do not build or run them):
#   cmake --build <dir> --target mino_minos_checks && <dir>/cores/minos/mino_minos_checks ...
add_executable(mino_minos_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/minos/cavern_map_test.cpp)
target_link_libraries(mino_minos_checks PRIVATE mino_minos_rules)
add_executable(mino_minos_draw_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/minos/cavern_draw_test.cpp)
target_link_libraries(mino_minos_draw_checks PRIVATE mino_minos_rules)
# Port integration check of the map draw emission over the engine sprite path (SPR_EXPLICIT_UV).
add_executable(mino_minos_emit_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/minos/cavern_draw_emit_test.cpp)
target_link_libraries(mino_minos_emit_checks PRIVATE mino_minos_rules neon_engine)
add_executable(mino_minos_launch_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/minos/cavern_launch_test.cpp)
target_link_libraries(mino_minos_launch_checks PRIVATE mino_minos_rules)
add_executable(mino_minos_entity_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/minos/entity_test.cpp)
target_link_libraries(mino_minos_entity_checks PRIVATE mino_minos_rules)
add_executable(mino_minos_ship_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/minos/ship_test.cpp)
target_link_libraries(mino_minos_ship_checks PRIVATE mino_minos_rules)
add_executable(mino_minos_play_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/minos/play_test.cpp)
target_link_libraries(mino_minos_play_checks PRIVATE mino_minos_rules)
# Host binding for ship classes, gameplay, menus, input and local records.
file(GLOB MINOS_ENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp)
list(APPEND MINOS_ENGINE_SOURCES
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_mixer.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_create.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/neon_synth.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/neon_base.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_showjoy.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/early/neon_sprite.cpp)
add_library(mino_core_minos STATIC EXCLUDE_FROM_ALL ${MINOS_ENGINE_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/minos_core.cpp)
target_compile_definitions(mino_core_minos PRIVATE NEON_GAME=minos)
target_include_directories(mino_core_minos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/engine ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mino_core_minos PUBLIC neon_renderer mino_glad mino_pugixml mino_shared mino_minos_rules)
target_include_directories(mino_core_minos PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/third_party/stb)
target_link_libraries(mino_core_minos PRIVATE neon_source_numeric mino_baked_assets)
if(NOT MSVC)
    target_compile_options(mino_core_minos PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
