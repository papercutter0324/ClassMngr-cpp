#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

#include <atomic>
#include <cstdint>

namespace ClassMngr::Next::Bootstrap::Diagnostics
{

inline void writeShutdownTrace(
    const char* const milestone,
    QJsonObject details = {}
    ) noexcept
{
    try
    {
        const QString path = qEnvironmentVariable(
            "CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH"
            );
        if (path.isEmpty())
        {
            return;
        }

        static QMutex mutex;
        static std::atomic<std::uint64_t> nextSequence{0};
        QMutexLocker lock(&mutex);

        details.insert(
            QStringLiteral("event"),
            QString::fromLatin1(milestone)
            );
        details.insert(
            QStringLiteral("captured_at_utc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
            );
        details.insert(
            QStringLiteral("process_id"),
            QCoreApplication::applicationPid()
            );
        details.insert(
            QStringLiteral("sequence"),
            static_cast<qint64>(nextSequence.fetch_add(1, std::memory_order_relaxed))
            );

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            return;
        }
        file.write(QJsonDocument(details).toJson(QJsonDocument::Compact));
        file.write("\n");
        file.flush();
    }
    catch (...)
    {
        // The opt-in diagnostic must never change shutdown behavior.
    }
}

}  // namespace ClassMngr::Next::Bootstrap::Diagnostics
