#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_details_page.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/sections/class_details_section.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"

#include <QLineEdit>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{
QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-read-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto result = services.classService()->create(name);
    return result ? *result : -1;
}

QLineEdit* studentCountEdit(ClassDetailsPage& page)
{
    auto* details = page.findChild<ClassDetailsSection*>();
    return details ? details->findChild<QLineEdit*>() : nullptr;
}

Roster rosterWithStudents(const QStringList& englishNames)
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    for (qsizetype index = 0; index < englishNames.size(); ++index)
    {
        QStringList row(Roster::BaseColumns.size(), QString());
        row[0] = englishNames.at(index);
        row[1] = index == 0
            ? QStringLiteral("\ud559\uc0dd \ud558\ub098")
            : QStringLiteral("\ud559\uc0dd \ub458");
        roster.rows.append(row);
    }
    return roster;
}
}

class ClassDetailsPageReadParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void displayReadPreservesVisibleFieldsScheduleTeacherAndCount();
    void displayReadUsesNoTeacherFallbackWithoutLosingClassOrCount();
};

void ClassDetailsPageReadParityTests::
displayReadPreservesVisibleFieldsScheduleTeacherAndCount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Read Parity")
        );
    QVERIFY(classId > 0);

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("English Teacher");
    teacher.preferredRomanization = QStringLiteral("Romanized Teacher");
    teacher.preferredName = QStringLiteral("Romanized Teacher");
    const auto createdTeacher = services.teacherService()->create(teacher);
    QVERIFY(createdTeacher);

    const auto infoResult = services.classService()->classInfo(classId);
    QVERIFY(infoResult);
    ClassInfo info = *infoResult;
    info.teacherId = *createdTeacher;
    info.classGrade = QStringLiteral("E5");
    info.classLevel = QStringLiteral("Artemis");
    info.readingBook = QStringLiteral("Reading Explorer 2");
    info.essayBook = QStringLiteral("5A");
    info.classColor = QStringLiteral("#112233");
    info.fontColor = QStringLiteral("#445566");
    info.classTimes = {
        {QStringLiteral("Friday"), QStringLiteral("3:00 PM"),
         QStringLiteral("3:55 PM")},
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:55 AM")}
    };
    info.intensiveTimes = {
        {QStringLiteral("Tuesday"), QStringLiteral("12:00 PM"),
         QStringLiteral("12:55 PM")}
    };
    QVERIFY(services.classService()->saveClassInfo(info));
    QVERIFY(services.rosterService()->saveRoster(
        classId,
        rosterWithStudents({QStringLiteral("Student One"),
                            QStringLiteral("Student Two")})
        ));

    ClassDetailsPage page(&services);
    page.loadClass(Classroom(QStringLiteral("Fallback Name"), classId));

    auto* details = page.findChild<ClassDetailsSection*>();
    auto* schedule = page.findChild<ClassScheduleSection*>();
    auto* header = page.findChild<PageHeader*>();
    auto* countEdit = studentCountEdit(page);
    QVERIFY(details && schedule && header && countEdit);

    QCOMPARE(details->grade(), QStringLiteral("E5"));
    QCOMPARE(details->level(), QStringLiteral("Artemis"));
    QCOMPARE(details->readingBook(), QStringLiteral("Reading Explorer 2"));
    QCOMPARE(details->essayBook(), QStringLiteral("5A"));
    QCOMPARE(details->classColor(), QStringLiteral("#112233"));
    QCOMPARE(details->fontColor(), QStringLiteral("#445566"));
    QCOMPARE(countEdit->text(), QStringLiteral("2"));
    QCOMPARE(schedule->regularRows().size(), 2);
    QCOMPARE(schedule->regularRows().at(0)->day(), QStringLiteral("Friday"));
    QCOMPARE(schedule->regularRows().at(0)->startTime(),
             QStringLiteral("3:00 PM"));
    QCOMPARE(schedule->regularRows().at(1)->day(), QStringLiteral("Monday"));
    QCOMPARE(schedule->regularRows().at(1)->startTime(),
             QStringLiteral("9:00 AM"));
    QCOMPARE(schedule->intensiveRows().size(), 1);
    QCOMPARE(schedule->intensiveRows().at(0)->day(), QStringLiteral("Tuesday"));
    QVERIFY(header->subtitle().contains(QStringLiteral("E5 Artemis")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Romanized Teacher")));
}

void ClassDetailsPageReadParityTests::
displayReadUsesNoTeacherFallbackWithoutLosingClassOrCount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Missing Info Parity")
        );
    QVERIFY(classId > 0);

    auto infoResult = services.classService()->classInfo(classId);
    QVERIFY(infoResult);
    ClassInfo info = *infoResult;
    info.classGrade = QStringLiteral("E5");
    info.classLevel = QStringLiteral("Artemis");
    info.readingBook = QStringLiteral("Reading Explorer 2");
    info.essayBook = QStringLiteral("5A");
    info.classColor = QStringLiteral("#112233");
    info.fontColor = QStringLiteral("#445566");
    info.classTimes = {
        {QStringLiteral("Friday"), QStringLiteral("3:00 PM"),
         QStringLiteral("3:55 PM")}
    };
    QVERIFY(services.classService()->saveClassInfo(info));
    QVERIFY(services.rosterService()->saveRoster(
        classId,
        rosterWithStudents({QStringLiteral("Student One")})
        ));

    QSqlQuery damageTeacherRead(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY2(
        damageTeacherRead.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
        qPrintable(damageTeacherRead.lastError().text())
        );
    damageTeacherRead.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"
        ));
    damageTeacherRead.addBindValue(987654);
    damageTeacherRead.addBindValue(classId);
    QVERIFY2(
        damageTeacherRead.exec(),
        qPrintable(damageTeacherRead.lastError().text())
        );

    ClassDetailsPage page(&services);
    page.loadClass(Classroom(QStringLiteral("Fallback Name"), classId));

    auto* details = page.findChild<ClassDetailsSection*>();
    auto* schedule = page.findChild<ClassScheduleSection*>();
    auto* header = page.findChild<PageHeader*>();
    auto* countEdit = studentCountEdit(page);
    QVERIFY(details && schedule && header && countEdit);

    QCOMPARE(details->grade(), QStringLiteral("E5"));
    QCOMPARE(details->classColor(), QStringLiteral("#112233"));
    QCOMPARE(details->fontColor(), QStringLiteral("#445566"));
    QCOMPARE(countEdit->text(), QStringLiteral("1"));
    QCOMPARE(schedule->regularRows().size(), 1);
    QCOMPARE(schedule->regularRows().first()->day(), QStringLiteral("Friday"));
    QVERIFY(schedule->intensiveRows().isEmpty());
    QVERIFY(header->subtitle().contains(QStringLiteral("E5 Artemis")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));
}

QTEST_MAIN(ClassDetailsPageReadParityTests)
#include "class_details_page_read_parity_tests.moc"
