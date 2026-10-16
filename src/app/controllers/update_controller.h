#pragma once

#include "next/domain/operation_result.h"

#include <QObject>
#include <QPointer>

#include <memory>

namespace ClassMngr::Next::Application
{
class SkippedUpdateVersionPreferencesPort;
class StartupUpdateCheckPort;
class StartupUpdateCheckUseCase;
struct StartupUpdateCheckResult;
}

class ActionRegistry;
class MainWindow;
class UpdateDialog;
class UpdateService;
struct UpdateCheckResult;

class UpdateController : public QObject
{
    Q_OBJECT

public:
    explicit UpdateController(
        UpdateService* service,
        QObject* parent = nullptr,
        std::shared_ptr<
            ClassMngr::Next::Application::
                SkippedUpdateVersionPreferencesPort
            > skippedUpdateVersionPreferencesPort = {}
        );
    ~UpdateController() override;

    void attachMainWindow(
        MainWindow* window,
        ActionRegistry& actions
        );
    void startAutomaticCheck();
    void setStartupComplete();

    [[nodiscard]] bool hasVisibleDialog() const;

private:
    [[nodiscard]] bool automaticChecksEnabled() const;
    void runStartupMaintenance();
    [[nodiscard]] bool reconcileSkippedVersion(
        const ClassMngr::Next::Application::StartupUpdateCheckResult& result
        );
    void handleCheckSucceeded(
        const ClassMngr::Next::Application::StartupUpdateCheckResult& result
        );
    [[nodiscard]] ClassMngr::Next::Domain::Result<void> skipVersion(
        const QString& version
        );
    [[nodiscard]] ClassMngr::Next::Domain::Result<void> unskipVersion();
    void showManualUpdateDialog();
    void showAutomaticUpdateDialog();
    UpdateDialog* ensureDialog(
        bool automaticPrompt
        );

private:
    QPointer<UpdateService> m_service;
    std::shared_ptr<
        ClassMngr::Next::Application::SkippedUpdateVersionPreferencesPort
        > m_skippedUpdateVersionPreferencesPort;
    QPointer<MainWindow> m_window;
    QPointer<UpdateDialog> m_dialog;
    std::unique_ptr<ClassMngr::Next::Application::StartupUpdateCheckPort>
        m_startupUpdateCheckPort;
    std::unique_ptr<ClassMngr::Next::Application::StartupUpdateCheckUseCase>
        m_startupUpdateCheckUseCase;
    bool m_startupCheckActive = false;
    bool m_startupMaintenanceRun = false;
    bool m_startupComplete = false;
    bool m_automaticPromptSuppressed = false;
};
