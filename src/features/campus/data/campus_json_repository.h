#ifndef CAMPUS_JSON_REPOSITORY_H
#define CAMPUS_JSON_REPOSITORY_H

#include "data/repositories/repository_error.h"
#include "domain/models/campus_info.h"

#include <QList>
#include <QString>

#include <expected>
#include <optional>

class CampusJsonRepository
{
public:
    explicit CampusJsonRepository(
        QString directoryPath
        );

    // Vector-only callers keep the legacy best-effort policy: unavailable,
    // unreadable, malformed, and default entries are omitted.
    [[nodiscard]] QList<CampusInfo> loadCampuses() const;

    // Point reads keep absence distinct from read and validation failures.
    [[nodiscard]] RepositoryResult<std::optional<CampusInfo>> loadCampus(
        const QString& campusId
        ) const;

    [[nodiscard]] RepositoryStatus saveCampus(
        const CampusInfo& campus
        ) const;

    [[nodiscard]] RepositoryStatus createCampus(
        const CampusInfo& campus
        ) const;

    [[nodiscard]] QString filePathForCampusId(
        const QString& campusId
        ) const;

    static QString idFromName(
        const QString& name
        );

private:
    [[nodiscard]] RepositoryStatus ensureDirectory() const;

    [[nodiscard]] RepositoryStatus writeCampusFile(
        const CampusInfo& campus,
        const QString& filePath
        ) const;

    [[nodiscard]] RepositoryResult<std::optional<CampusInfo>> readCampusFile(
        const QString& filePath
        ) const;

    [[nodiscard]] QList<QString> campusFilePaths() const;

    QString m_directoryPath;
};

#endif // CAMPUS_JSON_REPOSITORY_H
