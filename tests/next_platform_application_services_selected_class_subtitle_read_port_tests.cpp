#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/platform/application_services_selected_class_subtitle_read_port.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
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
    void readsDisplayNamesForWideAssignedTeacherId();
    void assignedMissingTeacherIsRecoverableNotFound();
    void missingParentsHaveRecoverableComponentErrorsAndLegacyDefaults();
    void existingParentWithoutMetadataReadsOrderedSchedule();
    void missingSessionAndInvalidIdsDoNotFallBackToServices();
    void classAndTeacherFailuresRemainIndependent();
    void malformedPersistedTeacherIdIsValidationAndDoesNotLoadTeacher();
    void metadataFailureRemainsTechnical();
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
        services.workspaceStore()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(teacherId);

    const auto createdClass = services.classService()->create(
        QStringLiteral("Subtitle Test")
        );
    QVERIFY(createdClass);
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
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
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        noTeacherInfo
        ));
    const auto noTeacher = query.execute(classId(*noTeacherClass));
    QVERIFY(noTeacher);
    QVERIFY(noTeacher.value().classFields);
    QVERIFY(noTeacher.value().assignedTeacher);
    QVERIFY(!noTeacher.value().assignedTeacher.value().has_value());
}

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
readsDisplayNamesForWideAssignedTeacherId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const QSqlDatabase database = services.workspaceStore()->database();

    constexpr qlonglong wideTeacherId = 2'147'483'648LL;
    QSqlQuery insertTeacher(database);
    QVERIFY(insertTeacher.prepare(QStringLiteral(R"(
        INSERT INTO teachers (
            id, teacher_kr, teacher_en, preferred_romanization, preferred_name
        ) VALUES (?, ?, ?, ?, ?)
    )")));
    insertTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher Korean"));
    insertTeacher.addBindValue(QStringLiteral("Wide Teacher English"));
    insertTeacher.addBindValue(QStringLiteral("Wide Romanized Name"));
    insertTeacher.addBindValue(QStringLiteral("Wide Preferred Name"));
    QVERIFY2(insertTeacher.exec(), qPrintable(insertTeacher.lastError().text()));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Wide Teacher Subtitle")
        );
    QVERIFY(createdClass);
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, -1)
        ));

    QSqlQuery assignTeacher(database);
    QVERIFY(assignTeacher.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?")));
    assignTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    assignTeacher.addBindValue(*createdClass);
    QVERIFY2(assignTeacher.exec(), qPrintable(assignTeacher.lastError().text()));
    QCOMPARE(assignTeacher.numRowsAffected(), 1);

    QSqlQuery verifyStoredId(database);
    QVERIFY(verifyStoredId.prepare(QStringLiteral(R"(
        SELECT typeof(teachers.id), teachers.id,
               typeof(class_info.teacher_id), class_info.teacher_id
        FROM teachers
        INNER JOIN class_info ON class_info.teacher_id = teachers.id
        WHERE class_info.class_id = ?
    )")));
    verifyStoredId.addBindValue(*createdClass);
    QVERIFY2(verifyStoredId.exec(), qPrintable(verifyStoredId.lastError().text()));
    QVERIFY(verifyStoredId.next());
    QCOMPARE(verifyStoredId.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredId.value(1).toLongLong(), wideTeacherId);
    QCOMPARE(verifyStoredId.value(2).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredId.value(3).toLongLong(), wideTeacherId);
    QVERIFY(!verifyStoredId.next());

    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);
    const auto result = query.execute(classId(*createdClass));
    QVERIFY(result);
    QVERIFY(result.value().classId == classId(*createdClass));
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().assignedTeacher);
    QVERIFY(result.value().assignedTeacher.value().has_value());

    const auto& fields = result.value().assignedTeacher.value().value();
    QCOMPARE(fields.teacherKr, std::u16string(u"Wide Teacher Korean"));
    QCOMPARE(fields.teacherEn, std::u16string(u"Wide Teacher English"));
    QCOMPARE(fields.preferredRomanization, std::u16string(u"Wide Romanized Name"));
    QCOMPARE(fields.preferredName, std::u16string(u"Wide Preferred Name"));
}

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
assignedMissingTeacherIsRecoverableNotFound()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Assigned Missing Subtitle Teacher")
        );
    QVERIFY(createdClass);
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, -1)
        ));

    QSqlQuery assignMissingTeacher(services.workspaceStore()->database());
    QVERIFY2(assignMissingTeacher.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
             qPrintable(assignMissingTeacher.lastError().text()));
    QVERIFY(assignMissingTeacher.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"
        )));
    assignMissingTeacher.addBindValue(812345);
    assignMissingTeacher.addBindValue(*createdClass);
    QVERIFY2(assignMissingTeacher.exec(),
             qPrintable(assignMissingTeacher.lastError().text()));
    QCOMPARE(assignMissingTeacher.numRowsAffected(), 1);

    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);
    const auto result = query.execute(classId(*createdClass));

    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().classFields.value().classGrade
        == std::u16string(u" E4 "));
    QVERIFY(!result.value().assignedTeacher);
    QCOMPARE(result.value().assignedTeacher.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(result.value().assignedTeacher.error().recoverable);
}

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
missingParentsHaveRecoverableComponentErrorsAndLegacyDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    constexpr int neverPresentClassId = 2'000'000'000;
    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);
    const auto neverPresent = query.execute(classId(neverPresentClassId));
    QVERIFY(neverPresent);
    QVERIFY(neverPresent.value().classId == classId(neverPresentClassId));
    QVERIFY(!neverPresent.value().classFields);
    QCOMPARE(neverPresent.value().classFields.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(neverPresent.value().classFields.error().recoverable);
    QVERIFY(!neverPresent.value().assignedTeacher);
    QCOMPARE(neverPresent.value().assignedTeacher.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(neverPresent.value().assignedTeacher.error().recoverable);

    const auto legacyUnknown = services.workspaceStore()->classInfoRepository()
        ->loadClassSubtitleRecord(neverPresentClassId);
    QVERIFY(legacyUnknown);
    QCOMPARE(legacyUnknown->classId, neverPresentClassId);
    QCOMPARE(legacyUnknown->teacherId, qint64(-1));
    QVERIFY(legacyUnknown->grade.isEmpty());
    QVERIFY(legacyUnknown->level.isEmpty());
    QVERIFY(legacyUnknown->regularTimes.isEmpty());

    const auto deletedClass = services.classService()->create(
        QStringLiteral("Deleted Subtitle Parent")
        );
    QVERIFY(deletedClass);
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(*deletedClass, -1)
        ));

    QSqlQuery disableForeignKeys(services.workspaceStore()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));

    QSqlQuery deleteParent(services.workspaceStore()->database());
    QVERIFY(deleteParent.prepare(QStringLiteral(
        "DELETE FROM classes WHERE id = ?")));
    deleteParent.addBindValue(*deletedClass);
    QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
    QCOMPARE(deleteParent.numRowsAffected(), 1);

    const auto deleted = query.execute(classId(*deletedClass));
    QVERIFY(deleted);
    QVERIFY(deleted.value().classId == classId(*deletedClass));
    QVERIFY(!deleted.value().classFields);
    QCOMPARE(deleted.value().classFields.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(deleted.value().classFields.error().recoverable);
    QVERIFY(!deleted.value().assignedTeacher);
    QCOMPARE(deleted.value().assignedTeacher.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(deleted.value().assignedTeacher.error().recoverable);

    const auto legacyDeleted = services.workspaceStore()->classInfoRepository()
        ->loadClassSubtitleRecord(*deletedClass);
    QVERIFY(legacyDeleted);
    QCOMPARE(legacyDeleted->classId, *deletedClass);
    QCOMPARE(legacyDeleted->teacherId, qint64(-1));
    QCOMPARE(legacyDeleted->grade, QStringLiteral(" E4 "));
    QCOMPARE(legacyDeleted->level, QStringLiteral("Theseus"));
    QCOMPARE(legacyDeleted->regularTimes.size(), 2);
    QCOMPARE(legacyDeleted->regularTimes[0].day, QStringLiteral("Monday"));
    QCOMPARE(legacyDeleted->regularTimes[0].startTime,
             QStringLiteral("9:00 AM"));
    QCOMPARE(legacyDeleted->regularTimes[1].day, QStringLiteral("Friday"));
    QCOMPARE(legacyDeleted->regularTimes[1].startTime,
             QStringLiteral("9:00 AM"));
}

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
existingParentWithoutMetadataReadsOrderedSchedule()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("No Subtitle Metadata")
        );
    QVERIFY(createdClass);

    QSqlQuery insertTimes(services.workspaceStore()->database());
    QVERIFY(insertTimes.prepare(QStringLiteral(R"(
        INSERT INTO class_times (
            id, class_id, day, start_time, end_time
        ) VALUES
            (22, ?, 'Friday', '9:00 AM', '9:55 AM'),
            (9, ?, 'Monday', '9:00 AM', '9:55 AM')
    )")));
    insertTimes.addBindValue(*createdClass);
    insertTimes.addBindValue(*createdClass);
    QVERIFY2(insertTimes.exec(), qPrintable(insertTimes.lastError().text()));

    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);
    const auto result = query.execute(classId(*createdClass));
    QVERIFY(result);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().assignedTeacher);
    QVERIFY(!result.value().assignedTeacher.value().has_value());

    const auto& fields = result.value().classFields.value();
    QVERIFY(fields.classGrade.empty());
    QVERIFY(fields.classLevel.empty());
    QCOMPARE(fields.regularSchedule.size(), std::size_t(2));
    QVERIFY((fields.regularSchedule[0] ==
        Application::SelectedClassSubtitleScheduleRow{
            u"Monday", u"9:00 AM"
        }));
    QVERIFY((fields.regularSchedule[1] ==
        Application::SelectedClassSubtitleScheduleRow{
            u"Friday", u"9:00 AM"
        }));
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
        services.workspaceStore()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(teacherId);
    const auto createdClass = services.classService()->create(
        QStringLiteral("Failure Test")
        );
    QVERIFY(createdClass);
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, *teacherId)
        ));

    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);

    QSqlQuery dropTimes(services.workspaceStore()->database());
    QVERIFY(dropTimes.exec(QStringLiteral("DROP TABLE class_times")));
    const auto classFailure = query.execute(classId(*createdClass));
    QVERIFY(classFailure);
    QVERIFY(!classFailure.value().classFields);
    QCOMPARE(classFailure.value().classFields.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(classFailure.value().assignedTeacher);
    QVERIFY(!classFailure.value().assignedTeacher.value().has_value());

    QSqlQuery restoreTimes(services.workspaceStore()->database());
    QVERIFY(restoreTimes.exec(QStringLiteral(
        "CREATE TABLE class_times ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "class_id INTEGER NOT NULL, day TEXT NOT NULL, "
        "start_time TEXT NOT NULL, end_time TEXT NOT NULL)"
        )));

    QSqlQuery disableForeignKeys(services.workspaceStore()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery dropTeachers(services.workspaceStore()->database());
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

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
malformedPersistedTeacherIdIsValidationAndDoesNotLoadTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Must not be looked up");
    const auto teacherId =
        services.workspaceStore()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(teacherId);
    const auto createdClass = services.classService()->create(
        QStringLiteral("Malformed Persisted Subtitle Teacher")
        );
    QVERIFY(createdClass);
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, *teacherId)
        ));

    const QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery disableForeignKeys(database);
    QVERIFY2(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    QSqlQuery corruptTeacherId(database);
    QVERIFY(corruptTeacherId.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?")));
    corruptTeacherId.addBindValue(QStringLiteral("not-a-number"));
    corruptTeacherId.addBindValue(*createdClass);
    QVERIFY2(corruptTeacherId.exec(),
             qPrintable(corruptTeacherId.lastError().text()));
    QCOMPARE(corruptTeacherId.numRowsAffected(), 1);

    QSqlQuery verifyStoredId(database);
    QVERIFY(verifyStoredId.prepare(QStringLiteral(
        "SELECT typeof(teacher_id), teacher_id FROM class_info WHERE class_id = ?")));
    verifyStoredId.addBindValue(*createdClass);
    QVERIFY2(verifyStoredId.exec(), qPrintable(verifyStoredId.lastError().text()));
    QVERIFY(verifyStoredId.next());
    QCOMPARE(verifyStoredId.value(0).toString(), QStringLiteral("text"));
    QCOMPARE(verifyStoredId.value(1).toString(), QStringLiteral("not-a-number"));
    QVERIFY(!verifyStoredId.next());

    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);
    const auto result = query.execute(classId(*createdClass));

    QVERIFY(result);
    QVERIFY(result.value().classId == classId(*createdClass));
    QVERIFY(!result.value().classFields);
    QCOMPARE(result.value().classFields.error().code,
             Domain::ErrorCode::Validation);
    QVERIFY(!result.value().classFields.error().recoverable);
    QCOMPARE(result.value().classFields.error().message,
             std::string("Loading class subtitle failed: the database returned an invalid teacher identifier."));
    QVERIFY(result.value().assignedTeacher);
    QVERIFY(!result.value().assignedTeacher.value().has_value());
}

void NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests::
metadataFailureRemainsTechnical()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Metadata Failure")
        );
    QVERIFY(createdClass);
    QVERIFY(services.workspaceStore()->classInfoRepository()->saveClassInfo(
        classInfo(*createdClass, -1)
        ));

    QSqlQuery dropMetadata(services.workspaceStore()->database());
    QVERIFY(dropMetadata.exec(QStringLiteral("DROP TABLE class_info")));

    Platform::ApplicationServicesSelectedClassSubtitleReadPort port(&services);
    const Application::SelectedClassSubtitleReadQuery query(port);
    const auto result = query.execute(classId(*createdClass));
    QVERIFY(result);
    QVERIFY(!result.value().classFields);
    QCOMPARE(result.value().classFields.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(result.value().assignedTeacher);
    QVERIFY(!result.value().assignedTeacher.value().has_value());
}

QTEST_MAIN(NextPlatformApplicationServicesSelectedClassSubtitleReadPortTests)

#include "next_platform_application_services_selected_class_subtitle_read_port_tests.moc"
