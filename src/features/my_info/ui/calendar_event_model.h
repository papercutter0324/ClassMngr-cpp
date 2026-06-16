#pragma once

#include <QObject>
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

    void reload();

signals:
    void revisionChanged();

private:
    DataService* m_dataService = nullptr;
    int m_revision = 0;
};
