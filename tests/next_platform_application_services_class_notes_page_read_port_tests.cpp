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
#include <vector>

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

class NextPlatformApplicationServicesClassNotesPageReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsProjectedFieldsAndPreferredTeacherNameFromActiveSession();
    void unavailableSessionDoesNotUseDataServiceFallback();
    void skipsNonpositiveTeacherId();
    void missingClassInfoKeepsScheduleAndUnknownClassUsesDefaults();
    void teacherDisplayNameUsesTrimmedFallbackOrder();
    void classFieldFailureIsReportedIndependently();
    void metadataQueryFailureKeepsPageErrorMapping();
    void teacherFailurePreservesClassTextFields();
    void teacherProjectionFailurePreservesClassTextFields();
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

    const auto& metrics = services.databaseSession()->classInfoRepository()
                              ->classPageDetailsReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.regularScheduleStatementCount, 1);
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
missingClassInfoKeepsScheduleAndUnknownClassUsesDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Schedule without metadata"));
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

    Platform::ApplicationServicesClassNotesPageReadPort port(&services);
    const auto scheduled = port.readClassNotesPage(classId(id));
    QVERIFY(scheduled);
    QVERIFY(scheduled.value().classFields);
    QVERIFY(scheduled.value().teacherDisplayName);
    const auto& fields = scheduled.value().classFields.value();
    QVERIFY(fields.classGrade.empty());
    QVERIFY(fields.classLevel.empty());
    QVERIFY(fields.notes.empty());
    QVERIFY(fields.timeFillerActivities.empty());
    QVERIFY((fields.regularSchedule == std::vector<
        Application::ClassNotesPageScheduleRow>{
            {u"Wednesday", u"2:00 PM"},
            {u"Monday", u"1:00 PM"}
        }));
    QVERIFY(scheduled.value().teacherDisplayName.value().empty());

    const auto unknown = port.readClassNotesPage(classId(812345));
    QVERIFY(unknown);
    QVERIFY(unknown.value().classId == classId(812345));
    QVERIFY(unknown.value().classFields);
    QVERIFY(unknown.value().teacherDisplayName);
    QVERIFY(unknown.value().classFields.value().classGrade.empty());
    QVERIFY(unknown.value().classFields.value().classLevel.empty());
    QVERIFY(unknown.value().classFields.value().regularSchedule.empty());
    QVERIFY(unknown.value().classFields.value().notes.empty());
    QVERIFY(unknown.value().classFields.value().timeFillerActivities.empty());
    QVERIFY(unknown.value().teacherDisplayName.value().empty());
}

void NextPlatformApplicationServicesClassNotesPageReadPortTests::
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

    Platform::ApplicationServicesClassNotesPageReadPort port(&services);
    for (std::size_t index = 0; index < teachers.size(); ++index)
    {
        const int id = createClass(
            services,
            QStringLiteral("Teacher fallback class %1").arg(
                static_cast<qulonglong>(index)
                )
            );
        QVERIFY(id > 0);
        const auto teacherId = services.databaseSession()
                                   ->teacherRepository()
                                   ->createTeacher(teachers[index]);
        QVERIFY(teacherId);
        ClassInfo info = makeClassInfo(id);
        info.teacherId = *teacherId;
        QVERIFY(services.databaseSession()->classInfoRepository()
                    ->saveClassInfo(info));

        const auto result = port.readClassNotesPage(classId(id));
        QVERIFY(result);
        QVERIFY(result.value().classFields);
        QVERIFY(result.value().teacherDisplayName);
        QVERIFY(result.value().teacherDisplayName.value()
            == expectedNames[index]);
    }
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
    QVERIFY(result.value().classFields.error().message.starts_with(
        "Loading regular class times failed"
        ));
    QCOMPARE(result.value().teacherDisplayName.error().message,
             std::string("The class teacher display name is unavailable."));
}

void NextPlatformApplicationServicesClassNotesPageReadPortTests::
metadataQueryFailureKeepsPageErrorMapping()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Broken class metadata"));
    QVERIFY(id > 0);

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE class_info")),
             qPrintable(query.lastError().text()));

    Platform::ApplicationServicesClassNotesPageReadPort port(&services);
    const auto result = port.readClassNotesPage(classId(id));

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
             std::string("The class teacher display name is unavailable."));

    const auto& metrics = services.databaseSession()->classInfoRepository()
                              ->classPageDetailsReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.regularScheduleStatementCount, 0);
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

void NextPlatformApplicationServicesClassNotesPageReadPortTests::
teacherProjectionFailurePreservesClassTextFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(
        services,
        QStringLiteral("Teacher projection failure")
        );
    QVERIFY(id > 0);

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Teacher name");
    const auto teacherId =
        services.databaseSession()->teacherRepository()->createTeacher(teacher);
    QVERIFY(teacherId);
    ClassInfo info = makeClassInfo(id);
    info.teacherId = *teacherId;
    QVERIFY(services.databaseSession()->classInfoRepository()
                ->saveClassInfo(info));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral(
                 "ALTER TABLE teachers RENAME COLUMN preferred_romanization "
                 "TO legacy_preferred_romanization"
                 )),
             qPrintable(query.lastError().text()));

    Platform::ApplicationServicesClassNotesPageReadPort port(&services);
    const auto result = port.readClassNotesPage(classId(id));

    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(!result.value().teacherDisplayName);
    QCOMPARE(result.value().teacherDisplayName.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(result.value().classFields.value().notes == u"  exact notes \U0001F642  ");
    QVERIFY(result.value().classFields.value().timeFillerActivities
        == u"  exact activities  ");
}

QTEST_MAIN(NextPlatformApplicationServicesClassNotesPageReadPortTests)

#include "next_platform_application_services_class_notes_page_read_port_tests.moc"
