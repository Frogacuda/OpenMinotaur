# Minotron core: the Neon engine copy + game code, in namespace minotron::.
file(GLOB MINOTRON_ENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp)
list(APPEND MINOTRON_ENGINE_SOURCES
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_mixer.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_create.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/neon_synth.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/mod_showjoy.cpp
    ${PROJECT_SOURCE_DIR}/engine/bindings/early/neon_sprite.cpp)
file(GLOB MINOTRON_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(mino_core_minotron STATIC ${MINOTRON_ENGINE_SOURCES} ${MINOTRON_CORE_SOURCES}
    ${PROJECT_SOURCE_DIR}/engine/bindings/common/neon_base.cpp
)
target_compile_definitions(mino_core_minotron PRIVATE NEON_GAME=minotron)
target_include_directories(mino_core_minotron PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(mino_core_minotron PUBLIC neon_renderer mino_glad mino_pugixml mino_shared)
target_include_directories(mino_core_minotron PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/third_party/stb)
if(NOT MSVC)
    target_compile_options(mino_core_minotron PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Explicit opt-in integration check: no SDL window or graphics context required.
add_executable(mino_minotron_host_checks EXCLUDE_FROM_ALL
    ${PROJECT_SOURCE_DIR}/tests/minotron_game_host.cpp)
target_compile_definitions(mino_minotron_host_checks PRIVATE NEON_GAME=minotron)
target_include_directories(mino_minotron_host_checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mino_minotron_host_checks PRIVATE mino_core_minotron)

# Match source floating-point operation boundaries in shared template consumers.
target_link_libraries(mino_core_minotron PRIVATE neon_source_numeric)
