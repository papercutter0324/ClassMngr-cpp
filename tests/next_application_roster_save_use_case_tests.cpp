#include "next/application/roster_save_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{
bool equals(std::u16string_view left, std::u16string_view right)
{
    return QStringView(left.data(), left.size()).compare(
        QStringView(right.data(), right.size()), Qt::CaseInsensitive) == 0;
}

Application::RosterSaveRequest request(std::string classId)
{
    const auto typedId = Domain::ClassId::fromString(std::move(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    Application::RosterSnapshot snapshot;
    snapshot.columns = {u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall", u"\u5099\u8003"};
    snapshot.columnWidths = {171, 121, 130, 209};
    snapshot.rows.resize(25);
    snapshot.rows[0] = {u"Alice", u"김민지", u"A+", u"수업 \U0001F4DA"};
    snapshot.rows[3] = {u"Zoe", u"박지훈", u"B", u"점검"};
    snapshot.rows[24] = {u"Final Row", u"최민서", u"C", u"끝"};

    return {
        .classId = *typedId,
        .roster = std::move(snapshot),
        .allowQuestionableKoreanNameLengths = true
    };
}

class RecordingRosterSavePort final : public Application::RosterSavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveRoster(
        const Application::PreparedRosterSaveRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::PreparedRosterSaveRequest> lastRequest;
    Domain::Result<void> result = Domain::Result<void>::success();
};

}

class NextApplicationRosterSaveUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void invalidClassIdsDoNotReachPort();
    void preservesCompleteSnapshotAndAllowFlag();
    void preservesPortFailure();
    void invalidRosterDoesNotReachPortAndIdTakesPriority();
    void preparesNormalizedSnapshotBeforePort();
    void directPreparationPreservesStructureAndIssueOrder();
    void rawBomConversionDoesNotMutateRequestOrAddSentinels();
    void policyAcceptsBoundarySizesAndCountsUtf16Units();
};

void NextApplicationRosterSaveUseCaseTests::invalidClassIdsDoNotReachPort()
{
    RecordingRosterSavePort port;
    for (const std::string classId : {
             "0",
             "-2",
             "+42",
             "class-42",
             " 42",
             "42 ",
             "01",
             "00042",
             "999999999999999999999"
         })
    {
        const auto result = Application::RosterSaveUseCase::execute(
            request(classId),
            port, equals
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationRosterSaveUseCaseTests::preservesCompleteSnapshotAndAllowFlag()
{
    RecordingRosterSavePort port;
    const Application::RosterSaveRequest value = request("42");

    const auto result = Application::RosterSaveUseCase::execute(value, port, equals);
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY((port.lastRequest.value() == Application::PreparedRosterSaveRequest{
        .classId = value.classId, .roster = value.roster,
        .allowQuestionableKoreanNameLengths = value.allowQuestionableKoreanNameLengths}));
    QCOMPARE(port.lastRequest->classId.value(), std::string("42"));
    QCOMPARE(port.lastRequest->roster.columns.size(), std::size_t(7));
    QCOMPARE(port.lastRequest->roster.columnWidths,
        std::vector<int>({171, 121, 130, 209}));
    QCOMPARE(port.lastRequest->roster.rows.size(), std::size_t(25));
    QVERIFY(port.lastRequest->roster.rows[1].empty());
    QCOMPARE(port.lastRequest->roster.rows[24][0], std::u16string(u"Final Row"));
    QCOMPARE(port.lastRequest->allowQuestionableKoreanNameLengths, true);
}

void NextApplicationRosterSaveUseCaseTests::preservesPortFailure()
{
    RecordingRosterSavePort port;
    port.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "save rejected",
        .recoverable = true
    });

    const auto result = Application::RosterSaveUseCase::execute(
        request("42"),
        port, equals
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("save rejected"));
    QCOMPARE(port.callCount, 1);
}


void NextApplicationRosterSaveUseCaseTests::invalidRosterDoesNotReachPortAndIdTakesPriority()
{
    RecordingRosterSavePort port;
    auto value = request("42");
    value.roster.rows[0][1] = u"";
    value.roster.rows[0][0] = u"Alice2";
    const auto rejected = Application::RosterSaveUseCase::execute(value, port, equals);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!rejected.error().recoverable);
    QCOMPARE(rejected.error().message, std::string(
        "Roster validation failed: rows[0].Korean: roster.student_name.required; "
        "rows[0].English: student_name.english.invalid_characters"));
    QCOMPARE(port.callCount, 0);
    value.classId = *Domain::ClassId::fromString("01");
    bool compared = false;
    const auto badId = Application::RosterSaveUseCase::execute(value, port,
        [&](auto, auto) { compared = true; return false; });
    QVERIFY(!badId);
    QCOMPARE(badId.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(badId.error().recoverable);
    QVERIFY(!compared);
    QCOMPARE(port.callCount, 0);
}

void NextApplicationRosterSaveUseCaseTests::preparesNormalizedSnapshotBeforePort()
{
    RecordingRosterSavePort port;
    auto value = request("42");
    value.roster.columns[0] = u" english ";
    value.roster.columns[5] = u" AUTUMN ";
    value.roster.rows[0][0] = u"  aLICE  ";
    value.roster.rows[0][1] = u"\uAE40 \uBBFC \uC9C0 (b)";
    const auto result = Application::RosterSaveUseCase::execute(value, port, equals);
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QCOMPARE(port.lastRequest->roster.columns[0], std::u16string(u"English"));
    QCOMPARE(port.lastRequest->roster.columns[5], std::u16string(u"Fall"));
    QCOMPARE(port.lastRequest->roster.rows[0][0], std::u16string(u"Alice"));
    QCOMPARE(port.lastRequest->roster.rows[0][1], std::u16string(u"\uAE40\uBBFC\uC9C0(B)"));
    QVERIFY(port.lastRequest->roster.columnWidths == value.roster.columnWidths);
    QVERIFY(port.lastRequest->allowQuestionableKoreanNameLengths);
    value.roster.rows[0][1] = u"\uAE40";
    value.allowQuestionableKoreanNameLengths = false;
    QVERIFY(!Application::RosterSaveUseCase::execute(value, port, equals));
    QCOMPARE(port.callCount, 1);
    value.allowQuestionableKoreanNameLengths = true;
    QVERIFY(Application::RosterSaveUseCase::execute(value, port, equals));
    QCOMPARE(port.callCount, 2);
}

void NextApplicationRosterSaveUseCaseTests::directPreparationPreservesStructureAndIssueOrder()
{
    Application::RosterSnapshot snapshot;
    snapshot.columns = {u" English ", u"ENGLISH", u" ", std::u16string(65, u'X')};
    snapshot.columnWidths = {-1, 0, 9, 10, 11};
    snapshot.rows.resize(26);
    snapshot.rows[0] = {u" A2 ", u"", u"", u"", std::u16string(10001, u'z')};
    const auto prepared = Application::prepareRosterSave(snapshot, false, equals);
    QVERIFY(prepared.hasErrors());
    QVERIFY(prepared.roster.columnWidths == snapshot.columnWidths);
    QCOMPARE(prepared.roster.rows.size(), std::size_t(26));
    QCOMPARE(prepared.roster.rows[0].size(), std::size_t(5));
    QCOMPARE(prepared.roster.rows[0][0], std::u16string(u"A2"));
    QVERIFY(prepared.roster.rows[1].empty());
    std::vector<std::string> codes;
    for (const auto& issue : prepared.issues) codes.push_back(issue.code);
    QVERIFY(codes == std::vector<std::string>({
        "roster.column.required", "roster.column.required", "roster.column.required",
        "roster.column.required", "roster.column.required", "roster.column.duplicate",
        "validation.length.out_of_bounds", "validation.length.out_of_bounds",
        "roster.column_widths.invalid_count", "roster.rows.too_many",
        "roster.row.too_many_cells", "validation.length.out_of_bounds",
        "roster.student_name.required", "student_name.english.invalid_characters"}));
    const auto& required = prepared.issues[12];
    QCOMPARE(required.field, std::u16string(u"rows[0].Korean"));
    QCOMPARE(required.row, 0);
    QCOMPARE(required.column, -1);
    QCOMPARE(std::get<std::u16string>(required.arguments.at("field")), required.field);
    const auto& cell = prepared.issues[11];
    QCOMPARE(cell.field, std::u16string(u"rows[0].cells[4]"));
    QCOMPARE(std::get<std::int64_t>(cell.arguments.at("length")), std::int64_t(10001));
}


void NextApplicationRosterSaveUseCaseTests::rawBomConversionDoesNotMutateRequestOrAddSentinels()
{
    RecordingRosterSavePort port;
    auto value = request("42");
    value.roster.columns[0] = u"\ufeffEnglish";
    value.roster.rows[0].resize(7);
    value.roster.rows[0][0] = u"\ufeffAlice";
    value.roster.rows[0][6] = u"\ufeff\ufeffNotes";
    const auto original = value;
    QVERIFY(Application::RosterSaveUseCase::execute(value, port, equals));
    QVERIFY(value == original);
    QCOMPARE(port.lastRequest->roster.columns[0], std::u16string(u"English"));
    QCOMPARE(port.lastRequest->roster.rows[0][0], std::u16string(u"Alice"));
    QCOMPARE(port.lastRequest->roster.rows[0][6], std::u16string(u"\ufeffNotes"));
    const auto feedback = Application::prepareRosterSaveText(value.roster, true, equals);
    QVERIFY(feedback.hasErrors());
    QCOMPARE(feedback.roster.rows[0][0], std::u16string(u"\ufeffAlice"));
    QCOMPARE(feedback.roster.rows[0][6], std::u16string(u"\ufeff\ufeffNotes"));
}


void NextApplicationRosterSaveUseCaseTests::policyAcceptsBoundarySizesAndCountsUtf16Units()
{
    auto value = request("42");
    value.roster.columnWidths.resize(value.roster.columns.size(), -1);
    std::u16string supplementary;
    for (int index = 0; index < 32; ++index) supplementary += u"\U0001f4da";
    value.roster.columns[6] = supplementary;
    value.roster.rows[0].resize(7);
    value.roster.rows[0][0] = std::u16string(20, u'a');
    value.roster.rows[0][6] = std::u16string(10000, u'x');
    const auto atLimit = Application::prepareRosterSave(value.roster, false, equals);
    QVERIFY(!atLimit.hasErrors());
    QVERIFY(atLimit.issues.empty());
    QCOMPARE(atLimit.roster.columns[6].size(), std::size_t(64));
    auto beyond = value.roster;
    beyond.columns[6] += u'x';
    beyond.rows[0][6] += u'x';
    beyond.rows[0][0] += u'a';
    beyond.rows.resize(26);
    const auto rejected = Application::prepareRosterSave(beyond, false, equals);
    QVERIFY(rejected.hasErrors());
    QCOMPARE(rejected.issues.size(), std::size_t(4));
    QCOMPARE(rejected.issues[0].code, std::string("validation.length.out_of_bounds"));
    QCOMPARE(std::get<std::int64_t>(rejected.issues[0].arguments.at("length")), std::int64_t(65));
    QCOMPARE(rejected.issues[1].code, std::string("roster.rows.too_many"));
    QCOMPARE(rejected.issues[2].code, std::string("validation.length.out_of_bounds"));
    QCOMPARE(rejected.issues[3].code, std::string("student_name.english.too_long"));
}

QTEST_APPLESS_MAIN(NextApplicationRosterSaveUseCaseTests)

#include "next_application_roster_save_use_case_tests.moc"
