#pragma once

#include <QObject>
#include <QPointer>

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
        QObject* parent = nullptr
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
    [[nodiscard]] bool isVersionSkipped(
        const QString& version
        ) const;
    void reconcileSkippedVersion(
        const UpdateCheckResult& result
        );
    void skipVersion(
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
    QPointer<MainWindow> m_window;
    QPointer<UpdateDialog> m_dialog;
    bool m_automaticCheckStarted = false;
    bool m_startupMaintenanceRun = false;
    bool m_startupComplete = false;
    bool m_automaticPromptSuppressed = false;
};
