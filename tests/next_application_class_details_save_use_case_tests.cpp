#include "next/application/class_details_save_use_case.h"
#include "next/application/class_details_save_workflow.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Application::ClassDetailsSaveRequest request(std::string classId)
{
    return {
        .classId = *Domain::ClassId::fromString(classId),
        .classGrade = u"E4",
        .classLevel = u"Theseus",
        .readingBook = u"Reading Explorer 1",
        .essayBook = u"",
        .classColor = u"#123456",
        .fontColor = u"#654321",
        .regularTimes = std::vector<Domain::ScheduleTime>{
            *Domain::ScheduleTime::fromMinutes(0, 9 * 60, 9 * 60 + 50)
        },
        .intensiveTimes = std::vector<Domain::ScheduleTime>{
            *Domain::ScheduleTime::fromMinutes(4, 10 * 60, 10 * 60 + 50)
        }
    };
}

class RecordingSavePort final : public Application::ClassDetailsSavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveClassDetails(
        const Application::ClassDetailsSaveRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassDetailsSaveRequest> lastRequest;
    Domain::Result<void> result = Domain::Result<void>::success();
};


class WorkflowPorts final : public Application::ClassDetailsValidationContextPort,
    public Application::ClassDetailsScheduleConflictPort, public Application::ClassDetailsSavePort
{
public:
    Application::ClassDetailsValidationContextPortResult loadClassDetailsValidationContext(
        const Domain::ClassId& classId) const override
    {
        calls.push_back("context");
        contextIds.push_back(classId);
        return context;
    }
    Application::ClassDetailsScheduleConflictResult classDetailsScheduleConflicts(
        const Application::ClassDetailsScheduleConflictRequest& value) const override
    {
        calls.push_back(value.mode == Application::ClassDetailsScheduleMode::Regular
            ? "regular" : "intensive");
        conflicts.push_back(value);
        return value.mode == Application::ClassDetailsScheduleMode::Regular ? regular : intensive;
    }
    Domain::Result<void> saveClassDetails(const Application::ClassDetailsSaveRequest& value) const override
    {
        calls.push_back("save");
        saved = value;
        return save;
    }
    mutable std::vector<std::string> calls;
    mutable std::vector<Domain::ClassId> contextIds;
    mutable std::vector<Application::ClassDetailsScheduleConflictRequest> conflicts;
    mutable std::optional<Application::ClassDetailsSaveRequest> saved;
    Application::ClassDetailsValidationContextPortResult context =
        Application::ClassDetailsValidationContextPortResult::success({
            *Domain::ClassId::fromString("42"), -1, u"  Fresh notes  ", u" Fresh activities "});
    Application::ClassDetailsScheduleConflictResult regular =
        Application::ClassDetailsScheduleConflictResult::success({});
    Application::ClassDetailsScheduleConflictResult intensive =
        Application::ClassDetailsScheduleConflictResult::success({});
    Domain::Result<void> save = Domain::Result<void>::success();
};

Application::ClassDetailsSaveWorkflowRequest workflowRequest()
{
    Application::ClassDetailsValidationInput input;
    input.classId = 42;
    input.teacherId = 0; // Fresh context must replace stale input hidden fields.
    input.notes = std::u16string(10001, u'x');
    input.classGrade = u" e4 ";
    input.classLevel = u" theseus ";
    input.readingBook = u" reading explorer 1 ";
    input.essayBook = u" 4a ";
    input.classColor = u" #aabbcc ";
    input.fontColor = u" #112233 ";
    input.regularTimes = {{u" Monday ", u"9:00 AM", u"9:50 AM"}};
    input.intensiveTimes = {{u"tuesday", u"12:00 AM", u"12:50 AM"}};
    return {.input = input};
}

Application::ClassDetailsValidationCatalog workflowCatalog()
{
    return {{{u"E4", {{u"Theseus", {u"Reading Explorer 1"}, {u"4A"}}}}}};
}

Application::ClassDetailsSaveWorkflowResult runWorkflow(
    const Application::ClassDetailsSaveWorkflowRequest& value, const WorkflowPorts& ports)
{
    return Application::ClassDetailsSaveWorkflow::execute(
        value, workflowCatalog(), ports, ports, ports);
}

const Domain::OperationError workflowError{Domain::ErrorCode::Technical, "workflow port failure", true};
const std::vector<Application::ClassDetailsScheduleConflict> workflowConflicts{
    {u"Class",u"Monday",u"9:00 AM",u"9:50 AM",u"Other Class"}};

}

class NextApplicationClassDetailsSaveUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void invalidClassIdsDoNotReachPort();
    void validRequestPreservesTypedScheduleValues();
    void forwardsRequestsWithAbsentSchedules();
    void forwardsOptionalTeacherOverride();
    void invalidOptionalTeacherIdsDoNotReachPort();
    void preservesPortFailure();
    void workflowSuccessNormalizesAndPreservesStageOrder();
    void workflowQueriesEmptySchedulesAndKeepsOptionalPayload();
    void workflowContextFailureAndMismatchFallBack();
    void workflowPreservesContextSnapshotTextDecoding();
    void workflowInvalidContextAndFieldsStopBeforeConflicts();
    void workflowConflictAndQueryFailuresShortCircuit_data();
    void workflowConflictAndQueryFailuresShortCircuit();
    void workflowSaveFailureRetainsErrorAndOrder();
    void workflowRawTimeConversionPreservesStrictBoundary_data();
    void workflowRawTimeConversionPreservesStrictBoundary();
    void workflowInvalidIdsDoNotReachPorts();
};

void NextApplicationClassDetailsSaveUseCaseTests::
invalidClassIdsDoNotReachPort()
{
    RecordingSavePort port;
    for (const std::string classId : {"0", "-2", "class-42"})
    {
        const auto result = Application::ClassDetailsSaveUseCase::execute(
            request(classId),
            port
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationClassDetailsSaveUseCaseTests::
validRequestPreservesTypedScheduleValues()
{
    RecordingSavePort port;
    const Application::ClassDetailsSaveRequest value = request("42");

    const auto result =
        Application::ClassDetailsSaveUseCase::execute(value, port);
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId.value(), std::string("42"));
    QCOMPARE(port.lastRequest->classGrade, std::u16string(u"E4"));
    QCOMPARE(port.lastRequest->regularTimes, value.regularTimes);
    QCOMPARE(port.lastRequest->intensiveTimes, value.intensiveTimes);
}

void NextApplicationClassDetailsSaveUseCaseTests::
forwardsRequestsWithAbsentSchedules()
{
    RecordingSavePort port;
    Application::ClassDetailsSaveRequest value = request("42");
    value.regularTimes.reset();
    value.intensiveTimes.reset();

    const auto result =
        Application::ClassDetailsSaveUseCase::execute(value, port);
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId.value(), std::string("42"));
    QCOMPARE(port.lastRequest->classGrade, std::u16string(u"E4"));
    QCOMPARE(port.lastRequest->classLevel, std::u16string(u"Theseus"));
    QCOMPARE(port.lastRequest->readingBook,
        std::u16string(u"Reading Explorer 1"));
    QCOMPARE(port.lastRequest->essayBook, std::u16string(u""));
    QCOMPARE(port.lastRequest->classColor, std::u16string(u"#123456"));
    QCOMPARE(port.lastRequest->fontColor, std::u16string(u"#654321"));
    QVERIFY(!port.lastRequest->regularTimes.has_value());
    QVERIFY(!port.lastRequest->intensiveTimes.has_value());
}

void NextApplicationClassDetailsSaveUseCaseTests::
forwardsOptionalTeacherOverride()
{
    RecordingSavePort port;
    Application::ClassDetailsSaveRequest value = request("42");
    value.teacherId = Domain::TeacherId::fromString("8");

    const auto result =
        Application::ClassDetailsSaveUseCase::execute(value, port);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest->teacherId.has_value());
    QCOMPARE(port.lastRequest->teacherId->value(), std::string("8"));
}

void NextApplicationClassDetailsSaveUseCaseTests::
invalidOptionalTeacherIdsDoNotReachPort()
{
    RecordingSavePort port;
    for (const std::string teacherId : {"0", "08", "-2", "2147483648"})
    {
        Application::ClassDetailsSaveRequest value = request("42");
        value.teacherId = Domain::TeacherId::fromString(teacherId);

        const auto result =
            Application::ClassDetailsSaveUseCase::execute(value, port);

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationClassDetailsSaveUseCaseTests::preservesPortFailure()
{
    RecordingSavePort port;
    port.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "save rejected",
        .recoverable = true
    });

    const auto result = Application::ClassDetailsSaveUseCase::execute(
        request("42"),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("save rejected"));
    QCOMPARE(port.callCount, 1);
}


void NextApplicationClassDetailsSaveUseCaseTests::workflowSuccessNormalizesAndPreservesStageOrder()
{
    WorkflowPorts ports;
    const auto input = workflowRequest();
    const auto result = runWorkflow(input, ports);
    const auto* success = std::get_if<Application::ClassDetailsSaveWorkflowSuccess>(&result);
    QVERIFY(success);
    QCOMPARE(ports.calls, (std::vector<std::string>{"context","regular","intensive","save"}));
    QCOMPARE(ports.contextIds.front().value(), std::string("42"));
    QVERIFY(ports.saved);
    QCOMPARE(ports.saved->classGrade, std::u16string(u"E4"));
    QCOMPARE(ports.saved->classLevel, std::u16string(u"Theseus"));
    QCOMPARE(ports.saved->readingBook, std::u16string(u"Reading Explorer 1"));
    QCOMPARE(ports.saved->essayBook, std::u16string(u"4A"));
    QCOMPARE(ports.saved->classColor, std::u16string(u"#AABBCC"));
    QCOMPARE(success->normalized.teacherId, -1);
    QCOMPARE(success->normalized.notes, std::u16string(u"Fresh notes"));
    QCOMPARE(success->normalized.timeFillerActivities, std::u16string(u"Fresh activities"));
    QCOMPARE(success->normalized.regularTimes.front().day, std::u16string(u"Monday"));
    QCOMPARE(ports.conflicts.size(), std::size_t(2));
    QCOMPARE(ports.saved->regularTimes, std::optional(ports.conflicts[0].candidateTimes));
    QCOMPARE(ports.saved->intensiveTimes, std::optional(ports.conflicts[1].candidateTimes));
    QCOMPARE(ports.saved->regularTimes->front().startMinute(), 9 * 60);
    QCOMPARE(ports.saved->intensiveTimes->front().startMinute(), 0);
    QCOMPARE(input.input.teacherId, 0);
    QCOMPARE(input.input.notes.size(), std::size_t(10001));
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowQueriesEmptySchedulesAndKeepsOptionalPayload()
{
    for (bool present : {true,false})
    {
        WorkflowPorts ports;
        auto input = workflowRequest();
        input.input.regularTimes.clear(); input.input.intensiveTimes.clear();
        input.saveRegularTimes = present; input.saveIntensiveTimes = present;
        const auto result = runWorkflow(input, ports);
        QVERIFY(std::holds_alternative<Application::ClassDetailsSaveWorkflowSuccess>(result));
        QCOMPARE(ports.calls, (std::vector<std::string>{"context","regular","intensive","save"}));
        QCOMPARE(ports.conflicts.size(), std::size_t(2));
        QVERIFY(ports.conflicts[0].candidateTimes.empty());
        QVERIFY(ports.conflicts[1].candidateTimes.empty());
        QCOMPARE(ports.saved->regularTimes.has_value(), present);
        QCOMPARE(ports.saved->intensiveTimes.has_value(), present);
        if (present)
        {
            QVERIFY(ports.saved->regularTimes->empty());
            QVERIFY(ports.saved->intensiveTimes->empty());
        }
    }
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowContextFailureAndMismatchFallBack()
{
    for (bool mismatch : {true,false})
    {
        WorkflowPorts ports;
        ports.context = mismatch
            ? Application::ClassDetailsValidationContextPortResult::success({
                *Domain::ClassId::fromString("43"), 0, std::u16string(10001,u'x'), {}})
            : Application::ClassDetailsValidationContextPortResult::failure(workflowError);
        const auto result = runWorkflow(workflowRequest(), ports);
        const auto* success = std::get_if<Application::ClassDetailsSaveWorkflowSuccess>(&result);
        QVERIFY(success);
        QCOMPARE(ports.calls, (std::vector<std::string>{"context","regular","intensive","save"}));
        QCOMPARE(success->normalized.teacherId, -1);
        QVERIFY(success->normalized.notes.empty());
        QVERIFY(success->normalized.timeFillerActivities.empty());
    }
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowPreservesContextSnapshotTextDecoding()
{
    WorkflowPorts ports;
    ports.context = Application::ClassDetailsValidationContextPortResult::success({
        *Domain::ClassId::fromString("42"), -1,
        std::u16string(u"\uFEFF") + std::u16string(10000, u'n'),
        u"\uFFFE\u2000\u4100\u2000"});
    const auto result = runWorkflow(workflowRequest(), ports);
    const auto* success = std::get_if<Application::ClassDetailsSaveWorkflowSuccess>(&result);
    QVERIFY(success);
    QCOMPARE(success->normalized.notes, std::u16string(10000, u'n'));
    QCOMPARE(success->normalized.timeFillerActivities, std::u16string(u"A"));
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowInvalidContextAndFieldsStopBeforeConflicts()
{
    for (bool badContext : {true,false})
    {
        WorkflowPorts ports;
        auto input = workflowRequest();
        if (badContext)
            ports.context = Application::ClassDetailsValidationContextPortResult::success({
                *Domain::ClassId::fromString("42"), 0, {}, {}});
        else input.input.classGrade = u"Unknown grade";
        const auto result = runWorkflow(input, ports);
        const auto* validation = std::get_if<Application::ClassDetailsValidationOutput>(&result);
        QVERIFY(validation && validation->hasErrors());
        QCOMPARE(validation->issues.front().code, badContext
            ? Application::ClassDetailsValidationCode::TeacherIdInvalid
            : Application::ClassDetailsValidationCode::ValueNotAllowed);
        QCOMPARE(ports.calls, (std::vector<std::string>{"context"}));
        QVERIFY(!ports.saved);
        QVERIFY(ports.conflicts.empty());
    }
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowConflictAndQueryFailuresShortCircuit_data()
{
    QTest::addColumn<bool>("regular"); QTest::addColumn<bool>("queryFailure");
    QTest::newRow("regular conflict") << true << false;
    QTest::newRow("intensive conflict") << false << false;
    QTest::newRow("regular query error") << true << true;
    QTest::newRow("intensive query error") << false << true;
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowConflictAndQueryFailuresShortCircuit()
{
    QFETCH(bool, regular); QFETCH(bool, queryFailure);
    WorkflowPorts ports;
    (regular ? ports.regular : ports.intensive) = queryFailure
        ? Application::ClassDetailsScheduleConflictResult::failure(workflowError)
        : Application::ClassDetailsScheduleConflictResult::success(workflowConflicts);
    const auto result = runWorkflow(workflowRequest(), ports);
    if (queryFailure)
    {
        const auto* failure = std::get_if<Application::ClassDetailsSaveWorkflowFailure>(&result);
        QVERIFY(failure);
        QCOMPARE(failure->stage, regular ? Application::ClassDetailsSaveWorkflowStage::RegularConflicts
            : Application::ClassDetailsSaveWorkflowStage::IntensiveConflicts);
        QVERIFY(failure->error == workflowError);
    }
    else
    {
        const auto* conflict = std::get_if<Application::ClassDetailsSaveWorkflowConflict>(&result);
        QVERIFY(conflict);
        QCOMPARE(conflict->mode, regular ? Application::ClassDetailsScheduleMode::Regular
            : Application::ClassDetailsScheduleMode::Intensive);
        QCOMPARE(conflict->conflicts, workflowConflicts);
    }
    QCOMPARE(ports.calls, (regular ? std::vector<std::string>{"context","regular"}
                                 : std::vector<std::string>{"context","regular","intensive"}));
    QVERIFY(!ports.saved);
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowSaveFailureRetainsErrorAndOrder()
{
    WorkflowPorts ports; ports.save = Domain::Result<void>::failure(workflowError);
    const auto result = runWorkflow(workflowRequest(), ports);
    const auto* failure = std::get_if<Application::ClassDetailsSaveWorkflowFailure>(&result);
    QVERIFY(failure);
    QCOMPARE(failure->stage, Application::ClassDetailsSaveWorkflowStage::Save);
    QVERIFY(failure->error == workflowError);
    QCOMPARE(ports.calls, (std::vector<std::string>{"context","regular","intensive","save"}));
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowRawTimeConversionPreservesStrictBoundary_data()
{
    QTest::addColumn<QString>("time"); QTest::addColumn<bool>("intensive");
    QTest::newRow("24 hour regular") << QStringLiteral("09:50") << false;
    QTest::newRow("lowercase period regular") << QStringLiteral("9:50 am") << false;
    QTest::newRow("whitespace regular") << QStringLiteral(" 9:50 AM ") << false;
    QTest::newRow("24 hour intensive") << QStringLiteral("00:50") << true;
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowRawTimeConversionPreservesStrictBoundary()
{
    QFETCH(QString, time); QFETCH(bool, intensive);
    WorkflowPorts ports; auto input = workflowRequest();
    (intensive ? input.input.intensiveTimes : input.input.regularTimes)[0].endTime = time.toStdU16String();
    auto validationInput = input.input;
    validationInput.teacherId = -1; validationInput.notes.clear();
    QVERIFY(!Application::ClassDetailsValidationPolicy::normalizeAndValidate(
        validationInput, workflowCatalog()).hasErrors());
    const auto result = runWorkflow(input, ports);
    const auto* failure = std::get_if<Application::ClassDetailsSaveWorkflowFailure>(&result);
    QVERIFY(failure);
    QCOMPARE(failure->stage, intensive ? Application::ClassDetailsSaveWorkflowStage::IntensiveConflicts
        : Application::ClassDetailsSaveWorkflowStage::RegularConflicts);
    QCOMPARE(failure->error.message, std::string("Class schedule values could not be checked."));
    QCOMPARE(ports.calls, (intensive ? std::vector<std::string>{"context","regular"}
                                   : std::vector<std::string>{"context"}));
    QVERIFY(!ports.saved);
}

void NextApplicationClassDetailsSaveUseCaseTests::workflowInvalidIdsDoNotReachPorts()
{
    for (int classId : {-1,0})
    {
        WorkflowPorts ports; auto input = workflowRequest(); input.input.classId = classId;
        const auto result = runWorkflow(input, ports);
        const auto* validation = std::get_if<Application::ClassDetailsValidationOutput>(&result);
        QVERIFY(validation && validation->hasErrors());
        QCOMPARE(validation->issues.front().code, Application::ClassDetailsValidationCode::ClassIdInvalid);
        QVERIFY(ports.calls.empty());
    }
}

QTEST_APPLESS_MAIN(NextApplicationClassDetailsSaveUseCaseTests)

#include "next_application_class_details_save_use_case_tests.moc"
