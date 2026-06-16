#ifndef CAMPUS_JSON_REPOSITORY_H
#define CAMPUS_JSON_REPOSITORY_H

#include "models/campus_info.h"

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

    QList<CampusInfo> loadCampuses() const;

    std::optional<CampusInfo> loadCampus(
        const QString& campusId
        ) const;

    bool saveCampus(
        const CampusInfo& campus,
        QString* errorMessage = nullptr
        ) const;

    QString filePathForCampusId(
        const QString& campusId
        ) const;

    static QString idFromName(
        const QString& name
        );

private:
    bool ensureDirectory(
        QString* errorMessage = nullptr
        ) const;

    std::optional<CampusInfo> readCampusFile(
        const QString& filePath
        ) const;

    QString bundledFilePathForCampusId(
        const QString& campusId
        ) const;

    QList<QString> campusFilePaths() const;

    QString m_directoryPath;
    QString m_bundledDirectoryPath;
};

#endif // CAMPUS_JSON_REPOSITORY_H
