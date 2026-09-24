#pragma once

#include "next/application/calendar_event_import_plan.h"
#include "next/application/calendar_event_import_save_port.h"
#include "next/application/calendar_event_import_signature_query_port.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// Keep each opaque legacy signature beside the save value it identifies so
// accepted plan indexes cannot be applied to a differently ordered batch.
struct CalendarEventImportCandidate final
{
    std::u16string signature;
    CalendarEventSaveRequest saveRequest;

    friend bool operator==(
        const CalendarEventImportCandidate&,
        const CalendarEventImportCandidate&
        ) = default;
};

struct CalendarEventImportUseCaseRequest final
{
    CalendarEventImportSignatureRangeRequest signatureRange;
    std::vector<CalendarEventImportCandidate> candidates;
    int initiallySkippedCount = 0;
};

struct CalendarEventImportUseCaseResult final
{
    int importedCount = 0;
    int skippedCount = 0;

    friend bool operator==(
        const CalendarEventImportUseCaseResult&,
        const CalendarEventImportUseCaseResult&
        ) = default;
};

// The feature can observe these boundaries for existing profiler events while
// the use case retains query, planning, and save orchestration ownership.
class CalendarEventImportUseCaseObserver
{
public:
    virtual ~CalendarEventImportUseCaseObserver() = default;

    virtual void existingSignaturesLoaded(std::size_t count) = 0;
    virtual void savePrepared(
        std::size_t eventCount,
        int skippedCount
        ) = 0;
};

class CalendarEventImportUseCase final
{
public:
    [[nodiscard]] static Domain::Result<CalendarEventImportUseCaseResult>
    execute(
        const CalendarEventImportUseCaseRequest& request,
        const CalendarEventImportSignatureQueryPort& signatureQueryPort,
        CalendarEventImportSavePort& savePort,
        CalendarEventImportUseCaseObserver* observer = nullptr
        )
    {
        if (request.candidates.empty())
        {
            return Domain::Result<CalendarEventImportUseCaseResult>::success({
                .importedCount = 0,
                .skippedCount = request.initiallySkippedCount
            });
        }

        const CalendarEventImportSignatureQueryResult existingSignatures =
            signatureQueryPort.loadSignaturesInRange(request.signatureRange);
        if (!existingSignatures)
        {
            return Domain::Result<CalendarEventImportUseCaseResult>::failure(
                existingSignatures.error()
                );
        }

        if (observer != nullptr)
        {
            observer->existingSignaturesLoaded(
                existingSignatures.value().size()
                );
        }

        CalendarEventImportPlanRequest planRequest;
        planRequest.initiallySkippedCount = request.initiallySkippedCount;
        planRequest.existingSignatures = existingSignatures.value();
        planRequest.candidateSignatures.reserve(request.candidates.size());
        for (const CalendarEventImportCandidate& candidate : request.candidates)
        {
            planRequest.candidateSignatures.push_back(candidate.signature);
        }

        const CalendarEventImportPlan plan =
            planCalendarEventImport(planRequest);

        CalendarEventImportSaveRequest saveRequest;
        saveRequest.events.reserve(plan.acceptedCandidateIndices.size());
        for (const std::size_t candidateIndex :
             plan.acceptedCandidateIndices)
        {
            saveRequest.events.push_back(
                request.candidates.at(candidateIndex).saveRequest
                );
        }

        if (observer != nullptr)
        {
            observer->savePrepared(
                saveRequest.events.size(),
                plan.skippedCount
                );
        }

        // A non-empty parsed batch that contains only duplicates still makes
        // one empty save call, preserving the legacy batch path semantics.
        const CalendarEventImportSaveResult saved =
            savePort.saveImportedEvents(saveRequest);
        if (!saved)
        {
            return Domain::Result<CalendarEventImportUseCaseResult>::failure(
                saved.error()
                );
        }

        return Domain::Result<CalendarEventImportUseCaseResult>::success({
            .importedCount = static_cast<int>(saved.value().size()),
            .skippedCount = plan.skippedCount
        });
    }
};

} // namespace ClassMngr::Next::Application
