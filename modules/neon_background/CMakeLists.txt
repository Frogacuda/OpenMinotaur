if(NOT TARGET mino_widget_vectors)
    add_library(mino_widget_vectors STATIC ../live_widgets/vector/vector_scene.cpp ../live_widgets/vector/vector_font.cpp)
    target_include_directories(mino_widget_vectors PUBLIC ../live_widgets)
    set_target_properties(mino_widget_vectors PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
add_library(mino_neon_background STATIC
    runtime/live_rig.cpp
    runtime/effect_runtime.cpp
    runtime/overlay_text.cpp
    runtime/component_scene.cpp
    runtime/shape_component.cpp
    runtime/primitive_mesh.cpp
    runtime/layer_asset.cpp
    authoring/image_assets.cpp
    authoring/graph_recipe.cpp
    authoring/preset_cycles.cpp
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
    authoring/preset_catalog.cpp
    authoring/vector_demo.cpp
)
set_target_properties(mino_neon_background PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(mino_neon_background PUBLIC runtime effects authoring PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
target_link_libraries(mino_neon_background PUBLIC mino_glad mino_widget_vectors)

if(BUILD_TESTING)
    add_executable(mino_live_rig_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tests/live_rig.cpp)
    target_link_libraries(mino_live_rig_checks PRIVATE mino_neon_background)
    add_test(NAME check.mino_live_rig_checks COMMAND mino_live_rig_checks ${PROJECT_SOURCE_DIR}/assets/presentation)
    set_tests_properties(check.mino_live_rig_checks PROPERTIES LABELS "collection" TIMEOUT 60)

    add_executable(mino_neon_geometry_checks EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tests/neon_geometry.cpp)
    target_link_libraries(mino_neon_geometry_checks PRIVATE mino_neon_background)
endif()
