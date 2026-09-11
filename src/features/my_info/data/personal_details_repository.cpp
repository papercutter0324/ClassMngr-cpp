#include "personal_details_repository.h"

#include "app/services/feature_services.h"
#include "classmngr/engine/application_settings_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/personal_details_service.h"
#include "classmngr/engine/sqlite_database.h"
#include "data/database/database_session.h"
#include "signature_image_processor.h"

#include <QDebug>
#include <QVariant>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace
{
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

QString engineErrorDetail(
    const classmngr::engine::Error& error
    )
{
    if (!error.message.empty())
    {
        return fromUtf8(error.message);
    }

    return QString::fromUtf8(
        classmngr::engine::errorCodeName(error.code).data(),
        static_cast<qsizetype>(
            classmngr::engine::errorCodeName(error.code).size()
            )
        );
}

QString engineFailure(
    const QString& operation,
    const classmngr::engine::Error& error
    )
{
    return QStringLiteral("%1: %2")
        .arg(operation, engineErrorDetail(error));
}
} // namespace

PersonalDetailsRepository::PersonalDetailsRepository(SettingsService* settingsService)
    : m_settingsService(settingsService)
{
}

PersonalDetailsRepository::~PersonalDetailsRepository() = default;

Status PersonalDetailsRepository::ensureEngineDatabase(
    const QString& operation
    ) const
{
    if (!m_settingsService || !m_settingsService->isAvailable())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile service is available.")
            );
    }

    DatabaseSession* const session =
        m_settingsService->databaseSession();
    if (!session || !session->isOpen())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    const QString databasePath = session->databasePath();
    if (databasePath.trimmed().isEmpty()
        || databasePath.trimmed() == QStringLiteral(":memory:"))
    {
        return std::unexpected(
            QStringLiteral("No database path is available.")
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
            engineFailure(operation, opened.error())
            );
    }
    if (*opened == nullptr)
    {
        return std::unexpected(
            QStringLiteral("%1: The engine database could not be opened.")
                .arg(operation)
            );
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

PersonalDetails PersonalDetailsRepository::load() const
{
    PersonalDetails details;

    if (!isAvailable())
    {
        return details;
    }

    const QString operation = QObject::tr("Loading personal details");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        qWarning().noquote() << engineReady.error();
        return details;
    }

    classmngr::engine::ApplicationSettingsService settings(*m_engineDatabase);
    classmngr::engine::PersonalDetailsService service(settings);
    const auto loaded = service.load();
    if (!loaded)
    {
        qWarning().noquote() << engineFailure(operation, loaded.error());
        return details;
    }

    details.name = fromUtf8(loaded->name);
    details.campus = fromUtf8(loaded->campus);
    details.zoomLoginId = fromUtf8(loaded->zoomLoginId);
    details.zoomPassword = fromUtf8(loaded->zoomPassword);
    details.zoomNotAvailable = loaded->zoomNotAvailable;
    details.signatureImage = SignatureImage::prepareForEmbedding(
        QByteArray::fromBase64(
            fromUtf8(loaded->signatureImageBase64).toLatin1()
            )
        );
    details.signatureMode =
        loaded->signatureMode
                == classmngr::engine::SignatureMode::Type
            ? SignatureMode::Type
            : SignatureMode::Image;
    details.typedSignatureText = fromUtf8(loaded->typedSignatureText);
    details.typedSignatureFont = loaded->typedSignatureFont;
    return details;
}

bool PersonalDetailsRepository::save(const PersonalDetails& details) const
{
    if (!isAvailable())
    {
        return false;
    }

    const QString operation = QObject::tr("Saving personal details");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        qWarning().noquote() << engineReady.error();
        return false;
    }

    classmngr::engine::PersonalDetails portable;
    portable.name = toUtf8(details.name);
    portable.campus = toUtf8(details.campus);
    portable.zoomLoginId = toUtf8(details.zoomLoginId);
    portable.zoomPassword = toUtf8(details.zoomPassword);
    portable.zoomNotAvailable = details.zoomNotAvailable;
    portable.signatureImageBase64 = toUtf8(
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(
                details.signatureImage
                ).toBase64()
            )
        );
    portable.signatureMode =
        details.signatureMode == SignatureMode::Type
            ? classmngr::engine::SignatureMode::Type
            : classmngr::engine::SignatureMode::Image;
    portable.typedSignatureText = toUtf8(details.typedSignatureText);
    portable.typedSignatureFont = details.typedSignatureFont;

    classmngr::engine::ApplicationSettingsService settings(*m_engineDatabase);
    classmngr::engine::PersonalDetailsService service(settings);
    const auto saved = service.save(portable);
    if (!saved)
    {
        qWarning().noquote() << engineFailure(operation, saved.error());
        return false;
    }

    return true;
}

Status PersonalDetailsRepository::saveCampus(const QString& campus) const
{
    if (!isAvailable())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile service is available.")
            );
    }

    const QString operation = QObject::tr("Saving personal campus");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    classmngr::engine::ApplicationSettingsService settings(*m_engineDatabase);
    classmngr::engine::PersonalDetailsService service(settings);
    const auto saved = service.saveCampus(toUtf8(campus));
    if (!saved)
    {
        return std::unexpected(engineFailure(operation, saved.error()));
    }

    return {};
}

bool PersonalDetailsRepository::isAvailable() const
{
    return m_settingsService
        && m_settingsService->isAvailable();
}

QVariant PersonalDetailsRepository::loadSetting(
    const QString& key,
    const QVariant& defaultValue
    ) const
{
    return m_settingsService
        ? m_settingsService->loadOrDefault(key, defaultValue)
        : defaultValue;
}

Status PersonalDetailsRepository::saveSetting(
    const QString& key,
    const QVariant& value
    ) const
{
    if (!m_settingsService)
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile service is available.")
            );
    }

    return m_settingsService->save(key, value);
}
