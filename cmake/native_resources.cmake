include_guard(GLOBAL)

include(cmake/resource_catalog.cmake)

classmngr_collect_resource_catalog_files(CLASSMNGR_NATIVE_RESOURCE_FILES)

set(CLASSMNGR_NATIVE_RESOURCE_MANIFEST
    "${CMAKE_CURRENT_BINARY_DIR}/generated/native-resources/manifest.json"
)
file(MAKE_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/generated/native-resources"
)
file(WRITE "${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}" "{\n  \"format\": \"classmngr-native-resources-v1\",\n  \"entries\": [\n")

list(LENGTH CLASSMNGR_NATIVE_RESOURCE_FILES classmngr_native_resource_count)
set(classmngr_native_resource_index 0)
foreach(resource_file IN LISTS CLASSMNGR_NATIVE_RESOURCE_FILES)
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

    math(EXPR classmngr_native_resource_index
        "${classmngr_native_resource_index} + 1"
    )
    set(resource_separator ",")
    if(classmngr_native_resource_index EQUAL classmngr_native_resource_count)
        set(resource_separator "")
    endif()
    file(APPEND "${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}"
        "    {\"source\":\"${resource_relative_path}\","
        "\"key\":\"/${resource_relative_path}\","
        "\"size\":${resource_size},"
        "\"sha256\":\"${resource_sha256}\"}${resource_separator}\n"
    )
endforeach()
file(APPEND "${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}"
    "  ]\n}\n"
)

add_custom_target(ClassMngrNativeResourceManifest
    SOURCES "${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}"
)
