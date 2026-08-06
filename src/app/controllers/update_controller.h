#pragma once

#include <QObject>
#include <QPointer>

class ActionRegistry;
class MainWindow;
class SplashScreen;
class UpdateDialog;
class UpdateService;

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
    void setSplashScreen(
        SplashScreen* splash
        );
    void startStartupCheck();
    void setStartupComplete();

    [[nodiscard]] bool hasVisibleDialog() const;

private:
    void showManualUpdateDialog();
    void showAutomaticUpdateDialog();
    UpdateDialog* ensureDialog(
        bool automaticPrompt
        );
    void yieldSplashToDialog();
    void restoreSplashAfterDialog();

private:
    UpdateService* m_service = nullptr;
    QPointer<MainWindow> m_window;
    QPointer<SplashScreen> m_splash;
    QPointer<UpdateDialog> m_dialog;
    bool m_startupCheckStarted = false;
    bool m_startupComplete = false;
    bool m_automaticPromptSuppressed = false;
};
