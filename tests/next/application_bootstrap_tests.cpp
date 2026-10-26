#include "next/application/shutdown_coordinator.h"
#include "next/application/startup_coordinator.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/ui/application_shell.h"
#include "next/ui/initial_page.h"

#include <QApplication>
#include <QLabel>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

class ApplicationBootstrapTests final : public QObject
{
    Q_OBJECT

private slots:
    void createsVisibleShellAndReportsOnlyCompletedStartupStage();
    void closeRequestsShutdownOnceAndRestoresApplicationPolicy();
    void startupFailureAndShutdownActionAreReportedPrecisely();
};

void ApplicationBootstrapTests::
createsVisibleShellAndReportsOnlyCompletedStartupStage()
{
    QApplication& application = *qApp;
    Bootstrap::ApplicationBootstrap bootstrap(application);

    const Application::StartupResult result = bootstrap.start();

    QCOMPARE(result.status, Application::StartupStatus::InitialExperienceAvailable);
    QVERIFY(result.succeeded());
    QVERIFY(!result.failure.has_value());
    QCOMPARE(result.completedEvents.size(), std::size_t{1});
    QCOMPARE(
        result.completedEvents.front().stage,
        Application::StartupStage::InitialExperienceAvailable
        );

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
    QVERIFY(inProgress.completedEvents.empty());
    QVERIFY(!inProgress.failure.has_value());

    const Application::StartupResult failed = startupCoordinator.fail(
        Application::StartupFailureReason::InitialExperienceUnavailable
        );
    QCOMPARE(failed.status, Application::StartupStatus::Failed);
    QVERIFY(!failed.succeeded());
    QVERIFY(failed.completedEvents.empty());
    QVERIFY(failed.failure.has_value());
    QCOMPARE(
        *failed.failure,
        Application::StartupFailureReason::InitialExperienceUnavailable
        );

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
