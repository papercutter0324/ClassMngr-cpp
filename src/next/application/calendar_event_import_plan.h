#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace ClassMngr::Next::Application
{

struct CalendarEventImportPlanRequest
{
    // Opaque UTF-16 code units from the legacy QString signature.
    std::vector<std::u16string> existingSignatures;
    std::vector<std::u16string> candidateSignatures;
    int initiallySkippedCount = 0;
};

struct CalendarEventImportPlan
{
    std::vector<std::size_t> acceptedCandidateIndices;
    int skippedCount = 0;
};

[[nodiscard]] inline CalendarEventImportPlan planCalendarEventImport(
    const CalendarEventImportPlanRequest& request
    )
{
    std::unordered_set<std::u16string> seenSignatures;
    seenSignatures.reserve(request.existingSignatures.size());
    seenSignatures.insert(
        request.existingSignatures.begin(),
        request.existingSignatures.end()
        );

    CalendarEventImportPlan plan;
    plan.skippedCount = request.initiallySkippedCount;
    plan.acceptedCandidateIndices.reserve(
        request.candidateSignatures.size()
        );

    for (std::size_t index = 0;
         index < request.candidateSignatures.size();
         ++index)
    {
        if (!seenSignatures.insert(
                request.candidateSignatures[index]
                ).second)
        {
            ++plan.skippedCount;
            continue;
        }

        plan.acceptedCandidateIndices.push_back(index);
    }

    return plan;
}

} // namespace ClassMngr::Next::Application
