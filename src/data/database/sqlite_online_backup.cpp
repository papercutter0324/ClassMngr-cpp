#include "data/database/sqlite_online_backup.h"

#include <sqlite3.h>

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>

namespace
{
class SqliteConnection final
{
public:
    SqliteConnection() = default;
    SqliteConnection(const SqliteConnection&) = delete;
    SqliteConnection& operator=(const SqliteConnection&) = delete;

    ~SqliteConnection()
    {
        if (m_database)
        {
            (void) sqlite3_close_v2(m_database);
        }
    }

    [[nodiscard]] sqlite3** out() noexcept
    {
        return &m_database;
    }

    [[nodiscard]] sqlite3* get() const noexcept
    {
        return m_database;
    }

private:
    sqlite3* m_database = nullptr;
};

[[nodiscard]] QString sqliteFailure(
    const char* operation,
    const int resultCode,
    sqlite3* database
    )
{
    const char* message = database
        ? sqlite3_errmsg(database)
        : sqlite3_errstr(resultCode);
    return QStringLiteral("SQLite %1 failed (%2): %3")
        .arg(
            QString::fromLatin1(operation),
            QString::number(resultCode),
            QString::fromUtf8(message ? message : "unknown error")
            );
}
}

Status DatabaseFileOperations::Internal::createOnlineBackupSnapshot(
    const QString& sourcePath,
    const QString& stagingPath
    )
{
    const QByteArray sourcePathUtf8 = sourcePath.toUtf8();
    const QByteArray stagingPathUtf8 = stagingPath.toUtf8();
    if (
        sourcePath.trimmed().isEmpty()
        || stagingPath.trimmed().isEmpty()
        || sourcePathUtf8.contains('\0')
        || stagingPathUtf8.contains('\0')
        )
    {
        return std::unexpected(
            QStringLiteral("A SQLite snapshot path is empty or invalid.")
            );
    }

    SqliteConnection source;
    int resultCode = sqlite3_open_v2(
        sourcePathUtf8.constData(),
        source.out(),
        SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX,
        nullptr
        );
    if (resultCode != SQLITE_OK)
    {
        return std::unexpected(
            sqliteFailure("source open", resultCode, source.get())
            );
    }

    SqliteConnection destination;
    resultCode = sqlite3_open_v2(
        stagingPathUtf8.constData(),
        destination.out(),
        SQLITE_OPEN_READWRITE
            | SQLITE_OPEN_CREATE
            | SQLITE_OPEN_EXCLUSIVE
            | SQLITE_OPEN_FULLMUTEX,
        nullptr
        );
    if (resultCode != SQLITE_OK)
    {
        return std::unexpected(
            sqliteFailure("staging open", resultCode, destination.get())
            );
    }

    sqlite3_backup* const backup = sqlite3_backup_init(
        destination.get(),
        "main",
        source.get(),
        "main"
        );
    if (!backup)
    {
        return std::unexpected(
            sqliteFailure(
                "backup initialization",
                sqlite3_errcode(destination.get()),
                destination.get()
                )
            );
    }

    constexpr int kPagesPerStep = 128;
    constexpr qint64 kMaximumBusyWaitMilliseconds = 5000;
    constexpr unsigned int kBusyWaitMilliseconds = 10;
    QElapsedTimer busyTimer;

    int stepResult = SQLITE_OK;
    while (stepResult == SQLITE_OK)
    {
        stepResult = sqlite3_backup_step(backup, kPagesPerStep);
        if (
            stepResult == SQLITE_BUSY
            || stepResult == SQLITE_LOCKED
            )
        {
            if (!busyTimer.isValid())
            {
                busyTimer.start();
            }
            if (busyTimer.elapsed() >= kMaximumBusyWaitMilliseconds)
            {
                break;
            }

            (void) sqlite3_sleep(
                static_cast<int>(kBusyWaitMilliseconds)
                );
            stepResult = SQLITE_OK;
        }
        else
        {
            busyTimer.invalidate();
        }
    }

    const int finishResult = sqlite3_backup_finish(backup);
    if (stepResult != SQLITE_DONE)
    {
        const int failureCode = stepResult == SQLITE_OK
            ? finishResult
            : stepResult;
        return std::unexpected(
            sqliteFailure(
                "backup step",
                failureCode,
                destination.get()
                )
            );
    }
    if (finishResult != SQLITE_OK)
    {
        return std::unexpected(
            sqliteFailure(
                "backup finalization",
                finishResult,
                destination.get()
                )
            );
    }

    return {};
}
