include_guard(GLOBAL)

set(CLASSMNGR_SQLITE_VERSION "3.54.1")
set(CLASSMNGR_SQLITE_SOURCE_DIR
    "${PROJECT_SOURCE_DIR}/third_party/sqlite/3.54.1")
set(_classmngr_sqlite_version_number 3054001)

foreach(_classmngr_sqlite_file IN ITEMS sqlite3.c sqlite3.h)
    if(NOT EXISTS "${CLASSMNGR_SQLITE_SOURCE_DIR}/${_classmngr_sqlite_file}")
        message(FATAL_ERROR
            "The pinned SQLite ${CLASSMNGR_SQLITE_VERSION} source is missing: "
            "${CLASSMNGR_SQLITE_SOURCE_DIR}/${_classmngr_sqlite_file}"
        )
    endif()
endforeach()

file(STRINGS "${CLASSMNGR_SQLITE_SOURCE_DIR}/sqlite3.h"
    _classmngr_sqlite_version_line
    REGEX "^#define SQLITE_VERSION_NUMBER ${_classmngr_sqlite_version_number}$"
)
if(NOT _classmngr_sqlite_version_line)
    message(FATAL_ERROR
        "The vendored SQLite header does not match pinned version "
        "${CLASSMNGR_SQLITE_VERSION}."
    )
endif()

add_library(ClassMngrSQLiteSnapshot STATIC
    "${CLASSMNGR_SQLITE_SOURCE_DIR}/sqlite3.c"
    "${CLASSMNGR_SQLITE_SOURCE_DIR}/sqlite3.h"
)
target_include_directories(ClassMngrSQLiteSnapshot
    INTERFACE
        "${CLASSMNGR_SQLITE_SOURCE_DIR}"
)
target_link_libraries(ClassMngrSQLiteSnapshot
    INTERFACE
        "${CMAKE_DL_LIBS}"
)
target_compile_definitions(ClassMngrSQLiteSnapshot
    PRIVATE
        SQLITE_THREADSAFE=1
)
set_target_properties(ClassMngrSQLiteSnapshot
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        C_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
)
