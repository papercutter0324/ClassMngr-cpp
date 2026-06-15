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
        QString directoryPath
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

    QString m_directoryPath;
};

#endif // CAMPUS_JSON_REPOSITORY_H
