#include "next/application/speaking_evaluation_roster_names_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Application::SpeakingEvaluationRosterNamesReadRequest request(
    std::string classId
    )
{
    const auto typedId = Domain::ClassId::fromString(std::move(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {.classId = *typedId};
}

class RecordingRosterNamesPort final
    : public Application::SpeakingEvaluationRosterNamesReadPort
{
public:
    [[nodiscard]] Application::SpeakingEvaluationRosterNamesReadResult
    readSpeakingEvaluationRosterNames(
        const Application::SpeakingEvaluationRosterNamesReadRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<
        Application::SpeakingEvaluationRosterNamesReadRequest> lastRequest;
    Application::SpeakingEvaluationRosterNamesReadResult result =
        Application::SpeakingEvaluationRosterNamesReadResult::success(
            Application::SpeakingEvaluationRosterNamesReadSnapshot(
                *Domain::ClassId::fromString("42")
                )
            );
};

}

class NextApplicationSpeakingEvaluationRosterNamesReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void invalidClassIdsDoNotReachPort();
    void preservesRawOrderedPairsAndColumnPresence();
    void preservesSuccessfulEmptyProjection();
    void rejectsMismatchedReturnedClass();
    void preservesPortError();
};

void NextApplicationSpeakingEvaluationRosterNamesReadQueryTests::
invalidClassIdsDoNotReachPort()
{
    RecordingRosterNamesPort port;
    for (const std::string classId : {
             "0", "-2", "+42", "class-42", " 42", "42 ", "01",
             "00042", "999999999999999999999"
         })
    {
        const auto result = Application::
            SpeakingEvaluationRosterNamesReadQuery::execute(
                request(classId),
                port
                );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationSpeakingEvaluationRosterNamesReadQueryTests::
preservesRawOrderedPairsAndColumnPresence()
{
    RecordingRosterNamesPort port;
    const auto requested = request("42");
    Application::SpeakingEvaluationRosterNamesReadSnapshot snapshot(
        requested.classId
        );
    snapshot.hasEnglishColumn = true;
    snapshot.hasKoreanColumn = false;
    snapshot.rows = {
        {u"  Alice  ", u" 민지 "},
        {u"Second", u""}
    };
    port.result = Application::
        SpeakingEvaluationRosterNamesReadResult::success(snapshot);

    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(requested, port);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.value().classId == requested.classId);
    QVERIFY(result.value().classId == requested.classId);
    QVERIFY(result.value().hasEnglishColumn);
    QVERIFY(!result.value().hasKoreanColumn);
    QCOMPARE(result.value().rows.size(), std::size_t(2));
    QCOMPARE(result.value().rows[0].englishName, std::u16string(u"  Alice  "));
    QCOMPARE(result.value().rows[0].koreanName, std::u16string(u" 민지 "));
    QCOMPARE(result.value().rows[1].englishName, std::u16string(u"Second"));
    QVERIFY(result.value().rows[1].koreanName.empty());
}

void NextApplicationSpeakingEvaluationRosterNamesReadQueryTests::
preservesSuccessfulEmptyProjection()
{
    RecordingRosterNamesPort port;
    const auto requested = request("42");
    port.result = Application::
        SpeakingEvaluationRosterNamesReadResult::success(
            Application::SpeakingEvaluationRosterNamesReadSnapshot(
                requested.classId
                )
            );

    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(requested, port);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value().rows.empty());
    QVERIFY(!result.value().hasEnglishColumn);
    QVERIFY(!result.value().hasKoreanColumn);
}

void NextApplicationSpeakingEvaluationRosterNamesReadQueryTests::
rejectsMismatchedReturnedClass()
{
    RecordingRosterNamesPort port;
    const auto requested = request("42");
    port.result = Application::
        SpeakingEvaluationRosterNamesReadResult::success(
            Application::SpeakingEvaluationRosterNamesReadSnapshot(
                *Domain::ClassId::fromString("43")
                )
            );

    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(requested, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(port.callCount, 1);
}

void NextApplicationSpeakingEvaluationRosterNamesReadQueryTests::
preservesPortError()
{
    RecordingRosterNamesPort port;
    const Domain::OperationError error{
        .code = Domain::ErrorCode::Technical,
        .message = "roster names read failed",
        .recoverable = true
    };
    port.result = Application::SpeakingEvaluationRosterNamesReadResult::failure(
        error);

    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(
            request("42"),
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error(), error);
    QCOMPARE(port.callCount, 1);
}

QTEST_GUILESS_MAIN(
    NextApplicationSpeakingEvaluationRosterNamesReadQueryTests
    )

#include "next_application_speaking_evaluation_roster_names_read_query_tests.moc"
