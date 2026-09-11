#include "classmngr/engine/database_file_format.h"

#include <iostream>
#include <string_view>

namespace
{
namespace DatabaseFileFormat =
    classmngr::engine::DatabaseFileFormat;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineDatabaseFileFormatTests: "
              << message
              << '\n';
    return false;
}
}

int main()
{
    bool passed = true;

    passed &= expect(
        DatabaseFileFormat::nativeExtension() == ".tps",
        "native extension changed"
        );
    passed &= expect(
        DatabaseFileFormat::legacyExtension() == ".db",
        "legacy extension changed"
        );

    passed &= expect(
        DatabaseFileFormat::isNativePath("school.tps"),
        "native path was not recognized"
        );
    passed &= expect(
        DatabaseFileFormat::isNativePath("school.TPS"),
        "native extension comparison is not case insensitive"
        );
    passed &= expect(
        DatabaseFileFormat::isLegacyPath("school.DB"),
        "legacy path was not recognized"
        );
    passed &= expect(
        DatabaseFileFormat::isSupportedInputPath("school.tps"),
        "native path was not accepted as input"
        );
    passed &= expect(
        DatabaseFileFormat::isSupportedInputPath("school.db"),
        "legacy path was not accepted as input"
        );
    passed &= expect(
        !DatabaseFileFormat::isSupportedInputPath("school.sqlite"),
        "unknown extension was accepted as input"
        );

    passed &= expect(
        DatabaseFileFormat::nativeOutputPath("school") == "school.tps",
        "native output extension was not appended"
        );
    passed &= expect(
        DatabaseFileFormat::nativeOutputPath("school.TPS") == "school.TPS",
        "native output changed an existing native extension"
        );
    passed &= expect(
        DatabaseFileFormat::nativeOutputPath("school.DB") == "school.tps",
        "legacy output extension was not migrated"
        );
    passed &= expect(
        DatabaseFileFormat::nativeOutputPath("학교") == "학교.tps",
        "UTF-8 output path was not preserved"
        );
    passed &= expect(
        DatabaseFileFormat::nativeOutputPath("   ") == "   ",
        "blank output path was changed"
        );

    passed &= expect(
        DatabaseFileFormat::supportedInputPath("school") == "school.tps",
        "extensionless input path was not completed"
        );
    passed &= expect(
        DatabaseFileFormat::supportedInputPath("school.tps") == "school.tps",
        "native input path was changed"
        );
    passed &= expect(
        DatabaseFileFormat::supportedInputPath("legacy.DB") == "legacy.DB",
        "legacy input path was changed"
        );
    passed &= expect(
        DatabaseFileFormat::supportedInputPath("school.sqlite") == "school.sqlite",
        "unknown extension was changed"
        );
    passed &= expect(
        DatabaseFileFormat::supportedInputPath("folder.with.dot/school")
            == "folder.with.dot/school.tps",
        "directory dots were mistaken for a file suffix"
        );
    passed &= expect(
        DatabaseFileFormat::supportedInputPath("school.") == "school..tps",
        "empty file suffix behavior changed"
        );

    return passed ? 0 : 1;
}
