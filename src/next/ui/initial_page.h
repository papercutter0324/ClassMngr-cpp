#pragma once

#include "next/ui/page.h"

#include <QString>

#include <functional>

class QLabel;
class QPushButton;

namespace ClassMngr::Next::Ui
{

struct InitialSetupPageState final
{
    bool setupPending = false;
    bool recoveryAvailable = false;
    bool recoveryError = false;
    bool recoveryHasOriginalProfile = false;
    QString profileLocation;
    QString recoveryProfileLocation;
    QString statusMessage;
};

struct InitialSetupPageActions final
{
    std::function<InitialSetupPageState()> state;
    std::function<void()> createProfile;
    std::function<void()> resumeSetup;
    std::function<void()> restoreSetup;
    std::function<void()> cancelSetup;
};

class InitialPage final : public Page
{
    Q_OBJECT

public:
    explicit InitialPage(
        const PageContext& context,
        InitialSetupPageActions actions = {},
        QWidget* parent = nullptr
        );

    void refreshSetupState();

private:
    InitialSetupPageActions m_actions;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_profileLabel = nullptr;
    QLabel* m_recoveryLabel = nullptr;
    QPushButton* m_createButton = nullptr;
    QPushButton* m_resumeButton = nullptr;
    QPushButton* m_restoreButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};

}  // namespace ClassMngr::Next::Ui
