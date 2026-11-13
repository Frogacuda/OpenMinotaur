# Baked resource bytes are source data; C++ initializers belong in the build tree.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(baked_manifest "${PROJECT_SOURCE_DIR}/assets/baked/MANIFEST.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${baked_manifest}")
file(READ "${baked_manifest}" baked_manifest_json)
string(JSON baked_count LENGTH "${baked_manifest_json}" assets)
math(EXPR baked_last "${baked_count} - 1")
set(baked_inputs "")
set(baked_includes "")
foreach(index RANGE ${baked_last})
    string(JSON resource GET "${baked_manifest_json}" assets ${index} file)
    list(APPEND baked_inputs "${PROJECT_SOURCE_DIR}/assets/baked/${resource}")
    string(REGEX REPLACE "[.][^.]+$" ".inc" resource_include "${resource}")
    list(APPEND baked_includes "${PROJECT_BINARY_DIR}/generated/baked_assets/${resource_include}")
endforeach()
set(baked_stamp "${PROJECT_BINARY_DIR}/generated/baked-assets.stamp")
add_custom_command(OUTPUT "${baked_stamp}"
    BYPRODUCTS ${baked_includes}
    COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/embed_baked_assets.py"
        --manifest "${baked_manifest}" --output "${PROJECT_BINARY_DIR}/generated/baked_assets"
    COMMAND ${CMAKE_COMMAND} -E touch "${baked_stamp}"
    DEPENDS ${baked_inputs} "${baked_manifest}" "${PROJECT_SOURCE_DIR}/tools/embed_baked_assets.py"
    VERBATIM)
add_custom_target(mino_baked_asset_bytes DEPENDS "${baked_stamp}")
add_library(mino_baked_assets INTERFACE)
target_include_directories(mino_baked_assets INTERFACE "${PROJECT_BINARY_DIR}/generated")
add_dependencies(mino_baked_assets mino_baked_asset_bytes)
