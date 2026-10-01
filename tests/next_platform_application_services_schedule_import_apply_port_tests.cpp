#include "next/platform/application_services_schedule_import_apply_port.h"

#include <QtTest>

using namespace ClassMngr::Next;

class ScheduleImportApplyPortTests : public QObject
{
    Q_OBJECT

private slots:
    void mapsAllPlanFields();
    void reportsUnavailableService();
    void mapsSummaryAndFailure();
};

void ScheduleImportApplyPortTests::mapsAllPlanFields()
{
    ScheduleImportPlan plan;
    plan.kind = ScheduleImportKind::Intensive;
    plan.intensiveMode = ScheduleImportIntensiveMode::ReplaceWithNew;
    plan.selectedUserName = QStringLiteral("김지원");
    plan.saveProfileNameIfBlank = true;
    plan.updateProfileName = true;
    plan.unknownCellsAcknowledged = true;
    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("김지원");
    candidate.teacherKr = QStringLiteral("김지원");
    candidate.rooms = {QStringLiteral(" 413 ")};
    candidate.importedColors = {QStringLiteral("#123456")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times.append({QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")});
    candidate.sourceCells = {QStringLiteral("B12")};
    candidate.meetingPatternError = QStringLiteral("pattern");
    plan.candidates.append(candidate);
    plan.intensiveSlotStates.append({QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"), QStringLiteral("Occupied")});
    plan.diagnostics.append({QStringLiteral("Sheet"), QStringLiteral("김지원"),
        QStringLiteral("B12"), QStringLiteral("?"), QStringLiteral("Ignored")});
    plan.teachers.append({QStringLiteral("김지원"), ScheduleImportTeacherAction::UpdateRoom,
        17, QStringLiteral(" 413 ")});
    plan.classes.append({0, ScheduleImportClassAction::UpdateExisting, 42,
        QStringLiteral(" #FFFFFF "), QStringLiteral(" #000000 ")});

    const auto typed = Platform::scheduleImportApplyRequest(plan);
    QVERIFY(typed.intensiveSchedule);
    QCOMPARE(typed.intensiveMode, Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew);
    QCOMPARE(typed.teachers.at(0).targetTeacherId->value(), std::string("17"));
    QCOMPARE(typed.classes.at(0).targetClassId->value(), std::string("42"));
    QCOMPARE(typed.teachers.at(0).selectedRoom, std::u16string(u"413"));
    QCOMPARE(typed.classes.at(0).classColor, std::u16string(u"#FFFFFF"));

    const auto restored = Platform::legacyScheduleImportPlan(typed);
    QCOMPARE(restored.kind, plan.kind);
    QCOMPARE(restored.intensiveMode, plan.intensiveMode);
    QCOMPARE(restored.selectedUserName, plan.selectedUserName);
    QVERIFY(restored.saveProfileNameIfBlank && restored.updateProfileName
        && restored.unknownCellsAcknowledged);
    QCOMPARE(restored.candidates.at(0).teacherKr, candidate.teacherKr);
    QCOMPARE(restored.candidates.at(0).rooms.at(0), QStringLiteral("413"));
    QCOMPARE(restored.candidates.at(0).importedColors, candidate.importedColors);
    QCOMPARE(restored.candidates.at(0).times.at(0).startTime,
        candidate.times.at(0).startTime);
    QCOMPARE(restored.candidates.at(0).sourceCells, candidate.sourceCells);
    QCOMPARE(restored.candidates.at(0).meetingPatternError, candidate.meetingPatternError);
    QCOMPARE(restored.intensiveSlotStates.at(0).state, QStringLiteral("Occupied"));
    QCOMPARE(restored.diagnostics.at(0).userName, QStringLiteral("김지원"));
    QCOMPARE(restored.teachers.at(0).action, ScheduleImportTeacherAction::UpdateRoom);
    QCOMPARE(restored.teachers.at(0).targetTeacherId, 17);
    QCOMPARE(restored.classes.at(0).action, ScheduleImportClassAction::UpdateExisting);
    QCOMPARE(restored.classes.at(0).targetClassId, 42);
    QCOMPARE(restored.classes.at(0).classColor, QStringLiteral("#FFFFFF"));
}

void ScheduleImportApplyPortTests::reportsUnavailableService()
{
    Platform::ApplicationServicesScheduleImportApplyPort port(nullptr);
    const auto result = port.applyScheduleImport({});
    QVERIFY(!result);
    QCOMPARE(result.error().message, std::u16string(u"No Teacher Profile is open."));
    QVERIFY(!result.error().policyIssue.has_value());
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
