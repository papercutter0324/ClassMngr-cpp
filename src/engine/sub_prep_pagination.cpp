#include "classmngr/engine/sub_prep_pagination.h"

#include <algorithm>
#include <cmath>

namespace classmngr::engine
{

std::vector<int> SubPrepPaginationService::teacherSectionsThatSpanPages(
    const std::vector<SubPrepTeacherSectionMeasurement>& sections,
    double pageHeight
    )
{
    std::vector<int> spanningSections;
    if (pageHeight <= 0.0)
    {
        return spanningSections;
    }

    for (std::size_t index = 0; index < sections.size(); ++index)
    {
        const SubPrepTeacherSectionMeasurement& section = sections[index];
        if (section.height <= 0.0 || section.height >= pageHeight)
        {
            continue;
        }

        const int firstPage = static_cast<int>(
            std::floor(section.top / pageHeight)
            );
        const int lastPage = static_cast<int>(
            std::floor((section.top + section.height - 0.01) / pageHeight)
            );
        if (firstPage != lastPage)
        {
            spanningSections.push_back(static_cast<int>(index));
        }
    }

    return spanningSections;
}

bool SubPrepPaginationService::shouldStartSubNotesOnNewPage(
    double headingBottom,
    double pageHeight
    )
{
    if (pageHeight <= 0.0)
    {
        return false;
    }

    const int pageIndex = std::max(
        0,
        static_cast<int>(std::floor((headingBottom - 0.01) / pageHeight))
        );
    const double availableBelowHeading =
        ((pageIndex + 1) * pageHeight) - headingBottom;
    return availableBelowHeading < (pageHeight / 3.0);
}

std::optional<double> SubPrepPaginationService::fallbackSubNotesTop(
    double contentBottom,
    int documentPageCount,
    double pageHeight,
    double majorSectionSpacing
    )
{
    if (pageHeight <= 0.0 || documentPageCount <= 0)
    {
        return std::nullopt;
    }

    const int contentPageIndex = std::max(
        0,
        static_cast<int>(std::floor((contentBottom - 0.01) / pageHeight))
        );
    const double contentBottomOnPage =
        contentBottom - (contentPageIndex * pageHeight);
    const double availableHeight = pageHeight - contentBottomOnPage;

    if (contentPageIndex != documentPageCount - 1
        || availableHeight < (pageHeight / 2.0))
    {
        return std::nullopt;
    }

    return contentBottomOnPage + majorSectionSpacing;
}

} // namespace classmngr::engine
