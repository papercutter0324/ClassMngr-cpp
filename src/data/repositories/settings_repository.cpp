#include "settings_repository.h"

#include "classmngr/engine/application_settings_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"

#include <QByteArray>
#include <QMetaType>
#include <QObject>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using EngineApplicationSettings =
    classmngr::engine::ApplicationSettings;
using EngineError = classmngr::engine::Error;
using EngineSettingValue = classmngr::engine::SettingValue;

std::string toUtf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(
    std::string_view value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::vector<std::byte> toEngineBytes(
    const QByteArray& value
    )
{
    std::vector<std::byte> result(
        static_cast<std::size_t>(value.size())
        );
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        result[index] = static_cast<std::byte>(
            static_cast<unsigned char>(value.at(static_cast<qsizetype>(index)))
            );
    }
    return result;
}

QByteArray fromEngineBytes(
    const std::vector<std::byte>& value
    )
{
    QByteArray result;
    result.resize(static_cast<qsizetype>(value.size()));
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        result[static_cast<qsizetype>(index)] = static_cast<char>(
            std::to_integer<unsigned char>(value[index])
            );
    }
    return result;
}

QString settingIdentity(
    const QString& key
    )
{
    return QObject::tr("setting key '%1'").arg(key);
}

QString operationFailure(
    const QString& operation,
    const QString& settingContext,
    const QString& detail
    )
{
    QString message = QObject::tr("%1 failed").arg(operation);

    if (!settingContext.trimmed().isEmpty())
    {
        message += QObject::tr(" for %1").arg(settingContext);
    }

    const QString trimmedDetail = detail.trimmed();
    if (!trimmedDetail.isEmpty())
    {
        message += QStringLiteral(": ") + trimmedDetail;
    }

    return message;
}

QString engineErrorDetail(
    const EngineError& error
    )
{
    const QString detail = fromUtf8(error.message);
    if (!detail.trimmed().isEmpty())
    {
        return detail;
    }

    return QObject::tr("The engine reported a %1 error.")
        .arg(fromUtf8(classmngr::engine::errorCodeName(error.code)));
}

QString engineFailure(
    const QString& operation,
    const QString& settingContext,
    const EngineError& error
    )
{
    return operationFailure(
        operation,
        settingContext,
        engineErrorDetail(error)
        );
}

QString unsupportedSettingType(
    const QVariant& value
    )
{
    const char* typeName = value.typeName();
    const QString type = typeName == nullptr
        ? QObject::tr("unknown")
        : QString::fromLatin1(typeName);
    return QObject::tr("Unsupported application setting type '%1'.")
        .arg(type);
}

QString integerRangeFailure()
{
    return QObject::tr(
        "The application setting integer is outside the engine range."
        );
}

Result<EngineSettingValue> toEngineSettingValue(
    const QString& operation,
    const QString& settingContext,
    const QVariant& value
    )
{
    if (!value.isValid())
    {
        return EngineSettingValue{std::monostate{}};
    }

    const int typeId = value.metaType().id();
    switch (typeId)
    {
    case QMetaType::Bool:
        return EngineSettingValue{
            std::int64_t{value.toBool() ? 1 : 0}
        };
    case QMetaType::Int:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<int>())
        };
    case QMetaType::UInt:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<unsigned int>())
        };
    case QMetaType::LongLong:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<qlonglong>())
        };
    case QMetaType::ULongLong:
    {
        const qulonglong integer = value.value<qulonglong>();
        if (integer > static_cast<qulonglong>(
                std::numeric_limits<std::int64_t>::max()
                ))
        {
            return std::unexpected(operationFailure(
                operation,
                settingContext,
                integerRangeFailure()
                ));
        }
        return EngineSettingValue{
            static_cast<std::int64_t>(integer)
        };
    }
    case QMetaType::Long:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<long>())
        };
    case QMetaType::Short:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<short>())
        };
    case QMetaType::Char:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<char>())
        };
    case QMetaType::ULong:
    {
        const unsigned long integer = value.value<unsigned long>();
        if (static_cast<unsigned long long>(integer)
            > static_cast<unsigned long long>(
                std::numeric_limits<std::int64_t>::max()
                ))
        {
            return std::unexpected(operationFailure(
                operation,
                settingContext,
                integerRangeFailure()
                ));
        }
        return EngineSettingValue{
            static_cast<std::int64_t>(integer)
        };
    }
    case QMetaType::UShort:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<unsigned short>())
        };
    case QMetaType::UChar:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<unsigned char>())
        };
    case QMetaType::SChar:
        return EngineSettingValue{
            static_cast<std::int64_t>(value.value<signed char>())
        };
    case QMetaType::Double:
        return EngineSettingValue{value.value<double>()};
    case QMetaType::Float:
        return EngineSettingValue{
            static_cast<double>(value.value<float>())
        };
    case QMetaType::QString:
        return EngineSettingValue{toUtf8(value.value<QString>())};
    case QMetaType::QByteArray:
        return EngineSettingValue{toEngineBytes(value.value<QByteArray>())};
    default:
        return std::unexpected(operationFailure(
            operation,
            settingContext,
            unsupportedSettingType(value)
            ));
    }
}

QVariant fromEngineSettingValue(
    const EngineSettingValue& value
    )
{
    return std::visit(
        [](const auto& item) -> QVariant
        {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::monostate>)
            {
                return QVariant();
            }
            else if constexpr (std::is_same_v<Value, std::int64_t>)
            {
                return QVariant::fromValue<qlonglong>(
                    static_cast<qlonglong>(item)
                    );
            }
            else if constexpr (std::is_same_v<Value, double>)
            {
                return QVariant::fromValue(item);
            }
            else if constexpr (std::is_same_v<Value, std::string>)
            {
                return QVariant::fromValue(fromUtf8(item));
            }
            else
            {
                return QVariant::fromValue(fromEngineBytes(item));
            }
        },
        value
        );
}
} // namespace

SettingsRepository::SettingsRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

SettingsRepository::SettingsRepository(
    QSqlDatabase& database
    )
    : SettingsRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

SettingsRepository::~SettingsRepository() = default;

Status SettingsRepository::ensureEngineDatabase(
    const QString& operation,
    const QString& settingContext
    )
{
    if (!m_compatibilityDatabaseWasOpen)
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                settingContext,
                QObject::tr("No Teacher Profile is open.")
                )
            );
    }

    const QString databasePath = m_databasePath;
    if (databasePath.trimmed().isEmpty()
        || databasePath.trimmed() == QStringLiteral(":memory:"))
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                settingContext,
                QObject::tr("No database path is available.")
                )
            );
    }

    if (m_engineDatabase
        && m_engineDatabase->isOpen()
        && m_engineDatabasePath == databasePath)
    {
        return {};
    }

    m_engineDatabase.reset();
    m_engineDatabasePath.clear();

    auto opened = classmngr::engine::OpenDatabase::execute(
        toUtf8(databasePath)
        );
    if (!opened)
    {
        return std::unexpected(
            engineFailure(operation, settingContext, opened.error())
            );
    }
    if (*opened == nullptr)
    {
        return std::unexpected(
            operationFailure(
                operation,
                settingContext,
                QObject::tr("The engine database could not be opened.")
                )
            );
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

Status SettingsRepository::saveSetting(
    const QString& key,
    const QVariant& value
    )
{
    const QString operation = QObject::tr("Saving application setting");
    const QString identity = settingIdentity(key);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return engineReady;
    }

    const Result<EngineSettingValue> converted = toEngineSettingValue(
        operation,
        identity,
        value
        );
    if (!converted)
    {
        return std::unexpected(converted.error());
    }

    classmngr::engine::ApplicationSettingsService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.save(
        toUtf8(key),
        *converted
        );
    if (!saved)
    {
        return std::unexpected(
            engineFailure(operation, identity, saved.error())
            );
    }

    return {};
}

Status SettingsRepository::saveSettings(
    const QVariantMap& values
    )
{
    if (values.isEmpty())
    {
        return {};
    }

    const QString operation = QObject::tr("Saving application settings");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineApplicationSettings settings;
    settings.reserve(static_cast<std::size_t>(values.size()));
    for (auto setting = values.cbegin(); setting != values.cend(); ++setting)
    {
        const QString identity = settingIdentity(setting.key());
        const Result<EngineSettingValue> converted = toEngineSettingValue(
            QObject::tr("Saving application setting"),
            identity,
            setting.value()
            );
        if (!converted)
        {
            return std::unexpected(converted.error());
        }

        settings.emplace_back(
            toUtf8(setting.key()),
            *converted
            );
    }

    classmngr::engine::ApplicationSettingsService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.saveBatch(settings);
    if (!saved)
    {
        return std::unexpected(
            engineFailure(operation, {}, saved.error())
            );
    }

    return {};
}

Result<QVariant> SettingsRepository::loadSetting(
    const QString& key
    )
{
    const QString operation = QObject::tr("Loading application setting");
    const QString identity = settingIdentity(key);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    classmngr::engine::ApplicationSettingsService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineSettingValue> loaded =
        service.load(toUtf8(key));
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, identity, loaded.error())
            );
    }

    return fromEngineSettingValue(*loaded);
}
