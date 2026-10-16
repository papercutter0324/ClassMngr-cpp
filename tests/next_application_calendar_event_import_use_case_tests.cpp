#include "next/application/calendar_event_import_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

std::string repeatedHangulTitle(const std::size_t count)
{
    QString title;
    for (std::size_t index = 0; index < count; ++index)
    {
        title.append(QChar(0xAC00));
    }
    return title.toUtf8().toStdString();
}

class FakeSignatureQueryPort final : public CalendarEventImportSignatureQueryPort
{
public:
    [[nodiscard]] bool isAvailable() const noexcept override
    {
        return available;
    }

    [[nodiscard]] CalendarEventImportSignatureQueryResult
    loadSignaturesInRange(
        const CalendarEventImportSignatureRangeRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        if (failure.has_value())
        {
            return CalendarEventImportSignatureQueryResult::failure(*failure);
        }

        return CalendarEventImportSignatureQueryResult::success(signatures);
    }

    bool available = true;
    mutable int callCount = 0;
    mutable CalendarEventImportSignatureRangeRequest lastRequest;
    std::vector<CalendarEventImportSignature> signatures;
    std::optional<Domain::OperationError> failure;
};

class FakeSavePort final : public CalendarEventImportSavePort
{
public:
    [[nodiscard]] CalendarEventImportSaveResult saveImportedEvents(
        const CalendarEventImportSaveRequest& request
        ) override
    {
        ++callCount;
        lastRequest = request;
        if (failure.has_value())
        {
            return CalendarEventImportSaveResult::failure(*failure);
        }

        return CalendarEventImportSaveResult::success(returnedIds);
    }

    int callCount = 0;
    CalendarEventImportSaveRequest lastRequest;
    std::vector<Domain::CalendarEventId> returnedIds;
    std::optional<Domain::OperationError> failure;
};

class RecordingObserver final : public CalendarEventImportUseCaseObserver
{
public:
    void existingSignaturesLoaded(const std::size_t count) override
    {
        stages.push_back("query");
        existingSignatureCount = count;
    }

    void savePrepared(
        const std::size_t eventCount,
        const int skippedCount
        ) override
    {
        stages.push_back("save");
        saveEventCount = eventCount;
        this->skippedCount = skippedCount;
    }

    std::vector<std::string> stages;
    std::size_t existingSignatureCount = 0;
    std::size_t saveEventCount = 0;
    int skippedCount = 0;
};

CalendarEventImportUseCaseRequest requestWithRange()
{
    CalendarEventImportUseCaseRequest request;
    request.signatureRange = {
        .startDate = CalendarEventDate("2026-02-01"),
        .endDate = CalendarEventDate("2026-02-28")
    };
    return request;
}

CalendarEventSaveRequest saveRequest(std::string title)
{
    CalendarEventSaveRequest request;
    request.title = std::move(title);
    request.startDate = "2026-02-03";
    request.endDate = "2026-02-03";
    request.eventType = "Other";
    request.timeStatus = "Unknown";
    return request;
}

CalendarEventImportCandidate candidate(CalendarEventSaveRequest saveRequest)
{
    const auto signature = CalendarEventImportIdentity::fromSaveRequest(
        saveRequest
        );
    Q_ASSERT(signature.has_value());
    return {
        .signature = *signature,
        .saveRequest = std::move(saveRequest)
    };
}

CalendarEventImportCandidate candidate(std::string title)
{
    return candidate(saveRequest(std::move(title)));
}

Domain::CalendarEventId calendarEventId(const int value)
{
    const auto id = Domain::CalendarEventId::fromString(std::to_string(value));
    Q_ASSERT(id.has_value());
    return *id;
}

Domain::OperationError sampleError(const std::string& message)
{
    return {
        .code = Domain::ErrorCode::Technical,
        .message = message,
        .recoverable = false
    };
}

} // namespace

class NextApplicationCalendarEventImportUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void emptyInputSkipsPortsAndPreservesParserSkipCount();
    void invalidCandidateFailsBeforeCallingEitherPort();
    void candidateIdFailsBeforeCallingEitherPort();
    void forgedSignatureFailsBeforeCallingEitherPort();
    void normalizedTitleMatchesQtSimplifiedWhitespace();
    void signatureRangeMustCoverEveryCandidateDate();
    void signatureRangeMustCoverCandidateStartDate();
    void acceptedCandidatesStayOrderedAndCountsMatch();
    void duplicateOnlyInputStillSavesAnEmptyBatch();
    void duplicateHeavyInputIsFilteredBeforeApplyingSaveLimit();
    void filteredBatchAboveSaveLimitFailsBeforeSaving();
    void signatureIdentityRemainsExactUtf16();
    void signatureIncludesEveryIdentityFieldButExcludesTimes();
    void queryFailurePropagatesWithoutSaving();
    void recoverableQueryFailurePropagatesWithoutSaving();
    void saveFailurePropagatesAfterPreparation();
};

void NextApplicationCalendarEventImportUseCaseTests::
emptyInputSkipsPortsAndPreservesParserSkipCount()
{
    const CalendarEventImportUseCaseRequest request = []()
    {
        auto empty = requestWithRange();
        empty.initiallySkippedCount = 4;
        return empty;
    }();
    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(result);
    QVERIFY((
        result.value() == CalendarEventImportUseCaseResult{
            .importedCount = 0,
            .skippedCount = 4
        }
        ));
    QCOMPARE(queryPort.callCount, 0);
    QCOMPARE(savePort.callCount, 0);
}

void NextApplicationCalendarEventImportUseCaseTests::
invalidCandidateFailsBeforeCallingEitherPort()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.candidates = {
        candidate("Valid candidate"),
        candidate(" \t ")
    };
    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(queryPort.callCount, 0);
    QCOMPARE(savePort.callCount, 0);
}

void NextApplicationCalendarEventImportUseCaseTests::
candidateIdFailsBeforeCallingEitherPort()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    CalendarEventSaveRequest updateRequest = saveRequest("Existing event");
    updateRequest.id = calendarEventId(17);
    request.candidates = {candidate(std::move(updateRequest))};
    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral("Calendar imports may only create new events.")
        );
    QCOMPARE(queryPort.callCount, 0);
    QCOMPARE(savePort.callCount, 0);
}

void NextApplicationCalendarEventImportUseCaseTests::
forgedSignatureFailsBeforeCallingEitherPort()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    CalendarEventImportCandidate forged = candidate("Trusted payload");
    forged.signature = candidate("Different identity").signature;
    request.candidates = {std::move(forged)};
    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;
    RecordingObserver observer;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort,
        &observer
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(queryPort.callCount, 0);
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(observer.stages.empty());
}

void NextApplicationCalendarEventImportUseCaseTests::
normalizedTitleMatchesQtSimplifiedWhitespace()
{
    const QString sourceTitle = QStringLiteral(
        "\u00a0  Plan\t\u2003\u00a0 Review\u0085"
        );
    CalendarEventImportUseCaseRequest request = requestWithRange();
    const CalendarEventImportCandidate normalized = candidate("Plan Review");
    const CalendarEventImportCandidate source = candidate(saveRequest(
        sourceTitle.toUtf8().toStdString()
        ));
    request.candidates = {source};

    std::u16string expectedSignature =
        sourceTitle.simplified().toStdU16String();
    expectedSignature.append(u"|Other|2026-02-03|2026-02-03|0|Unknown");
    QVERIFY(source.signature.value() == expectedSignature);
    QVERIFY(source.signature == normalized.signature);

    FakeSignatureQueryPort queryPort;
    queryPort.signatures = {normalized.signature};
    FakeSavePort savePort;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(result);
    QCOMPARE(result.value().importedCount, 0);
    QCOMPARE(result.value().skippedCount, 1);
    QCOMPARE(queryPort.callCount, 1);
    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.events.empty());
}

void NextApplicationCalendarEventImportUseCaseTests::
signatureRangeMustCoverEveryCandidateDate()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    CalendarEventSaveRequest spanningRequest = saveRequest("Range boundary");
    spanningRequest.startDate = "2026-02-28";
    spanningRequest.endDate = "2026-03-01";
    request.candidates = {candidate(std::move(spanningRequest))};
    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;
    RecordingObserver observer;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort,
        &observer
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(queryPort.callCount, 0);
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(observer.stages.empty());
}

void NextApplicationCalendarEventImportUseCaseTests::
signatureRangeMustCoverCandidateStartDate()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    CalendarEventSaveRequest earlyStartRequest = saveRequest("Early start");
    earlyStartRequest.startDate = "2026-01-31";
    earlyStartRequest.endDate = "2026-02-03";
    request.candidates = {candidate(std::move(earlyStartRequest))};
    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;
    RecordingObserver observer;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort,
        &observer
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(queryPort.callCount, 0);
    QCOMPARE(savePort.callCount, 0);
    QVERIFY(observer.stages.empty());
}

void NextApplicationCalendarEventImportUseCaseTests::
acceptedCandidatesStayOrderedAndCountsMatch()
{
    const std::string multibyteTitle = repeatedHangulTitle(86);
    const CalendarEventImportCandidate existingDuplicate =
        candidate("Existing duplicate");
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.initiallySkippedCount = 4;
    request.candidates = {
        candidate(multibyteTitle),
        existingDuplicate,
        candidate("Second accepted"),
        candidate(multibyteTitle)
    };

    FakeSignatureQueryPort queryPort;
    queryPort.signatures = {
        existingDuplicate.signature,
        existingDuplicate.signature
    };
    FakeSavePort savePort;
    savePort.returnedIds = {calendarEventId(31), calendarEventId(32)};
    RecordingObserver observer;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort,
        &observer
        );

    QVERIFY(result);
    QVERIFY((
        result.value() == CalendarEventImportUseCaseResult{
            .importedCount = 2,
            .skippedCount = 6
        }
        ));
    QCOMPARE(queryPort.callCount, 1);
    QVERIFY(queryPort.lastRequest == request.signatureRange);
    QCOMPARE(savePort.callCount, 1);
    QCOMPARE(savePort.lastRequest.events.size(), std::size_t(2));
    QCOMPARE(
        savePort.lastRequest.events.at(0).title,
        multibyteTitle
        );
    QCOMPARE(savePort.lastRequest.events.at(0).title.size(), std::size_t(258));
    QCOMPARE(
        QString::fromStdString(savePort.lastRequest.events.at(1).title),
        QStringLiteral("Second accepted")
        );
    QVERIFY(observer.stages == (std::vector<std::string>{"query", "save"}));
    QCOMPARE(observer.existingSignatureCount, std::size_t(2));
    QCOMPARE(observer.saveEventCount, std::size_t(2));
    QCOMPARE(observer.skippedCount, 6);
}

void NextApplicationCalendarEventImportUseCaseTests::
duplicateOnlyInputStillSavesAnEmptyBatch()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.initiallySkippedCount = 2;
    const CalendarEventImportCandidate duplicate = candidate("Duplicate");
    request.candidates = {duplicate};

    FakeSignatureQueryPort queryPort;
    queryPort.signatures = {duplicate.signature};
    FakeSavePort savePort;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(result);
    QVERIFY((
        result.value() == CalendarEventImportUseCaseResult{
            .importedCount = 0,
            .skippedCount = 3
        }
        ));
    QCOMPARE(queryPort.callCount, 1);
    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.events.empty());
}

void NextApplicationCalendarEventImportUseCaseTests::
duplicateHeavyInputIsFilteredBeforeApplyingSaveLimit()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.candidates.reserve(kCalendarEventImportSaveMaxEvents + 1);
    for (std::size_t index = 0;
         index < kCalendarEventImportSaveMaxEvents + 1;
         ++index)
    {
        request.candidates.push_back(candidate("Repeated candidate"));
    }

    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;
    savePort.returnedIds = {calendarEventId(51)};

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(result);
    QCOMPARE(result.value().importedCount, 1);
    QCOMPARE(
        result.value().skippedCount,
        static_cast<int>(kCalendarEventImportSaveMaxEvents)
        );
    QCOMPARE(queryPort.callCount, 1);
    QCOMPARE(savePort.callCount, 1);
    QCOMPARE(savePort.lastRequest.events.size(), std::size_t(1));
}

void NextApplicationCalendarEventImportUseCaseTests::
filteredBatchAboveSaveLimitFailsBeforeSaving()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.candidates.reserve(kCalendarEventImportSaveMaxEvents + 1);
    for (std::size_t index = 0;
         index < kCalendarEventImportSaveMaxEvents + 1;
         ++index)
    {
        const std::string suffix = std::to_string(index);
        request.candidates.push_back(candidate("Candidate " + suffix));
    }

    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(queryPort.callCount, 1);
    QCOMPARE(savePort.callCount, 0);
}

void NextApplicationCalendarEventImportUseCaseTests::
signatureIdentityRemainsExactUtf16()
{
    const QString supplementaryTitle = QStringLiteral("Launch \U0001f680");
    const CalendarEventImportCandidate exactUtf16 = candidate(saveRequest(
        supplementaryTitle.toUtf8().toStdString()
        ));
    std::u16string expectedUtf16 =
        supplementaryTitle.simplified().toStdU16String();
    expectedUtf16.append(u"|Other|2026-02-03|2026-02-03|0|Unknown");
    QVERIFY(exactUtf16.signature.value() == expectedUtf16);

    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.candidates = {
        candidate("case-sensitive"),
        candidate("CASE-SENSITIVE")
    };

    FakeSignatureQueryPort queryPort;
    queryPort.signatures = {candidate("CASE-SENSITIVE").signature};
    FakeSavePort savePort;
    savePort.returnedIds = {calendarEventId(41)};

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(result);
    QCOMPARE(result.value().importedCount, 1);
    QCOMPARE(result.value().skippedCount, 1);
    QCOMPARE(savePort.lastRequest.events.size(), std::size_t(1));
    QCOMPARE(
        QString::fromStdString(savePort.lastRequest.events.at(0).title),
        QStringLiteral("case-sensitive")
        );
}

void NextApplicationCalendarEventImportUseCaseTests::
signatureIncludesEveryIdentityFieldButExcludesTimes()
{
    CalendarEventSaveRequest baseRequest = saveRequest("Identity");
    baseRequest.startDate = "2026-02-03";
    baseRequest.endDate = "2026-02-05";
    baseRequest.timeStatus = "Timed";
    baseRequest.startTime = "09:00";
    baseRequest.endTime = "10:00";
    const CalendarEventImportSignature original =
        candidate(baseRequest).signature;

    CalendarEventSaveRequest changed = baseRequest;
    changed.title = "Different identity";
    QVERIFY(candidate(changed).signature != original);

    changed = baseRequest;
    changed.eventType = "Meeting";
    QVERIFY(candidate(changed).signature != original);

    changed = baseRequest;
    changed.startDate = "2026-02-04";
    QVERIFY(candidate(changed).signature != original);

    changed = baseRequest;
    changed.endDate = "2026-02-06";
    QVERIFY(candidate(changed).signature != original);

    changed = baseRequest;
    changed.allDay = true;
    changed.startTime.reset();
    changed.endTime.reset();
    QVERIFY(candidate(changed).signature != original);

    changed = baseRequest;
    changed.timeStatus = "Unconfirmed";
    changed.startTime.reset();
    changed.endTime.reset();
    QVERIFY(candidate(changed).signature != original);

    changed = baseRequest;
    changed.startTime = "14:00";
    changed.endTime = "15:00";
    QVERIFY(candidate(changed).signature == original);
}

void NextApplicationCalendarEventImportUseCaseTests::
queryFailurePropagatesWithoutSaving()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.candidates = {candidate("Candidate")};
    FakeSignatureQueryPort queryPort;
    const Domain::OperationError expectedError =
        sampleError("signature lookup failed");
    queryPort.failure = expectedError;
    FakeSavePort savePort;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(queryPort.callCount, 1);
    QCOMPARE(savePort.callCount, 0);
}

void NextApplicationCalendarEventImportUseCaseTests::
recoverableQueryFailurePropagatesWithoutSaving()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.candidates = {candidate("Candidate")};
    FakeSignatureQueryPort queryPort;
    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::NotFound,
        .message = "signature service is unavailable",
        .recoverable = true
    };
    queryPort.failure = expectedError;
    FakeSavePort savePort;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(queryPort.callCount, 1);
    QCOMPARE(savePort.callCount, 0);
}

void NextApplicationCalendarEventImportUseCaseTests::
saveFailurePropagatesAfterPreparation()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.initiallySkippedCount = 3;
    request.candidates = {candidate("Candidate")};
    FakeSignatureQueryPort queryPort;
    FakeSavePort savePort;
    const Domain::OperationError expectedError = sampleError("batch save failed");
    savePort.failure = expectedError;
    RecordingObserver observer;

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort,
        &observer
        );

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(queryPort.callCount, 1);
    QCOMPARE(savePort.callCount, 1);
    QCOMPARE(savePort.lastRequest.events.size(), std::size_t(1));
    QVERIFY(observer.stages == (std::vector<std::string>{"query", "save"}));
    QCOMPARE(observer.saveEventCount, std::size_t(1));
    QCOMPARE(observer.skippedCount, 3);
}

QTEST_APPLESS_MAIN(NextApplicationCalendarEventImportUseCaseTests)

#include "next_application_calendar_event_import_use_case_tests.moc"
