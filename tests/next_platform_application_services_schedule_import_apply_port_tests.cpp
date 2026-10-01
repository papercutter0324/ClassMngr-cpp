#include "next/platform/application_services_schedule_import_apply_port.h"

#include <QtTest>

using namespace ClassMngr::Next;

class ScheduleImportApplyPortTests : public QObject
{
    Q_OBJECT

private slots:
    void mapsTypedRequestToLegacyPlan();
    void reportsUnavailableService();
    void mapsSummaryAndFailure();
};

void ScheduleImportApplyPortTests::mapsTypedRequestToLegacyPlan()
{
    Application::ScheduleImportApplyRequest typed;
    typed.intensiveSchedule = true;
    typed.intensiveMode = Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew;
    typed.selectedUserName = u"\uAE40\uC9C0\uC6D0";
    typed.saveProfileNameIfBlank = true;
    typed.updateProfileName = true;
    typed.diagnosticsAcknowledged = true;

    Application::ScheduleImportApplyCandidate candidate;
    candidate.teacherKey = u"\uAE40\uC9C0\uC6D0";
    candidate.teacherName = u"\uAE40\uC9C0\uC6D0";
    candidate.rooms = {u"413"};
    candidate.importedColors = {u"#123456"};
    candidate.grade = u"E4";
    candidate.level = u"Hercules";
    candidate.times.push_back({u"Monday", u"4:00 PM", u"4:50 PM"});
    candidate.sourceCells = {u"B12"};
    candidate.meetingPatternError = u"pattern";
    typed.candidates.push_back(candidate);
    typed.intensiveSlotStates.push_back({u"Monday", u"4:00 PM", u"Occupied"});
    typed.diagnostics.push_back({u"Sheet", u"\uAE40\uC9C0\uC6D0", u"B12", u"?", u"Ignored"});
    typed.teachers.push_back({
        u"\uAE40\uC9C0\uC6D0",
        Application::ScheduleImportReviewTeacherAction::UpdateRoom,
        Domain::TeacherId::fromString("17"),
        u"413"
    });
    typed.classes.push_back({
        0,
        Application::ScheduleImportReviewClassAction::UpdateExisting,
        Domain::ClassId::fromString("42"),
        u"#FFFFFF",
        u"#000000"
    });

    const auto restored = Platform::legacyScheduleImportPlan(typed);
    QCOMPARE(restored.kind, ScheduleImportKind::Intensive);
    QCOMPARE(restored.intensiveMode, ScheduleImportIntensiveMode::ReplaceWithNew);
    QCOMPARE(restored.selectedUserName, QString::fromStdU16String(typed.selectedUserName));
    QVERIFY(restored.saveProfileNameIfBlank && restored.updateProfileName
        && restored.unknownCellsAcknowledged);
    QCOMPARE(restored.candidates.at(0).teacherKr,
        QString::fromStdU16String(candidate.teacherName));
    QCOMPARE(restored.candidates.at(0).rooms.at(0), QStringLiteral("413"));
    QCOMPARE(restored.candidates.at(0).importedColors.at(0), QStringLiteral("#123456"));
    QCOMPARE(restored.candidates.at(0).times.at(0).startTime,
        QStringLiteral("4:00 PM"));
    QCOMPARE(restored.candidates.at(0).sourceCells.at(0), QStringLiteral("B12"));
    QCOMPARE(restored.candidates.at(0).meetingPatternError, QStringLiteral("pattern"));
    QCOMPARE(restored.intensiveSlotStates.at(0).state, QStringLiteral("Occupied"));
    QCOMPARE(restored.diagnostics.at(0).userName,
        QString::fromStdU16String(u"\uAE40\uC9C0\uC6D0"));
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
