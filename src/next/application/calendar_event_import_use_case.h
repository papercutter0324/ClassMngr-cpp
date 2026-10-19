#pragma once

#include "next/application/calendar_event_import_save_port.h"
#include "next/application/calendar_event_import_plan.h"
#include "next/application/calendar_event_import_signature_query_port.h"
#include "next/application/qt_compatible_text.h"
#include "next/domain/gregorian_date.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Application
{

namespace CalendarEventImportIdentity
{

namespace Detail
{

[[nodiscard]] inline std::optional<std::u16string> simplifiedUtf8Title(
    const std::string_view value
    )
{
    std::u16string simplified;
    simplified.reserve(value.size());

    bool hasPendingSpace = false;
    std::size_t offset = 0;
    while (offset < value.size())
    {
        std::uint32_t codePoint = 0;
        if (!QtCompatibleTextDetail::decodeNextUtf8CodePoint(
                value,
                offset,
                codePoint
                ))
        {
            return std::nullopt;
        }

        if (codePoint <= 0xFFFFU
            && isQtWhitespace(static_cast<char16_t>(codePoint)))
        {
            hasPendingSpace = !simplified.empty();
            continue;
        }

        if (hasPendingSpace)
        {
            simplified.push_back(u' ');
            hasPendingSpace = false;
        }

        if (codePoint <= 0xFFFFU)
        {
            simplified.push_back(static_cast<char16_t>(codePoint));
            continue;
        }

        const std::uint32_t supplementary = codePoint - 0x10000U;
        simplified.push_back(static_cast<char16_t>(
            0xD800U + (supplementary >> 10U)
            ));
        simplified.push_back(static_cast<char16_t>(
            0xDC00U + (supplementary & 0x3FFU)
            ));
    }

    return simplified;
}

[[nodiscard]] inline std::optional<std::u16string_view> canonicalEventType(
    const Domain::CalendarEventType value
    ) noexcept
{
    switch (value)
    {
    case Domain::CalendarEventType::Vacation:
        return u"Vacation";
    case Domain::CalendarEventType::Holiday:
        return u"Holiday";
    case Domain::CalendarEventType::Workshop:
        return u"Workshop";
    case Domain::CalendarEventType::CM:
        return u"CM";
    case Domain::CalendarEventType::Meeting:
        return u"Meeting";
    case Domain::CalendarEventType::Other:
        return u"Other";
    }

    return std::nullopt;
}

[[nodiscard]] inline std::optional<std::u16string_view> canonicalTimeStatus(
    const Domain::CalendarEventTimeStatus value
    ) noexcept
{
    switch (value)
    {
    case Domain::CalendarEventTimeStatus::Timed:
        return u"Timed";
    case Domain::CalendarEventTimeStatus::Unknown:
        return u"Unknown";
    case Domain::CalendarEventTimeStatus::Unconfirmed:
        return u"Unconfirmed";
    }

    return std::nullopt;
}

[[nodiscard]] inline std::u16string toUtf16(
    const std::string_view ascii
    )
{
    std::u16string value;
    value.reserve(ascii.size());
    for (const unsigned char character : ascii)
    {
        value.push_back(static_cast<char16_t>(character));
    }
    return value;
}

} // namespace Detail

// Call after CalendarEventSaveRequest validation. This derives the unchanged
// six-field legacy identity from the exact value that will be saved.
[[nodiscard]] inline std::optional<CalendarEventImportSignature>
fromSaveRequest(const CalendarEventSaveRequest& request)
{
    const std::optional<std::u16string> simplifiedTitle =
        Detail::simplifiedUtf8Title(request.title);
    const auto eventType = Domain::calendarEventTypeFromName(
        CalendarEventSaveRequestDetail::trimAscii(request.eventType)
        );
    const auto canonicalType = eventType
        ? Detail::canonicalEventType(*eventType)
        : std::nullopt;
    const auto canonicalStatus =
        Detail::canonicalTimeStatus(request.timeStatus);
    const auto startDate = Domain::GregorianDate::fromIsoString(
        request.startDate
        );
    const auto endDate = Domain::GregorianDate::fromIsoString(
        request.endDate
        );

    if (!simplifiedTitle.has_value()
        || !canonicalType.has_value()
        || !canonicalStatus.has_value()
        || !startDate.has_value()
        || !endDate.has_value())
    {
        return std::nullopt;
    }

    return CalendarEventImportSignature::fromNormalizedFields({
        .simplifiedTitle = *simplifiedTitle,
        .normalizedEventType = std::u16string(*canonicalType),
        .startDateIso = Detail::toUtf16(startDate->toIsoString()),
        .endDateIso = Detail::toUtf16(endDate->toIsoString()),
        .allDay = request.allDay,
        .normalizedTimeStatus = std::u16string(*canonicalStatus)
    });
}

} // namespace CalendarEventImportIdentity

// Keep each opaque legacy signature beside the save value it identifies so
// accepted plan indexes cannot be applied to a differently ordered batch.
struct CalendarEventImportCandidate final
{
    CalendarEventImportSignature signature;
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

        for (const CalendarEventImportCandidate& candidate : request.candidates)
        {
            const Domain::Result<void> validation =
                CalendarEventImportSaveRequestDetail::validateEvent(
                    candidate.saveRequest
                    );
            if (!validation)
            {
                return Domain::Result<CalendarEventImportUseCaseResult>::failure(
                    validation.error()
                    );
            }

            const auto expectedSignature =
                CalendarEventImportIdentity::fromSaveRequest(
                    candidate.saveRequest
                    );
            if (!expectedSignature.has_value()
                || candidate.signature != *expectedSignature)
            {
                return invalidInput(
                    "Calendar import candidate signature does not match its save request."
                    );
            }
        }

        if (!signatureRangeCoversCandidates(request))
        {
            return invalidInput(
                "Calendar event signature query range must be valid and cover all candidate dates."
                );
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

        const Domain::Result<void> saveRequestValidation =
            saveRequest.validate();
        if (!saveRequestValidation)
        {
            return Domain::Result<CalendarEventImportUseCaseResult>::failure(
                saveRequestValidation.error()
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

private:
    [[nodiscard]] static Domain::Result<CalendarEventImportUseCaseResult>
    invalidInput(const char* message)
    {
        return Domain::Result<CalendarEventImportUseCaseResult>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = message,
            .recoverable = false
        });
    }

    [[nodiscard]] static bool signatureRangeCoversCandidates(
        const CalendarEventImportUseCaseRequest& request
        )
    {
        const auto rangeStart = Domain::GregorianDate::fromIsoString(
            request.signatureRange.startDate.value()
            );
        const auto rangeEnd = Domain::GregorianDate::fromIsoString(
            request.signatureRange.endDate.value()
            );
        if (!rangeStart.has_value()
            || !rangeEnd.has_value()
            || rangeEnd->dayNumber() < rangeStart->dayNumber())
        {
            return false;
        }

        for (const CalendarEventImportCandidate& candidate : request.candidates)
        {
            const auto candidateStart = Domain::GregorianDate::fromIsoString(
                candidate.saveRequest.startDate
                );
            const auto candidateEnd = Domain::GregorianDate::fromIsoString(
                candidate.saveRequest.endDate
                );
            if (!candidateStart.has_value()
                || !candidateEnd.has_value()
                || candidateStart->dayNumber() < rangeStart->dayNumber()
                || candidateEnd->dayNumber() > rangeEnd->dayNumber())
            {
                return false;
            }
        }

        return true;
    }
};

} // namespace ClassMngr::Next::Application
