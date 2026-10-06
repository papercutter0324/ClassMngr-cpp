#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_import_repository.h"
#include "next/platform/application_services_teacher_import_apply_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{
QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("teacher-import-apply-%1.tps")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

TeacherImportPlan planFor()
{
    TeacherImportPlan plan;
    plan.sourceDate = QDate(2026, 10, 1);
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김 교사");
    teacher.teacherEn = QStringLiteral(" Alex Kim ");
    teacher.phoneNumber = QStringLiteral("01012345678");
    plan.koreanTeachers.append(teacher);
    plan.nativeEnglishTeachers.append(NativeEnglishTeacher{
        .name = QStringLiteral("Alex Smith")
    });
    plan.gsTeamMembers.append(GsTeamMember{
        .name = QStringLiteral("Front Desk")
    });
    return plan;
}

int countRows(QSqlDatabase database, const QString& table)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

QString latestSourceDate(QSqlDatabase database)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key=?"));
    query.addBindValue(QString::fromLatin1(
        TeacherImportRepository::LatestSourceDateSetting));
    return query.exec() && query.next() ? query.value(0).toString() : QString();
}
}

using namespace ClassMngr::Next;

class NextPlatformApplicationServicesTeacherImportApplyPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void appliesAllGroupsAndKeepsNewestDate();
    void preservesTeacherServiceValidationText();
    void rollsBackEarlierGroupWritesWhenLateWriteFails();
    void rejectsUnavailableSession();
};

void NextPlatformApplicationServicesTeacherImportApplyPortTests::
appliesAllGroupsAndKeepsNewestDate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesTeacherImportApplyPort port(&services);

    const auto imported = port.apply(planFor());
    QVERIFY2(imported, qPrintable(imported ? QString() : imported.error()));
    QCOMPARE(imported->koreanTeachers.created, 1);
    QCOMPARE(imported->nativeEnglishTeachers.created, 1);
    QCOMPARE(imported->gsTeamMembers.created, 1);
    QCOMPARE(countRows(services.databaseSession()->database(), QStringLiteral("teachers")), 1);
    QCOMPARE(countRows(services.databaseSession()->database(), QStringLiteral("native_english_teachers")), 1);
    QCOMPARE(countRows(services.databaseSession()->database(), QStringLiteral("gs_team")), 1);
    QSqlQuery storedTeacher(services.databaseSession()->database());
    QVERIFY(storedTeacher.exec(QStringLiteral(
        "SELECT teacher_kr, teacher_en, phone_number FROM teachers")));
    QVERIFY(storedTeacher.next());
    QCOMPARE(storedTeacher.value(0).toString(), QStringLiteral("김교사"));
    QCOMPARE(storedTeacher.value(1).toString(), QStringLiteral("Alex Kim"));
    QCOMPARE(storedTeacher.value(2).toString(), QStringLiteral("010-1234-5678"));
    QCOMPARE(latestSourceDate(services.databaseSession()->database()),
        QStringLiteral("2026-10-01"));

    TeacherImportPlan olderPlan;
    olderPlan.sourceDate = QDate(2025, 9, 1);
    const auto older = port.apply(olderPlan);
    QVERIFY2(older, qPrintable(older ? QString() : older.error()));
    QCOMPARE(latestSourceDate(services.databaseSession()->database()),
        QStringLiteral("2026-10-01"));
}

void NextPlatformApplicationServicesTeacherImportApplyPortTests::
preservesTeacherServiceValidationText()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesTeacherImportApplyPort port(&services);

    TeacherImportPlan invalidPlan;
    invalidPlan.sourceDate = QDate(2026, 10, 1);
    invalidPlan.koreanTeachers.append(Teacher{});
    const auto applied = port.apply(invalidPlan);
    QVERIFY(!applied);
    const auto legacy = services.teacherService()->importTeachers(invalidPlan);
    QVERIFY(!legacy);
    QCOMPARE(applied.error(), legacy.error());
    QCOMPARE(applied.error(), QStringLiteral(
        "Teacher import validation failed: koreanTeachers[0].teacherEn: teacher.name.required"));
}

void NextPlatformApplicationServicesTeacherImportApplyPortTests::
rollsBackEarlierGroupWritesWhenLateWriteFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery trigger(services.databaseSession()->database());
    QVERIFY2(trigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_gs_import BEFORE INSERT ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'forced test failure'); END")),
        qPrintable(trigger.lastError().text()));
    Platform::ApplicationServicesTeacherImportApplyPort port(&services);

    const auto imported = port.apply(planFor());

    QVERIFY(!imported);
    QCOMPARE(countRows(services.databaseSession()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(countRows(services.databaseSession()->database(), QStringLiteral("native_english_teachers")), 0);
    QCOMPARE(countRows(services.databaseSession()->database(), QStringLiteral("gs_team")), 0);
    QVERIFY(latestSourceDate(services.databaseSession()->database()).isEmpty());
}

void NextPlatformApplicationServicesTeacherImportApplyPortTests::
rejectsUnavailableSession()
{
    Platform::ApplicationServicesTeacherImportApplyPort nullServicesPort(nullptr);
    auto missing = nullServicesPort.apply(planFor());
    QVERIFY(!missing);
    QCOMPARE(missing.error(), QStringLiteral("No Teacher Profile service is available."));

    ApplicationServices services;
    Platform::ApplicationServicesTeacherImportApplyPort closedPort(&services);
    missing = closedPort.apply(planFor());
    QVERIFY(!missing);
    QCOMPARE(missing.error(), QStringLiteral("No Teacher Profile service is available."));
}

QTEST_MAIN(NextPlatformApplicationServicesTeacherImportApplyPortTests)

#include "next_platform_application_services_teacher_import_apply_port_tests.moc"
