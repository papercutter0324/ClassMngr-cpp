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
        ClassSummaryStudentCount{18},
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
    void acceptsMaximumBoundaryVisibleClassIdentifier();
    void invalidWeekdaysAreRejectedBeforeRead();
    void selectedDayValidationPreservesCountAndErrorPrecedence();
    void acceptsMaximumSelectedDayCount();
    void forwardsExactScopeAndReturns96ClassProjection();
    void projectionOrderingIsDeterministicForEqualOrders();
    void gatewayFailureIsPropagatedStructurally();
    void outOfScopeRowsFailAllOrNothing();
    void selectedDetailsFailAllOrNothing();
    void malformedProjectionDataFailsAllOrNothing();
    void unicodeWhitespaceOnlyRequiredTextErrorIsPropagatedUnchanged();
    void boundedNavigationMetadataIsEnforced();
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

    auto invalidMode = validRequest();
    invalidMode.mode = static_cast<ScheduleViewMode>(255);

    auto blankClassId = validRequest();
    blankClassId.visibleClassIds = {classId("  ")};

    auto overlongClassId = validRequest();
    overlongClassId.visibleClassIds = {classId(std::string(257, 'c'))};

    const std::vector<SubPrepScheduleScopeRequest> invalidRequests{
        duplicateClasses,
        oversizedClassScope,
        duplicateDays,
        invalidMode
    };
    for (const auto& request : invalidRequests)
    {
        const auto result = query.execute(request);
        verifyError(result, ErrorCode::InvalidInput);
        if (request.mode == static_cast<ScheduleViewMode>(255))
        {
            QCOMPARE(
                result.error().message,
                std::string("The Sub Prep schedule mode is invalid.")
                );
            QVERIFY(!result.error().recoverable);
        }
    }

    for (const auto& request : {blankClassId, overlongClassId})
    {
        const auto result = query.execute(request);
        verifyError(result, ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("Visible class identifiers must be non-blank and bounded.")
            );
        QVERIFY(!result.error().recoverable);
    }

    auto duplicateBlankClassIds = validRequest();
    duplicateBlankClassIds.visibleClassIds = {
        classId(" \t "),
        classId(" \t ")
    };
    const auto duplicateBlankResult = query.execute(duplicateBlankClassIds);
    verifyError(duplicateBlankResult, ErrorCode::InvalidInput);
    QCOMPARE(
        duplicateBlankResult.error().message,
        std::string("Visible class identifiers must be non-blank and bounded.")
        );
    QVERIFY(!duplicateBlankResult.error().recoverable);
    QCOMPARE(readPort.calls, 0);

    auto overLimitWithBlankId = oversizedClassScope;
    overLimitWithBlankId.visibleClassIds.front() = classId(" \t ");
    const auto overLimitResult = query.execute(overLimitWithBlankId);
    verifyError(overLimitResult, ErrorCode::InvalidInput);
    QCOMPARE(
        overLimitResult.error().message,
        std::string("Visible Sub Prep class scope exceeds its bounded limit.")
        );
    QVERIFY(!overLimitResult.error().recoverable);

    QCOMPARE(readPort.calls, 0);
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
acceptsMaximumBoundaryVisibleClassIdentifier()
{
    const std::string maximumClassIdentifier(256, 'c');
    auto request = validRequest();
    request.visibleClassIds = {classId(maximumClassIdentifier)};
    request.selectedDays = {SubPrepWeekday::Monday};

    auto input = validInput();
    input.classes = {
        summary(maximumClassIdentifier, teacherId("teacher-1"), 0)
    };
    input.classes.front().displayLabel = "Class at identifier boundary";

    FakeReadPort readPort;
    readPort.result =
        SubPrepScheduleSummaryReadResult::success(std::move(input));
    const SubPrepScheduleSummaryQuery query(readPort);
    const auto result = query.execute(request);

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QVERIFY(readPort.lastRequest == request);
    QCOMPARE(result.value().classes().size(), std::size_t(1));
    QCOMPARE(result.value().classes().front().id, classId(maximumClassIdentifier));
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
invalidWeekdaysAreRejectedBeforeRead()
{
    for (const auto weekday : {0, 8})
    {
        FakeReadPort readPort;
        auto request = validRequest();
        request.selectedDays = {static_cast<SubPrepWeekday>(weekday)};

        const SubPrepScheduleSummaryQuery query(readPort);
        const auto result = query.execute(request);

        verifyError(result, ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("A selected Sub Prep weekday is invalid.")
            );
        QVERIFY(!result.error().recoverable);
        QCOMPARE(readPort.calls, 0);
    }
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
selectedDayValidationPreservesCountAndErrorPrecedence()
{
    const auto verifyInvalidDays = [](
                                   const std::vector<SubPrepWeekday>& days,
                                   const char* expectedMessage
                                   )
    {
        FakeReadPort readPort;
        auto request = validRequest();
        request.selectedDays = days;

        const SubPrepScheduleSummaryQuery query(readPort);
        const auto result = query.execute(request);

        verifyError(result, ErrorCode::InvalidInput);
        QCOMPARE(result.error().message, std::string(expectedMessage));
        QVERIFY(!result.error().recoverable);
        QCOMPARE(readPort.calls, 0);
    };

    verifyInvalidDays(
        {
            static_cast<SubPrepWeekday>(8),
            SubPrepWeekday::Monday,
            SubPrepWeekday::Tuesday,
            SubPrepWeekday::Wednesday,
            SubPrepWeekday::Thursday,
            SubPrepWeekday::Friday,
            SubPrepWeekday::Saturday,
            SubPrepWeekday::Sunday
        },
        "Selected Sub Prep days exceed the weekly limit."
        );
    verifyInvalidDays(
        {
            SubPrepWeekday::Monday,
            static_cast<SubPrepWeekday>(8),
            static_cast<SubPrepWeekday>(8)
        },
        "A selected Sub Prep weekday is invalid."
        );
    verifyInvalidDays(
        {
            SubPrepWeekday::Monday,
            SubPrepWeekday::Tuesday,
            SubPrepWeekday::Monday
        },
        "Selected Sub Prep weekdays must be unique."
        );
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
acceptsMaximumSelectedDayCount()
{
    FakeReadPort readPort;
    auto request = validRequest();
    request.selectedDays = {
        SubPrepWeekday::Monday,
        SubPrepWeekday::Tuesday,
        SubPrepWeekday::Wednesday,
        SubPrepWeekday::Thursday,
        SubPrepWeekday::Friday,
        SubPrepWeekday::Saturday,
        SubPrepWeekday::Sunday
    };

    const SubPrepScheduleSummaryQuery query(readPort);
    const auto result = query.execute(request);

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QVERIFY(readPort.lastRequest == request);
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
    QCOMPARE(
        result.value().classes().front().studentCount.rawValue(),
        std::size_t(18)
        );
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
        .classId = classId("class-1"),
        .teacherId = teacherId("teacher-1"),
        .classNotes = "Class detail notes",
        .teacherDisplayName = "Teacher One",
        .teacherFacilities = {.room = "Room 4"},
        .teacherNotes = "Teacher detail notes"
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

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
unicodeWhitespaceOnlyRequiredTextErrorIsPropagatedUnchanged()
{
    auto blankTeacherName = validInput();
    blankTeacherName.teachers.front().displayName = "\xC2\xA0";

    auto blankClassText = validInput();
    blankClassText.classes.front().grade = "\xE3\x80\x80";

    const std::pair<ClassSummaryProjectionInput, std::string> cases[] = {
        {
            std::move(blankTeacherName),
            "Teacher display name must be required and bounded; facilities and notes must be bounded."
        },
        {
            std::move(blankClassText),
            "Class grade, level, display label, and meeting text must be non-blank and bounded."
        }
    };

    for (const auto& testCase : cases)
    {
        const auto projectionResult = ClassSummaryProjection::create(
            testCase.first
            );
        QVERIFY(!projectionResult);

        FakeReadPort readPort;
        readPort.result = SubPrepScheduleSummaryReadResult::success(
            testCase.first
            );
        const SubPrepScheduleSummaryQuery query(readPort);
        const auto result = query.execute(validRequest());

        verifyError(result, ErrorCode::InvalidInput);
        QCOMPARE(result.error().message, testCase.second);
        QVERIFY(result.error() == projectionResult.error());
        QVERIFY(!result.error().recoverable);
        QCOMPARE(readPort.calls, 1);
    }
}

void ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests::
boundedNavigationMetadataIsEnforced()
{
    FakeReadPort readPort;
    const SubPrepScheduleSummaryQuery query(readPort);

    auto oversizedLabel = validInput();
    oversizedLabel.classes.front().navigationLabel.assign(
        kClassSummaryMaxNavigationLabelLength + 1,
        'N'
        );
    auto oversizedTeacherLabel = validInput();
    oversizedTeacherLabel.classes.front().navigationTeacherLabel.assign(
        kClassSummaryMaxNavigationTeacherLabelLength + 1,
        'T'
        );
    auto invalidDayOrder = validInput();
    invalidDayOrder.classes.front().navigationFirstDayOrder =
        ClassTabNavigationOrderRank{7};
    auto invalidTimeOrder = validInput();
    invalidTimeOrder.classes.front().navigationFirstTimeOrder =
        ClassTabNavigationOrderRank{24 * 60};

    for (auto input : {
             oversizedLabel,
             oversizedTeacherLabel,
             invalidDayOrder,
             invalidTimeOrder
         })
    {
        readPort.result = SubPrepScheduleSummaryReadResult::success(
            std::move(input)
            );
        verifyError(query.execute(validRequest()), ErrorCode::InvalidInput);
    }
    QCOMPARE(readPort.calls, 4);
}

QTEST_APPLESS_MAIN(ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests)

#include "next_application_sub_prep_schedule_summary_query_tests.moc"
