#pragma once

#include <QDate>
#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <vector>

namespace ClassMngr::Next::Application
{
class CalendarEventProjection;
class CalendarEventSummary;
}

class CalendarEventCache;

class CalendarEventModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)

public:
    explicit CalendarEventModel(
        CalendarEventCache* cache,
        QObject* parent = nullptr
        );

    int revision() const;
    bool isLoading() const;

    Q_INVOKABLE QVariantList eventsForDate(
        int year,
        int month,
        int day
        ) const;
    Q_INVOKABLE bool isMonthLoaded(
        int year,
        int month
        ) const;

    [[nodiscard]] ClassMngr::Next::Application::CalendarEventProjection
    visibleEventsInRange(
        const QDate& startDate,
        const QDate& endDate
        ) const;

    void setCampusFilter(
        const QStringList& currentCampusCodes,
        const QStringList& allCampusCodes,
        bool showAllCampuses,
        bool hideStartOfTermEvents
        );

    void setActiveEventTypes(const QStringList& eventTypes);

    void reload();

signals:
    void revisionChanged();
    void loadingChanged();

private:
    [[nodiscard]] bool isVisible(
        const ClassMngr::Next::Application::CalendarEventSummary& event
        ) const;

    CalendarEventCache* m_cache = nullptr;
    QStringList m_currentCampusCodes;
    QStringList m_allCampusCodes;
    bool m_showAllCampuses = false;
    bool m_hideStartOfTermEvents = false;
    QStringList m_activeEventTypes;
    bool m_activeEventTypesSet = false;
    int m_revision = 0;
};
