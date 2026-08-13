#pragma once

#include <QString>

#include <array>

class SpeakingEvalReportDataAssembler final
{
public:
    [[nodiscard]] static QString overallGrade(
        const std::array<QString, 6>& scores
        );
};
