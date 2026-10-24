#include "core/application_services.h"
#include "data/database/workspace_store.h"
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
    candidate.teacherKey = Application::ScheduleImportApplyTeacherKey{
        u"\uAE40"
    };
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
        Application::ScheduleImportApplyTeacherKey{u"\uAE40"},
        Application::ScheduleImportReviewTeacherAction::Create,
        std::nullopt,
        u"413"
    });
    request.classes.push_back({
        Application::ScheduleImportCandidateIndex{0},
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

void seedSentinelClass(QSqlQuery& query)
{
    requireQuery(query, QStringLiteral(R"(
        INSERT INTO classes (id, name) VALUES (1, 'sentinel class')
    )"));
    requireQuery(query, QStringLiteral(R"(
        INSERT INTO class_info (
            class_id, teacher_id, class_grade, class_level,
            class_color, font_color, notes
        ) VALUES (1, NULL, 'sentinel grade', 'sentinel level',
                  '#ABCDEF', '#123456', 'sentinel notes')
    )"));
    requireQuery(query, QStringLiteral(R"(
        INSERT INTO class_times (class_id, day, start_time, end_time)
        VALUES (1, 'Tuesday', '7:00 PM', '8:00 PM')
    )"));
}

void setClassesSequence(QSqlQuery& query, const qint64 sequence)
{
    query.prepare(QStringLiteral(
        "UPDATE sqlite_sequence SET seq = ? WHERE name = 'classes'"));
    query.addBindValue(sequence);
    if (!query.exec())
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
    void rejectsGeneratedClassIdOutsideIntRangeAndRollsBack();
    void acceptsGeneratedClassIdAtIntMax();
    void reportsUnavailableAndClosedSession();
    void preservesLegacyScheduleServiceRoute();
    void mapsSummaryAndFailure();
    void preservesStructuredRepositoryFailures();
};

void ScheduleImportApplyPortTests::
appliesTypedRequestThroughActiveRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const RepositoryStatus opened= services.openDatabase(
        directory.filePath(QStringLiteral("typed-apply.sqlite"))
        );
    const QString openError = opened.has_value()
        ? QString()
        : opened.error().diagnostic;
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
    QCOMPARE(result->teachersCreated.rawValue(), 1);
    QCOMPARE(result->teachersUpdated.rawValue(), 0);
    QCOMPARE(result->classesCreated.rawValue(), 1);
    QCOMPARE(result->classesUpdated.rawValue(), 0);
    QCOMPARE(result->classesSkipped.rawValue(), 0);
    QCOMPARE(result->schedulesCleared.rawValue(), 0);
    QCOMPARE(result->ignoredCells.rawValue(), 1);
    QVERIFY(result->profileNameUpdated);

    QSqlQuery query(services.workspaceStore()->database());
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

void ScheduleImportApplyPortTests::
rejectsGeneratedClassIdOutsideIntRangeAndRollsBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const RepositoryStatus opened= services.openDatabase(
        directory.filePath(QStringLiteral("narrowed-class-id.sqlite"))
        );
    const QString openError = opened.has_value()
        ? QString()
        : opened.error().diagnostic;
    QVERIFY2(opened.has_value(), qPrintable(openError));

    QSqlQuery query(services.workspaceStore()->database());
    seedSentinelClass(query);
    setClassesSequence(query, qint64{4294967296LL});

    Platform::ApplicationServicesScheduleImportApplyPort port(&services);
    const auto result = Application::ScheduleImportApplyUseCase::execute(
        typedRequest(),
        port
        );
    QVERIFY(!result.has_value());
    QVERIFY(!result.error().message.empty());

    requireQuery(query, QStringLiteral(R"(
        SELECT
            (SELECT COUNT(*) FROM teachers),
            (SELECT COUNT(*) FROM classes),
            (SELECT COUNT(*) FROM classes WHERE id = 4294967297),
            (SELECT COUNT(*) FROM class_info),
            (SELECT COUNT(*) FROM class_info WHERE class_id = 4294967297),
            (SELECT COUNT(*) FROM class_times),
            (SELECT COUNT(*) FROM class_times WHERE class_id = 4294967297)
    )"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
    QCOMPARE(query.value(1).toInt(), 1);
    QCOMPARE(query.value(2).toInt(), 0);
    QCOMPARE(query.value(3).toInt(), 1);
    QCOMPARE(query.value(4).toInt(), 0);
    QCOMPARE(query.value(5).toInt(), 1);
    QCOMPARE(query.value(6).toInt(), 0);
    QVERIFY(!query.next());

    requireQuery(query, QStringLiteral(R"(
        SELECT c.name, ci.teacher_id, ci.class_grade, ci.class_level,
               ci.class_color, ci.font_color, ci.notes
        FROM classes c
        JOIN class_info ci ON ci.class_id = c.id
        WHERE c.id = 1
    )"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("sentinel class"));
    QVERIFY(query.value(1).isNull());
    QCOMPARE(query.value(2).toString(), QStringLiteral("sentinel grade"));
    QCOMPARE(query.value(3).toString(), QStringLiteral("sentinel level"));
    QCOMPARE(query.value(4).toString(), QStringLiteral("#ABCDEF"));
    QCOMPARE(query.value(5).toString(), QStringLiteral("#123456"));
    QCOMPARE(query.value(6).toString(), QStringLiteral("sentinel notes"));
    QVERIFY(!query.next());

    requireQuery(query, QStringLiteral(R"(
        SELECT day, start_time, end_time
        FROM class_times WHERE class_id = 1 ORDER BY id
    )"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Tuesday"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("7:00 PM"));
    QCOMPARE(query.value(2).toString(), QStringLiteral("8:00 PM"));
    QVERIFY(!query.next());
}

void ScheduleImportApplyPortTests::acceptsGeneratedClassIdAtIntMax()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const RepositoryStatus opened= services.openDatabase(
        directory.filePath(QStringLiteral("int-max-class-id.sqlite"))
        );
    const QString openError = opened.has_value()
        ? QString()
        : opened.error().diagnostic;
    QVERIFY2(opened.has_value(), qPrintable(openError));

    QSqlQuery query(services.workspaceStore()->database());
    seedSentinelClass(query);
    setClassesSequence(query, qint64{2147483646});

    Platform::ApplicationServicesScheduleImportApplyPort port(&services);
    const auto result = Application::ScheduleImportApplyUseCase::execute(
        typedRequest(),
        port
        );
    const QString failureMessage = result.has_value()
        ? QString()
        : QString::fromStdU16String(result.error().message);
    QVERIFY2(result.has_value(), qPrintable(failureMessage));
    QCOMPARE(result->teachersCreated.rawValue(), 1);
    QCOMPARE(result->classesCreated.rawValue(), 1);

    requireQuery(query, QStringLiteral(R"(
        SELECT typeof(c.id), c.id, c.name, ci.class_grade, ci.class_level,
               ci.class_color, ci.font_color
        FROM classes c
        JOIN class_info ci ON ci.class_id = c.id
        WHERE c.id = 2147483647
    )"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(query.value(1).toLongLong(), qint64{2147483647});
    QCOMPARE(query.value(2).toString(), QStringLiteral("E4 Hercules"));
    QCOMPARE(query.value(3).toString(), QStringLiteral("E4"));
    QCOMPARE(query.value(4).toString(), QStringLiteral("Hercules"));
    QCOMPARE(query.value(5).toString(), QStringLiteral("#123456"));
    QCOMPARE(query.value(6).toString(), QStringLiteral("#FFFFFF"));
    QVERIFY(!query.next());

    requireQuery(query, QStringLiteral(R"(
        SELECT day, start_time, end_time
        FROM class_times WHERE class_id = 2147483647 ORDER BY id
    )"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Monday"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("4:00 PM"));
    QCOMPARE(query.value(2).toString(), QStringLiteral("4:50 PM"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Wednesday"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("4:00 PM"));
    QCOMPARE(query.value(2).toString(), QStringLiteral("4:50 PM"));
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
    const RepositoryStatus opened= services.openDatabase(
        directory.filePath(QStringLiteral("legacy-apply.sqlite"))
        );
    const QString openError = opened.has_value()
        ? QString()
        : opened.error().diagnostic;
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
        std::nullopt,
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

    QSqlQuery query(services.workspaceStore()->database());
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
    QCOMPARE(mapped->teachersCreated.rawValue(), 1);
    QCOMPARE(mapped->teachersUpdated.rawValue(), 2);
    QCOMPARE(mapped->classesCreated.rawValue(), 3);
    QCOMPARE(mapped->classesUpdated.rawValue(), 4);
    QCOMPARE(mapped->classesSkipped.rawValue(), 5);
    QCOMPARE(mapped->schedulesCleared.rawValue(), 6);
    QCOMPARE(mapped->ignoredCells.rawValue(), 7);
    QVERIFY(mapped->profileNameUpdated);

    const auto mappedZeroCounts =
        Platform::scheduleImportApplyResult(ScheduleImportSummary{});
    QVERIFY(mappedZeroCounts);
    QCOMPARE(mappedZeroCounts->teachersCreated.rawValue(), 0);
    QCOMPARE(mappedZeroCounts->teachersUpdated.rawValue(), 0);
    QCOMPARE(mappedZeroCounts->classesCreated.rawValue(), 0);
    QCOMPARE(mappedZeroCounts->classesUpdated.rawValue(), 0);
    QCOMPARE(mappedZeroCounts->classesSkipped.rawValue(), 0);
    QCOMPARE(mappedZeroCounts->schedulesCleared.rawValue(), 0);
    QCOMPARE(mappedZeroCounts->ignoredCells.rawValue(), 0);
    QVERIFY(!mappedZeroCounts->profileNameUpdated);

    const auto failed = Platform::scheduleImportApplyResult(
        Result<ScheduleImportSummary>(std::unexpected(QStringLiteral("database failed"))));
    QVERIFY(!failed);
    QCOMPARE(failed.error().message, std::u16string(u"database failed"));
    QVERIFY(!failed.error().policyIssue.has_value());
}

void ScheduleImportApplyPortTests::preservesStructuredRepositoryFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const RepositoryStatus opened= services.openDatabase(
        directory.filePath(QStringLiteral("structured-apply.sqlite"))
        );
    const QString openError = opened.has_value()
        ? QString()
        : opened.error().diagnostic;
    QVERIFY2(opened.has_value(), qPrintable(openError));

    Platform::ApplicationServicesScheduleImportApplyPort port(&services);

    auto policyRequest = typedRequest();
    policyRequest.diagnosticsAcknowledged = false;
    const auto policyFailure = port.applyScheduleImport(policyRequest);
    QVERIFY(!policyFailure);
    QVERIFY(policyFailure.error().policyIssue.has_value());
    QCOMPARE(
        policyFailure.error().policyIssue->code,
        Application::ScheduleImportPlanEligibilityIssueCode::
            UnacknowledgedDiagnostics
        );
    QVERIFY(!policyFailure.error().teacherTargetIssue.has_value());
    QVERIFY(!policyFailure.error().stateValidationError.has_value());

    auto teacherRequest = typedRequest();
    teacherRequest.teachers[0].action =
        Application::ScheduleImportReviewTeacherAction::Reuse;
    const auto teacherFailure = port.applyScheduleImport(teacherRequest);
    QVERIFY(!teacherFailure);
    QVERIFY(teacherFailure.error().teacherTargetIssue.has_value());
    QCOMPARE(
        teacherFailure.error().teacherTargetIssue->code,
        Application::ScheduleImportApplyTeacherTargetIssueCode::
            ExistingTeacherMissingTarget
        );
    QCOMPARE(
        teacherFailure.error().teacherTargetIssue->teacherKey.value(),
        std::u16string(u"\uAE40")
        );
    QVERIFY(!teacherFailure.error().policyIssue.has_value());
    QVERIFY(!teacherFailure.error().stateValidationError.has_value());

    auto overlappingRequest = typedRequest();
    auto secondCandidate = overlappingRequest.candidates.front();
    secondCandidate.teacherKey = Application::ScheduleImportApplyTeacherKey{
        u"\uBC15"
    };
    secondCandidate.teacherName = u"\uBC15";
    secondCandidate.grade = u"E5";
    secondCandidate.level = u"Apollo";
    overlappingRequest.candidates.push_back(secondCandidate);
    overlappingRequest.teachers.push_back({
        Application::ScheduleImportApplyTeacherKey{u"\uBC15"},
        Application::ScheduleImportReviewTeacherAction::Create,
        std::nullopt,
        u"413"
    });
    overlappingRequest.classes.push_back({
        Application::ScheduleImportCandidateIndex{1},
        Application::ScheduleImportReviewClassAction::CreateNew,
        std::nullopt,
        u"#123456",
        u"#FFFFFF"
    });

    const auto overlapFailure = port.applyScheduleImport(overlappingRequest);
    QVERIFY(!overlapFailure);
    QVERIFY2(
        overlapFailure.error().stateValidationError.has_value(),
        qPrintable(QString::fromStdU16String(overlapFailure.error().message))
        );
    QCOMPARE(
        overlapFailure.error().stateValidationError->code,
        Application::ScheduleImportStateValidationErrorCode::
            ProjectedScheduleOverlap
        );
    QCOMPARE(
        overlapFailure.error().stateValidationError->classLabel,
        std::string("E4 Hercules")
        );
    QCOMPARE(
        overlapFailure.error().stateValidationError->conflictingClassLabel,
        std::string("E5 Apollo")
        );
    QCOMPARE(overlapFailure.error().stateValidationError->day, std::string("Monday"));
    QCOMPARE(
        overlapFailure.error().stateValidationError->startTime,
        std::string("4:00 PM")
        );
    QCOMPARE(
        overlapFailure.error().stateValidationError->endTime,
        std::string("4:50 PM")
        );
    QVERIFY(!overlapFailure.error().policyIssue.has_value());
    QVERIFY(!overlapFailure.error().teacherTargetIssue.has_value());

    QSqlQuery countQuery(services.workspaceStore()->database());
    requireQuery(countQuery, QStringLiteral(
        "SELECT (SELECT COUNT(*) FROM teachers), "
        "(SELECT COUNT(*) FROM classes), "
        "(SELECT COUNT(*) FROM class_times)"));
    QVERIFY(countQuery.next());
    QCOMPARE(countQuery.value(0).toInt(), 0);
    QCOMPARE(countQuery.value(1).toInt(), 0);
    QCOMPARE(countQuery.value(2).toInt(), 0);
}

QTEST_GUILESS_MAIN(ScheduleImportApplyPortTests)
#include "next_platform_application_services_schedule_import_apply_port_tests.moc"
