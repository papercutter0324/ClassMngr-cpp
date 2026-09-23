#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "domain/models/roster.h"
#include "domain/models/teacher.h"
#include "next/application/sub_prep_print_source_query.h"
#include "next/platform/application_services_sub_prep_class_details_port.h"
#include "next/platform/application_services_sub_prep_print_source_port.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Platform::
    ApplicationServicesSubPrepClassDetailsPort;
using ClassMngr::Next::Platform::
    ApplicationServicesSubPrepPrintSourcePort;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sub-prep-print-source-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
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
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    return query.exec(statement);
}

int createTeacher(
    ApplicationServices& services,
    const QString& englishName,
    const QString& preferredName,
    const QString& suffix
    )
{
    Teacher teacher;
    teacher.teacherKr = QString::fromUtf8("\xEA\xB0\x80\xEB\x82\x98");
    teacher.teacherEn = englishName;
    teacher.preferredRomanization = preferredName;
    teacher.preferredName = preferredName;
    teacher.roomNumber = QStringLiteral("Room ") + suffix;
    teacher.wifiName = QStringLiteral("Network ") + suffix;
    teacher.wifiPassword = QStringLiteral("wifi-password-") + suffix;
    teacher.internetType = QStringLiteral("LAN");
    teacher.zoomId = QStringLiteral("zoom-") + suffix;
    teacher.zoomPassword = QStringLiteral("zoom-password-") + suffix;
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.notes = QString::fromUtf8("\xEA\xB5\x90\xEC\x82\xAC \xEB\x85\xB8\xED\x8A\xB8 ")
        + suffix;

    const auto created = services.teacherService()->create(teacher);
    return created ? *created : -1;
}

int createClass(
    ApplicationServices& services,
    const QString& name,
    const int assignedTeacherId,
    const QString& grade,
    const QString& level,
    const QString& notes,
    const QString& color,
    const QString& fontColor,
    const QList<ClassTime>& regularTimes,
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
    info.classGrade = grade;
    info.classLevel = level;
    info.classColor = color;
    info.fontColor = fontColor;
    info.notes = notes;
    info.classTimes = regularTimes;
    info.intensiveTimes = intensiveTimes;
    if (!services.classService()->saveClassInfo(info))
    {
        return -1;
    }

    return *created;
}

Roster rosterWithTwoStudents()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        {QStringLiteral("Alex"), QString::fromUtf8("\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D"),
         {}, {}, {}, {}},
        {QStringLiteral("Casey"), QString::fromUtf8("\xEC\x9D\xB4\xED\x95\x99\xEC\x83\x9D"),
         {}, {}, {}, {}}
    };
    return roster;
}

SubPrepPrintSourceRequest requestFor(
    const std::vector<ClassId>& selectedClasses,
    const std::vector<SubPrepWeekday>& selectedDays,
    const ScheduleViewMode mode
    )
{
    return {
        .selectedClassIds = selectedClasses,
        .selectedDays = selectedDays,
        .mode = mode
    };
}

} // namespace

class NextPlatformApplicationServicesSubPrepPrintSourcePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void selectedClassDetailsReadUsesOnlyScopedSessionData();
    void selectedClassDetailsUsesMissingTeacherFallbackAndBoundsFields();
    void projectsSelectedClassesInRequestOrderAndCopiesFilteredSource();
    void selectsIntensiveTimesAndOmitsClassesOutsideSelectedDays();
    void supportsUnassignedAndMissingTeachersAndEmptyRosterFallback();
    void queryEmptyScopeDoesNotReadAndNonemptyReadFailurePropagates();
    void rejectsNoncanonicalClassIdAliasesBeforeReading();
    void maximumClassScopeFitsSqliteBindLimits();
    void perClassMeetingOverflowSurfacesValidation();
    void aggregateMeetingOverflowSurfacesValidation();
    void boundaryReturnsOwningTypedValues();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectedClassDetailsReadUsesOnlyScopedSessionData()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacher = createTeacher(
        services,
        QStringLiteral("English Name"),
        QStringLiteral("Preferred Name"),
        QStringLiteral("details")
        );
    QVERIFY(teacher > 0);
    const int selectedClass = createClass(
        services,
        QStringLiteral("Selected details"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Selected class notes"),
        QStringLiteral("#112233"),
        QStringLiteral("#AABBCC"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(selectedClass > 0);

    // The selected details query must not load either schedule relation.
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE class_times")));
    QVERIFY(executeSql(
        services,
        QStringLiteral("DROP TABLE class_intensive_times")
        ));

    ApplicationServicesSubPrepClassDetailsPort port(services);
    const auto result = port.loadDetails(classId(selectedClass));

    QVERIFY(result);
    const auto& details = result.value();
    QVERIFY(details.classId == classId(selectedClass));
    QVERIFY(details.teacherId == teacherId(teacher));
    QCOMPARE(details.classNotes, std::string("Selected class notes"));
    QCOMPARE(details.teacherDisplayName, std::string("Preferred Name"));
    QCOMPARE(details.teacherFacilities.room, std::string("Room details"));
    QCOMPARE(details.teacherFacilities.wifiName, std::string("Network details"));
    QCOMPARE(
        details.teacherFacilities.wifiPassword,
        std::string("wifi-password-details")
        );
    QCOMPARE(details.teacherFacilities.internetType, std::string("LAN"));
    QCOMPARE(details.teacherFacilities.zoomId, std::string("zoom-details"));
    QCOMPARE(
        details.teacherFacilities.zoomPassword,
        std::string("zoom-password-details")
        );
    QCOMPARE(details.teacherFacilities.projectionType, std::string("Zoom"));
    QCOMPARE(
        details.teacherNotes,
        utf8(QString::fromUtf8(
            "\xEA\xB5\x90\xEC\x82\xAC \xEB\x85\xB8\xED\x8A\xB8 details"
            ))
        );

    const auto ownedCopy = details;
    services.closeDatabase();
    QVERIFY(ownedCopy == details);
    QCOMPARE(ownedCopy.teacherFacilities.room, std::string("Room details"));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectedClassDetailsUsesMissingTeacherFallbackAndBoundsFields()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int unassignedClass = createClass(
        services,
        QStringLiteral("Unassigned details"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Unassigned class notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(unassignedClass > 0);

    const int missingTeacherClass = createClass(
        services,
        QStringLiteral("Stale teacher details"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Stale teacher notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(missingTeacherClass > 0);

    const auto classWithoutInfo = services.classService()->create(
        QStringLiteral("Missing class info")
        );
    QVERIFY(classWithoutInfo);

    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = OFF")));
    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlQuery orphanAssignment(dataService->databaseSession()->database());
    QVERIFY(orphanAssignment.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?"
        )));
    orphanAssignment.addBindValue(999999);
    orphanAssignment.addBindValue(missingTeacherClass);
    QVERIFY2(
        orphanAssignment.exec(),
        qPrintable(orphanAssignment.lastError().text())
        );
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = ON")));

    ApplicationServicesSubPrepClassDetailsPort port(services);
    for (const int id : {unassignedClass, missingTeacherClass})
    {
        const auto result = port.loadDetails(classId(id));
        QVERIFY(result);
        QVERIFY(!result.value().teacherId.has_value());
        QVERIFY(result.value().teacherDisplayName.empty());
        QVERIFY(
            result.value().teacherFacilities
            == SelectedClassTeacherFacilities{}
            );
        QVERIFY(result.value().teacherNotes.empty());
    }

    const auto noInfo = port.loadDetails(classId(*classWithoutInfo));
    QVERIFY(noInfo);
    QCOMPARE(noInfo.value().classNotes, std::string());
    QVERIFY(!noInfo.value().teacherId.has_value());

    const auto missingClass = port.loadDetails(classId(999999));
    QVERIFY(!missingClass);
    QCOMPARE(missingClass.error().code, ErrorCode::NotFound);

    const auto alias = ClassId::fromString("01");
    QVERIFY(alias.has_value());
    const auto invalidId = port.loadDetails(*alias);
    QVERIFY(!invalidId);
    QCOMPARE(invalidId.error().code, ErrorCode::InvalidInput);

    const int oversizedTeacher = createTeacher(
        services,
        QStringLiteral("Oversized Room"),
        QStringLiteral("Oversized Room"),
        QStringLiteral("oversized")
        );
    QVERIFY(oversizedTeacher > 0);
    DataService* oversizedDataService = services.dataService();
    QVERIFY(oversizedDataService);
    QVERIFY(oversizedDataService->databaseSession());
    QSqlQuery oversizedRoom(oversizedDataService->databaseSession()->database());
    QVERIFY(oversizedRoom.prepare(QStringLiteral(
        "UPDATE teachers SET room_number = ? WHERE id = ?"
        )));
    oversizedRoom.addBindValue(QString(
        static_cast<qsizetype>(kSelectedClassDetailsMaxTeacherRoomLength + 1),
        QLatin1Char('R')
        ));
    oversizedRoom.addBindValue(oversizedTeacher);
    QVERIFY2(oversizedRoom.exec(), qPrintable(oversizedRoom.lastError().text()));
    const int oversizedDetailsClass = createClass(
        services,
        QStringLiteral("Oversized detail field"),
        oversizedTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        QStringLiteral("Valid class notes"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(oversizedDetailsClass > 0);
    const auto oversized = port.loadDetails(classId(oversizedDetailsClass));
    QVERIFY(!oversized);
    QCOMPARE(oversized.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
projectsSelectedClassesInRequestOrderAndCopiesFilteredSource()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    const int teacherOne = createTeacher(
        services,
        QStringLiteral("Jin Park"),
        QStringLiteral("Jin Park"),
        QStringLiteral("one")
        );
    const int teacherTwo = createTeacher(
        services,
        QStringLiteral("Alex Morgan"),
        QStringLiteral("Alex"),
        QStringLiteral("two")
        );
    QVERIFY(teacherOne > 0);
    QVERIFY(teacherTwo > 0);

    const int classZ = createClass(
        services,
        QStringLiteral("Z Class"),
        teacherOne,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Z class notes"),
        QStringLiteral("#112233"),
        QStringLiteral("#AABBCC"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("10:00 AM"), QStringLiteral("10:45 AM")},
            {QStringLiteral("Thursday"), QStringLiteral("1:00 PM"), QStringLiteral("1:45 PM")},
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        },
        {
            {QStringLiteral("Wednesday"), QStringLiteral("2:00 PM"), QStringLiteral("2:45 PM")}
        }
        );
    const int classA = createClass(
        services,
        QStringLiteral("A Class"),
        teacherTwo,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("A class notes"),
        QStringLiteral("#223344"),
        QStringLiteral("#DDEEFF"),
        {
            {QStringLiteral("Friday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")},
            {QStringLiteral("Monday"), QStringLiteral("11:00 AM"), QStringLiteral("11:45 AM")}
        },
        {
            {QStringLiteral("Monday"), QStringLiteral("3:00 PM"), QStringLiteral("3:45 PM")}
        }
        );
    const int classY = createClass(
        services,
        QStringLiteral("Y Class"),
        teacherOne,
        QStringLiteral("E6"),
        QStringLiteral("Helios"),
        QStringLiteral("Y class notes"),
        QStringLiteral("#445566"),
        QStringLiteral("#778899"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("2:00 PM"), QStringLiteral("2:45 PM")}
        }
        );
    const int unselectedClass = createClass(
        services,
        QStringLiteral("B Unselected"),
        teacherOne,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("outside selected scope"),
        QStringLiteral("#334455"),
        QStringLiteral("#FFFFFF"),
        {
            {QStringLiteral("Monday"), QStringLiteral("12:00 PM"), QStringLiteral("12:45 PM")}
        }
        );
    QVERIFY(classZ > 0);
    QVERIFY(classA > 0);
    QVERIFY(classY > 0);
    QVERIFY(unselectedClass > 0);

    QVERIFY(services.rosterService()->saveRoster(classZ, rosterWithTwoStudents()));
    QVERIFY(executeSql(
        services,
        QStringLiteral("DROP TABLE class_intensive_times")
        ));

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classZ), classId(classY), classId(classA)},
        {SubPrepWeekday::Monday, SubPrepWeekday::Tuesday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    const SubPrepPrintSourceInput& source = result.value();
    QCOMPARE(source.classes.size(), std::size_t(3));
    QCOMPARE(source.classes[0].id, classId(classZ));
    QCOMPARE(source.classes[1].id, classId(classY));
    QCOMPARE(source.classes[2].id, classId(classA));
    QVERIFY(source.classes[0].teacherId == teacherId(teacherOne));
    QVERIFY(source.classes[1].teacherId == teacherId(teacherOne));
    QVERIFY(source.classes[2].teacherId == teacherId(teacherTwo));

    QCOMPARE(source.classes[0].grade, std::string("E4"));
    QCOMPARE(source.classes[0].level, std::string("Perseus"));
    QCOMPARE(source.classes[0].classNotes, std::string("Z class notes"));
    QCOMPARE(source.classes[0].classColor, std::string("#112233"));
    QCOMPARE(source.classes[0].fontColor, std::string("#AABBCC"));
    QCOMPARE(source.classes[0].studentCount, std::size_t(2));
    QCOMPARE(source.classes[0].meetings.size(), std::size_t(2));
    QCOMPARE(source.classes[0].meetings[0].weekday, SubPrepWeekday::Tuesday);
    QCOMPARE(source.classes[0].meetings[0].startTime, std::string("10:00 AM"));
    QCOMPARE(source.classes[0].meetings[0].endTime, std::string("10:45 AM"));
    QCOMPARE(source.classes[0].meetings[1].weekday, SubPrepWeekday::Monday);
    QCOMPARE(source.classes[0].meetings[1].startTime, std::string("9:00 AM"));
    QCOMPARE(source.classes[1].meetings.size(), std::size_t(1));
    QCOMPARE(source.classes[1].meetings[0].weekday, SubPrepWeekday::Tuesday);
    QCOMPARE(source.classes[1].meetings[0].startTime, std::string("2:00 PM"));
    QCOMPARE(source.classes[2].meetings.size(), std::size_t(1));
    QCOMPARE(source.classes[2].meetings[0].weekday, SubPrepWeekday::Monday);
    QCOMPARE(source.classes[2].meetings[0].startTime, std::string("11:00 AM"));

    QCOMPARE(source.teachers.size(), std::size_t(2));
    QCOMPARE(source.teachers[0].id, teacherId(teacherOne));
    QCOMPARE(source.teachers[0].englishName, std::string("Jin Park"));
    QCOMPARE(source.teachers[0].room, std::string("Room one"));
    QCOMPARE(source.teachers[0].wifiName, std::string("Network one"));
    QCOMPARE(source.teachers[0].wifiPassword, std::string("wifi-password-one"));
    QCOMPARE(source.teachers[0].internetType, std::string("LAN"));
    QCOMPARE(source.teachers[0].zoomId, std::string("zoom-one"));
    QCOMPARE(source.teachers[0].zoomPassword, std::string("zoom-password-one"));
    QCOMPARE(source.teachers[0].projectionType, std::string("Zoom"));
    QCOMPARE(source.teachers[0].teacherNotes, utf8(QString::fromUtf8(
        "\xEA\xB5\x90\xEC\x82\xAC \xEB\x85\xB8\xED\x8A\xB8 one"
        )));
    QCOMPARE(source.teachers[1].id, teacherId(teacherTwo));
    QCOMPARE(source.teachers[1].englishName, std::string("Alex Morgan"));

    const SubPrepPrintSourceInput previous = source;
    services.closeDatabase();
    QVERIFY(source == previous);
    QCOMPARE(source.teachers[0].room, std::string("Room one"));
    QCOMPARE(source.classes[0].meetings[0].startTime, std::string("10:00 AM"));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
selectsIntensiveTimesAndOmitsClassesOutsideSelectedDays()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Taylor"),
        QStringLiteral("Taylor"),
        QStringLiteral("mode")
        );
    QVERIFY(teacher > 0);

    const int classOne = createClass(
        services,
        QStringLiteral("First"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        },
        {
            {QStringLiteral("Wednesday"), QStringLiteral("1:00 PM"), QStringLiteral("1:45 PM")},
            {QStringLiteral("Monday"), QStringLiteral("2:00 PM"), QStringLiteral("2:45 PM")}
        }
        );
    const int classTwo = createClass(
        services,
        QStringLiteral("Second"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("10:00 AM"), QStringLiteral("10:45 AM")}
        },
        {
            {QStringLiteral("Friday"), QStringLiteral("3:00 PM"), QStringLiteral("3:45 PM")}
        }
        );
    QVERIFY(classOne > 0);
    QVERIFY(classTwo > 0);
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE class_times")));

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(classOne), classId(classTwo)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Intensive
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    QCOMPARE(result.value().classes[0].id, classId(classOne));
    QCOMPARE(result.value().classes[0].meetings.size(), std::size_t(1));
    QCOMPARE(result.value().classes[0].meetings[0].weekday, SubPrepWeekday::Monday);
    QCOMPARE(result.value().classes[0].meetings[0].startTime, std::string("2:00 PM"));
    QCOMPARE(result.value().teachers.size(), std::size_t(1));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
supportsUnassignedAndMissingTeachersAndEmptyRosterFallback()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int assignedTeacher = createTeacher(
        services,
        QStringLiteral("Roster Teacher"),
        QStringLiteral("Roster Teacher"),
        QStringLiteral("roster")
        );
    QVERIFY(assignedTeacher > 0);
    const int classIdWithoutAssignment = createClass(
        services,
        QStringLiteral("No teacher"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("8:00 AM"), QStringLiteral("8:45 AM")}
        }
        );
    QVERIFY(classIdWithoutAssignment > 0);
    const int classWithMissingTeacher = createClass(
        services,
        QStringLiteral("Missing teacher"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("11:00 AM"), QStringLiteral("11:45 AM")}
        }
        );
    QVERIFY(classWithMissingTeacher > 0);
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = OFF")));
    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlQuery orphanAssignment(
        dataService->databaseSession()->database()
        );
    QVERIFY(orphanAssignment.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?"
        )));
    orphanAssignment.addBindValue(999999);
    orphanAssignment.addBindValue(classWithMissingTeacher);
    QVERIFY2(
        orphanAssignment.exec(),
        qPrintable(orphanAssignment.lastError().text())
        );
    QVERIFY(executeSql(services, QStringLiteral("PRAGMA foreign_keys = ON")));
    const int classWithTeacherButNoRoster = createClass(
        services,
        QStringLiteral("No roster"),
        assignedTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(classWithTeacherButNoRoster > 0);
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE roster_columns")));

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {
            classId(classIdWithoutAssignment),
            classId(classWithMissingTeacher),
            classId(classWithTeacherButNoRoster)
        },
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(), std::size_t(1));
    QCOMPARE(result.value().classes[0].id, classId(classWithTeacherButNoRoster));
    QVERIFY(result.value().classes[0].teacherId == teacherId(assignedTeacher));
    QCOMPARE(result.value().classes[0].studentCount, std::size_t(0));
    QCOMPARE(result.value().teachers.size(), std::size_t(1));
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
queryEmptyScopeDoesNotReadAndNonemptyReadFailurePropagates()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesSubPrepPrintSourcePort port(services);
    const SubPrepPrintSourceQuery query(port);

    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE classes")));
    const auto emptyResult = query.execute(requestFor(
        {},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));
    QVERIFY(emptyResult);
    QVERIFY(emptyResult.value().empty());
    const auto emptyDaysResult = query.execute(requestFor(
        {classId(1)},
        {},
        ScheduleViewMode::Regular
        ));
    QVERIFY(emptyDaysResult);
    QVERIFY(emptyDaysResult.value().empty());

    ApplicationServices failureServices;
    QVERIFY(openDatabase(failureServices, m_directory));
    const int classIdWithSchedule = createClass(
        failureServices,
        QStringLiteral("Unreadable schedule"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        }
        );
    QVERIFY(classIdWithSchedule > 0);
    QVERIFY(executeSql(failureServices, QStringLiteral("DROP TABLE class_times")));

    ApplicationServicesSubPrepPrintSourcePort failingPort(failureServices);
    const auto failed = failingPort.loadSource(requestFor(
        {classId(classIdWithSchedule)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(!failed);
    QCOMPARE(failed.error().code, ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
rejectsNoncanonicalClassIdAliasesBeforeReading()
{
    ApplicationServices services;
    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        {classId(1), *ClassId::fromString("01")},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
maximumClassScopeFitsSqliteBindLimits()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    std::vector<ClassId> selectedClassIds;
    selectedClassIds.reserve(kSubPrepPrintSourceMaxClassIds);
    for (std::size_t index = 1;
         index <= kSubPrepPrintSourceMaxClassIds;
         ++index)
    {
        selectedClassIds.push_back(classId(static_cast<int>(index)));
    }

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const auto result = port.loadSource(requestFor(
        selectedClassIds,
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(result);
    QVERIFY(result.value().classes.empty());
    QVERIFY(result.value().teachers.empty());
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
perClassMeetingOverflowSurfacesValidation()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Overflow Teacher"),
        QStringLiteral("Overflow Teacher"),
        QStringLiteral("overflow")
        );
    QVERIFY(teacher > 0);
    const int classWithManyMeetings = createClass(
        services,
        QStringLiteral("Many meetings"),
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral(""),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#000000"),
        {}
        );
    QVERIFY(classWithManyMeetings > 0);

    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    QSqlQuery insert(dataService->databaseSession()->database());
    QVERIFY(insert.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id, day, start_time, end_time) "
        "VALUES (?, ?, ?, ?)"
        )));
    for (std::size_t index = 0;
         index <= kSubPrepPrintSourceMaxMeetingsPerClass;
         ++index)
    {
        insert.bindValue(0, classWithManyMeetings);
        insert.bindValue(1, QStringLiteral("Monday"));
        insert.bindValue(2, QStringLiteral("9:00 AM"));
        insert.bindValue(3, QStringLiteral("9:45 AM"));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const SubPrepPrintSourceQuery query(port);
    const auto result = query.execute(requestFor(
        {classId(classWithManyMeetings)},
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
aggregateMeetingOverflowSurfacesValidation()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const int teacher = createTeacher(
        services,
        QStringLiteral("Overflow"),
        QStringLiteral("Overflow"),
        QStringLiteral("agg")
        );
    QVERIFY(teacher > 0);

    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    QVERIFY(dataService->databaseSession());
    const QSqlDatabase database = dataService->databaseSession()->database();

    std::vector<ClassId> selectedClassIds;
    selectedClassIds.reserve(253);
    QSqlQuery classInsert(database);
    QVERIFY(classInsert.prepare(QStringLiteral(
        "INSERT INTO classes (name) VALUES (?)"
        )));
    QSqlQuery classInfoInsert(database);
    QVERIFY(classInfoInsert.prepare(QStringLiteral(
        "INSERT INTO class_info "
        "(class_id, teacher_id, class_grade, class_level, class_color, font_color) "
        "VALUES (?, ?, 'E4', 'Aggregate overflow', '#FFFFFF', '#000000')"
        )));
    for (int index = 0; index < 253; ++index)
    {
        classInsert.bindValue(0, QStringLiteral("Aggregate overflow %1")
                                     .arg(index));
        QVERIFY2(classInsert.exec(), qPrintable(classInsert.lastError().text()));
        const int classId = classInsert.lastInsertId().toInt();
        QVERIFY(classId > 0);
        selectedClassIds.push_back(ClassId::fromString(
            std::to_string(classId)
            ).value());

        classInfoInsert.bindValue(0, classId);
        classInfoInsert.bindValue(1, teacher);
        QVERIFY2(
            classInfoInsert.exec(),
            qPrintable(classInfoInsert.lastError().text())
            );
    }

    QSqlQuery insertMeetings(database);
    QVERIFY(insertMeetings.exec(QStringLiteral(R"(
        WITH RECURSIVE meeting_number(n) AS (
            SELECT 1
            UNION ALL SELECT n + 1 FROM meeting_number WHERE n < 65
        )
        INSERT INTO class_times (class_id, day, start_time, end_time)
        SELECT
            class_info.class_id,
            'Monday',
            '9:00 AM',
            '9:45 AM'
        FROM class_info
        CROSS JOIN meeting_number
        WHERE class_info.class_grade = 'E4'
          AND class_info.class_level = 'Aggregate overflow'
    )")));

    ApplicationServicesSubPrepPrintSourcePort port(services);
    const SubPrepPrintSourceQuery query(port);
    const auto result = query.execute(requestFor(
        selectedClassIds,
        {SubPrepWeekday::Monday},
        ScheduleViewMode::Regular
        ));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Validation);
}

void NextPlatformApplicationServicesSubPrepPrintSourcePortTests::
boundaryReturnsOwningTypedValues()
{
    using Port = ApplicationServicesSubPrepPrintSourcePort;
    static_assert(std::is_same_v<
        decltype(std::declval<Port&>().loadSource(
            std::declval<const SubPrepPrintSourceRequest&>()
            )),
        SubPrepPrintSourceReadResult
        >);
    static_assert(!std::is_copy_constructible_v<Port>);
    static_assert(!std::is_move_constructible_v<Port>);
    QVERIFY(true);
}

QTEST_MAIN(NextPlatformApplicationServicesSubPrepPrintSourcePortTests)

#include "next_platform_application_services_sub_prep_print_source_port_tests.moc"
