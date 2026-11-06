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
    // Pure output path policy for callers that receive Documents through an
    // application port. Unlike defaultDirectory(), this never queries the OS.
    [[nodiscard]] static QString defaultDirectoryFromDocumentsDirectory(
        const ClassInfo& classInfo,
        const QString& evaluationName,
        const QString& documentsDirectory
        );
    [[nodiscard]] static QString batchArchivePath(
        const QString& outputDirectory
        );
    [[nodiscard]] static QString studentFileName(
        const QString& englishName,
        const QString& koreanName
        );
};
