#pragma once

#include "classmngr/engine/class_info.h"

#include <string>
#include <string_view>

namespace classmngr::engine
{

class SpeakingEvaluationReportOutputPolicy final
{
public:
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
};

} // namespace classmngr::engine
