#include "campus_json_repository.h"

#include "campus_json_codec.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QObject>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace
{
bool isDefaultCampus(const CampusInfo& campus)
{
    return campus.id.compare(
               QStringLiteral("default"),
               Qt::CaseInsensitive
               ) == 0
        || campus.campusName.compare(
               QStringLiteral("Default"),
               Qt::CaseInsensitive
               ) == 0;
}

bool isDefaultCampusFile(const QString& filePath)
{
    return QFileInfo(filePath)
               .baseName()
               .compare(
                   QStringLiteral("default"),
                   Qt::CaseInsensitive
                   ) == 0;
}
} // namespace

CampusJsonRepository::CampusJsonRepository(
    QString directoryPath
    )
    : m_directoryPath(std::move(directoryPath))
{
}

QList<CampusInfo>
CampusJsonRepository::loadCampuses() const
{
    QList<CampusInfo> campuses;

    for (const QString& filePath : campusFilePaths())
    {
        if (isDefaultCampusFile(filePath))
        {
            continue;
        }

        const RepositoryResult<std::optional<CampusInfo>> loaded =
            readCampusFile(filePath);

        // Bulk directory consumers intentionally use a best-effort policy:
        // unavailable, unreadable, and malformed entries are omitted. Point
        // reads return the underlying typed error instead.
        if (!loaded || !loaded->has_value())
        {
            continue;
        }

        const CampusInfo& campus = loaded->value();
        if (!isDefaultCampus(campus))
        {
            campuses.append(
                campus
                );
        }
    }

    std::ranges::sort(
        campuses,
        [](const CampusInfo& lhs, const CampusInfo& rhs)
        {
            return QString::compare(
                lhs.campusName,
                rhs.campusName,
                Qt::CaseInsensitive
                ) < 0;
        }
        );

    return campuses;
}

RepositoryResult<std::optional<CampusInfo>>
CampusJsonRepository::loadCampus(
    const QString& campusId
    ) const
{
    if (campusId.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0)
    {
        return std::optional<CampusInfo>{};
    }

    const QString campusFilePath =
        filePathForCampusId(campusId);

    return readCampusFile(
        campusFilePath
        );
}

RepositoryStatus CampusJsonRepository::saveCampus(
    const CampusInfo& campus
    ) const
{
    const RepositoryStatus directoryReady =
        ensureDirectory();

    if (!directoryReady)
    {
        return directoryReady;
    }

    CampusInfo campusToSave = campus;

    if (campusToSave.id.trimmed().isEmpty())
    {
        campusToSave.id =
            idFromName(
                campusToSave.campusName
                );
    }

    return writeCampusFile(
        campusToSave,
        filePathForCampusId(campusToSave.id)
        );
}

RepositoryStatus CampusJsonRepository::createCampus(
    const CampusInfo& campus
    ) const
{
    const RepositoryStatus directoryReady = ensureDirectory();
    if (!directoryReady)
    {
        return std::unexpected(directoryReady.error());
    }

    CampusInfo campusToSave = campus;
    if (campusToSave.id.trimmed().isEmpty())
    {
        campusToSave.id = idFromName(campusToSave.campusName);
    }

    const QString filePath = filePathForCampusId(campusToSave.id);
    if (QFileInfo::exists(filePath))
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Conflict,
            QObject::tr("A campus with this ID already exists.")
        });
    }

    const RepositoryStatus saved = writeCampusFile(campusToSave, filePath);
    if (!saved)
    {
        return std::unexpected(saved.error());
    }

    return {};
}

RepositoryStatus CampusJsonRepository::writeCampusFile(
    const CampusInfo& campus,
    const QString& filePath
    ) const
{
    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to open campus file for writing:\n%1")
                .arg(file.errorString())
        });
    }

    const QJsonDocument document(
        CampusJsonCodec::toJson(campus)
        );

    const QByteArray json = document.toJson(QJsonDocument::Indented);
    const qint64 bytesWritten = file.write(json);
    if (bytesWritten != json.size())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to write campus file:\n%1")
                .arg(file.errorString())
        });
    }

    if (!file.commit())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to save campus file:\n%1")
                .arg(file.errorString())
        });
    }

    return {};
}

QString CampusJsonRepository::filePathForCampusId(
    const QString& campusId
    ) const
{
    return QDir(m_directoryPath)
        .filePath(
            idFromName(campusId) + QStringLiteral(".json")
            );
}

QList<QString> CampusJsonRepository::campusFilePaths() const
{
    QList<QString> filePaths;

    if (m_directoryPath.trimmed().isEmpty())
    {
        return filePaths;
    }

    const QDir directory(m_directoryPath);

    const QStringList files =
        directory.entryList(
            {QStringLiteral("*.json")},
            QDir::Files,
            QDir::Name
            );

    for (const QString& file : files)
    {
        filePaths.append(
            directory.filePath(file)
            );
    }

    return filePaths;
}

QString CampusJsonRepository::idFromName(
    const QString& name
    )
{
    QString id =
        name.trimmed().toLower();

    id.replace(
        QRegularExpression(QStringLiteral("[^a-z0-9]+")),
        QStringLiteral("_")
        );

    id.replace(
        QRegularExpression(QStringLiteral("^_+|_+$")),
        QString()
        );

    if (id.isEmpty())
    {
        id =
            QStringLiteral("campus");
    }

    return id;
}

RepositoryStatus CampusJsonRepository::ensureDirectory() const
{
    QDir directory(m_directoryPath);

    if (directory.exists())
    {
        return {};
    }

    if (directory.mkpath(QStringLiteral(".")))
    {
        return {};
    }

    return std::unexpected(RepositoryError{
        RepositoryErrorCode::Technical,
        QObject::tr("Unable to create campus data directory.")
    });
}

RepositoryResult<std::optional<CampusInfo>>
CampusJsonRepository::readCampusFile(
    const QString& filePath
    ) const
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (!QFileInfo::exists(filePath))
        {
            return std::optional<CampusInfo>{};
        }

        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to open campus file for reading:\n%1")
                .arg(file.errorString())
        });
    }

    QJsonParseError parseError;

    const QByteArray json = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to read campus file:\n%1")
                .arg(file.errorString())
        });
    }

    const QJsonDocument document =
        QJsonDocument::fromJson(
            json,
            &parseError
            );

    if (parseError.error != QJsonParseError::NoError)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("Unable to parse campus JSON:\n%1")
                .arg(parseError.errorString())
        });
    }

    if (!document.isObject())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("Campus JSON root must be an object.")
        });
    }

    return std::optional<CampusInfo>{CampusJsonCodec::fromJson(
        document.object()
    )};
}
