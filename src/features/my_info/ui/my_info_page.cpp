#include "my_info_page.h"

#include "calendar_event_dialog.h"
#include "calendar_event_model.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"
#include "data/data_service.h"
#include "features/campus/data/campus_json_repository.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/sections/schedule_section_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
constexpr int AutosaveDelayMs = 750;
constexpr int FieldMinimumWidth = 190;
constexpr int FieldMaximumWidth = FieldMinimumWidth * 2;
constexpr int UntitledCardTopMargin = 4;
const QString NotAvailableText =
    QStringLiteral("N/A");

DataService* openDataService(
    ApplicationServices* services
    )
{
    auto* dataService =
        services
            ? services->dataService()
            : nullptr;

    return dataService && dataService->isOpen()
        ? dataService
        : nullptr;
}

CampusJsonRepository campusRepository()
{
    return CampusJsonRepository(
        ResourcePaths::Campuses::directory()
        );
}

QString campusDisplayName(
    const CampusInfo& campus
    )
{
    return campus.campusName.trimmed().isEmpty()
        ? campus.id.trimmed()
        : campus.campusName.trimmed();
}

int findCampusIndex(
    QComboBox* combo,
    const QString& savedCampus
    )
{
    if (!combo || savedCampus.trimmed().isEmpty())
    {
        return -1;
    }

    const QString normalized =
        savedCampus.trimmed();

    for (int index = 0; index < combo->count(); ++index)
    {
        if (
            combo->itemData(index).toString().compare(
                normalized,
                Qt::CaseInsensitive
                ) == 0
            || combo->itemText(index).compare(
                normalized,
                Qt::CaseInsensitive
                ) == 0
            )
        {
            return index;
        }
    }

    return -1;
}

namespace SettingsKeys
{
const QString Campus =
    QStringLiteral("myInfo/campus");
const QString ZoomLoginId =
    QStringLiteral("myInfo/zoomLoginId");
const QString ZoomPassword =
    QStringLiteral("myInfo/zoomPassword");
const QString ZoomNotAvailable =
    QStringLiteral("myInfo/zoomNotAvailable");

const QString LegacyZoomEmail =
    QStringLiteral("subPrep/personalZoomEmail");
const QString LegacyZoomPassword =
    QStringLiteral("subPrep/personalZoomPassword");
const QString LegacyZoomNotAvailable =
    QStringLiteral("subPrep/personalZoomNotAvailable");
}

void applyFieldWidth(
    QWidget* widget
    )
{
    if (!widget)
    {
        return;
    }

    widget->setMinimumWidth(
        FieldMinimumWidth
        );
    widget->setMaximumWidth(
        FieldMaximumWidth
        );
}

QVariant loadSettingWithLegacyFallback(
    DataService* dataService,
    const QString& primaryKey,
    const QString& legacyKey,
    const QVariant& defaultValue
    )
{
    QVariant value =
        dataService->loadSetting(
            primaryKey,
            QVariant()
            );

    if (value.isValid())
    {
        return value;
    }

    value =
        dataService->loadSetting(
            legacyKey,
            QVariant()
            );

    if (value.isValid())
    {
        dataService->saveSetting(
            primaryKey,
            value
            );
        return value;
    }

    return defaultValue;
}

bool settingToBool(
    const QVariant& value,
    bool defaultValue
    )
{
    if (!value.isValid())
    {
        return defaultValue;
    }

    const QString text =
        value.toString().trimmed().toLower();

    if (text == QStringLiteral("true") || text == QStringLiteral("1"))
    {
        return true;
    }

    if (text == QStringLiteral("false") || text == QStringLiteral("0"))
    {
        return false;
    }

    return value.toBool();
}
}

MyInfoPage::MyInfoPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::MyInfo);

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

    loadPageData();
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

    if (m_calendarModel)
    {
        m_calendarModel->reload();
    }

    if (!m_dirty)
    {
        loadPageData();
    }
}

void MyInfoPage::saveData()
{
    saveMyInfoInternal();
}

bool MyInfoPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    return saveMyInfoInternal();
}

bool MyInfoPage::hasUnsavedChanges() const
{
    return m_dirty;
}

void MyInfoPage::discardChanges()
{
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

    case MyInfoSection::MyInformation:
        target = m_myInformationHeading;
        break;

    case MyInfoSection::MonthlyCalendar:
        target = m_monthlyCalendarHeading;
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

QString MyInfoPage::currentSectionName() const
{
    switch (m_currentSection)
    {
    case MyInfoSection::ClassSchedule:
        return tr("Class Schedule");

    case MyInfoSection::MyInformation:
        return tr("My Information");

    case MyInfoSection::MonthlyCalendar:
        return tr("Monthly Calendar");
    }

    return QString();
}

void MyInfoPage::showEvent(
    QShowEvent* event
    )
{
    BasePage::showEvent(event);

    if (!m_dirty)
    {
        loadPageData();
    }

    if (m_scheduleWidget)
    {
        m_scheduleWidget->refreshSchedule();
    }

    if (m_calendarModel)
    {
        m_calendarModel->reload();
    }
}

bool MyInfoPage::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    return BasePage::eventFilter(
        watched,
        event
        );
}

void MyInfoPage::handleEditableChanged()
{
    if (m_loading)
    {
        return;
    }

    m_dirty = true;

    if (
        m_autosaveTimer
        && m_saveMode == SaveMode::Automatic
        )
    {
        m_autosaveTimer->start();
    }
}

void MyInfoPage::handleZoomNotAvailableChanged(
    bool checked
    )
{
    Q_UNUSED(checked);

    setZoomFieldsEnabled();
    handleEditableChanged();
}

void MyInfoPage::autosave()
{
    if (!hasUnsavedChanges())
    {
        return;
    }

    saveMyInfoInternal();
}

void MyInfoPage::handleCalendarDayActivated(
    int year,
    int month,
    int day
    )
{
    CalendarEvent event;

    event.startDate =
        QDate(
            year,
            month,
            day
            );
    event.endDate =
        event.startDate;
    event.startTime =
        QTime(9, 0);
    event.endTime =
        QTime(10, 0);

    openCalendarDialog(
        event,
        false
        );
}

void MyInfoPage::handleCalendarEventActivated(
    int eventId
    )
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService || eventId <= 0)
    {
        return;
    }

    const CalendarEvent event =
        dataService->getCalendarEvent(
            eventId
            );

    if (event.id <= 0)
    {
        return;
    }

    openCalendarDialog(
        event,
        true
        );
}

void MyInfoPage::buildUi()
{
    contentLayout()->setContentsMargins(
        0,
        0,
        0,
        0
        );

    m_scrollArea =
        new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_scrollContent =
        new QWidget(m_scrollArea);
    m_scrollContentLayout =
        new QVBoxLayout(m_scrollContent);
    m_scrollContentLayout->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin
        );
    m_scrollContentLayout->setSpacing(
        UiConstants::Pages::Spacing
        );
    m_scrollContentLayout->setAlignment(Qt::AlignTop);

    auto* headerLayout =
        new QVBoxLayout;

    auto* titleLabel =
        new QLabel(
            tr("My Info"),
            m_scrollContent
            );
    titleLabel->setObjectName("pageTitle");
    titleLabel->setFont(
        FontManager::getUiFont(
            24,
            QFont::Bold
            )
        );

    auto* subtitleLabel =
        new QLabel(
            tr("Manage your schedule, personal details, and monthly events."),
            m_scrollContent
            );
    subtitleLabel->setObjectName("pageSubtitle");
    subtitleLabel->setFont(
        FontManager::getUiFont(11)
        );

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);
    m_scrollContentLayout->addLayout(headerLayout);

    buildMyInformationSection();
    buildClassScheduleSection();
    buildMonthlyCalendarSection();

    m_scrollContentLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);
    contentLayout()->addWidget(m_scrollArea);
}

void MyInfoPage::buildClassScheduleSection()
{
    m_classScheduleHeading =
        createTopLevelHeading(
            tr("Class Schedule"),
            m_scrollContent
            );
    m_scrollContentLayout->addWidget(
        m_classScheduleHeading
        );

    auto* card =
        new QFrame(m_scrollContent);
    card->setProperty(
        "role",
        UiRoles::Card
        );
    card->setObjectName(
        "sectionCard"
        );

    auto* cardLayout =
        new QVBoxLayout(card);
    cardLayout->setAlignment(Qt::AlignTop);
    cardLayout->setContentsMargins(
        UiConstants::ClassInfo::SectionCard::Margin,
        UntitledCardTopMargin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin
        );
    cardLayout->setSpacing(
        UiConstants::ClassInfo::SectionCard::Spacing
        );

    m_scheduleWidget =
        new ScheduleSectionWidget(
            m_services,
            card
            );

    connect(
        m_scheduleWidget,
        &ScheduleSectionWidget::classInfoSaved,
        this,
        &MyInfoPage::classInfoSaved
        );

    cardLayout->addWidget(
        m_scheduleWidget
        );

    m_scrollContentLayout->addWidget(
        card
        );
}

void MyInfoPage::buildMyInformationSection()
{
    m_myInformationHeading =
        createTopLevelHeading(
            tr("My Information"),
            m_scrollContent
            );
    m_scrollContentLayout->addWidget(
        m_myInformationHeading
        );

    auto* card =
        new QFrame(m_scrollContent);
    card->setProperty(
        "role",
        UiRoles::Card
        );
    card->setObjectName(
        "sectionCard"
        );

    auto* cardLayout =
        new QVBoxLayout(card);
    cardLayout->setAlignment(Qt::AlignTop);
    cardLayout->setContentsMargins(
        UiConstants::ClassInfo::SectionCard::Margin,
        UntitledCardTopMargin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin
        );
    cardLayout->setSpacing(
        UiConstants::ClassInfo::SectionCard::Spacing
        );

    auto* grid =
        new QGridLayout;
    grid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    grid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    m_campusCombo =
        new QComboBox(card);

    m_zoomLoginIdEdit =
        new QLineEdit(card);
    m_zoomPasswordEdit =
        new QLineEdit(card);
    m_zoomNotAvailableCheck =
        new QCheckBox(
            tr("N/A"),
            card
            );

    applyFieldWidth(m_campusCombo);
    applyFieldWidth(m_zoomLoginIdEdit);
    applyFieldWidth(m_zoomPasswordEdit);

    m_zoomLoginIdEdit->installEventFilter(this);
    m_zoomPasswordEdit->installEventFilter(this);

    grid->addWidget(
        createFieldLabel(tr("My Campus"), card),
        0,
        0,
        Qt::AlignLeft
        );
    grid->addWidget(
        createFieldLabel(tr("Zoom Login ID"), card),
        0,
        1,
        Qt::AlignLeft
        );
    grid->addWidget(
        createFieldLabel(tr("Zoom Password"), card),
        0,
        2,
        Qt::AlignLeft
        );
    grid->addWidget(
        createFieldLabel(tr("Zoom"), card),
        0,
        3,
        Qt::AlignLeft
        );

    grid->addWidget(
        m_campusCombo,
        1,
        0,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomLoginIdEdit,
        1,
        1,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomPasswordEdit,
        1,
        2,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomNotAvailableCheck,
        1,
        3,
        Qt::AlignLeft | Qt::AlignVCenter
        );
    grid->setColumnStretch(4, 1);

    cardLayout->addLayout(
        grid
        );

    m_scrollContentLayout->addWidget(
        card
        );

    connect(
        m_campusCombo,
        &QComboBox::currentTextChanged,
        this,
        &MyInfoPage::handleEditableChanged
        );
    connect(
        m_zoomLoginIdEdit,
        &QLineEdit::textChanged,
        this,
        &MyInfoPage::handleEditableChanged
        );
    connect(
        m_zoomPasswordEdit,
        &QLineEdit::textChanged,
        this,
        &MyInfoPage::handleEditableChanged
        );
    connect(
        m_zoomNotAvailableCheck,
        &QCheckBox::toggled,
        this,
        &MyInfoPage::handleZoomNotAvailableChanged
        );
}

void MyInfoPage::buildMonthlyCalendarSection()
{
    m_monthlyCalendarHeading =
        createTopLevelHeading(
            tr("Monthly Calendar"),
            m_scrollContent
            );
    m_scrollContentLayout->addWidget(
        m_monthlyCalendarHeading
        );

    auto* card =
        new QFrame(m_scrollContent);
    card->setProperty(
        "role",
        UiRoles::Card
        );
    card->setObjectName(
        "sectionCard"
        );

    auto* cardLayout =
        new QVBoxLayout(card);
    cardLayout->setAlignment(Qt::AlignTop);
    cardLayout->setContentsMargins(
        UiConstants::ClassInfo::SectionCard::Margin,
        UntitledCardTopMargin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin
        );
    cardLayout->setSpacing(
        UiConstants::ClassInfo::SectionCard::Spacing
        );

    m_calendarModel =
        new CalendarEventModel(
            m_services
                ? m_services->dataService()
                : nullptr,
            this
            );

    m_calendarView =
        new QQuickWidget(card);
    m_calendarView->setResizeMode(
        QQuickWidget::SizeRootObjectToView
        );
    m_calendarView->setMinimumHeight(560);
    m_calendarView->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );
    m_calendarView
        ->rootContext()
        ->setContextProperty(
            QStringLiteral("calendarEventProvider"),
            m_calendarModel
            );
    m_calendarView->setSource(
        QUrl(
            QStringLiteral(
                "qrc:/qt/qml/ClassMngr/MyInfo/EventCalendar.qml"
                )
            )
        );

    if (auto* root = m_calendarView->rootObject())
    {
        connect(
            root,
            SIGNAL(dayActivated(int,int,int)),
            this,
            SLOT(handleCalendarDayActivated(int,int,int))
            );
        connect(
            root,
            SIGNAL(eventActivated(int)),
            this,
            SLOT(handleCalendarEventActivated(int))
            );
    }

    cardLayout->addWidget(
        m_calendarView
        );

    m_scrollContentLayout->addWidget(
        card
        );
}

void MyInfoPage::loadPageData()
{
    m_loading = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadStoredSettings();

    m_loading = false;
    clearDirty();
}

void MyInfoPage::loadStoredSettings()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    const QSignalBlocker campusBlocker(m_campusCombo);
    const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
    const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);
    const QSignalBlocker checkBlocker(m_zoomNotAvailableCheck);

    const QString campus =
        dataService
            ->loadSetting(
                SettingsKeys::Campus,
                QString()
                )
            .toString();

    m_campusCombo->clear();

    const QList<CampusInfo> campuses =
        campusRepository().loadCampuses();

    for (const CampusInfo& campusInfo : campuses)
    {
        const QString displayName =
            campusDisplayName(campusInfo);

        if (displayName.isEmpty())
        {
            continue;
        }

        m_campusCombo->addItem(
            displayName,
            campusInfo.id
            );
    }

    const int campusIndex =
        findCampusIndex(
            m_campusCombo,
            campus
            );

    m_campusCombo->setCurrentIndex(
        campusIndex >= 0
            ? campusIndex
            : 0
        );

    if (
        m_campusCombo->currentIndex() >= 0
        && m_campusCombo->currentText().compare(
            campus.trimmed(),
            Qt::CaseInsensitive
            ) != 0
        )
    {
        dataService->saveSetting(
            SettingsKeys::Campus,
            m_campusCombo->currentText()
            );
    }

    const QString loginId =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::ZoomLoginId,
            SettingsKeys::LegacyZoomEmail,
            NotAvailableText
            )
            .toString();
    const QString password =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::ZoomPassword,
            SettingsKeys::LegacyZoomPassword,
            NotAvailableText
            )
            .toString();

    m_zoomLoginIdEdit->setText(
        loginId.trimmed().isEmpty()
            ? NotAvailableText
            : loginId
        );
    m_zoomPasswordEdit->setText(
        password.trimmed().isEmpty()
            ? NotAvailableText
            : password
        );
    m_zoomNotAvailableCheck->setChecked(
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::ZoomNotAvailable,
            SettingsKeys::LegacyZoomNotAvailable,
            true
            )
            .toBool()
        );

    setZoomFieldsEnabled();
}

bool MyInfoPage::saveMyInfoInternal()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return false;
    }

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    if (
        !m_zoomNotAvailableCheck
        || !m_zoomNotAvailableCheck->isChecked()
        )
    {
        normalizeZoomFields();
    }

    dataService->saveSetting(
        SettingsKeys::Campus,
        m_campusCombo->currentText()
        );
    dataService->saveSetting(
        SettingsKeys::ZoomLoginId,
        m_zoomLoginIdEdit->text()
        );
    dataService->saveSetting(
        SettingsKeys::ZoomPassword,
        m_zoomPasswordEdit->text()
        );
    dataService->saveSetting(
        SettingsKeys::ZoomNotAvailable,
        m_zoomNotAvailableCheck->isChecked()
        );

    clearDirty();
    return true;
}

bool MyInfoPage::normalizeZoomFields()
{
    bool changed = false;

    changed =
        normalizeLineEdit(
            m_zoomLoginIdEdit,
            NotAvailableText
            )
        || changed;
    changed =
        normalizeLineEdit(
            m_zoomPasswordEdit,
            NotAvailableText
            )
        || changed;

    return changed;
}

bool MyInfoPage::normalizeLineEdit(
    QLineEdit* edit,
    const QString& defaultText
    )
{
    if (
        !edit
        || !edit->text().trimmed().isEmpty()
        )
    {
        return false;
    }

    const QSignalBlocker blocker(edit);

    edit->setText(
        defaultText
        );

    return true;
}

void MyInfoPage::setZoomFieldsEnabled()
{
    const bool fieldsEnabled =
        !m_zoomNotAvailableCheck
        || !m_zoomNotAvailableCheck->isChecked();

    if (m_zoomLoginIdEdit)
    {
        m_zoomLoginIdEdit->setEnabled(
            fieldsEnabled
            );
    }

    if (m_zoomPasswordEdit)
    {
        m_zoomPasswordEdit->setEnabled(
            fieldsEnabled
            );
    }
}

void MyInfoPage::clearDirty()
{
    m_dirty = false;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }
}

void MyInfoPage::openCalendarDialog(
    const CalendarEvent& event,
    bool existingEvent
    )
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    CalendarEventDialog dialog(
        event,
        existingEvent,
        settingToBool(
            dataService->loadSetting(
                QStringLiteral("schedule_use_24h"),
                QStringLiteral("false")
                ),
            false
            ),
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    if (dialog.deleteRequested())
    {
        dataService->deleteCalendarEvent(
            event.id
            );
    }
    else
    {
        dataService->saveCalendarEvent(
            dialog.eventData()
            );
    }

    if (m_calendarModel)
    {
        m_calendarModel->reload();
    }
}

QLabel* MyInfoPage::createTopLevelHeading(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(
            text,
            parent
            );

    label->setObjectName("sectionTitle");
    label->setAlignment(Qt::AlignCenter);
    label->setFont(
        FontManager::getUiFont(
            20,
            QFont::DemiBold
            )
        );

    return label;
}

QLabel* MyInfoPage::createFieldLabel(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(
            text,
            parent
            );

    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    return label;
}
