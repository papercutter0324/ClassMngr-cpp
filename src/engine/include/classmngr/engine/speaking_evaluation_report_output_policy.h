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
};

} // namespace classmngr::engine
