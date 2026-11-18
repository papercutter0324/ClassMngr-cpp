#include "next/ui/calendar_view.h"

#include "features/calendar/ui/academic_calendar_provider.h"
#include "features/calendar/ui/calendar_event_model.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEnterEvent>
#include <QFrame>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>
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

class CalendarDayCellFrame final : public QFrame
{
public:
    using Activation = std::function<void()>;

    CalendarDayCellFrame(Activation activation, QWidget* parent)
        : QFrame(parent)
        , m_activation(std::move(activation))
    {
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
    }

    void setCalendarAppearance(
        const QColor& background,
        const QColor& gridLine,
        const QColor& focusColor,
        const bool activeMonth,
        const bool firstColumn,
        const bool lastColumn,
        const bool nextCellActive,
        const bool lowerCellActive
        )
    {
        m_background = background;
        m_gridLine = gridLine;
        m_focusColor = focusColor;
        m_drawLeft = activeMonth && !firstColumn;
        m_drawTop = activeMonth;
        m_drawRight = activeMonth && !lastColumn && !nextCellActive;
        m_drawBottom = activeMonth && !lowerCellActive;
        update();
    }

    void setHolidayTint(const bool enabled, const int height)
    {
        m_holidayTint = enabled;
        m_holidayTintHeight = qMax(0, height);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), m_background);
        if (m_holidayTint)
        {
            painter.fillRect(
                QRect(rect().left(), rect().top(), rect().width(),
                      qMin(rect().height(), m_holidayTintHeight)),
                QColor(207, 63, 53, 41));
        }
        painter.setPen(QPen(m_gridLine, 1));
        if (m_drawLeft)
        {
            painter.drawLine(rect().topLeft(), rect().bottomLeft());
        }
        if (m_drawTop)
        {
            painter.drawLine(rect().topLeft(), rect().topRight());
        }
        if (m_drawRight)
        {
            painter.drawLine(rect().topRight(), rect().bottomRight());
        }
        if (m_drawBottom)
        {
            painter.drawLine(rect().bottomLeft(), rect().bottomRight());
        }
        if (hasFocus())
        {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(m_focusColor, 1, Qt::DotLine));
            painter.drawRect(rect().adjusted(3, 3, -4, -4));
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()))
        {
            if (m_activation)
            {
                m_activation();
            }
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
             || event->key() == Qt::Key_Space)
            && m_activation)
        {
            m_activation();
            event->accept();
            return;
        }
        QFrame::keyPressEvent(event);
    }

private:
    Activation m_activation;
    QColor m_background{Qt::white};
    QColor m_gridLine{QStringLiteral("#cbc9c2")};
    QColor m_focusColor{QStringLiteral("#536f8a")};
    bool m_drawLeft = false;
    bool m_drawTop = false;
    bool m_drawRight = false;
    bool m_drawBottom = false;
    bool m_holidayTint = false;
    int m_holidayTintHeight = 0;
};

class CalendarEventButton final : public QPushButton
{
public:
    CalendarEventButton(
        QString label,
        QColor background,
        QColor foreground,
        QWidget* parent
        )
        : QPushButton(parent)
        , m_label(std::move(label))
        , m_background(std::move(background))
        , m_foreground(std::move(foreground))
    {
        setText(m_label);
        setToolTip(m_label);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_marqueeTimer.setInterval(33);
        QObject::connect(&m_marqueeTimer, &QTimer::timeout,
            this, qOverload<>(&CalendarEventButton::update));
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        QPushButton::enterEvent(event);
        m_hoverElapsed.start();
        m_marqueeTimer.start();
        update();
    }

    void leaveEvent(QEvent* event) override
    {
        m_marqueeTimer.stop();
        QPushButton::leaveEvent(event);
        update();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_background);
        painter.drawRoundedRect(rect(), 3, 3);

        const QRect textRect = rect().adjusted(4, 0, -4, 0);
        const QFontMetrics metrics(font());
        const int textWidth = metrics.horizontalAdvance(m_label);
        painter.setPen(m_foreground);
        if (underMouse() && textWidth > textRect.width())
        {
            const qint64 elapsed = qMax<qint64>(0, m_hoverElapsed.elapsed() - 250);
            const int maxOffset = textWidth - textRect.width() + 24;
            const qint64 scrollDuration = static_cast<qint64>(maxOffset) * 42;
            const qint64 cycleDuration = scrollDuration + 350;
            const qint64 phase = cycleDuration > 0
                ? elapsed % cycleDuration : 0;
            const int offset = phase < scrollDuration
                ? static_cast<int>(phase / 42) : 0;
            painter.save();
            painter.setClipRect(textRect);
            const int baseline = textRect.center().y()
                + (metrics.ascent() - metrics.descent()) / 2;
            painter.drawText(textRect.left() - offset, baseline, m_label);
            painter.restore();
        }
        else
        {
            painter.drawText(textRect,
                Qt::AlignHCenter | Qt::AlignVCenter,
                metrics.elidedText(m_label, Qt::ElideRight, textRect.width()));
        }

        if (hasFocus())
        {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(palette().color(QPalette::Highlight), 1));
            painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 3, 3);
        }
    }

private:
    QString m_label;
    QColor m_background;
    QColor m_foreground;
    QTimer m_marqueeTimer;
    QElapsedTimer m_hoverElapsed;
};

class CalendarMonthTitleLabel final : public QLabel
{
public:
    CalendarMonthTitleLabel(
        QString title,
        QFont preferredFont,
        QWidget* parent
        )
        : QLabel(std::move(title), parent)
        , m_preferredFont(std::move(preferredFont))
    {
        setAlignment(Qt::AlignCenter);
        setAccessibleName(text());
        setToolTip(text());
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setWordWrap(true);
        setFont(m_preferredFont);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QLabel::resizeEvent(event);
        updateFontForAvailableWidth();
    }

private:
    void updateFontForAvailableWidth()
    {
        const int availableWidth = contentsRect().width();
        if (availableWidth <= 0)
        {
            return;
        }

        QFont fittedFont = m_preferredFont;
        const int preferredPixelSize = fittedFont.pixelSize() > 0
            ? fittedFont.pixelSize() : 14;
        constexpr int minimumTitlePixelSize = 10;
        for (int pixelSize = preferredPixelSize;
             pixelSize >= minimumTitlePixelSize;
             --pixelSize)
        {
            fittedFont.setPixelSize(pixelSize);
            if (QFontMetrics(fittedFont).horizontalAdvance(text())
                <= availableWidth)
            {
                if (font() != fittedFont)
                {
                    setFont(fittedFont);
                }
                return;
            }
        }

        fittedFont.setPixelSize(minimumTitlePixelSize);
        if (font() != fittedFont)
        {
            setFont(fittedFont);
        }
    }

    QFont m_preferredFont;
};

// QWidget-backed CalendarView used by the packaged application. It keeps the
// existing model, palette, and callback boundary while drawing the month grid.
class WidgetCalendarView final : public CalendarView
{
public:
    explicit WidgetCalendarView(const CalendarViewRequest& request)
        : m_widget(new QWidget(request.parent))
        , m_eventModel(request.eventModel)
        , m_academicProvider(request.academicProvider)
        , m_callbacks(request.callbacks)
        , m_shownMonth(request.shownMonth.isValid()
              ? QDate(request.shownMonth.year(), request.shownMonth.month(), 1)
              : QDate::currentDate())
        , m_embedded(request.embedded)
    {
        m_widget->setObjectName(QStringLiteral("nextCalendarMonthGridWidget"));
        m_widget->setAccessibleName(QCoreApplication::translate(
            "EventCalendar", "Calendar"));
        m_widget->setMinimumHeight(m_embedded ? 840 : 500);
        m_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        if (m_eventModel)
        {
            QObject::connect(
                m_eventModel, &CalendarEventModel::revisionChanged,
                m_widget, [this] { rebuild(); });
            QObject::connect(
                m_eventModel, &CalendarEventModel::loadingChanged,
                m_widget, [this] { rebuild(); });
        }
        if (m_academicProvider)
        {
            QObject::connect(
                m_academicProvider, &AcademicCalendarProvider::revisionChanged,
                m_widget, [this] { rebuild(); });
            QObject::connect(
                m_academicProvider,
                &AcademicCalendarProvider::firstDayOfWeekChanged,
                m_widget, [this] { rebuild(); });
        }

        rebuild();
        notifyDisplayedMonthChanged();
    }

    ~WidgetCalendarView() override
    {
        delete m_widget;
    }

    [[nodiscard]] QWidget* widget() const noexcept override
    {
        return m_widget;
    }

    [[nodiscard]] bool isLoaded() const noexcept override
    {
        return m_widget && m_loaded && m_dayCellCount > 0;
    }

    void applyPalette(const QPalette& palette, const bool embedded) override
    {
        m_embedded = embedded;
        const bool dark = !embedded
            && palette.color(QPalette::Window).lightness() < 128;
        m_toolbarColor = dark ? QColor(QStringLiteral("#353b43"))
                              : QColor(QStringLiteral("#536f8a"));
        m_toolbarTextColor = dark ? QColor(QStringLiteral("#f5f4f0"))
                                  : QColor(QStringLiteral("#fbfaf7"));
        m_textColor = embedded ? QColor(QStringLiteral("#27313a"))
                               : palette.color(QPalette::Text);
        m_mutedTextColor = dark ? QColor(QStringLiteral("#bac1c8"))
                                : QColor(QStringLiteral("#66727a"));
        m_inactiveTextColor = dark ? QColor(QStringLiteral("#747c84"))
                                   : QColor(QStringLiteral("#a5aaa9"));
        m_calendarBackground = embedded ? QColor(QStringLiteral("#f5f3ee"))
                                        : palette.color(QPalette::Base);
        m_headerBackground = embedded ? QColor(QStringLiteral("#e2e1dc"))
                                      : palette.color(QPalette::AlternateBase);
        m_cellBackground = m_calendarBackground;
        m_weekendBackground = dark ? QColor(QStringLiteral("#30363d"))
                                   : QColor(QStringLiteral("#e8edf1"));
        m_gridLineColor = embedded ? QColor(QStringLiteral("#cbc9c2"))
                                   : palette.color(QPalette::Mid);
        m_accentColor = embedded ? m_toolbarColor
                                 : palette.color(QPalette::Highlight);
        m_accentTextColor = embedded ? m_toolbarTextColor
                                     : palette.color(QPalette::HighlightedText);
        m_widget->setProperty("calendarGridLineColor", m_gridLineColor);
        m_widget->setProperty(
            "calendarCalendarBackground", m_calendarBackground);
        const int pixelSize = QFontInfo(m_widget->font()).pixelSize();
        if (pixelSize > 0)
        {
            m_baseFontPixelSize = pixelSize;
        }
        rebuild();
    }

    void setEventTypeColors(
        const QVariantMap& colors,
        const QVariantMap& textColors
        ) override
    {
        m_eventTypeColors = colors;
        m_eventTypeTextColors = textColors;
        rebuild();
    }

private:
    [[nodiscard]] int firstDayOfWeek() const
    {
        return m_academicProvider
            ? m_academicProvider->firstDayOfWeek()
            : (static_cast<int>(QLocale().firstDayOfWeek()) % 7);
    }

    [[nodiscard]] int weekColumnWidth() const
    {
        return qMax(54, QFontMetrics(m_widget->font()).horizontalAdvance(
            QCoreApplication::translate("EventCalendar", "Elem")) + 19);
    }

    [[nodiscard]] int eventDelegateHeight() const
    {
        const int fontPixelSize = qMax(1, qRound(m_baseFontPixelSize * 0.8));
        const int verticalPadding = qMax(4, qCeil(fontPixelSize / 3.0));
        return qMax(22, fontPixelSize + (verticalPadding * 2) + 8);
    }

    void notifyDisplayedMonthChanged() const
    {
        if (m_callbacks.displayedMonthChanged)
        {
            m_callbacks.displayedMonthChanged(
                m_shownMonth.year(), m_shownMonth.month());
        }
    }

    void setDisplayedMonth(const QDate& date)
    {
        if (!date.isValid())
        {
            return;
        }
        QDate next(date.year(), date.month(), 1);
        if (next < QDate(2026, 1, 1))
        {
            next = QDate(2026, 1, 1);
        }
        if (next == m_shownMonth)
        {
            return;
        }
        m_shownMonth = next;
        rebuild();
        notifyDisplayedMonthChanged();
    }

    void shiftMonth(const int delta)
    {
        setDisplayedMonth(m_shownMonth.addMonths(delta));
    }

    [[nodiscard]] QColor eventColor(
        const QVariantMap& event,
        const bool textColor
        ) const
    {
        const QString eventType = event.value(QStringLiteral("eventType"))
                                      .toString();
        const QVariantMap& colors = textColor
            ? m_eventTypeTextColors : m_eventTypeColors;
        const QColor color(colors.value(eventType).toString());
        if (color.isValid())
        {
            return color;
        }
        return textColor ? m_accentTextColor : m_accentColor;
    }

    void clearLayout(QLayout* layout)
    {
        if (!layout)
        {
            return;
        }
        while (QLayoutItem* const item = layout->takeAt(0))
        {
            if (QWidget* const widget = item->widget())
            {
                delete widget;
            }
            else if (QLayout* const childLayout = item->layout())
            {
                clearLayout(childLayout);
                delete childLayout;
            }
            delete item;
        }
    }

    void addWeekdayHeader(QGridLayout* grid, const int column, const int day)
    {
        static const char* const labels[] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };
        const int localeDay = day == 0 ? 7 : day;
        auto* const label = new QLabel(
            QCoreApplication::translate("EventCalendar", labels[day]), m_widget);
        label->setObjectName(QStringLiteral("calendarWidgetWeekdayHeader"));
        label->setAlignment(Qt::AlignCenter);
        label->setAccessibleName(label->text());
        label->setToolTip(QLocale().dayName(localeDay, QLocale::LongFormat));
        QPalette palette = label->palette();
        palette.setColor(QPalette::Window, m_headerBackground);
        palette.setColor(QPalette::WindowText, m_mutedTextColor);
        label->setPalette(palette);
        label->setAutoFillBackground(true);
        label->setStyleSheet(QStringLiteral(
            "QLabel { background-color: %1; color: %2; }")
            .arg(m_headerBackground.name(), m_mutedTextColor.name()));
        label->setMinimumHeight(40);
        grid->addWidget(label, 0, column + 1);
    }

    void addAcademicWeekColumn(
        QGridLayout* grid,
        const int column,
        const QString& headerText,
        const QString& headerTooltip,
        const QVariantList& weekRows,
        const QString& weekKey,
        const QString& tooltipKey,
        const int rowCount
        )
    {
        auto* const header = new QLabel(headerText, m_widget);
        header->setObjectName(QStringLiteral("calendarWidgetAcademicHeader"));
        header->setAlignment(Qt::AlignCenter);
        header->setAccessibleName(headerText);
        header->setToolTip(headerTooltip);
        header->setMinimumWidth(weekColumnWidth());
        header->setMinimumHeight(40);
        QPalette headerPalette = header->palette();
        headerPalette.setColor(QPalette::Window, m_headerBackground);
        headerPalette.setColor(QPalette::WindowText, m_mutedTextColor);
        header->setPalette(headerPalette);
        header->setAutoFillBackground(true);
        header->setStyleSheet(QStringLiteral(
            "QLabel { background-color: %1; color: %2; %3 }")
            .arg(m_headerBackground.name(), m_mutedTextColor.name(),
                 column == 0
                    ? QStringLiteral("border-right: 3px solid %1;")
                          .arg(m_gridLineColor.name())
                    : QStringLiteral("border-left: 3px solid %1;")
                          .arg(m_gridLineColor.name())));
        grid->addWidget(header, 0, column);

        for (int row = 0; row < rowCount; ++row)
        {
            const QVariantMap record = row < weekRows.size()
                ? weekRows.at(row).toMap() : QVariantMap{};
            const int weekNumber = record.value(weekKey).toInt();
            auto* const label = new QLabel(
                weekNumber > 0 ? QString::number(weekNumber)
                               : QStringLiteral("—"), m_widget);
            label->setObjectName(QStringLiteral("calendarWidgetAcademicWeek"));
            label->setAlignment(Qt::AlignCenter);
            label->setToolTip(record.value(tooltipKey).toString());
            label->setAccessibleName(label->toolTip().isEmpty()
                ? label->text() : label->toolTip());
            label->setMinimumWidth(weekColumnWidth());
            label->setMinimumHeight(52);
            QPalette palette = label->palette();
            palette.setColor(QPalette::Window, m_calendarBackground);
            palette.setColor(QPalette::WindowText, m_mutedTextColor);
            label->setPalette(palette);
            label->setAutoFillBackground(true);
            label->setStyleSheet(QStringLiteral(
                "QLabel { background-color: %1; color: %2; %3 }")
                .arg(m_calendarBackground.name(), m_mutedTextColor.name(),
                     column == 0
                        ? QStringLiteral("border-right: 3px solid %1;")
                              .arg(m_gridLineColor.name())
                        : QStringLiteral("border-left: 3px solid %1;")
                              .arg(m_gridLineColor.name())));
            grid->addWidget(label, row + 1, column);
        }
    }

    void addDayCell(
        QGridLayout* grid,
        const QDate& date,
        const int row,
        const int column
        )
    {
        const bool activeMonth = date.year() == m_shownMonth.year()
            && date.month() == m_shownMonth.month();
        QVariantList dayEvents;
        if (activeMonth && m_eventModel)
        {
            dayEvents = m_eventModel->eventsForDate(
                date.year(), date.month(), date.day()).toList();
        }
        bool holiday = false;
        for (const QVariant& value : dayEvents)
        {
            if (value.toMap().value(QStringLiteral("eventType")).toString()
                == QStringLiteral("Holiday"))
            {
                holiday = true;
                break;
            }
        }

        auto* const cell = new CalendarDayCellFrame(
            [this, date] {
                if (m_callbacks.dayActivated)
                {
                    m_callbacks.dayActivated(
                        date.year(), date.month(), date.day());
                }
            }, m_widget);
        cell->setObjectName(QStringLiteral("calendarWidgetDayCell"));
        cell->setAccessibleName(QLocale().toString(date, QLocale::LongFormat));
        cell->setAccessibleDescription(QCoreApplication::translate(
            "EventCalendar", "Activate to open this date"));
        cell->setFrameShape(QFrame::NoFrame);
        cell->setLineWidth(0);
        const bool weekend = date.dayOfWeek() == Qt::Saturday
            || date.dayOfWeek() == Qt::Sunday;
        const QColor background = !activeMonth ? m_gridLineColor
            : (holiday ? QColor(QStringLiteral("#f5d7d2"))
                       : (weekend ? m_weekendBackground : m_cellBackground));
        const QDate nextDate = date.addDays(1);
        const QDate lowerDate = date.addDays(7);
        const bool nextCellActive = nextDate.year() == m_shownMonth.year()
            && nextDate.month() == m_shownMonth.month();
        const bool lowerCellActive = lowerDate.year() == m_shownMonth.year()
            && lowerDate.month() == m_shownMonth.month();
        cell->setCalendarAppearance(
            background, m_gridLineColor, m_accentColor, activeMonth,
            column == 0, column == 6, nextCellActive, lowerCellActive);
        cell->setProperty("calendarDate", date);
        cell->setProperty("calendarBackground", background);
        cell->setProperty("calendarGridLineColor", m_gridLineColor);
        cell->setProperty("calendarHoliday", holiday);
        cell->setProperty("calendarWeekend", weekend);

        auto* const layout = new QVBoxLayout(cell);
        layout->setContentsMargins(2, 2, 2, 1);
        layout->setSpacing(2);
        auto* const dayButton = new QPushButton(
            activeMonth ? QString::number(date.day()) : QString{}, cell);
        dayButton->setObjectName(QStringLiteral("calendarWidgetDateButton"));
        dayButton->setProperty("calendarDate", date);
        dayButton->setProperty("calendarIsToday",
                               date == QDate::currentDate());
        dayButton->setAccessibleName(QLocale().toString(
            date, QLocale::LongFormat));
        dayButton->setToolTip(dayButton->accessibleName());
        dayButton->setFlat(true);
        dayButton->setFocusPolicy(Qt::StrongFocus);
        dayButton->setEnabled(activeMonth);
        QFont dayFont = dayButton->font();
        dayFont.setPixelSize(qMax(1, m_baseFontPixelSize));
        dayButton->setFont(dayFont);
        const QFontMetrics dayMetrics(dayFont);
        cell->setHolidayTint(
            holiday && date != QDate::currentDate(), dayMetrics.height() + 4);
        const int dayDiameter = qMax(
            dayMetrics.horizontalAdvance(dayButton->text()),
            dayMetrics.height()) + 8;
        dayButton->setFixedSize(dayDiameter, dayDiameter);
        QPalette dayPalette = dayButton->palette();
        dayPalette.setColor(QPalette::ButtonText,
            date == QDate::currentDate() ? m_accentTextColor
                : (activeMonth ? m_textColor : m_inactiveTextColor));
        dayPalette.setColor(QPalette::Button,
            date == QDate::currentDate() ? m_accentColor : background);
        dayButton->setPalette(dayPalette);
        if (date == QDate::currentDate())
        {
            dayButton->setStyleSheet(QStringLiteral(
                "QPushButton { border: 0px; border-radius: 15px; "
                "background: %1; color: %2; }")
                .arg(m_accentColor.name(), m_accentTextColor.name()));
        }
        else
        {
            dayButton->setStyleSheet(QStringLiteral(
                "QPushButton { border: 0px; background: transparent; color: %1; }")
                .arg(activeMonth ? m_textColor.name()
                                 : m_inactiveTextColor.name()));
        }
        layout->addWidget(dayButton, 0, Qt::AlignHCenter);
        QObject::connect(dayButton, &QPushButton::clicked,
            m_widget, [this, date] {
                if (m_callbacks.dayActivated)
                {
                    m_callbacks.dayActivated(
                        date.year(), date.month(), date.day());
                }
            });

        if (!dayEvents.isEmpty())
        {
            auto* const eventScroll = new QScrollArea(cell);
            eventScroll->setObjectName(QStringLiteral(
                "calendarWidgetEventScrollArea"));
            eventScroll->setProperty("calendarDate", date);
            eventScroll->setFrameShape(QFrame::NoFrame);
            eventScroll->setWidgetResizable(true);
            eventScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            QPalette eventPalette = eventScroll->palette();
            eventPalette.setColor(QPalette::Window, background);
            eventPalette.setColor(QPalette::Base, background);
            eventScroll->setPalette(eventPalette);
            eventScroll->setAutoFillBackground(true);
            eventScroll->viewport()->setPalette(eventPalette);
            eventScroll->viewport()->setAutoFillBackground(true);
            auto* const eventContent = new QWidget(eventScroll);
            eventContent->setObjectName(QStringLiteral(
                "calendarWidgetEventContent"));
            eventContent->setPalette(eventPalette);
            eventContent->setAutoFillBackground(true);
            auto* const eventLayout = new QVBoxLayout(eventContent);
            eventLayout->setContentsMargins(0, 0, 0, 0);
            eventLayout->setSpacing(2);
            for (const QVariant& eventValue : dayEvents)
            {
                const QVariantMap event = eventValue.toMap();
                const QString eventId = event.value(QStringLiteral("id"))
                                            .toString();
                const QString title = event.value(QStringLiteral("title"))
                                          .toString();
                auto* const eventButton = new CalendarEventButton(
                    title,
                    eventColor(event, false),
                    eventColor(event, true),
                    eventContent);
                eventButton->setObjectName(QStringLiteral(
                    "calendarWidgetEventButton"));
                eventButton->setProperty("calendarEventId", eventId);
                eventButton->setProperty("calendarDate", date);
                eventButton->setProperty(
                    "calendarEventBackground", eventColor(event, false));
                eventButton->setProperty(
                    "calendarEventForeground", eventColor(event, true));
                eventButton->setAccessibleName(
                    QCoreApplication::translate("EventCalendar", "%1 on %2")
                        .arg(title, QLocale().toString(
                            date, QLocale::LongFormat)));
                eventButton->setToolTip(title);
                eventButton->setFocusPolicy(Qt::StrongFocus);
                QFont eventFont = eventButton->font();
                eventFont.setPixelSize(qMax(1, qRound(m_baseFontPixelSize * 0.8)));
                eventButton->setFont(eventFont);
                eventButton->setMinimumHeight(eventDelegateHeight());
                eventButton->setMaximumHeight(eventDelegateHeight());
                QObject::connect(eventButton, &QPushButton::clicked,
                    m_widget, [this, eventId] {
                        if (m_callbacks.eventActivated)
                        {
                            m_callbacks.eventActivated(eventId);
                        }
                    });
                eventLayout->addWidget(eventButton);
                ++m_eventButtonCount;
            }
            eventLayout->addStretch(1);
            eventScroll->setWidget(eventContent);
            layout->addWidget(eventScroll, 1);
        }
        else
        {
            layout->addStretch(1);
        }

        grid->addWidget(cell, row, column + 1);
        ++m_dayCellCount;
    }

    void rebuild()
    {
        if (!m_widget || !m_eventModel || !m_academicProvider)
        {
            m_loaded = false;
            return;
        }
        if (QLayout* const oldLayout = m_widget->layout())
        {
            clearLayout(oldLayout);
            delete oldLayout;
        }
        m_dayCellCount = 0;
        m_eventButtonCount = 0;
        const int firstDay = firstDayOfWeek();
        const int jsWeekday = m_shownMonth.dayOfWeek() % 7;
        const int firstOffset = (jsWeekday - firstDay + 7) % 7;
        const int rowCount = (firstOffset + m_shownMonth.daysInMonth() + 6) / 7;

        auto* const mainLayout = new QVBoxLayout(m_widget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        auto* const toolbar = new QWidget(m_widget);
        toolbar->setObjectName(QStringLiteral("calendarWidgetMonthToolbar"));
        toolbar->setMinimumHeight(48);
        QPalette toolbarPalette = toolbar->palette();
        toolbarPalette.setColor(QPalette::Window, m_toolbarColor);
        toolbarPalette.setColor(QPalette::WindowText, m_toolbarTextColor);
        toolbar->setPalette(toolbarPalette);
        toolbar->setAutoFillBackground(true);
        auto* const toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setContentsMargins(8, 0, 8, 0);
        toolbarLayout->setSpacing(8);

        auto* const previous = new QToolButton(toolbar);
        previous->setObjectName(QStringLiteral("calendarWidgetPreviousMonth"));
        previous->setText(QStringLiteral("\u2039"));
        previous->setAccessibleName(QCoreApplication::translate(
            "EventCalendar", "Previous month"));
        previous->setToolTip(previous->accessibleName());
        previous->setAutoRaise(true);
        previous->setEnabled(m_shownMonth > QDate(2026, 1, 1));
        QPalette arrowPalette = previous->palette();
        arrowPalette.setColor(QPalette::ButtonText, m_toolbarTextColor);
        previous->setPalette(arrowPalette);
        QObject::connect(previous, &QToolButton::clicked,
            m_widget, [this] { shiftMonth(-1); });

        QFont titleFont = toolbar->font();
        titleFont.setPixelSize(qMax(1, qRound(m_baseFontPixelSize * 1.12)));
        auto* const title = new CalendarMonthTitleLabel(
            m_academicProvider->monthTitle(
                m_shownMonth.year(), m_shownMonth.month() - 1),
            titleFont, toolbar);
        title->setObjectName(QStringLiteral("calendarWidgetMonthTitle"));
        QPalette titlePalette = title->palette();
        titlePalette.setColor(QPalette::WindowText, m_toolbarTextColor);
        title->setPalette(titlePalette);

        auto* const next = new QToolButton(toolbar);
        next->setObjectName(QStringLiteral("calendarWidgetNextMonth"));
        next->setText(QStringLiteral("\u203a"));
        next->setAccessibleName(QCoreApplication::translate(
            "EventCalendar", "Next month"));
        next->setToolTip(next->accessibleName());
        next->setAutoRaise(true);
        next->setPalette(arrowPalette);
        QObject::connect(next, &QToolButton::clicked,
            m_widget, [this] { shiftMonth(1); });
        toolbarLayout->addWidget(previous);
        toolbarLayout->addWidget(title, 1);
        toolbarLayout->addWidget(next);
        mainLayout->addWidget(toolbar);

        auto* const body = new QWidget(m_widget);
        body->setObjectName(QStringLiteral("calendarWidgetMonthGrid"));
        auto* const grid = new QGridLayout(body);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(0);
        grid->setVerticalSpacing(0);
        const int weekWidth = weekColumnWidth();
        grid->setColumnMinimumWidth(0, weekWidth);
        grid->setColumnMinimumWidth(8, weekWidth);
        for (int column = 0; column < 7; ++column)
        {
            addWeekdayHeader(grid, column, (firstDay + column) % 7);
            grid->setColumnStretch(column + 1, 1);
        }

        const QVariantList weekRows = m_academicProvider->weekRows(
            m_shownMonth.year(), m_shownMonth.month() - 1, firstDay);
        addAcademicWeekColumn(
            grid, 0, QCoreApplication::translate("EventCalendar", "Elem"),
            QCoreApplication::translate(
                "EventCalendar", "Elementary academic week"),
            weekRows, QStringLiteral("elementaryWeek"),
            QStringLiteral("elementaryTooltip"), rowCount);
        addAcademicWeekColumn(
            grid, 8, QCoreApplication::translate("EventCalendar", "MS"),
            QCoreApplication::translate(
                "EventCalendar", "Middle School academic week"),
            weekRows, QStringLiteral("middleWeek"),
            QStringLiteral("middleTooltip"), rowCount);

        for (int row = 0; row < rowCount; ++row)
        {
            grid->setRowStretch(row + 1, 1);
            for (int column = 0; column < 7; ++column)
            {
                const int index = row * 7 + column;
                const QDate date = m_shownMonth.addDays(index - firstOffset);
                addDayCell(grid, date, row + 1, column);
            }
        }
        if (m_eventModel->isLoading()
            && !m_eventModel->isMonthLoaded(
                m_shownMonth.year(), m_shownMonth.month()))
        {
            auto* const loading = new QLabel(
                QCoreApplication::translate("EventCalendar", "Loading events…"),
                body);
            loading->setObjectName(QStringLiteral(
                "calendarWidgetLoadingOverlay"));
            loading->setAlignment(Qt::AlignCenter);
            loading->setAttribute(Qt::WA_TransparentForMouseEvents);
            loading->setStyleSheet(QStringLiteral(
                "QLabel { background: %1; color: %2; border: 1px solid %3; "
                "border-radius: 4px; padding: 10px; }")
                .arg(m_calendarBackground.name(), m_textColor.name(),
                     m_gridLineColor.name()));
            grid->addWidget(loading, 1, 1, rowCount, 7, Qt::AlignCenter);
        }
        mainLayout->addWidget(body, 1);
        m_loaded = m_dayCellCount == rowCount * 7;
        m_widget->setProperty("calendarShownMonth", m_shownMonth);
        m_widget->setProperty("calendarDayCellCount", m_dayCellCount);
        m_widget->setProperty("calendarEventButtonCount", m_eventButtonCount);
    }

    QWidget* m_widget = nullptr;
    CalendarEventModel* m_eventModel = nullptr;
    AcademicCalendarProvider* m_academicProvider = nullptr;
    CalendarViewCallbacks m_callbacks;
    QDate m_shownMonth;
    bool m_embedded = false;
    bool m_loaded = false;
    int m_baseFontPixelSize = 14;
    int m_dayCellCount = 0;
    int m_eventButtonCount = 0;
    QColor m_toolbarColor{QStringLiteral("#536f8a")};
    QColor m_toolbarTextColor{QStringLiteral("#fbfaf7")};
    QColor m_textColor{QStringLiteral("#27313a")};
    QColor m_mutedTextColor{QStringLiteral("#66727a")};
    QColor m_inactiveTextColor{QStringLiteral("#a5aaa9")};
    QColor m_calendarBackground{QStringLiteral("#f5f3ee")};
    QColor m_headerBackground{QStringLiteral("#e2e1dc")};
    QColor m_cellBackground{QStringLiteral("#f5f3ee")};
    QColor m_weekendBackground{QStringLiteral("#e8edf1")};
    QColor m_gridLineColor{QStringLiteral("#cbc9c2")};
    QColor m_accentColor{QStringLiteral("#536f8a")};
    QColor m_accentTextColor{QStringLiteral("#fbfaf7")};
    QVariantMap m_eventTypeColors;
    QVariantMap m_eventTypeTextColors;
};


} // namespace

CalendarViewCreationResult createWidgetCalendarView(
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

    auto view = std::make_unique<WidgetCalendarView>(request);
    if (!view->isLoaded())
    {
        return {
            nullptr,
            QCoreApplication::translate(
                "CalendarPage", "The calendar grid could not be loaded.")
        };
    }
    return {std::move(view), {}};
}

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
