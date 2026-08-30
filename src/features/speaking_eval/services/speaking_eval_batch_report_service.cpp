#include "speaking_eval_batch_report_service.h"
#include "speaking_eval_report_output_policy.h"
#include "speaking_eval_report_output.h"
#include "speaking_eval_report_asset_resolver.h"
#include "speaking_eval_internal_pdf_renderer.h"
#include "speaking_eval_powerpoint_automation.h"
#include "speaking_eval_powerpoint_job_model.h"
#include "speaking_eval_powerpoint_scripts.h"
#include "speaking_eval_powerpoint_workspace.h"

#include "classmngr/engine/speaking_evaluation_batch_report_policy.h"

#include "core/zip_archive_writer.h"
#include "core/resource_paths.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QSaveFile>
#include <QTemporaryDir>

#include <cstddef>

namespace SpeakingEvalBatchReportService
{
namespace
{

constexpr int PowerPointTimeoutMs = 5 * 60 * 1000;

Result completed(
    const QStringList& savedPdfPaths = {},
    const QString& savedArchivePath = {}
    )
{
    return {
        Status::Completed,
        QObject::tr("Reports created successfully."),
        savedPdfPaths,
        savedArchivePath
    };
}

Result canceled()
{
    return {
        Status::Canceled,
        QObject::tr("Report export was canceled."),
        {},
        {}
    };
}

Result failed(
    const QString& message,
    bool internalRendererFailed = false
    )
{
    return {
        internalRendererFailed
            ? Status::InternalRendererFailed
            : Status::Failed,
        message,
        {},
        {}
    };
}

enum class PowerPointBatchStatus
{
    Completed,
    Canceled,
    Failed
};

bool writeUtf8File(
    const QString& path,
    const QByteArray& data,
    QString* errorMessage
    )
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size()
        || !file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Unable to prepare PowerPoint automation.");
        }
        return false;
    }

    return true;
}

classmngr::engine::SpeakingEvaluationBatchReportRequest
portableBatchReportRequest(
    const Request& request
    )
{
    classmngr::engine::SpeakingEvaluationBatchReportRequest portable;
    portable.reportCount = static_cast<std::size_t>(request.reports.size());
    portable.renderer =
        request.renderer == Renderer::PowerPoint
            ? classmngr::engine::SpeakingEvaluationReportRenderer::PowerPoint
            : classmngr::engine::SpeakingEvaluationReportRenderer::Internal;
    portable.savePdf = request.savePdf;
    portable.printReports = request.printReports;
    portable.keepIndividualPdfFiles = request.keepIndividualPdfFiles;
    portable.hasOutputDirectory =
        !request.outputDirectory.trimmed().isEmpty();
    portable.hasExactOutputFilePath =
        !request.outputFilePath.trimmed().isEmpty();
    portable.reportTemplates.reserve(request.reports.size());
    for (const StudentReport& report : request.reports)
    {
        portable.reportTemplates.push_back(report.report.reportTemplate);
    }
    return portable;
}

QString batchReportPolicyErrorMessage(
    const classmngr::engine::Error& error
    )
{
    if (error.message == "no-reports")
    {
        return QObject::tr("There are no student reports to export.");
    }
    if (error.message == "output-mode-required")
    {
        return QObject::tr("Choose PDF saving, printing, or both.");
    }
    if (error.message == "pdf-destination-required")
    {
        return QObject::tr("Choose a destination for the PDF reports.");
    }
    if (error.message == "exact-file-requires-single-report")
    {
        return QObject::tr(
            "An exact PDF file can be selected only for one report."
            );
    }
    if (error.message == "mixed-powerpoint-templates")
    {
        return QObject::tr(
            "All reports in a PowerPoint batch must use the same template."
            );
    }
    if (error.message == "template-count-mismatch")
    {
        return QObject::tr(
            "The PowerPoint batch has an incomplete template description."
            );
    }
    return QString::fromUtf8(
        error.message.data(),
        static_cast<qsizetype>(error.message.size())
        );
}


PowerPointBatchStatus renderPowerPointBatch(
    const SpeakingEvalPowerPointJobModel::BatchJob& batch,
    const QString& automationDirectory,
    const QString& presentationDirectory,
    const std::function<
        bool(int completed, int total, const QString& studentName)
        >& progressCallback,
    QString* errorMessage
    )
{
    if (batch.students.isEmpty()
        || automationDirectory.trimmed().isEmpty()
        || presentationDirectory.trimmed().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "The temporary PowerPoint batch is unavailable."
                );
        }
        return PowerPointBatchStatus::Failed;
    }

    const QDir automation(automationDirectory);
    const QDir presentation(presentationDirectory);
    const QString preparedPptxPath =
        automation.filePath(QStringLiteral("report-template.pptx"));
    const QString pptxPath =
        presentation.filePath(QStringLiteral("report-template.pptx"));
    const QString jobPath =
        automation.filePath(QStringLiteral("batch.json"));
    const QString cancelPath =
        automation.filePath(QStringLiteral("cancel-requested"));
    const QString scriptPath =
        automation.filePath(
#ifdef Q_OS_WIN
            QStringLiteral("export-batch.ps1")
#else
            QStringLiteral("export-batch.applescript")
#endif
            );

    QString preparedSignaturePath;
    if (!SpeakingEvalReportAssetResolver::copyResourceToFile(
            batch.templateProfile.resourcePath,
            preparedPptxPath,
            errorMessage
            )
        || !SpeakingEvalReportAssetResolver::prepareSignatureImage(
            batch.signatureImage,
            automationDirectory,
            &preparedSignaturePath,
            errorMessage
            ))
    {
        return PowerPointBatchStatus::Failed;
    }

    QString signaturePath = preparedSignaturePath;
    if (automationDirectory != presentationDirectory)
    {
        if (!SpeakingEvalReportAssetResolver::copyFileReplacing(
                preparedPptxPath,
                pptxPath,
                QObject::tr(
                    "The PowerPoint template could not be copied into PowerPoint's private workspace."
                    ),
                errorMessage
                ))
        {
            return PowerPointBatchStatus::Failed;
        }

        signaturePath.clear();
        if (!preparedSignaturePath.isEmpty())
        {
            signaturePath =
                presentation.filePath(
                    QStringLiteral("signature.png")
                    );
            if (!SpeakingEvalReportAssetResolver::copyFileReplacing(
                    preparedSignaturePath,
                    signaturePath,
                    QObject::tr(
                        "The signature could not be copied into PowerPoint's private workspace."
                        ),
                    errorMessage
                    ))
            {
                return PowerPointBatchStatus::Failed;
            }
        }
    }

    QString executable;
    QStringList arguments;

#ifdef Q_OS_WIN
    executable = SpeakingEvalPowerPointAutomation::executable();
    if (executable.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = powerPointRendererAvailabilityMessage();
        }
        return PowerPointBatchStatus::Failed;
    }

    const QByteArray jobJson =
        QByteArrayLiteral("\xEF\xBB\xBF")
        + QJsonDocument(
            SpeakingEvalPowerPointJobModel::toJson(
                batch,
                pptxPath,
                signaturePath,
                cancelPath
                )
            ).toJson(QJsonDocument::Compact);
    if (!writeUtf8File(jobPath, jobJson, errorMessage)
        || !writeUtf8File(
            scriptPath,
            SpeakingEvalPowerPointScripts::windowsScript().toUtf8(),
            errorMessage
            ))
    {
        return PowerPointBatchStatus::Failed;
    }

    arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-File"),
        scriptPath,
        jobPath
    };
#elif defined(Q_OS_MACOS)
    executable = SpeakingEvalPowerPointAutomation::executable();
    if (executable.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = powerPointRendererAvailabilityMessage();
        }
        return PowerPointBatchStatus::Failed;
    }

    if (!writeUtf8File(
            scriptPath,
            SpeakingEvalPowerPointScripts::macScript().toUtf8(),
            errorMessage
            ))
    {
        return PowerPointBatchStatus::Failed;
    }

    const SpeakingEvalPowerPointJobModel::TemplateProfile& profile =
        batch.templateProfile;
    arguments = {
        scriptPath,
        pptxPath,
        profile.scoreTableOnMaster
            ? QStringLiteral("true")
            : QStringLiteral("false"),
        profile.scoreTableName,
        QString::number(profile.minimumTableRows),
        QString::number(profile.minimumTableColumns),
        QString::number(profile.firstGradeColumn),
        QString::number(profile.neutralFillRed),
        QString::number(profile.neutralFillGreen),
        QString::number(profile.neutralFillBlue),
        signaturePath,
        QString::number(profile.signatureBounds.left()),
        QString::number(profile.signatureBounds.top()),
        QString::number(profile.signatureBounds.width()),
        QString::number(profile.signatureBounds.height()),
        profile.signatureAlignsBottomLeft
            ? QStringLiteral("true")
            : QStringLiteral("false"),
        cancelPath,
        QString::number(batch.students.size())
    };
    for (
        const SpeakingEvalPowerPointJobModel::StudentJob& job :
        batch.students
        )
    {
        arguments.append(
            QStringList{
                SpeakingEvalPowerPointScripts::macTextArgument(job.displayName),
                job.pdfPath,
                job.completionPath,
                SpeakingEvalPowerPointScripts::macTextArgument(job.englishName),
                SpeakingEvalPowerPointScripts::macTextArgument(job.koreanName),
                SpeakingEvalPowerPointScripts::macTextArgument(job.classLabel),
                SpeakingEvalPowerPointScripts::macTextArgument(job.nativeTeacher),
                SpeakingEvalPowerPointScripts::macTextArgument(job.koreanTeacher),
                SpeakingEvalPowerPointScripts::macTextArgument(job.date),
                SpeakingEvalPowerPointScripts::macTextArgument(job.comments),
                SpeakingEvalPowerPointScripts::macTextArgument(
                    QString::fromLatin1(
                        SpeakingEvalPowerPointJobModel::CommentFontName
                        )
                    ),
                QString::number(job.commentsFontSizePoints),
                SpeakingEvalPowerPointScripts::macTextArgument(job.overallGrade),
                job.scores[0],
                job.scores[1],
                job.scores[2],
                job.scores[3],
                job.scores[4],
                job.scores[5]
            }
            );
    }
#else
    Q_UNUSED(jobPath);
    Q_UNUSED(scriptPath);
    Q_UNUSED(cancelPath);
    if (errorMessage)
    {
        *errorMessage = powerPointRendererAvailabilityMessage();
    }
    return PowerPointBatchStatus::Failed;
#endif

    QList<SpeakingEvalPowerPointAutomation::ReportMarker> reports;
    reports.reserve(batch.students.size());
    for (
        const SpeakingEvalPowerPointJobModel::StudentJob& job :
        batch.students
        )
    {
        reports.append({
            job.displayName,
            job.pdfPath,
            job.completionPath
        });
    }

    const SpeakingEvalPowerPointAutomation::Status status =
        SpeakingEvalPowerPointAutomation::run(
            {
                executable,
                arguments,
                automationDirectory,
                cancelPath,
                reports,
                PowerPointTimeoutMs,
                progressCallback
            },
            errorMessage
            );
    switch (status)
    {
    case SpeakingEvalPowerPointAutomation::Status::Completed:
        return PowerPointBatchStatus::Completed;
    case SpeakingEvalPowerPointAutomation::Status::Canceled:
        return PowerPointBatchStatus::Canceled;
    case SpeakingEvalPowerPointAutomation::Status::Failed:
    default:
        return PowerPointBatchStatus::Failed;
    }
}

} // namespace

QString rendererDisplayName(
    Renderer renderer
    )
{
    switch (renderer)
    {
    case Renderer::Internal:
        return QObject::tr("Internal Template (Default)");
    case Renderer::PowerPoint:
        return QObject::tr("PowerPoint (Fallback)");
    }

    return {};
}

QString defaultOutputDirectory(
    const ClassInfo& classInfo,
    const QString& evaluationName,
    const QString& documentsDirectory
    )
{
    return SpeakingEvalReportOutputPolicy::defaultDirectory(
        classInfo,
        evaluationName,
        documentsDirectory
        );
}

QString batchArchivePath(
    const QString& outputDirectory
    )
{
    return SpeakingEvalReportOutputPolicy::batchArchivePath(outputDirectory);
}

QString safeFileName(
    const QString& englishName,
    const QString& koreanName
    )
{
    return SpeakingEvalReportOutputPolicy::studentFileName(
        englishName,
        koreanName
        );
}

bool isPowerPointRendererAvailable()
{
    return SpeakingEvalPowerPointAutomation::isAvailable();
}

QString powerPointRendererAvailabilityMessage()
{
    return SpeakingEvalPowerPointAutomation::availabilityMessage();
}

Result exportReports(
    const Request& request
    )
{
    const auto batchPlanResult =
        classmngr::engine::SpeakingEvaluationBatchReportPolicy::plan(
            portableBatchReportRequest(request)
            );
    if (!batchPlanResult)
    {
        return failed(batchReportPolicyErrorMessage(batchPlanResult.error()));
    }

    const classmngr::engine::SpeakingEvaluationBatchReportPlan& batchPlan =
        *batchPlanResult;

    if (request.renderer == Renderer::PowerPoint
        && !isPowerPointRendererAvailable())
    {
        return failed(powerPointRendererAvailabilityMessage());
    }

    const bool creatingBatchArchive = batchPlan.createsBatchArchive;
    const bool savingIndividualPdfFiles = batchPlan.savesIndividualPdfFiles;

    QStringList targetPaths;
    QString targetArchivePath;
    QString errorMessage;
    if (request.savePdf
        && !SpeakingEvalReportOutput::targetFilePaths(
            request,
            savingIndividualPdfFiles,
            &targetPaths,
            &errorMessage
            ))
    {
        return failed(errorMessage);
    }
    if (creatingBatchArchive)
    {
        targetArchivePath =
            batchArchivePath(request.outputDirectory);
        if (!request.overwriteExisting
            && QFileInfo::exists(targetArchivePath))
        {
            return failed(
                QObject::tr("A ZIP archive named \"%1\" already exists.")
                    .arg(QFileInfo(targetArchivePath).fileName())
                );
        }
    }

    QTemporaryDir stagingDirectory(
        QDir::temp().filePath(
            QStringLiteral("ClassMngr-speaking-evaluations-XXXXXX")
            )
        );
    if (!stagingDirectory.isValid())
    {
        return failed(QObject::tr("A temporary report folder could not be created."));
    }

    QStringList stagedPdfPaths;
    stagedPdfPaths.reserve(request.reports.size());
    SpeakingEvalPowerPointWorkspace powerPointWorkspace;
    if (request.renderer == Renderer::PowerPoint)
    {
        auto documentsLease = ResourcePaths::Documents::acquire();
        if (!documentsLease)
        {
            return failed(documentsLease.error());
        }

        if (!powerPointWorkspace.prepare(
                stagingDirectory.path(),
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }

        QStringList powerPointPdfPaths;
        powerPointPdfPaths.reserve(request.reports.size());
        for (int index = 0; index < request.reports.size(); ++index)
        {
            const QString fileName =
                QStringLiteral("report-%1.pdf")
                    .arg(index, 6, 10, QLatin1Char('0'));
            stagedPdfPaths.append(
                QDir(
                    powerPointWorkspace.automationDirectory()
                    ).filePath(fileName)
                );
            powerPointPdfPaths.append(
                QDir(
                    powerPointWorkspace.presentationDirectory()
                    ).filePath(fileName)
                );
        }

        const SpeakingEvalPowerPointJobModel::BatchJob batch =
            SpeakingEvalPowerPointJobModel::build(
                request.reports,
                powerPointPdfPaths,
                powerPointWorkspace.automationDirectory(),
                ResourcePaths::Documents::directory(*documentsLease)
                );
        const PowerPointBatchStatus powerPointStatus =
            renderPowerPointBatch(
                batch,
                powerPointWorkspace.automationDirectory(),
                powerPointWorkspace.presentationDirectory(),
                request.progressCallback,
                &errorMessage
                );
        if (powerPointStatus == PowerPointBatchStatus::Canceled)
        {
            return canceled();
        }
        if (powerPointStatus == PowerPointBatchStatus::Failed)
        {
            return failed(errorMessage);
        }

        QStringList displayNames;
        displayNames.reserve(batch.students.size());
        for (
            const SpeakingEvalPowerPointJobModel::StudentJob& job :
            batch.students
            )
        {
            displayNames.append(job.displayName);
        }

        if (powerPointWorkspace.usesSeparatePresentationDirectory()
            && !powerPointWorkspace.copyOutputFiles(
                powerPointPdfPaths,
                stagedPdfPaths,
                displayNames,
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }

        if (!powerPointWorkspace.removePresentationDirectory(
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }
    }
    else
    {
        for (int index = 0; index < request.reports.size(); ++index)
        {
            const StudentReport& student = request.reports.at(index);
            if (request.progressCallback
                && !request.progressCallback(
                    index,
                    request.reports.size(),
                    student.displayName
                    ))
            {
                return canceled();
            }

            const QString stagedPath =
                QDir(stagingDirectory.path()).filePath(
                    safeFileName(
                        student.report.englishName,
                        student.report.koreanName
                        )
                    );

            const bool rendered =
                SpeakingEvalInternalPdfRenderer::render(
                    student.report,
                    stagedPath,
                    &errorMessage
                    );

            if (!rendered)
            {
                return failed(
                    QObject::tr("%1: %2")
                        .arg(student.displayName, errorMessage),
                    true
                    );
            }

            stagedPdfPaths.append(stagedPath);
        }

        if (request.progressCallback
            && !request.progressCallback(
                request.reports.size(),
                request.reports.size(),
                QString()
                ))
        {
            return canceled();
        }
    }

    QString stagedArchivePath;
    if (creatingBatchArchive)
    {
        stagedArchivePath =
            QDir(stagingDirectory.path()).filePath(
                QStringLiteral("reports.zip")
                );
        QList<ZipArchiveWriter::Entry> archiveEntries;
        archiveEntries.reserve(stagedPdfPaths.size());
        for (int index = 0; index < stagedPdfPaths.size(); ++index)
        {
            archiveEntries.append({
                stagedPdfPaths.at(index),
                QFileInfo(targetPaths.at(index)).fileName()
            });
        }

        if (!ZipArchiveWriter::writeArchive(
                stagedArchivePath,
                archiveEntries,
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }
    }

    if (request.savePdf)
    {
        QStringList stagedOutputPaths;
        QStringList targetOutputPaths;
        if (creatingBatchArchive)
        {
            stagedOutputPaths.append(stagedArchivePath);
            targetOutputPaths.append(targetArchivePath);
        }
        if (savingIndividualPdfFiles)
        {
            stagedOutputPaths.append(stagedPdfPaths);
            targetOutputPaths.append(targetPaths);
        }

        if (!SpeakingEvalReportOutput::commitFiles(
                stagedOutputPaths,
                targetOutputPaths,
                request.overwriteExisting,
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }
    }

    if (request.printReports)
    {
        const bool printingOneReport =
            request.reports.size() == 1;
        const PdfPrintService::Result printResult =
            PdfPrintService::printPdfDocuments(
                {
                    request.parent,
                    stagedPdfPaths,
                    printingOneReport
                        ? QObject::tr("Print Speaking Evaluation Report")
                        : QObject::tr("Print Speaking Evaluation Reports"),
                    QPageLayout::Portrait,
                    QPageSize::A4,
                    true
                }
                );

        if (printResult.status == PdfPrintService::Status::Canceled)
        {
            return canceled();
        }

        if (printResult.status == PdfPrintService::Status::Failed)
        {
            return failed(printResult.message);
        }
    }

    return completed(
        savingIndividualPdfFiles
            ? targetPaths
            : QStringList(),
        targetArchivePath
        );
}

} // namespace SpeakingEvalBatchReportService
