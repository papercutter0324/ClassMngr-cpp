#include "my_info_page.h"

#include "academic_calendar_dialog.h"
#include "academic_calendar_provider.h"
#include "calendar_event_dialog.h"
#include "calendar_event_model.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/my_info/calendar_event_campus_filter.h"
#include "features/my_info/calendar_settings_keys.h"
#include "features/campus/data/campus_json_repository.h"
#include "features/sub_prep/ui/sub_prep_class_navigation.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/sections/schedule_section_widget.h"
#include "ui/shared/widgets/uniform_width_tab_bar.h"

#include <algorithm>
#include <utility>

#include <QCheckBox>
#include <QBasicTimer>
#include <QColorDialog>
#include <QComboBox>
#include "ui/shared/widgets/no_wheel_combobox.h"
#include <QDialog>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QTimerEvent>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <QVariantMap>

namespace
{
constexpr int AutosaveDelayMs = 750;
constexpr int UntitledCardTopMargin = 4;
constexpr int CompactFieldWidth = 170;
constexpr int MyInformationFieldVerticalPadding = 14;
constexpr int TextEditVerticalPadding = 24;
constexpr int ClassTabContentTopMargin = 16;
constexpr int UpcomingEventsNext30Days = 30;
constexpr int UpcomingEventsLimit = 10;
constexpr int UpcomingEventColumnSpacing = 16;
constexpr int UpcomingEventDateColumnMinimumWidth = 72;
constexpr int UpcomingEventTimeColumnMinimumWidth = 104;
constexpr int UpcomingEventTypeColumnMinimumWidth = 82;
constexpr int UpcomingEventColumnTextPadding = 8;
constexpr int UpcomingEventTagMinimumHeight = 28;
constexpr int UpcomingEventRowMinimumHeight = 38;
const QString NotAvailableText =
    QStringLiteral("N/A");

int upcomingEventTagVerticalPadding(
    const QFont& font
    )
{
    return qMax(
        6,
        QFontMetrics(font).height() / 3
        );
}

int upcomingEventTagHorizontalPadding(
    const QFont& font
    )
{
    return qMax(
        8,
        QFontMetrics(font).height() / 2
        );
}

int upcomingEventTagHeight(
    const QFont& font
    )
{
    const QFontMetrics metrics(font);

    return qMax(
        UpcomingEventTagMinimumHeight,
        metrics.lineSpacing()
            + (upcomingEventTagVerticalPadding(font) * 2)
            + 4
        );
}

int upcomingEventTagWidth(
    const QString& text,
    const QFont& font
    )
{
    const QFontMetrics metrics(font);

    return metrics.horizontalAdvance(text)
        + (upcomingEventTagHorizontalPadding(font) * 2)
        + 2;
}

int upcomingEventRowHeight(
    const QFont& font
    )
{
    return qMax(
        UpcomingEventRowMinimumHeight,
        upcomingEventTagHeight(font) + 10
        );
}

struct ClassSummary
{
    Classroom classroom;
    ClassInfo info;
    Teacher teacher;
    QString displayName;
    int studentCount = 0;
};

class HoverMarqueeLabel : public QLabel
{
public:
    explicit HoverMarqueeLabel(
        QWidget* parent = nullptr
        )
        : QLabel(parent)
    {
        setMouseTracking(true);
    }

    QSize minimumSizeHint() const override
    {
        return QSize(
            0,
            QLabel::minimumSizeHint().height()
            );
    }

    QSize sizeHint() const override
    {
        return QSize(
            160,
            QLabel::sizeHint().height()
            );
    }

    void setMarqueeActive(bool active)
    {
        if (m_hovered == active)
        {
            return;
        }

        m_hovered = active;

        if (!m_hovered)
        {
            m_offset = 0;
            m_timer.stop();
            update();
            return;
        }

        updateMarqueeState();
    }

protected:
    void enterEvent(
        QEnterEvent* event
        ) override
    {
        QLabel::enterEvent(event);
        setMarqueeActive(true);
    }

    void leaveEvent(
        QEvent* event
        ) override
    {
        QLabel::leaveEvent(event);
        setMarqueeActive(false);
    }

    void resizeEvent(
        QResizeEvent* event
        ) override
    {
        QLabel::resizeEvent(event);
        updateMarqueeState();
    }

    void timerEvent(
        QTimerEvent* event
        ) override
    {
        if (event->timerId() != m_timer.timerId())
        {
            QLabel::timerEvent(event);
            return;
        }

        const int textWidth =
            fontMetrics().horizontalAdvance(text());
        const int gap = 32;

        m_offset =
            (m_offset + 1) % qMax(1, textWidth + gap);
        update();
    }

    void paintEvent(
        QPaintEvent* event
        ) override
    {
        const int textWidth =
            fontMetrics().horizontalAdvance(text());

        if (!m_hovered || textWidth <= contentsRect().width())
        {
            QLabel::paintEvent(event);
            return;
        }

        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(palette().color(QPalette::WindowText));
        painter.setClipRect(contentsRect());

        const QRect rect =
            contentsRect();
        const int baseline =
            rect.y()
            + (rect.height() + fontMetrics().ascent() - fontMetrics().descent()) / 2;
        const int gap = 32;
        const int x =
            rect.x() - m_offset;

        painter.drawText(
            x,
            baseline,
            text()
            );
        painter.drawText(
            x + textWidth + gap,
            baseline,
            text()
            );
    }

private:
    void updateMarqueeState()
    {
        const bool shouldRun =
            m_hovered
            && fontMetrics().horizontalAdvance(text()) > contentsRect().width();

        if (shouldRun && !m_timer.isActive())
        {
            m_timer.start(35, this);
        }
        else if (!shouldRun && m_timer.isActive())
        {
            m_timer.stop();
            m_offset = 0;
        }
    }

    QBasicTimer m_timer;
    bool m_hovered = false;
    int m_offset = 0;
};

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

int scopeIndex(
    UpcomingEventsScope scope
    )
{
    return static_cast<int>(scope);
}

QColor defaultCalendarEventTypeColor(
    const QString& eventType
    )
{
    const QString normalized =
        normalizedCalendarEventType(
            eventType
            );

    QColor color =
        QColor(QStringLiteral("#66727a"));

    if (normalized == QStringLiteral("Vacation"))
    {
        color = QColor(QStringLiteral("#4b6f91"));
    }
    else if (normalized == QStringLiteral("Holiday"))
    {
        color = QColor(QStringLiteral("#7a5f9e"));
    }
    else if (normalized == QStringLiteral("Workshop"))
    {
        color = QColor(QStringLiteral("#5f7f52"));
    }
    else if (normalized == QStringLiteral("CM"))
    {
        color = QColor(QStringLiteral("#9a6b3f"));
    }
    else if (normalized == QStringLiteral("Meeting"))
    {
        color = QColor(QStringLiteral("#8a4f5d"));
    }

    return color;
}

QString calendarEventTypeColorSettingKey(
    const QString& eventType
    )
{
    return QStringLiteral("calendar/eventTypeColor/%1").arg(
        normalizedCalendarEventType(eventType)
        );
}

QString readableTextColor(
    const QColor& color
    )
{
    const int brightness =
        (color.red() * 299
         + color.green() * 587
         + color.blue() * 114) / 1000;

    return brightness > 145
        ? QStringLiteral("#27313a")
        : QStringLiteral("#ffffff");
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

int readableFieldHeight(
    const QWidget* field
    )
{
    if (!field)
    {
        return 0;
    }

    const QFontMetrics metrics(field->font());

    return metrics.lineSpacing()
        + qMax(
            MyInformationFieldVerticalPadding,
            (metrics.descent() * 2) + 8
            );
}

void matchFieldHeights(
    QWidget* first,
    QWidget* second,
    QWidget* third,
    QWidget* fourth
    )
{
    int fieldHeight = 0;

    const auto includeField =
        [&fieldHeight](QWidget* field)
        {
            if (!field)
            {
                return;
            }

            fieldHeight =
                qMax(
                    fieldHeight,
                    qMax(
                        qMax(
                            field->sizeHint().height(),
                            field->minimumSizeHint().height()
                            ),
                        readableFieldHeight(field)
                        )
                    );
        };

    includeField(first);
    includeField(second);
    includeField(third);
    includeField(fourth);

    const auto applyHeight =
        [fieldHeight](QWidget* field)
        {
            if (!field || fieldHeight <= 0)
            {
                return;
            }

            field->setMinimumHeight(fieldHeight);
            field->updateGeometry();
        };

    applyHeight(first);
    applyHeight(second);
    applyHeight(third);
    applyHeight(fourth);
}

QString valueOrNa(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        ? NotAvailableText
        : trimmed;
}

QString gradeLevelText(
    const ClassInfo& info
    )
{
    const QString grade =
        info.classGrade.trimmed();
    const QString level =
        info.classLevel.trimmed();

    if (!grade.isEmpty() && !level.isEmpty())
    {
        return QStringLiteral("%1 - %2")
            .arg(grade, level);
    }

    if (!grade.isEmpty())
    {
        return grade;
    }

    if (!level.isEmpty())
    {
        return level;
    }

    return NotAvailableText;
}

QString classTitleText(
    const Classroom& classroom,
    const ClassInfo& info
    )
{
    const QString gradeLevel =
        gradeLevelText(info);

    if (gradeLevel != NotAvailableText)
    {
        return gradeLevel;
    }

    if (!classroom.name.trimmed().isEmpty())
    {
        return classroom.name.trimmed();
    }

    return QStringLiteral("Class %1")
        .arg(classroom.id);
}

QString dayAbbreviation(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QStringLiteral("Mon");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QStringLiteral("Tues");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QStringLiteral("Wed");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QStringLiteral("Thurs");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QStringLiteral("Fri");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QStringLiteral("Sat");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QStringLiteral("Sun");
    }

    return day.trimmed();
}

QString formatSchedule(
    const QList<ClassTime>& times
    )
{
    if (times.isEmpty())
    {
        return NotAvailableText;
    }

    struct TimeGroup
    {
        QString startTime;
        QString endTime;
        QStringList days;
    };

    QList<TimeGroup> groups;

    for (const ClassTime& time : times)
    {
        const QString start =
            time.startTime.trimmed();
        const QString end =
            time.endTime.trimmed();

        auto group =
            std::find_if(
                groups.begin(),
                groups.end(),
                [&start, &end](const TimeGroup& candidate)
                {
                    return candidate.startTime == start
                        && candidate.endTime == end;
                }
                );

        if (group == groups.end())
        {
            TimeGroup newGroup;
            newGroup.startTime = start;
            newGroup.endTime = end;
            newGroup.days.append(
                dayAbbreviation(time.day)
                );

            groups.append(newGroup);
        }
        else
        {
            group->days.append(
                dayAbbreviation(time.day)
                );
        }
    }

    QStringList labels;

    for (const TimeGroup& group : groups)
    {
        const QString days =
            group.days.join(QStringLiteral("/"));

        if (group.startTime.isEmpty() && group.endTime.isEmpty())
        {
            labels.append(
                days.isEmpty()
                    ? NotAvailableText
                    : days
                );
            continue;
        }

        labels.append(
            QStringLiteral("%1 %2-%3")
                .arg(
                    days.isEmpty()
                        ? NotAvailableText
                        : days,
                    valueOrNa(group.startTime),
                    valueOrNa(group.endTime)
                    )
            );
    }

    return labels.isEmpty()
        ? NotAvailableText
        : labels.join(QStringLiteral("; "));
}

QString teacherDisplayName(
    const Teacher& teacher
    )
{
    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(
            teacher
            )
            .trimmed();

    return displayName.isEmpty()
        ? QObject::tr("Unassigned")
        : displayName;
}

QString teacherHeadingText(
    const Teacher& teacher,
    bool unassigned
    )
{
    if (unassigned)
    {
        return QObject::tr("Unassigned");
    }

    const QString name =
        teacherDisplayName(teacher);

    const QString room =
        teacher.roomNumber.trimmed();

    if (room.isEmpty())
    {
        return name;
    }

    return QStringLiteral("%1 - Room %2")
        .arg(name, room);
}

int textEditHeightForLines(
    const QTextEdit* edit,
    int lines
    )
{
    if (!edit)
    {
        return 0;
    }

    return edit->fontMetrics().lineSpacing() * lines
        + TextEditVerticalPadding;
}

void clearLayout(
    QLayout* layout
    )
{
    if (!layout)
    {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (auto* childLayout = item->layout())
        {
            clearLayout(childLayout);
            delete childLayout;
        }

        if (auto* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }
}

QLabel* createValueLabel(
    const QString& value,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(
            valueOrNa(value),
            parent
            );

    label->setTextInteractionFlags(
        Qt::TextSelectableByMouse
        | Qt::TextSelectableByKeyboard
        );
    label->setWordWrap(true);
    label->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    return label;
}

QLineEdit* createReadOnlyValueEdit(
    const QString& value,
    QWidget* parent
    )
{
    auto* edit =
        new QLineEdit(
            valueOrNa(value),
            parent
            );

    edit->setReadOnly(true);
    edit->setCursorPosition(0);
    WidgetSizing::installTextAwareFieldWidth(
        edit,
        CompactFieldWidth
        );

    return edit;
}

void addInfoRow(
    QGridLayout* grid,
    int row,
    const QString& labelText,
    const QString& value,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(labelText, parent);

    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    auto* valueLabel =
        createValueLabel(
            value,
            parent
            );

    grid->addWidget(
        label,
        row,
        0,
        Qt::AlignLeft | Qt::AlignTop
        );
    grid->addWidget(
        valueLabel,
        row,
        1,
        Qt::AlignLeft | Qt::AlignTop
        );
}

void addHorizontalInfoField(
    QGridLayout* grid,
    int labelRow,
    int valueRow,
    int column,
    const QString& labelText,
    const QString& value,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(labelText, parent);

    label->setContentsMargins(
        0,
        0,
        0,
        0
        );

    grid->addWidget(
        label,
        labelRow,
        column,
        Qt::AlignLeft
        );
    auto* valueEdit =
        createReadOnlyValueEdit(value, parent);

    grid->addWidget(
        valueEdit,
        valueRow,
        column
        );
}

namespace SettingsKeys
{
const QString Name =
    QStringLiteral("myInfo/name");
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
    case MyInfoPageMode::InformationCalendar:
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

    if (m_calendarModel)
    {
        m_calendarModel->reload();
    }

    if (m_academicCalendarProvider)
    {
        m_academicCalendarProvider->reload();
    }

    refreshUpcomingEvents();

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

    if (m_monthlyCalendarHeading)
    {
        m_monthlyCalendarHeading->setText(
            tr("Monthly Calendar")
            );
    }

    if (m_upcomingEventsHeading)
    {
        m_upcomingEventsHeading->setText(
            tr("Upcoming Events")
            );
    }

    if (m_upcomingEventsTabs && m_upcomingEventsTabs->count() >= 3)
    {
        m_upcomingEventsTabs->setTabText(
            0,
            tr("Current Month")
            );
        m_upcomingEventsTabs->setTabText(
            1,
            tr("Next 30 Days")
            );
        m_upcomingEventsTabs->setTabText(
            2,
            tr("Next 10 Events")
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

    if (m_calendarView && m_calendarView->engine())
    {
        m_calendarView->engine()->retranslate();
    }

    if (m_academicCalendarProvider)
    {
        m_academicCalendarProvider->reload();
    }

    refreshUpcomingEvents();

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

    case MyInfoSection::MonthlyCalendar:
        return tr("Monthly Calendar");
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

    case MyInfoSection::MonthlyCalendar:
        return QStringLiteral("my_info_calendar");
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

    if (m_calendarModel)
    {
        m_calendarModel->reload();
    }

    if (m_academicCalendarProvider)
    {
        m_academicCalendarProvider->reload();
    }
}

bool MyInfoPage::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        event
        && (
            watched == m_nameEdit
            || watched == m_campusCombo
            || watched == m_zoomLoginIdEdit
            || watched == m_zoomPasswordEdit
            )
        && (
            event->type() == QEvent::FontChange
            || event->type() == QEvent::ApplicationFontChange
            || event->type() == QEvent::Polish
            || event->type() == QEvent::Show
            || event->type() == QEvent::StyleChange
            )
        )
    {
        QTimer::singleShot(
            0,
            this,
            [this]()
            {
                updateMyInformationFieldWidths();
            }
            );
    }

    if (
        event
        && watched == m_calendarView
        && (
            event->type() == QEvent::FontChange
            || event->type() == QEvent::ApplicationFontChange
            )
        )
    {
        QTimer::singleShot(
            0,
            this,
            [this]()
            {
                syncCalendarFontSize();
                refreshUpcomingEvents();
            }
            );
    }

    if (
        event
        && watched
        && (
            event->type() == QEvent::Enter
            || event->type() == QEvent::Leave
            )
        )
    {
        auto* title =
            dynamic_cast<HoverMarqueeLabel*>(watched);

        if (!title)
        {
            title =
                dynamic_cast<HoverMarqueeLabel*>(
                    watched
                        ->property("calendarEventTitleLabel")
                        .value<QObject*>()
                    );
        }

        if (title)
        {
            title->setMarqueeActive(
                event->type() == QEvent::Enter
                );
        }
    }

    if (
        event
        && event->type() == QEvent::MouseButtonRelease
        && watched
        )
    {
        const int eventId =
            watched
                ->property("calendarEventId")
                .toInt();

        if (eventId > 0)
        {
            handleCalendarEventActivated(eventId);
            return true;
        }
    }

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

    updateCalendarCampusFilter();

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

void MyInfoPage::handleCalendarConfigureRequested(
    int year,
    int month
    )
{
    if (!m_academicCalendarProvider)
    {
        return;
    }

    const QDate firstOfMonth(year, month, 1);
    if (!firstOfMonth.isValid())
    {
        return;
    }

    const QDate firstMonday =
        firstOfMonth.addDays(
            (Qt::Monday - firstOfMonth.dayOfWeek() + 7) % 7
            );
    const int termYear =
        m_academicCalendarProvider->termYearForDate(firstMonday);

    AcademicCalendarDialog dialog(
        m_academicCalendarProvider,
        openDataService(m_services),
        termYear,
        this
        );

    connect(
        &dialog,
        &AcademicCalendarDialog::calendarEventsImported,
        this,
        [this]()
        {
            if (m_calendarModel)
            {
                m_calendarModel->reload();
            }

            refreshUpcomingEvents();
        }
        );

    if (dialog.exec() == QDialog::Accepted)
    {
        updateCalendarCampusFilter();
    }
}

void MyInfoPage::handleCalendarDisplayedMonthChanged(
    int year,
    int month
    )
{
    const QDate firstOfMonth(
        year,
        month,
        1
        );

    if (!firstOfMonth.isValid())
    {
        return;
    }

    m_calendarVisibleMonth =
        firstOfMonth;

    refreshUpcomingEvents();
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
    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );
    headerLayout->setSpacing(
        UiConstants::Pages::HeaderSpacing
        );

    m_titleLabel =
        new QLabel(
            pageTitle(),
            m_scrollContent
            );
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );

    m_subtitleLabel =
        new QLabel(
            pageSubtitle(),
            m_scrollContent
            );
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    m_scrollContentLayout->addLayout(headerLayout);
    m_scrollContentLayout->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    if (includesMyInformation())
    {
        buildMyInformationSection();
    }

    if (includesClassSchedule())
    {
        buildClassScheduleSection();
    }

    if (includesClassInformation())
    {
        buildClassInformationSection();
    }

    if (includesMonthlyCalendar())
    {
        buildMonthlyCalendarSection();
    }

    m_scrollContentLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);
    contentLayout()->addWidget(m_scrollArea);
}

QString MyInfoPage::pageTitle() const
{
    switch (m_mode)
    {
    case MyInfoPageMode::InformationCalendar:
        return tr("My Information & Calendar");

    case MyInfoPageMode::Schedule:
        return tr("Schedule");

    case MyInfoPageMode::ClassInformation:
        return tr("Class Information");
    }

    return QString();
}

QString MyInfoPage::pageSubtitle() const
{
    switch (m_mode)
    {
    case MyInfoPageMode::InformationCalendar:
        return tr("Manage your personal details and monthly events.");

    case MyInfoPageMode::Schedule:
        return tr("View and adjust your class schedule.");

    case MyInfoPageMode::ClassInformation:
        return tr("Review teacher, class, and roster details.");
    }

    return QString();
}

bool MyInfoPage::includesMyInformation() const
{
    return m_mode == MyInfoPageMode::InformationCalendar;
}

bool MyInfoPage::includesClassSchedule() const
{
    return m_mode == MyInfoPageMode::Schedule;
}

bool MyInfoPage::includesClassInformation() const
{
    return m_mode == MyInfoPageMode::ClassInformation;
}

bool MyInfoPage::includesMonthlyCalendar() const
{
    return m_mode == MyInfoPageMode::InformationCalendar;
}

void MyInfoPage::buildClassScheduleSection()
{
    if (m_mode != MyInfoPageMode::Schedule)
    {
        m_scrollContentLayout->addSpacing(
            UiConstants::Pages::MajorSectionSpacing
            );

        m_classScheduleHeading =
            createTopLevelHeading(
                tr("Schedule"),
                m_scrollContent
                );
        m_scrollContentLayout->addWidget(
            m_classScheduleHeading
            );
    }

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

void MyInfoPage::buildClassInformationSection()
{
    if (m_mode != MyInfoPageMode::ClassInformation)
    {
        m_scrollContentLayout->addSpacing(
            UiConstants::Pages::MajorSectionSpacing
            );

        m_classInformationHeading =
            createTopLevelHeading(
                tr("Class Information"),
                m_scrollContent
                );
        m_scrollContentLayout->addWidget(
            m_classInformationHeading
            );
    }

    m_classInformationContent =
        new QWidget(m_scrollContent);
    m_classInformationLayout =
        new QVBoxLayout(m_classInformationContent);
    m_classInformationLayout->setContentsMargins(0, 0, 0, 0);
    m_classInformationLayout->setSpacing(
        UiConstants::ClassInfo::Page::ContentSpacing
        );
    m_classInformationLayout->setAlignment(Qt::AlignTop);
    m_scrollContentLayout->addWidget(
        m_classInformationContent
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

    m_nameEdit =
        new QLineEdit(card);
    m_campusCombo =
        new NoWheelComboBox(card);

    m_zoomLoginIdEdit =
        new QLineEdit(card);
    m_zoomPasswordEdit =
        new QLineEdit(card);
    m_zoomNotAvailableCheck =
        new QCheckBox(
            tr("N/A"),
            card
            );
    m_zoomNotAvailableCheck->setObjectName(
        "zoomNotAvailableCheck"
        );

    WidgetSizing::installTextAwareFieldWidth(
        m_nameEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_campusCombo,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_zoomLoginIdEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_zoomPasswordEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    matchFieldHeights(
        m_nameEdit,
        m_campusCombo,
        m_zoomLoginIdEdit,
        m_zoomPasswordEdit
        );

    m_nameEdit->installEventFilter(this);
    m_campusCombo->installEventFilter(this);
    m_zoomLoginIdEdit->installEventFilter(this);
    m_zoomPasswordEdit->installEventFilter(this);

    m_nameLabel =
        createFieldLabel(tr("My Name"), card);
    m_campusLabel =
        createFieldLabel(tr("My Campus"), card);
    m_zoomLoginIdLabel =
        createFieldLabel(tr("Zoom Login ID"), card);
    m_zoomPasswordLabel =
        createFieldLabel(tr("Zoom Password"), card);
    m_zoomLabel =
        createFieldLabel(tr("Zoom"), card);

    grid->addWidget(
        m_nameLabel,
        0,
        0,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_campusLabel,
        0,
        1,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomLoginIdLabel,
        0,
        2,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomPasswordLabel,
        0,
        3,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomLabel,
        0,
        4,
        Qt::AlignLeft
        );

    grid->addWidget(
        m_nameEdit,
        1,
        0
        );
    grid->addWidget(
        m_campusCombo,
        1,
        1
        );
    grid->addWidget(
        m_zoomLoginIdEdit,
        1,
        2
        );
    grid->addWidget(
        m_zoomPasswordEdit,
        1,
        3
        );
    grid->addWidget(
        m_zoomNotAvailableCheck,
        1,
        4,
        Qt::AlignLeft | Qt::AlignVCenter
        );
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);
    grid->setColumnStretch(5, 0);

    cardLayout->addLayout(
        grid
        );

    m_scrollContentLayout->addWidget(
        card
        );

    connect(
        m_nameEdit,
        &QLineEdit::textChanged,
        this,
        &MyInfoPage::handleEditableChanged
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
    m_scrollContentLayout->addSpacing(
        UiConstants::Pages::MajorSectionSpacing
        );

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

    m_academicCalendarProvider =
        new AcademicCalendarProvider(
            m_services
                ? m_services->dataService()
                : nullptr,
            this
            );

    const QDate today =
        QDate::currentDate();
    m_calendarVisibleMonth =
        QDate(
            qMax(today.year(), 2026),
            today.year() < 2026 ? 1 : today.month(),
            1
            );

    m_calendarView =
        new QQuickWidget(card);
    m_calendarView->installEventFilter(this);
    m_calendarView->setResizeMode(
        QQuickWidget::SizeRootObjectToView
        );
    m_calendarView->setMinimumHeight(840);
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
    m_calendarView
        ->rootContext()
        ->setContextProperty(
            QStringLiteral("academicCalendarProvider"),
            m_academicCalendarProvider
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
        syncCalendarFontSize();
        syncCalendarEventTypeColors();

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
        connect(
            root,
            SIGNAL(configureRequested(int,int)),
            this,
            SLOT(handleCalendarConfigureRequested(int,int))
            );
        connect(
            root,
            SIGNAL(displayedMonthChanged(int,int)),
            this,
            SLOT(handleCalendarDisplayedMonthChanged(int,int))
            );
    }

    cardLayout->addWidget(
        m_calendarView
        );

    buildUpcomingEventsPanel(
        cardLayout,
        card
        );

    m_scrollContentLayout->addWidget(
        card
        );
}

void MyInfoPage::buildUpcomingEventsPanel(
    QVBoxLayout* cardLayout,
    QWidget* parent
    )
{
    m_upcomingEventsHeading =
        new QLabel(
            tr("Upcoming Events"),
            parent
            );
    m_upcomingEventsHeading->setProperty(
        "role",
        QStringLiteral("section_header")
        );
    m_upcomingEventsHeading->setFont(
        FontManager::getUiFont(
            12,
            QFont::DemiBold
            )
        );

    cardLayout->addSpacing(8);
    cardLayout->addWidget(m_upcomingEventsHeading);

    for (const QString& eventType : calendarEventTypes())
    {
        m_eventTypeFilterStates.insert(
            eventType,
            true
            );
    }

    m_upcomingEventsTabs =
        new UniformWidthTabWidget(
            UniformWidthTabKind::Class,
            QStringLiteral("calendarUpcomingTabBar"),
            parent
            );
    m_upcomingEventsTabs->setObjectName(
        QStringLiteral("calendarUpcomingTabs")
        );
    m_upcomingEventsTabs->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Maximum
        );

    m_upcomingEventsTabs->addTab(
        createUpcomingEventsPage(
            &m_upcomingEventLayouts[scopeIndex(UpcomingEventsScope::CurrentMonth)],
            m_upcomingEventsTabs
            ),
        tr("Current Month")
        );
    m_upcomingEventsTabs->addTab(
        createUpcomingEventsPage(
            &m_upcomingEventLayouts[scopeIndex(UpcomingEventsScope::Next30Days)],
            m_upcomingEventsTabs
            ),
        tr("Next 30 Days")
        );
    m_upcomingEventsTabs->addTab(
        createUpcomingEventsPage(
            &m_upcomingEventLayouts[scopeIndex(UpcomingEventsScope::Next10Events)],
            m_upcomingEventsTabs
            ),
        tr("Next 10 Events")
        );

    connect(
        m_upcomingEventsTabs,
        &QTabWidget::currentChanged,
        this,
        [this](int)
        {
            refreshUpcomingEvents();
        }
        );

    cardLayout->addWidget(
        m_upcomingEventsTabs
        );

    syncEventTypeFilterButtons();
    refreshUpcomingEvents();
}

QWidget* MyInfoPage::createUpcomingEventsPage(
    QVBoxLayout** pageLayout,
    QWidget* parent
    )
{
    auto* page =
        new QWidget(parent);

    auto* layout =
        new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignTop);

    layout->addWidget(
        createEventTypeFilterRow(page)
        );

    auto* list =
        new QWidget(page);
    auto* listLayout =
        new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(6);
    listLayout->setAlignment(Qt::AlignTop);

    if (pageLayout)
    {
        *pageLayout =
            listLayout;
    }

    layout->addWidget(list);

    return page;
}

QWidget* MyInfoPage::createEventTypeFilterRow(
    QWidget* parent
    )
{
    auto* container =
        new QWidget(parent);

    auto* layout =
        new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addStretch(1);

    const QFont tagFont =
        FontManager::getUiFont();
    container->setFixedHeight(
        upcomingEventTagHeight(tagFont)
        );

    for (const QString& eventType : calendarEventTypes())
    {
        auto* button =
            new QPushButton(
                eventType,
                container
                );
        button->setCheckable(true);
        button->setProperty(
            "eventType",
            eventType
            );
        button->setFont(tagFont);
        button->setFixedSize(
            upcomingEventTagWidth(eventType, tagFont),
            upcomingEventTagHeight(tagFont)
            );
        button->setSizePolicy(
            QSizePolicy::Fixed,
            QSizePolicy::Fixed
            );
        button->setCursor(
            Qt::PointingHandCursor
            );

        connect(
            button,
            &QPushButton::toggled,
            this,
            [this, button](bool checked)
            {
                const QString eventType =
                    normalizedCalendarEventType(
                        button->property("eventType").toString()
                        );

                if (m_eventTypeFilterStates.value(eventType, true) == checked)
                {
                    button->setStyleSheet(
                        eventTypeFilterButtonStyle(
                            eventType,
                            checked,
                            button->font()
                            )
                        );
                    return;
                }

                m_eventTypeFilterStates.insert(
                    eventType,
                    checked
                    );
                syncEventTypeFilterButtons();
                refreshUpcomingEvents();
            }
            );

        m_eventTypeFilterButtons.append(button);
        layout->addWidget(button);
    }

    layout->addStretch(1);

    return container;
}

void MyInfoPage::refreshUpcomingEvents()
{
    if (!m_upcomingEventsTabs)
    {
        return;
    }

    syncEventTypeFilterButtons();

    const QList<UpcomingEventsScope> scopes{
        UpcomingEventsScope::CurrentMonth,
        UpcomingEventsScope::Next30Days,
        UpcomingEventsScope::Next10Events
    };

    std::array<QList<CalendarEvent>, UpcomingEventsScopeCount> eventsByScope;
    for (UpcomingEventsScope scope : scopes)
    {
        eventsByScope[scopeIndex(scope)] =
            upcomingEventsForScope(scope);
    }

    int dateColumnWidth = UpcomingEventDateColumnMinimumWidth;
    int timeColumnWidth = UpcomingEventTimeColumnMinimumWidth;
    int eventTypeColumnWidth = UpcomingEventTypeColumnMinimumWidth;
    const QFont eventTextFont =
        FontManager::getUiFont();
    const QFontMetrics eventTextMetrics(
        eventTextFont
        );

    for (const QString& eventType : calendarEventTypes())
    {
        eventTypeColumnWidth =
            qMax(
                eventTypeColumnWidth,
                upcomingEventTagWidth(
                    normalizedCalendarEventType(eventType),
                    eventTextFont
                    )
                );
    }

    const QStringList activeTypes =
        activeCalendarEventTypes();

    for (UpcomingEventsScope scope : scopes)
    {
        QList<CalendarEvent> filteredEvents;
        for (const CalendarEvent& event : eventsByScope[scopeIndex(scope)])
        {
            if (
                activeTypes.contains(
                    normalizedCalendarEventType(event.eventType)
                    )
                && calendarEventVisibleForCampus(event)
                )
            {
                filteredEvents.append(event);
            }
        }

        if (scope == UpcomingEventsScope::Next10Events)
        {
            while (filteredEvents.size() > UpcomingEventsLimit)
            {
                filteredEvents.removeLast();
            }
        }

        for (const CalendarEvent& event : filteredEvents)
        {
            dateColumnWidth =
                qMax(
                    dateColumnWidth,
                    eventTextMetrics.horizontalAdvance(
                        upcomingEventDateText(event)
                        ) + UpcomingEventColumnTextPadding
                    );
            timeColumnWidth =
                qMax(
                    timeColumnWidth,
                    eventTextMetrics.horizontalAdvance(
                        upcomingEventTimeText(event)
                        ) + UpcomingEventColumnTextPadding
                    );
        }
    }

    for (UpcomingEventsScope scope : scopes)
    {
        renderUpcomingEvents(
            scope,
            eventsByScope[scopeIndex(scope)],
            dateColumnWidth,
            timeColumnWidth,
            eventTypeColumnWidth
            );
    }
}

void MyInfoPage::updateCalendarCampusFilter()
{
    if (!m_calendarModel)
    {
        return;
    }

    m_calendarModel->setCampusFilter(
        currentCampusCodes(),
        allCampusCodes(),
        showAllCalendarCampuses()
        );
    refreshUpcomingEvents();
}

void MyInfoPage::renderUpcomingEvents(
    UpcomingEventsScope scope,
    const QList<CalendarEvent>& events,
    int dateColumnWidth,
    int timeColumnWidth,
    int eventTypeColumnWidth
    )
{
    QVBoxLayout* layout =
        m_upcomingEventLayouts[scopeIndex(scope)];

    if (!layout)
    {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            delete widget;
        }

        delete item;
    }

    const QStringList activeTypes =
        activeCalendarEventTypes();

    QList<CalendarEvent> filteredEvents;
    for (const CalendarEvent& event : events)
    {
        if (
            activeTypes.contains(
                normalizedCalendarEventType(event.eventType)
                )
            && calendarEventVisibleForCampus(event)
            )
        {
            filteredEvents.append(event);
        }
    }

    if (scope == UpcomingEventsScope::Next10Events)
    {
        while (filteredEvents.size() > UpcomingEventsLimit)
        {
            filteredEvents.removeLast();
        }
    }

    if (filteredEvents.isEmpty())
    {
        auto* empty =
            new QLabel(
                tr("No upcoming events."),
                layout->parentWidget()
                );
        empty->setObjectName(
            QStringLiteral("sectionSubtitle")
            );
        empty->setAlignment(
            Qt::AlignCenter
            );
        layout->addWidget(empty);
        return;
    }

    for (const CalendarEvent& event : filteredEvents)
    {
        layout->addWidget(
            createUpcomingEventRow(
                event,
                dateColumnWidth,
                timeColumnWidth,
                eventTypeColumnWidth,
                layout->parentWidget()
                )
            );
    }
}

QList<CalendarEvent> MyInfoPage::upcomingEventsForScope(
    UpcomingEventsScope scope
    ) const
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return {};
    }

    const QDate today =
        QDate::currentDate();

    switch (scope)
    {
    case UpcomingEventsScope::CurrentMonth:
    {
        const QDate firstOfMonth =
            m_calendarVisibleMonth.isValid()
                ? m_calendarVisibleMonth
                : QDate(today.year(), today.month(), 1);
        return dataService->loadCalendarEventsInRange(
            firstOfMonth,
            firstOfMonth.addMonths(1).addDays(-1)
            );
    }

    case UpcomingEventsScope::Next30Days:
        return dataService->loadCalendarEventsInRange(
            today,
            today.addDays(UpcomingEventsNext30Days)
            );

    case UpcomingEventsScope::Next10Events:
        return dataService->loadUpcomingCalendarEvents(
            today,
            100
            );
    }

    return {};
}

QStringList MyInfoPage::activeCalendarEventTypes() const
{
    QStringList activeTypes;

    for (const QString& eventType : calendarEventTypes())
    {
        if (m_eventTypeFilterStates.value(eventType, true))
        {
            activeTypes.append(
                eventType
                );
        }
    }

    return activeTypes;
}

QColor MyInfoPage::calendarEventTypeColor(
    const QString& eventType
    ) const
{
    const QString normalized =
        normalizedCalendarEventType(eventType);

    auto* dataService =
        openDataService(m_services);

    if (dataService)
    {
        const QColor storedColor(
            dataService
                ->loadSetting(
                    calendarEventTypeColorSettingKey(normalized),
                    QString()
                    )
                .toString()
            );

        if (storedColor.isValid())
        {
            return storedColor;
        }
    }

    return defaultCalendarEventTypeColor(normalized);
}

void MyInfoPage::saveCalendarEventTypeColor(
    const QString& eventType,
    const QColor& color
    )
{
    if (!color.isValid())
    {
        return;
    }

    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    dataService->saveSetting(
        calendarEventTypeColorSettingKey(eventType),
        color.name(QColor::HexRgb)
        );
}

void MyInfoPage::chooseCalendarEventTypeColor(
    const QString& eventType
    )
{
    const QString normalized =
        normalizedCalendarEventType(eventType);

    const QColor selected =
        QColorDialog::getColor(
            calendarEventTypeColor(normalized),
            this,
            tr("Choose %1 Color").arg(normalized)
            );

    if (!selected.isValid())
    {
        return;
    }

    saveCalendarEventTypeColor(
        normalized,
        selected
        );
    syncEventTypeFilterButtons();
    syncCalendarEventTypeColors();
    refreshUpcomingEvents();
}

QString MyInfoPage::eventTypeBadgeStyle(
    const QString& eventType,
    const QFont& font
    ) const
{
    const QColor color =
        calendarEventTypeColor(eventType);
    const int horizontalPadding =
        upcomingEventTagHorizontalPadding(font);
    const int verticalPadding =
        upcomingEventTagVerticalPadding(font);

    return QStringLiteral(
        "QPushButton {"
        " background-color: %1;"
        " color: %2;"
        " border: 1px solid transparent;"
        " border-radius: 4px;"
        " padding: %3px %4px;"
        "}"
        "QPushButton:hover {"
        " border: 1px solid rgba(255, 255, 255, 160);"
        "}"
        ).arg(
            color.name(QColor::HexRgb),
            readableTextColor(color),
            QString::number(verticalPadding),
            QString::number(horizontalPadding)
            );
}

QString MyInfoPage::eventTypeFilterButtonStyle(
    const QString& eventType,
    bool checked,
    const QFont& font
    ) const
{
    const QColor color =
        calendarEventTypeColor(eventType);
    const QString textColor =
        checked
            ? readableTextColor(color)
            : QStringLiteral("#66727a");
    const int horizontalPadding =
        upcomingEventTagHorizontalPadding(font);
    const int verticalPadding =
        upcomingEventTagVerticalPadding(font);

    return QStringLiteral(
        "QPushButton {"
        " background-color: %1;"
        " color: %2;"
        " border: 1px solid %3;"
        " border-radius: 6px;"
        " padding: %4px %5px;"
        "}"
        "QPushButton:hover {"
        " border-color: %6;"
        "}"
        ).arg(
            checked ? color.name(QColor::HexRgb) : QStringLiteral("transparent"),
            textColor,
            checked ? color.name(QColor::HexRgb) : QStringLiteral("#a8b2b8"),
            QString::number(verticalPadding),
            QString::number(horizontalPadding),
            color.name(QColor::HexRgb)
            );
}

void MyInfoPage::syncEventTypeFilterButtons()
{
    for (QPushButton* button : m_eventTypeFilterButtons)
    {
        if (!button)
        {
            continue;
        }

        const QString eventType =
            normalizedCalendarEventType(
                button->property("eventType").toString()
                );
        const bool checked =
            m_eventTypeFilterStates.value(
                eventType,
                true
                );
        const QFont tagFont =
            FontManager::getUiFont();

        const QSignalBlocker blocker(button);
        button->setFont(tagFont);
        button->setFixedSize(
            upcomingEventTagWidth(eventType, tagFont),
            upcomingEventTagHeight(tagFont)
            );
        button->setSizePolicy(
            QSizePolicy::Fixed,
            QSizePolicy::Fixed
            );
        button->setChecked(checked);
        button->setStyleSheet(
            eventTypeFilterButtonStyle(
                eventType,
                checked,
                tagFont
                )
            );
    }
}

void MyInfoPage::syncCalendarEventTypeColors()
{
    if (!m_calendarView)
    {
        return;
    }

    auto* root =
        m_calendarView->rootObject();

    if (!root)
    {
        return;
    }

    QVariantMap colors;
    QVariantMap textColors;

    for (const QString& eventType : calendarEventTypes())
    {
        const QString normalized =
            normalizedCalendarEventType(eventType);
        const QColor color =
            calendarEventTypeColor(normalized);

        colors.insert(
            normalized,
            color.name(QColor::HexRgb)
            );
        textColors.insert(
            normalized,
            readableTextColor(color)
            );
    }

    root->setProperty(
        "eventTypeColors",
        colors
        );
    root->setProperty(
        "eventTypeTextColors",
        textColors
        );
}

void MyInfoPage::syncCalendarFontSize()
{
    if (!m_calendarView)
    {
        return;
    }

    auto* root =
        m_calendarView->rootObject();

    if (!root)
    {
        return;
    }

    const int pixelSize =
        qMax(
            1,
            QFontInfo(
                m_calendarView->font()
                ).pixelSize()
            );

    root->setProperty(
        "baseFontPixelSize",
        pixelSize
        );
}

QString MyInfoPage::upcomingEventDateText(
    const CalendarEvent& event
    ) const
{
    if (!event.startDate.isValid())
    {
        return QStringLiteral("-");
    }

    if (
        !event.endDate.isValid()
        || event.endDate == event.startDate
        )
    {
        return event.startDate.toString(
            QStringLiteral("MMM d")
            );
    }

    const QString startFormat =
        event.startDate.year() == event.endDate.year()
            ? QStringLiteral("MMM d")
            : QStringLiteral("MMM d yyyy");

    return QStringLiteral("%1 - %2")
        .arg(
            event.startDate.toString(startFormat),
            event.endDate.toString(QStringLiteral("MMM d yyyy"))
            );
}

QString MyInfoPage::upcomingEventTimeText(
    const CalendarEvent& event
    ) const
{
    if (event.allDay)
    {
        return tr("All day");
    }

    const QString timeStatus =
        normalizedCalendarEventTimeStatus(event.timeStatus);

    if (timeStatus == QStringLiteral("Unknown"))
    {
        return tr("Unknown Time");
    }

    if (timeStatus == QStringLiteral("Unconfirmed"))
    {
        return tr("Uncomfirmed Time");
    }

    if (!event.startTime.isValid())
    {
        return QString();
    }

    auto* dataService =
        openDataService(m_services);
    const bool use24h =
        dataService
        && settingToBool(
            dataService->loadSetting(
                QStringLiteral("schedule_use_24h"),
                QStringLiteral("false")
                ),
            false
            );

    const QString format =
        use24h
            ? QStringLiteral("HH:mm")
            : QStringLiteral("h:mm AP");

    if (!event.endTime.isValid())
    {
        return event.startTime.toString(format);
    }

    return QStringLiteral("%1 - %2")
        .arg(
            event.startTime.toString(format),
            event.endTime.toString(format)
            );
}

bool MyInfoPage::calendarEventVisibleForCampus(
    const CalendarEvent& event
    ) const
{
    return CalendarEventCampusFilter::eventMatchesCampus(
        event,
        currentCampusCodes(),
        allCampusCodes(),
        showAllCalendarCampuses()
        );
}

bool MyInfoPage::showAllCalendarCampuses() const
{
    auto* dataService =
        openDataService(m_services);

    return dataService
        && settingToBool(
            dataService->loadSetting(
                CalendarSettingsKeys::ShowEventsAtAllCampuses,
                false
                ),
            false
            );
}

QStringList MyInfoPage::currentCampusCodes() const
{
    QStringList codes;

    if (m_campusCombo && m_campusCombo->currentIndex() >= 0)
    {
        codes.append(
            m_campusCombo->currentData().toString()
            );
        codes.append(
            m_campusCombo->currentText()
            );
    }

    const QString currentId =
        m_campusCombo
        && m_campusCombo->currentIndex() >= 0
            ? m_campusCombo->currentData().toString()
            : QString();
    const QString currentName =
        m_campusCombo
        && m_campusCombo->currentIndex() >= 0
            ? m_campusCombo->currentText()
            : QString();

    const QList<CampusInfo> campuses =
        campusRepository().loadCampuses();

    for (const CampusInfo& campus : campuses)
    {
        if (
            campus.id.compare(currentId, Qt::CaseInsensitive) == 0
            || campusDisplayName(campus).compare(currentName, Qt::CaseInsensitive) == 0
            || campus.campusName.compare(currentName, Qt::CaseInsensitive) == 0
            )
        {
            codes.append(campus.campusCode);
            codes.append(campus.id);
            codes.append(campusDisplayName(campus));
        }
    }

    codes.removeAll(QString());
    codes.removeDuplicates();
    return codes;
}

QStringList MyInfoPage::allCampusCodes() const
{
    QStringList codes;
    const QList<CampusInfo> campuses =
        campusRepository().loadCampuses();

    for (const CampusInfo& campus : campuses)
    {
        codes.append(campus.campusCode);
        codes.append(campus.id);
    }

    codes.removeAll(QString());
    codes.removeDuplicates();
    return codes;
}

QWidget* MyInfoPage::createUpcomingEventRow(
    const CalendarEvent& event,
    int dateColumnWidth,
    int timeColumnWidth,
    int eventTypeColumnWidth,
    QWidget* parent
    )
{
    const QFont eventFont =
        FontManager::getUiFont();
    const int tagHeight =
        upcomingEventTagHeight(eventFont);
    const int rowHeight =
        upcomingEventRowHeight(eventFont);

    auto* row =
        new QFrame(parent);
    row->setObjectName(
        QStringLiteral("upcomingCalendarEventRow")
        );
    row->setCursor(
        Qt::PointingHandCursor
        );
    row->setMouseTracking(true);
    row->setFixedHeight(
        rowHeight
        );
    row->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );
    row->setStyleSheet(
        QStringLiteral(
            "QFrame#upcomingCalendarEventRow {"
            " background: transparent;"
            " border: 1px solid transparent;"
            " border-radius: 6px;"
            "}"
            "QFrame#upcomingCalendarEventRow:hover {"
            " background-color: rgba(83, 111, 138, 35);"
            " border-color: rgba(83, 111, 138, 120);"
            "}"
            )
        );
    row->setProperty(
        "calendarEventId",
        event.id
        );
    row->installEventFilter(this);

    auto* layout =
        new QHBoxLayout(row);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(UpcomingEventColumnSpacing);

    auto* date =
        new QLabel(
            upcomingEventDateText(event),
            row
            );
    date->setFont(eventFont);
    date->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Preferred
        );
    date->setFixedWidth(dateColumnWidth);
    date->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    date->setCursor(
        Qt::PointingHandCursor
        );
    date->setProperty(
        "calendarEventId",
        event.id
        );

    auto* time =
        new QLabel(
            upcomingEventTimeText(event),
            row
            );
    time->setFont(eventFont);
    time->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Preferred
        );
    time->setFixedWidth(timeColumnWidth);
    time->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    time->setCursor(
        Qt::PointingHandCursor
        );
    time->setProperty(
        "calendarEventId",
        event.id
        );

    auto* title =
        new HoverMarqueeLabel(row);
    title->setText(event.title);
    title->setFont(eventFont);
    title->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );
    title->setMinimumWidth(0);
    title->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    title->setTextInteractionFlags(
        Qt::NoTextInteraction
        );
    title->setCursor(
        Qt::PointingHandCursor
        );
    title->setProperty(
        "calendarEventId",
        event.id
        );

    auto* type =
        new QPushButton(
            normalizedCalendarEventType(event.eventType),
            row
            );
    type->setFont(eventFont);
    type->setFixedSize(
        eventTypeColumnWidth,
        tagHeight
        );
    type->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );
    type->setCursor(
        Qt::PointingHandCursor
        );
    type->setToolTip(
        tr("Choose %1 color").arg(
            normalizedCalendarEventType(event.eventType)
            )
        );
    type->setStyleSheet(
        eventTypeBadgeStyle(
            event.eventType,
            eventFont
            )
        );

    row->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );
    date->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );
    time->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );
    title->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );
    type->setProperty(
        "calendarEventTitleLabel",
        QVariant::fromValue<QObject*>(title)
        );

    date->installEventFilter(this);
    time->installEventFilter(this);
    title->installEventFilter(this);
    type->installEventFilter(this);

    layout->addWidget(date);
    layout->addWidget(time);
    layout->addWidget(title, 1);
    layout->addWidget(type);

    connect(
        type,
        &QPushButton::clicked,
        this,
        [this, event]()
        {
            chooseCalendarEventTypeColor(
                event.eventType
                );
        }
        );

    return row;
}

void MyInfoPage::loadPageData()
{
    m_loading = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadStoredSettings();
    refreshGeneratedContent();

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

    const QSignalBlocker nameBlocker(m_nameEdit);
    const QSignalBlocker campusBlocker(m_campusCombo);
    const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
    const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);
    const QSignalBlocker checkBlocker(m_zoomNotAvailableCheck);

    m_nameEdit->setText(
        dataService
            ->loadSetting(
                SettingsKeys::Name,
                QString()
                )
            .toString()
        );

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
    updateMyInformationFieldWidths();
    updateCalendarCampusFilter();
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
        SettingsKeys::Name,
        m_nameEdit->text()
        );
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

void MyInfoPage::refreshGeneratedContent()
{
    rebuildClassInformation();
}

void MyInfoPage::rebuildClassInformation()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService || !m_classInformationLayout)
    {
        return;
    }

    clearClassInformation();
    m_classInformationTabs = nullptr;

    QList<ClassSummary> summaries;

    const QList<Classroom> classes =
        dataService->getClasses();

    for (const Classroom& classroom : classes)
    {
        ClassSummary summary;
        summary.classroom = classroom;
        summary.info =
            dataService->loadClassInfo(
                classroom.id
                );
        summary.studentCount =
            dataService->getRosterStudentCount(
                classroom.id
                );

        if (summary.info.teacherId > 0)
        {
            summary.teacher =
                dataService->getTeacher(
                    summary.info.teacherId
                    );
        }

        summary.displayName =
            classTitleText(
                classroom,
                summary.info
                );

        summaries.append(summary);
    }

    if (summaries.isEmpty())
    {
        m_selectedClassId = -1;

        auto* emptyLabel =
            new QLabel(
                tr("No classes available."),
                m_classInformationContent
                );
        emptyLabel->setObjectName("pageSubtitle");
        m_classInformationLayout->addWidget(
            emptyLabel
            );
        return;
    }

    QList<SubPrepClassNavigation::ClassEntry> navigationEntries;

    for (const ClassSummary& summary : std::as_const(summaries))
    {
        SubPrepClassNavigation::ClassEntry entry;
        entry.classId =
            summary.classroom.id;
        entry.classroomName =
            summary.classroom.name;
        entry.grade =
            summary.info.classGrade;
        entry.level =
            summary.info.classLevel;
        entry.regularTimes =
            summary.info.classTimes;
        entry.intensiveTimes =
            summary.info.intensiveTimes;
        entry.teacherEn =
            summary.teacher.teacherEn;
        entry.teacherKr =
            summary.teacher.teacherKr;

        navigationEntries.append(entry);
    }

    const SubPrepClassNavigation::Model navigation =
        SubPrepClassNavigation::build(
            navigationEntries
            );

    const int previousClassId =
        m_selectedClassId;

    auto findSummary =
        [&summaries](int classId) -> const ClassSummary*
        {
            for (const ClassSummary& summary : summaries)
            {
                if (summary.classroom.id == classId)
                {
                    return &summary;
                }
            }

            return nullptr;
        };

    auto tabIndexForClass =
        [](QTabWidget* tabs, int classId)
        {
            if (!tabs)
            {
                return -1;
            }

            for (int index = 0; index < tabs->count(); ++index)
            {
                QWidget* page =
                    tabs->widget(index);

                if (
                    page
                    && page->property("class_id").toInt() == classId
                    )
                {
                    return index;
                }
            }

            return -1;
        };

    auto updateSelectedFromTabs =
        [this](QTabWidget* tabs)
        {
            if (!tabs || tabs->currentIndex() < 0)
            {
                return;
            }

            QWidget* page =
                tabs->currentWidget();

            const int classId =
                page
                    ? page->property("class_id").toInt()
                    : -1;

            if (classId > 0)
            {
                m_selectedClassId = classId;
            }
        };

    auto createClassPage =
        [this](const ClassSummary& summary, QWidget* parent)
        {
            const bool unassigned =
                summary.teacher.id <= 0;

            auto* page =
                new QWidget(parent);
            page->setProperty(
                "class_id",
                summary.classroom.id
                );

            auto* pageLayout =
                new QVBoxLayout(page);
            pageLayout->setContentsMargins(
                0,
                ClassTabContentTopMargin,
                0,
                0
                );
            pageLayout->setSpacing(
                UiConstants::ClassInfo::Page::ContentSpacing
                );
            pageLayout->setAlignment(Qt::AlignTop);

            auto* teacherHeading =
                new QLabel(
                    teacherHeadingText(
                        summary.teacher,
                        unassigned
                        ),
                    page
                    );
            teacherHeading->setObjectName("sectionTitle");
            teacherHeading->setFont(
                FontManager::getUiFont(
                    16,
                    QFont::DemiBold
                    )
                );
            pageLayout->addWidget(
                teacherHeading
                );

            auto* teacherCard =
                new QFrame(page);
            teacherCard->setProperty(
                "role",
                UiRoles::Card
                );
            teacherCard->setObjectName(
                "sectionCard"
                );

            auto* teacherLayout =
                new QVBoxLayout(teacherCard);
            teacherLayout->setAlignment(Qt::AlignTop);
            teacherLayout->setContentsMargins(
                UiConstants::ClassInfo::SectionCard::Margin,
                UiConstants::ClassInfo::SectionCard::Margin,
                UiConstants::ClassInfo::SectionCard::Margin,
                UiConstants::ClassInfo::SectionCard::Margin
                );
            teacherLayout->setSpacing(
                UiConstants::ClassInfo::SectionCard::Spacing
                );

            auto* teacherGrid =
                new QGridLayout;
            teacherGrid->setHorizontalSpacing(
                UiConstants::ClassInfo::Form::HorizontalSpacing
                );
            teacherGrid->setVerticalSpacing(
                UiConstants::ClassInfo::Form::VerticalSpacing
                );
            teacherGrid->setColumnStretch(0, 1);
            teacherGrid->setColumnStretch(1, 1);
            teacherGrid->setColumnStretch(2, 1);
            teacherGrid->setColumnStretch(3, 0);

            addHorizontalInfoField(
                teacherGrid,
                0,
                1,
                0,
                tr("Internet Type"),
                unassigned ? QString() : summary.teacher.internetType,
                teacherCard
                );
            addHorizontalInfoField(
                teacherGrid,
                0,
                1,
                1,
                tr("WiFi Name"),
                unassigned ? QString() : summary.teacher.wifiName,
                teacherCard
                );
            addHorizontalInfoField(
                teacherGrid,
                0,
                1,
                2,
                tr("WiFi Password"),
                unassigned ? QString() : summary.teacher.wifiPassword,
                teacherCard
                );
            teacherGrid->addItem(
                new QSpacerItem(
                    0,
                    UiConstants::ClassInfo::Form::GroupSpacerHeight,
                    QSizePolicy::Minimum,
                    QSizePolicy::Fixed
                    ),
                2,
                0,
                1,
                4
                );
            addHorizontalInfoField(
                teacherGrid,
                3,
                4,
                0,
                tr("Projection Type"),
                unassigned ? QString() : summary.teacher.projectionType,
                teacherCard
                );
            addHorizontalInfoField(
                teacherGrid,
                3,
                4,
                1,
                tr("Zoom ID"),
                unassigned ? QString() : summary.teacher.zoomId,
                teacherCard
                );
            addHorizontalInfoField(
                teacherGrid,
                3,
                4,
                2,
                tr("Zoom Password"),
                unassigned ? QString() : summary.teacher.zoomPassword,
                teacherCard
                );

            teacherLayout->addLayout(
                teacherGrid
                );

            teacherLayout->addWidget(
                createFieldLabel(
                    tr("Notes"),
                    teacherCard
                    )
                );

            auto* teacherNotes =
                createTextEdit(
                    6,
                    true,
                    teacherCard
                    );
            teacherNotes->setPlainText(
                unassigned ? QString() : summary.teacher.notes
                );
            teacherLayout->addWidget(
                teacherNotes
                );

            pageLayout->addWidget(
                teacherCard
                );

            auto* classCard =
                new SectionCard(
                    summary.displayName,
                    page
                    );

            auto* classGrid =
                new QGridLayout;
            classGrid->setHorizontalSpacing(
                UiConstants::ClassInfo::Form::HorizontalSpacing
                );
            classGrid->setVerticalSpacing(
                UiConstants::ClassInfo::Form::VerticalSpacing
                );
            classGrid->setColumnStretch(1, 1);

            int row = 0;
            addInfoRow(
                classGrid,
                row++,
                tr("# of Students"),
                QString::number(summary.studentCount),
                classCard
                );

            if (!summary.info.classTimes.isEmpty())
            {
                addInfoRow(
                    classGrid,
                    row++,
                    tr("Regular"),
                    formatSchedule(summary.info.classTimes),
                    classCard
                    );
            }

            if (!summary.info.intensiveTimes.isEmpty())
            {
                addInfoRow(
                    classGrid,
                    row++,
                    tr("Intensive"),
                    formatSchedule(summary.info.intensiveTimes),
                    classCard
                    );
            }

            if (
                summary.info.classTimes.isEmpty()
                && summary.info.intensiveTimes.isEmpty()
                )
            {
                addInfoRow(
                    classGrid,
                    row++,
                    tr("Schedule"),
                    NotAvailableText,
                    classCard
                    );
            }

            classCard->contentLayout()->addLayout(
                classGrid
                );

            classCard->contentLayout()->addWidget(
                createFieldLabel(
                    tr("Class Notes"),
                    classCard
                    )
                );

            auto* classNotes =
                createTextEdit(
                    6,
                    true,
                    classCard
                    );
            classNotes->setPlainText(
                summary.info.notes
                );
            classCard->contentLayout()->addWidget(
                classNotes
                );

            classCard->contentLayout()->addWidget(
                createFieldLabel(
                    tr("Time Filler Activities"),
                    classCard
                    )
                );

            auto* timeFillerActivities =
                createTextEdit(
                    4,
                    true,
                    classCard
                    );
            timeFillerActivities->setPlainText(
                valueOrNa(
                    summary.info.timeFillerActivities
                    )
                );
            classCard->contentLayout()->addWidget(
                timeFillerActivities
                );

            pageLayout->addWidget(
                classCard
                );

            return page;
        };

    if (navigation.mode == SubPrepClassNavigation::Mode::Flat)
    {
        auto* tabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Class,
                QStringLiteral("myInfoClassTabBar"),
                m_classInformationContent
                );
        tabs->setObjectName("myInfoClassTabs");

        for (const SubPrepClassNavigation::ClassTab& tab
             : navigation.flatClasses)
        {
            const ClassSummary* summary =
                findSummary(tab.classId);

            if (!summary)
            {
                continue;
            }

            tabs->addTab(
                createClassPage(
                    *summary,
                    tabs
                    ),
                tab.label
                );
        }

        connect(
            tabs,
            &QTabWidget::currentChanged,
            this,
            [tabs, updateSelectedFromTabs](int)
            {
                updateSelectedFromTabs(tabs);
            }
            );

        int selectedIndex =
            tabIndexForClass(
                tabs,
                previousClassId
                );

        if (selectedIndex < 0 && tabs->count() > 0)
        {
            selectedIndex = 0;
        }

        if (selectedIndex >= 0)
        {
            tabs->setCurrentIndex(selectedIndex);
        }

        updateSelectedFromTabs(tabs);

        m_classInformationTabs = tabs;
        m_classInformationLayout->addWidget(
            tabs
            );
        return;
    }

    auto* gradeTabs =
        new UniformWidthTabWidget(
            UniformWidthTabKind::Grade,
            QStringLiteral("myInfoGradeTabBar"),
            m_classInformationContent
            );
    gradeTabs->setObjectName("myInfoGradeTabs");

    int selectedGradeIndex = -1;
    int selectedClassIndex = -1;

    for (const SubPrepClassNavigation::GradeGroup& group
         : navigation.gradeGroups)
    {
        auto* gradePage =
            new QWidget(gradeTabs);

        auto* gradeLayout =
            new QVBoxLayout(gradePage);
        gradeLayout->setContentsMargins(0, 0, 0, 0);
        gradeLayout->setSpacing(
            UiConstants::ClassInfo::Page::ContentSpacing
            );
        gradeLayout->setAlignment(Qt::AlignTop);

        auto* classTabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Class,
                QStringLiteral("myInfoClassTabBar"),
                gradePage
                );
        classTabs->setObjectName("myInfoClassTabs");

        for (const SubPrepClassNavigation::ClassTab& tab
             : group.classes)
        {
            const ClassSummary* summary =
                findSummary(tab.classId);

            if (!summary)
            {
                continue;
            }

            classTabs->addTab(
                createClassPage(
                    *summary,
                    classTabs
                    ),
                tab.label
                );

            if (tab.classId == previousClassId)
            {
                selectedGradeIndex =
                    gradeTabs->count();
                selectedClassIndex =
                    classTabs->count() - 1;
            }
        }

        connect(
            classTabs,
            &QTabWidget::currentChanged,
            this,
            [classTabs, updateSelectedFromTabs](int)
            {
                updateSelectedFromTabs(classTabs);
            }
            );

        gradeLayout->addWidget(
            classTabs
            );

        gradeTabs->addTab(
            gradePage,
            group.label
            );
    }

    connect(
        gradeTabs,
        &QTabWidget::currentChanged,
        this,
        [gradeTabs, updateSelectedFromTabs](int index)
        {
            QWidget* gradePage =
                gradeTabs->widget(index);

            auto* classTabs =
                gradePage
                    ? gradePage->findChild<QTabWidget*>(
                        QStringLiteral("myInfoClassTabs"),
                        Qt::FindDirectChildrenOnly
                        )
                    : nullptr;

            updateSelectedFromTabs(classTabs);
        }
        );

    if (gradeTabs->count() > 0)
    {
        if (selectedGradeIndex < 0)
        {
            selectedGradeIndex = 0;
        }

        gradeTabs->setCurrentIndex(
            selectedGradeIndex
            );

        QWidget* gradePage =
            gradeTabs->widget(
                selectedGradeIndex
                );

        auto* selectedClassTabs =
            gradePage
                ? gradePage->findChild<QTabWidget*>(
                    QStringLiteral("myInfoClassTabs"),
                    Qt::FindDirectChildrenOnly
                    )
                : nullptr;

        if (selectedClassTabs)
        {
            if (
                selectedClassIndex < 0
                || selectedClassIndex >= selectedClassTabs->count()
                )
            {
                selectedClassIndex = 0;
            }

            if (selectedClassTabs->count() > 0)
            {
                selectedClassTabs->setCurrentIndex(
                    selectedClassIndex
                    );
            }

            updateSelectedFromTabs(selectedClassTabs);
        }
    }

    m_classInformationTabs = gradeTabs;
    m_classInformationLayout->addWidget(
        gradeTabs
        );
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

    if (changed)
    {
        updateMyInformationFieldWidths();
    }

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

void MyInfoPage::updateMyInformationFieldWidths()
{
    WidgetSizing::updateTextAwareFieldWidth(
        m_nameEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_campusCombo,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_zoomLoginIdEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_zoomPasswordEdit,
        UiConstants::Forms::FieldMinimumWidth
        );

    matchFieldHeights(
        m_nameEdit,
        m_campusCombo,
        m_zoomLoginIdEdit,
        m_zoomPasswordEdit
        );
}

void MyInfoPage::clearClassInformation()
{
    clearLayout(
        m_classInformationLayout
        );
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

    refreshUpcomingEvents();
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
            UiConstants::Pages::SectionTitleFontSize,
            QFont::DemiBold
            )
        );

    return label;
}

QTextEdit* MyInfoPage::createTextEdit(
    int minimumLines,
    bool readOnly,
    QWidget* parent
    ) const
{
    auto* edit =
        new QTextEdit(parent);

    edit->setReadOnly(
        readOnly
        );
    edit->setMinimumHeight(
        textEditHeightForLines(
            edit,
            minimumLines
            )
        );
    edit->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    edit->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    edit->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    return edit;
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
