#include "my_info_page.h"

#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/pages/basepage.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sections/schedule_section_widget.h"

#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>

namespace
{
constexpr int AutosaveDelayMs = 750;
}

MyInfoPage::MyInfoPage(
    ApplicationServices* services,
    MyInfoPageMode mode,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_mode(mode)
{
    setProperty("role", UiRoles::MyInfo);

    switch (m_mode)
    {
    case MyInfoPageMode::Information:
        m_currentSection =
            MyInfoSection::MyInformation;
        break;

    case MyInfoPageMode::Schedule:
        m_currentSection =
            MyInfoSection::ClassSchedule;
        break;

    case MyInfoPageMode::ClassInformation:
        m_currentSection =
            MyInfoSection::ClassInformation;
        break;
    }

    buildUi();

    m_autosaveTimer =
        new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(
        AutosaveDelayMs
        );

    connect(
        m_autosaveTimer,
        &QTimer::timeout,
        this,
        &MyInfoPage::autosave
        );

    if (includesMyInformation())
    {
        loadPageData();
    }
    else
    {
        refreshGeneratedContent();
    }
}
void MyInfoPage::refresh()
{
    BasePage::refresh();

    if (!isVisible())
    {
        return;
    }

    if (m_scheduleWidget)
    {
        m_scheduleWidget->refreshSchedule();
    }

    if (!m_dirty && includesMyInformation())
    {
        loadPageData();
    }

    refreshGeneratedContent();
}
void MyInfoPage::retranslateUi()
{
    if (m_titleLabel)
    {
        m_titleLabel->setText(
            pageTitle()
            );
    }

    if (m_subtitleLabel)
    {
        m_subtitleLabel->setText(
            pageSubtitle()
            );
    }

    if (m_myInformationHeading)
    {
        m_myInformationHeading->setText(
            tr("My Information")
            );
    }

    if (m_signatureHeading)
    {
        m_signatureHeading->setText(
            tr("Signature")
            );
    }

    if (m_signatureInstructionsLabel)
    {
        m_signatureInstructionsLabel->setText(
            tr("Add a PNG or JPEG signature image. Other supported image formats are converted to PNG.")
            );
    }

    if (m_chooseSignatureButton)
    {
        m_chooseSignatureButton->setText(
            m_signatureImageData.isEmpty()
                ? tr("Add Signature Image...")
                : tr("Replace Signature Image...")
            );
    }

    if (m_removeSignatureButton)
    {
        m_removeSignatureButton->setText(
            tr("Remove")
            );
    }

    updateSignaturePreview();

    if (m_classScheduleHeading)
    {
        m_classScheduleHeading->setText(
            tr("Schedule")
            );
    }

    if (m_classInformationHeading)
    {
        m_classInformationHeading->setText(
            tr("Class Information")
            );
    }

    if (m_nameLabel)
    {
        m_nameLabel->setText(
            tr("My Name")
            );
    }

    if (m_campusLabel)
    {
        m_campusLabel->setText(
            tr("My Campus")
            );
    }

    if (m_zoomLoginIdLabel)
    {
        m_zoomLoginIdLabel->setText(
            tr("Zoom Login ID")
            );
    }

    if (m_zoomPasswordLabel)
    {
        m_zoomPasswordLabel->setText(
            tr("Zoom Password")
            );
    }

    if (m_zoomLabel)
    {
        m_zoomLabel->setText(
            tr("Zoom")
            );
    }

    if (m_zoomNotAvailableCheck)
    {
        m_zoomNotAvailableCheck->setText(
            tr("N/A")
            );
    }

    if (m_scheduleWidget)
    {
        m_scheduleWidget->retranslateUi();
    }

    refreshGeneratedContent();
}
void MyInfoPage::saveData()
{
    if (!includesMyInformation())
    {
        return;
    }

    saveMyInfoInternal();
}
bool MyInfoPage::saveChanges()
{
    if (!includesMyInformation())
    {
        return true;
    }

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    return saveMyInfoInternal();
}
bool MyInfoPage::hasUnsavedChanges() const
{
    return includesMyInformation() && m_dirty;
}
void MyInfoPage::discardChanges()
{
    if (!includesMyInformation())
    {
        return;
    }

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadPageData();
}
void MyInfoPage::setSaveMode(
    SaveMode mode
    )
{
    m_saveMode = mode;

    if (!includesMyInformation())
    {
        return;
    }

    if (!m_autosaveTimer)
    {
        return;
    }

    if (m_saveMode == SaveMode::Automatic && hasUnsavedChanges())
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}
void MyInfoPage::scrollToSection(
    MyInfoSection section
    )
{
    m_currentSection =
        section;

    QWidget* target = nullptr;

    switch (section)
    {
    case MyInfoSection::ClassSchedule:
        target = m_classScheduleHeading;
        break;

    case MyInfoSection::ClassInformation:
        target = m_classInformationHeading;
        break;

    case MyInfoSection::MyInformation:
        target = m_myInformationHeading;
        break;

    }

    if (!target || !m_scrollArea)
    {
        return;
    }

    QTimer::singleShot(
        0,
        this,
        [this, target]()
        {
            if (!m_scrollArea || !target)
            {
                return;
            }

            m_scrollArea->ensureWidgetVisible(
                target,
                0,
                0
                );

            if (auto* scrollBar = m_scrollArea->verticalScrollBar())
            {
                scrollBar->setValue(
                    target->y()
                    );
            }
        }
        );
}
void MyInfoPage::scrollToTop()
{
    m_currentSection =
        MyInfoSection::MyInformation;

    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            if (auto* scrollBar =
                    m_scrollArea
                        ? m_scrollArea->verticalScrollBar()
                        : nullptr)
            {
                scrollBar->setValue(
                    scrollBar->minimum()
                    );
            }
        }
        );
}
QString MyInfoPage::currentSectionName() const
{
    switch (m_currentSection)
    {
    case MyInfoSection::ClassSchedule:
        return tr("Schedule");

    case MyInfoSection::ClassInformation:
        return tr("Class Information");

    case MyInfoSection::MyInformation:
        return tr("My Information");

    }

    return QString();
}
QString MyInfoPage::currentSectionKey() const
{
    switch (m_currentSection)
    {
    case MyInfoSection::ClassSchedule:
        return QStringLiteral("my_info_schedule");

    case MyInfoSection::ClassInformation:
        return QStringLiteral("my_info_class_information");

    case MyInfoSection::MyInformation:
        return QStringLiteral("my_info_information");

    }

    return QString();
}
void MyInfoPage::showEvent(
    QShowEvent* event
    )
{
    BasePage::showEvent(event);

    if (!m_dirty && includesMyInformation())
    {
        loadPageData();
    }

    refreshGeneratedContent();

    if (m_scheduleWidget)
    {
        m_scheduleWidget->refreshSchedule();
    }

}
