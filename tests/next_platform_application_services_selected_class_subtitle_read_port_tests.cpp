#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/platform/application_services_selected_class_subtitle_read_port.h"

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
        QStringLiteral("selected-class-subtitle-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

ClassInfo classInfo(
    const int id,
    const int teacherId
    )
{
    ClassInfo info;
    info.classId = id;
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral(" E4 ");
    info.classLevel = QStringLiteral("Theseus");
    info.readingBook = QStringLiteral("Must not cross the subtitle boundary");
    info.classTimes = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
        },
        {
            QStringLiteral("Friday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
        }
    };
    info.intensiveTimes = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("12:00 PM"),
            QStringLiteral("12:55 PM")
        }
    };
    return info;
}

}

class NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsOnlySubtitleProjectionFromActiveSessionRepositories();
    void missingSessionAndInvalidIdsDoNotFallBackToServices();
    void classAndTeacherFailuresRemainIndependent();
};

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
readsOnlySubtitleProjectionFromActiveSessionRepositories()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("Korean Name");
    teacher.teacherEn = QStringLiteral("English Name");
    teacher.preferredRomanization = QStringLiteral("Romanized Name");
    teacher.preferredName = QStringLiteral("Preferred Name");
    teacher.roomNumber = QStringLiteral("Not part of the projection");
    const auto teacherId =
        services.databaseSession()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(teacherId);

    const auto createdClass = services.classService()->create(
        QStringLiteral("Subtitle Test")
        );
    QVERIFY(createdClass);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, *teacherId)
        ));

    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);
    const auto result = query.execute(classId(*createdClass));

    QVERIFY(result);
    const auto& snapshot = result.value();
    QVERIFY(snapshot.classFields);
    QVERIFY(snapshot.assignedTeacher);
    QVERIFY(snapshot.assignedTeacher.value().has_value());

    const auto& fields = snapshot.classFields.value();
    QCOMPARE(fields.classGrade, std::u16string(u" E4 "));
    QCOMPARE(fields.classLevel, std::u16string(u"Theseus"));
    QCOMPARE(fields.regularSchedule.size(), std::size_t(2));
    QVERIFY((fields.regularSchedule[0] ==
        Application::SelectedClassSubtitleScheduleRow{
            u"Monday", u"9:00 AM"
        }));
    QVERIFY((fields.regularSchedule[1] ==
        Application::SelectedClassSubtitleScheduleRow{
            u"Friday", u"9:00 AM"
        }));
    const auto& teacherFields = snapshot.assignedTeacher.value().value();
    QCOMPARE(teacherFields.teacherKr, std::u16string(u"Korean Name"));
    QCOMPARE(teacherFields.teacherEn, std::u16string(u"English Name"));
    QCOMPARE(teacherFields.preferredRomanization,
             std::u16string(u"Romanized Name"));
    QCOMPARE(teacherFields.preferredName, std::u16string(u"Preferred Name"));

    const auto noTeacherClass = services.classService()->create(
        QStringLiteral("No Teacher")
        );
    QVERIFY(noTeacherClass);
    ClassInfo noTeacherInfo = classInfo(*noTeacherClass, -1);
    noTeacherInfo.intensiveTimes.clear();
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        noTeacherInfo
        ));
    const auto noTeacher = query.execute(classId(*noTeacherClass));
    QVERIFY(noTeacher);
    QVERIFY(noTeacher.value().classFields);
    QVERIFY(noTeacher.value().assignedTeacher);
    QVERIFY(!noTeacher.value().assignedTeacher.value().has_value());
}

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
missingSessionAndInvalidIdsDoNotFallBackToServices()
{
    ApplicationServices services;
    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);

    const auto noSession = port.readSelectedClassSubtitle(classId(42));
    QVERIFY(!noSession);
    QCOMPARE(noSession.error().code, Domain::ErrorCode::NotFound);

    const auto nonCanonical = Domain::ClassId::fromString("042");
    QVERIFY(nonCanonical);
    const auto invalid = port.readSelectedClassSubtitle(*nonCanonical);
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, Domain::ErrorCode::InvalidInput);

    Platform::ApplicationServicesSelectedClassSubtitleReadPort nullPort(nullptr);
    const auto missingServices =
        nullPort.readSelectedClassSubtitle(classId(42));
    QVERIFY(!missingServices);
    QCOMPARE(missingServices.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
classAndTeacherFailuresRemainIndependent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Available Teacher");
    const auto teacherId =
        services.databaseSession()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(teacherId);
    const auto createdClass = services.classService()->create(
        QStringLiteral("Failure Test")
        );
    QVERIFY(createdClass);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, *teacherId)
        ));

    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);

    QSqlQuery dropTimes(services.databaseSession()->database());
    QVERIFY(dropTimes.exec(QStringLiteral("DROP TABLE class_times")));
    const auto classFailure = query.execute(classId(*createdClass));
    QVERIFY(classFailure);
    QVERIFY(!classFailure.value().classFields);
    QVERIFY(classFailure.value().assignedTeacher);
    QVERIFY(!classFailure.value().assignedTeacher.value().has_value());

    QSqlQuery restoreTimes(services.databaseSession()->database());
    QVERIFY(restoreTimes.exec(QStringLiteral(
        "CREATE TABLE class_times ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "class_id INTEGER NOT NULL, day TEXT NOT NULL, "
        "start_time TEXT NOT NULL, end_time TEXT NOT NULL)"
        )));

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")));
    const auto teacherFailure = query.execute(classId(*createdClass));
    QVERIFY(teacherFailure);
    QVERIFY(teacherFailure.value().classFields);
    QCOMPARE(teacherFailure.value().classFields.value().classGrade,
             std::u16string(u" E4 "));
    QVERIFY(!teacherFailure.value().assignedTeacher);
    QCOMPARE(teacherFailure.value().assignedTeacher.error().code,
             Domain::ErrorCode::Technical);
}

QTEST_MAIN(NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests)

#include "next_platform_application_services_selected_class_subtitle_read_port_tests.moc"
