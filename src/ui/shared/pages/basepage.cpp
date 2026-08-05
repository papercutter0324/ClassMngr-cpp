#include "basepage.h"

#include <QFrame>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QWidget>

namespace
{
constexpr int NoDatabaseBannerSpacer = 8;
}



// =========================================================
// Constructor
// =========================================================

BasePage::BasePage(
    QWidget* parent
    )
    : QWidget(parent)
{
    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );



    // =====================================================
    // Content Layout
    // =====================================================

    m_contentLayout =
        new QVBoxLayout;



    // =====================================================
    // Main Layout
    // =====================================================

    m_mainLayout =
        new QVBoxLayout(this);

    m_defaultMainLayoutMargins =
        m_mainLayout->contentsMargins();

    m_mainLayout->addLayout(
        m_contentLayout
        );

    m_mainLayout->setStretch(
        0,
        1
        );

    m_mainLayout->addStretch();



    // =====================================================
    // Bottom Bar
    // =====================================================

    m_bottomBar =
        new QWidget(this);

    m_bottomLayout =
        new QHBoxLayout(m_bottomBar);

    m_bottomLayout->setContentsMargins(
        16,
        8,
        16,
        8
        );

    m_bottomLayout->setSpacing(8);

    m_mainLayout->addWidget(
        m_bottomBar
        );



    // =====================================================
    // No Database Banner
    // =====================================================

    m_noDatabaseBanner =
        new QFrame(this);

    m_noDatabaseBanner->setObjectName(
        QStringLiteral("noDatabaseBanner")
        );

    auto* bannerLayout =
        new QVBoxLayout(m_noDatabaseBanner);

    bannerLayout->setContentsMargins(
        20,
        16,
        20,
        16
        );

    bannerLayout->setSpacing(8);

    m_noDatabaseTitle =
        new QLabel(m_noDatabaseBanner);

    m_noDatabaseTitle->setObjectName(
        QStringLiteral("noDatabaseTitle")
        );

    bannerLayout->addWidget(
        m_noDatabaseTitle
        );

    m_noDatabaseMessage =
        new QLabel(m_noDatabaseBanner);

    m_noDatabaseMessage->setObjectName(
        QStringLiteral("noDatabaseMessage")
        );

    m_noDatabaseMessage->setWordWrap(true);

    bannerLayout->addWidget(
        m_noDatabaseMessage
        );

    m_noDatabaseStepOne =
        new QLabel(m_noDatabaseBanner);

    m_noDatabaseStepOne->setObjectName(
        QStringLiteral("noDatabaseStepOne")
        );

    m_noDatabaseStepOne->setWordWrap(true);

    bannerLayout->addWidget(
        m_noDatabaseStepOne
        );

    m_noDatabaseStepTwo =
        new QLabel(m_noDatabaseBanner);

    m_noDatabaseStepTwo->setObjectName(
        QStringLiteral("noDatabaseStepTwo")
        );

    m_noDatabaseStepTwo->setWordWrap(true);

    bannerLayout->addWidget(
        m_noDatabaseStepTwo
        );

    m_noDatabaseStepThree =
        new QLabel(m_noDatabaseBanner);

    m_noDatabaseStepThree->setObjectName(
        QStringLiteral("noDatabaseStepThree")
        );

    m_noDatabaseStepThree->setWordWrap(true);

    bannerLayout->addWidget(
        m_noDatabaseStepThree
        );

    m_noDatabaseNextSteps =
        new QLabel(m_noDatabaseBanner);

    m_noDatabaseNextSteps->setObjectName(
        QStringLiteral("noDatabaseNextSteps")
        );

    m_noDatabaseNextSteps->setWordWrap(true);

    bannerLayout->addWidget(
        m_noDatabaseNextSteps
        );

    auto* buttonLayout =
        new QHBoxLayout;

    buttonLayout->setContentsMargins(
        0,
        4,
        0,
        0
        );

    buttonLayout->setSpacing(8);

    m_newDatabaseButton =
        new QPushButton(m_noDatabaseBanner);

    m_newDatabaseButton->setObjectName(
        QStringLiteral("noDatabaseNewButton")
        );

    buttonLayout->addWidget(
        m_newDatabaseButton
        );

    m_openDatabaseButton =
        new QPushButton(m_noDatabaseBanner);

    m_openDatabaseButton->setObjectName(
        QStringLiteral("noDatabaseOpenButton")
        );

    buttonLayout->addWidget(
        m_openDatabaseButton
        );

    buttonLayout->addStretch();

    bannerLayout->addLayout(
        buttonLayout
        );

    connect(
        m_openDatabaseButton,
        &QPushButton::clicked,
        this,
        &BasePage::openDatabaseRequested
        );

    connect(
        m_newDatabaseButton,
        &QPushButton::clicked,
        this,
        &BasePage::newDatabaseRequested
        );

    retranslateUi();

    m_noDatabaseBanner->hide();
}



// =========================================================
// Persistence
// =========================================================

void BasePage::saveData()
{
}

bool BasePage::saveChanges()
{
    saveData();

    return !hasUnsavedChanges();
}

bool BasePage::hasUnsavedChanges() const
{
    return false;
}

void BasePage::discardChanges()
{
}

QString BasePage::unsavedChangesTitle() const
{
    return tr("Unsaved Changes");
}

QString BasePage::unsavedChangesMessage() const
{
    return tr("This page has unsaved changes.");
}

void BasePage::setSaveMode(
    SaveMode mode
    )
{
    Q_UNUSED(mode);
}



// =========================================================
// Refresh
// =========================================================

void BasePage::refresh()
{
    if (!isVisible())
    {
        return;
    }

    update();
}

void BasePage::retranslateUi()
{
    if (m_noDatabaseTitle)
    {
        m_noDatabaseTitle->setText(
            tr("Getting Started")
            );
    }

    if (m_noDatabaseMessage)
    {
        m_noDatabaseMessage->setText(
            tr("No database is open. Set up ClassMngr in this order:")
            );
    }

    if (m_noDatabaseStepOne)
    {
        m_noDatabaseStepOne->setText(
            tr("1. Create a new database, or open an existing one.")
            );
    }

    if (m_noDatabaseStepTwo)
    {
        m_noDatabaseStepTwo->setText(
            tr("2. Create or import your Korean teachers.")
            );
    }

    if (m_noDatabaseStepThree)
    {
        m_noDatabaseStepThree->setText(
            tr("3. Create your classes and assign their teachers.")
            );
    }

    if (m_noDatabaseNextSteps)
    {
        m_noDatabaseNextSteps->setText(
            tr("Next, add schedules and rosters, then fill in any other information you need.")
            );
    }

    if (m_openDatabaseButton)
    {
        m_openDatabaseButton->setText(
            tr("Open Database...")
            );
    }

    if (m_newDatabaseButton)
    {
        m_newDatabaseButton->setText(
            tr("New Database...")
            );
    }
}

void BasePage::clearDatabaseState()
{
}

void BasePage::setDatabaseOpen(
    bool databaseOpen
    )
{
    if (!m_noDatabaseBanner)
    {
        return;
    }

    m_noDatabaseBannerEnabled =
        !databaseOpen;

    m_noDatabaseBanner->setVisible(
        m_noDatabaseBannerEnabled
        );

    updateNoDatabaseBannerGeometry();
    updateNoDatabaseBannerLayout();
}

void BasePage::changeEvent(
    QEvent* event
    )
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::LanguageChange)
    {
        BasePage::retranslateUi();
        updateNoDatabaseBannerGeometry();
        updateNoDatabaseBannerLayout();
    }
}

void BasePage::resizeEvent(
    QResizeEvent* event
    )
{
    QWidget::resizeEvent(event);

    updateNoDatabaseBannerGeometry();
    updateNoDatabaseBannerLayout();
}



// =========================================================
// Accessors
// =========================================================

QVBoxLayout* BasePage::contentLayout() const
{
    return m_contentLayout;
}

QHBoxLayout* BasePage::bottomLayout() const
{
    return m_bottomLayout;
}

void BasePage::setPageLayoutMargins(
    const QMargins& margins
    )
{
    if (!m_mainLayout)
    {
        return;
    }

    m_defaultMainLayoutMargins = margins;
    m_mainLayout->setContentsMargins(margins);
    updateNoDatabaseBannerLayout();
}

void BasePage::setBottomBarVisible(
    bool visible
    )
{
    if (m_bottomBar)
    {
        m_bottomBar->setVisible(visible);
    }
}

void BasePage::updateNoDatabaseBannerGeometry()
{
    if (!m_noDatabaseBanner)
    {
        return;
    }

    int bannerHeight =
        m_noDatabaseBanner->heightForWidth(
            width()
            );

    if (bannerHeight < 0)
    {
        bannerHeight =
            m_noDatabaseBanner->sizeHint().height();
    }

    m_noDatabaseBanner->setGeometry(
        0,
        0,
        width(),
        bannerHeight
        );

    m_noDatabaseBanner->raise();
}

void BasePage::updateNoDatabaseBannerLayout()
{
    if (!m_mainLayout || !m_noDatabaseBanner)
    {
        return;
    }

    QMargins margins =
        m_defaultMainLayoutMargins;

    if (m_noDatabaseBannerEnabled)
    {
        margins.setTop(
            margins.top()
            + m_noDatabaseBanner->height()
            + NoDatabaseBannerSpacer
            );
    }

    if (m_mainLayout->contentsMargins() != margins)
    {
        m_mainLayout->setContentsMargins(margins);
    }
}
