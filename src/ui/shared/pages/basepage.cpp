#include "basepage.h"
#include "core/fontmanager.h"

#include <algorithm>
#include <array>
#include <QFrame>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QWidget>

namespace
{
constexpr int NoDatabaseBannerSpacer = 8;
constexpr int NoDatabaseBannerTitlePointSizeAtLarge = 20;
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

    auto* initialSetupLayout =
        new QHBoxLayout;

    initialSetupLayout->setContentsMargins(
        0,
        4,
        0,
        0
        );

    initialSetupLayout->setSpacing(8);

    m_initialSetupButton =
        new QPushButton(m_noDatabaseBanner);

    m_initialSetupButton->setObjectName(
        QStringLiteral("noDatabaseSetupButton")
        );

    m_initialSetupButton->setDefault(true);

    initialSetupLayout->addWidget(
        m_initialSetupButton
        );

    m_initialSetupDescription =
        new QLabel(m_noDatabaseBanner);

    m_initialSetupDescription->setObjectName(
        QStringLiteral("noDatabaseSetupDescription")
        );

    m_initialSetupDescription->setWordWrap(true);

    m_initialSetupDescription->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    initialSetupLayout->addWidget(
        m_initialSetupDescription,
        1
        );

    bannerLayout->addLayout(
        initialSetupLayout
        );

    auto* newProfileLayout =
        new QHBoxLayout;

    newProfileLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    newProfileLayout->setSpacing(8);

    m_newDatabaseButton =
        new QPushButton(m_noDatabaseBanner);

    m_newDatabaseButton->setObjectName(
        QStringLiteral("noDatabaseNewButton")
        );

    newProfileLayout->addWidget(
        m_newDatabaseButton
        );

    m_newDatabaseDescription =
        new QLabel(m_noDatabaseBanner);

    m_newDatabaseDescription->setObjectName(
        QStringLiteral("noDatabaseNewDescription")
        );

    m_newDatabaseDescription->setWordWrap(true);

    m_newDatabaseDescription->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    newProfileLayout->addWidget(
        m_newDatabaseDescription,
        1
        );

    bannerLayout->addLayout(
        newProfileLayout
        );

    auto* openProfileLayout =
        new QHBoxLayout;

    openProfileLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    openProfileLayout->setSpacing(8);

    m_openDatabaseButton =
        new QPushButton(m_noDatabaseBanner);

    m_openDatabaseButton->setObjectName(
        QStringLiteral("noDatabaseOpenButton")
        );

    openProfileLayout->addWidget(
        m_openDatabaseButton
        );

    m_openDatabaseDescription =
        new QLabel(m_noDatabaseBanner);

    m_openDatabaseDescription->setObjectName(
        QStringLiteral("noDatabaseOpenDescription")
        );

    m_openDatabaseDescription->setWordWrap(true);

    m_openDatabaseDescription->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    openProfileLayout->addWidget(
        m_openDatabaseDescription,
        1
        );

    bannerLayout->addLayout(
        openProfileLayout
        );

    connect(
        m_initialSetupButton,
        &QPushButton::clicked,
        this,
        &BasePage::initialSetupRequested
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

    updateNoDatabaseTitleFont();

    if (m_initialSetupButton)
    {
        m_initialSetupButton->setText(
            tr("Initial Setup")
            );
    }

    if (m_initialSetupDescription)
    {
        m_initialSetupDescription->setText(
            tr("— A guided setup to help you import or add your schedule, "
               "classes, and co-teachers.")
            );
    }

    if (m_newDatabaseButton)
    {
        m_newDatabaseButton->setText(
            tr("New Profile")
            );
    }

    if (m_newDatabaseDescription)
    {
        m_newDatabaseDescription->setText(
            tr("— Create a new Teacher Profile and manually enter your schedule, "
               "classes, and co-teachers.")
            );
    }

    if (m_openDatabaseButton)
    {
        m_openDatabaseButton->setText(
            tr("Open Profile")
            );
    }

    if (m_openDatabaseDescription)
    {
        m_openDatabaseDescription->setText(
            tr("— Open an existing Teacher Profile file.")
            );
    }

    updateNoDatabaseBannerButtonWidths();
}

void BasePage::clearDatabaseState()
{
}

Status BasePage::prepareForActivation()
{
    return {};
}

void BasePage::releaseFeatureResources()
{
}

PageOutputCapabilities BasePage::outputCapabilities() const
{
    return {};
}

void BasePage::printCurrentPage()
{
}

void BasePage::saveCurrentPageAs()
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

    const bool databaseStateChanged =
        m_databaseOpen != databaseOpen;

    m_databaseOpen = databaseOpen;

    m_noDatabaseBannerEnabled =
        !databaseOpen;

    m_noDatabaseBanner->setVisible(
        m_noDatabaseBannerEnabled
        );

    updateNoDatabaseBannerGeometry();
    updateNoDatabaseBannerLayout();

    if (databaseStateChanged)
    {
        emit outputCapabilitiesChanged();
    }
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

        return;
    }

    if (
        event->type() == QEvent::ApplicationFontChange
        || event->type() == QEvent::FontChange
        )
    {
        scheduleNoDatabaseBannerFontUpdate();
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

bool BasePage::isDatabaseOpen() const
{
    return m_databaseOpen;
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

void BasePage::updateNoDatabaseBannerButtonWidths()
{
    const std::array<QPushButton*, 3> buttons{
        m_initialSetupButton,
        m_newDatabaseButton,
        m_openDatabaseButton
    };

    int buttonWidth = 0;

    for (QPushButton* button : buttons)
    {
        if (!button)
        {
            continue;
        }

        button->setMinimumWidth(0);
        button->setMaximumWidth(QWIDGETSIZE_MAX);

        buttonWidth = std::max(
            buttonWidth,
            button->sizeHint().width()
            );
    }

    for (QPushButton* button : buttons)
    {
        if (button)
        {
            button->setFixedWidth(buttonWidth);
        }
    }
}

void BasePage::updateNoDatabaseTitleFont()
{
    if (!m_noDatabaseBanner || !m_noDatabaseTitle)
    {
        return;
    }

    QFont titleFont =
        m_noDatabaseBanner->font();

    const int standardFontSize =
        titleFont.pointSize();

    if (standardFontSize <= 0)
    {
        return;
    }

    const int standardFontSizeAtLarge =
        standardFontSize
        - FontManager::sizeOffset()
        + fontSizeOffset(FontSize::Large);

    const int titleFontSizeDifference =
        NoDatabaseBannerTitlePointSizeAtLarge
        - standardFontSizeAtLarge;

    titleFont.setPointSize(
        std::max(
            1,
            standardFontSize + titleFontSizeDifference
            )
        );

    m_noDatabaseTitle->setFont(titleFont);
}

void BasePage::scheduleNoDatabaseBannerFontUpdate()
{
    if (m_noDatabaseBannerFontUpdateQueued)
    {
        return;
    }

    m_noDatabaseBannerFontUpdateQueued = true;

    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            m_noDatabaseBannerFontUpdateQueued = false;

            updateNoDatabaseTitleFont();
            updateNoDatabaseBannerButtonWidths();
            updateNoDatabaseBannerGeometry();
            updateNoDatabaseBannerLayout();
        }
        );
}
