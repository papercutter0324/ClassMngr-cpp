#include "next/application/shutdown_coordinator.h"
#include "next/application/startup_coordinator.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/ui/application_shell.h"
#include "next/ui/initial_page.h"

#include <QApplication>
#include <QLabel>
#include <QtTest/QtTest>

#include <array>
#include <vector>

using namespace ClassMngr::Next;

class ApplicationBootstrapTests final : public QObject
{
    Q_OBJECT

private slots:
    void createsVisibleShellAndReportsOnlyCompletedStartupStage();
    void startupCoordinatorAcceptsEveryPhaseFiveStageInOrder();
    void startupCoordinatorRejectsInvalidTransitionsWithoutRecordingThem();
    void startupCoordinatorRetainsTypedWarningAndTerminalFailure();
    void closeRequestsShutdownOnceAndRestoresApplicationPolicy();
    void startupFailureAndShutdownActionAreReportedPrecisely();
};

void ApplicationBootstrapTests::
createsVisibleShellAndReportsOnlyCompletedStartupStage()
{
    QApplication& application = *qApp;
    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        }
        );

    const Application::StartupResult result = bootstrap.start();

    QCOMPARE(result.status, Application::StartupStatus::InitialExperienceAvailable);
    QVERIFY(result.succeeded());
    QVERIFY(!result.failure.has_value());
    QCOMPARE(result.stageResults.size(), std::size_t{3});
    QCOMPARE(result.completedEvents.size(), std::size_t{3});
    QVERIFY(!result.diagnostic.has_value());

    constexpr std::array expectedStages{
        Application::StartupStage::ProcessCreated,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady
    };
    QCOMPARE(observedEvents.size(), expectedStages.size());
    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        QCOMPARE(result.stageResults[index].stage, expectedStages[index]);
        QCOMPARE(result.stageResults[index].outcome, Application::StartupStageOutcome::Completed);
        QVERIFY(!result.stageResults[index].diagnostic.has_value());
        QCOMPARE(result.completedEvents[index].stage, expectedStages[index]);
        QCOMPARE(observedEvents[index].stage, expectedStages[index]);
        QCOMPARE(observedEvents[index].outcome, Application::StartupStageOutcome::Completed);
        QVERIFY(!observedEvents[index].diagnostic.has_value());
    }
    QVERIFY(!result.stageResults.empty());
    QVERIFY(result.stageResults.back().stage != Application::StartupStage::StartupReady);

    const Application::StartupResult repeatedStart = bootstrap.start();
    QCOMPARE(repeatedStart.status, Application::StartupStatus::Failed);
    QVERIFY(repeatedStart.failure.has_value());
    QCOMPARE(*repeatedStart.failure, Application::StartupFailureReason::AlreadyStarted);
    QCOMPARE(observedEvents.size(), expectedStages.size());

    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell != nullptr);
    QVERIFY(shell->isVisible());
    QVERIFY(!shell->isFullScreen());
    QVERIFY(!(shell->windowState() & Qt::WindowFullScreen));
    QCOMPARE(shell->windowTitle(), QStringLiteral("ClassMngr Next"));

    auto* const page = dynamic_cast<Ui::InitialPage*>(shell->centralWidget());
    QVERIFY(page != nullptr);
    QVERIFY(page->isVisible());
    QCOMPARE(page->objectName(), QStringLiteral("classMngrNextInitialPage"));

    QLabel* const title = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialPageTitle")
        );
    QVERIFY(title != nullptr);
    QCOMPARE(title->text(), QStringLiteral("ClassMngr Next"));

    QLabel* const description = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialPageDescription")
        );
    QVERIFY(description != nullptr);
    QCOMPARE(
        description->text(),
        QStringLiteral("The next-generation workspace is under development.")
        );
}

void ApplicationBootstrapTests::
startupCoordinatorAcceptsEveryPhaseFiveStageInOrder()
{
    Application::StartupCoordinator coordinator;
    const Application::StartupResult begun = coordinator.begin();
    QCOMPARE(begun.status, Application::StartupStatus::InProgress);
    QVERIFY(begun.stageResults.empty());
    QVERIFY(begun.completedEvents.empty());

    constexpr std::array expectedStages{
        Application::StartupStage::ProcessCreated,
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };

    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        const Application::StartupResult accepted = coordinator.recordStage(
            expectedStages[index]
            );
        QVERIFY(accepted.latestStageResult.has_value());
        QCOMPARE(accepted.latestStageResult->stage, expectedStages[index]);
        QCOMPARE(
            accepted.latestStageResult->outcome,
            Application::StartupStageOutcome::Completed
            );
        QCOMPARE(accepted.stageResults.size(), index + 1);
        QCOMPARE(accepted.completedEvents.size(), index + 1);
        QCOMPARE(accepted.stageResults.back().stage, expectedStages[index]);
        QCOMPARE(accepted.completedEvents.back().stage, expectedStages[index]);
        QVERIFY(!accepted.failure.has_value());
        QVERIFY(!accepted.diagnostic.has_value());

        if (expectedStages[index] == Application::StartupStage::InitialPageReady)
        {
            QCOMPARE(
                accepted.status,
                Application::StartupStatus::InitialExperienceAvailable
                );
            QVERIFY(accepted.succeeded());
        }
        else if (expectedStages[index] == Application::StartupStage::StartupReady)
        {
            QCOMPARE(accepted.status, Application::StartupStatus::StartupReady);
            QVERIFY(accepted.succeeded());
        }
        else
        {
            QCOMPARE(accepted.status, Application::StartupStatus::InProgress);
        }
    }

    const Application::StartupResult finalResult = coordinator.result();
    QCOMPARE(finalResult.stageResults.size(), expectedStages.size());
    QCOMPARE(finalResult.completedEvents.size(), expectedStages.size());
    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        QCOMPARE(finalResult.stageResults[index].stage, expectedStages[index]);
        QCOMPARE(
            finalResult.stageResults[index].outcome,
            Application::StartupStageOutcome::Completed
            );
        QCOMPARE(finalResult.completedEvents[index].stage, expectedStages[index]);
    }
    QVERIFY(!finalResult.latestStageResult.has_value());
}

void ApplicationBootstrapTests::
startupCoordinatorRejectsInvalidTransitionsWithoutRecordingThem()
{
    Application::StartupCoordinator coordinator;

    const Application::StartupResult beforeBegin = coordinator.recordStage(
        Application::StartupStage::ProcessCreated
        );
    QCOMPARE(beforeBegin.status, Application::StartupStatus::Failed);
    QVERIFY(beforeBegin.failure.has_value());
    QCOMPARE(*beforeBegin.failure, Application::StartupFailureReason::InvalidTransition);
    QVERIFY(beforeBegin.stageResults.empty());
    QVERIFY(!beforeBegin.latestStageResult.has_value());
    QCOMPARE(coordinator.result().status, Application::StartupStatus::NotStarted);

    QVERIFY(coordinator.begin().status == Application::StartupStatus::InProgress);

    const Application::StartupResult wrongFirstStage = coordinator.recordStage(
        Application::StartupStage::CoreServicesCreated
        );
    QCOMPARE(wrongFirstStage.status, Application::StartupStatus::Failed);
    QVERIFY(wrongFirstStage.failure.has_value());
    QCOMPARE(*wrongFirstStage.failure, Application::StartupFailureReason::InvalidTransition);
    QVERIFY(wrongFirstStage.diagnostic.has_value());
    QCOMPARE(wrongFirstStage.diagnostic->code, Application::StartupDiagnosticCode::InvalidTransition);
    QVERIFY(wrongFirstStage.diagnostic->stage.has_value());
    QCOMPARE(*wrongFirstStage.diagnostic->stage, Application::StartupStage::CoreServicesCreated);
    QVERIFY(wrongFirstStage.stageResults.empty());
    QVERIFY(!wrongFirstStage.latestStageResult.has_value());
    QCOMPARE(coordinator.result().status, Application::StartupStatus::InProgress);

    const Application::StartupResult invalidStage = coordinator.recordStage(
        static_cast<Application::StartupStage>(255)
        );
    QVERIFY(invalidStage.failure.has_value());
    QCOMPARE(*invalidStage.failure, Application::StartupFailureReason::InvalidTransition);
    QVERIFY(invalidStage.stageResults.empty());
    QVERIFY(!invalidStage.latestStageResult.has_value());

    const Application::StartupResult processCreated = coordinator.recordStage(
        Application::StartupStage::ProcessCreated
        );
    QVERIFY(processCreated.latestStageResult.has_value());

    const Application::StartupResult duplicate = coordinator.recordStage(
        Application::StartupStage::ProcessCreated
        );
    QVERIFY(duplicate.failure.has_value());
    QCOMPARE(*duplicate.failure, Application::StartupFailureReason::DuplicateStage);
    QVERIFY(duplicate.diagnostic.has_value());
    QCOMPARE(duplicate.diagnostic->code, Application::StartupDiagnosticCode::DuplicateStage);
    QCOMPARE(duplicate.stageResults.size(), std::size_t{1});
    QVERIFY(!duplicate.latestStageResult.has_value());

    // Skipping a stage is allowed for compositions that do not own it, but
    // subsequent accepted outcomes must still move strictly forward.
    const Application::StartupResult settingsLoaded = coordinator.recordStage(
        Application::StartupStage::SettingsLoaded
        );
    QVERIFY(settingsLoaded.latestStageResult.has_value());
    QCOMPARE(settingsLoaded.stageResults.size(), std::size_t{2});

    const Application::StartupResult backward = coordinator.recordStage(
        Application::StartupStage::CoreServicesCreated
        );
    QVERIFY(backward.failure.has_value());
    QCOMPARE(*backward.failure, Application::StartupFailureReason::BackwardStage);
    QVERIFY(backward.diagnostic.has_value());
    QCOMPARE(backward.diagnostic->code, Application::StartupDiagnosticCode::BackwardStage);
    QVERIFY(backward.diagnostic->stage.has_value());
    QCOMPARE(*backward.diagnostic->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(backward.stageResults.size(), std::size_t{2});
    QVERIFY(!backward.latestStageResult.has_value());
    QCOMPARE(coordinator.result().stageResults.size(), std::size_t{2});

    Application::StartupCoordinator pageCoordinator;
    QVERIFY(pageCoordinator.begin().status == Application::StartupStatus::InProgress);
    QVERIFY(pageCoordinator.recordStage(Application::StartupStage::ProcessCreated)
                .latestStageResult.has_value());
    const Application::StartupResult pageWithoutShell = pageCoordinator.recordStage(
        Application::StartupStage::InitialPageReady
        );
    QVERIFY(pageWithoutShell.failure.has_value());
    QCOMPARE(*pageWithoutShell.failure, Application::StartupFailureReason::InvalidTransition);
    QCOMPARE(pageWithoutShell.stageResults.size(), std::size_t{1});
    QVERIFY(!pageWithoutShell.latestStageResult.has_value());

    QVERIFY(pageCoordinator.recordStage(Application::StartupStage::ShellCreated)
                .latestStageResult.has_value());
    const Application::StartupResult pageReady = pageCoordinator.recordStage(
        Application::StartupStage::InitialPageReady
        );
    QVERIFY(pageReady.latestStageResult.has_value());
    QCOMPARE(pageReady.status, Application::StartupStatus::InitialExperienceAvailable);

    Application::StartupCoordinator readinessCoordinator;
    QVERIFY(readinessCoordinator.begin().status == Application::StartupStatus::InProgress);
    QVERIFY(readinessCoordinator.recordStage(Application::StartupStage::ProcessCreated)
                .latestStageResult.has_value());
    const Application::StartupResult prematureReady = readinessCoordinator.recordStage(
        Application::StartupStage::StartupReady
        );
    QVERIFY(prematureReady.failure.has_value());
    QCOMPARE(
        *prematureReady.failure,
        Application::StartupFailureReason::StartupReadyPrerequisitesIncomplete
        );
    QVERIFY(prematureReady.diagnostic.has_value());
    QCOMPARE(
        prematureReady.diagnostic->code,
        Application::StartupDiagnosticCode::StartupReadyPrerequisitesIncomplete
        );
    QCOMPARE(prematureReady.stageResults.size(), std::size_t{1});
    QVERIFY(!prematureReady.latestStageResult.has_value());
    QCOMPARE(readinessCoordinator.result().status, Application::StartupStatus::InProgress);

    constexpr std::array remainingStages{
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };
    Application::StartupResult recovered;
    for (const Application::StartupStage stage : remainingStages)
    {
        recovered = readinessCoordinator.recordStage(stage);
        QVERIFY(recovered.latestStageResult.has_value());
    }
    QCOMPARE(recovered.status, Application::StartupStatus::StartupReady);
    QCOMPARE(recovered.stageResults.size(), std::size_t{10});
}

void ApplicationBootstrapTests::
startupCoordinatorRetainsTypedWarningAndTerminalFailure()
{
    Application::StartupCoordinator warningCoordinator;
    QVERIFY(warningCoordinator.begin().status == Application::StartupStatus::InProgress);
    QVERIFY(warningCoordinator.recordStage(Application::StartupStage::ProcessCreated)
                .latestStageResult.has_value());

    const Application::StartupResult warning = warningCoordinator.recordStage(
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStageOutcome::Warning
        );
    QCOMPARE(warning.status, Application::StartupStatus::InProgress);
    QVERIFY(warning.latestStageResult.has_value());
    QCOMPARE(warning.latestStageResult->outcome, Application::StartupStageOutcome::Warning);
    QVERIFY(warning.latestStageResult->diagnostic.has_value());
    QCOMPARE(warning.latestStageResult->diagnostic->code, Application::StartupDiagnosticCode::StageWarning);
    QVERIFY(warning.latestStageResult->diagnostic->stage.has_value());
    QCOMPARE(*warning.latestStageResult->diagnostic->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(warning.stageResults.size(), std::size_t{2});
    QCOMPARE(warning.completedEvents.size(), std::size_t{1});
    QCOMPARE(warning.stageResults.back().outcome, Application::StartupStageOutcome::Warning);
    QVERIFY(warning.diagnostic.has_value());
    QCOMPARE(warning.diagnostic->code, Application::StartupDiagnosticCode::StageWarning);
    QVERIFY(!warning.failure.has_value());

    constexpr std::array remainingStages{
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };
    Application::StartupResult afterWarning;
    for (const Application::StartupStage stage : remainingStages)
    {
        afterWarning = warningCoordinator.recordStage(stage);
        QVERIFY(afterWarning.latestStageResult.has_value());
    }
    QCOMPARE(afterWarning.status, Application::StartupStatus::StartupReady);
    QVERIFY(afterWarning.succeeded());
    QCOMPARE(afterWarning.stageResults.size(), std::size_t{10});
    QCOMPARE(afterWarning.completedEvents.size(), std::size_t{9});
    QCOMPARE(afterWarning.stageResults[1].outcome, Application::StartupStageOutcome::Warning);
    QCOMPARE(afterWarning.completedEvents[1].stage, Application::StartupStage::SettingsLoaded);

    Application::StartupCoordinator failureCoordinator;
    QVERIFY(failureCoordinator.begin().status == Application::StartupStatus::InProgress);
    QVERIFY(failureCoordinator.recordStage(Application::StartupStage::ProcessCreated)
                .latestStageResult.has_value());
    const Application::StartupResult stageFailure = failureCoordinator.recordStage(
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStageOutcome::Failed,
        Application::StartupFailureReason::StageFailed
        );
    QCOMPARE(stageFailure.status, Application::StartupStatus::Failed);
    QVERIFY(!stageFailure.succeeded());
    QVERIFY(stageFailure.failure.has_value());
    QCOMPARE(*stageFailure.failure, Application::StartupFailureReason::StageFailed);
    QCOMPARE(stageFailure.stageResults.size(), std::size_t{2});
    QCOMPARE(stageFailure.completedEvents.size(), std::size_t{1});
    QVERIFY(stageFailure.latestStageResult.has_value());
    QCOMPARE(stageFailure.latestStageResult->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(stageFailure.latestStageResult->outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(stageFailure.latestStageResult->diagnostic.has_value());
    QCOMPARE(stageFailure.latestStageResult->diagnostic->code, Application::StartupDiagnosticCode::StageFailed);
    QVERIFY(stageFailure.latestStageResult->diagnostic->stage.has_value());
    QCOMPARE(*stageFailure.latestStageResult->diagnostic->stage, Application::StartupStage::CoreServicesCreated);

    const Application::StartupResult afterTerminalFailure = failureCoordinator.recordStage(
        Application::StartupStage::SettingsLoaded
        );
    QCOMPARE(afterTerminalFailure.status, Application::StartupStatus::Failed);
    QVERIFY(afterTerminalFailure.failure.has_value());
    QCOMPARE(*afterTerminalFailure.failure, Application::StartupFailureReason::InvalidTransition);
    QCOMPARE(afterTerminalFailure.stageResults.size(), std::size_t{2});
    QVERIFY(!afterTerminalFailure.latestStageResult.has_value());
    const Application::StartupResult terminalState = failureCoordinator.result();
    QCOMPARE(terminalState.status, Application::StartupStatus::Failed);
    QVERIFY(terminalState.failure.has_value());
    QCOMPARE(*terminalState.failure, Application::StartupFailureReason::StageFailed);
    QCOMPARE(terminalState.stageResults.size(), std::size_t{2});
}

void ApplicationBootstrapTests::
closeRequestsShutdownOnceAndRestoresApplicationPolicy()
{
    QApplication& application = *qApp;
    const bool previousQuitPolicy = application.quitOnLastWindowClosed();
    {
        Bootstrap::ApplicationBootstrap bootstrap(application);
        const Application::StartupResult startup = bootstrap.start();
        QVERIFY(startup.succeeded());
        QVERIFY(!bootstrap.shutdownRequested());
        QVERIFY(!application.quitOnLastWindowClosed());

        QVERIFY(bootstrap.shell()->close());

        QTRY_VERIFY(bootstrap.shutdownRequested());
        QVERIFY(!bootstrap.shell()->isVisible());

        const Application::ShutdownResult repeated = bootstrap.shutdown();
        QVERIFY(repeated.shutdownRequested);
        QVERIFY(!repeated.newlyRequested);

        const Application::ShutdownResult repeatedAgain = bootstrap.shutdown();
        QVERIFY(repeatedAgain.shutdownRequested);
        QVERIFY(!repeatedAgain.newlyRequested);
    }

    QCOMPARE(application.quitOnLastWindowClosed(), previousQuitPolicy);
}

void ApplicationBootstrapTests::
startupFailureAndShutdownActionAreReportedPrecisely()
{
    Application::StartupCoordinator startupCoordinator;
    const Application::StartupResult inProgress = startupCoordinator.begin();
    QCOMPARE(inProgress.status, Application::StartupStatus::InProgress);
    QVERIFY(inProgress.stageResults.empty());
    QVERIFY(inProgress.completedEvents.empty());
    QVERIFY(!inProgress.failure.has_value());

    const Application::StartupResult failed = startupCoordinator.fail(
        Application::StartupFailureReason::InitialExperienceUnavailable
        );
    QCOMPARE(failed.status, Application::StartupStatus::Failed);
    QVERIFY(!failed.succeeded());
    QVERIFY(failed.stageResults.empty());
    QVERIFY(failed.completedEvents.empty());
    QVERIFY(failed.failure.has_value());
    QCOMPARE(
        *failed.failure,
        Application::StartupFailureReason::InitialExperienceUnavailable
        );
    QVERIFY(failed.diagnostic.has_value());
    QCOMPARE(
        failed.diagnostic->code,
        Application::StartupDiagnosticCode::InitialExperienceUnavailable
        );

    const Application::StartupResult afterFailure = startupCoordinator.recordStage(
        Application::StartupStage::ProcessCreated
        );
    QCOMPARE(afterFailure.status, Application::StartupStatus::Failed);
    QVERIFY(afterFailure.failure.has_value());
    QCOMPARE(*afterFailure.failure, Application::StartupFailureReason::InvalidTransition);
    QVERIFY(afterFailure.stageResults.empty());
    QVERIFY(!afterFailure.latestStageResult.has_value());
    const Application::StartupResult failureState = startupCoordinator.result();
    QVERIFY(failureState.failure.has_value());
    QCOMPARE(*failureState.failure, Application::StartupFailureReason::InitialExperienceUnavailable);

    int shutdownActionCount = 0;
    Application::ShutdownCoordinator shutdownCoordinator([&shutdownActionCount] {
        ++shutdownActionCount;
    });
    const Application::ShutdownResult first =
        shutdownCoordinator.requestShutdown();
    QVERIFY(first.shutdownRequested);
    QVERIFY(first.newlyRequested);
    QCOMPARE(shutdownActionCount, 1);

    const Application::ShutdownResult second =
        shutdownCoordinator.requestShutdown();
    QVERIFY(second.shutdownRequested);
    QVERIFY(!second.newlyRequested);
    QCOMPARE(shutdownActionCount, 1);
}

QTEST_MAIN(ApplicationBootstrapTests)

#include "application_bootstrap_tests.moc"
