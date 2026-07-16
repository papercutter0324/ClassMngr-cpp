#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class DataService;

class CalendarEventModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit CalendarEventModel(
        DataService* dataService,
        QObject* parent = nullptr
        );

    int revision() const;

    Q_INVOKABLE QVariantList eventsForDate(
        int year,
        int month,
        int day
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

private:
    DataService* m_dataService = nullptr;
    QStringList m_currentCampusCodes;
    QStringList m_allCampusCodes;
    bool m_showAllCampuses = false;
    bool m_hideStartOfTermEvents = false;
    int m_revision = 0;
};
