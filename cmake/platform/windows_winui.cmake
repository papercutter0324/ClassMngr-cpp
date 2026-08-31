include_guard(GLOBAL)

if(NOT WIN32)
    message(FATAL_ERROR
        "windows_winui.cmake may only be included for a Windows target."
    )
endif()

if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    message(FATAL_ERROR
        "The WinUI target must be built by a Visual Studio generator so the "
        "official XAML and Windows App SDK MSBuild targets remain authoritative."
    )
endif()

set(CLASSMNGR_WINUI_MIN_WINDOWS_SDK "10.0.26100.0")
if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_LESS
        CLASSMNGR_WINUI_MIN_WINDOWS_SDK)
    message(FATAL_ERROR
        "Windows WinUI builds require Windows SDK "
        "${CLASSMNGR_WINUI_MIN_WINDOWS_SDK} or newer; selected "
        "'${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}'."
    )
endif()

set(CLASSMNGR_WINUI_MIN_WINDOWS_VERSION "10.0.17763.0")
set(CLASSMNGR_WINUI_WINDOWS_APP_SDK_VERSION "2.4.0")
set(CLASSMNGR_WINUI_WINDOWS_APP_SDK_BASE_VERSION "2.0.4")
set(CLASSMNGR_WINUI_WINDOWS_APP_SDK_FOUNDATION_VERSION "2.3.9")
set(CLASSMNGR_WINUI_WINDOWS_APP_SDK_INTERACTIVE_VERSION "2.1.6")
set(CLASSMNGR_WINUI_WINDOWS_APP_SDK_WINUI_VERSION "2.3.6")
set(CLASSMNGR_WINUI_WINDOWS_APP_SDK_RUNTIME_VERSION "2.4.0")
set(CLASSMNGR_WINUI_WEBVIEW2_VERSION "1.0.3719.77")
set(CLASSMNGR_WINUI_MSIX_BUILD_TOOLS_VERSION "1.7.251221100")
set(CLASSMNGR_WINUI_CPPWINRT_VERSION "3.0.260818.1")
set(CLASSMNGR_WINUI_BUILD_TOOLS_VERSION "10.0.26100.4654")

if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    set(CLASSMNGR_WINUI_PLATFORM "x64")
    set(CLASSMNGR_WINUI_STAGE_ARCHITECTURE "x64")
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
    set(CLASSMNGR_WINUI_PLATFORM "Win32")
    set(CLASSMNGR_WINUI_STAGE_ARCHITECTURE "x86")
else()
    message(FATAL_ERROR
        "Windows WinUI supports only the x64 and Win32 project platforms; "
        "selected '${CMAKE_GENERATOR_PLATFORM}'."
    )
endif()

set(CLASSMNGR_WINDOWS_WINUI_STAGE_ROOT
    "${PROJECT_SOURCE_DIR}/dist/ClassMngr-windows-winui-${CLASSMNGR_WINUI_STAGE_ARCHITECTURE}"
    CACHE PATH
    "Architecture-specific root for the unpackaged WinUI staging tree"
)

set(CLASSMNGR_WINUI_PROJECT_FILE
    "${PROJECT_SOURCE_DIR}/src/platform/windows/winui/ClassMngrWinUI.vcxproj"
)
set(CLASSMNGR_WINUI_PROJECT_DIRECTORY
    "${PROJECT_SOURCE_DIR}/src/platform/windows/winui"
)
set(CLASSMNGR_WINUI_SCRIPT
    "${PROJECT_SOURCE_DIR}/scripts/build_windows_winui.ps1"
)
set(CLASSMNGR_WINUI_STAGE_VERIFICATION_SCRIPT
    "${PROJECT_SOURCE_DIR}/scripts/verify_windows_winui_stage.ps1"
)
set(CLASSMNGR_WINUI_NUGET_CONFIG
    "${CLASSMNGR_WINUI_PROJECT_DIRECTORY}/NuGet.Config"
)
set(CLASSMNGR_WINUI_PACKAGES_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/winui/packages"
)
set(CLASSMNGR_WINUI_INTERMEDIATE_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/winui/intermediate"
)
set(CLASSMNGR_WINUI_GENERATED_INCLUDE_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/generated/winui"
)
set(CLASSMNGR_WINUI_RESOURCE_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/winui/ClassMngrWinUI.rc"
)

file(MAKE_DIRECTORY "${CLASSMNGR_WINUI_GENERATED_INCLUDE_DIRECTORY}")
configure_file(
    "${PROJECT_SOURCE_DIR}/src/platform/windows/winui/winui_build_info.h.in"
    "${CLASSMNGR_WINUI_GENERATED_INCLUDE_DIRECTORY}/winui_build_info.h"
    @ONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/src/platform/windows/winui/ClassMngrWinUI.rc.in"
    "${CLASSMNGR_WINUI_RESOURCE_FILE}"
    @ONLY
)

# Prefer the inbox Windows PowerShell executable.  It is available on every
# supported Windows development host and avoids resolving the WindowsApps
# App Execution Alias for pwsh, which is not always executable by build tools.
find_program(CLASSMNGR_WINUI_POWERSHELL_EXECUTABLE
    NAMES powershell pwsh
)
if(NOT CLASSMNGR_WINUI_POWERSHELL_EXECUTABLE)
    message(FATAL_ERROR
        "PowerShell is required to orchestrate the WinUI MSBuild project."
    )
endif()

function(classmngr_add_windows_winui_target)
    set(classmngr_winui_target ClassMngrWindowsWinUI)

    add_custom_target(${classmngr_winui_target}
        COMMAND "${CLASSMNGR_WINUI_POWERSHELL_EXECUTABLE}"
            -NoLogo
            -NoProfile
            -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CLASSMNGR_WINUI_SCRIPT}"
            -ProjectFile "${CLASSMNGR_WINUI_PROJECT_FILE}"
            -ProjectDirectory "${CLASSMNGR_WINUI_PROJECT_DIRECTORY}"
            -Configuration "$<CONFIG>"
            -Platform "${CLASSMNGR_WINUI_PLATFORM}"
            -PackagesDirectory "${CLASSMNGR_WINUI_PACKAGES_DIRECTORY}"
            -NuGetConfigFile "${CLASSMNGR_WINUI_NUGET_CONFIG}"
            -OutputDirectory
                "${CLASSMNGR_WINDOWS_WINUI_STAGE_ROOT}/$<CONFIG>"
            -IntermediateDirectory
                "${CLASSMNGR_WINUI_INTERMEDIATE_DIRECTORY}/$<CONFIG>"
            -EngineLibrary "$<TARGET_FILE:ClassMngrEngine>"
            -EngineIncludeDirectory
                "${PROJECT_SOURCE_DIR}/src/engine/include"
            -GeneratedIncludeDirectory
                "${CLASSMNGR_WINUI_GENERATED_INCLUDE_DIRECTORY}"
            -ResourceFile "${CLASSMNGR_WINUI_RESOURCE_FILE}"
            -ResourceManifest "${CLASSMNGR_RESOURCE_MANIFEST}"
            -ProjectRoot "${PROJECT_SOURCE_DIR}"
            -MinimumWindowsVersion "${CLASSMNGR_WINUI_MIN_WINDOWS_VERSION}"
            -WindowsAppSdkVersion "${CLASSMNGR_WINUI_WINDOWS_APP_SDK_VERSION}"
            -WindowsAppSdkBaseVersion
                "${CLASSMNGR_WINUI_WINDOWS_APP_SDK_BASE_VERSION}"
            -WindowsAppSdkFoundationVersion
                "${CLASSMNGR_WINUI_WINDOWS_APP_SDK_FOUNDATION_VERSION}"
            -WindowsAppSdkInteractiveVersion
                "${CLASSMNGR_WINUI_WINDOWS_APP_SDK_INTERACTIVE_VERSION}"
            -WindowsAppSdkWinUIVersion
                "${CLASSMNGR_WINUI_WINDOWS_APP_SDK_WINUI_VERSION}"
            -WindowsAppSdkRuntimeVersion
                "${CLASSMNGR_WINUI_WINDOWS_APP_SDK_RUNTIME_VERSION}"
            -WebView2Version "${CLASSMNGR_WINUI_WEBVIEW2_VERSION}"
            -WindowsSdkMsixVersion
                "${CLASSMNGR_WINUI_MSIX_BUILD_TOOLS_VERSION}"
            -CppWinRTVersion "${CLASSMNGR_WINUI_CPPWINRT_VERSION}"
            -BuildToolsVersion "${CLASSMNGR_WINUI_BUILD_TOOLS_VERSION}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        DEPENDS
            ClassMngrEngine
            ClassMngrResourceManifest
        USES_TERMINAL
    )

    if(TARGET ClassMngrEngineTests)
        add_dependencies(${classmngr_winui_target} ClassMngrEngineTests)
    endif()
    if(TARGET ClassMngrEngineDatabaseFileFormatTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineDatabaseFileFormatTests
        )
    endif()
    if(TARGET ClassMngrEngineSqliteDatabaseTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSqliteDatabaseTests
        )
    endif()
    if(TARGET ClassMngrEngineDatabaseSchemaTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineDatabaseSchemaTests
        )
    endif()
    if(TARGET ClassMngrEngineClassRepositoryTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineClassRepositoryTests
        )
    endif()
    if(TARGET ClassMngrEngineTeacherServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineTeacherServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineNativeEnglishTeacherServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineNativeEnglishTeacherServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineGsTeamServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineGsTeamServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineClassInfoServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineClassInfoServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineClassScheduleServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineClassScheduleServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineScheduleImportServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineScheduleImportServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineClassTransferServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineClassTransferServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingAnalyticsTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingAnalyticsTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationValidatorTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationValidatorTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationReportServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationReportServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationReportModelTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationReportModelTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationReportTemplateTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationReportTemplateTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationReportContentTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationReportContentTests
        )
    endif()
    if(TARGET ClassMngrEngineSpeakingEvaluationAiPromptTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSpeakingEvaluationAiPromptTests
        )
    endif()
    if(TARGET ClassMngrEngineScheduleReportServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineScheduleReportServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineRosterReportServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineRosterReportServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineRosterServiceTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineRosterServiceTests
        )
    endif()
    if(TARGET ClassMngrEngineRosterReportTemplateTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineRosterReportTemplateTests
        )
    endif()
    if(TARGET ClassMngrEngineSubPrepPaginationTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSubPrepPaginationTests
        )
    endif()
    if(TARGET ClassMngrEngineSubPrepDocumentTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineSubPrepDocumentTests
        )
    endif()
    if(TARGET ClassMngrEngineAcademicCalendarTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineAcademicCalendarTests
        )
    endif()
    if(TARGET ClassMngrEngineCalendarEventRulesTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineCalendarEventRulesTests
        )
    endif()
    if(TARGET ClassMngrEngineCalendarEventValidatorTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineCalendarEventValidatorTests
        )
    endif()
    if(TARGET ClassMngrEngineDocumentCatalogTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineDocumentCatalogTests
        )
    endif()
    if(TARGET ClassMngrEngineZipArchiveWriterTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineZipArchiveWriterTests
        )
    endif()
    if(TARGET ClassMngrEngineDocumentOutputResultTests)
        add_dependencies(
            ${classmngr_winui_target}
            ClassMngrEngineDocumentOutputResultTests
        )
    endif()

    if(BUILD_TESTING)
        add_test(
            NAME ClassMngrWindowsWinUIStageTests
            COMMAND "${CLASSMNGR_WINUI_POWERSHELL_EXECUTABLE}"
                -NoLogo
                -NoProfile
                -NonInteractive
                -ExecutionPolicy Bypass
                -File "${CLASSMNGR_WINUI_STAGE_VERIFICATION_SCRIPT}"
                -StageDirectory
                    "${CLASSMNGR_WINDOWS_WINUI_STAGE_ROOT}/$<CONFIG>"
                -Platform "${CLASSMNGR_WINUI_PLATFORM}"
        )
        add_test(
            NAME ClassMngrWindowsWinUIResourceManifestTests
            COMMAND "${CMAKE_COMMAND}"
                -DCLASSMNGR_PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
                -DCLASSMNGR_RESOURCE_MANIFEST=${CLASSMNGR_RESOURCE_MANIFEST}
                -P "${PROJECT_SOURCE_DIR}/cmake/verify_resource_manifest.cmake"
        )
        set_tests_properties(ClassMngrWindowsWinUIStageTests
            PROPERTIES
                LABELS "windows;winui;phase1"
        )
        set_tests_properties(ClassMngrWindowsWinUIResourceManifestTests
            PROPERTIES
                LABELS "windows;winui;phase1"
        )
    endif()
endfunction()
