#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/platform/application_services_class_notes_page_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-notes-page-read-%1.tps").arg(
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

ClassInfo makeClassInfo(const int id)
{
    ClassInfo info;
    info.classId = id;
    info.teacherId = -1;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
         QStringLiteral("4:50 PM")},
        {QStringLiteral("Wednesday"), QStringLiteral("4:00 PM"),
         QStringLiteral("4:50 PM")}
    };
    info.notes = QStringLiteral("  exact notes \U0001F642  ");
    info.timeFillerActivities = QStringLiteral("  exact activities  ");
    return info;
}

}

class NextPlatformApplicationServicesClassNotesPageReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsProjectedFieldsAndPreferredTeacherNameFromActiveSession();
    void unavailableSessionDoesNotUseDataServiceFallback();
    void skipsNonpositiveTeacherId();
    void classFieldFailureIsReportedIndependently();
    void teacherFailurePreservesClassTextFields();
};

void NextPlatformApplicationServicesClassNotesPageReadPortTests::
readsProjectedFieldsAndPreferredTeacherNameFromActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Read boundary class"));
    QVERIFY(id > 0);

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("Korean Name");
    teacher.teacherEn = QStringLiteral("English Name");
    teacher.preferredName = QStringLiteral(" Preferred Display ");
    const auto teacherId =
        services.databaseSession()->teacherRepository()->createTeacher(teacher);
    QVERIFY(teacherId);

    ClassInfo info = makeClassInfo(id);
    info.teacherId = *teacherId;
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(info));

    Platform::ApplicationServicesClassNotesPageReadPort port(&services);
    const auto result = port.readClassNotesPage(classId(id));

    QVERIFY(result);
    QVERIFY(result.value().classId == classId(id));
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().teacherDisplayName);
    const auto& fields = result.value().classFields.value();
    QVERIFY(fields.classGrade == u"E4");
    QVERIFY(fields.classLevel == u"Theseus");
    QVERIFY((fields.regularSchedule == std::vector<
        Application::ClassNotesPageScheduleRow>{
            {u"Monday", u"4:00 PM"},
            {u"Wednesday", u"4:00 PM"}
        }));
    QVERIFY(fields.notes == u"  exact notes \U0001F642  ");
    QVERIFY(fields.timeFillerActivities == u"  exact activities  ");
    QVERIFY(result.value().teacherDisplayName.value() == u"Preferred Display");
}

void NextPlatformApplicationServicesClassNotesPageReadPortTests::
unavailableSessionDoesNotUseDataServiceFallback()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());
    Platform::ApplicationServicesClassNotesPageReadPort port(&services);

    const auto result = port.readClassNotesPage(classId(42));

    QVERIFY(result);
    QVERIFY(!result.value().classFields);
    QVERIFY(!result.value().teacherDisplayName);
    QCOMPARE(result.value().classFields.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(result.value().teacherDisplayName.error().code,
             Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesClassNotesPageReadPortTests::
skipsNonpositiveTeacherId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Unassigned class"));
    QVERIFY(id > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        makeClassInfo(id)
        ));

    Platform::ApplicationServicesClassNotesPageReadPort port(&services);
    const auto result = port.readClassNotesPage(classId(id));

    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().teacherDisplayName);
    QVERIFY(result.value().teacherDisplayName.value().empty());
    QVERIFY(result.value().classFields.value().notes == u"  exact notes \U0001F642  ");
}

void NextPlatformApplicationServicesClassNotesPageReadPortTests::
classFieldFailureIsReportedIndependently()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Broken class fields"));
    QVERIFY(id > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        makeClassInfo(id)
        ));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE class_times")),
             qPrintable(query.lastError().text()));

    Platform::ApplicationServicesClassNotesPageReadPort port(&services);
    const auto result = port.readClassNotesPage(classId(id));

    QVERIFY(result);
    QVERIFY(!result.value().classFields);
    QCOMPARE(result.value().classFields.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.value().teacherDisplayName);
    QCOMPARE(result.value().teacherDisplayName.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesClassNotesPageReadPortTests::
teacherFailurePreservesClassTextFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Teacher failure class"));
    QVERIFY(id > 0);

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Teacher to replace");
    const auto teacherId =
        services.databaseSession()->teacherRepository()->createTeacher(teacher);
    QVERIFY(teacherId);
    ClassInfo info = makeClassInfo(id);
    info.teacherId = *teacherId;
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(info));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
             qPrintable(query.lastError().text()));
    query.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"
        ));
    query.addBindValue(812345);
    query.addBindValue(id);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QCOMPARE(query.numRowsAffected(), 1);

    Platform::ApplicationServicesClassNotesPageReadPort port(&services);
    const auto result = port.readClassNotesPage(classId(id));

    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(!result.value().teacherDisplayName);
    QCOMPARE(result.value().teacherDisplayName.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.value().classFields.value().notes == u"  exact notes \U0001F642  ");
    QVERIFY(result.value().classFields.value().timeFillerActivities
        == u"  exact activities  ");
}

QTEST_MAIN(NextPlatformApplicationServicesClassNotesPageReadPortTests)

#include "next_platform_application_services_class_notes_page_read_port_tests.moc"
