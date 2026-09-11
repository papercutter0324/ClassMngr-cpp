#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/result.h"

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

class SpeakingEvaluationReportOutputPolicy final
{
public:
    struct StudentFileNameInput
    {
        std::string englishName;
        std::string koreanName;
    };

    // documentsDirectory is supplied by the presentation adapter so the
    // engine does not need a platform-specific standard-paths API.
    [[nodiscard]] static std::string defaultDirectory(
        const ClassInfo& classInfo,
        std::string_view evaluationName,
        std::string_view documentsDirectory,
        std::string_view classFallback = "Speaking Evaluation",
        std::string_view evaluationFallback = "Evaluation"
        );

    [[nodiscard]] static std::string batchArchivePath(
        std::string_view outputDirectory,
        std::string_view fallbackName = "Speaking Evaluation Reports"
        );

    // The inputs are expected to be UTF-8. Unicode normalization is supplied
    // by presentation adapters that already own the platform text type.
    [[nodiscard]] static std::string studentFileName(
        std::string_view englishName,
        std::string_view koreanName,
        std::string_view extension = ".pdf",
        std::string_view fallbackName = "Student",
        char replacement = '-'
        );

    // Returns the deterministic output names in input order. Filesystem
    // existence and commit checks remain in the presentation adapter; this
    // boundary rejects Windows-compatible case-insensitive duplicate names
    // before any output is rendered.
    [[nodiscard]] static Result<std::vector<std::string>> studentFileNames(
        const std::vector<StudentFileNameInput>& students,
        std::string_view extension = ".pdf",
        std::string_view fallbackName = "Student",
        char replacement = '-'
        );
};

} // namespace classmngr::engine
