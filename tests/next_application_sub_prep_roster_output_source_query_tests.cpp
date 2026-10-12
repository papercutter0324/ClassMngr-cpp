#include "next/application/sub_prep_roster_output_source_query.h"

#include <QtTest/QtTest>

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
    return *ClassId::fromString(std::move(value));
}

TeacherId teacherId(std::string value)
{
    return *TeacherId::fromString(std::move(value));
}

SubPrepRosterOutputSourceRequest validRequest()
{
    return {
        {classId("class-1"), classId("class-2")},
        {SubPrepWeekday::Monday, SubPrepWeekday::Wednesday},
        ScheduleViewMode::Regular,
        {"Notes"}
    };
}

SubPrepRosterOutputTeacher validTeacher()
{
    return {
        teacherId("teacher-1"),
        "Teacher One",
        "선생님",
        "Preferred One",
        "Preferred Romanization"
    };
}

SubPrepRosterOutputClass validClass(
    std::string id = "class-1",
    std::optional<TeacherId> assignedTeacher = teacherId("teacher-1")
    )
{
    return {
        .id = classId(std::move(id)),
        .teacherId = std::move(assignedTeacher),
        .classroomName = "Room Alpha",
        .grade = "Grade 4",
        .level = "Level B",
        .classTeacherEnglishName = "Teacher One",
        .classTeacherKoreanName = "선생님",
        .room = "Room 2",
        .wifiName = "Network",
        .wifiPassword = "Password",
        .zoomId = "zoom-1",
        .zoomPassword = "zoom-pass",
        .meetings = {
            {SubPrepWeekday::Monday, "09:00", "09:45"},
            {SubPrepWeekday::Wednesday, "10:00", "10:45"}
        },
        .rosterColumns = {"English", "Korean", "Notes"},
        .rosterRows = {
            {"Student One", "학생 하나", "First row"},
            {"Student Two", "학생 둘", "Second row"}
        }
    };
}

SubPrepRosterOutputSourceInput validInput()
{
    return {
        {validTeacher()},
        {validClass()}
    };
}

std::string uniqueId(const char prefix, const std::size_t index)
{
    const auto suffix = std::to_string(index);
    return std::string(
               kSubPrepPrintSourceMaxIdentifierLength - suffix.size(),
               prefix
               )
        + suffix;
}

class FakeReadPort final : public SubPrepRosterOutputSourceReadPort
{
public:
    std::optional<SubPrepRosterOutputSourceReadResult> result;
    int calls = 0;
    std::optional<SubPrepRosterOutputSourceRequest> lastRequest;

    [[nodiscard]] SubPrepRosterOutputSourceReadResult loadSource(
        const SubPrepRosterOutputSourceRequest& request
        ) override
    {
        ++calls;
        lastRequest = request;
        if (!result.has_value())
        {
            return SubPrepRosterOutputSourceReadResult::failure(
                OperationError{
                    .code = ErrorCode::Technical,
                    .message = "no source configured",
                    .recoverable = false
                }
                );
        }
        return std::move(*result);
    }
};

void setInput(
    FakeReadPort& port,
    SubPrepRosterOutputSourceInput input
    )
{
    port.result.emplace(
        SubPrepRosterOutputSourceReadResult::success(std::move(input))
        );
}

void verifyError(
    const SubPrepRosterOutputSourceQueryResult& result,
    const ErrorCode expectedCode
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, expectedCode);
}

} // namespace

class ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void requestAndSourceOwnTypedBoundedValues();
    void invalidRequestsAreRejectedBeforeRead();
    void invalidWeekdaysAreRejectedBeforeRead();
    void acceptsMaximumIdentifierLength();
    void emptyScopeReturnsWithoutRead();
    void forwardsExactScopeAndPreservesSourceOrder();
    void propagatesReadFailure();
    void rejectsOutOfScopeDuplicateAndMalformedTeacherData();
    void rejectsInvalidAndUnselectedMeetingWeekdays();
    void rejectsUnrequestedColumnsAndMalformedRows();
    void acceptsMaximumPerClassRowsAndColumns();
    void rejectsAggregateRowCellAndTextOverflow();
};

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
requestAndSourceOwnTypedBoundedValues()
{
    static_assert(std::is_same_v<
        decltype(SubPrepRosterOutputSourceRequest::selectedClassIds),
        std::vector<ClassId>
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepRosterOutputSourceRequest::selectedDays),
        std::vector<SubPrepWeekday>
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepRosterOutputClass::rosterRows),
        std::vector<std::vector<std::string>>
        >);

    FakeReadPort readPort;
    setInput(readPort, validInput());
    SubPrepRosterOutputSourceQuery query(readPort);
    const auto result = query.execute(validRequest());

    QVERIFY(result);
    QCOMPARE(result.value().classes().size(), std::size_t(1));
    QCOMPARE(result.value().classes().front(), validClass());
    QCOMPARE(result.value().teachers().front(), validTeacher());
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
invalidRequestsAreRejectedBeforeRead()
{
    const auto verifyInvalid = [](SubPrepRosterOutputSourceRequest request)
    {
        FakeReadPort readPort;
        SubPrepRosterOutputSourceQuery query(readPort);
        const auto result = query.execute(request);
        verifyError(result, ErrorCode::InvalidInput);
        QCOMPARE(readPort.calls, 0);
    };

    auto request = validRequest();
    request.selectedClassIds.push_back(request.selectedClassIds.front());
    verifyInvalid(std::move(request));

    request = validRequest();
    request.selectedDays.push_back(request.selectedDays.front());
    verifyInvalid(std::move(request));

    request = validRequest();
    request.mode = static_cast<ScheduleViewMode>(255);
    {
        FakeReadPort readPort;
        SubPrepRosterOutputSourceQuery query(readPort);
        const auto result = query.execute(request);
        verifyError(result, ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("The roster-output schedule mode is invalid.")
            );
        QVERIFY(!result.error().recoverable);
        QCOMPARE(readPort.calls, 0);
    }

    request = validRequest();
    request.selectedExtraColumns = {"Notes", "Notes"};
    verifyInvalid(std::move(request));

    request = validRequest();
    request.selectedClassIds.clear();
    for (std::size_t index = 0;
         index <= kSubPrepPrintSourceMaxClassIds;
         ++index)
    {
        request.selectedClassIds.push_back(
            classId(uniqueId('c', index))
            );
    }
    verifyInvalid(std::move(request));

    const auto verifyMalformedSelectedClassId = [](const std::string& id)
    {
        FakeReadPort readPort;
        auto malformedRequest = validRequest();
        malformedRequest.selectedClassIds = {classId(id)};
        SubPrepRosterOutputSourceQuery query(readPort);
        const auto result = query.execute(malformedRequest);

        verifyError(result, ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string(
                "Selected roster-output class identifiers must be non-blank and bounded."
                )
            );
        QVERIFY(!result.error().recoverable);
        QCOMPARE(readPort.calls, 0);
    };
    verifyMalformedSelectedClassId(" \t ");
    verifyMalformedSelectedClassId(std::string(
        kSubPrepPrintSourceMaxIdentifierLength + 1,
        'c'
        ));
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
invalidWeekdaysAreRejectedBeforeRead()
{
    for (const auto weekday : {0, 8})
    {
        FakeReadPort readPort;
        auto request = validRequest();
        request.selectedDays = {static_cast<SubPrepWeekday>(weekday)};

        SubPrepRosterOutputSourceQuery query(readPort);
        const auto result = query.execute(request);

        verifyError(result, ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("A selected roster-output weekday is invalid.")
            );
        QVERIFY(!result.error().recoverable);
        QCOMPARE(readPort.calls, 0);
    }
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
acceptsMaximumIdentifierLength()
{
    const auto maximumClassId = std::string(
        kSubPrepPrintSourceMaxIdentifierLength,
        'c'
        );
    const auto maximumTeacherId = std::string(
        kSubPrepPrintSourceMaxIdentifierLength,
        't'
        );
    auto request = validRequest();
    request.selectedClassIds = {classId(maximumClassId)};

    auto input = validInput();
    input.teachers.front().id = teacherId(maximumTeacherId);
    input.classes.front().id = classId(maximumClassId);
    input.classes.front().teacherId = teacherId(maximumTeacherId);

    FakeReadPort readPort;
    setInput(readPort, std::move(input));
    SubPrepRosterOutputSourceQuery query(readPort);
    const auto result = query.execute(request);

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QCOMPARE(result.value().classes().size(), std::size_t(1));
    QCOMPARE(result.value().classes().front().id, classId(maximumClassId));
    QCOMPARE(result.value().teachers().size(), std::size_t(1));
    QCOMPARE(result.value().teachers().front().id, teacherId(maximumTeacherId));
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
emptyScopeReturnsWithoutRead()
{
    FakeReadPort readPort;
    SubPrepRosterOutputSourceQuery query(readPort);

    auto request = validRequest();
    request.selectedClassIds.clear();
    auto result = query.execute(request);
    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(readPort.calls, 0);

    request = validRequest();
    request.selectedDays.clear();
    result = query.execute(request);
    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(readPort.calls, 0);
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
forwardsExactScopeAndPreservesSourceOrder()
{
    auto input = validInput();
    input.classes = {
        validClass("class-2", teacherId("teacher-1")),
        validClass("class-1", teacherId("teacher-1"))
    };

    FakeReadPort readPort;
    setInput(readPort, std::move(input));
    SubPrepRosterOutputSourceQuery query(readPort);
    const auto request = validRequest();
    const auto result = query.execute(request);

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QVERIFY(readPort.lastRequest == request);
    QCOMPARE(result.value().classes().size(), std::size_t(2));
    QCOMPARE(result.value().classes().at(0).id, classId("class-2"));
    QCOMPARE(result.value().classes().at(1).id, classId("class-1"));
    QCOMPARE(result.value().classes().at(0).meetings.size(), std::size_t(2));
    QCOMPARE(result.value().classes().at(0).rosterRows.at(1).at(2), std::string("Second row"));
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
propagatesReadFailure()
{
    FakeReadPort readPort;
    readPort.result.emplace(
        SubPrepRosterOutputSourceReadResult::failure(
            OperationError{
                .code = ErrorCode::NotFound,
                .message = "selected class was removed",
                .recoverable = true
            }
            )
        );
    SubPrepRosterOutputSourceQuery query(readPort);
    const auto result = query.execute(validRequest());

    verifyError(result, ErrorCode::NotFound);
    QCOMPARE(result.error().message, std::string("selected class was removed"));
    QCOMPARE(readPort.calls, 1);
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
rejectsOutOfScopeDuplicateAndMalformedTeacherData()
{
    const auto verifyInvalidSource = [](
                                  SubPrepRosterOutputSourceInput input
                                  )
    {
        FakeReadPort readPort;
        setInput(readPort, std::move(input));
        SubPrepRosterOutputSourceQuery query(readPort);
        const auto result = query.execute(validRequest());
        verifyError(result, ErrorCode::Validation);
        QCOMPARE(readPort.calls, 1);
    };

    auto input = validInput();
    input.classes.front().id = classId("class-outside");
    verifyInvalidSource(std::move(input));

    input = validInput();
    input.classes.push_back(input.classes.front());
    verifyInvalidSource(std::move(input));

    input = validInput();
    input.teachers.push_back(validTeacher());
    verifyInvalidSource(std::move(input));

    input = validInput();
    input.classes.front().teacherId = teacherId("missing-teacher");
    verifyInvalidSource(std::move(input));

    input = validInput();
    input.classes.front().teacherId.reset();
    verifyInvalidSource(std::move(input));

    const auto verifyMalformedIdentifier = [](
                                           SubPrepRosterOutputSourceInput input,
                                           const char* expectedMessage
                                           )
    {
        FakeReadPort malformedReadPort;
        setInput(malformedReadPort, std::move(input));
        SubPrepRosterOutputSourceQuery malformedQuery(malformedReadPort);
        const auto result = malformedQuery.execute(validRequest());

        verifyError(result, ErrorCode::Validation);
        QCOMPARE(result.error().message, std::string(expectedMessage));
        QVERIFY(!result.error().recoverable);
        QCOMPARE(malformedReadPort.calls, 1);
    };

    auto malformedTeacher = validInput();
    const auto blankTeacherId = teacherId(" \t ");
    auto blankIdTeacher = validTeacher();
    blankIdTeacher.id = blankTeacherId;
    malformedTeacher.teachers.push_back(std::move(blankIdTeacher));
    verifyMalformedIdentifier(
        std::move(malformedTeacher),
        "A roster-output teacher identifier is malformed."
        );

    malformedTeacher = validInput();
    const auto overlongTeacherId = teacherId(std::string(
        kSubPrepPrintSourceMaxIdentifierLength + 1,
        't'
        ));
    auto overlongIdTeacher = validTeacher();
    overlongIdTeacher.id = overlongTeacherId;
    malformedTeacher.teachers.push_back(std::move(overlongIdTeacher));
    verifyMalformedIdentifier(
        std::move(malformedTeacher),
        "A roster-output teacher identifier is malformed."
        );

    auto malformedClass = validInput();
    malformedClass.classes.front().id = classId(" \t ");
    verifyMalformedIdentifier(
        std::move(malformedClass),
        "A roster-output class identifier is malformed."
        );

    malformedClass = validInput();
    malformedClass.classes.front().id = classId(std::string(
        kSubPrepPrintSourceMaxIdentifierLength + 1,
        'c'
        ));
    verifyMalformedIdentifier(
        std::move(malformedClass),
        "A roster-output class identifier is malformed."
        );

    auto malformedTeacherReference = validInput();
    malformedTeacherReference.classes.front().teacherId =
        teacherId(" \t ");
    verifyMalformedIdentifier(
        std::move(malformedTeacherReference),
        "A roster-output class teacher identifier is malformed."
        );

    malformedTeacherReference = validInput();
    malformedTeacherReference.classes.front().teacherId = teacherId(
        std::string(kSubPrepPrintSourceMaxIdentifierLength + 1, 't')
        );
    verifyMalformedIdentifier(
        std::move(malformedTeacherReference),
        "A roster-output class teacher identifier is malformed."
        );
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
rejectsInvalidAndUnselectedMeetingWeekdays()
{
    for (const auto weekday : {0, 8})
    {
        auto input = validInput();
        input.classes.front().meetings.front().weekday =
            static_cast<SubPrepWeekday>(weekday);

        FakeReadPort readPort;
        setInput(readPort, std::move(input));
        SubPrepRosterOutputSourceQuery query(readPort);
        const auto result = query.execute(validRequest());

        verifyError(result, ErrorCode::Validation);
        QCOMPARE(
            result.error().message,
            std::string("A roster-output meeting is outside the requested days.")
            );
        QVERIFY(!result.error().recoverable);
        QCOMPARE(readPort.calls, 1);
    }

    auto unselectedInput = validInput();
    unselectedInput.classes.front().meetings.front().weekday =
        SubPrepWeekday::Friday;

    FakeReadPort readPort;
    setInput(readPort, std::move(unselectedInput));
    SubPrepRosterOutputSourceQuery query(readPort);
    const auto result = query.execute(validRequest());

    verifyError(result, ErrorCode::Validation);
    QCOMPARE(
        result.error().message,
        std::string("A roster-output meeting is outside the requested days.")
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(readPort.calls, 1);
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
rejectsUnrequestedColumnsAndMalformedRows()
{
    const auto verifyInvalidSource = [](
                                  SubPrepRosterOutputSourceInput input
                                  )
    {
        FakeReadPort readPort;
        setInput(readPort, std::move(input));
        SubPrepRosterOutputSourceQuery query(readPort);
        const auto result = query.execute(validRequest());
        verifyError(result, ErrorCode::Validation);
    };

    auto input = validInput();
    input.classes.front().rosterColumns.push_back("Unused");
    input.classes.front().rosterRows.front().push_back("unrequested");
    verifyInvalidSource(std::move(input));

    input = validInput();
    input.classes.front().rosterRows.front().pop_back();
    verifyInvalidSource(std::move(input));

    input = validInput();
    input.classes.front().rosterColumns[1] = "English";
    verifyInvalidSource(std::move(input));

    input = validInput();
    input.classes.front().rosterRows.front().front() = std::string(
        kSubPrepRosterOutputMaxCellBytes + 1,
        'x'
        );
    verifyInvalidSource(std::move(input));
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
acceptsMaximumPerClassRowsAndColumns()
{
    auto request = validRequest();
    request.selectedExtraColumns.clear();
    std::vector<std::string> columns;
    columns.reserve(kSubPrepRosterOutputMaxRosterColumns);
    for (std::size_t index = 0;
         index < kSubPrepRosterOutputMaxRosterColumns;
         ++index)
    {
        auto column = "Field " + std::to_string(index);
        request.selectedExtraColumns.push_back(column);
        columns.push_back(std::move(column));
    }

    auto record = validClass("class-1", std::nullopt);
    record.rosterColumns = std::move(columns);
    record.rosterRows.clear();
    record.rosterRows.reserve(kSubPrepRosterOutputMaxRowsPerClass);
    for (std::size_t rowIndex = 0;
         rowIndex < kSubPrepRosterOutputMaxRowsPerClass;
         ++rowIndex)
    {
        record.rosterRows.emplace_back(kSubPrepRosterOutputMaxRosterColumns);
    }

    FakeReadPort readPort;
    setInput(
        readPort,
        SubPrepRosterOutputSourceInput{
            {},
            {std::move(record)}
        }
        );
    SubPrepRosterOutputSourceQuery query(readPort);
    const auto result = query.execute(request);

    QVERIFY(result);
    QCOMPARE(
        result.value().classes().front().rosterColumns.size(),
        kSubPrepRosterOutputMaxRosterColumns
        );
    QCOMPARE(
        result.value().classes().front().rosterRows.size(),
        kSubPrepRosterOutputMaxRowsPerClass
        );
}

void ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests::
rejectsAggregateRowCellAndTextOverflow()
{
    auto request = validRequest();
    request.selectedExtraColumns.clear();

    SubPrepRosterOutputSourceInput input;
    constexpr std::size_t classCount = 17;
    request.selectedClassIds.clear();
    input.classes.reserve(classCount);
    for (std::size_t classIndex = 0; classIndex < classCount; ++classIndex)
    {
        auto id = "class-" + std::to_string(classIndex);
        request.selectedClassIds.push_back(classId(id));
        auto record = validClass(std::move(id), std::nullopt);
        record.rosterColumns.clear();
        record.rosterRows.clear();
        record.rosterRows.resize(kSubPrepRosterOutputMaxRowsPerClass);
        input.classes.push_back(std::move(record));
    }

    FakeReadPort rowPort;
    setInput(rowPort, std::move(input));
    SubPrepRosterOutputSourceQuery rowQuery(rowPort);
    auto result = rowQuery.execute(request);
    verifyError(result, ErrorCode::Validation);

    request = validRequest();
    request.selectedExtraColumns.clear();
    auto cellRecord = validClass("class-1", std::nullopt);
    cellRecord.rosterColumns.clear();
    for (std::size_t index = 0;
         index < kSubPrepRosterOutputMaxRosterColumns;
         ++index)
    {
        const auto column = "Field " + std::to_string(index);
        request.selectedExtraColumns.push_back(column);
        cellRecord.rosterColumns.push_back(column);
    }
    cellRecord.rosterRows.resize(kSubPrepRosterOutputMaxRowsPerClass);
    for (auto& row : cellRecord.rosterRows)
    {
        row.resize(kSubPrepRosterOutputMaxRosterColumns);
    }
    auto secondCellRecord = cellRecord;
    secondCellRecord.id = classId("class-2");
    FakeReadPort cellPort;
    setInput(
        cellPort,
        SubPrepRosterOutputSourceInput{
            {},
            {std::move(cellRecord), std::move(secondCellRecord)}
        }
        );
    SubPrepRosterOutputSourceQuery cellQuery(cellPort);
    result = cellQuery.execute(request);
    verifyError(result, ErrorCode::Validation);

    request = validRequest();
    request.selectedExtraColumns.clear();
    auto textRecord = validClass("class-1", std::nullopt);
    textRecord.rosterColumns.clear();
    for (std::size_t index = 0; index < 128; ++index)
    {
        const auto column = "Field " + std::to_string(index);
        request.selectedExtraColumns.push_back(column);
        textRecord.rosterColumns.push_back(column);
    }
    textRecord.rosterRows.resize(16);
    for (auto& row : textRecord.rosterRows)
    {
        row.resize(128);
        for (auto& cell : row)
        {
            cell.assign(kSubPrepRosterOutputMaxCellBytes, 'x');
        }
    }
    FakeReadPort textPort;
    setInput(
        textPort,
        SubPrepRosterOutputSourceInput{{}, {std::move(textRecord)}}
        );
    SubPrepRosterOutputSourceQuery textQuery(textPort);
    result = textQuery.execute(request);
    verifyError(result, ErrorCode::Validation);
}

QTEST_APPLESS_MAIN(ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests)

#include "next_application_sub_prep_roster_output_source_query_tests.moc"
