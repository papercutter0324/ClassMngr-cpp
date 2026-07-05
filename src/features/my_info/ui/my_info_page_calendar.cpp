#include "my_info_page.h"

#include "academic_calendar_dialog.h"
#include "academic_calendar_provider.h"
#include "calendar_event_dialog.h"
#include "calendar_event_model.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"

#include <QDate>
#include <QDialog>
#include <QFontInfo>
#include <QFrame>
#include <QLabel>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSizePolicy>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
constexpr int UntitledCardTopMargin = 4;

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
