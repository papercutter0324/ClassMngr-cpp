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
    std::vector<std::u16string> signatures;
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

CalendarEventImportCandidate candidate(
    std::u16string signature,
    std::string title
    )
{
    return {
        .signature = std::move(signature),
        .saveRequest = saveRequest(std::move(title))
    };
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
    void acceptedCandidatesStayOrderedAndCountsMatch();
    void duplicateOnlyInputStillSavesAnEmptyBatch();
    void signatureIdentityRemainsExactUtf16();
    void queryFailurePropagatesWithoutSaving();
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
acceptedCandidatesStayOrderedAndCountsMatch()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.initiallySkippedCount = 4;
    request.candidates = {
        candidate(u"new-a", "First accepted"),
        candidate(u"existing", "Existing duplicate"),
        candidate(u"new-b", "Second accepted"),
        candidate(u"new-a", "In-batch duplicate")
    };

    FakeSignatureQueryPort queryPort;
    queryPort.signatures = {u"existing", u"existing"};
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
        QString::fromStdString(savePort.lastRequest.events.at(0).title),
        QStringLiteral("First accepted")
        );
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
    request.candidates = {candidate(u"existing", "Duplicate")};

    FakeSignatureQueryPort queryPort;
    queryPort.signatures = {u"existing"};
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
signatureIdentityRemainsExactUtf16()
{
    const std::u16string loneSurrogate(1, static_cast<char16_t>(0xD800));
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.candidates = {
        candidate(u"case-sensitive", "Different case"),
        candidate(u"CASE-SENSITIVE", "Exact match"),
        candidate(loneSurrogate, "Unpaired surrogate")
    };

    FakeSignatureQueryPort queryPort;
    queryPort.signatures = {u"CASE-SENSITIVE"};
    FakeSavePort savePort;
    savePort.returnedIds = {calendarEventId(41), calendarEventId(42)};

    const auto result = CalendarEventImportUseCase::execute(
        request,
        queryPort,
        savePort
        );

    QVERIFY(result);
    QCOMPARE(result.value().importedCount, 2);
    QCOMPARE(result.value().skippedCount, 1);
    QCOMPARE(savePort.lastRequest.events.size(), std::size_t(2));
    QCOMPARE(
        QString::fromStdString(savePort.lastRequest.events.at(0).title),
        QStringLiteral("Different case")
        );
    QCOMPARE(
        QString::fromStdString(savePort.lastRequest.events.at(1).title),
        QStringLiteral("Unpaired surrogate")
        );
}

void NextApplicationCalendarEventImportUseCaseTests::
queryFailurePropagatesWithoutSaving()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.candidates = {candidate(u"candidate", "Candidate")};
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
saveFailurePropagatesAfterPreparation()
{
    CalendarEventImportUseCaseRequest request = requestWithRange();
    request.initiallySkippedCount = 3;
    request.candidates = {candidate(u"candidate", "Candidate")};
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
