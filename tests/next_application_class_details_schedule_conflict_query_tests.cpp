#include "next/application/class_details_schedule_conflict_query.h"

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

Domain::ScheduleTime time(
    const int weekday,
    const int startMinute,
    const int endMinute
    )
{
    return *Domain::ScheduleTime::fromMinutes(
        weekday,
        startMinute,
        endMinute
        );
}

class RecordingPort final : public ClassDetailsScheduleConflictPort
{
public:
    [[nodiscard]] ClassDetailsScheduleConflictResult
    classDetailsScheduleConflicts(
        const ClassDetailsScheduleConflictRequest& request
        ) const override
    {
        requests.push_back(request);
        return result;
    }

    mutable std::vector<ClassDetailsScheduleConflictRequest> requests;
    ClassDetailsScheduleConflictResult result =
        ClassDetailsScheduleConflictResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured conflict query port.",
            .recoverable = false
        });
};

}

class NextApplicationClassDetailsScheduleConflictQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void contractUsesQtFreeTypedOrderedValues();
    void sendsRequestOnceAndPreservesConflictOrderAndFields();
    void forwardsEmptyCandidatesWithoutSkippingThePort();
    void propagatesPortFailureExactly();
    void rejectsNonCanonicalIdsBeforePortCall();
};

void NextApplicationClassDetailsScheduleConflictQueryTests::
contractUsesQtFreeTypedOrderedValues()
{
    static_assert(std::is_same_v<
        decltype(ClassDetailsScheduleConflictRequest::classId),
        Domain::ClassId
        >);
    static_assert(std::is_same_v<
        decltype(ClassDetailsScheduleConflictRequest::mode),
        ClassDetailsScheduleMode
        >);
    static_assert(std::is_same_v<
        decltype(ClassDetailsScheduleConflictRequest::candidateTimes),
        std::vector<Domain::ScheduleTime>
        >);
    static_assert(std::is_same_v<
        decltype(ClassDetailsScheduleConflict::conflictingClassName),
        std::u16string
        >);

    const ClassDetailsScheduleConflictRequest request{
        .classId = classId("42"),
        .mode = ClassDetailsScheduleMode::Intensive,
        .candidateTimes = {time(2, 12 * 60, 12 * 60 + 55)}
    };
    QCOMPARE(request.classId.value(), std::string("42"));
    QCOMPARE(request.mode, ClassDetailsScheduleMode::Intensive);
    QCOMPARE(request.candidateTimes.size(), std::size_t(1));
    QCOMPARE(request.candidateTimes.front().weekdayIndex(), 2);
}

void NextApplicationClassDetailsScheduleConflictQueryTests::
sendsRequestOnceAndPreservesConflictOrderAndFields()
{
    RecordingPort port;
    const ClassDetailsScheduleConflictRequest request{
        .classId = classId("42"),
        .mode = ClassDetailsScheduleMode::Regular,
        .candidateTimes = {
            time(4, 15 * 60, 15 * 60 + 55),
            time(0, 9 * 60, 9 * 60 + 55)
        }
    };
    const std::vector<ClassDetailsScheduleConflict> expected{
        {
            .className = u"Selected Class",
            .day = u"Friday",
            .startTime = u"3:00 PM",
            .endTime = u"3:55 PM",
            .conflictingClassName = u"Friday Class"
        },
        {
            .className = u"Selected Class",
            .day = u"Monday",
            .startTime = u"9:00 AM",
            .endTime = u"9:55 AM",
            .conflictingClassName = u"Selected Class"
        }
    };
    port.result = ClassDetailsScheduleConflictResult::success(expected);

    const auto result = ClassDetailsScheduleConflictQuery::execute(
        request,
        port
        );

    QVERIFY(result);
    QCOMPARE(port.requests.size(), std::size_t(1));
    QVERIFY(port.requests.front().classId == request.classId);
    QCOMPARE(port.requests.front().mode, request.mode);
    QVERIFY(port.requests.front().candidateTimes == request.candidateTimes);
    QVERIFY(result.value() == expected);
}

void NextApplicationClassDetailsScheduleConflictQueryTests::
forwardsEmptyCandidatesWithoutSkippingThePort()
{
    RecordingPort port;
    const ClassDetailsScheduleConflictRequest request{
        .classId = classId("7"),
        .mode = ClassDetailsScheduleMode::Intensive,
        .candidateTimes = {}
    };
    port.result = ClassDetailsScheduleConflictResult::success({});

    const auto result = ClassDetailsScheduleConflictQuery::execute(
        request,
        port
        );

    QVERIFY(result);
    QCOMPARE(port.requests.size(), std::size_t(1));
    QVERIFY(port.requests.front().candidateTimes.empty());
}

void NextApplicationClassDetailsScheduleConflictQueryTests::
propagatesPortFailureExactly()
{
    RecordingPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "schedule conflict query unavailable",
        .recoverable = true
    };
    port.result = ClassDetailsScheduleConflictResult::failure(expected);

    const auto result = ClassDetailsScheduleConflictQuery::execute(
        {
            .classId = classId("42"),
            .mode = ClassDetailsScheduleMode::Regular,
            .candidateTimes = {}
        },
        port
        );

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.requests.size(), std::size_t(1));
}

void NextApplicationClassDetailsScheduleConflictQueryTests::
rejectsNonCanonicalIdsBeforePortCall()
{
    RecordingPort port;
    const std::vector<std::string> values{"0", "042", "42x", "-1"};

    for (const std::string& value : values)
    {
        const auto result = ClassDetailsScheduleConflictQuery::execute(
            {
                .classId = classId(value),
                .mode = ClassDetailsScheduleMode::Regular,
                .candidateTimes = {}
            },
            port
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    QVERIFY(port.requests.empty());
}

QTEST_APPLESS_MAIN(NextApplicationClassDetailsScheduleConflictQueryTests)

#include "next_application_class_details_schedule_conflict_query_tests.moc"
