#pragma once

#include "next/application/automatic_update_preferences.h"
#include "next/application/manual_update_check.h"
#include "next/application/skipped_update_version_preferences.h"
#include "next/application/startup_update_check_use_case.h"
#include "next/bootstrap/startup_update_dispatcher.h"

#include <QObject>
#include <QPointer>

class QWidget;
class UpdateDialog;

namespace ClassMngr::Next::Bootstrap
{

// Qt composition/presentation edge. Ports are borrowed and must outlive this
// coordinator; the bootstrap owns the production ports and their single service.
class UpdateCoordinator final : public QObject, public StartupUpdateDispatcher
{
public:
    UpdateCoordinator(
        Application::StartupUpdateCheckPort& startupPort,
        Application::ManualUpdateCheckPort& manualPort,
        const Application::AutomaticUpdatePreferencesPort& automaticPreferences,
        const Application::SkippedUpdateVersionPreferencesPort& skippedPreferences,
        bool checkOnStartup,
        bool hasReleasesApiUrl,
        QWidget* presentationParent
        );
    ~UpdateCoordinator() override;

    void dispatch() override;
    void openManual() override;
    void cancel() noexcept override;

private:
    UpdateDialog* ensureDialog(bool automatic);
    bool reconcile(const Application::ManualUpdateCheckResult& result);
    void maintainDownloads(const Application::ManualUpdateCheckResult& result);
    void runMaintenance();

    Application::ManualUpdateCheckPort& m_manualPort;
    const Application::AutomaticUpdatePreferencesPort& m_automaticPreferences;
    const Application::SkippedUpdateVersionPreferencesPort& m_skippedPreferences;
    Application::StartupUpdateCheckUseCase m_startup;
    QPointer<QWidget> m_parent;
    QPointer<UpdateDialog> m_dialog;
    bool m_checkOnStartup;
    bool m_hasReleasesApiUrl;
    bool m_maintenanceRun = false;
    bool m_promptSuppressed = false;
    bool m_cancelled = false;
};

// The QWidget dependency is confined to this presentation factory.
[[nodiscard]] std::unique_ptr<StartupUpdateDispatcher>
makeUpdateCoordinator(QWidget* presentationParent);

} // namespace ClassMngr::Next::Bootstrap
