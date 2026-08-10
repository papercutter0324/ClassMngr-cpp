#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

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

    void setCampusFilter(
        const QStringList& currentCampusCodes,
        const QStringList& allCampusCodes,
        bool showAllCampuses,
        bool hideStartOfTermEvents
        );

    void reload();

signals:
    void revisionChanged();
    void loadingChanged();

private:
    CalendarEventCache* m_cache = nullptr;
    QStringList m_currentCampusCodes;
    QStringList m_allCampusCodes;
    bool m_showAllCampuses = false;
    bool m_hideStartOfTermEvents = false;
    int m_revision = 0;
};
