#include "core/application_services.h"
#include "data/database/database_session.h"
#include "app/services/feature_services.h"
#include "next/application/schedule_import_apply_use_case.h"
#include "next/platform/application_services_schedule_import_apply_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include <utility>

using namespace ClassMngr::Next;

namespace
{
Application::ScheduleImportApplyRequest typedRequest()
{
    Application::ScheduleImportApplyRequest request;
    request.selectedUserName = u"Alice";
    request.updateProfileName = true;
    request.diagnosticsAcknowledged = true;
    request.diagnostics.push_back({
        u"Sheet", u"Alice", u"B12", u"?", u"Ignored cell"
    });

    Application::ScheduleImportApplyCandidate candidate;
    candidate.teacherKey = u"\uAE40";
    candidate.teacherName = u"\uAE40";
    candidate.rooms = {u"413"};
    candidate.importedColors = {u"#123456"};
    candidate.grade = u"E4";
    candidate.level = u"Hercules";
    candidate.times = {
        {u"Monday", u"4:00 PM", u"4:50 PM"},
        {u"Wednesday", u"4:00 PM", u"4:50 PM"}
    };
    candidate.sourceCells = {u"B12"};
    request.candidates.push_back(std::move(candidate));
    request.teachers.push_back({
        u"\uAE40",
        Application::ScheduleImportReviewTeacherAction::Create,
        std::nullopt,
        u"413"
    });
    request.classes.push_back({
        0,
        Application::ScheduleImportReviewClassAction::CreateNew,
        std::nullopt,
        u"#123456",
        u"#FFFFFF"
    });
    return request;
}

void requireQuery(QSqlQuery& query, const QString& statement)
{
    if (!query.exec(statement))
    {
        qFatal("SQL query failed: %s", qPrintable(query.lastError().text()));
    }
}
}

class ScheduleImportApplyPortTests : public QObject
{
    Q_OBJECT

private slots:
    void appliesTypedRequestThroughActiveRepository();
    void reportsUnavailableAndClosedSession();
    void preservesLegacyScheduleServiceRoute();
    void mapsSummaryAndFailure();
};

void ScheduleImportApplyPortTests::
appliesTypedRequestThroughActiveRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const Status opened = services.openDatabase(
        directory.filePath(QStringLiteral("typed-apply.sqlite"))
        );
    const QString openError = opened.has_value() ? QString() : opened.error();
    QVERIFY2(opened.has_value(), qPrintable(openError));

    Platform::ApplicationServicesScheduleImportApplyPort port(&services);
    const auto result = Application::ScheduleImportApplyUseCase::execute(
        typedRequest(),
        port
        );
    const QString failureMessage = result.has_value()
        ? QString()
        : QString::fromStdU16String(result.error().message);
    QVERIFY2(result.has_value(), qPrintable(failureMessage));
    QCOMPARE(result->teachersCreated, 1);
    QCOMPARE(result->teachersUpdated, 0);
    QCOMPARE(result->classesCreated, 1);
    QCOMPARE(result->classesUpdated, 0);
    QCOMPARE(result->classesSkipped, 0);
    QCOMPARE(result->schedulesCleared, 0);
    QCOMPARE(result->ignoredCells, 1);
    QVERIFY(result->profileNameUpdated);

    QSqlQuery query(services.databaseSession()->database());
    requireQuery(query, QStringLiteral(
        "SELECT teacher_kr, room_number FROM teachers"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString::fromUtf16(u"\uAE40"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("413"));
    QVERIFY(!query.next());

    requireQuery(query, QStringLiteral(
        "SELECT c.name, ci.class_grade, ci.class_level, ci.class_color, "
        "ci.font_color, t.day, t.start_time, t.end_time "
        "FROM classes c "
        "JOIN class_info ci ON ci.class_id=c.id "
        "JOIN class_times t ON t.class_id=c.id"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("E4 Hercules"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("E4"));
    QCOMPARE(query.value(2).toString(), QStringLiteral("Hercules"));
    QCOMPARE(query.value(3).toString(), QStringLiteral("#123456"));
    QCOMPARE(query.value(4).toString(), QStringLiteral("#FFFFFF"));
    QCOMPARE(query.value(5).toString(), QStringLiteral("Monday"));
    QCOMPARE(query.value(6).toString(), QStringLiteral("4:00 PM"));
    QCOMPARE(query.value(7).toString(), QStringLiteral("4:50 PM"));
    QVERIFY(query.next());
    QCOMPARE(query.value(5).toString(), QStringLiteral("Wednesday"));
    QCOMPARE(query.value(6).toString(), QStringLiteral("4:00 PM"));
    QCOMPARE(query.value(7).toString(), QStringLiteral("4:50 PM"));
    QVERIFY(!query.next());

    requireQuery(query, QStringLiteral(
        "SELECT value FROM app_settings WHERE key='myInfo/name'"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Alice"));
    QVERIFY(!query.next());
}

void ScheduleImportApplyPortTests::reportsUnavailableAndClosedSession()
{
    Platform::ApplicationServicesScheduleImportApplyPort missingPort(nullptr);
    const auto missing = missingPort.applyScheduleImport(typedRequest());
    QVERIFY(!missing);
    QCOMPARE(missing.error().message, std::u16string(u"No Teacher Profile is open."));
    QVERIFY(!missing.error().policyIssue.has_value());

    ApplicationServices services;
    QVERIFY(!services.hasOpenDatabase());
    Platform::ApplicationServicesScheduleImportApplyPort closedPort(&services);
    const auto closed = closedPort.applyScheduleImport(typedRequest());
    QVERIFY(!closed);
    QCOMPARE(closed.error().message, std::u16string(u"No Teacher Profile is open."));
    QVERIFY(!closed.error().policyIssue.has_value());
}

void ScheduleImportApplyPortTests::preservesLegacyScheduleServiceRoute()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const Status opened = services.openDatabase(
        directory.filePath(QStringLiteral("legacy-apply.sqlite"))
        );
    const QString openError = opened.has_value() ? QString() : opened.error();
    QVERIFY2(opened.has_value(), qPrintable(openError));

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QString::fromUtf16(u"\uAE40");
    candidate.teacherKr = QString::fromUtf16(u"\uAE40");
    candidate.rooms = {QStringLiteral("415")};
    candidate.classGrade = QStringLiteral("E5");
    candidate.classLevel = QStringLiteral("Apollo");
    candidate.times = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:50 PM")
        },
        {
            QStringLiteral("Wednesday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:50 PM")
        }
    };

    ScheduleImportPlan plan;
    plan.unknownCellsAcknowledged = true;
    plan.candidates = {candidate};
    plan.teachers = {{
        candidate.teacherKey,
        ScheduleImportTeacherAction::Create,
        -1,
        QStringLiteral("415")
    }};
    plan.classes = {{
        0,
        ScheduleImportClassAction::CreateNew,
        -1,
        QStringLiteral("#223344"),
        QStringLiteral("#FFFFFF")
    }};

    const auto result = services.scheduleService()->importSchedule(plan);
    const QString failureMessage = result.has_value()
        ? QString()
        : result.error();
    QVERIFY2(result.has_value(), qPrintable(failureMessage));
    QCOMPARE(result->teachersCreated, 1);
    QCOMPARE(result->classesCreated, 1);

    QSqlQuery query(services.databaseSession()->database());
    requireQuery(query, QStringLiteral(
        "SELECT teacher_kr FROM teachers"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString::fromUtf16(u"\uAE40"));
    QVERIFY(!query.next());
}

void ScheduleImportApplyPortTests::mapsSummaryAndFailure()
{
    ScheduleImportSummary summary;
    summary.teachersCreated = 1;
    summary.teachersUpdated = 2;
    summary.classesCreated = 3;
    summary.classesUpdated = 4;
    summary.classesSkipped = 5;
    summary.schedulesCleared = 6;
    summary.ignoredCells = 7;
    summary.profileNameUpdated = true;
    const auto mapped = Platform::scheduleImportApplyResult(summary);
    QVERIFY(mapped);
    QCOMPARE(mapped->teachersCreated, 1);
    QCOMPARE(mapped->teachersUpdated, 2);
    QCOMPARE(mapped->classesCreated, 3);
    QCOMPARE(mapped->classesUpdated, 4);
    QCOMPARE(mapped->classesSkipped, 5);
    QCOMPARE(mapped->schedulesCleared, 6);
    QCOMPARE(mapped->ignoredCells, 7);
    QVERIFY(mapped->profileNameUpdated);

    const auto failed = Platform::scheduleImportApplyResult(
        Result<ScheduleImportSummary>(std::unexpected(QStringLiteral("database failed"))));
    QVERIFY(!failed);
    QCOMPARE(failed.error().message, std::u16string(u"database failed"));
    QVERIFY(!failed.error().policyIssue.has_value());
}

QTEST_GUILESS_MAIN(ScheduleImportApplyPortTests)
#include "next_platform_application_services_schedule_import_apply_port_tests.moc"
