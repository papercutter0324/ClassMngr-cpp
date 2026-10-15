#ifndef CAMPUS_JSON_REPOSITORY_H
#define CAMPUS_JSON_REPOSITORY_H

#include "core/result.h"
#include "domain/models/campus_info.h"

#include <QList>
#include <QString>

#include <expected>
#include <optional>

class CampusJsonRepository
{
public:
    struct CreateError final
    {
        enum class Code
        {
            Conflict,
            Technical
        };

        Code code = Code::Technical;
        QString message;
    };

    using CreateResult = std::expected<void, CreateError>;

    explicit CampusJsonRepository(
        QString directoryPath
        );

    [[nodiscard]] QList<CampusInfo> loadCampuses() const;

    [[nodiscard]] std::optional<CampusInfo> loadCampus(
        const QString& campusId
        ) const;

    [[nodiscard]] Status saveCampus(
        const CampusInfo& campus
        ) const;

    [[nodiscard]] CreateResult createCampus(
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

    [[nodiscard]] Status writeCampusFile(
        const CampusInfo& campus,
        const QString& filePath
        ) const;

    [[nodiscard]] std::optional<CampusInfo> readCampusFile(
        const QString& filePath
        ) const;

    [[nodiscard]] QList<QString> campusFilePaths() const;

    QString m_directoryPath;
};

#endif // CAMPUS_JSON_REPOSITORY_H
