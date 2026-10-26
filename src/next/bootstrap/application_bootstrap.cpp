#include "next/bootstrap/application_bootstrap.h"

#include "next/bootstrap/qt_startup_diagnostics_sink.h"
#include "next/ui/application_shell.h"

#include <QApplication>

#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <utility>

namespace ClassMngr::Next::Bootstrap
{

int ApplicationBootstrap::run(int argc, char* argv[])
{
    // Keep launch probes and support scripts independent of Qt platform plugin
    // initialization and application storage setup.
    if (argc == 2 && argv != nullptr && argv[1] != nullptr
        && std::strcmp(argv[1], "--version") == 0)
    {
        std::printf("ClassMngrNext launch version=%s\n", CLASSMNGR_NEXT_VERSION);
        return 0;
    }

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ClassMngrNext"));
    QApplication::setApplicationVersion(QStringLiteral(CLASSMNGR_NEXT_VERSION));

    ApplicationBootstrap bootstrap(application);
    const Application::StartupResult startup = bootstrap.start();
    if (!startup.succeeded())
    {
        std::fprintf(stderr, "ClassMngrNext could not start its initial experience.\n");
        return 1;
    }

    std::printf("ClassMngrNext launch version=%s\n", CLASSMNGR_NEXT_VERSION);
    const int eventLoopExitCode = application.exec();
    static_cast<void>(bootstrap.shutdown());
    return eventLoopExitCode;
}

ApplicationBootstrap::ApplicationBootstrap(
    QApplication& application,
    StartupEventObserver startupEventObserver,
    StartupDiagnosticsSinkFactory startupDiagnosticsSinkFactory
    )
    : m_application(application),
      m_startupEventObserver(std::move(startupEventObserver)),
      m_startupDiagnosticsSinkFactory(std::move(startupDiagnosticsSinkFactory)),
      m_shutdownCoordinator([&application] {
          application.quit();
      })
{
}

ApplicationBootstrap::~ApplicationBootstrap()
{
    if (m_quitOnLastWindowClosedChanged)
    {
        m_application.setQuitOnLastWindowClosed(
            m_previousQuitOnLastWindowClosed
            );
    }
}

Application::StartupResult ApplicationBootstrap::start()
{
    const Application::StartupResult beginResult = m_startupCoordinator.begin();
    if (beginResult.status != Application::StartupStatus::InProgress)
    {
        return beginResult;
    }

    const Application::StartupResult processCreated =
        m_startupCoordinator.recordStage(Application::StartupStage::ProcessCreated);
    observeStartupResult(processCreated);
    if (processCreated.status == Application::StartupStatus::Failed)
    {
        return processCreated;
    }

    try
    {
        m_startupDiagnosticsSink = m_startupDiagnosticsSinkFactory
            ? m_startupDiagnosticsSinkFactory()
            : makeQtStartupDiagnosticsSink();
        if (!m_startupDiagnosticsSink)
        {
            throw std::runtime_error("Startup diagnostics sink unavailable.");
        }
    }
    catch (...)
    {
        if (processCreated.latestStageResult.has_value())
        {
            writeEarlyQtStartupDiagnostic(*processCreated.latestStageResult);
        }
        const Application::StartupResult failure = m_startupCoordinator.recordStage(
            Application::StartupStage::CoreServicesCreated,
            Application::StartupStageOutcome::Failed,
            Application::StartupFailureReason::StageFailed
            );
        if (failure.latestStageResult.has_value())
        {
            writeEarlyQtStartupDiagnostic(*failure.latestStageResult);
        }
        observeStartupResult(failure);
        return failure;
    }

    // ProcessCreated predates the diagnostics owner. Retain event order by
    // flushing it through the newly-created sink before recording its owner.
    if (processCreated.latestStageResult.has_value())
    {
        m_startupDiagnosticsSink->record(*processCreated.latestStageResult);
    }
    const Application::StartupResult coreServicesCreated =
        m_startupCoordinator.recordStage(
            Application::StartupStage::CoreServicesCreated
            );
    observeStartupResult(coreServicesCreated);
    if (coreServicesCreated.status == Application::StartupStatus::Failed)
    {
        return coreServicesCreated;
    }

    m_previousQuitOnLastWindowClosed = m_application.quitOnLastWindowClosed();
    m_application.setQuitOnLastWindowClosed(false);
    m_quitOnLastWindowClosedChanged = true;

    try
    {
        m_shell = std::make_unique<Ui::ApplicationShell>();
        m_shell->setCloseHandler([this] {
            static_cast<void>(shutdown());
        });
    }
    catch (const std::exception&)
    {
        const Application::StartupResult failure = m_startupCoordinator.recordStage(
            Application::StartupStage::ShellCreated,
            Application::StartupStageOutcome::Failed,
            Application::StartupFailureReason::InitialExperienceUnavailable
            );
        observeStartupResult(failure);
        return failure;
    }

    const Application::StartupResult shellCreated = m_startupCoordinator.recordStage(
        Application::StartupStage::ShellCreated
        );
    observeStartupResult(shellCreated);
    if (shellCreated.status == Application::StartupStatus::Failed)
    {
        return shellCreated;
    }

    m_shell->show();

    const Application::StartupResult initialPageReady =
        m_startupCoordinator.recordStage(Application::StartupStage::InitialPageReady);
    observeStartupResult(initialPageReady);
    return initialPageReady;
}

Application::ShutdownResult ApplicationBootstrap::shutdown()
{
    return m_shutdownCoordinator.requestShutdown();
}

bool ApplicationBootstrap::shutdownRequested() const noexcept
{
    return m_shutdownCoordinator.shutdownRequested();
}

Ui::ApplicationShell* ApplicationBootstrap::shell() noexcept
{
    return m_shell.get();
}

const Ui::ApplicationShell* ApplicationBootstrap::shell() const noexcept
{
    return m_shell.get();
}

void ApplicationBootstrap::observeStartupResult(
    const Application::StartupResult& result
    ) const
{
    if (m_startupEventObserver && result.latestStageResult.has_value())
    {
        m_startupEventObserver(*result.latestStageResult);
    }
    if (m_startupDiagnosticsSink && result.latestStageResult.has_value())
    {
        m_startupDiagnosticsSink->record(*result.latestStageResult);
    }
}

}  // namespace ClassMngr::Next::Bootstrap
