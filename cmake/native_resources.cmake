include_guard(GLOBAL)

include(cmake/resource_catalog.cmake)

classmngr_collect_resource_catalog_files(CLASSMNGR_RESOURCE_FILES)

set(CLASSMNGR_RESOURCE_MANIFEST
    "${CMAKE_CURRENT_BINARY_DIR}/generated/native-resources/manifest.json"
)
file(MAKE_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/generated/native-resources"
)
set(classmngr_resource_manifest_content
    "{\n  \"format\": \"classmngr-native-resources-v1\",\n  \"entries\": [\n"
)

list(LENGTH CLASSMNGR_RESOURCE_FILES classmngr_resource_count)
set(classmngr_resource_index 0)
foreach(resource_file IN LISTS CLASSMNGR_RESOURCE_FILES)
    if(IS_DIRECTORY "${resource_file}")
        continue()
    endif()

    file(RELATIVE_PATH resource_relative_path
        "${PROJECT_SOURCE_DIR}"
        "${resource_file}"
    )
    file(TO_CMAKE_PATH "${resource_relative_path}" resource_relative_path)
    file(SIZE "${resource_file}" resource_size)
    file(SHA256 "${resource_file}" resource_sha256)

    math(EXPR classmngr_resource_index
        "${classmngr_resource_index} + 1"
    )
    set(resource_separator ",")
    if(classmngr_resource_index EQUAL classmngr_resource_count)
        set(resource_separator "")
    endif()
    string(APPEND classmngr_resource_manifest_content
        "    {\"source\":\"${resource_relative_path}\","
        "\"key\":\"/${resource_relative_path}\","
        "\"size\":${resource_size},"
        "\"sha256\":\"${resource_sha256}\"}${resource_separator}\n"
    )
endforeach()
string(APPEND classmngr_resource_manifest_content
    "  ]\n}\n"
)

# Do not touch the manifest when its content is unchanged.  The WinUI build
# consumes this file as an input, so an unconditional file(WRITE) would make
# every reconfigure look like a resource change.
set(classmngr_existing_resource_manifest_content "")
if(EXISTS "${CLASSMNGR_RESOURCE_MANIFEST}")
    file(READ
        "${CLASSMNGR_RESOURCE_MANIFEST}"
        classmngr_existing_resource_manifest_content
    )
endif()
if(NOT "${classmngr_existing_resource_manifest_content}"
       STREQUAL "${classmngr_resource_manifest_content}")
    file(WRITE
        "${CLASSMNGR_RESOURCE_MANIFEST}"
        "${classmngr_resource_manifest_content}"
    )
endif()

add_custom_target(ClassMngrResourceManifest
    SOURCES "${CLASSMNGR_RESOURCE_MANIFEST}"
)
