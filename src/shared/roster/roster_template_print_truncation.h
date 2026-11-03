#pragma once

#include "shared/roster/roster_template_renderer.h"

#include <algorithm>
#include <vector>

namespace RosterTemplatePrintTruncation
{

inline constexpr qsizetype ByDayAndDailyCapacity = 25;
inline constexpr qsizetype PerClassWithExtraInfoCapacity = 23;

struct ClassStudentCounts final
{
    qsizetype total = 0;
    qsizetype positionalIncluded = 0;
};

struct Summary final
{
    qsizetype capacityPerClass = 0;
    qsizetype totalStudents = 0;
    qsizetype includedStudents = 0;

    [[nodiscard]] qsizetype omittedStudents() const noexcept
    {
        return std::max<qsizetype>(0, totalStudents - includedStudents);
    }

    [[nodiscard]] bool requiresConfirmation() const noexcept
    {
        return omittedStudents() > 0;
    }
};

[[nodiscard]] inline Summary summarize(
    const RosterTemplateRenderer::TemplateId templateId,
    const std::vector<ClassStudentCounts>& classes
    )
{
    const bool positional = templateId
        == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo;
    Summary result;
    result.capacityPerClass = positional
        ? PerClassWithExtraInfoCapacity
        : ByDayAndDailyCapacity;
    for (const ClassStudentCounts& counts : classes)
    {
        const qsizetype total = std::max<qsizetype>(0, counts.total);
        const qsizetype included = positional
            ? std::min(total, std::max<qsizetype>(0, counts.positionalIncluded))
            : std::min(total, result.capacityPerClass);
        result.totalStudents += total;
        result.includedStudents += included;
    }
    return result;
}

} // namespace RosterTemplatePrintTruncation
