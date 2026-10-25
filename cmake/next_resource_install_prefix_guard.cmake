include_guard(GLOBAL)

function(classmngr_next_add_install_prefix_guard component bindir datadir)
    if(IS_ABSOLUTE "${bindir}")
        set(_classmngr_next_bindir_is_absolute TRUE)
    else()
        set(_classmngr_next_bindir_is_absolute FALSE)
    endif()

    if(IS_ABSOLUTE "${datadir}")
        set(_classmngr_next_datadir_is_absolute TRUE)
    else()
        set(_classmngr_next_datadir_is_absolute FALSE)
    endif()

    if("${_classmngr_next_bindir_is_absolute}" STREQUAL "${_classmngr_next_datadir_is_absolute}")
        return()
    endif()

    cmake_path(NORMAL_PATH CMAKE_INSTALL_PREFIX
        OUTPUT_VARIABLE _classmngr_next_configured_install_prefix
    )
    set(_classmngr_next_install_prefix_guard_code [=[
cmake_path(NORMAL_PATH CMAKE_INSTALL_PREFIX
    OUTPUT_VARIABLE _classmngr_next_actual_install_prefix
)
set(_classmngr_next_configured_install_prefix "@_classmngr_next_configured_install_prefix@")
cmake_path(NORMAL_PATH _classmngr_next_configured_install_prefix)
if(NOT _classmngr_next_actual_install_prefix STREQUAL _classmngr_next_configured_install_prefix)
    message(FATAL_ERROR
        "ClassMngrNext resource install uses mixed absolute and relative install directories; "
        "--prefix cannot change the configured CMAKE_INSTALL_PREFIX "
        "('${_classmngr_next_configured_install_prefix}' -> "
        "'${_classmngr_next_actual_install_prefix}'). "
        "Keep the configured prefix or use DESTDIR for staging."
    )
endif()
]=])
    string(CONFIGURE
        "${_classmngr_next_install_prefix_guard_code}"
        _classmngr_next_install_prefix_guard_code
        @ONLY
    )
    install(CODE "${_classmngr_next_install_prefix_guard_code}"
        COMPONENT "${component}"
        EXCLUDE_FROM_ALL
    )
endfunction()
