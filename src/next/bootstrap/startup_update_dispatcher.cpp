#include "next/bootstrap/startup_update_dispatcher.h"

#include "core/updater/update_configuration.h"
#include "core/updater/update_service.h"
#include "next/application/startup_update_check_use_case.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"
#include "next/platform/update_service_startup_check_port.h"

#include <memory>

namespace ClassMngr::Next::Bootstrap
{
namespace
{

class ProductionStartupUpdateDispatcher final : public StartupUpdateDispatcher
{
public:
    ProductionStartupUpdateDispatcher()
        : m_configuration(UpdateConfiguration::fromBuild())
        , m_service(m_configuration)
        , m_servicePort(&m_service)
        , m_useCase(m_servicePort)
    {
    }

    void dispatch() override
    {
        const Application::StartupUpdateCheckInput input{
            .startupComplete = true,
            .checkOnStartup = m_configuration.checkOnStartup,
            .preferences = m_preferences.read(),
            .hasReleasesApiUrl = m_configuration.hasReleasesApiUrl()
        };
        static_cast<void>(m_useCase.start(
            input,
            [](Application::StartupUpdateCheckOutcome) {}
            ));
    }

private:
    UpdateConfiguration m_configuration;
    UpdateService m_service;
    Platform::UpdateServiceStartupCheckPort m_servicePort;
    Platform::SettingsManagerAutomaticUpdatePreferencesPort m_preferences;
    Application::StartupUpdateCheckUseCase m_useCase;
};

}  // namespace

std::unique_ptr<StartupUpdateDispatcher> makeStartupUpdateDispatcher()
{
    return std::make_unique<ProductionStartupUpdateDispatcher>();
}

}  // namespace ClassMngr::Next::Bootstrap
