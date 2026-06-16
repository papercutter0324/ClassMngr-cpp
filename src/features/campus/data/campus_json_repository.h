#ifndef CAMPUS_JSON_REPOSITORY_H
#define CAMPUS_JSON_REPOSITORY_H

#include "core/result.h"
#include "domain/models/campus_info.h"

#include <QList>
#include <QString>

#include <optional>

class CampusJsonRepository
{
public:
    explicit CampusJsonRepository(
        QString directoryPath,
        QString bundledDirectoryPath = QString()
        );

    void ensurePlaceholderCampus() const;

    [[nodiscard]] QList<CampusInfo> loadCampuses() const;

    [[nodiscard]] std::optional<CampusInfo> loadCampus(
        const QString& campusId
        ) const;

    [[nodiscard]] Status saveCampus(
        const CampusInfo& campus
        ) const;

    [[nodiscard]] QString filePathForCampusId(
        const QString& campusId
        ) const;

    static QString idFromName(
        const QString& name
        );

private:
    [[nodiscard]] Status ensureDirectory() const;

    [[nodiscard]] std::optional<CampusInfo> readCampusFile(
        const QString& filePath
        ) const;

    [[nodiscard]] QString bundledFilePathForCampusId(
        const QString& campusId
        ) const;

    [[nodiscard]] QList<QString> campusFilePaths() const;

    QString m_directoryPath;
    QString m_bundledDirectoryPath;
};

#endif // CAMPUS_JSON_REPOSITORY_H
