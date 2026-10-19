#include "next/application/speaking_evaluation_save_use_case.h"

#include <QtTest/QtTest>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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
        cells[1] = u"Student ";
        cells[1].push_back(static_cast<char16_t>(u'A' + row));
        cells[2] = u"\uAE40\uBBFC\uC9C0";
        cells[3] = row % 2 == 0 ? u"A+" : u"B";
        cells[9] = row == 0 ? u"Review \U0001F4DA" : u"";
        cells[10] = row == 24 ? u"Last row" : u"";
    }
    snapshot.rows[3][1] = u"  zOE   dOE  ";
    snapshot.rows[3][10] = u"\uC218\uC5C5 \U0001F4DA";
    snapshot.changedCells = {
        {24, 10},
        {0, 1},
        {12, 4}
    };

    return {
        .classId = *typedId,
        .evaluationName = u"  Fall \U0001F4DA  ",
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

bool hasIssue(
    const Application::SpeakingEvaluationValidationResult& validation,
    const Application::SpeakingEvaluationValidationCode code,
    const std::string_view field = {},
    const int row = -1,
    const int column = -1,
    const Application::SpeakingEvaluationValidationSeverity severity =
        Application::SpeakingEvaluationValidationSeverity::Error
    )
{
    return std::ranges::any_of(
        validation.issues,
        [code, field, row, column, severity](const auto& issue)
        {
            return issue.code == code
                && (field.empty() || issue.field == field)
                && (row < 0 || issue.row == row)
                && (column < 0 || issue.column == column)
                && issue.severity == severity;
        }
        );
}

void clearRows(Application::SpeakingEvaluationSaveRequest& value)
{
    for (std::vector<std::u16string>& row : value.evaluation.rows)
    {
        std::fill(row.begin(), row.end(), std::u16string{});
    }
}

void setStudent(
    Application::SpeakingEvaluationSaveRequest& value,
    const int row,
    std::u16string english,
    std::u16string korean
    )
{
    value.evaluation.rows[static_cast<std::size_t>(row)][1] = std::move(english);
    value.evaluation.rows[static_cast<std::size_t>(row)][2] = std::move(korean);
}

}

class NextApplicationSpeakingEvaluationSaveUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidClassIdsWithoutCallingPort();
    void rejectsNoncanonicalLeadingZeroClassIdDirectly();
    void acceptsMaximumClassIdAndForwardsUnchanged();
    void rejectsIncompleteMatrixAndInvalidCoordinates();
    void rejectsOversizedMatrixRowsAndCellsBeforeCallingPort();
    void normalizesValidRequestBeforePortAndPreservesUnicodeAndCellOrder();
    void rejectsInvalidContentBeforeCallingPort();
    void groupsDuplicateNamesAfterUnicodeTrimmingInFirstSeenOrder();
    void preservesQuestionableKoreanLengthWarningAndAllowSemantics();
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
        QCOMPARE(
            result.error().message,
            std::string("Class ID must be a canonical positive integer.")
            );
        QVERIFY(result.error().recoverable);
        QCOMPARE(port.callCount, 0);
    }

    Application::SpeakingEvaluationSaveRequest invalidIdAndMatrix =
        request("42x");
    invalidIdAndMatrix.evaluation.rows.pop_back();
    const auto precedenceResult =
        Application::SpeakingEvaluationSaveUseCase::execute(
            invalidIdAndMatrix,
            port
            );
    QVERIFY(!precedenceResult);
    QCOMPARE(
        precedenceResult.error().code,
        Domain::ErrorCode::InvalidInput
        );
    QCOMPARE(
        precedenceResult.error().message,
        std::string("Class ID must be a canonical positive integer.")
        );
    QVERIFY(precedenceResult.error().recoverable);
    QCOMPARE(port.callCount, 0);

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
rejectsNoncanonicalLeadingZeroClassIdDirectly()
{
    const auto validation = Application::validateAndNormalizeSpeakingEvaluation(
        request("01")
        );

    QVERIFY(validation.hasErrors());
    QCOMPARE(validation.issues.size(), std::size_t(1));

    const Application::SpeakingEvaluationValidationIssue& issue =
        validation.issues.front();
    QCOMPARE(
        issue.code,
        Application::SpeakingEvaluationValidationCode::SpeakingEvaluationClassIdInvalid
        );
    QCOMPARE(issue.field, std::string("classId"));
    QCOMPARE(issue.row, -1);
    QCOMPARE(issue.column, -1);
    QCOMPARE(
        issue.severity,
        Application::SpeakingEvaluationValidationSeverity::Error
        );
    QCOMPARE(issue.arguments.size(), std::size_t(1));
    QVERIFY(issue.arguments.contains("value"));
    const auto* parsedClassId = std::get_if<std::int64_t>(
        &issue.arguments.at("value")
        );
    QVERIFY(parsedClassId != nullptr);
    QCOMPARE(*parsedClassId, std::int64_t(1));
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
acceptsMaximumClassIdAndForwardsUnchanged()
{
    RecordingSpeakingEvaluationSavePort port;

    const auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        request("2147483647"),
        port
        );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId.value(), std::string("2147483647"));
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
rejectsOversizedMatrixRowsAndCellsBeforeCallingPort()
{
    RecordingSpeakingEvaluationSavePort port;

    Application::SpeakingEvaluationSaveRequest tooManyRows = request("42");
    tooManyRows.evaluation.rows.emplace_back(
        Application::SpeakingEvaluationColumnCount
        );
    auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        tooManyRows,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    const auto rowsValidation =
        Application::validateAndNormalizeSpeakingEvaluation(tooManyRows);
    QVERIFY(hasIssue(
        rowsValidation,
        Application::SpeakingEvaluationValidationCode::SpeakingEvaluationRowsTooMany,
        "rows"
        ));
    QCOMPARE(port.callCount, 0);

    Application::SpeakingEvaluationSaveRequest tooManyCells = request("42");
    tooManyCells.evaluation.rows[6].push_back(u"unexpected");
    result = Application::SpeakingEvaluationSaveUseCase::execute(
        tooManyCells,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    const auto cellsValidation =
        Application::validateAndNormalizeSpeakingEvaluation(tooManyCells);
    QVERIFY(hasIssue(
        cellsValidation,
        Application::SpeakingEvaluationValidationCode::SpeakingEvaluationRowTooManyCells,
        "rows[6]",
        6
        ));
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
normalizesValidRequestBeforePortAndPreservesUnicodeAndCellOrder()
{
    RecordingSpeakingEvaluationSavePort port;
    Application::SpeakingEvaluationSaveRequest value = request("42");
    value.evaluation.rows[0][1] = u"  aLIce   sMITH  ";
    value.evaluation.rows[0][2] = u" \uAE40 \uBBFC \uC9C0 ";
    value.evaluation.rows[0][3] = u" 1 ";
    value.evaluation.rows[0][4] = u" 2 ";
    value.evaluation.rows[0][5] = u" 3 ";
    value.evaluation.rows[0][6] = u" 4 ";
    value.evaluation.rows[0][7] = u" 5 ";
    value.evaluation.rows[0][8] = u" \u314A ";
    value.evaluation.rows[1][3] = u" \u3160 ";
    value.evaluation.rows[2][3] = u" \u3160 + ";
    value.evaluation.rows[3][3] = u" \u3141 ";
    value.evaluation.rows[4][3] = u" \u3141 + ";
    value.evaluation.rows[0][9] = u" Review \U0001F4DA ";
    value.evaluation.rows[3][10] = u"\uC218\uC5C5 \U0001F4DA";

    const auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        value,
        port
        );
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId.value(), std::string("42"));
    QCOMPARE(port.lastRequest->evaluationName, std::u16string(u"Fall \U0001F4DA"));
    QCOMPARE(port.lastRequest->evaluation.rows.size(), std::size_t(25));
    QCOMPARE(port.lastRequest->evaluation.rows[0].size(), std::size_t(11));
    QCOMPARE(port.lastRequest->evaluation.rows[0][1], std::u16string(u"Alice Smith"));
    QCOMPARE(port.lastRequest->evaluation.rows[0][2],
        std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(port.lastRequest->evaluation.rows[0][3], std::u16string(u"C"));
    QCOMPARE(port.lastRequest->evaluation.rows[0][4], std::u16string(u"B"));
    QCOMPARE(port.lastRequest->evaluation.rows[0][5], std::u16string(u"B+"));
    QCOMPARE(port.lastRequest->evaluation.rows[0][6], std::u16string(u"A"));
    QCOMPARE(port.lastRequest->evaluation.rows[0][7], std::u16string(u"A+"));
    QCOMPARE(port.lastRequest->evaluation.rows[0][8], std::u16string(u"C"));
    QCOMPARE(port.lastRequest->evaluation.rows[1][3], std::u16string(u"B"));
    QCOMPARE(port.lastRequest->evaluation.rows[2][3], std::u16string(u"B+"));
    QCOMPARE(port.lastRequest->evaluation.rows[3][3], std::u16string(u"A"));
    QCOMPARE(port.lastRequest->evaluation.rows[4][3], std::u16string(u"A+"));
    QCOMPARE(port.lastRequest->evaluation.rows[0][9],
        std::u16string(u" Review \U0001F4DA "));
    QCOMPARE(port.lastRequest->evaluation.rows[3][1], std::u16string(u"Zoe Doe"));
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
rejectsInvalidContentBeforeCallingPort()
{
    RecordingSpeakingEvaluationSavePort port;
    auto value = request("42");
    clearRows(value);
    value.evaluationName = std::u16string(129, u'x');

    auto& rows = value.evaluation.rows;
    rows[0][3] = u"A"; // Editable data without either name.
    setStudent(value, 1, u" Zo\u00EB O'Neil ", u"\uAE40\uBBFC1");
    setStudent(value, 2, u"Same", u"\uAE40\uBBFC\uC9C0");
    setStudent(value, 3, u"Same", u"\uAE40\uBBFC\uC9C0");
    setStudent(value, 4, u"Score", u"\uAE40\uBBFC\uC9C0");
    rows[4][3] = u"D";
    setStudent(value, 5, u"Comment", u"\uAE40\uBBFC\uC9C0");
    rows[5][9] = std::u16string(451, u'c');
    setStudent(value, 6, u"Notes", u"\uAE40\uBBFC\uC9C0");
    rows[6][10] = std::u16string(10001, u'n');
    setStudent(value, 7, std::u16string(21, u'e'), u"\uAE40\uBBFC\uC9C0");
    setStudent(value, 8, u"Korean", u"\uAC00\uB0981");

    const auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        value,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        result.error().message,
        std::string("Speaking evaluation content is invalid.")
        );
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());

    const auto validation = Application::validateAndNormalizeSpeakingEvaluation(
        value
        );
    QVERIFY(validation.hasErrors());
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::ValidationLengthOutOfBounds,
        "evaluationName"
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::SpeakingEvaluationStudentNameRequired,
        "rows[0].English Name",
        0,
        1
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::SpeakingEvaluationStudentNameRequired,
        "rows[0].Korean Name",
        0,
        2
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::StudentNameEnglishNonAscii,
        "rows[1].English Name",
        1,
        1
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::StudentNameEnglishInvalidCharacters,
        "rows[1].English Name",
        1,
        1
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::StudentNameKoreanInvalidCharacters,
        "rows[1].Korean Name",
        1,
        2
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::StudentNameDuplicatePair,
        "English Name",
        2,
        1
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::ValidationEnumInvalidValue,
        "rows[4].Grammar",
        4,
        3
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::ValidationLengthOutOfBounds,
        "rows[5].Comments",
        5,
        9
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::ValidationLengthOutOfBounds,
        "rows[6].Notes",
        6,
        10
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::StudentNameEnglishTooLong,
        "rows[7].English Name",
        7,
        1
        ));
    QVERIFY(hasIssue(
        validation,
        Application::SpeakingEvaluationValidationCode::StudentNameKoreanInvalidCharacters,
        "rows[8].Korean Name",
        8,
        2
        ));

    auto blankRows = request("42");
    clearRows(blankRows);
    blankRows.evaluationName = std::u16string(128, u'n');
    setStudent(blankRows, 0, std::u16string(20, u'e'), u"\uAE40\uBBFC\uC9C0");
    blankRows.evaluation.rows[0][9] = std::u16string(450, u'c');
    blankRows.evaluation.rows[0][10] = std::u16string(10000, u'n');
    const auto blankValidation =
        Application::validateAndNormalizeSpeakingEvaluation(blankRows);
    QVERIFY(!blankValidation.hasErrors());
    QVERIFY(!blankValidation.hasWarnings());

    blankRows.evaluationName = u" \t ";
    const auto emptyNameValidation =
        Application::validateAndNormalizeSpeakingEvaluation(blankRows);
    QVERIFY(hasIssue(
        emptyNameValidation,
        Application::SpeakingEvaluationValidationCode::ValidationLengthOutOfBounds,
        "evaluationName"
        ));
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
groupsDuplicateNamesAfterUnicodeTrimmingInFirstSeenOrder()
{
    RecordingSpeakingEvaluationSavePort port;
    auto value = request("42");
    clearRows(value);
    value.evaluationName = u"Fall";

    setStudent(
        value,
        0,
        u"\u3000Alice\u3000",
        u"\u3000\uAE40\uBBFC\uC9C0\u3000"
        );
    setStudent(value, 1, u"Bob", u"\uBC15\uC9C0\uBBFC");
    setStudent(
        value,
        2,
        u"\u2003Alice\u2003",
        u"\u2003\uAE40\uBBFC\uC9C0\u2003"
        );
    setStudent(value, 3, u" Bob ", u" \uBC15\uC9C0\uBBFC ");
    setStudent(value, 4, u" Alice ", u"");
    setStudent(value, 5, u"Bob", u"\uBC15\uC9C0\uBBFC");
    setStudent(value, 6, u"Alice", u"\uAE40\uBBFC\uC9C0");
    setStudent(value, 7, u"", u" \uBC15\uC9C0\uBBFC ");

    const auto saveResult = Application::SpeakingEvaluationSaveUseCase::execute(
        value,
        port
        );
    QVERIFY(!saveResult);
    QCOMPARE(saveResult.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        saveResult.error().message,
        std::string("Speaking evaluation content is invalid.")
        );
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());

    const auto validation = Application::validateAndNormalizeSpeakingEvaluation(
        value
        );
    QVERIFY(validation.hasErrors());
    QCOMPARE(
        validation.normalizedRequest.evaluation.rows[0][1],
        std::u16string(u"Alice")
        );
    QCOMPARE(
        validation.normalizedRequest.evaluation.rows[0][2],
        std::u16string(u"\uAE40\uBBFC\uC9C0")
        );
    QCOMPARE(
        validation.normalizedRequest.evaluation.rows[2][1],
        std::u16string(u"Alice")
        );
    QCOMPARE(
        validation.normalizedRequest.evaluation.rows[2][2],
        std::u16string(u"\uAE40\uBBFC\uC9C0")
        );

    struct ExpectedDuplicateIssue final
    {
        int row;
        int column;
        std::vector<int> duplicateRows;
    };
    const std::vector<ExpectedDuplicateIssue> expectedDuplicateIssues{
        {0, 1, {0, 2, 6}},
        {0, 2, {0, 2, 6}},
        {2, 1, {0, 2, 6}},
        {2, 2, {0, 2, 6}},
        {6, 1, {0, 2, 6}},
        {6, 2, {0, 2, 6}},
        {1, 1, {1, 3, 5}},
        {1, 2, {1, 3, 5}},
        {3, 1, {1, 3, 5}},
        {3, 2, {1, 3, 5}},
        {5, 1, {1, 3, 5}},
        {5, 2, {1, 3, 5}}
    };
    QCOMPARE(validation.issues.size(), expectedDuplicateIssues.size() + 2);

    const auto& incompleteEnglish = validation.issues[0];
    QCOMPARE(
        incompleteEnglish.code,
        Application::SpeakingEvaluationValidationCode::SpeakingEvaluationStudentNameRequired
        );
    QCOMPARE(incompleteEnglish.field, std::string("rows[4].Korean Name"));
    QCOMPARE(incompleteEnglish.row, 4);
    QCOMPARE(incompleteEnglish.column, 2);
    QCOMPARE(
        incompleteEnglish.severity,
        Application::SpeakingEvaluationValidationSeverity::Error
        );
    QVERIFY(!incompleteEnglish.arguments.contains("duplicateRows"));

    const auto& incompleteKorean = validation.issues[1];
    QCOMPARE(
        incompleteKorean.code,
        Application::SpeakingEvaluationValidationCode::SpeakingEvaluationStudentNameRequired
        );
    QCOMPARE(incompleteKorean.field, std::string("rows[7].English Name"));
    QCOMPARE(incompleteKorean.row, 7);
    QCOMPARE(incompleteKorean.column, 1);
    QCOMPARE(
        incompleteKorean.severity,
        Application::SpeakingEvaluationValidationSeverity::Error
        );
    QVERIFY(!incompleteKorean.arguments.contains("duplicateRows"));

    for (std::size_t index = 0; index < expectedDuplicateIssues.size(); ++index)
    {
        const auto& issue = validation.issues[index + 2];
        const auto& expected = expectedDuplicateIssues[index];
        const std::string expectedField = expected.column == 1
            ? "English Name"
            : "Korean Name";

        QCOMPARE(
            issue.code,
            Application::SpeakingEvaluationValidationCode::StudentNameDuplicatePair
            );
        QCOMPARE(issue.field, expectedField);
        QCOMPARE(issue.row, expected.row);
        QCOMPARE(issue.column, expected.column);
        QCOMPARE(
            issue.severity,
            Application::SpeakingEvaluationValidationSeverity::Error
            );
        QCOMPARE(issue.arguments.size(), std::size_t(1));
        QVERIFY(issue.arguments.contains("duplicateRows"));
        const auto* duplicateRows = std::get_if<std::vector<int>>(
            &issue.arguments.at("duplicateRows")
            );
        QVERIFY(duplicateRows != nullptr);
        QCOMPARE(duplicateRows->size(), expected.duplicateRows.size());
        for (std::size_t rowIndex = 0;
             rowIndex < expected.duplicateRows.size();
             ++rowIndex)
        {
            QCOMPARE((*duplicateRows)[rowIndex], expected.duplicateRows[rowIndex]);
        }
    }
}

void NextApplicationSpeakingEvaluationSaveUseCaseTests::
preservesQuestionableKoreanLengthWarningAndAllowSemantics()
{
    auto value = request("42");
    clearRows(value);
    value.allowQuestionableKoreanNameLengths = false;
    setStudent(value, 0, u"Short", u"\uAE40");
    setStudent(value, 1, u"Long", u"\uAC00\uB098\uB2E4\uB77C\uB9C8");
    setStudent(value, 2, u"Unusual", u"\uAC00\uB098");

    const auto strict = Application::validateAndNormalizeSpeakingEvaluation(value);
    QVERIFY(strict.hasErrors());
    QVERIFY(strict.hasWarnings());
    QVERIFY(hasIssue(
        strict,
        Application::SpeakingEvaluationValidationCode::StudentNameKoreanTooShort,
        "rows[0].Korean Name",
        0,
        2
        ));
    QVERIFY(hasIssue(
        strict,
        Application::SpeakingEvaluationValidationCode::StudentNameKoreanTooLong,
        "rows[1].Korean Name",
        1,
        2
        ));
    QVERIFY(hasIssue(
        strict,
        Application::SpeakingEvaluationValidationCode::StudentNameKoreanUnusualLength,
        "rows[2].Korean Name",
        2,
        2,
        Application::SpeakingEvaluationValidationSeverity::Warning
        ));

    value.allowQuestionableKoreanNameLengths = true;
    const auto allowed = Application::validateAndNormalizeSpeakingEvaluation(value);
    QVERIFY(!allowed.hasErrors());
    QVERIFY(allowed.hasWarnings());
    QVERIFY(hasIssue(
        allowed,
        Application::SpeakingEvaluationValidationCode::StudentNameKoreanTooShort,
        "rows[0].Korean Name",
        0,
        2,
        Application::SpeakingEvaluationValidationSeverity::Warning
        ));
    QVERIFY(hasIssue(
        allowed,
        Application::SpeakingEvaluationValidationCode::StudentNameKoreanTooLong,
        "rows[1].Korean Name",
        1,
        2,
        Application::SpeakingEvaluationValidationSeverity::Warning
        ));
    QVERIFY(hasIssue(
        allowed,
        Application::SpeakingEvaluationValidationCode::StudentNameKoreanUnusualLength,
        "rows[2].Korean Name",
        2,
        2,
        Application::SpeakingEvaluationValidationSeverity::Warning
        ));

    RecordingSpeakingEvaluationSavePort port;
    const auto saved = Application::SpeakingEvaluationSaveUseCase::execute(
        value,
        port
        );
    QVERIFY(saved);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->evaluation.rows[0][2], std::u16string(u"\uAE40"));
    QCOMPARE(port.lastRequest->evaluation.rows[1][2],
        std::u16string(u"\uAC00\uB098\uB2E4\uB77C\uB9C8"));
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
