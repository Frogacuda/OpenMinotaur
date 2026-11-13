add_library(mino_neon_background STATIC
    runtime/effect_runtime.cpp
    runtime/overlay_text.cpp
    runtime/component_scene.cpp
    runtime/shape_component.cpp
    runtime/primitive_mesh.cpp
    runtime/layer_asset.cpp
    authoring/authoring_model.cpp
    effects/title_flight.cpp
    authoring/geometry_model.cpp
    effects/pc_preset.cpp
    effects/pc_animation.cpp
    effects/response_animation.cpp
    effects/read_bindings.cpp
    effects/pc_tunnel.cpp
    effects/pc_render_state.cpp
    effects/pc_cube.cpp
    effects/pc_geometry.cpp
    effects/pc_texture.cpp
    effects/pc_particles.cpp
    effects/pc_flower.cpp
    effects/pc_shapes.cpp
    effects/recovered_scene.cpp
    authoring/vector_demo.cpp
    authoring/preset_catalog.cpp
)
set_target_properties(mino_neon_background PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(mino_neon_background PUBLIC runtime effects authoring PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
target_link_libraries(mino_neon_background PUBLIC mino_glad mino_widget_vectors)

if(BUILD_TESTING)
    add_executable(mino_neon_geometry_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tests/neon_geometry.cpp)
    target_link_libraries(mino_neon_geometry_checks PRIVATE mino_neon_background)
endif()
