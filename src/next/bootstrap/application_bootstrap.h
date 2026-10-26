#pragma once

#include "next/application/recent_workspace_history.h"
#include "next/application/shutdown_coordinator.h"
#include "next/application/startup_coordinator.h"
#include "next/application/startup_workspace_policy.h"
#include "next/application/application_runtime.h"
#include "next/application/workspace_contracts.h"
#include "next/application/workspace_state.h"
#include "next/bootstrap/startup_diagnostics_sink.h"
#include "next/bootstrap/startup_update_dispatcher.h"
#include "next/resources/resource_root_resolver.h"

#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

class QApplication;

namespace ClassMngr::Next::Ui
{
class ApplicationShell;
}

namespace ClassMngr::Next::Bootstrap
{

class ApplicationBootstrap final
{
public:
    using StartupEventObserver =
        std::function<void(const Application::StartupEvent&)>;
    using StartupDiagnosticsSinkFactory =
        std::function<std::unique_ptr<StartupDiagnosticsSink>()>;
    using StartupUpdateDispatcherFactory =
        std::function<std::unique_ptr<StartupUpdateDispatcher>()>;
    using ResourcePackagePathsResolver =
        std::function<Resources::ResourceResult<Resources::ResourcePackagePaths>()>;

    // Narrow inputs for deterministic startup-policy and gateway composition
    // checks. Production defaults use QApplication arguments, saved recent
    // history, filesystem existence, and the store-backed gateway.
    struct StartupWorkspaceSeams final
    {
        std::optional<QStringList> commandLineArguments;
        std::optional<bool> recentAutoOpenEnabled;
        std::function<bool(const Application::WorkspaceLocation&)> pathExists;
        std::function<std::unique_ptr<Application::WorkspaceGateway>()>
            workspaceGatewayFactory;
        std::function<std::unique_ptr<Application::RecentWorkspaceHistoryPort>()>
            recentWorkspaceHistoryPortFactory;
    };

    // Sole production process entry. It handles the side-effect-free version
    // probe before creating QApplication, then owns startup and event-loop
    // lifetime through explicit shutdown.
    [[nodiscard]] static int run(int argc, char* argv[]);

    // Injected QApplication seam for focused startup composition tests.
    explicit ApplicationBootstrap(
        QApplication& application,
        StartupEventObserver startupEventObserver = {},
        StartupDiagnosticsSinkFactory startupDiagnosticsSinkFactory = {},
        ResourcePackagePathsResolver resourcePackagePathsResolver = {},
        std::optional<QStringList> systemUiLanguages = std::nullopt,
        StartupWorkspaceSeams startupWorkspaceSeams = {},
        StartupUpdateDispatcherFactory startupUpdateDispatcherFactory = {}
        );
    ~ApplicationBootstrap();

    ApplicationBootstrap(const ApplicationBootstrap&) = delete;
    ApplicationBootstrap& operator=(const ApplicationBootstrap&) = delete;

    [[nodiscard]] Application::StartupResult start();
    [[nodiscard]] Application::ShutdownResult shutdown();
    [[nodiscard]] bool shutdownRequested() const noexcept;
    [[nodiscard]] Ui::ApplicationShell* shell() noexcept;
    [[nodiscard]] const Ui::ApplicationShell* shell() const noexcept;
    [[nodiscard]] const std::optional<Application::StartupWorkspaceOutcome>&
    startupWorkspaceOutcome() const noexcept;
    [[nodiscard]] Application::WorkspaceStateSnapshot
    workspaceStateSnapshot() const;

private:
    struct CoreServices;

    [[nodiscard]] bool createCoreServices();
    [[nodiscard]] bool loadStartupPreferences();
    [[nodiscard]] Application::StartupStageOutcome loadStartupLanguage();
    [[nodiscard]] bool loadStartupTheme();
    [[nodiscard]] bool loadRequiredFonts();
    [[nodiscard]] bool loadRequiredDocumentCatalog();
    [[nodiscard]] Application::StartupStageOutcome openStartupWorkspace();
    [[nodiscard]] std::optional<Application::WorkspaceLocation>
    commandLineStartupWorkspaceLocation() const;
    void ensureWorkspaceComposition();
    void releaseWorkspace() noexcept;
    void scheduleStartupUpdateCheck();
    void tryDispatchStartupUpdateCheck();
    [[nodiscard]] Application::StartupResult failStartupStage(
        Application::StartupStage stage,
        Application::StartupFailureReason reason =
            Application::StartupFailureReason::StageFailed
        );
    void rollbackStartupComposition() noexcept;
    void observeStartupResult(const Application::StartupResult& result) const;

    QApplication& m_application;
    StartupEventObserver m_startupEventObserver;
    StartupDiagnosticsSinkFactory m_startupDiagnosticsSinkFactory;
    ResourcePackagePathsResolver m_resourcePackagePathsResolver;
    std::optional<QStringList> m_systemUiLanguages;
    StartupWorkspaceSeams m_startupWorkspaceSeams;
    StartupUpdateDispatcherFactory m_startupUpdateDispatcherFactory;
    std::shared_ptr<int> m_callbackLifetime = std::make_shared<int>(0);
    std::unique_ptr<StartupDiagnosticsSink> m_startupDiagnosticsSink;
    std::unique_ptr<StartupUpdateDispatcher> m_startupUpdateDispatcher;
    std::unique_ptr<CoreServices> m_coreServices;
    std::unique_ptr<Application::WorkspaceGateway> m_workspaceGateway;
    std::unique_ptr<Application::ApplicationRuntime> m_applicationRuntime;
    std::optional<Application::StartupWorkspaceOutcome> m_startupWorkspaceOutcome;
    Application::StartupCoordinator m_startupCoordinator;
    Application::ShutdownCoordinator m_shutdownCoordinator;
    std::unique_ptr<Ui::ApplicationShell> m_shell;
    bool m_previousQuitOnLastWindowClosed = true;
    bool m_quitOnLastWindowClosedChanged = false;
    bool m_startupUpdateCheckScheduled = false;
    bool m_startupUpdateCheckConsumed = false;
};

}  // namespace ClassMngr::Next::Bootstrap
