#include "next/application/sub_prep_schedule_summary_query.h"

#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

ClassId classId(std::string value)
{
    return *ClassId::fromString(value);
}

TeacherId teacherId(std::string value)
{
    return *TeacherId::fromString(value);
}

ClassSummary summary(
    std::string id,
    std::optional<TeacherId> teacher = std::nullopt,
    std::int32_t order = 0
    )
{
    const std::string displayId = id;
    return ClassSummary{
        classId(std::move(id)),
        std::move(teacher),
        "Grade 4",
        "Level B",
        "Class " + displayId,
        "Mon 09:00",
        18,
        order
    };
}

TeacherSummary teacher(std::string id)
{
    const std::string displayId = id;
    return TeacherSummary{
        teacherId(std::move(id)),
        "Teacher " + displayId,
        "Room 4",
        "Notes for " + displayId
    };
}

SubPrepScheduleScopeRequest validRequest()
{
    return {
        {classId("class-1"), classId("class-2")},
        {SubPrepWeekday::Monday, SubPrepWeekday::Wednesday},
        ScheduleViewMode::Regular
    };
}

ClassSummaryProjectionInput validInput()
{
    ClassSummaryProjectionInput input;
    input.teachers = {teacher("teacher-1")};
    input.classes = {summary("class-1", teacherId("teacher-1"), 0)};
    return input;
}

class FakeReadPort final : public SubPrepScheduleSummaryReadPort
{
public:
    SubPrepScheduleSummaryReadResult result =
        SubPrepScheduleSummaryReadResult::success({});
    int calls = 0;
    std::optional<SubPrepScheduleScopeRequest> lastRequest;

    [[nodiscard]] SubPrepScheduleSummaryReadResult loadSummaries(
        const SubPrepScheduleScopeRequest& request
        ) override
    {
        ++calls;
        lastRequest = request;
        return result;
    }
};

void verifyError(
    const SubPrepScheduleSummaryQueryResult& result,
    const ErrorCode expectedCode
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, expectedCode);
}

}

class ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void requestUsesTypedOwnedValues();
    void emptyVisibilityReturnsEmptyProjectionWithoutRead();
    void invalidRequestsAreRejectedBeforeRead();
    void forwardsExactScopeAndReturns96ClassProjection();
    void projectionOrderingIsDeterministicForEqualOrders();
    void gatewayFailureIsPropagatedStructurally();
    void outOfScopeRowsFailAllOrNothing();
    void selectedDetailsFailAllOrNothing();
    void malformedProjectionDataFailsAllOrNothing();
};

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
requestUsesTypedOwnedValues()
{
    static_assert(std::is_same_v<
        decltype(SubPrepScheduleScopeRequest::visibleClassIds),
        std::vector<ClassId>
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepScheduleScopeRequest::selectedDays),
        std::vector<SubPrepWeekday>
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepScheduleScopeRequest::mode),
        ScheduleViewMode
        >);
    static_assert(std::is_copy_constructible_v<SubPrepScheduleScopeRequest>);
    static_assert(std::is_copy_assignable_v<SubPrepScheduleScopeRequest>);

    const auto original = validRequest();
    auto changed = original;
    changed.visibleClassIds.clear();
    changed.selectedDays.clear();
    changed.mode = ScheduleViewMode::Intensive;
    QCOMPARE(original.visibleClassIds.size(), std::size_t(2));
    QCOMPARE(original.selectedDays.size(), std::size_t(2));
    QCOMPARE(original.mode, ScheduleViewMode::Regular);
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
emptyVisibilityReturnsEmptyProjectionWithoutRead()
{
    FakeReadPort readPort;
    const SubPrepScheduleSummaryQuery query(readPort);

    auto noClasses = validRequest();
    noClasses.visibleClassIds.clear();
    noClasses.selectedDays = {
        SubPrepWeekday::Monday,
        SubPrepWeekday::Tuesday,
        SubPrepWeekday::Wednesday,
        SubPrepWeekday::Thursday,
        SubPrepWeekday::Friday,
        SubPrepWeekday::Saturday,
        SubPrepWeekday::Sunday
    };
    auto classesOnly = validRequest();
    classesOnly.selectedDays.clear();

    for (const auto& request : {noClasses, classesOnly})
    {
        const auto result = query.execute(request);
        QVERIFY(result);
        QVERIFY(result.value().empty());
        QVERIFY(result.value().classes().empty());
        QVERIFY(result.value().teacherIndex().empty());
        QVERIFY(!result.value().selectedDetails().has_value());
    }

    QCOMPARE(readPort.calls, 0);
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
invalidRequestsAreRejectedBeforeRead()
{
    FakeReadPort readPort;
    const SubPrepScheduleSummaryQuery query(readPort);

    auto duplicateClasses = validRequest();
    duplicateClasses.visibleClassIds.push_back(classId("class-1"));

    auto oversizedClassScope = validRequest();
    oversizedClassScope.visibleClassIds.clear();
    oversizedClassScope.visibleClassIds.reserve(
        kSubPrepScheduleScopeMaxVisibleClasses + 1
        );
    for (std::size_t index = 0;
         index <= kSubPrepScheduleScopeMaxVisibleClasses;
         ++index)
    {
        oversizedClassScope.visibleClassIds.push_back(
            classId("class-" + std::to_string(index))
            );
    }

    auto duplicateDays = validRequest();
    duplicateDays.selectedDays.push_back(SubPrepWeekday::Monday);

    auto invalidDay = validRequest();
    invalidDay.selectedDays = {static_cast<SubPrepWeekday>(0)};

    auto invalidMode = validRequest();
    invalidMode.mode = static_cast<ScheduleViewMode>(255);

    auto blankClassId = validRequest();
    blankClassId.visibleClassIds = {classId("  ")};

    const std::vector<SubPrepScheduleScopeRequest> invalidRequests{
        duplicateClasses,
        oversizedClassScope,
        duplicateDays,
        invalidDay,
        invalidMode,
        blankClassId
    };
    for (const auto& request : invalidRequests)
    {
        verifyError(query.execute(request), ErrorCode::InvalidInput);
    }

    QCOMPARE(readPort.calls, 0);
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
forwardsExactScopeAndReturns96ClassProjection()
{
    FakeReadPort readPort;
    auto request = validRequest();
    request.visibleClassIds.clear();
    for (int index = 96; index > 0; --index)
    {
        request.visibleClassIds.push_back(
            classId("class-" + std::to_string(index))
            );
    }
    request.selectedDays = {
        SubPrepWeekday::Friday,
        SubPrepWeekday::Monday,
        SubPrepWeekday::Wednesday
    };
    request.mode = ScheduleViewMode::Intensive;

    ClassSummaryProjectionInput input;
    input.teachers = {teacher("teacher-2"), teacher("teacher-1")};
    input.classes.reserve(96);
    for (int index = 95; index >= 0; --index)
    {
        const auto teacher = teacherId(
            index % 2 == 0 ? "teacher-1" : "teacher-2"
            );
        input.classes.push_back(summary(
            "class-" + std::to_string(index + 1),
            teacher,
            index
            ));
    }
    readPort.result = SubPrepScheduleSummaryReadResult::success(
        std::move(input)
        );

    const SubPrepScheduleSummaryQuery query(readPort);
    const auto result = query.execute(request);

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QVERIFY(readPort.lastRequest.has_value());
    QVERIFY(*readPort.lastRequest == request);
    QCOMPARE(result.value().classes().size(), std::size_t(96));
    QCOMPARE(result.value().teacherIndex().summaries().size(), std::size_t(2));
    QCOMPARE(result.value().classes().front().id.value(), std::string("class-1"));
    QCOMPARE(result.value().classes().front().order, std::int32_t(0));
    QCOMPARE(result.value().classes().front().grade, std::string("Grade 4"));
    QCOMPARE(result.value().classes().front().studentCount, std::size_t(18));
    QCOMPARE(result.value().classes().back().id.value(), std::string("class-96"));
    QCOMPARE(result.value().classes().back().order, std::int32_t(95));
    QCOMPARE(
        result.value().teacherIndex().summaries().front().id.value(),
        std::string("teacher-1")
        );
    QVERIFY(!result.value().selectedDetails().has_value());
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
projectionOrderingIsDeterministicForEqualOrders()
{
    FakeReadPort readPort;
    const SubPrepScheduleSummaryQuery query(readPort);
    auto request = validRequest();
    request.visibleClassIds = {classId("class-a"), classId("class-b")};

    ClassSummaryProjectionInput firstInput;
    firstInput.teachers = {teacher("teacher-2"), teacher("teacher-1")};
    firstInput.classes = {
        summary("class-b", teacherId("teacher-2"), 7),
        summary("class-a", teacherId("teacher-1"), 7)
    };
    readPort.result = SubPrepScheduleSummaryReadResult::success(firstInput);
    const auto firstResult = query.execute(request);
    QVERIFY(firstResult);

    std::reverse(firstInput.classes.begin(), firstInput.classes.end());
    std::reverse(firstInput.teachers.begin(), firstInput.teachers.end());
    readPort.result = SubPrepScheduleSummaryReadResult::success(firstInput);
    const auto secondResult = query.execute(request);
    QVERIFY(secondResult);

    QVERIFY(firstResult.value() == secondResult.value());
    QCOMPARE(firstResult.value().classes().size(), std::size_t(2));
    QCOMPARE(firstResult.value().classes()[0].id.value(), std::string("class-a"));
    QCOMPARE(firstResult.value().classes()[1].id.value(), std::string("class-b"));
    QCOMPARE(
        firstResult.value().teacherIndex().summaries()[0].id.value(),
        std::string("teacher-1")
        );
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
gatewayFailureIsPropagatedStructurally()
{
    FakeReadPort readPort;
    readPort.result = SubPrepScheduleSummaryReadResult::failure({
        .code = ErrorCode::Technical,
        .message = "Sub Prep schedule read failed",
        .recoverable = true
    });

    const SubPrepScheduleSummaryQuery query(readPort);
    const auto result = query.execute(validRequest());

    verifyError(result, ErrorCode::Technical);
    QCOMPARE(result.error().message, std::string("Sub Prep schedule read failed"));
    QVERIFY(result.error().recoverable);
    QCOMPARE(readPort.calls, 1);
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
outOfScopeRowsFailAllOrNothing()
{
    FakeReadPort readPort;
    auto input = validInput();
    input.classes.push_back(summary("class-outside-scope", std::nullopt, 1));
    readPort.result = SubPrepScheduleSummaryReadResult::success(std::move(input));

    const SubPrepScheduleSummaryQuery query(readPort);
    const auto result = query.execute(validRequest());

    verifyError(result, ErrorCode::Validation);
    QCOMPARE(readPort.calls, 1);
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
selectedDetailsFailAllOrNothing()
{
    FakeReadPort readPort;
    auto input = validInput();
    input.selectedDetails = SelectedClassDetails{
        classId("class-1"),
        teacherId("teacher-1"),
        "Class detail notes",
        "Teacher One",
        "Room 4",
        "Teacher detail notes"
    };
    readPort.result = SubPrepScheduleSummaryReadResult::success(std::move(input));

    const SubPrepScheduleSummaryQuery query(readPort);
    const auto result = query.execute(validRequest());

    verifyError(result, ErrorCode::Validation);
    QCOMPARE(readPort.calls, 1);
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
malformedProjectionDataFailsAllOrNothing()
{
    FakeReadPort readPort;
    auto input = validInput();
    input.classes.push_back(input.classes.front());
    readPort.result = SubPrepScheduleSummaryReadResult::success(std::move(input));

    const SubPrepScheduleSummaryQuery query(readPort);
    const auto result = query.execute(validRequest());

    verifyError(result, ErrorCode::InvalidInput);
    QCOMPARE(readPort.calls, 1);
}

QTEST_APPLESS_MAIN(ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests)

#include "next_application_sub_prep_schedule_summary_query_tests.moc"
