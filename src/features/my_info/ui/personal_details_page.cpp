#include "personal_details_page.h"

#include "ui/shared/styles/roles.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>

namespace
{
constexpr int AutosaveDelayMs = 750;
}

PersonalDetailsPage::PersonalDetailsPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::MyInfo);
    buildUi();

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(AutosaveDelayMs);

    connect(
        m_autosaveTimer,
        &QTimer::timeout,
        this,
        &PersonalDetailsPage::autosave
        );

    loadPageData();
}

void PersonalDetailsPage::refresh()
{
    BasePage::refresh();

    if (isVisible() && !m_dirty)
    {
        loadPageData();
    }
}

void PersonalDetailsPage::retranslateUi()
{
    m_titleLabel->setText(tr("My Details"));
    m_subtitleLabel->setText(
        tr("Manage your personal information and signature.")
        );
    m_myInformationHeading->setText(tr("My Information"));
    m_signatureHeading->setText(tr("Signature"));
    m_signatureInstructionsLabel->setText(
        tr("Add a PNG or JPEG signature image. Other supported image formats are converted to PNG.")
        );
    m_chooseSignatureButton->setText(
        m_signatureImageData.isEmpty()
            ? tr("Add Signature Image...")
            : tr("Replace Signature Image...")
        );
    m_removeSignatureButton->setText(tr("Remove"));
    m_nameLabel->setText(tr("My Name"));
    m_campusLabel->setText(tr("My Campus"));
    m_zoomLoginIdLabel->setText(tr("Zoom Login ID"));
    m_zoomPasswordLabel->setText(tr("Zoom Password"));
    m_zoomLabel->setText(tr("Zoom"));
    m_zoomNotAvailableCheck->setText(tr("N/A"));
    updateSignaturePreview();
}

void PersonalDetailsPage::saveData()
{
    saveMyInfoInternal();
}

bool PersonalDetailsPage::saveChanges()
{
    m_autosaveTimer->stop();
    return saveMyInfoInternal();
}

bool PersonalDetailsPage::hasUnsavedChanges() const
{
    return m_dirty;
}

void PersonalDetailsPage::discardChanges()
{
    m_autosaveTimer->stop();
    loadPageData();
}

void PersonalDetailsPage::setSaveMode(SaveMode mode)
{
    m_saveMode = mode;

    if (m_saveMode == SaveMode::Automatic && hasUnsavedChanges())
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

void PersonalDetailsPage::scrollToTop()
{
    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            if (auto* scrollBar = m_scrollArea->verticalScrollBar())
            {
                scrollBar->setValue(scrollBar->minimum());
            }
        }
        );
}

void PersonalDetailsPage::showEvent(QShowEvent* event)
{
    BasePage::showEvent(event);

    if (!m_dirty)
    {
        loadPageData();
    }
}
