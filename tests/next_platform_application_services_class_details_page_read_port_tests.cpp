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
    void unavailableSessionReturnsIndependentSourceFailures();
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
    teacher.preferredRomanization = QStringLiteral("Preferred Teacher");
    teacher.preferredName = QStringLiteral("Preferred Teacher");
    const auto createdTeacher = services.teacherService()->save(teacher);
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
