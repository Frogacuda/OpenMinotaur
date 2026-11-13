# CPU checks form the default suite. Graphics checks require a desktop GL driver.
set(MINO_CHECK_TARGETS "")
function(mino_check target)
    if(NOT TARGET ${target})
        return()
    endif()
    add_test(NAME check.${target} COMMAND ${target} ${ARGN})
    set_tests_properties(check.${target} PROPERTIES LABELS "collection" TIMEOUT 180)
    set_property(GLOBAL APPEND PROPERTY MINO_CHECK_TARGETS ${target})
endfunction()
# Build check executables declared by rule libraries too, including fixture-driven
# checks. Registration below is explicit; it never guesses arguments from source.
function(mino_collect_check_targets directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(kind ${target} TYPE)
        get_target_property(excluded ${target} EXCLUDE_FROM_ALL)
        if(kind STREQUAL "EXECUTABLE" AND (NOT excluded OR target MATCHES "checks$"))
            set_property(GLOBAL APPEND PROPERTY MINO_CHECK_TARGETS ${target})
        endif()
    endforeach()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        if(NOT child MATCHES "/(third_party|SDL3)(/|$)")
            mino_collect_check_targets("${child}")
        endif()
    endforeach()
endfunction()
mino_collect_check_targets("${PROJECT_SOURCE_DIR}")
# This fixture replay is excluded from ordinary builds but registered by CTest.
if(TARGET mino_goatup2_runtime)
    set_property(GLOBAL APPEND PROPERTY MINO_CHECK_TARGETS mino_goatup2_runtime)
endif()
foreach(target IN ITEMS
    mino_runtime_mechanics_checks
    mino_native_input_checks
    mino_foundation_checks
    mino_sdl_input_checks
    mino_minotron_presentation_checks
    mino_gridrunner_presentation_checks
    mino_minorescue_presentation_checks
    mino_minotron_grid_storage_checks
    mino_minorescue_grid_storage_checks
    mino_minos_grid_storage_checks
    mino_gridrunner_grid_storage_checks
    mino_goatup_grid_storage_checks
    mino_goatup2_grid_storage_checks
    mino_fiveaday_grid_storage_checks
    mino_superoxwars_grid_storage_checks
    mino_deflex_cell_effects_checks
    mino_minotron_particle_checks
    mino_minorescue_particle_checks
    mino_native_particle_checks
    mino_goatup_command_checks
    mino_fiveaday_command_checks
    mino_superoxwars_command_checks
    mino_particle_progress_checks
    mino_minos_text_checks
    mino_fiveaday_text_checks
    mino_superoxwars_text_checks
    mino_native_text_notice_checks
    mino_backend_setup_checks
    mino_motion_direction_checks
    mino_minos_motion_checks
    mino_tilt_calibration_checks
    mino_goatup_checks
    mino_goatup_touch_checks
    mino_fiveaday_checks
    mino_superoxwars_checks
    mino_superoxwars_presentation_checks
    mino_minotron_host_checks
    mino_display_checks
    mino_gridrunner_frontend_checks
    mino_gridrunner_profile_checks
    mino_goatup2_foundation_checks
    mino_goatup2_falling_checks
    mino_goatup2_horizontal_checks
    mino_goatup2_vertical_checks
    mino_minos_emit_checks
    mino_neon_geometry_checks
)
    mino_check(${target})
endforeach()
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/check-output")
mino_check(mino_achievement_checks "${PROJECT_BINARY_DIR}/check-output/achievements")
mino_check(mino_options_checks "${PROJECT_BINARY_DIR}/check-output/options.ini")
mino_check(mino_bundled_asset_checks "${PROJECT_SOURCE_DIR}/assets/games")
if(MINO_GRAPHICS_CHECKS)
    mino_check(mino_launcher_checks "${PROJECT_BINARY_DIR}/check-output/launcher")
    mino_check(mino_app_resume_checks gridrunner "${PROJECT_BINARY_DIR}/check-output/resume")
    if(TARGET mino_collection_title)
        add_test(NAME check.title-context-recovery COMMAND mino_app_resume_checks title "${PROJECT_BINARY_DIR}/check-output/title-resume" --lose-context)
        set_tests_properties(check.title-context-recovery PROPERTIES LABELS "graphics" TIMEOUT 180)
    endif()
    mino_check(mino_crt_checks)
    mino_check(mino_crt_optimization_checks)
    if(WIN32)
        mino_check(mino_window_checks)
    endif()
endif()
get_property(check_targets GLOBAL PROPERTY MINO_CHECK_TARGETS)
add_custom_target(check-build DEPENDS ${check_targets})
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
    DEPENDS check-build USES_TERMINAL)
