#pragma once

#include "domain/models/class_info.h"

#include <QString>

class SpeakingEvalReportOutputPolicy final
{
public:
    [[nodiscard]] static QString defaultDirectory(
        const ClassInfo& classInfo,
        const QString& evaluationName,
        const QString& documentsDirectory = {}
        );
    [[nodiscard]] static QString batchArchivePath(
        const QString& outputDirectory
        );
    [[nodiscard]] static QString studentFileName(
        const QString& englishName,
        const QString& koreanName
        );
};
