if(NOT DEFINED CLASSMNGR_PROJECT_SOURCE_DIR
   OR NOT DEFINED CLASSMNGR_NATIVE_RESOURCE_MANIFEST)
    message(FATAL_ERROR
        "The native resource manifest verifier requires project and manifest paths."
    )
endif()

set(PROJECT_SOURCE_DIR "${CLASSMNGR_PROJECT_SOURCE_DIR}")
include("${PROJECT_SOURCE_DIR}/cmake/resource_catalog.cmake")
classmngr_collect_resource_catalog_files(classmngr_expected_files)

if(NOT EXISTS "${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}")
    message(FATAL_ERROR
        "Native resource manifest does not exist: "
        "${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}"
    )
endif()

file(READ "${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}" classmngr_manifest_json)
string(JSON classmngr_manifest_count
    LENGTH "${classmngr_manifest_json}" entries
)
list(LENGTH classmngr_expected_files classmngr_expected_count)
if(NOT classmngr_manifest_count EQUAL classmngr_expected_count)
    message(FATAL_ERROR
        "Native resource manifest has ${classmngr_manifest_count} entries; "
        "catalog has ${classmngr_expected_count}."
    )
endif()

math(EXPR classmngr_last_index "${classmngr_expected_count} - 1")
foreach(classmngr_index RANGE 0 ${classmngr_last_index})
    list(GET classmngr_expected_files ${classmngr_index} classmngr_expected_file)
    file(RELATIVE_PATH classmngr_expected_source
        "${PROJECT_SOURCE_DIR}"
        "${classmngr_expected_file}"
    )
    file(TO_CMAKE_PATH "${classmngr_expected_source}" classmngr_expected_source)

    string(JSON classmngr_manifest_source GET
        "${classmngr_manifest_json}" entries ${classmngr_index} source
    )
    string(JSON classmngr_manifest_key GET
        "${classmngr_manifest_json}" entries ${classmngr_index} key
    )
    string(JSON classmngr_manifest_size GET
        "${classmngr_manifest_json}" entries ${classmngr_index} size
    )
    string(JSON classmngr_manifest_sha256 GET
        "${classmngr_manifest_json}" entries ${classmngr_index} sha256
    )

    if(NOT classmngr_manifest_source STREQUAL classmngr_expected_source)
        message(FATAL_ERROR
            "Native resource source mismatch at entry ${classmngr_index}: "
            "'${classmngr_manifest_source}' vs '${classmngr_expected_source}'."
        )
    endif()
    if(NOT classmngr_manifest_key STREQUAL "/${classmngr_expected_source}")
        message(FATAL_ERROR
            "Native resource key mismatch for '${classmngr_expected_source}'."
        )
    endif()

    file(SIZE "${classmngr_expected_file}" classmngr_expected_size)
    if(NOT classmngr_manifest_size EQUAL classmngr_expected_size)
        message(FATAL_ERROR
            "Native resource size mismatch for '${classmngr_expected_source}'."
        )
    endif()

    file(SHA256 "${classmngr_expected_file}" classmngr_expected_sha256)
    if(NOT classmngr_manifest_sha256 STREQUAL classmngr_expected_sha256)
        message(FATAL_ERROR
            "Native resource hash mismatch for '${classmngr_expected_source}'."
        )
    endif()
endforeach()

message(STATUS
    "Verified ${classmngr_manifest_count} native resource entries against the catalog."
)
