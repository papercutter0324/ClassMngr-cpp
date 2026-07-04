#pragma once

#include <QObject>

class DataService;
class QNetworkAccessManager;
class QNetworkReply;

class CalendarEventImportService : public QObject
{
    Q_OBJECT

public:
    explicit CalendarEventImportService(
        DataService* dataService,
        QObject* parent = nullptr
        );

    static QString defaultImportUrl();
    bool isImporting() const;

public slots:
    void importFromDefaultSource();

signals:
    void importFinished(
        int importedCount,
        int skippedCount
        );
    void importFailed(
        const QString& message
        );

private:
    void handleFinished(
        QNetworkReply* reply
        );

    DataService* m_dataService = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    bool m_importing = false;
};
