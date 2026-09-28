#include "next/application/speaking_evaluation_save_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Application::SpeakingEvaluationSaveRequest request(std::string classId)
{
    const auto typedId = Domain::ClassId::fromString(std::move(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    Application::SpeakingEvaluationSnapshot snapshot;
    snapshot.rows.resize(Application::SpeakingEvaluationRowCount);
    for (int row = 0; row < Application::SpeakingEvaluationRowCount; ++row)
    {
        std::vector<std::u16string>& cells =
            snapshot.rows[static_cast<std::size_t>(row)];
        cells.resize(Application::SpeakingEvaluationColumnCount);
        cells[1] = u"Student";
        cells[2] = u"\uAE40\uBBFC\uC9C0";
        cells[3] = row % 2 == 0 ? u"A+" : u"B";
        cells[9] = row == 0 ? u"Review \U0001F4DA" : u"";
        cells[10] = row == 24 ? u"Last row" : u"";
    }
    snapshot.rows[3][1] = u"Zo\u00EB";
    snapshot.rows[3][10] = u"\uC218\uC5C5 \U0001F4DA";
    snapshot.changedCells = {
        {24, 10},
        {0, 1},
        {12, 4}
    };

    return {
        .classId = *typedId,
        .evaluationName = u"Fall \U0001F4DA",
        .evaluation = std::move(snapshot),
        .allowQuestionableKoreanNameLengths = true
    };
}

class RecordingSpeakingEvaluationSavePort final
    : public Application::SpeakingEvaluationSavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveEvaluation(
        const Application::SpeakingEvaluationSaveRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::SpeakingEvaluationSaveRequest> lastRequest;
    Domain::Result<void> result = Domain::Result<void>::success();
};

}

class NextApplicationSpeakingEvaluationSaveUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidClassIdsWithoutCallingPort();
    void rejectsIncompleteMatrixAndInvalidCoordinates();
    void preservesMatrixUnicodeCoordinateOrderAndQuestionableNameFlag();
    void preservesEmptyDeltaAsAllCellsRequest();
    void preservesPortFailure();
};

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
rejectsInvalidClassIdsWithoutCallingPort()
{
    RecordingSpeakingEvaluationSavePort port;
    for (const std::string classId : {
             "0",
             "-3",
             "+42",
             "class-42",
             " 42",
             "42 ",
             "01",
             "999999999999999999999"
         })
    {
        const auto result = Application::SpeakingEvaluationSaveUseCase::execute(
            request(classId),
            port
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
rejectsIncompleteMatrixAndInvalidCoordinates()
{
    RecordingSpeakingEvaluationSavePort port;

    Application::SpeakingEvaluationSaveRequest incomplete = request("42");
    incomplete.evaluation.rows.pop_back();
    auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        incomplete,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    Application::SpeakingEvaluationSaveRequest shortRow = request("42");
    shortRow.evaluation.rows[8].pop_back();
    result = Application::SpeakingEvaluationSaveUseCase::execute(shortRow, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    Application::SpeakingEvaluationSaveRequest invalidCell = request("42");
    invalidCell.evaluation.changedCells.push_back({25, 0});
    result = Application::SpeakingEvaluationSaveUseCase::execute(
        invalidCell,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(port.callCount, 0);
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
preservesMatrixUnicodeCoordinateOrderAndQuestionableNameFlag()
{
    RecordingSpeakingEvaluationSavePort port;
    const Application::SpeakingEvaluationSaveRequest value = request("42");

    const auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        value,
        port
        );
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(port.lastRequest.value() == value);
    QCOMPARE(port.lastRequest->classId.value(), std::string("42"));
    QCOMPARE(port.lastRequest->evaluationName, std::u16string(u"Fall \U0001F4DA"));
    QCOMPARE(port.lastRequest->evaluation.rows.size(), std::size_t(25));
    QCOMPARE(port.lastRequest->evaluation.rows[0].size(), std::size_t(11));
    QCOMPARE(port.lastRequest->evaluation.rows[0][2],
        std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(port.lastRequest->evaluation.rows[3][1], std::u16string(u"Zo\u00EB"));
    QCOMPARE(port.lastRequest->evaluation.rows[3][10],
        std::u16string(u"\uC218\uC5C5 \U0001F4DA"));
    QCOMPARE(port.lastRequest->evaluation.rows[24][10],
        std::u16string(u"Last row"));
    const auto& changedCells = port.lastRequest->evaluation.changedCells;
    QCOMPARE(changedCells.size(), std::size_t(3));
    QCOMPARE(changedCells[0].row, 24);
    QCOMPARE(changedCells[0].column, 10);
    QCOMPARE(changedCells[1].row, 0);
    QCOMPARE(changedCells[1].column, 1);
    QCOMPARE(changedCells[2].row, 12);
    QCOMPARE(changedCells[2].column, 4);
    QVERIFY(port.lastRequest->allowQuestionableKoreanNameLengths);
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
preservesEmptyDeltaAsAllCellsRequest()
{
    RecordingSpeakingEvaluationSavePort port;
    Application::SpeakingEvaluationSaveRequest value = request("42");
    value.evaluation.changedCells.clear();

    const auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        value,
        port
        );
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(port.lastRequest->evaluation.changedCells.empty());
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::preservesPortFailure()
{
    RecordingSpeakingEvaluationSavePort port;
    port.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "save rejected",
        .recoverable = true
    });

    const auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        request("42"),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("save rejected"));
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationSpeakingEvaluationSaveUseCaseTests)

#include "next_application_speaking_evaluation_save_use_case_tests.moc"
