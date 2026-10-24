#pragma once

#include <QObject>
#include <QHash>

class ApplicationServices;
class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;

class CalendarEventImportService : public QObject
{
    Q_OBJECT

public:
    explicit CalendarEventImportService(
        ApplicationServices* services,
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
    void handleReadyRead(
        QNetworkReply* reply
        );

    void handleFinished(
        QNetworkReply* reply
        );

    ApplicationServices* m_services = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    QHash<QNetworkReply*, QTemporaryFile*> m_stagedWorkbooks;
    QHash<QNetworkReply*, QString> m_stagingErrors;
    bool m_importing = false;
};
