#pragma once

#include <QDate>
#include <QString>
#include <QVariantMap>

#include <functional>
#include <memory>

class AcademicCalendarProvider;
class CalendarEventModel;
class QPalette;
class QWidget;

namespace ClassMngr::Next::Ui
{

struct CalendarViewCallbacks final
{
    std::function<void(int year, int month, int day)> dayActivated;
    std::function<void(const QString& eventId)> eventActivated;
    std::function<void(int year, int month)> displayedMonthChanged;
};

struct CalendarViewRequest final
{
    QWidget* parent = nullptr;
    // Borrowed for the lifetime of the created view. The caller destroys the
    // view before either provider.
    CalendarEventModel* eventModel = nullptr;
    AcademicCalendarProvider* academicProvider = nullptr;
    QDate shownMonth;
    bool embedded = false;
    CalendarViewCallbacks callbacks;
};

class CalendarView
{
public:
    virtual ~CalendarView() = default;

    [[nodiscard]] virtual QWidget* widget() const noexcept = 0;
    [[nodiscard]] virtual bool isLoaded() const noexcept = 0;
    virtual void applyPalette(const QPalette& palette, bool embedded) = 0;
    virtual void setEventTypeColors(
        const QVariantMap& colors,
        const QVariantMap& textColors
        ) = 0;
};

struct CalendarViewCreationResult final
{
    std::unique_ptr<CalendarView> view;
    QString error;
};

using CalendarViewFactory = std::function<CalendarViewCreationResult(
    const CalendarViewRequest& request)>;

[[nodiscard]] CalendarViewCreationResult createQmlCalendarView(
    const CalendarViewRequest& request
    );

} // namespace ClassMngr::Next::Ui
