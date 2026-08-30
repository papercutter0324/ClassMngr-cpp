#include "speaking_eval_report_output.h"

#include "speaking_eval_batch_report_service.h"

#include "classmngr/engine/speaking_evaluation_report_output_policy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <string_view>
#include <vector>

namespace SpeakingEvalReportOutput
{
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
        QDir targetDirectory(targetInfo.absolutePath());
        if (!targetDirectory.exists()
            && !QDir().mkpath(targetDirectory.path()))
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
        if (!request.overwriteExisting && QFileInfo::exists(targetPath))
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
    if (!outputDirectory.exists() && !QDir().mkpath(outputDirectory.path()))
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

        if (saveIndividualPdfFiles
            && !request.overwriteExisting
            && QFileInfo::exists(path))
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

    QStringList backupTargets;
    backupTargets.reserve(targetPaths.size());
    for (const QString& targetPath : targetPaths)
    {
        if (!overwriteExisting || !QFileInfo::exists(targetPath))
        {
            backupTargets.append(QString());
            continue;
        }

        const QString backupPath =
            targetPath + QStringLiteral(".classmngr-backup");
        QFile::remove(backupPath);
        if (!QFile::rename(targetPath, backupPath))
        {
            for (int index = 0; index < backupTargets.size(); ++index)
            {
                if (!backupTargets.at(index).isEmpty())
                {
                    QFile::rename(
                        backupTargets.at(index),
                        targetPaths.at(index)
                        );
                }
            }
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An existing output file could not be prepared for replacement."
                    );
            }
            return false;
        }
        backupTargets.append(backupPath);
    }

    QStringList temporaryTargets;
    for (int index = 0; index < stagedPaths.size(); ++index)
    {
        const QString temporaryTarget =
            targetPaths.at(index) + QStringLiteral(".classmngr-part");
        QFile::remove(temporaryTarget);

        if (!QFile::copy(stagedPaths.at(index), temporaryTarget))
        {
            for (const QString& createdPath : temporaryTargets)
            {
                QFile::remove(createdPath);
            }
            for (int backupIndex = 0;
                 backupIndex < backupTargets.size();
                 ++backupIndex)
            {
                if (!backupTargets.at(backupIndex).isEmpty())
                {
                    QFile::rename(
                        backupTargets.at(backupIndex),
                        targetPaths.at(backupIndex)
                        );
                }
            }
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "An output file could not be copied to the selected folder."
                    );
            }
            return false;
        }

        temporaryTargets.append(temporaryTarget);
    }

    QStringList committedTargets;
    for (int index = 0; index < temporaryTargets.size(); ++index)
    {
        if (!QFile::rename(temporaryTargets.at(index), targetPaths.at(index)))
        {
            for (const QString& createdPath : temporaryTargets)
            {
                QFile::remove(createdPath);
            }
            for (const QString& committedPath : committedTargets)
            {
                QFile::remove(committedPath);
            }
            for (int backupIndex = 0;
                 backupIndex < backupTargets.size();
                 ++backupIndex)
            {
                if (!backupTargets.at(backupIndex).isEmpty())
                {
                    QFile::rename(
                        backupTargets.at(backupIndex),
                        targetPaths.at(backupIndex)
                        );
                }
            }
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
            QFile::remove(backupPath);
        }
    }

    return true;
}
}
