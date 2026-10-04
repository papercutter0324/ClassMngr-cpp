#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "next/application/class_details_page_query.h"
#include "next/platform/application_services_class_details_page_read_port.h"

#include <QTemporaryDir>
#include <QUuid>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-page-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

}

class NextPlatformApplicationServicesClassDetailsPageReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsFieldsOrderedRawSchedulesTeacherAndRosterFromSession();
    void missingMetadataRetainsDefaultsAndReadsSchedules();
    void unavailableSessionReturnsIndependentSourceFailures();
    void classReadFailureDoesNotDiscardRosterCount();
    void intensiveScheduleReadFailureDoesNotDiscardRosterCount();
    void teacherReadFailureDoesNotDiscardClassOrRoster();
    void rosterReadFailureDoesNotDiscardClassOrTeacher();
    void nonCanonicalClassIdReturnsInvalidInputOutcomes();
};

void NextPlatformApplicationServicesClassDetailsPageReadPortTests::
readsFieldsOrderedRawSchedulesTeacherAndRosterFromSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Read Test")
        );
    QVERIFY(createdClass);

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("English Name");
    teacher.preferredRomanization = QStringLiteral("Romanized Teacher");
    teacher.preferredName = QStringLiteral("Preferred Teacher");
    const auto createdTeacher =
        services.databaseSession()->teacherRepository()->saveTeacher(teacher);
    QVERIFY(createdTeacher);

    const auto original = services.classService()->classInfo(*createdClass);
    QVERIFY(original);
    ClassInfo info = *original;
    info.teacherId = *createdTeacher;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.readingBook = QStringLiteral("Reading Explorer 1");
    info.essayBook = QStringLiteral("4A");
    info.classColor = QStringLiteral("#112233");
    info.fontColor = QStringLiteral("#445566");
    info.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")}
    };
    info.intensiveTimes = {
        {QStringLiteral("Tuesday"), QStringLiteral("12:00 PM"),
         QStringLiteral("12:55 PM")}
    };
    QVERIFY(services.classService()->saveClassInfo(info));

    // Seed legacy values directly because the current service save validates
    // schedule text; the display read must still copy stored rows unchanged.
    QSqlQuery scheduleRows(services.databaseSession()->database());
    scheduleRows.prepare(QStringLiteral(
        "DELETE FROM class_times WHERE class_id=?"
        ));
    scheduleRows.addBindValue(*createdClass);
    QVERIFY2(scheduleRows.exec(), qPrintable(scheduleRows.lastError().text()));
    const auto insertScheduleRow = [&scheduleRows, createdClass](
        const QString& table,
        const QString& day,
        const QString& start,
        const QString& end
        )
    {
        scheduleRows.prepare(QStringLiteral(
            "INSERT INTO %1 (class_id, day, start_time, end_time) "
            "VALUES (?, ?, ?, ?)"
            ).arg(table));
        scheduleRows.addBindValue(*createdClass);
        scheduleRows.addBindValue(day);
        scheduleRows.addBindValue(start);
        scheduleRows.addBindValue(end);
        return scheduleRows.exec();
    };
    QVERIFY2(
        insertScheduleRow(
            QStringLiteral("class_times"),
            QStringLiteral("Legacy Friday"),
            QStringLiteral("raw start"),
            QStringLiteral("raw end")
            ),
        qPrintable(scheduleRows.lastError().text())
        );
    QVERIFY2(
        insertScheduleRow(
            QStringLiteral("class_times"),
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
            ),
        qPrintable(scheduleRows.lastError().text())
        );
    scheduleRows.prepare(QStringLiteral(
        "DELETE FROM class_intensive_times WHERE class_id=?"
        ));
    scheduleRows.addBindValue(*createdClass);
    QVERIFY2(scheduleRows.exec(), qPrintable(scheduleRows.lastError().text()));
    QVERIFY2(
        insertScheduleRow(
            QStringLiteral("class_intensive_times"),
            QStringLiteral("not a weekday"),
            QStringLiteral("??"),
            QStringLiteral("end text")
            ),
        qPrintable(scheduleRows.lastError().text())
        );

    Roster roster;
    roster.columns = Roster::BaseColumns;
    QStringList student(Roster::BaseColumns.size(), QString());
    student[0] = QStringLiteral("Student One");
    student[1] = QStringLiteral("학생 하나");
    roster.rows.append(student);
    QVERIFY(services.rosterService()->saveRoster(*createdClass, roster));

    Platform::ApplicationServicesClassDetailsPageReadPort port(services);
    const Application::ClassDetailsPageQuery query(port);
    const auto loaded = query.execute(classId(*createdClass));

    QVERIFY(loaded);
    const auto& snapshot = loaded.value();
    QVERIFY(snapshot.classId == classId(*createdClass));
    QVERIFY(snapshot.classFields);
    QVERIFY(snapshot.teacherDisplayName);
    QVERIFY(snapshot.studentCount);

    const auto& fields = snapshot.classFields.value();
    QCOMPARE(fields.classGrade, std::string("E4"));
    QCOMPARE(fields.classLevel, std::string("Theseus"));
    QCOMPARE(fields.readingBook, std::string("Reading Explorer 1"));
    QCOMPARE(fields.essayBook, std::string("4A"));
    QCOMPARE(fields.classColor, std::string("#112233"));
    QCOMPARE(fields.fontColor, std::string("#445566"));
    QCOMPARE(fields.regularSchedule.size(), std::size_t(2));
    QVERIFY((fields.regularSchedule[0] ==
        Application::ClassDetailsPageScheduleRow{
            "Legacy Friday", "raw start", "raw end"
        }));
    QVERIFY((fields.regularSchedule[1] ==
        Application::ClassDetailsPageScheduleRow{
            "Monday", "9:00 AM", "9:55 AM"
        }));
    QCOMPARE(fields.intensiveSchedule.size(), std::size_t(1));
    QVERIFY((fields.intensiveSchedule[0] ==
        Application::ClassDetailsPageScheduleRow{
            "not a weekday", "??", "end text"
        }));
    QCOMPARE(snapshot.teacherDisplayName.value(),
             std::string("Preferred Teacher"));
    QCOMPARE(snapshot.studentCount.value(), 1);

    const auto& metrics = services.databaseSession()->classInfoRepository()
                              ->classDetailsPageReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.scheduleStatementCount, 1);

    const auto classWithoutTeacher = services.classService()->create(
        QStringLiteral("Class Details Read Fallback")
        );
    QVERIFY(classWithoutTeacher);
    const auto fallback = query.execute(classId(*classWithoutTeacher));
    QVERIFY(fallback);
    QVERIFY(fallback.value().classFields);
    QVERIFY(fallback.value().teacherDisplayName);
    QVERIFY(fallback.value().teacherDisplayName.value().empty());
    QVERIFY(fallback.value().studentCount);
    QCOMPARE(fallback.value().studentCount.value(), 0);
}

void NextPlatformApplicationServicesClassDetailsPageReadPortTests::
missingMetadataRetainsDefaultsAndReadsSchedules()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Without Metadata")
        );
    QVERIFY(createdClass);
    QSqlQuery removeMetadata(services.databaseSession()->database());
    removeMetadata.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id=?"
        ));
    removeMetadata.addBindValue(*createdClass);
    QVERIFY2(removeMetadata.exec(), qPrintable(removeMetadata.lastError().text()));

    const auto insertSchedule = [
        database = services.databaseSession()->database(), createdClass
        ](
        const QString& table,
        const QString& day,
        const QString& start,
        const QString& end
        )
    {
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO %1 (class_id, day, start_time, end_time) "
            "VALUES (?, ?, ?, ?)"
            ).arg(table));
        query.addBindValue(*createdClass);
        query.addBindValue(day);
        query.addBindValue(start);
        query.addBindValue(end);
        return query.exec();
    };
    QVERIFY(insertSchedule(
        QStringLiteral("class_times"),
        QStringLiteral("Legacy regular day"),
        QStringLiteral("raw regular start"),
        QStringLiteral("raw regular end")
        ));
    QVERIFY(insertSchedule(
        QStringLiteral("class_intensive_times"),
        QStringLiteral("Legacy intensive day"),
        QStringLiteral("raw intensive start"),
        QStringLiteral("raw intensive end")
        ));

    Platform::ApplicationServicesClassDetailsPageReadPort port(services);
    const Application::ClassDetailsPageQuery query(port);
    const auto loaded = query.execute(classId(*createdClass));
    QVERIFY(loaded);
    const auto& snapshot = loaded.value();
    QVERIFY(snapshot.classFields);
    QVERIFY(snapshot.teacherDisplayName);
    QVERIFY(snapshot.teacherDisplayName.value().empty());
    QVERIFY(snapshot.studentCount);
    QCOMPARE(snapshot.studentCount.value(), 0);

    const auto& fields = snapshot.classFields.value();
    QVERIFY(fields.classGrade.empty());
    QVERIFY(fields.classLevel.empty());
    QVERIFY(fields.readingBook.empty());
    QVERIFY(fields.essayBook.empty());
    QCOMPARE(fields.classColor, std::string("#FFFFFF"));
    QCOMPARE(fields.fontColor, std::string("#000000"));
    QCOMPARE(fields.regularSchedule.size(), std::size_t(1));
    QVERIFY((fields.regularSchedule.front() ==
        Application::ClassDetailsPageScheduleRow{
            "Legacy regular day", "raw regular start", "raw regular end"
        }));
    QCOMPARE(fields.intensiveSchedule.size(), std::size_t(1));
    QVERIFY((fields.intensiveSchedule.front() ==
        Application::ClassDetailsPageScheduleRow{
            "Legacy intensive day", "raw intensive start", "raw intensive end"
        }));

    const auto& metrics = services.databaseSession()->classInfoRepository()
                              ->classDetailsPageReadMetrics();
    QCOMPARE(metrics.callCount, 1);
    QCOMPARE(metrics.metadataStatementCount, 1);
    QCOMPARE(metrics.scheduleStatementCount, 1);
}

void NextPlatformApplicationServicesClassDetailsPageReadPortTests::
unavailableSessionReturnsIndependentSourceFailures()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassDetailsPageReadPort port(services);

    const auto result = port.readClassDetailsPage(classId(42));

    QVERIFY(result);
    const auto& snapshot = result.value();
    QVERIFY(!snapshot.classFields);
    QCOMPARE(snapshot.classFields.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!snapshot.teacherDisplayName);
    QCOMPARE(snapshot.teacherDisplayName.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(!snapshot.studentCount);
    QCOMPARE(snapshot.studentCount.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(snapshot.classFields.error().message)
        .contains(QStringLiteral("unavailable")));
}

void NextPlatformApplicationServicesClassDetailsPageReadPortTests::
classReadFailureDoesNotDiscardRosterCount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlQuery damageClassRead(services.databaseSession()->database());
    QVERIFY2(
        damageClassRead.exec(QStringLiteral("DROP TABLE class_times")),
        qPrintable(damageClassRead.lastError().text())
        );

    Platform::ApplicationServicesClassDetailsPageReadPort port(services);
    const auto result = port.readClassDetailsPage(classId(42));

    QVERIFY(result);
    const auto& snapshot = result.value();
    QVERIFY(!snapshot.classFields);
    QCOMPARE(snapshot.classFields.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(snapshot.classFields.error().message)
        .contains(QStringLiteral("Loading regular class times")));
    QVERIFY(!snapshot.teacherDisplayName);
    QVERIFY(snapshot.studentCount);
    QCOMPARE(snapshot.studentCount.value(), 0);
}

void NextPlatformApplicationServicesClassDetailsPageReadPortTests::
intensiveScheduleReadFailureDoesNotDiscardRosterCount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlQuery damageClassRead(services.databaseSession()->database());
    QVERIFY2(
        damageClassRead.exec(QStringLiteral(
            "DROP TABLE class_intensive_times")),
        qPrintable(damageClassRead.lastError().text())
        );

    Platform::ApplicationServicesClassDetailsPageReadPort port(services);
    const auto result = port.readClassDetailsPage(classId(42));

    QVERIFY(result);
    const auto& snapshot = result.value();
    QVERIFY(!snapshot.classFields);
    QCOMPARE(snapshot.classFields.error().code, Domain::ErrorCode::Technical);
    const QString classFieldError = QString::fromStdString(
        snapshot.classFields.error().message);
    QVERIFY2(classFieldError.contains(
                 QStringLiteral("Loading intensive class times")),
             qPrintable(classFieldError));
    QVERIFY(!snapshot.teacherDisplayName);
    QVERIFY(snapshot.studentCount);
    QCOMPARE(snapshot.studentCount.value(), 0);
}

void NextPlatformApplicationServicesClassDetailsPageReadPortTests::
teacherReadFailureDoesNotDiscardClassOrRoster()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Teacher Failure")
        );
    QVERIFY(createdClass);
    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Teacher English Name");
    const auto createdTeacher = services.teacherService()->save(teacher);
    QVERIFY(createdTeacher);
    const auto original = services.classService()->classInfo(*createdClass);
    QVERIFY(original);
    ClassInfo info = *original;
    info.teacherId = *createdTeacher;
    QVERIFY(services.classService()->saveClassInfo(info));

    QSqlQuery damageTeacherRead(services.databaseSession()->database());
    QVERIFY2(
        damageTeacherRead.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
        qPrintable(damageTeacherRead.lastError().text())
        );
    damageTeacherRead.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"
        ));
    damageTeacherRead.addBindValue(987654);
    damageTeacherRead.addBindValue(*createdClass);
    QVERIFY2(
        damageTeacherRead.exec(),
        qPrintable(damageTeacherRead.lastError().text())
        );

    Platform::ApplicationServicesClassDetailsPageReadPort port(services);
    const auto result = port.readClassDetailsPage(classId(*createdClass));

    QVERIFY(result);
    const auto& snapshot = result.value();
    QVERIFY(snapshot.classFields);
    QVERIFY(!snapshot.teacherDisplayName);
    QCOMPARE(snapshot.teacherDisplayName.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(snapshot.studentCount);
    QCOMPARE(snapshot.studentCount.value(), 0);
}

void NextPlatformApplicationServicesClassDetailsPageReadPortTests::
rosterReadFailureDoesNotDiscardClassOrTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Roster Failure")
        );
    QVERIFY(createdClass);

    QSqlQuery damageRosterRead(services.databaseSession()->database());
    QVERIFY2(
        damageRosterRead.exec(QStringLiteral("DROP TABLE roster_columns")),
        qPrintable(damageRosterRead.lastError().text())
        );

    Platform::ApplicationServicesClassDetailsPageReadPort port(services);
    const auto result = port.readClassDetailsPage(classId(*createdClass));

    QVERIFY(result);
    const auto& snapshot = result.value();
    QVERIFY(snapshot.classFields);
    QVERIFY(snapshot.teacherDisplayName);
    QVERIFY(snapshot.teacherDisplayName.value().empty());
    QVERIFY(!snapshot.studentCount);
}

void NextPlatformApplicationServicesClassDetailsPageReadPortTests::
nonCanonicalClassIdReturnsInvalidInputOutcomes()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassDetailsPageReadPort port(services);
    const auto nonCanonical = Domain::ClassId::fromString("042");
    QVERIFY(nonCanonical);

    const auto result = port.readClassDetailsPage(*nonCanonical);

    QVERIFY(result);
    QVERIFY(!result.value().classFields);
    QCOMPARE(result.value().classFields.error().code,
             Domain::ErrorCode::InvalidInput);
    QVERIFY(!result.value().teacherDisplayName);
    QVERIFY(!result.value().studentCount);
}

QTEST_MAIN(NextPlatformApplicationServicesClassDetailsPageReadPortTests)

#include "next_platform_application_services_class_details_page_read_port_tests.moc"
