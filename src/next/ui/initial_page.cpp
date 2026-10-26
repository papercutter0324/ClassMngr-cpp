#include "next/ui/initial_page.h"

#include <QCoreApplication>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{

QString initialPageText(const char* sourceText)
{
    return QCoreApplication::translate("InitialPage", sourceText);
}

} // namespace

namespace ClassMngr::Next::Ui
{

InitialPage::InitialPage(
    const PageContext& context,
    InitialSetupPageActions actions,
    QWidget* parent
    )
    : Page(context, parent),
      m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("classMngrNextInitialSetupPage"));

    auto* title = new QLabel(initialPageText("Initial Setup"), this);
    title->setObjectName(QStringLiteral("classMngrNextInitialSetupTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 8);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);

    auto* description = new QLabel(
        initialPageText(
            "Create a ClassMngr profile to begin setup. Your profile is saved as a .tps file."
            ),
        this
        );
    description->setObjectName(
        QStringLiteral("classMngrNextInitialSetupDescription")
        );
    description->setWordWrap(true);
    description->setAlignment(Qt::AlignHCenter);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupStatus")
        );
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setText(initialPageText("Choose where to save your profile."));

    m_profileLabel = new QLabel(this);
    m_profileLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupProfileLocation")
        );
    m_profileLabel->setWordWrap(true);
    m_profileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_recoveryLabel = new QLabel(this);
    m_recoveryLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupRecoveryLocation")
        );
    m_recoveryLabel->setWordWrap(true);
    m_recoveryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_createButton = new QPushButton(initialPageText("Create Profile…"), this);
    m_createButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    m_resumeButton = new QPushButton(initialPageText("Resume Setup"), this);
    m_resumeButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupResume")
        );
    m_restoreButton = new QPushButton(this);
    m_restoreButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupRestore")
        );
    m_cancelButton = new QPushButton(initialPageText("Cancel Setup"), this);
    m_cancelButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(48, 44, 48, 44);
    layout->setSpacing(12);
    layout->addStretch(1);
    layout->addWidget(title, 0, Qt::AlignHCenter);
    layout->addWidget(description);
    layout->addSpacing(8);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_profileLabel);
    layout->addWidget(m_recoveryLabel);
    layout->addWidget(m_createButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_resumeButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_restoreButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_cancelButton, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    connect(m_createButton, &QPushButton::clicked, this, [this] {
        if (m_actions.createProfile)
        {
            m_actions.createProfile();
        }
        refreshSetupState();
    });
    connect(m_resumeButton, &QPushButton::clicked, this, [this] {
        if (m_actions.resumeSetup)
        {
            m_actions.resumeSetup();
        }
        refreshSetupState();
    });
    connect(m_restoreButton, &QPushButton::clicked, this, [this] {
        if (m_actions.restoreSetup)
        {
            m_actions.restoreSetup();
        }
        refreshSetupState();
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this] {
        if (m_actions.cancelSetup)
        {
            m_actions.cancelSetup();
        }
        refreshSetupState();
    });

    refreshSetupState();
}

void InitialPage::refreshSetupState()
{
    const InitialSetupPageState state = m_actions.state
        ? m_actions.state()
        : InitialSetupPageState{};

    if (!state.statusMessage.isEmpty())
    {
        m_statusLabel->setText(state.statusMessage);
    }
    else if (state.setupPending)
    {
        m_statusLabel->setText(initialPageText("Profile created. Setup is pending."));
    }
    else if (state.recoveryAvailable)
    {
        m_statusLabel->setText(initialPageText("An interrupted setup needs your choice."));
    }
    else if (state.recoveryError)
    {
        m_statusLabel->setText(initialPageText("The setup recovery record could not be read."));
    }
    else
    {
        m_statusLabel->setText(initialPageText("Choose where to save your profile."));
    }

    if (state.setupPending && !state.profileLocation.isEmpty())
    {
        m_profileLabel->setText(
            initialPageText("Profile: %1").arg(state.profileLocation)
            );
        m_profileLabel->show();
    }
    else if (state.recoveryAvailable
             && !state.recoveryProfileLocation.isEmpty())
    {
        m_profileLabel->setText(
            initialPageText("Profile to recover: %1").arg(state.recoveryProfileLocation)
            );
        m_profileLabel->show();
    }
    else
    {
        m_profileLabel->clear();
        m_profileLabel->hide();
    }

    if ((state.recoveryAvailable || state.setupPending)
        && state.recoveryHasOriginalProfile)
    {
        m_recoveryLabel->setText(
            initialPageText("The previous profile is preserved until setup is complete.")
            );
        m_recoveryLabel->show();
        m_restoreButton->setText(initialPageText("Restore Previous Profile"));
    }
    else if (state.recoveryAvailable)
    {
        m_recoveryLabel->setText(
            initialPageText(
                "No previous profile was replaced. You can resume or discard this profile."
                )
            );
        m_recoveryLabel->show();
        m_restoreButton->setText(initialPageText("Discard Incomplete Profile"));
    }
    else if (state.setupPending)
    {
        m_recoveryLabel->clear();
        m_recoveryLabel->hide();
    }
    else
    {
        m_recoveryLabel->clear();
        m_recoveryLabel->hide();
    }

    m_createButton->setVisible(
        !state.setupPending && !state.recoveryAvailable && !state.recoveryError
        );
    m_resumeButton->setVisible(
        !state.setupPending && state.recoveryAvailable
        );
    m_restoreButton->setVisible(
        !state.setupPending && state.recoveryAvailable
        );
    m_cancelButton->setVisible(state.setupPending);
}

} // namespace ClassMngr::Next::Ui
