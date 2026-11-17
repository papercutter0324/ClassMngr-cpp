if(NOT WIN32)
    return()
endif()

# The diagnostic inspects the actual selected QFontEngine backend after
# resolving Inter styles. Qt exposes that implementation type through this
# exact-version private module; product targets do not depend on it.
find_package(Qt6GuiPrivate 6.12.0 EXACT REQUIRED CONFIG)

add_executable(ClassMngrNextFontAllocationProbe
    "${PROJECT_SOURCE_DIR}/cmake/phase9/font_allocation_probe.cpp"
)
target_compile_features(ClassMngrNextFontAllocationProbe
    PRIVATE
        cxx_std_23
)
target_include_directories(ClassMngrNextFontAllocationProbe
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(ClassMngrNextFontAllocationProbe
    PRIVATE
        ClassMngrBuildSettings
        ClassMngrProcessMemory
        Qt6::GuiPrivate
        Qt6::Widgets
)
set_target_properties(ClassMngrNextFontAllocationProbe
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}/phase9-probe"
)
