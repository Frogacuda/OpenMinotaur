cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MinotaurWidgets LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

option(MINO_WIDGET_RENDERER "Build the optional GLES3 widget presenter" OFF)
option(MINO_WIDGET_TESTS "Build widget and title contract checks" OFF)
set(MINO_WIDGET_GL_TARGET "mino_glad" CACHE STRING "Existing GL loader target")
set(MINO_WIDGET_DEPENDENCIES "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party" CACHE PATH "glad and stb dependencies")

add_library(mino_widget_model STATIC value.cpp widget_runtime.cpp widgetml_layout.cpp title_sequence.cpp)
target_include_directories(mino_widget_model PUBLIC .)
set_target_properties(mino_widget_model PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(minotaur::widgets ALIAS mino_widget_model)

if(MINO_WIDGET_RENDERER)
    if(NOT TARGET ${MINO_WIDGET_GL_TARGET})
        add_library(mino_widget_gl STATIC "${MINO_WIDGET_DEPENDENCIES}/glad/src/gl.c")
        target_include_directories(mino_widget_gl PUBLIC "${MINO_WIDGET_DEPENDENCIES}/glad/include")
        set(MINO_WIDGET_GL_TARGET mino_widget_gl)
    endif()
    add_library(mino_widget_vectors STATIC vector/vector_scene.cpp vector/vector_font.cpp)
    target_include_directories(mino_widget_vectors PUBLIC .)
    set_target_properties(mino_widget_vectors PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(mino_widget_renderer STATIC widget_renderer.cpp title_presenter.cpp)
    target_include_directories(mino_widget_renderer PUBLIC . PRIVATE "${MINO_WIDGET_DEPENDENCIES}/stb")
    target_link_libraries(mino_widget_renderer PUBLIC mino_widget_model mino_widget_vectors ${MINO_WIDGET_GL_TARGET})
    set_target_properties(mino_widget_renderer PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(minotaur::widget_renderer ALIAS mino_widget_renderer)
endif()

if(MINO_WIDGET_TESTS)
    add_executable(mino_title_sequence_checks tests/title_sequence.cpp)
    target_link_libraries(mino_title_sequence_checks PRIVATE mino_widget_model)
endif()

# Explicit runtime resources keep optional presentation bundles small.
set(MINO_TITLE_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/title/Minotaur-Title.widgetml"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/title/assets/minotaur-face.nvector"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/title/assets/minotaur-extrusion.nvector"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/title/assets/project-face.nvector"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/title/assets/project-extrusion.nvector"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/title/assets/hammrf-raster.png"
)
set_property(TARGET mino_widget_model PROPERTY MINO_TITLE_ASSETS "${MINO_TITLE_ASSETS}")

