#include "personal_details_page.h"

#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSizePolicy>
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

    m_saveStatusLabel = new QLabel(this);
    m_saveStatusLabel->setObjectName(
        QStringLiteral("personalDetailsSaveStatus")
        );
    m_saveStatusLabel->setAccessibleName(tr("Save status"));
    m_saveStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_saveStatusLabel->setWordWrap(true);
    m_saveStatusLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );
    m_saveStatusLabel->hide();
    bottomLayout()->addWidget(m_saveStatusLabel, 1);
    setBottomBarVisible(false);

    connect(
        m_autosave,
        &AutosaveCoordinator::saveRequested,
        this,
        [this](bool interactive)
        {
            if (!saveMyInfoInternal() && !interactive)
            {
                showNoninteractiveSaveFailureStatus();
            }
        }
        );
}

void PersonalDetailsPage::refresh()
{
    BasePage::refresh();

    if (!m_autosave->isDirty())
    {
        loadPageData();
    }
}

void PersonalDetailsPage::clearDatabaseState()
{
    m_autosave->setLoading(true);
    clearSaveFailureStatus();
    m_storedNameKnown = false;
    m_currentCampusKnown = false;
    m_signatureImageKnown = false;
    m_signaturePreferencesKnown = false;
    m_zoomPreferencesKnown = false;
    m_failedCampusCorrectionStoredValue.clear();
    m_failedCampusCorrectionSelectedCampus.clear();
    m_hasFailedCampusCorrection = false;

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
    if (m_saveFailureStatusActive)
    {
        showNoninteractiveSaveFailureStatus();
    }
    updateSignatureControls();
    updateTypedSignatureFontOptions();
    updateSignaturePreview();
}

void PersonalDetailsPage::saveData()
{
    if (!saveMyInfoInternal())
    {
        showNoninteractiveSaveFailureStatus();
    }
}

bool PersonalDetailsPage::saveChanges()
{
    m_autosave->cancelPendingSave();

    QString errorMessage;
    if (saveMyInfoInternal(&errorMessage))
    {
        return true;
    }

    DialogServices::showWarning(
        this,
        tr("My Details Could Not Be Saved"),
        tr("Your personal details could not be saved. Your changes are still here."),
        errorMessage
        );
    return false;
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

void PersonalDetailsPage::showNoninteractiveSaveFailureStatus()
{
    m_saveFailureStatusActive = true;
    if (m_saveStatusLabel)
    {
        m_saveStatusLabel->setText(
            tr("Your personal details could not be saved. Your changes are still here.")
            );
        m_saveStatusLabel->show();
    }
    setBottomBarVisible(true);
}

void PersonalDetailsPage::clearSaveFailureStatus()
{
    m_saveFailureStatusActive = false;
    if (m_saveStatusLabel)
    {
        m_saveStatusLabel->clear();
        m_saveStatusLabel->hide();
    }
    setBottomBarVisible(false);
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
