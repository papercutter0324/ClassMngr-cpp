#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/platform/application_services_class_co_teacher_page_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("co-teacher-page-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.classService()->create(name);
    return created.value_or(-1);
}

ClassInfo classInfo(const int id, const int teacherId = -1)
{
    ClassInfo info;
    info.classId = id;
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
         QStringLiteral("4:50 PM")},
        {QStringLiteral("Wednesday"), QStringLiteral("4:00 PM"),
         QStringLiteral("4:50 PM")}
    };
    return info;
}

int createTeacher(ApplicationServices& services)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("Korean Name");
    teacher.teacherEn = QStringLiteral("English Name");
    teacher.preferredName = QStringLiteral(" Preferred Display ");
    const auto created =
        services.databaseSession()->teacherRepository()->createTeacher(teacher);
    return created.value_or(-1);
}

bool updateTeacherId(
    ApplicationServices& services,
    const int classIdValue,
    const int teacherIdValue
    )
{
    QSqlQuery query(services.databaseSession()->database());
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys=OFF")))
    {
        return false;
    }
    query.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"
        ));
    query.addBindValue(teacherIdValue);
    query.addBindValue(classIdValue);
    return query.exec() && query.numRowsAffected() == 1;
}

}

class NextPlatformApplicationServicesClassCoTeacherPageReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsAssignedClassAndPreferredTeacherFromActiveSession();
    void mapsUnassignedTeacherToEmptyTypedSelection();
    void treatsNonpositiveTeacherIdsAsUnassignedAndPreservesClassFields();
    void unavailableSessionDoesNotUseDataServiceFallback();
    void classAndTeacherSourceFailuresRemainIndependent();
};

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
mapsAssignedClassAndPreferredTeacherFromActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Co-Teacher Read Test"));
    const int teacherId = createTeacher(services);
    QVERIFY(id > 0);
    QVERIFY(teacherId > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(id, teacherId)
        ));

    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);
    const auto result = port.readClassCoTeacherPage(classId(id));

    QVERIFY(result);
    QVERIFY(result.value().classId == classId(id));
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().teacherDisplayName);
    const auto& fields = result.value().classFields.value();
    QVERIFY(fields.selectedTeacherId.has_value());
    QVERIFY(fields.selectedTeacherId.value()
        == *Domain::TeacherId::fromString(std::to_string(teacherId)));
    QVERIFY(fields.classGrade == u"E4");
    QVERIFY(fields.classLevel == u"Theseus");
    QVERIFY((fields.regularSchedule == std::vector<
        Application::ClassCoTeacherPageScheduleRow>{
            {u"Monday", u"4:00 PM"},
            {u"Wednesday", u"4:00 PM"}
        }));
    QVERIFY(result.value().teacherDisplayName.value() == u"Preferred Display");
}

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
mapsUnassignedTeacherToEmptyTypedSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Unassigned Co-Teacher"));
    QVERIFY(id > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(id)
        ));

    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);
    const auto result = port.readClassCoTeacherPage(classId(id));

    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(!result.value().classFields.value().selectedTeacherId.has_value());
    QVERIFY(result.value().teacherDisplayName);
    QVERIFY(result.value().teacherDisplayName.value().empty());

    const int missingInfoClass = createClass(
        services,
        QStringLiteral("Class without co-teacher info")
        );
    QVERIFY(missingInfoClass > 0);
    const auto missingInfo =
        port.readClassCoTeacherPage(classId(missingInfoClass));
    QVERIFY(missingInfo);
    QVERIFY(missingInfo.value().classFields);
    QVERIFY(!missingInfo.value().classFields.value().selectedTeacherId);
    QVERIFY(missingInfo.value().classFields.value().classGrade.empty());
    QVERIFY(missingInfo.value().classFields.value().regularSchedule.empty());
    QVERIFY(missingInfo.value().teacherDisplayName);
    QVERIFY(missingInfo.value().teacherDisplayName.value().empty());
}

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
treatsNonpositiveTeacherIdsAsUnassignedAndPreservesClassFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Invalid Teacher ID"));
    QVERIFY(id > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(id)
        ));
    QVERIFY(updateTeacherId(services, id, 0));

    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);
    auto result = port.readClassCoTeacherPage(classId(id));
    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(!result.value().classFields.value().selectedTeacherId);
    QVERIFY(result.value().classFields.value().classGrade == u"E4");
    QVERIFY(result.value().classFields.value().classLevel == u"Theseus");
    QVERIFY((result.value().classFields.value().regularSchedule == std::vector<
        Application::ClassCoTeacherPageScheduleRow>{
            {u"Monday", u"4:00 PM"},
            {u"Wednesday", u"4:00 PM"}
        }));
    QVERIFY(result.value().teacherDisplayName);
    QVERIFY(result.value().teacherDisplayName.value().empty());

    QVERIFY(updateTeacherId(services, id, -2));
    result = port.readClassCoTeacherPage(classId(id));
    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(!result.value().classFields.value().selectedTeacherId);
    QVERIFY(result.value().classFields.value().classGrade == u"E4");
    QVERIFY(result.value().classFields.value().classLevel == u"Theseus");
    QVERIFY((result.value().classFields.value().regularSchedule == std::vector<
        Application::ClassCoTeacherPageScheduleRow>{
            {u"Monday", u"4:00 PM"},
            {u"Wednesday", u"4:00 PM"}
        }));
    QVERIFY(result.value().teacherDisplayName);
    QVERIFY(result.value().teacherDisplayName.value().empty());
}

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
unavailableSessionDoesNotUseDataServiceFallback()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());
    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);

    const auto result = port.readClassCoTeacherPage(classId(42));

    QVERIFY(result);
    QVERIFY(!result.value().classFields);
    QVERIFY(!result.value().teacherDisplayName);
    QCOMPARE(result.value().classFields.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(result.value().teacherDisplayName.error().code,
             Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
classAndTeacherSourceFailuresRemainIndependent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Teacher Read Failure"));
    const int teacherId = createTeacher(services);
    QVERIFY(id > 0);
    QVERIFY(teacherId > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(id, teacherId)
        ));
    QVERIFY(updateTeacherId(services, id, 812345));

    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);
    auto result = port.readClassCoTeacherPage(classId(id));
    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().classFields.value().selectedTeacherId.has_value());
    QVERIFY(!result.value().teacherDisplayName);
    QCOMPARE(result.value().teacherDisplayName.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(result.value().classFields.value().classGrade == u"E4");

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE class_times")),
             qPrintable(query.lastError().text()));
    result = port.readClassCoTeacherPage(classId(id));
    QVERIFY(result);
    QVERIFY(!result.value().classFields);
    QCOMPARE(result.value().classFields.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.value().teacherDisplayName);
}

QTEST_MAIN(NextPlatformApplicationServicesClassCoTeacherPageReadPortTests)

#include "next_platform_application_services_class_co_teacher_page_read_port_tests.moc"
