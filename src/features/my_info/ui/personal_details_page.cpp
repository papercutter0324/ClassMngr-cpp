#include "personal_details_page.h"

#include "ui/shared/styles/roles.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>

PersonalDetailsPage::PersonalDetailsPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_autosave(new AutosaveCoordinator(this))
{
    setProperty("role", UiRoles::MyInfo);
    buildUi();

    connect(
        m_autosave,
        &AutosaveCoordinator::saveRequested,
        this,
        [this](bool) { saveMyInfoInternal(); }
        );

    loadPageData();
}

void PersonalDetailsPage::refresh()
{
    BasePage::refresh();

    if (isVisible() && !m_autosave->isDirty())
    {
        loadPageData();
    }
}

void PersonalDetailsPage::clearDatabaseState()
{
    m_autosave->setLoading(true);

    if (m_nameEdit)
    {
        m_nameEdit->clear();
    }

    if (m_campusCombo)
    {
        m_campusCombo->clear();
    }

    if (m_zoomLoginIdEdit)
    {
        m_zoomLoginIdEdit->clear();
    }

    if (m_zoomPasswordEdit)
    {
        m_zoomPasswordEdit->clear();
    }

    if (m_zoomNotAvailableCheck)
    {
        m_zoomNotAvailableCheck->setChecked(false);
    }

    m_signatureImageData.clear();
    m_signatureMode = SignatureMode::Image;
    m_typedSignatureFont = TypedSignatureFont::JustAnotherHand;

    if (m_typedSignatureEdit)
    {
        m_typedSignatureEdit->clear();
    }

    setZoomFieldsEnabled();
    updateMyInformationFieldWidths();
    updateSignatureControls();
    updateTypedSignatureFontOptions();
    updateSignaturePreview();

    m_autosave->setLoading(false);
    clearDirty();
}

void PersonalDetailsPage::retranslateUi()
{
    m_pageHeader->setTitle(tr("My Details"));
    m_pageHeader->setSubtitle(
        tr("Manage your personal information and signature.")
        );
    m_myInformationHeading->setText(tr("My Information"));
    m_signatureHeading->setText(tr("Signature"));
    m_signatureInstructionsLabel->setText(
        tr("Choose an image file or type your signature.")
        );
    m_signatureImageModeButton->setText(tr("Image"));
    m_signatureTypeModeButton->setText(tr("Type"));
    m_typedSignatureLabel->setText(tr("Type your signature"));
    m_typedSignatureFontLabel->setText(tr("Choose a style"));
    m_typedSignatureEdit->setPlaceholderText(
        tr("Type your name")
        );
    m_nameLabel->setText(tr("My Name"));
    m_campusLabel->setText(tr("My Campus"));
    m_zoomLoginIdLabel->setText(tr("Zoom Login ID"));
    m_zoomPasswordLabel->setText(tr("Zoom Password"));
    m_zoomLabel->setText(tr("Zoom"));
    m_zoomNotAvailableCheck->setText(tr("N/A"));
    updateSignatureControls();
    updateTypedSignatureFontOptions();
    updateSignaturePreview();
}

void PersonalDetailsPage::saveData()
{
    saveMyInfoInternal();
}

bool PersonalDetailsPage::saveChanges()
{
    m_autosave->cancelPendingSave();
    return saveMyInfoInternal();
}

bool PersonalDetailsPage::hasUnsavedChanges() const
{
    return m_autosave->isDirty();
}

void PersonalDetailsPage::discardChanges()
{
    m_autosave->cancelPendingSave();
    loadPageData();
}

void PersonalDetailsPage::setSaveMode(SaveMode mode)
{
    m_autosave->setSaveMode(mode);
}

void PersonalDetailsPage::setPageHeaderVisible(bool visible)
{
    if (m_pageHeader)
    {
        m_pageHeader->setVisible(visible);
    }
}

void PersonalDetailsPage::scrollToTop()
{
    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            if (auto* scrollBar = m_pageBody->verticalScrollBar())
            {
                scrollBar->setValue(scrollBar->minimum());
            }
        }
        );
}

void PersonalDetailsPage::showEvent(QShowEvent* event)
{
    BasePage::showEvent(event);

    if (!m_autosave->isDirty())
    {
        loadPageData();
    }
}
