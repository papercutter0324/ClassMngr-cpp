#include "speaking_eval_report_output.h"

#include "speaking_eval_batch_report_service.h"

#include "classmngr/engine/file_system.h"
#include "classmngr/engine/speaking_evaluation_report_output_policy.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QObject>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace SpeakingEvalReportOutput
{
namespace
{

using EngineFileSystem = classmngr::engine::StandardFileSystem;

std::string toUtf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
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

bool normalizePath(
    const EngineFileSystem& fileSystem,
    const QString& path,
    QString* normalizedPath
    )
{
    if (!normalizedPath)
    {
        return false;
    }

    const auto normalized = fileSystem.normalizePath(toUtf8(path));
    if (!normalized)
    {
        return false;
    }

    *normalizedPath = fromUtf8(*normalized);
    return true;
}

bool pathExists(
    const EngineFileSystem& fileSystem,
    const QString& path,
    bool* exists
    )
{
    if (!exists)
    {
        return false;
    }

    QString normalizedPathValue;
    if (!normalizePath(fileSystem, path, &normalizedPathValue))
    {
        return false;
    }

    const auto result = fileSystem.exists(toUtf8(normalizedPathValue));
    if (!result)
    {
        return false;
    }

    *exists = *result;
    return true;
}

bool ensureDirectory(
    const EngineFileSystem& fileSystem,
    const QString& path
    )
{
    QString normalizedPathValue;
    if (!normalizePath(fileSystem, path, &normalizedPathValue))
    {
        return false;
    }

    // Let the engine distinguish an existing directory from a blocking file;
    // a bool-only existence check cannot preserve that contract.
    return fileSystem.createDirectories(toUtf8(normalizedPathValue)).has_value();
}

void restoreBackups(
    const EngineFileSystem& fileSystem,
    const QStringList& backupTargets,
    const QStringList& targetPaths
    )
{
    for (int index = 0; index < backupTargets.size(); ++index)
    {
        const QString& backupPath = backupTargets.at(index);
        if (!backupPath.isEmpty())
        {
            (void)fileSystem.copyFile(
                toUtf8(backupPath),
                toUtf8(targetPaths.at(index)),
                true
                );
        }
    }
}

} // namespace

QString duplicateFileNameError(
    const classmngr::engine::Error& error
    )
{
    constexpr std::string_view prefix = "duplicate-student-file-name:";
    if (error.message.starts_with(prefix))
    {
        const std::string_view fileName =
            std::string_view(error.message).substr(prefix.size());
        return QObject::tr("A PDF named \"%1\" already exists.")
            .arg(QString::fromUtf8(
                fileName.data(),
                static_cast<qsizetype>(fileName.size())
                ));
    }

    return QString::fromUtf8(
        error.message.data(),
        static_cast<qsizetype>(error.message.size())
        );
}

bool targetFilePaths(
    const SpeakingEvalBatchReportService::Request& request,
    bool saveIndividualPdfFiles,
    QStringList* targetPaths,
    QString* errorMessage
    )
{
    if (!targetPaths)
    {
        return false;
    }

    const EngineFileSystem fileSystem;

    if (!request.outputFilePath.trimmed().isEmpty())
    {
        if (request.reports.size() != 1)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An exact PDF file can be selected only for one report."
                    );
            }
            return false;
        }

        const QFileInfo targetInfo(request.outputFilePath);
        if (!ensureDirectory(fileSystem, targetInfo.absolutePath()))
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "The selected PDF folder could not be created."
                    );
            }
            return false;
        }

        const QString targetPath = targetInfo.absoluteFilePath();
        bool targetExists = false;
        if (!pathExists(fileSystem, targetPath, &targetExists))
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "The selected PDF path could not be inspected."
                    );
            }
            return false;
        }

        if (!request.overwriteExisting && targetExists)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "A PDF named \"%1\" already exists."
                    ).arg(targetInfo.fileName());
            }
            return false;
        }

        *targetPaths = {targetPath};
        return true;
    }

    QDir outputDirectory(request.outputDirectory);
    if (!ensureDirectory(fileSystem, outputDirectory.path()))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "The selected PDF folder could not be created."
                );
        }
        return false;
    }

    targetPaths->clear();
    targetPaths->reserve(request.reports.size());
    std::vector<
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::StudentFileNameInput
        > students;
    students.reserve(request.reports.size());
    for (const auto& report : request.reports)
    {
        students.push_back({
            report.report.englishName
                .normalized(QString::NormalizationForm_C)
                .simplified()
                .toStdString(),
            report.report.koreanName
                .normalized(QString::NormalizationForm_C)
                .simplified()
                .toStdString()
        });
    }

    const auto names =
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::studentFileNames(
            students
            );
    if (!names)
    {
        if (errorMessage)
        {
            *errorMessage = duplicateFileNameError(names.error());
        }
        return false;
    }

    for (const std::string& name : *names)
    {
        const QString path = outputDirectory.filePath(
            QString::fromUtf8(
                name.data(),
                static_cast<qsizetype>(name.size())
                )
            );

        bool pathAlreadyExists = false;
        if (!pathExists(fileSystem, path, &pathAlreadyExists))
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "The selected PDF path could not be inspected."
                    );
            }
            return false;
        }

        if (saveIndividualPdfFiles
            && !request.overwriteExisting
            && pathAlreadyExists)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "A PDF named \"%1\" already exists."
                    ).arg(QFileInfo(path).fileName());
            }
            return false;
        }

        targetPaths->append(path);
    }

    return true;
}

bool commitFiles(
    const QStringList& stagedPaths,
    const QStringList& targetPaths,
    bool overwriteExisting,
    QString* errorMessage
    )
{
    if (stagedPaths.size() != targetPaths.size())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "The staged output files are incomplete."
                );
        }
        return false;
    }

    const EngineFileSystem fileSystem;
    for (int index = 0; index < stagedPaths.size(); ++index)
    {
        bool stagedExists = false;
        if (!pathExists(fileSystem, stagedPaths.at(index), &stagedExists)
            || !stagedExists)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "The staged output files are incomplete."
                    );
            }
            return false;
        }

        bool targetExists = false;
        if (!pathExists(fileSystem, targetPaths.at(index), &targetExists))
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An output file could not be finalized in the selected folder."
                    );
            }
            return false;
        }
        if (targetExists && !overwriteExisting)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An output file already exists in the selected folder."
                    );
            }
            return false;
        }
    }

    QStringList backupTargets;
    backupTargets.reserve(targetPaths.size());
    for (const QString& targetPath : targetPaths)
    {
        backupTargets.append(QString());

        bool targetExists = false;
        if (!pathExists(fileSystem, targetPath, &targetExists))
        {
            restoreBackups(fileSystem, backupTargets, targetPaths);
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An existing output file could not be prepared for replacement."
                    );
            }
            return false;
        }

        if (!overwriteExisting || !targetExists)
        {
            continue;
        }

        const QString backupPath =
            targetPath + QStringLiteral(".classmngr-backup");
        if (!fileSystem.removeFile(toUtf8(backupPath)).has_value()
            || !fileSystem.copyFile(
                toUtf8(targetPath),
                toUtf8(backupPath),
                true
                ).has_value())
        {
            restoreBackups(fileSystem, backupTargets, targetPaths);
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An existing output file could not be prepared for replacement."
                    );
            }
            return false;
        }
        backupTargets[backupTargets.size() - 1] = backupPath;
    }

    QStringList temporaryTargets;
    for (int index = 0; index < stagedPaths.size(); ++index)
    {
        const QString temporaryTarget =
            targetPaths.at(index) + QStringLiteral(".classmngr-part");
        temporaryTargets.append(temporaryTarget);

        if (!fileSystem.removeFile(toUtf8(temporaryTarget)).has_value()
            || !fileSystem.copyFile(
                toUtf8(stagedPaths.at(index)),
                toUtf8(temporaryTarget),
                true
                ).has_value())
        {
            for (const QString& createdPath : temporaryTargets)
            {
                (void)fileSystem.removeFile(toUtf8(createdPath));
            }
            restoreBackups(fileSystem, backupTargets, targetPaths);
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An output file could not be copied to the selected folder."
                    );
            }
            return false;
        }
    }

    QStringList committedTargets;
    for (int index = 0; index < temporaryTargets.size(); ++index)
    {
        if (!fileSystem.replaceFileAtomically(
                toUtf8(temporaryTargets.at(index)),
                toUtf8(targetPaths.at(index))
                ).has_value())
        {
            for (const QString& createdPath : temporaryTargets)
            {
                (void)fileSystem.removeFile(toUtf8(createdPath));
            }
            for (int committedIndex = 0;
                 committedIndex < committedTargets.size();
                 ++committedIndex)
            {
                if (backupTargets.at(committedIndex).isEmpty())
                {
                    (void)fileSystem.removeFile(
                        toUtf8(committedTargets.at(committedIndex))
                        );
                }
            }
            restoreBackups(fileSystem, backupTargets, targetPaths);
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An output file could not be finalized in the selected folder."
                    );
            }
            return false;
        }

        committedTargets.append(targetPaths.at(index));
    }

    for (const QString& backupPath : backupTargets)
    {
        if (!backupPath.isEmpty())
        {
            (void)fileSystem.removeFile(toUtf8(backupPath));
        }
    }

    return true;
}
}
