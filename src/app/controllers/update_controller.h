#pragma once

#include "core/updater/update_service.h"

#include <QObject>

class ActionRegistry;
class MainWindow;

class UpdateController : public QObject
{
    Q_OBJECT

public:
    explicit UpdateController(
        MainWindow* window,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    void maybeCheckOnStartup();

private:
    void showManualUpdateDialog();
    void showUpdateDialogForResult(
        const UpdateCheckResult& result
        );

private:
    MainWindow* m_window = nullptr;
    bool m_startupCheckStarted = false;
};
