# Deflex game rules, renderer binding and optional checks.
add_subdirectory(${PROJECT_SOURCE_DIR}/reconstruction/deflex ${CMAKE_CURRENT_BINARY_DIR}/reconstruction)
add_executable(mino_deflex_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/deflex/level_data_test.cpp)
target_link_libraries(mino_deflex_checks PRIVATE mino_deflex_rules)
add_executable(mino_deflex_cell_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/reconstruction/deflex/cell_grid_addressing_test.cpp)
target_link_libraries(mino_deflex_cell_checks PRIVATE mino_deflex_rules)
set(DF_BINDING ${PROJECT_SOURCE_DIR}/engine/bindings/late)
set(DF_RESOURCES ${PROJECT_SOURCE_DIR}/engine/bindings/common)
add_library(mino_core_deflex STATIC EXCLUDE_FROM_ALL backend.cpp deflex_core.cpp
    ${DF_BINDING}/neon_sprite.cpp ${PROJECT_SOURCE_DIR}/engine/bindings/common/neon_synth.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_showjoy.cpp ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_mixer.cpp)
target_compile_definitions(mino_core_deflex PRIVATE NEON_GAME=deflex)
target_include_directories(mino_core_deflex PRIVATE ${DF_BINDING} ${DF_RESOURCES} ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(mino_core_deflex PUBLIC mino_deflex_rules neon_renderer)
