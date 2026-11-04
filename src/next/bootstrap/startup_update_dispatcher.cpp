#include "next/bootstrap/startup_update_dispatcher.h"
#include "next/bootstrap/update_coordinator.h"

#include "core/updater/update_configuration.h"
#include "core/updater/update_service.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"
#include "next/platform/settings_manager_skipped_update_version_port.h"
#include "next/platform/update_service_manual_check_port.h"
#include "next/platform/update_service_startup_check_port.h"
#include "next/platform/qt_update_download_port.h"
#include "next/platform/qt_update_package_action_port.h"

namespace ClassMngr::Next::Bootstrap
{
namespace
{

// The service outlives both adapters and the presentation. Manual and startup
// checks consequently share busy state, cache freshness, and terminal results.
class ProductionUpdateCoordinator final : public StartupUpdateDispatcher
{
public:
    explicit ProductionUpdateCoordinator(QWidget* presentationParent)
        : m_configuration(UpdateConfiguration::fromBuild())
        , m_service(m_configuration)
        , m_startupPort(&m_service)
        , m_manualPort(&m_service)
        , m_downloadPort()
        , m_packageActionPort()
        , m_presentation(m_startupPort, m_manualPort, m_downloadPort,
              m_packageActionPort, m_automaticPreferences, m_skippedPreferences,
              m_configuration.checkOnStartup,
              m_configuration.hasReleasesApiUrl(), presentationParent)
    {
    }

    ~ProductionUpdateCoordinator() override { cancel(); }
    void dispatch() override { m_presentation.dispatch(); }
    void openManual() override { m_presentation.openManual(); }
    void cancel() noexcept override
    {
        m_presentation.cancel();
        m_service.cancelActiveCheck();
    }

private:
    UpdateConfiguration m_configuration;
    UpdateService m_service;
    Platform::UpdateServiceStartupCheckPort m_startupPort;
    Platform::UpdateServiceManualCheckPort m_manualPort;
    Platform::SettingsManagerAutomaticUpdatePreferencesPort m_automaticPreferences;
    Platform::SettingsManagerSkippedUpdateVersionPort m_skippedPreferences;
    Platform::QtUpdateDownloadPort m_downloadPort;
    Platform::QtUpdatePackageActionPort m_packageActionPort;
    UpdateCoordinator m_presentation;
};

} // namespace

std::unique_ptr<StartupUpdateDispatcher> makeUpdateCoordinator(QWidget* presentationParent)
{
    return std::make_unique<ProductionUpdateCoordinator>(presentationParent);
}

std::unique_ptr<StartupUpdateDispatcher> makeStartupUpdateDispatcher()
{
    return makeUpdateCoordinator(nullptr);
}

} // namespace ClassMngr::Next::Bootstrap
