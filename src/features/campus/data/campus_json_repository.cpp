#include "campus_json_repository.h"

#include "campus_json_codec.h"

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

        const std::optional<CampusInfo> campus =
            readCampusFile(
                filePath
                );

        if (campus.has_value() && !isDefaultCampus(campus.value()))
        {
            campuses.append(
                campus.value()
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

std::optional<CampusInfo>
CampusJsonRepository::loadCampus(
    const QString& campusId
    ) const
{
    if (campusId.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0)
    {
        return std::nullopt;
    }

    const QString campusFilePath =
        filePathForCampusId(campusId);

    return readCampusFile(
        campusFilePath
        );
}

Status CampusJsonRepository::saveCampus(
    const CampusInfo& campus
    ) const
{
    const Status directoryReady =
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

    QSaveFile file(
        filePathForCampusId(campusToSave.id)
        );

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return std::unexpected(
            QObject::tr("Unable to open campus file for writing:\n%1")
                .arg(file.errorString())
            );
    }

    const QJsonDocument document(
        CampusJsonCodec::toJson(campusToSave)
        );

    file.write(
        document.toJson(QJsonDocument::Indented)
        );

    if (!file.commit())
    {
        return std::unexpected(
            QObject::tr("Unable to save campus file:\n%1")
                .arg(file.errorString())
            );
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

Status CampusJsonRepository::ensureDirectory() const
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

    return std::unexpected(
        QObject::tr("Unable to create campus data directory.")
        );
}

std::optional<CampusInfo> CampusJsonRepository::readCampusFile(
    const QString& filePath
    ) const
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return std::nullopt;
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            file.readAll(),
            &parseError
            );

    if (
        parseError.error != QJsonParseError::NoError
        || !document.isObject()
        )
    {
        return std::nullopt;
    }

    return CampusJsonCodec::fromJson(
        document.object()
        );
}
