#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "domain/models/roster.h"
#include "next/application/sub_prep_roster_output_source_query.h"
#include "next/platform/application_services_sub_prep_roster_output_source_port.h"

#include <QByteArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Platform::
    ApplicationServicesSubPrepRosterOutputSourcePort;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sub-prep-roster-output-%1.db").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(ApplicationServices& services, QTemporaryDir& directory)
{
    return services.openDatabase(databasePath(directory)).has_value();
}

ClassId classId(const int id)
{
    return *ClassId::fromString(std::to_string(id));
}

TeacherId teacherId(const int id)
{
    return *TeacherId::fromString(std::to_string(id));
}

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

bool executeSql(ApplicationServices& services, const QString& statement)
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }
    QSqlQuery query(dataService->databaseSession()->database());
    return query.exec(statement);
}

int createTeacher(ApplicationServices& services)
{
    Teacher teacher;
    teacher.teacherKr = QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98");
    teacher.teacherEn = QStringLiteral("Teacher English");
    teacher.preferredName = QStringLiteral("Teacher English");
    teacher.preferredRomanization = QStringLiteral("Romanized Name");
    teacher.roomNumber = QStringLiteral("Teacher Room");
    teacher.wifiName = QStringLiteral("Teacher Network");
    teacher.wifiPassword = QStringLiteral("Teacher WiFi Password");
    teacher.zoomId = QStringLiteral("teacher-zoom-id");
    teacher.zoomPassword = QStringLiteral("teacher-zoom-password");
    teacher.notes = QStringLiteral("not copied into roster output");
    const auto created = services.teacherService()->create(teacher);
    return created ? *created : -1;
}

int createClass(
    ApplicationServices& services,
    const QString& name,
    const int assignedTeacherId,
    const QString& grade,
    const QString& level,
    const QString& day,
    const QString& startTime,
    const QString& endTime,
    const QList<ClassTime>& intensiveTimes = {}
    )
{
    const auto created = services.classService()->create(name);
    if (!created)
    {
        return -1;
    }

    ClassInfo info;
    info.classId = *created;
    info.teacherId = assignedTeacherId;
    info.teacherEn = QStringLiteral("Class English Name");
    info.teacherKr = QStringLiteral("Class Korean Name");
    info.classGrade = grade;
    info.classLevel = level;
    info.roomNumber = QStringLiteral("Class Room");
    info.wifiName = QStringLiteral("Class Network");
    info.wifiPassword = QStringLiteral("Class WiFi Password");
    info.zoomId = QStringLiteral("class-zoom-id");
    info.zoomPassword = QStringLiteral("class-zoom-password");
    info.classTimes = {{day, startTime, endTime}};
    info.intensiveTimes = intensiveTimes;
    const auto saved = services.classService()->saveClassInfo(info);
    if (!saved)
    {
        return -1;
    }
    return *created;
}

Roster rosterWithColumns()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.columns.append(QStringLiteral("Notes"));
    roster.columns.append(QStringLiteral("Unused"));
    roster.rows = {
        {
            QStringLiteral("Alex"),
            QString::fromUtf8("\xEA\xB9\x80\xEA\xB0\x80"),
            {},
            {},
            {},
            {},
            QStringLiteral("First note"),
            QStringLiteral("not projected")
        },
        {
            QStringLiteral("Casey"),
            QString::fromUtf8("\xEC\x9D\xB4\xEB\xAF\xBC"),
            {},
            {},
            {},
            {},
            QStringLiteral("Second note"),
            QStringLiteral("also not projected")
        }
    };
    return roster;
}

SubPrepRosterOutputSourceRequest requestFor(
    const std::vector<ClassId>& ids,
    const std::vector<SubPrepWeekday>& days,
    const ScheduleViewMode mode = ScheduleViewMode::Regular
    )
{
    return {
        .selectedClassIds = ids,
        .selectedDays = days,
        .mode = mode,
        .selectedExtraColumns = {"Notes"}
    };
}

} // namespace

class NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readsSelectedClassesModeAndRequestedRosterColumns();
    void includesUnassignedTeacherAndUsesSelectedMode();
    void emptyScopeAndNoncanonicalIdsAvoidDatabaseReads();
    void rejectsOutOfBoundRosterAndClassText();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
readsSelectedClassesModeAndRequestedRosterColumns()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);

    const int firstClass = createClass(
        services,
        QStringLiteral("First class"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    const int secondClass = createClass(
        services,
        QStringLiteral("Second class"),
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Artemis"),
        QStringLiteral("Monday"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("12:00 PM")
        );
    const int outsideSelectedDays = createClass(
        services,
        QStringLiteral("Tuesday class"),
        teacher,
        QStringLiteral("E6"),
        QStringLiteral("Helios"),
        QStringLiteral("Tuesday"),
        QStringLiteral("1:00 PM"),
        QStringLiteral("2:00 PM")
        );
    QVERIFY(firstClass > 0);
    QVERIFY(secondClass > 0);
    QVERIFY(outsideSelectedDays > 0);
    QVERIFY(services.rosterService()->saveRoster(firstClass, rosterWithColumns()));
    QVERIFY(services.rosterService()->saveRoster(secondClass, rosterWithColumns()));
    QVERIFY(services.rosterService()->saveRoster(
        outsideSelectedDays,
        rosterWithColumns()
        ));

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const SubPrepRosterOutputSourceQuery query(port);
    const auto requested = requestFor(
        {
            classId(secondClass),
            classId(firstClass),
            classId(outsideSelectedDays)
        },
        {SubPrepWeekday::Monday}
        );
    const auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(result.value().classes().size(), std::size_t(2));
    QCOMPARE(result.value().classes().at(0).id, classId(secondClass));
    QCOMPARE(result.value().classes().at(1).id, classId(firstClass));
    QCOMPARE(result.value().teachers().size(), std::size_t(1));
    QCOMPARE(
        result.value().teachers().front(),
        (SubPrepRosterOutputTeacher{
            teacherId(teacher),
            "Teacher English",
            utf8(QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98")),
            "Teacher English",
            "Romanized Name"
        })
        );

    const auto& first = result.value().classes().front();
    QCOMPARE(first.classroomName, std::string("Second class"));
    QCOMPARE(first.grade, std::string("E5"));
    QCOMPARE(first.level, std::string("Artemis"));
    QCOMPARE(first.classTeacherEnglishName, std::string("Teacher English"));
    QCOMPARE(
        first.classTeacherKoreanName,
        utf8(QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98"))
        );
    QCOMPARE(first.room, std::string("Teacher Room"));
    QCOMPARE(first.wifiName, std::string("Teacher Network"));
    QCOMPARE(first.wifiPassword, std::string("Teacher WiFi Password"));
    QCOMPARE(first.zoomId, std::string("teacher-zoom-id"));
    QCOMPARE(first.zoomPassword, std::string("teacher-zoom-password"));
    QCOMPARE(first.meetings.size(), std::size_t(1));
    QCOMPARE(first.meetings.front().weekday, SubPrepWeekday::Monday);
    QCOMPARE(first.meetings.front().startTime, std::string("11:00 AM"));
    QCOMPARE(
        first.rosterColumns,
        (std::vector<std::string>{"English", "Korean", "Notes"})
        );
    QCOMPARE(first.rosterRows.size(), std::size_t(2));
    QCOMPARE(
        first.rosterRows.front(),
        (std::vector<std::string>{
            "Alex",
            utf8(QString::fromUtf8("\xEA\xB9\x80\xEA\xB0\x80")),
            "First note"
        })
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
includesUnassignedTeacherAndUsesSelectedMode()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int intensiveClass = createClass(
        services,
        QStringLiteral("Intensive only"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Tuesday"),
        QStringLiteral("1:00 PM"),
        QStringLiteral("2:00 PM"),
        {{
            QStringLiteral("Monday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:45 PM")
        }}
        );
    QVERIFY(intensiveClass > 0);
    QVERIFY(services.rosterService()->saveRoster(
        intensiveClass,
        rosterWithColumns()
        ));

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const SubPrepRosterOutputSourceQuery query(port);
    const std::vector<ClassId> ids{classId(intensiveClass)};
    const std::vector<SubPrepWeekday> days{SubPrepWeekday::Monday};

    const auto regular = query.execute(requestFor(ids, days));
    QVERIFY(regular);
    QVERIFY(regular.value().empty());

    const auto intensive = query.execute(
        requestFor(ids, days, ScheduleViewMode::Intensive)
        );
    QVERIFY(intensive);
    QCOMPARE(intensive.value().classes().size(), std::size_t(1));
    QVERIFY(intensive.value().teachers().empty());
    QVERIFY(!intensive.value().classes().front().teacherId.has_value());
    QCOMPARE(
        intensive.value().classes().front().meetings.front().startTime,
        std::string("3:00 PM")
        );
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
emptyScopeAndNoncanonicalIdsAvoidDatabaseReads()
{
    ApplicationServices services;
    ApplicationServicesSubPrepRosterOutputSourcePort port(services);

    auto emptyRequest = requestFor({}, {SubPrepWeekday::Monday});
    const auto empty = port.loadSource(emptyRequest);
    QVERIFY(empty);
    QVERIFY(empty.value().classes.empty());

    const auto aliasedId = ClassId::fromString("01");
    QVERIFY(aliasedId.has_value());
    auto invalidRequest = requestFor(
        {*aliasedId},
        {SubPrepWeekday::Monday}
        );
    const auto invalid = port.loadSource(invalidRequest);
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, ErrorCode::InvalidInput);
}

void NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests::
rejectsOutOfBoundRosterAndClassText()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(services);
    QVERIFY(teacher > 0);
    const int classIdValue = createClass(
        services,
        QStringLiteral("Bounded source"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
        );
    QVERIFY(classIdValue > 0);
    const auto savedRoster = services.rosterService()->saveRoster(
        classIdValue,
        rosterWithColumns()
        );
    QVERIFY(savedRoster);

    ApplicationServicesSubPrepRosterOutputSourcePort port(services);
    const auto request = requestFor(
        {classId(classIdValue)},
        {SubPrepWeekday::Monday}
        );
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (%1, 4096, 0, 'outside cap')"
            ).arg(classIdValue)
        ));
    const auto rosterOverflow = port.loadSource(request);
    QVERIFY(!rosterOverflow);
    QCOMPARE(rosterOverflow.error().code, ErrorCode::Validation);

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "DELETE FROM roster_data WHERE class_id=%1 AND row_index=4096"
            ).arg(classIdValue)
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "UPDATE teachers SET zoom_id='%1' WHERE id=%2"
            ).arg(
                QString(257, QLatin1Char('z')),
                QString::number(teacher)
                )
        ));
    const auto oversizedField = port.loadSource(request);
    QVERIFY(!oversizedField);
    QCOMPARE(oversizedField.error().code, ErrorCode::Validation);
}

QTEST_GUILESS_MAIN(NextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests)

#include "next_platform_application_services_sub_prep_roster_output_source_port_tests.moc"
