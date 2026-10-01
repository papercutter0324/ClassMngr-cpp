#include "next/application/schedule_import_state_snapshot.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(std::string value)
{
    return *Domain::ClassId::fromString(std::move(value));
}

Domain::TeacherId teacherId(std::string value)
{
    return *Domain::TeacherId::fromString(std::move(value));
}

class RecordingScheduleImportStateSnapshotReadPort final
    : public ScheduleImportStateSnapshotReadPort
{
public:
    [[nodiscard]] ScheduleImportStateSnapshotOutcome
    readCurrentScheduleImportState(
        const ScheduleImportStateSnapshotQuery& query
        ) const override
    {
        ++readCount;
        lastQuery = query;
        return result;
    }

    mutable int readCount = 0;
    mutable ScheduleImportStateSnapshotQuery lastQuery;
    ScheduleImportStateSnapshotOutcome result = ScheduleImportStateSnapshot{};
};

}

class NextApplicationScheduleImportStateSnapshotTests final : public QObject
{
    Q_OBJECT

private slots:
    void returnsCompleteSnapshotInSourceOrder();
    void preservesStructuredUnavailableAndReadFailures();
    void rejectsInvalidOrDuplicateIdentifiers();
};

void NextApplicationScheduleImportStateSnapshotTests::
returnsCompleteSnapshotInSourceOrder()
{
    RecordingScheduleImportStateSnapshotReadPort port;
    const ScheduleImportStateSnapshotQuery query;
    ScheduleImportStateSnapshot expected;
    expected.teachers = {
        {teacherId("9"), u" \uAE40 \uC120\uC0DD\uB2D8 ", u" Room 9 "},
        {teacherId("7"), u"\uBC15 \uC120\uC0DD\uB2D8", u"413"}
    };
    expected.classes = {
        {
            classId("43"),
            teacherId("9"),
            u"Duplicate name order first",
            u" E5 ",
            u"Athena",
            u"#123456",
            {{u" Thursday ", u"5:00 PM", u"5:50 PM"}},
            {{u" Friday ", u"9:00 AM", u"9:50 AM"}}
        },
        {
            classId("42"),
            teacherId("-1"),
            u"Duplicate name order second",
            u"E4",
            u"Hercules",
            u"#FFFFFF",
            {},
            {}
        }
    };
    port.result = expected;

    const auto result =
        ScheduleImportStateSnapshotQueryHandler::execute(query, port);

    QCOMPARE(port.readCount, 1);
    QVERIFY(port.lastQuery == query);
    const auto* snapshot =
        std::get_if<ScheduleImportStateSnapshot>(&result);
    QVERIFY(snapshot);
    QVERIFY(*snapshot == expected);
    QCOMPARE(snapshot->classes[0].id.value(), std::string("43"));
    QCOMPARE(snapshot->classes[1].id.value(), std::string("42"));
    QCOMPARE(snapshot->classes[0].normalTimes[0].day,
             std::u16string(u" Thursday "));
}

void NextApplicationScheduleImportStateSnapshotTests::
preservesStructuredUnavailableAndReadFailures()
{
    RecordingScheduleImportStateSnapshotReadPort port;
    const ScheduleImportStateSnapshotFailure unavailable{
        ScheduleImportStateSnapshotFailureKind::ActiveSessionUnavailable,
        ScheduleImportStateSnapshotFailureSource::Session,
        "session is closed"
    };
    port.result = unavailable;

    auto result = ScheduleImportStateSnapshotQueryHandler::execute({}, port);
    const auto* unavailableResult =
        std::get_if<ScheduleImportStateSnapshotFailure>(&result);
    QVERIFY(unavailableResult);
    QVERIFY(*unavailableResult == unavailable);

    const ScheduleImportStateSnapshotFailure readFailure{
        ScheduleImportStateSnapshotFailureKind::RepositoryReadFailed,
        ScheduleImportStateSnapshotFailureSource::ClassSchedules,
        "schedule rows could not be loaded"
    };
    port.result = readFailure;
    result = ScheduleImportStateSnapshotQueryHandler::execute({}, port);
    const auto* readFailureResult =
        std::get_if<ScheduleImportStateSnapshotFailure>(&result);
    QVERIFY(readFailureResult);
    QVERIFY(*readFailureResult == readFailure);
    QCOMPARE(port.readCount, 2);
}

void NextApplicationScheduleImportStateSnapshotTests::
rejectsInvalidOrDuplicateIdentifiers()
{
    RecordingScheduleImportStateSnapshotReadPort port;
    ScheduleImportStateSnapshot snapshot;
    snapshot.teachers = {
        {teacherId("7"), u"First", u"Room"},
        {teacherId("07"), u"Duplicate", u"Room"}
    };
    port.result = snapshot;

    auto result = ScheduleImportStateSnapshotQueryHandler::execute({}, port);
    const auto* failure =
        std::get_if<ScheduleImportStateSnapshotFailure>(&result);
    QVERIFY(failure);
    QCOMPARE(
        failure->kind,
        ScheduleImportStateSnapshotFailureKind::InvalidSnapshot
        );
    QCOMPARE(
        failure->source,
        ScheduleImportStateSnapshotFailureSource::Snapshot
        );

    snapshot.teachers.clear();
    snapshot.classes = {
        {
            classId("42"), teacherId("7"), u"First", u"E4", u"Hercules",
            u"#FFFFFF", {}, {}
        },
        {
            classId("42"), teacherId("7"), u"Second", u"E5", u"Athena",
            u"#FFFFFF", {}, {}
        }
    };
    port.result = snapshot;
    result = ScheduleImportStateSnapshotQueryHandler::execute({}, port);
    failure = std::get_if<ScheduleImportStateSnapshotFailure>(&result);
    QVERIFY(failure);
    QCOMPARE(
        failure->kind,
        ScheduleImportStateSnapshotFailureKind::InvalidSnapshot
        );
}

QTEST_APPLESS_MAIN(NextApplicationScheduleImportStateSnapshotTests)

#include "next_application_schedule_import_state_snapshot_tests.moc"
