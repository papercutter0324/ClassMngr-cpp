#include "next/application/sub_prep_print_source_query.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <functional>
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
    return *ClassId::fromString(std::move(value));
}

TeacherId teacherId(std::string value)
{
    return *TeacherId::fromString(std::move(value));
}

SubPrepPrintTeacher teacher(
    std::string id = "teacher-1"
    )
{
    return {
        .id = teacherId(std::move(id)),
        .englishName = "Teacher One",
        .room = "Room 1",
        .wifiName = "Network",
        .wifiPassword = "Password",
        .internetType = "Fiber",
        .zoomId = "zoom-1",
        .zoomPassword = "zoom-pass",
        .projectionType = "HDMI",
        .teacherNotes = "Teacher notes"
    };
}

SubPrepPrintMeeting meeting(
    const SubPrepWeekday weekday = SubPrepWeekday::Monday,
    std::string start = "09:00",
    std::string end = "09:45"
    )
{
    return {
        .weekday = weekday,
        .startTime = std::move(start),
        .endTime = std::move(end)
    };
}

SubPrepPrintClass printClass(
    std::string id = "class-1",
    std::optional<TeacherId> assignedTeacher = teacherId("teacher-1")
    )
{
    return {
        .id = classId(std::move(id)),
        .teacherId = std::move(assignedTeacher),
        .grade = "Grade 4",
        .level = "Level B",
        .classNotes = "Class notes",
        .classColor = "#112233",
        .fontColor = "#ffffff",
        .studentCount = 18,
        .meetings = {meeting()}
    };
}

SubPrepPrintSourceRequest validRequest()
{
    return {
        {classId("class-1"), classId("class-2")},
        {SubPrepWeekday::Monday, SubPrepWeekday::Wednesday},
        ScheduleViewMode::Regular
    };
}

SubPrepPrintSourceInput validInput()
{
    return {
        {teacher()},
        {printClass()}
    };
}

std::string uniqueId(
    const char prefix,
    const std::size_t index
    )
{
    const auto suffix = std::to_string(index);
    return std::string(
               kSubPrepPrintSourceMaxIdentifierLength - suffix.size(),
               prefix
               )
        + suffix;
}

class FakeReadPort final : public SubPrepPrintSourceReadPort
{
public:
    SubPrepPrintSourceReadResult result =
        SubPrepPrintSourceReadResult::success({});
    int calls = 0;
    std::optional<SubPrepPrintSourceRequest> lastRequest;

    [[nodiscard]] SubPrepPrintSourceReadResult loadSource(
        const SubPrepPrintSourceRequest& request
        ) override
    {
        ++calls;
        lastRequest = request;
        return result;
    }
};

void verifyError(
    const SubPrepPrintSourceQueryResult& result,
    const ErrorCode expectedCode
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, expectedCode);
}

void verifyValidationError(
    const SubPrepPrintSourceQueryResult& result
    )
{
    verifyError(result, ErrorCode::Validation);
}

} // namespace

class ClassMngrNextApplicationSubPrepPrintSourceQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void requestAndResultsOwnTypedValues();
    void invalidRequestsAreRejectedBeforeRead();
    void emptyClassesOrDaysReturnWithoutRead();
    void forwardsExactScopePreservesPortOrderAndTeacherReferences();
    void propagatesReadPortFailure();
    void acceptsMissingTeacherAndSubsetOfRequestedClasses();
    void acceptsEmptyOptionalTextFields();
    void rejectsOutOfScopeAndDuplicateClassesAllOrNothing();
    void rejectsMalformedIdentifiersAndMissingOrUnusedTeachers();
    void rejectsDuplicateTeacherIdsAndInvalidMeetingWeekdays();
    void rejectsMeetingsOutsideSelectedDaysAllOrNothing();
    void acceptsMaximumBoundaryTextLengths();
    void rejectsMalformedAndOverBoundTextAllOrNothing();
    void acceptsMaximumMeetingsPerClass();
    void acceptsMaximumAggregateCounts();
    void rejectsOverBoundCountsAllOrNothing();
    void repeatedCallsReadThroughAndKeepPreviousResultIndependent();
};

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
requestAndResultsOwnTypedValues()
{
    static_assert(std::is_same_v<
        decltype(SubPrepPrintSourceRequest::selectedClassIds),
        std::vector<ClassId>
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepPrintSourceRequest::selectedDays),
        std::vector<SubPrepWeekday>
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepPrintSourceRequest::mode),
        ScheduleViewMode
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<SubPrepPrintSourceReadPort&>().loadSource(
            std::declval<const SubPrepPrintSourceRequest&>()
            )),
        SubPrepPrintSourceReadResult
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const SubPrepPrintSourceQuery>().execute(
            std::declval<const SubPrepPrintSourceRequest&>()
            )),
        SubPrepPrintSourceQueryResult
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const SubPrepPrintSource&>().classes()),
        const std::vector<SubPrepPrintClass>&
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const SubPrepPrintSource&>().teachers()),
        const std::vector<SubPrepPrintTeacher>&
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepPrintSourceInput::classes),
        std::vector<SubPrepPrintClass>
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepPrintSourceInput::teachers),
        std::vector<SubPrepPrintTeacher>
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepPrintClass::meetings),
        std::vector<SubPrepPrintMeeting>
        >);
    static_assert(std::is_copy_constructible_v<SubPrepPrintSourceRequest>);
    static_assert(std::is_copy_assignable_v<SubPrepPrintSourceRequest>);
    static_assert(std::is_copy_constructible_v<SubPrepPrintSourceInput>);
    static_assert(std::is_copy_assignable_v<SubPrepPrintSourceInput>);
    static_assert(std::is_copy_constructible_v<SubPrepPrintTeacher>);
    static_assert(std::is_copy_constructible_v<SubPrepPrintClass>);
    static_assert(std::is_copy_constructible_v<SubPrepPrintMeeting>);
    static_assert(std::is_copy_constructible_v<SubPrepPrintSource>);
    static_assert(std::is_copy_assignable_v<SubPrepPrintSource>);

    FakeReadPort readPort;
    readPort.result = SubPrepPrintSourceReadResult::success(validInput());
    const SubPrepPrintSourceQuery query(readPort);
    const auto request = validRequest();
    const auto expectedRequest = request;
    const auto result = query.execute(request);

    QVERIFY(result);
    QVERIFY(readPort.lastRequest == expectedRequest);
    QVERIFY(result.value().classes() == validInput().classes);
    QVERIFY(result.value().teachers() == validInput().teachers);

    auto changedRequest = request;
    changedRequest.selectedClassIds.clear();
    changedRequest.selectedDays.clear();
    changedRequest.mode = ScheduleViewMode::Intensive;
    QCOMPARE(request.selectedClassIds.size(), std::size_t(2));
    QCOMPARE(request.selectedDays.size(), std::size_t(2));
    QCOMPARE(request.mode, ScheduleViewMode::Regular);

    auto resultCopy = result.value();
    QVERIFY(resultCopy == result.value());
    readPort.result = SubPrepPrintSourceReadResult::success({});
    QVERIFY(resultCopy.classes() == result.value().classes());
    QVERIFY(resultCopy.teachers() == result.value().teachers());
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
invalidRequestsAreRejectedBeforeRead()
{
    FakeReadPort readPort;
    const SubPrepPrintSourceQuery query(readPort);

    auto duplicateIds = validRequest();
    duplicateIds.selectedClassIds.push_back(duplicateIds.selectedClassIds.front());

    auto oversizedClassScope = validRequest();
    oversizedClassScope.selectedClassIds.clear();
    oversizedClassScope.selectedClassIds.reserve(
        kSubPrepPrintSourceMaxClassIds + 1
        );
    for (std::size_t index = 0;
         index <= kSubPrepPrintSourceMaxClassIds;
         ++index)
    {
        oversizedClassScope.selectedClassIds.push_back(
            classId(uniqueId('c', index))
            );
    }

    auto duplicateDays = validRequest();
    duplicateDays.selectedDays.push_back(SubPrepWeekday::Monday);

    auto oversizedDayList = validRequest();
    oversizedDayList.selectedDays = {
        SubPrepWeekday::Monday,
        SubPrepWeekday::Tuesday,
        SubPrepWeekday::Wednesday,
        SubPrepWeekday::Thursday,
        SubPrepWeekday::Friday,
        SubPrepWeekday::Saturday,
        SubPrepWeekday::Sunday,
        SubPrepWeekday::Monday
    };

    auto invalidDay = validRequest();
    invalidDay.selectedDays = {static_cast<SubPrepWeekday>(0)};

    auto invalidMode = validRequest();
    invalidMode.mode = static_cast<ScheduleViewMode>(255);

    auto blankClassId = validRequest();
    blankClassId.selectedClassIds = {classId(" \t ")};

    auto longClassId = validRequest();
    longClassId.selectedClassIds = {classId(std::string(
        kSubPrepPrintSourceMaxIdentifierLength + 1,
        'c'
        ))};

    const std::vector<SubPrepPrintSourceRequest> invalidRequests{
        duplicateIds,
        oversizedClassScope,
        duplicateDays,
        oversizedDayList,
        invalidDay,
        invalidMode,
        blankClassId,
        longClassId
    };
    for (const auto& request : invalidRequests)
    {
        verifyError(query.execute(request), ErrorCode::InvalidInput);
    }

    QCOMPARE(readPort.calls, 0);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
emptyClassesOrDaysReturnWithoutRead()
{
    FakeReadPort readPort;
    const SubPrepPrintSourceQuery query(readPort);

    auto noClasses = validRequest();
    noClasses.selectedClassIds.clear();
    auto noDays = validRequest();
    noDays.selectedDays.clear();

    for (const auto& request : {noClasses, noDays})
    {
        const auto result = query.execute(request);
        QVERIFY(result);
        QVERIFY(result.value().empty());
        QVERIFY(result.value().classes().empty());
        QVERIFY(result.value().teachers().empty());
    }

    QCOMPARE(readPort.calls, 0);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
forwardsExactScopePreservesPortOrderAndTeacherReferences()
{
    FakeReadPort readPort;
    auto request = validRequest();
    request.selectedClassIds = {
        classId("class-1"),
        classId("class-2"),
        classId("class-3")
    };
    request.selectedDays = {
        SubPrepWeekday::Friday,
        SubPrepWeekday::Monday,
        SubPrepWeekday::Wednesday
    };
    request.mode = ScheduleViewMode::Intensive;

    auto teacher2 = teacher("teacher-2");
    teacher2.englishName = "Second teacher";
    auto teacher1 = teacher("teacher-1");
    teacher1.englishName = "First teacher";
    auto class3 = printClass("class-3", teacherId("teacher-2"));
    class3.meetings = {
        meeting(SubPrepWeekday::Wednesday, "11:00", "11:45"),
        meeting(SubPrepWeekday::Friday, "10:00", "10:45")
    };
    auto class1 = printClass("class-1", teacherId("teacher-1"));
    class1.meetings = {meeting(SubPrepWeekday::Monday, "08:00", "08:45")};
    auto class2 = printClass("class-2", teacherId("teacher-2"));
    class2.meetings = {meeting(SubPrepWeekday::Wednesday, "12:00", "12:45")};

    const SubPrepPrintSourceInput expectedInput{
        {teacher2, teacher1},
        {class3, class1, class2}
    };
    readPort.result = SubPrepPrintSourceReadResult::success(expectedInput);

    const SubPrepPrintSourceQuery query(readPort);
    const auto result = query.execute(request);

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QVERIFY(readPort.lastRequest == request);
    QVERIFY(result.value().classes() == expectedInput.classes);
    QVERIFY(result.value().teachers() == expectedInput.teachers);
    QVERIFY(result.value().classes()[0].id == classId("class-3"));
    QVERIFY(
        result.value().classes()[0].teacherId
        == std::optional<TeacherId>(teacherId("teacher-2"))
        );
    QVERIFY(result.value().classes()[0].meetings == class3.meetings);
    QVERIFY(result.value().classes()[1].id == classId("class-1"));
    QCOMPARE(result.value().teachers().size(), std::size_t(2));
    QCOMPARE(result.value().teachers()[0].id, teacherId("teacher-2"));
    QCOMPARE(result.value().teachers()[1].id, teacherId("teacher-1"));
    QVERIFY(result.value().classes()[2].teacherId == teacherId("teacher-2"));
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
propagatesReadPortFailure()
{
    FakeReadPort readPort;
    const OperationError expected{
        .code = ErrorCode::Technical,
        .message = "Sub Prep print source read failed",
        .recoverable = true
    };
    readPort.result = SubPrepPrintSourceReadResult::failure(expected);

    const SubPrepPrintSourceQuery query(readPort);
    const auto result = query.execute(validRequest());

    verifyError(result, expected.code);
    QCOMPARE(result.error().message, expected.message);
    QCOMPARE(result.error().recoverable, expected.recoverable);
    QCOMPARE(readPort.calls, 1);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
acceptsMissingTeacherAndSubsetOfRequestedClasses()
{
    FakeReadPort readPort;
    auto classWithoutTeacher = printClass("class-2", std::nullopt);
    classWithoutTeacher.meetings = {
        meeting(SubPrepWeekday::Wednesday)
    };
    const SubPrepPrintSourceInput subset{
        {},
        {classWithoutTeacher}
    };
    readPort.result = SubPrepPrintSourceReadResult::success(subset);

    const SubPrepPrintSourceQuery query(readPort);
    const auto result = query.execute(validRequest());

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QVERIFY(result.value().classes() == subset.classes);
    QVERIFY(!result.value().classes().front().teacherId.has_value());
    QVERIFY(result.value().teachers().empty());
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
acceptsEmptyOptionalTextFields()
{
    auto input = validInput();
    auto& sourceTeacher = input.teachers.front();
    sourceTeacher.englishName.clear();
    sourceTeacher.room.clear();
    sourceTeacher.wifiName.clear();
    sourceTeacher.wifiPassword.clear();
    sourceTeacher.internetType.clear();
    sourceTeacher.zoomId.clear();
    sourceTeacher.zoomPassword.clear();
    sourceTeacher.projectionType.clear();
    sourceTeacher.teacherNotes.clear();

    auto& sourceClass = input.classes.front();
    sourceClass.grade.clear();
    sourceClass.level.clear();
    sourceClass.classNotes.clear();
    sourceClass.classColor.clear();
    sourceClass.fontColor.clear();
    sourceClass.studentCount = 0;
    sourceClass.meetings = {meeting(SubPrepWeekday::Monday, "", "")};

    FakeReadPort readPort;
    readPort.result = SubPrepPrintSourceReadResult::success(input);
    const SubPrepPrintSourceQuery query(readPort);
    const auto result = query.execute(validRequest());

    QVERIFY(result);
    QVERIFY(result.value().teachers() == input.teachers);
    QVERIFY(result.value().classes() == input.classes);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
rejectsOutOfScopeAndDuplicateClassesAllOrNothing()
{
    FakeReadPort readPort;
    const SubPrepPrintSourceQuery query(readPort);

    auto outOfScope = validInput();
    outOfScope.classes.front().id = classId("class-outside-scope");
    readPort.result = SubPrepPrintSourceReadResult::success(outOfScope);
    verifyValidationError(query.execute(validRequest()));

    auto duplicate = validInput();
    duplicate.classes.push_back(duplicate.classes.front());
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(duplicate));
    verifyValidationError(query.execute(validRequest()));

    QCOMPARE(readPort.calls, 2);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
rejectsMalformedIdentifiersAndMissingOrUnusedTeachers()
{
    FakeReadPort readPort;
    const SubPrepPrintSourceQuery query(readPort);

    auto malformedClassId = validInput();
    malformedClassId.classes.front().id = classId(" \t ");
    readPort.result = SubPrepPrintSourceReadResult::success(malformedClassId);
    verifyValidationError(query.execute(validRequest()));

    malformedClassId = validInput();
    malformedClassId.classes.front().id = classId(std::string(
        kSubPrepPrintSourceMaxIdentifierLength + 1,
        'c'
        ));
    readPort.result = SubPrepPrintSourceReadResult::success(malformedClassId);
    verifyValidationError(query.execute(validRequest()));

    auto malformedTeacherId = validInput();
    malformedTeacherId.teachers.front().id = teacherId(" \t ");
    readPort.result = SubPrepPrintSourceReadResult::success(malformedTeacherId);
    verifyValidationError(query.execute(validRequest()));

    malformedTeacherId = validInput();
    malformedTeacherId.teachers.front().id = teacherId(std::string(
        kSubPrepPrintSourceMaxIdentifierLength + 1,
        't'
        ));
    malformedTeacherId.classes.front().teacherId =
        malformedTeacherId.teachers.front().id;
    readPort.result = SubPrepPrintSourceReadResult::success(malformedTeacherId);
    verifyValidationError(query.execute(validRequest()));

    auto malformedClassTeacherReference = validInput();
    malformedClassTeacherReference.classes.front().teacherId =
        teacherId(" \t ");
    readPort.result = SubPrepPrintSourceReadResult::success(
        malformedClassTeacherReference
        );
    verifyValidationError(query.execute(validRequest()));

    auto missingTeacher = validInput();
    missingTeacher.classes.front().teacherId = teacherId("missing-teacher");
    readPort.result = SubPrepPrintSourceReadResult::success(missingTeacher);
    verifyValidationError(query.execute(validRequest()));

    auto unusedTeacher = validInput();
    unusedTeacher.teachers.push_back(teacher("teacher-unused"));
    readPort.result = SubPrepPrintSourceReadResult::success(unusedTeacher);
    verifyValidationError(query.execute(validRequest()));

    auto overlongClassTeacherReference = validInput();
    overlongClassTeacherReference.classes.front().teacherId = teacherId(
        std::string(kSubPrepPrintSourceMaxIdentifierLength + 1, 't')
        );
    readPort.result = SubPrepPrintSourceReadResult::success(
        overlongClassTeacherReference
        );
    verifyValidationError(query.execute(validRequest()));

    QCOMPARE(readPort.calls, 8);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
rejectsDuplicateTeacherIdsAndInvalidMeetingWeekdays()
{
    FakeReadPort readPort;
    const SubPrepPrintSourceQuery query(readPort);

    auto duplicateTeacher = validInput();
    duplicateTeacher.teachers.push_back(duplicateTeacher.teachers.front());
    readPort.result = SubPrepPrintSourceReadResult::success(duplicateTeacher);
    verifyValidationError(query.execute(validRequest()));

    auto invalidWeekday = validInput();
    invalidWeekday.classes.front().meetings.front().weekday =
        static_cast<SubPrepWeekday>(0);
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(invalidWeekday));
    verifyValidationError(query.execute(validRequest()));

    QCOMPARE(readPort.calls, 2);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
rejectsMeetingsOutsideSelectedDaysAllOrNothing()
{
    FakeReadPort readPort;
    auto input = validInput();
    input.classes.front().meetings = {
        meeting(SubPrepWeekday::Monday),
        meeting(SubPrepWeekday::Tuesday)
    };
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(input));

    const SubPrepPrintSourceQuery query(readPort);
    const auto result = query.execute(validRequest());

    verifyValidationError(result);
    QCOMPARE(readPort.calls, 1);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
acceptsMaximumBoundaryTextLengths()
{
    const std::string classIdentifier = std::string(
        kSubPrepPrintSourceMaxIdentifierLength,
        'c'
        );
    const std::string teacherIdentifier = std::string(
        kSubPrepPrintSourceMaxIdentifierLength,
        't'
        );
    auto expectedTeacher = teacher(teacherIdentifier);
    expectedTeacher.englishName = std::string(
        kSubPrepPrintSourceMaxEnglishNameLength,
        'n'
        );
    expectedTeacher.room = std::string(kSubPrepPrintSourceMaxRoomLength, 'r');
    expectedTeacher.wifiName = std::string(kSubPrepPrintSourceMaxWifiNameLength, 'w');
    expectedTeacher.wifiPassword = std::string(
        kSubPrepPrintSourceMaxWifiPasswordLength,
        'p'
        );
    expectedTeacher.internetType = std::string(
        kSubPrepPrintSourceMaxInternetTypeLength,
        'i'
        );
    expectedTeacher.zoomId = std::string(kSubPrepPrintSourceMaxZoomIdLength, 'z');
    expectedTeacher.zoomPassword = std::string(
        kSubPrepPrintSourceMaxZoomPasswordLength,
        'x'
        );
    expectedTeacher.projectionType = std::string(
        kSubPrepPrintSourceMaxProjectionTypeLength,
        'j'
        );
    expectedTeacher.teacherNotes = std::string(
        kSubPrepPrintSourceMaxTeacherNotesLength,
        't'
        );

    auto expectedClass = printClass(
        classIdentifier,
        teacherId(teacherIdentifier)
        );
    expectedClass.grade = std::string(kSubPrepPrintSourceMaxGradeLength, 'g');
    expectedClass.level = std::string(kSubPrepPrintSourceMaxLevelLength, 'l');
    expectedClass.classNotes = std::string(
        kSubPrepPrintSourceMaxClassNotesLength,
        'c'
        );
    expectedClass.classColor = std::string(kSubPrepPrintSourceMaxColorLength, 'a');
    expectedClass.fontColor = std::string(kSubPrepPrintSourceMaxColorLength, 'f');
    expectedClass.studentCount = kSubPrepPrintSourceMaxStudentCount;
    expectedClass.meetings = {
        meeting(
            SubPrepWeekday::Monday,
            std::string(kSubPrepPrintSourceMaxMeetingTimeLength, 's'),
            std::string(kSubPrepPrintSourceMaxMeetingTimeLength, 'e')
            )
    };

    auto request = validRequest();
    request.selectedClassIds = {classId(classIdentifier)};
    request.selectedDays = {
        SubPrepWeekday::Monday,
        SubPrepWeekday::Tuesday,
        SubPrepWeekday::Wednesday,
        SubPrepWeekday::Thursday,
        SubPrepWeekday::Friday,
        SubPrepWeekday::Saturday,
        SubPrepWeekday::Sunday
    };
    const SubPrepPrintSourceInput expectedInput{
        {expectedTeacher},
        {expectedClass}
    };

    FakeReadPort readPort;
    readPort.result = SubPrepPrintSourceReadResult::success(expectedInput);
    const SubPrepPrintSourceQuery query(readPort);
    const auto result = query.execute(request);

    QVERIFY(result);
    QVERIFY(result.value().teachers() == expectedInput.teachers);
    QVERIFY(result.value().classes() == expectedInput.classes);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
rejectsMalformedAndOverBoundTextAllOrNothing()
{
    using Mutator = std::function<void(SubPrepPrintSourceInput&, std::size_t)>;
    const std::vector<std::pair<const char*, Mutator>> oversizedFields{
        {
            "teacher English name",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].englishName = std::string(length, 'n');
            }
        },
        {
            "teacher room",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].room = std::string(length, 'r');
            }
        },
        {
            "teacher Wi-Fi name",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].wifiName = std::string(length, 'w');
            }
        },
        {
            "teacher Wi-Fi password",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].wifiPassword = std::string(length, 'p');
            }
        },
        {
            "teacher internet type",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].internetType = std::string(length, 'i');
            }
        },
        {
            "teacher Zoom ID",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].zoomId = std::string(length, 'z');
            }
        },
        {
            "teacher Zoom password",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].zoomPassword = std::string(length, 'x');
            }
        },
        {
            "teacher projection type",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].projectionType = std::string(length, 'j');
            }
        },
        {
            "teacher notes",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.teachers[0].teacherNotes = std::string(length, 't');
            }
        },
        {
            "class grade",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.classes[0].grade = std::string(length, 'g');
            }
        },
        {
            "class level",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.classes[0].level = std::string(length, 'l');
            }
        },
        {
            "class notes",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.classes[0].classNotes = std::string(length, 'c');
            }
        },
        {
            "class color",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.classes[0].classColor = std::string(length, 'a');
            }
        },
        {
            "class font color",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.classes[0].fontColor = std::string(length, 'f');
            }
        },
        {
            "meeting start time",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.classes[0].meetings[0].startTime = std::string(length, 's');
            }
        },
        {
            "meeting end time",
            [](SubPrepPrintSourceInput& input, const std::size_t length)
            {
                input.classes[0].meetings[0].endTime = std::string(length, 'e');
            }
        }
    };
    const std::vector<std::size_t> maximumLengths{
        kSubPrepPrintSourceMaxEnglishNameLength,
        kSubPrepPrintSourceMaxRoomLength,
        kSubPrepPrintSourceMaxWifiNameLength,
        kSubPrepPrintSourceMaxWifiPasswordLength,
        kSubPrepPrintSourceMaxInternetTypeLength,
        kSubPrepPrintSourceMaxZoomIdLength,
        kSubPrepPrintSourceMaxZoomPasswordLength,
        kSubPrepPrintSourceMaxProjectionTypeLength,
        kSubPrepPrintSourceMaxTeacherNotesLength,
        kSubPrepPrintSourceMaxGradeLength,
        kSubPrepPrintSourceMaxLevelLength,
        kSubPrepPrintSourceMaxClassNotesLength,
        kSubPrepPrintSourceMaxColorLength,
        kSubPrepPrintSourceMaxColorLength,
        kSubPrepPrintSourceMaxMeetingTimeLength,
        kSubPrepPrintSourceMaxMeetingTimeLength
    };

    FakeReadPort readPort;
    const SubPrepPrintSourceQuery query(readPort);
    for (std::size_t index = 0; index < oversizedFields.size(); ++index)
    {
        auto input = validInput();
        oversizedFields[index].second(input, maximumLengths[index] + 1);
        readPort.result = SubPrepPrintSourceReadResult::success(std::move(input));
        const auto result = query.execute(validRequest());
        QVERIFY2(!result, oversizedFields[index].first);
        QCOMPARE(result.error().code, ErrorCode::Validation);
        QVERIFY(!result.hasValue());
    }

    auto blankTeacher = validInput();
    blankTeacher.teachers[0].englishName = " \t ";
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(blankTeacher));
    verifyValidationError(query.execute(validRequest()));

    auto blankClass = validInput();
    blankClass.classes[0].grade = " \t ";
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(blankClass));
    verifyValidationError(query.execute(validRequest()));

    auto blankMeeting = validInput();
    blankMeeting.classes[0].meetings[0].startTime = " \t ";
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(blankMeeting));
    verifyValidationError(query.execute(validRequest()));

    QCOMPARE(readPort.calls, static_cast<int>(oversizedFields.size() + 3));
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
acceptsMaximumMeetingsPerClass()
{
    auto input = validInput();
    auto& meetings = input.classes.front().meetings;
    meetings.clear();
    meetings.reserve(kSubPrepPrintSourceMaxMeetingsPerClass);
    for (std::size_t index = 0;
         index < kSubPrepPrintSourceMaxMeetingsPerClass;
         ++index)
    {
        meetings.push_back(meeting(SubPrepWeekday::Monday));
    }

    FakeReadPort readPort;
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(input));
    const SubPrepPrintSourceQuery query(readPort);
    const auto result = query.execute(validRequest());

    QVERIFY(result);
    QCOMPARE(
        result.value().classes().front().meetings.size(),
        kSubPrepPrintSourceMaxMeetingsPerClass
        );
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
acceptsMaximumAggregateCounts()
{
    SubPrepPrintSourceRequest request;
    request.selectedClassIds.reserve(kSubPrepPrintSourceMaxClassIds);
    request.selectedDays = {
        SubPrepWeekday::Monday,
        SubPrepWeekday::Tuesday,
        SubPrepWeekday::Wednesday,
        SubPrepWeekday::Thursday,
        SubPrepWeekday::Friday,
        SubPrepWeekday::Saturday,
        SubPrepWeekday::Sunday
    };

    SubPrepPrintSourceInput input;
    input.teachers.reserve(kSubPrepPrintSourceMaxTeachers);
    input.classes.reserve(kSubPrepPrintSourceMaxClasses);
    for (std::size_t index = 0;
         index < kSubPrepPrintSourceMaxClasses;
         ++index)
    {
        const auto classIdentifier = uniqueId('c', index);
        const auto teacherIdentifier = uniqueId('t', index);
        request.selectedClassIds.push_back(classId(classIdentifier));
        input.teachers.push_back(teacher(teacherIdentifier));
        auto classRecord = printClass(
            classIdentifier,
            teacherId(teacherIdentifier)
            );
        classRecord.meetings.clear();
        for (const auto day : {
                 SubPrepWeekday::Monday,
                 SubPrepWeekday::Tuesday,
                 SubPrepWeekday::Wednesday,
                 SubPrepWeekday::Thursday
             })
        {
            classRecord.meetings.push_back(meeting(day));
        }
        input.classes.push_back(std::move(classRecord));
    }

    const auto expectedMeetings = kSubPrepPrintSourceMaxClasses * 4;
    QCOMPARE(expectedMeetings, kSubPrepPrintSourceMaxMeetings);

    FakeReadPort readPort;
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(input));
    const SubPrepPrintSourceQuery query(readPort);
    const auto result = query.execute(request);

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QCOMPARE(result.value().classes().size(), kSubPrepPrintSourceMaxClasses);
    QCOMPARE(result.value().teachers().size(), kSubPrepPrintSourceMaxTeachers);
    std::size_t totalMeetings = 0;
    for (const auto& classRecord : result.value().classes())
    {
        totalMeetings += classRecord.meetings.size();
    }
    QCOMPARE(totalMeetings, kSubPrepPrintSourceMaxMeetings);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
rejectsOverBoundCountsAllOrNothing()
{
    FakeReadPort readPort;
    const SubPrepPrintSourceQuery query(readPort);

    auto request = validRequest();
    request.selectedClassIds.clear();
    request.selectedClassIds.reserve(kSubPrepPrintSourceMaxClassIds + 1);
    for (std::size_t index = 0;
         index <= kSubPrepPrintSourceMaxClassIds;
         ++index)
    {
        request.selectedClassIds.push_back(classId(uniqueId('c', index)));
    }
    verifyError(query.execute(request), ErrorCode::InvalidInput);
    QCOMPARE(readPort.calls, 0);

    auto tooManyTeachers = validInput();
    tooManyTeachers.teachers.clear();
    tooManyTeachers.teachers.reserve(kSubPrepPrintSourceMaxTeachers + 1);
    for (std::size_t index = 0;
         index <= kSubPrepPrintSourceMaxTeachers;
         ++index)
    {
        tooManyTeachers.teachers.push_back(
            teacher(uniqueId('t', index))
            );
    }
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(tooManyTeachers));
    verifyValidationError(query.execute(validRequest()));

    auto tooManyClasses = SubPrepPrintSourceInput{};
    tooManyClasses.classes.reserve(kSubPrepPrintSourceMaxClasses + 1);
    for (std::size_t index = 0;
         index <= kSubPrepPrintSourceMaxClasses;
         ++index)
    {
        tooManyClasses.classes.push_back(
            printClass(uniqueId('c', index), std::nullopt)
            );
    }
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(tooManyClasses));
    verifyValidationError(query.execute(validRequest()));

    auto tooManyMeetingsPerClass = validInput();
    tooManyMeetingsPerClass.classes.front().meetings.assign(
        kSubPrepPrintSourceMaxMeetingsPerClass + 1,
        meeting()
        );
    readPort.result = SubPrepPrintSourceReadResult::success(
        std::move(tooManyMeetingsPerClass)
        );
    verifyValidationError(query.execute(validRequest()));

    SubPrepPrintSourceRequest manyMeetingRequest;
    manyMeetingRequest.selectedDays = {
        SubPrepWeekday::Monday,
        SubPrepWeekday::Tuesday,
        SubPrepWeekday::Wednesday,
        SubPrepWeekday::Thursday,
        SubPrepWeekday::Friday,
        SubPrepWeekday::Saturday,
        SubPrepWeekday::Sunday
    };
    SubPrepPrintSourceInput tooManyMeetings;
    constexpr std::size_t fullClasses = kSubPrepPrintSourceMaxMeetings / 64;
    constexpr std::size_t finalClassMeetings =
        (kSubPrepPrintSourceMaxMeetings % 64) + 1;
    for (std::size_t index = 0; index < fullClasses; ++index)
    {
        const auto id = uniqueId('c', index);
        manyMeetingRequest.selectedClassIds.push_back(classId(id));
        auto classRecord = printClass(id, std::nullopt);
        classRecord.meetings.assign(64, meeting());
        tooManyMeetings.classes.push_back(std::move(classRecord));
    }
    const auto finalId = uniqueId('c', fullClasses);
    manyMeetingRequest.selectedClassIds.push_back(classId(finalId));
    auto finalClass = printClass(finalId, std::nullopt);
    finalClass.meetings.assign(finalClassMeetings, meeting());
    tooManyMeetings.classes.push_back(std::move(finalClass));
    QCOMPARE(
        fullClasses * 64 + finalClassMeetings,
        kSubPrepPrintSourceMaxMeetings + 1
        );
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(tooManyMeetings));
    verifyValidationError(query.execute(manyMeetingRequest));

    auto tooManyStudents = validInput();
    tooManyStudents.classes.front().studentCount =
        kSubPrepPrintSourceMaxStudentCount + 1;
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(tooManyStudents));
    verifyValidationError(query.execute(validRequest()));

    QCOMPARE(readPort.calls, 5);
}

void ClassMngrNextApplicationSubPrepPrintSourceQueryTests::
repeatedCallsReadThroughAndKeepPreviousResultIndependent()
{
    FakeReadPort readPort;
    const auto requested = validRequest();
    readPort.result = SubPrepPrintSourceReadResult::success(validInput());
    const SubPrepPrintSourceQuery query(readPort);

    const auto firstResult = query.execute(requested);
    QVERIFY(firstResult);
    QCOMPARE(firstResult.value().classes().front().classNotes, std::string("Class notes"));

    auto updatedInput = validInput();
    updatedInput.classes.front().classNotes = "updated class notes";
    updatedInput.teachers.front().teacherNotes = "updated teacher notes";
    readPort.result = SubPrepPrintSourceReadResult::success(std::move(updatedInput));
    const auto secondResult = query.execute(requested);

    QVERIFY(secondResult);
    QCOMPARE(secondResult.value().classes().front().classNotes, std::string("updated class notes"));
    QCOMPARE(
        secondResult.value().teachers().front().teacherNotes,
        std::string("updated teacher notes")
        );
    QCOMPARE(firstResult.value().classes().front().classNotes, std::string("Class notes"));
    QCOMPARE(firstResult.value().teachers().front().teacherNotes, std::string("Teacher notes"));
    QCOMPARE(readPort.calls, 2);
    QVERIFY(readPort.lastRequest == requested);
}

QTEST_APPLESS_MAIN(ClassMngrNextApplicationSubPrepPrintSourceQueryTests)

#include "next_application_sub_prep_print_source_query_tests.moc"
