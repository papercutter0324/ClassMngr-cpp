#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "next/platform/application_services_teacher_import_apply_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <type_traits>
#include <utility>

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

static_assert(std::is_same_v<
    decltype(std::declval<Platform::ApplicationServicesTeacherImportApplyPort>()
        .apply(std::declval<const TeacherImportPlan&>())),
    Result<Application::TeacherImportUseCaseResult>>);

class NextPlatformApplicationServicesTeacherImportApplyPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void appliesAllGroupsAndKeepsNewestDate();
    void treatsMalformedSnapshotDateAsAbsentAndDoesNotRewriteEqualOrOlderDates();
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
    QCOMPARE(imported->koreanTeachers.created.rawValue(), 1);
    QCOMPARE(imported->koreanTeachers.updated.rawValue(), 0);
    QCOMPARE(imported->koreanTeachers.unchanged.rawValue(), 0);
    QCOMPARE(imported->nativeEnglishTeachers.created.rawValue(), 1);
    QCOMPARE(imported->nativeEnglishTeachers.updated.rawValue(), 0);
    QCOMPARE(imported->nativeEnglishTeachers.unchanged.rawValue(), 0);
    QCOMPARE(imported->gsTeamMembers.created.rawValue(), 1);
    QCOMPARE(imported->gsTeamMembers.updated.rawValue(), 0);
    QCOMPARE(imported->gsTeamMembers.unchanged.rawValue(), 0);
    QCOMPARE(countRows(services.workspaceStore()->database(), QStringLiteral("teachers")), 1);
    QCOMPARE(countRows(services.workspaceStore()->database(), QStringLiteral("native_english_teachers")), 1);
    QCOMPARE(countRows(services.workspaceStore()->database(), QStringLiteral("gs_team")), 1);
    QSqlQuery storedTeacher(services.workspaceStore()->database());
    QVERIFY(storedTeacher.exec(QStringLiteral(
        "SELECT teacher_kr, teacher_en, phone_number FROM teachers")));
    QVERIFY(storedTeacher.next());
    QCOMPARE(storedTeacher.value(0).toString(), QStringLiteral("김교사"));
    QCOMPARE(storedTeacher.value(1).toString(), QStringLiteral("Alex Kim"));
    QCOMPARE(storedTeacher.value(2).toString(), QStringLiteral("010-1234-5678"));
    QCOMPARE(latestSourceDate(services.workspaceStore()->database()),
        QStringLiteral("2026-10-01"));

    const auto unchanged = port.apply(planFor());
    QVERIFY2(unchanged, qPrintable(unchanged ? QString() : unchanged.error()));
    QCOMPARE(unchanged->koreanTeachers.created.rawValue(), 0);
    QCOMPARE(unchanged->koreanTeachers.updated.rawValue(), 0);
    QCOMPARE(unchanged->koreanTeachers.unchanged.rawValue(), 1);
    QCOMPARE(unchanged->nativeEnglishTeachers.created.rawValue(), 0);
    QCOMPARE(unchanged->nativeEnglishTeachers.updated.rawValue(), 0);
    QCOMPARE(unchanged->nativeEnglishTeachers.unchanged.rawValue(), 1);
    QCOMPARE(unchanged->gsTeamMembers.created.rawValue(), 0);
    QCOMPARE(unchanged->gsTeamMembers.updated.rawValue(), 0);
    QCOMPARE(unchanged->gsTeamMembers.unchanged.rawValue(), 1);

    TeacherImportPlan normalizedNameUpdate;
    normalizedNameUpdate.sourceDate = QDate(2026, 10, 2);
    normalizedNameUpdate.nativeEnglishTeachers.append(NativeEnglishTeacher{
        .name = QStringLiteral("  aLeX   SmItH\t")
    });
    const auto updated = port.apply(normalizedNameUpdate);
    QVERIFY2(updated, qPrintable(updated ? QString() : updated.error()));
    QCOMPARE(updated->koreanTeachers.created.rawValue(), 0);
    QCOMPARE(updated->koreanTeachers.updated.rawValue(), 0);
    QCOMPARE(updated->koreanTeachers.unchanged.rawValue(), 0);
    QCOMPARE(updated->nativeEnglishTeachers.created.rawValue(), 0);
    QCOMPARE(updated->nativeEnglishTeachers.updated.rawValue(), 1);
    QCOMPARE(updated->nativeEnglishTeachers.unchanged.rawValue(), 0);
    QCOMPARE(updated->gsTeamMembers.created.rawValue(), 0);
    QCOMPARE(updated->gsTeamMembers.updated.rawValue(), 0);
    QCOMPARE(updated->gsTeamMembers.unchanged.rawValue(), 0);
    QCOMPARE(countRows(services.workspaceStore()->database(),
                 QStringLiteral("native_english_teachers")), 1);
    QSqlQuery normalizedName(services.workspaceStore()->database());
    QVERIFY(normalizedName.exec(QStringLiteral(
        "SELECT name FROM native_english_teachers")));
    QVERIFY(normalizedName.next());
    QCOMPARE(normalizedName.value(0).toString(), QStringLiteral("aLeX SmItH"));
    QCOMPARE(latestSourceDate(services.workspaceStore()->database()),
        QStringLiteral("2026-10-02"));

    TeacherImportPlan olderPlan;
    olderPlan.sourceDate = QDate(2025, 9, 1);
    const auto older = port.apply(olderPlan);
    QVERIFY2(older, qPrintable(older ? QString() : older.error()));
    QCOMPARE(latestSourceDate(services.workspaceStore()->database()),
        QStringLiteral("2026-10-02"));
}

void NextPlatformApplicationServicesTeacherImportApplyPortTests::
treatsMalformedSnapshotDateAsAbsentAndDoesNotRewriteEqualOrOlderDates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(services.workspaceStore()->settingsRepository()->saveSetting(
        QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting),
        QStringLiteral("not-a-date")));
    Platform::ApplicationServicesTeacherImportApplyPort port(&services);

    TeacherImportPlan initial;
    initial.sourceDate = QDate(2026, 10, 1);
    const auto imported = port.apply(initial);
    QVERIFY2(imported, qPrintable(imported ? QString() : imported.error()));
    QCOMPARE(latestSourceDate(services.workspaceStore()->database()),
        QStringLiteral("2026-10-01"));

    QSqlQuery rejectDateWrite(services.workspaceStore()->database());
    QVERIFY2(rejectDateWrite.exec(QStringLiteral(
        "CREATE TRIGGER reject_teacher_import_date_update "
        "BEFORE UPDATE ON app_settings "
        "WHEN OLD.key='teacher_import/latest_source_date' "
        "BEGIN SELECT RAISE(ABORT, 'unexpected date rewrite'); END")),
        qPrintable(rejectDateWrite.lastError().text()));

    TeacherImportPlan equalDate;
    equalDate.sourceDate = QDate(2026, 10, 1);
    const auto equal = port.apply(equalDate);
    QVERIFY2(equal, qPrintable(equal ? QString() : equal.error()));

    TeacherImportPlan olderDate;
    olderDate.sourceDate = QDate(2026, 9, 30);
    const auto older = port.apply(olderDate);
    QVERIFY2(older, qPrintable(older ? QString() : older.error()));
    QCOMPARE(latestSourceDate(services.workspaceStore()->database()),
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
    QSqlQuery trigger(services.workspaceStore()->database());
    QVERIFY2(trigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_gs_import BEFORE INSERT ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'forced test failure'); END")),
        qPrintable(trigger.lastError().text()));
    Platform::ApplicationServicesTeacherImportApplyPort port(&services);

    const auto imported = port.apply(planFor());

    QVERIFY(!imported);
    QCOMPARE(countRows(services.workspaceStore()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(countRows(services.workspaceStore()->database(), QStringLiteral("native_english_teachers")), 0);
    QCOMPARE(countRows(services.workspaceStore()->database(), QStringLiteral("gs_team")), 0);
    QVERIFY(latestSourceDate(services.workspaceStore()->database()).isEmpty());
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
