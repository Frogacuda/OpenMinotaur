# Rescue game/presentation bindings; resource catalog and GL backend are shared.
file(GLOB RESCUE_ENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp)
list(APPEND RESCUE_ENGINE_SOURCES
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_mixer.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_create.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/neon_synth.cpp)
file(GLOB RESCUE_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(mino_core_minorescue STATIC ${RESCUE_ENGINE_SOURCES} ${RESCUE_CORE_SOURCES})
target_compile_definitions(mino_core_minorescue PRIVATE NEON_GAME=minorescue)
target_include_directories(mino_core_minorescue PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(mino_core_minorescue PUBLIC mino_glad mino_pugixml mino_shared neon_renderer)
target_include_directories(mino_core_minorescue PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/third_party/stb)
if(NOT MSVC)
    target_compile_options(mino_core_minorescue PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(mino_core_minorescue PUBLIC ${PROJECT_SOURCE_DIR}/engine/include)

# Match source floating-point operation boundaries in shared template consumers.
target_link_libraries(mino_core_minorescue PRIVATE neon_source_numeric)
