#include "qt_platform_services.h"

#include "core/updater/update_signature_verifier.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QTimeZone>

#include <algorithm>
#include <cstring>
#include <limits>
#include <type_traits>

namespace classmngr::qt
{
namespace
{
using engine::ByteBuffer;
using engine::Error;
using engine::ErrorCode;

Error error(
    ErrorCode code,
    const QString& message,
    std::optional<int> nativeCode = std::nullopt
    )
{
    const QByteArray utf8 = message.toUtf8();
    return Error{
        code,
        std::string(utf8.constData(), static_cast<std::size_t>(utf8.size())),
        nativeCode
    };
}

QString fromUtf8(std::string_view value)
{
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return {};
    }

    return QString::fromUtf8(
        value.data(),
        static_cast<int>(value.size())
        );
}

ByteBuffer toBytes(const QByteArray& value)
{
    ByteBuffer result;
    result.resize(static_cast<std::size_t>(value.size()));

    if (!value.isEmpty())
    {
        std::memcpy(
            result.data(),
            value.constData(),
            static_cast<std::size_t>(value.size())
            );
    }

    return result;
}

QByteArray fromBytes(const ByteBuffer& value)
{
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return {};
    }

    return QByteArray(
        reinterpret_cast<const char*>(value.data()),
        static_cast<int>(value.size())
        );
}

bool validKey(std::string_view key)
{
    return !key.empty()
        && key.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max());
}

std::optional<int> timeoutMilliseconds(
    std::chrono::milliseconds timeout
    )
{
    if (
        timeout.count() < 0
        || timeout.count() > std::numeric_limits<int>::max()
        )
    {
        return std::nullopt;
    }

    return static_cast<int>(timeout.count());
}

engine::Result<QVariant> toQVariant(
    const engine::PlatformSettingValue& value
    )
{
    return std::visit(
        [](const auto& item) -> engine::Result<QVariant>
        {
            using Value = std::decay_t<decltype(item)>;

            if constexpr (std::is_same_v<Value, std::monostate>)
            {
                return QVariant();
            }
            else if constexpr (std::is_same_v<Value, bool>)
            {
                return QVariant(item);
            }
            else if constexpr (std::is_same_v<Value, std::int64_t>)
            {
                return QVariant::fromValue<qlonglong>(item);
            }
            else if constexpr (std::is_same_v<Value, double>)
            {
                return QVariant(item);
            }
            else if constexpr (std::is_same_v<Value, std::string>)
            {
                if (item.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                {
                    return std::unexpected(error(
                        ErrorCode::NumericOverflow,
                        QStringLiteral("The setting value is too large.")
                        ));
                }
                return QString::fromUtf8(item.data(), static_cast<int>(item.size()));
            }
            else
            {
                const QByteArray bytes = fromBytes(item);
                if (item.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                {
                    return std::unexpected(error(
                        ErrorCode::NumericOverflow,
                        QStringLiteral("The setting value is too large.")
                        ));
                }
                return bytes;
            }
        },
        value
        );
}

engine::Result<engine::PlatformSettingValue> fromQVariant(
    const QVariant& value
    )
{
    if (!value.isValid())
    {
        return engine::PlatformSettingValue{std::monostate{}};
    }

    switch (value.metaType().id())
    {
    case QMetaType::Bool:
        return engine::PlatformSettingValue{value.toBool()};
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    {
        bool ok = false;
        const qlonglong converted = value.toLongLong(&ok);
        if (!ok)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                QStringLiteral("The setting integer value is invalid.")
                ));
        }
        if (
            value.metaType().id() == QMetaType::ULongLong
            && value.toULongLong() > static_cast<qulonglong>(std::numeric_limits<std::int64_t>::max())
            )
        {
            return std::unexpected(error(
                ErrorCode::NumericOverflow,
                QStringLiteral("The setting integer value is out of range.")
                ));
        }
        return engine::PlatformSettingValue{
            static_cast<std::int64_t>(converted)
        };
    }
    case QMetaType::Float:
    case QMetaType::Double:
    {
        bool ok = false;
        const double converted = value.toDouble(&ok);
        return ok
            ? engine::PlatformSettingValue{converted}
            : engine::Result<engine::PlatformSettingValue>{std::unexpected(error(
                ErrorCode::InvalidFormat,
                QStringLiteral("The setting floating-point value is invalid.")
                ))};
    }
    case QMetaType::QString:
    {
        const QByteArray utf8 = value.toString().toUtf8();
        return engine::PlatformSettingValue{
            std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()))
        };
    }
    case QMetaType::QByteArray:
        return engine::PlatformSettingValue{toBytes(value.toByteArray())};
    default:
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            QStringLiteral("The setting has an unsupported value type.")
            ));
    }
}

engine::Status syncSettings(QSettings& settings)
{
    settings.sync();
    switch (settings.status())
    {
    case QSettings::NoError:
        return {};
    case QSettings::AccessError:
        return std::unexpected(error(
            ErrorCode::Io,
            QStringLiteral("Unable to access the settings store.")
            ));
    case QSettings::FormatError:
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            QStringLiteral("The settings store has an invalid format.")
            ));
    }
    return std::unexpected(error(
        ErrorCode::Internal,
        QStringLiteral("The settings store returned an unknown status.")
        ));
}

engine::Result<engine::NetworkResponse> networkError(
    ErrorCode code,
    const QString& message,
    std::optional<int> nativeCode = std::nullopt
    )
{
    return std::unexpected(error(code, message, nativeCode));
}

} // namespace

QtSettingsStore::QtSettingsStore(QSettings& settings)
    : m_settings(settings)
{
}

engine::Result<std::optional<engine::PlatformSettingValue>> QtSettingsStore::read(
    std::string_view key
    ) const
{
    if (!validKey(key))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The settings key is invalid.")
            ));
    }

    const QString qtKey = fromUtf8(key);
    if (qtKey.isEmpty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The settings key is not valid UTF-8.")
            ));
    }
    if (!m_settings.contains(qtKey))
    {
        return std::optional<engine::PlatformSettingValue>{};
    }

    const auto converted = fromQVariant(m_settings.value(qtKey));
    if (!converted)
    {
        return std::unexpected(converted.error());
    }
    return std::optional<engine::PlatformSettingValue>{*converted};
}

engine::Status QtSettingsStore::write(
    std::string_view key,
    const engine::PlatformSettingValue& value
    )
{
    if (!validKey(key))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The settings key is invalid.")
            ));
    }

    const QString qtKey = fromUtf8(key);
    if (qtKey.isEmpty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The settings key is not valid UTF-8.")
            ));
    }
    const auto converted = toQVariant(value);
    if (!converted)
    {
        return std::unexpected(converted.error());
    }

    m_settings.setValue(qtKey, *converted);
    return syncSettings(m_settings);
}

engine::Status QtSettingsStore::remove(std::string_view key)
{
    if (!validKey(key))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The settings key is invalid.")
            ));
    }

    const QString qtKey = fromUtf8(key);
    if (qtKey.isEmpty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The settings key is not valid UTF-8.")
            ));
    }
    m_settings.remove(qtKey);
    return syncSettings(m_settings);
}

QtNetworkClient::QtNetworkClient(QNetworkAccessManager& manager)
    : m_manager(manager)
{
}

engine::Result<engine::NetworkResponse> QtNetworkClient::request(
    const engine::NetworkRequest& request
    )
{
    const auto timeout = timeoutMilliseconds(request.timeout);
    if (!timeout)
    {
        return networkError(
            ErrorCode::InvalidArgument,
            QStringLiteral("The network request timeout is invalid.")
            );
    }
    if (request.url.empty() || request.url.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return networkError(
            ErrorCode::InvalidArgument,
            QStringLiteral("The network request URL is invalid.")
            );
    }
    const QUrl url = QUrl::fromEncoded(
        QByteArray(request.url.data(), static_cast<int>(request.url.size())),
        QUrl::StrictMode
        );
    if (!url.isValid() || url.scheme().isEmpty())
    {
        return networkError(
            ErrorCode::InvalidArgument,
            QStringLiteral("The network request URL is invalid.")
            );
    }
    if (
        request.method.empty()
        || request.method.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
        )
    {
        return networkError(
            ErrorCode::InvalidArgument,
            QStringLiteral("The network request method is empty.")
            );
    }
    if (request.cancellation && request.cancellation->isCancellationRequested())
    {
        return networkError(
            ErrorCode::Cancelled,
            QStringLiteral("The network request was cancelled.")
            );
    }

    QNetworkRequest qtRequest(url);
    for (const auto& [name, value] : request.headers)
    {
        if (
            name.empty()
            || name.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
            || value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
            )
        {
            return networkError(
                ErrorCode::InvalidArgument,
                QStringLiteral("A network request header is invalid.")
                );
        }
        qtRequest.setRawHeader(
            QByteArray(name.data(), static_cast<int>(name.size())),
            QByteArray(value.data(), static_cast<int>(value.size()))
            );
    }

    if (request.body.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return networkError(
            ErrorCode::NumericOverflow,
            QStringLiteral("The network request body is too large.")
            );
    }

    const QByteArray method(request.method.data(), static_cast<int>(request.method.size()));
    QNetworkReply* reply = nullptr;
    if (method.compare("GET", Qt::CaseInsensitive) == 0 && request.body.empty())
    {
        reply = m_manager.get(qtRequest);
    }
    else if (method.compare("POST", Qt::CaseInsensitive) == 0)
    {
        reply = m_manager.post(qtRequest, fromBytes(request.body));
    }
    else if (method.compare("PUT", Qt::CaseInsensitive) == 0)
    {
        reply = m_manager.put(qtRequest, fromBytes(request.body));
    }
    else if (method.compare("DELETE", Qt::CaseInsensitive) == 0 && request.body.empty())
    {
        reply = m_manager.deleteResource(qtRequest);
    }
    else
    {
        reply = m_manager.sendCustomRequest(qtRequest, method, fromBytes(request.body));
    }
    if (reply == nullptr)
    {
        return networkError(
            ErrorCode::Internal,
            QStringLiteral("The network request could not be created.")
            );
    }

    QEventLoop loop;
    QTimer cancellationTimer;
    QTimer timeoutTimer;
    bool cancelled = false;
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(
        &cancellationTimer,
        &QTimer::timeout,
        [&]
        {
            if (request.cancellation && request.cancellation->isCancellationRequested())
            {
                cancelled = true;
                reply->abort();
                loop.quit();
            }
        }
        );
    QObject::connect(
        &timeoutTimer,
        &QTimer::timeout,
        [&]
        {
            timedOut = true;
            reply->abort();
            loop.quit();
        }
        );
    cancellationTimer.setInterval(10);
    cancellationTimer.start();
    timeoutTimer.setSingleShot(true);
    timeoutTimer.start(*timeout);
    if (!reply->isFinished())
    {
        loop.exec();
    }
    cancellationTimer.stop();
    timeoutTimer.stop();

    const auto networkStatus = reply->error();
    const QString networkMessage = reply->errorString();
    if (cancelled)
    {
        reply->deleteLater();
        return networkError(
            ErrorCode::Cancelled,
            QStringLiteral("The network request was cancelled.")
            );
    }
    if (timedOut)
    {
        reply->deleteLater();
        return networkError(
            ErrorCode::Io,
            QStringLiteral("The network request timed out.")
            );
    }
    if (networkStatus != QNetworkReply::NoError)
    {
        reply->deleteLater();
        return networkError(
            ErrorCode::Io,
            networkMessage.isEmpty()
                ? QStringLiteral("The network request failed.")
                : networkMessage,
            static_cast<int>(networkStatus)
            );
    }

    engine::NetworkResponse response;
    const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    response.statusCode = status.isValid() ? status.toInt() : 0;
    for (const auto& pair : reply->rawHeaderPairs())
    {
        response.headers.emplace_back(
            std::string(pair.first.constData(), static_cast<std::size_t>(pair.first.size())),
            std::string(pair.second.constData(), static_cast<std::size_t>(pair.second.size()))
            );
    }
    response.body = toBytes(reply->readAll());
    reply->deleteLater();
    return response;
}

engine::Status QtSignatureVerifier::verify(
    const engine::DetachedSignature& detached
    ) const
{
    if (detached.payload.empty() || detached.signature.empty() || detached.publicKey.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The detached signature inputs are incomplete.")
            ));
    }
    const QByteArray payload = fromBytes(detached.payload);
    const QByteArray signature = fromBytes(detached.signature);
    const QByteArray publicKey = fromBytes(detached.publicKey);
    if (
        payload.size() != static_cast<qsizetype>(detached.payload.size())
        || signature.size() != static_cast<qsizetype>(detached.signature.size())
        || publicKey.size() != static_cast<qsizetype>(detached.publicKey.size())
        )
    {
        return std::unexpected(error(
            ErrorCode::NumericOverflow,
            QStringLiteral("The detached signature input is too large.")
            ));
    }

    const ::Status status = UpdateSignatureVerifier::verifyDetachedSignature(
        payload,
        signature,
        QString::fromUtf8(publicKey)
        );
    if (!status)
    {
        const QString message = status.error();
        return std::unexpected(error(ErrorCode::InvalidFormat, message));
    }
    return {};
}

engine::Result<engine::ProcessResult> QtProcessLauncher::launch(
    const engine::ProcessLaunchRequest& request
    )
{
    const auto timeout = timeoutMilliseconds(request.timeout);
    if (
        !timeout
        || request.executable.empty()
        || request.executable.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || request.workingDirectory.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
        )
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The process launch request is invalid.")
            ));
    }
    if (request.cancellation && request.cancellation->isCancellationRequested())
    {
        return std::unexpected(error(
            ErrorCode::Cancelled,
            QStringLiteral("The process launch was cancelled.")
            ));
    }

    QProcess process;
    process.setProgram(fromUtf8(request.executable));
    QStringList arguments;
    for (const std::string& argument : request.arguments)
    {
        if (argument.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return std::unexpected(error(
                ErrorCode::InvalidArgument,
                QStringLiteral("A process argument is too large.")
                ));
        }
        arguments.append(fromUtf8(argument));
    }
    process.setArguments(arguments);
    if (!request.workingDirectory.empty())
    {
        process.setWorkingDirectory(fromUtf8(request.workingDirectory));
    }

    QElapsedTimer elapsed;
    elapsed.start();
    process.start();
    while (process.state() == QProcess::Starting)
    {
        if (request.cancellation && request.cancellation->isCancellationRequested())
        {
            process.kill();
            process.waitForFinished(100);
            return std::unexpected(error(
                ErrorCode::Cancelled,
                QStringLiteral("The process launch was cancelled.")
                ));
        }
        const qint64 remaining = static_cast<qint64>(*timeout) - elapsed.elapsed();
        if (remaining <= 0)
        {
            process.kill();
            process.waitForFinished(100);
            return std::unexpected(error(
                ErrorCode::Io,
                QStringLiteral("The process launch timed out.")
                ));
        }
        process.waitForStarted(static_cast<int>(std::min<qint64>(remaining, 20)));
    }
    if (process.state() == QProcess::NotRunning)
    {
        return std::unexpected(error(
            ErrorCode::Io,
            process.errorString().isEmpty()
                ? QStringLiteral("The process could not be started.")
                : process.errorString(),
            static_cast<int>(process.error())
            ));
    }

    while (process.state() != QProcess::NotRunning)
    {
        if (request.cancellation && request.cancellation->isCancellationRequested())
        {
            process.kill();
            process.waitForFinished(100);
            return std::unexpected(error(
                ErrorCode::Cancelled,
                QStringLiteral("The process launch was cancelled.")
                ));
        }
        const qint64 remaining = static_cast<qint64>(*timeout) - elapsed.elapsed();
        if (remaining <= 0)
        {
            process.kill();
            process.waitForFinished(100);
            return std::unexpected(error(
                ErrorCode::Io,
                QStringLiteral("The process launch timed out.")
                ));
        }
        process.waitForFinished(static_cast<int>(std::min<qint64>(remaining, 20)));
    }
    if (process.exitStatus() != QProcess::NormalExit)
    {
        return std::unexpected(error(
            ErrorCode::Io,
            QStringLiteral("The process terminated abnormally.")
            ));
    }

    engine::ProcessResult result;
    result.exitCode = process.exitCode();
    result.standardOutput = toBytes(process.readAllStandardOutput());
    result.standardError = toBytes(process.readAllStandardError());
    return result;
}

engine::Status QtProcessLauncher::launchDetached(
    const engine::ProcessLaunchRequest& request
    )
{
    if (
        !timeoutMilliseconds(request.timeout)
        || request.executable.empty()
        || request.executable.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || request.workingDirectory.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
        )
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The detached process launch request is invalid.")
            ));
    }
    if (request.cancellation && request.cancellation->isCancellationRequested())
    {
        return std::unexpected(error(
            ErrorCode::Cancelled,
            QStringLiteral("The detached process launch was cancelled.")
            ));
    }

    QStringList arguments;
    for (const std::string& argument : request.arguments)
    {
        if (argument.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return std::unexpected(error(
                ErrorCode::InvalidArgument,
                QStringLiteral("A process argument is too large.")
                ));
        }
        arguments.append(fromUtf8(argument));
    }
    const bool started = QProcess::startDetached(
        fromUtf8(request.executable),
        arguments,
        fromUtf8(request.workingDirectory)
        );
    if (!started)
    {
        return std::unexpected(error(
            ErrorCode::Io,
            QStringLiteral("The detached process could not be started.")
            ));
    }
    return {};
}

engine::WallClockTimePoint QtClock::nowUtc() const noexcept
{
    const auto milliseconds = std::chrono::milliseconds{
        QDateTime::currentDateTime(QTimeZone::UTC).toMSecsSinceEpoch()
    };
    return engine::WallClockTimePoint{
        std::chrono::duration_cast<engine::WallClockTimePoint::duration>(milliseconds)
    };
}

engine::MonotonicTimePoint QtClock::monotonicNow() const noexcept
{
    return std::chrono::steady_clock::now();
}

void QtLogger::log(const engine::LogRecord& record) noexcept
{
    QDebug stream = qDebug().noquote();
    switch (record.severity)
    {
    case engine::LogSeverity::Trace:
    case engine::LogSeverity::Debug:
        stream = qDebug().noquote();
        break;
    case engine::LogSeverity::Info:
        stream = qInfo().noquote();
        break;
    case engine::LogSeverity::Warning:
        stream = qWarning().noquote();
        break;
    case engine::LogSeverity::Error:
    case engine::LogSeverity::Critical:
        stream = qCritical().noquote();
        break;
    }
    stream << fromUtf8(record.message);
    for (const auto& [key, value] : record.context)
    {
        stream << QStringLiteral(" ")
               << fromUtf8(key)
               << QStringLiteral("=")
               << fromUtf8(value);
    }
}

engine::Result<bool> QtResourceProvider::exists(
    std::string_view logicalPath
    ) const
{
    if (logicalPath.empty() || logicalPath.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The resource path is invalid.")
            ));
    }
    return QFileInfo(fromUtf8(logicalPath)).exists();
}

engine::Result<ByteBuffer> QtResourceProvider::readBytes(
    std::string_view logicalPath
    ) const
{
    if (logicalPath.empty() || logicalPath.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The resource path is invalid.")
            ));
    }
    const QString path = fromUtf8(logicalPath);
    QFileInfo info(path);
    if (!info.exists())
    {
        return std::unexpected(error(
            ErrorCode::NotFound,
            QStringLiteral("The resource was not found.")
            ));
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::unexpected(error(
            ErrorCode::Io,
            QStringLiteral("Unable to read the resource: %1").arg(file.errorString()),
            static_cast<int>(file.error())
            ));
    }
    const QByteArray contents = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        return std::unexpected(error(
            ErrorCode::Io,
            QStringLiteral("Unable to read the resource: %1").arg(file.errorString()),
            static_cast<int>(file.error())
            ));
    }
    return toBytes(contents);
}

engine::Result<engine::ResourceMetadata> QtResourceProvider::metadata(
    std::string_view logicalPath
    ) const
{
    if (logicalPath.empty() || logicalPath.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            QStringLiteral("The resource path is invalid.")
            ));
    }
    const QFileInfo info(fromUtf8(logicalPath));
    if (!info.exists())
    {
        return std::unexpected(error(
            ErrorCode::NotFound,
            QStringLiteral("The resource was not found.")
            ));
    }
    const qint64 size = info.size();
    if (size < 0)
    {
        return std::unexpected(error(
            ErrorCode::Io,
            QStringLiteral("Unable to read resource metadata.")
            ));
    }
    return engine::ResourceMetadata{
        static_cast<std::uintmax_t>(size),
        info.isFile()
    };
}

} // namespace classmngr::qt
