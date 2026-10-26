#pragma once

#include "next/application/shutdown_coordinator.h"
#include "next/application/startup_coordinator.h"
#include "next/bootstrap/startup_diagnostics_sink.h"
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
    using ResourcePackagePathsResolver =
        std::function<Resources::ResourceResult<Resources::ResourcePackagePaths>()>;

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
        std::optional<QStringList> systemUiLanguages = std::nullopt
        );
    ~ApplicationBootstrap();

    ApplicationBootstrap(const ApplicationBootstrap&) = delete;
    ApplicationBootstrap& operator=(const ApplicationBootstrap&) = delete;

    [[nodiscard]] Application::StartupResult start();
    [[nodiscard]] Application::ShutdownResult shutdown();
    [[nodiscard]] bool shutdownRequested() const noexcept;
    [[nodiscard]] Ui::ApplicationShell* shell() noexcept;
    [[nodiscard]] const Ui::ApplicationShell* shell() const noexcept;

private:
    struct CoreServices;

    [[nodiscard]] bool createCoreServices();
    [[nodiscard]] bool loadStartupPreferences();
    [[nodiscard]] Application::StartupStageOutcome loadStartupLanguage();
    [[nodiscard]] bool loadStartupTheme();
    [[nodiscard]] bool loadRequiredFonts();
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
    std::unique_ptr<StartupDiagnosticsSink> m_startupDiagnosticsSink;
    std::unique_ptr<CoreServices> m_coreServices;
    Application::StartupCoordinator m_startupCoordinator;
    Application::ShutdownCoordinator m_shutdownCoordinator;
    std::unique_ptr<Ui::ApplicationShell> m_shell;
    bool m_previousQuitOnLastWindowClosed = true;
    bool m_quitOnLastWindowClosedChanged = false;
};

}  // namespace ClassMngr::Next::Bootstrap
