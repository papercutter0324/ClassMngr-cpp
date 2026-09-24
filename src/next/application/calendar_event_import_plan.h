#pragma once

#include "next/application/calendar_event_import_signature.h"

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace ClassMngr::Next::Application
{

struct CalendarEventImportPlanRequest
{
    // Opaque UTF-16 code units from the legacy QString signature.
    std::vector<CalendarEventImportSignature> existingSignatures;
    std::vector<CalendarEventImportSignature> candidateSignatures;
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
    struct SignatureHash final
    {
        [[nodiscard]] std::size_t operator()(
            const CalendarEventImportSignature& signature
            ) const noexcept
        {
            return std::hash<std::u16string>{}(signature.value());
        }
    };

    std::unordered_set<CalendarEventImportSignature, SignatureHash>
        seenSignatures;
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
