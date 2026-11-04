#include "next/ui/calendar_view.h"

#include "features/calendar/ui/academic_calendar_provider.h"
#include "features/calendar/ui/calendar_event_model.h"

#include <QCoreApplication>
#include <QColor>
#include <QDateTime>
#include <QFontInfo>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSizePolicy>
#include <QTime>
#include <QUrl>

#include <utility>

namespace ClassMngr::Next::Ui
{

class CalendarViewEventRelay final : public QObject
{
    Q_OBJECT

public:
    CalendarViewEventRelay(CalendarViewCallbacks callbacks, QObject* parent)
        : QObject(parent)
        , m_callbacks(std::move(callbacks))
    {
    }

public slots:
    void relayDayActivated(const int year, const int month, const int day)
    {
        if (m_callbacks.dayActivated)
        {
            m_callbacks.dayActivated(year, month, day);
        }
    }

    void relayEventActivated(const QString& eventId)
    {
        if (m_callbacks.eventActivated)
        {
            m_callbacks.eventActivated(eventId);
        }
    }

    void relayDisplayedMonthChanged(const int year, const int month)
    {
        if (m_callbacks.displayedMonthChanged)
        {
            m_callbacks.displayedMonthChanged(year, month);
        }
    }

private:
    CalendarViewCallbacks m_callbacks;
};

namespace
{

class QmlCalendarView final : public CalendarView
{
public:
    explicit QmlCalendarView(QQuickWidget* widget)
        : m_widget(widget)
    {
    }

    ~QmlCalendarView() override
    {
        delete m_widget;
    }

    [[nodiscard]] QWidget* widget() const noexcept override
    {
        return m_widget;
    }

    [[nodiscard]] bool isLoaded() const noexcept override
    {
        return m_widget && m_widget->rootObject();
    }

    void applyPalette(const QPalette& palette, const bool embedded) override
    {
        if (!isLoaded())
        {
            return;
        }

        QObject* const root = m_widget->rootObject();
        const bool dark = !embedded
            && palette.color(QPalette::Window).lightness() < 128;
        const QColor toolbarColor = dark ? QColor(QStringLiteral("#353b43"))
            : QColor(QStringLiteral("#536f8a"));
        const QColor toolbarTextColor = dark ? QColor(QStringLiteral("#f5f4f0"))
            : QColor(QStringLiteral("#fbfaf7"));
        root->setProperty("toolbarColor", toolbarColor);
        root->setProperty("toolbarTextColor", toolbarTextColor);
        root->setProperty("textColor", embedded
            ? QColor(QStringLiteral("#27313a")) : palette.color(QPalette::Text));
        root->setProperty("mutedTextColor", dark ? QColor(QStringLiteral("#bac1c8"))
            : QColor(QStringLiteral("#66727a")));
        root->setProperty("inactiveTextColor", dark ? QColor(QStringLiteral("#747c84"))
            : QColor(QStringLiteral("#a5aaa9")));
        root->setProperty("calendarBackground", embedded
            ? QColor(QStringLiteral("#f5f3ee")) : palette.color(QPalette::Base));
        root->setProperty("headerBackground", embedded
            ? QColor(QStringLiteral("#e2e1dc")) : palette.color(QPalette::AlternateBase));
        root->setProperty("cellBackground", embedded
            ? QColor(QStringLiteral("#f5f3ee")) : palette.color(QPalette::Base));
        root->setProperty("weekendBackground", dark ? QColor(QStringLiteral("#30363d"))
            : QColor(QStringLiteral("#e8edf1")));
        root->setProperty("gridLineColor", embedded
            ? QColor(QStringLiteral("#cbc9c2")) : palette.color(QPalette::Mid));
        root->setProperty("accentColor", embedded
            ? toolbarColor : palette.color(QPalette::Highlight));
        root->setProperty("accentTextColor", embedded
            ? toolbarTextColor : palette.color(QPalette::HighlightedText));
        const int pixelSize = QFontInfo(m_widget->font()).pixelSize();
        if (pixelSize > 0)
        {
            root->setProperty("baseFontPixelSize", pixelSize);
        }
    }

    void setEventTypeColors(
        const QVariantMap& colors,
        const QVariantMap& textColors
        ) override
    {
        if (!isLoaded())
        {
            return;
        }
        QObject* const root = m_widget->rootObject();
        root->setProperty("eventTypeColors", colors);
        root->setProperty("eventTypeTextColors", textColors);
    }

private:
    QQuickWidget* m_widget = nullptr;
};

} // namespace

CalendarViewCreationResult createQmlCalendarView(
    const CalendarViewRequest& request
    )
{
    if (!request.parent || !request.eventModel || !request.academicProvider)
    {
        return {
            nullptr,
            QCoreApplication::translate(
                "CalendarPage", "The calendar view could not be initialized.")
        };
    }

    auto widget = std::make_unique<QQuickWidget>(request.parent);
    widget->setObjectName(QStringLiteral("nextCalendarMonthGrid"));
    widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    widget->setMinimumHeight(request.embedded ? 840 : 500);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    widget->rootContext()->setContextProperty(
        QStringLiteral("calendarEventProvider"), request.eventModel);
    widget->rootContext()->setContextProperty(
        QStringLiteral("academicCalendarProvider"), request.academicProvider);
    widget->setInitialProperties({
        {QStringLiteral("shownDate"), QDateTime(request.shownMonth, QTime(0, 0))}
    });
    widget->setSource(QUrl(QStringLiteral(
        "qrc:/qt/qml/ClassMngr/Calendar/EventCalendar.qml")));

    QObject* const root = widget->rootObject();
    if (!root)
    {
        const QString error = widget->errors().isEmpty()
            ? QCoreApplication::translate(
                "CalendarPage", "The calendar grid could not be loaded.")
            : widget->errors().front().toString();
        return {nullptr, error};
    }

    auto* const relay = new CalendarViewEventRelay(
        request.callbacks, widget.get());
    QObject::connect(root, SIGNAL(dayActivated(int,int,int)),
        relay, SLOT(relayDayActivated(int,int,int)));
    QObject::connect(root, SIGNAL(eventActivated(QString)),
        relay, SLOT(relayEventActivated(QString)));
    QObject::connect(root, SIGNAL(displayedMonthChanged(int,int)),
        relay, SLOT(relayDisplayedMonthChanged(int,int)));

    auto view = std::make_unique<QmlCalendarView>(widget.get());
    static_cast<void>(widget.release());
    return {std::move(view), {}};
}

} // namespace ClassMngr::Next::Ui

#include "calendar_view.moc"
