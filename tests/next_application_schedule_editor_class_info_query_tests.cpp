#include "next/application/schedule_editor_class_info_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

ScheduleEditorClassInfoSnapshot snapshot(
    const Domain::ClassId& id,
    const std::u16string& grade = {},
    const std::u16string& level = {},
    const std::u16string& readingBook = {},
    const std::u16string& essayBook = {},
    const std::u16string& classColor = {},
    const std::u16string& fontColor = {},
    const std::u16string& teacherKoreanName = {},
    const std::u16string& roomNumber = {}
    )
{
    return {
        .classId = id,
        .classGrade = grade,
        .classLevel = level,
        .readingBook = readingBook,
        .essayBook = essayBook,
        .classColor = classColor,
        .fontColor = fontColor,
        .teacherKoreanName = teacherKoreanName,
        .roomNumber = roomNumber
    };
}

class RecordingReadPort final : public ScheduleEditorClassInfoReadPort
{
public:
    [[nodiscard]] ScheduleEditorClassInfoReadResult readScheduleEditorClassInfo(
        const Domain::ClassId& classId
        ) const override
    {
        ++callCount;
        lastClassId = classId;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Domain::ClassId> lastClassId;
    ScheduleEditorClassInfoReadResult result =
        ScheduleEditorClassInfoReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured read port",
            .recoverable = false
        });
};

}

class NextApplicationScheduleEditorClassInfoQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void compactContractUsesOwningUtf16Fields();
    void callsPortOnceWithTypedIdAndReturnsMappedSnapshot();
    void forwardsSuccessfulEmptySnapshot();
    void forwardsPortFailureExactly();
    void rejectsInvalidIdsBeforePortCall();
    void rejectsReturnedIdMismatch();
};

void NextApplicationScheduleEditorClassInfoQueryTests::
compactContractUsesOwningUtf16Fields()
{
    static_assert(std::is_same_v<
        decltype(ScheduleEditorClassInfoSnapshot::classGrade),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(ScheduleEditorClassInfoSnapshot::classLevel),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(ScheduleEditorClassInfoSnapshot::readingBook),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(ScheduleEditorClassInfoSnapshot::essayBook),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(ScheduleEditorClassInfoSnapshot::classColor),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(ScheduleEditorClassInfoSnapshot::fontColor),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(ScheduleEditorClassInfoSnapshot::teacherKoreanName),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(ScheduleEditorClassInfoSnapshot::roomNumber),
        std::u16string
        >);

    const Domain::ClassId id = classId("42");
    const ScheduleEditorClassInfoSnapshot value = snapshot(
        id,
        u"E4",
        u"Theseus",
        u"Reading Explorer 1",
        u"4A",
        u"#123456",
        u"#654321",
        u"\uAE40\uC120\uC0DD",
        u"308"
        );
    QVERIFY(value.classId == id);
    QCOMPARE(
        value.teacherKoreanName,
        std::u16string(u"\uAE40\uC120\uC0DD"));
    QCOMPARE(value.roomNumber, std::u16string(u"308"));
}

void NextApplicationScheduleEditorClassInfoQueryTests::
callsPortOnceWithTypedIdAndReturnsMappedSnapshot()
{
    RecordingReadPort port;
    const Domain::ClassId id = classId("42");
    const ScheduleEditorClassInfoSnapshot expected = snapshot(
        id,
        u"E4",
        u"Theseus",
        u"Reading Explorer 1",
        u"4A",
        u"#123456",
        u"#654321",
        u"\uAE40\uC120\uC0DD",
        u"308"
        );
    port.result = ScheduleEditorClassInfoReadResult::success(expected);

    const auto result = ScheduleEditorClassInfoQuery::execute(id, port);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastClassId.has_value());
    QVERIFY(*port.lastClassId == id);
    QVERIFY(result.value() == expected);
}

void NextApplicationScheduleEditorClassInfoQueryTests::
forwardsSuccessfulEmptySnapshot()
{
    RecordingReadPort port;
    const Domain::ClassId id = classId("7");
    const ScheduleEditorClassInfoSnapshot empty = snapshot(id);
    port.result = ScheduleEditorClassInfoReadResult::success(empty);

    const auto result = ScheduleEditorClassInfoQuery::execute(id, port);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value() == empty);
}

void NextApplicationScheduleEditorClassInfoQueryTests::
forwardsPortFailureExactly()
{
    RecordingReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "class details read rejected",
        .recoverable = true
    };
    port.result = ScheduleEditorClassInfoReadResult::failure(expected);

    const auto result =
        ScheduleEditorClassInfoQuery::execute(classId("42"), port);

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

void NextApplicationScheduleEditorClassInfoQueryTests::
rejectsInvalidIdsBeforePortCall()
{
    RecordingReadPort port;
    const std::vector<Domain::ClassId> invalidIds{
        classId("0"),
        classId("042"),
        classId("42x")
    };

    for (const Domain::ClassId& id : invalidIds)
    {
        const auto result = ScheduleEditorClassInfoQuery::execute(id, port);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastClassId.has_value());
}

void NextApplicationScheduleEditorClassInfoQueryTests::
rejectsReturnedIdMismatch()
{
    RecordingReadPort port;
    port.result = ScheduleEditorClassInfoReadResult::success(
        snapshot(classId("43"), u"E4", u"Theseus")
        );

    const auto result =
        ScheduleEditorClassInfoQuery::execute(classId("42"), port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastClassId.has_value());
    QVERIFY(*port.lastClassId == classId("42"));
}

QTEST_APPLESS_MAIN(NextApplicationScheduleEditorClassInfoQueryTests)

#include "next_application_schedule_editor_class_info_query_tests.moc"
