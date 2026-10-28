#pragma once

#include "next/application/calendar_event_import_count.h"

#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;

namespace ClassMngr::Next::Ui
{

struct CalendarEventImportRuntimeMetrics final
{
    qint64 downloadedBytes = 0;
    qint64 largestTransferChunkBytes = 0;
    int transferChunkCount = 0;
    int streamedWorksheetCellCount = 0;
    int materializedWorksheetCellCount = 0;
    int parsedEventCount = 0;
    int typedCandidateCountAtApply = 0;
    int legacyEventCountAtApply = 0;
    bool usedDeviceParser = false;
    bool stagedWorkbookReleasedBeforeApply = false;
    bool stagedWorkbookRemovedOnCancel = false;
    bool stagedWorkbookRemovedAtTerminal = false;
};

// Owns the short-lived download, staged workbook, parser output conversion,
// and typed path-scoped apply. Its owner cancels it when Calendar suspends.
class NextCalendarEventImportService final : public QObject
{
    Q_OBJECT

public:
    using ActiveWorkspacePath = std::function<QString()>;
    using CampusCodeProvider = std::function<QStringList()>;
    using NetworkAccessManagerFactory =
        std::function<QNetworkAccessManager*(QObject*)>;

    explicit NextCalendarEventImportService(
        ActiveWorkspacePath activeWorkspacePath,
        CampusCodeProvider campusCodeProvider,
        QString sourceUrlOverride = {},
        QString temporaryFileTemplate = {},
        NetworkAccessManagerFactory networkAccessManagerFactory = {},
        QObject* parent = nullptr
        );
    ~NextCalendarEventImportService() override;

    [[nodiscard]] static QString defaultImportUrl();
    [[nodiscard]] bool isImporting() const noexcept;
    [[nodiscard]] CalendarEventImportRuntimeMetrics runtimeMetrics() const;

    void start(const QString& databasePath);
    void cancel() noexcept;

signals:
    void importingChanged(bool importing);
    void importFinished(int importedCount, int skippedCount);
    void importFailed(const QString& message);
    void importCanceled();

private slots:
    void handleFinished(QNetworkReply* reply);

private:
    void handleReadyRead(QNetworkReply* reply);
    void setImporting(bool importing);
    void fail(const QString& message);
    void canceled();
    [[nodiscard]] bool operationIsCurrent() const noexcept;
    [[nodiscard]] QString sourceUrl() const;

    ActiveWorkspacePath m_activeWorkspacePath;
    CampusCodeProvider m_campusCodeProvider;
    QString m_sourceUrlOverride;
    QString m_temporaryFileTemplate;
    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
    std::unique_ptr<QTemporaryFile> m_stagedWorkbook;
    QString m_stagedWorkbookPath;
    QString m_databasePath;
    QString m_stagingError;
    CalendarEventImportRuntimeMetrics m_metrics;
    bool m_importing = false;
    bool m_cancelRequested = false;
};

} // namespace ClassMngr::Next::Ui
