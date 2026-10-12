#pragma once

#include "next/domain/operation_result.h"

#include <QObject>
#include <QPointer>

#include <memory>

namespace ClassMngr::Next::Application
{
class SkippedUpdateVersionPreferencesPort;
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
        const UpdateCheckResult& result
        );
    [[nodiscard]] ClassMngr::Next::Domain::Result<void> skipVersion(
        const QString& version
        );
    void unskipVersion();
    void showManualUpdateDialog();
    void showAutomaticUpdateDialog();
    UpdateDialog* ensureDialog(
        bool automaticPrompt
        );

private:
    UpdateService* m_service = nullptr;
    std::shared_ptr<
        ClassMngr::Next::Application::SkippedUpdateVersionPreferencesPort
        > m_skippedUpdateVersionPreferencesPort;
    QPointer<MainWindow> m_window;
    QPointer<UpdateDialog> m_dialog;
    bool m_automaticCheckStarted = false;
    bool m_startupMaintenanceRun = false;
    bool m_startupComplete = false;
    bool m_automaticPromptSuppressed = false;
};
