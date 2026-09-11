#pragma once

#include <optional>
#include <vector>

namespace classmngr::engine
{

struct SubPrepTeacherSectionMeasurement
{
    double top = 0.0;
    double height = 0.0;
};

class SubPrepPaginationService final
{
public:
    [[nodiscard]] static std::vector<int> teacherSectionsThatSpanPages(
        const std::vector<SubPrepTeacherSectionMeasurement>& sections,
        double pageHeight
        );

    [[nodiscard]] static bool shouldStartSubNotesOnNewPage(
        double headingBottom,
        double pageHeight
        );

    [[nodiscard]] static std::optional<double> fallbackSubNotesTop(
        double contentBottom,
        int documentPageCount,
        double pageHeight,
        double majorSectionSpacing
        );
};

} // namespace classmngr::engine
