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

bool insertRegularTime(
    ApplicationServices& services,
    const int classIdValue,
    const QString& day,
    const QString& startTime
    )
{
    QSqlQuery query(services.databaseSession()->database());
    query.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id, day, start_time) "
        "VALUES (?, ?, ?)"
        ));
    query.addBindValue(classIdValue);
    query.addBindValue(day);
    query.addBindValue(startTime);
    return query.exec();
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
    void metadataQueryFailureKeepsPageErrorMapping();
    void teacherDisplayNameUsesTrimmedFallbackOrder();
    void teacherProjectionFailurePreservesSelectedTeacherAndClassFields();
    void missingClassInfoKeepsScheduleAndUnknownClassUsesDefaults();
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

    const auto& metrics = services.databaseSession()->classInfoRepository()
                              ->classPageDetailsReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.regularScheduleStatementCount, 1);
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
    QVERIFY(result.value().classFields.value().selectedTeacherId.value()
        == *Domain::TeacherId::fromString("812345"));
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
    QVERIFY(result.value().classFields.error().message.starts_with(
        "Loading regular class times failed"
        ));
    QCOMPARE(result.value().teacherDisplayName.error().message,
             std::string("The co-teacher display name is unavailable."));
}

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
metadataQueryFailureKeepsPageErrorMapping()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Broken co-teacher metadata"));
    QVERIFY(id > 0);

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE class_info")),
             qPrintable(query.lastError().text()));

    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);
    const auto result = port.readClassCoTeacherPage(classId(id));

    QVERIFY(result);
    QVERIFY(!result.value().classFields);
    QCOMPARE(result.value().classFields.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.value().classFields.error().message.starts_with(
        "Loading class information failed"
        ));
    QVERIFY(!result.value().teacherDisplayName);
    QCOMPARE(result.value().teacherDisplayName.error().code,
             Domain::ErrorCode::NotFound);
    QCOMPARE(result.value().teacherDisplayName.error().message,
             std::string("The co-teacher display name is unavailable."));

    const auto& metrics = services.databaseSession()->classInfoRepository()
                              ->classPageDetailsReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.regularScheduleStatementCount, 0);
}

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
teacherDisplayNameUsesTrimmedFallbackOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    std::vector<Teacher> teachers(3);
    teachers[0].teacherKr = QStringLiteral("Korean fallback");
    teachers[0].teacherEn = QStringLiteral("  English fallback  ");
    teachers[0].preferredRomanization = QStringLiteral("Romanization fallback");
    teachers[0].preferredName = QStringLiteral(" \t ");
    teachers[1].teacherKr = QStringLiteral("Korean fallback");
    teachers[1].teacherEn = QStringLiteral(" \t ");
    teachers[1].preferredRomanization =
        QStringLiteral("  Romanization fallback  ");
    teachers[1].preferredName = QStringLiteral(" ");
    teachers[2].teacherKr = QStringLiteral("  Korean fallback  ");
    teachers[2].teacherEn = QStringLiteral(" ");
    teachers[2].preferredRomanization = QStringLiteral(" \t ");
    teachers[2].preferredName = QStringLiteral(" ");
    const std::vector<std::u16string> expectedNames = {
        u"English fallback",
        u"Romanization fallback",
        u"Korean fallback"
    };

    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);
    for (std::size_t index = 0; index < teachers.size(); ++index)
    {
        const int id = createClass(
            services,
            QStringLiteral("Co-teacher fallback class %1").arg(
                static_cast<qulonglong>(index)
                )
            );
        const auto teacherId = services.databaseSession()
                                   ->teacherRepository()
                                   ->createTeacher(teachers[index]);
        QVERIFY(id > 0);
        QVERIFY(teacherId);
        QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
            classInfo(id, *teacherId)
            ));

        const auto result = port.readClassCoTeacherPage(classId(id));
        QVERIFY(result);
        QVERIFY(result.value().classFields);
        QVERIFY(result.value().teacherDisplayName);
        QVERIFY(result.value().classFields.value().selectedTeacherId.has_value());
        QVERIFY(result.value().teacherDisplayName.value()
            == expectedNames[index]);
    }
}

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
teacherProjectionFailurePreservesSelectedTeacherAndClassFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(
        services,
        QStringLiteral("Co-teacher projection failure")
        );
    const int teacherId = createTeacher(services);
    QVERIFY(id > 0);
    QVERIFY(teacherId > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(id, teacherId)
        ));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral(
                 "ALTER TABLE teachers RENAME COLUMN preferred_romanization "
                 "TO legacy_preferred_romanization"
                 )),
             qPrintable(query.lastError().text()));

    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);
    const auto result = port.readClassCoTeacherPage(classId(id));

    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().classFields.value().selectedTeacherId.has_value());
    QVERIFY(result.value().classFields.value().selectedTeacherId.value()
        == *Domain::TeacherId::fromString(std::to_string(teacherId)));
    QVERIFY(!result.value().teacherDisplayName);
    QCOMPARE(result.value().teacherDisplayName.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(result.value().classFields.value().classGrade == u"E4");
    QVERIFY(result.value().classFields.value().classLevel == u"Theseus");
}

void NextPlatformApplicationServicesClassCoTeacherPageReadPortTests::
missingClassInfoKeepsScheduleAndUnknownClassUsesDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(
        services,
        QStringLiteral("Schedule without co-teacher metadata")
        );
    QVERIFY(id > 0);
    QSqlQuery metadataQuery(services.databaseSession()->database());
    metadataQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_id=?"
        ));
    metadataQuery.addBindValue(id);
    QVERIFY2(metadataQuery.exec(), qPrintable(metadataQuery.lastError().text()));
    QVERIFY(metadataQuery.next());
    QCOMPARE(metadataQuery.value(0).toInt(), 0);
    QVERIFY(insertRegularTime(
        services,
        id,
        QStringLiteral("Wednesday"),
        QStringLiteral("2:00 PM")
        ));
    QVERIFY(insertRegularTime(
        services,
        id,
        QStringLiteral("Monday"),
        QStringLiteral("1:00 PM")
        ));

    Platform::ApplicationServicesClassCoTeacherPageReadPort port(&services);
    const auto scheduled = port.readClassCoTeacherPage(classId(id));
    QVERIFY(scheduled);
    QVERIFY(scheduled.value().classFields);
    QVERIFY(scheduled.value().teacherDisplayName);
    const auto& fields = scheduled.value().classFields.value();
    QVERIFY(!fields.selectedTeacherId);
    QVERIFY(fields.classGrade.empty());
    QVERIFY(fields.classLevel.empty());
    QVERIFY((fields.regularSchedule == std::vector<
        Application::ClassCoTeacherPageScheduleRow>{
            {u"Wednesday", u"2:00 PM"},
            {u"Monday", u"1:00 PM"}
        }));
    QVERIFY(scheduled.value().teacherDisplayName.value().empty());

    const auto unknown = port.readClassCoTeacherPage(classId(812345));
    QVERIFY(unknown);
    QVERIFY(unknown.value().classId == classId(812345));
    QVERIFY(unknown.value().classFields);
    QVERIFY(unknown.value().teacherDisplayName);
    QVERIFY(!unknown.value().classFields.value().selectedTeacherId);
    QVERIFY(unknown.value().classFields.value().classGrade.empty());
    QVERIFY(unknown.value().classFields.value().classLevel.empty());
    QVERIFY(unknown.value().classFields.value().regularSchedule.empty());
    QVERIFY(unknown.value().teacherDisplayName.value().empty());
}

QTEST_MAIN(NextPlatformApplicationServicesClassCoTeacherPageReadPortTests)

#include "next_platform_application_services_class_co_teacher_page_read_port_tests.moc"
