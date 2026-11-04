#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/services/speaking_eval_report_output_policy.h"
#include "features/speaking_eval/services/speaking_eval_report_output.h"
#include "features/speaking_eval/services/speaking_eval_internal_pdf_renderer.h"
#include "features/speaking_eval/services/speaking_eval_powerpoint_job_model.h"

#include "core/utils/file_name_utils.h"
#include "core/zip_archive_writer.h"
#include "domain/models/speaking_evaluation.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QTemporaryDir>

namespace SpeakingEvalBatchReportService
{
namespace
{

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

Result canceled(
    const QStringList& savedPdfPaths = {},
    const QString& savedArchivePath = {}
    )
{
    return {
        Status::Canceled,
        savedPdfPaths.isEmpty() && savedArchivePath.isEmpty()
            ? QObject::tr("Report export was canceled.")
            : QObject::tr("Printing was canceled after the reports were saved."),
        savedPdfPaths,
        savedArchivePath
    };
}

Result failed(
    const QString& message,
    bool internalRendererFailed = false,
    const QStringList& savedPdfPaths = {},
    const QString& savedArchivePath = {}
    )
{
    return {
        internalRendererFailed
            ? Status::InternalRendererFailed
            : Status::Failed,
        message,
        savedPdfPaths,
        savedArchivePath
    };
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

Result exportResolvedReports(
    const Request& request
    )
{
    if (static_cast<std::size_t>(request.reports.size())
        > static_cast<std::size_t>(SpeakingEval::MaximumRowCount))
        return failed(QObject::tr("A report batch may contain at most 100 students."));

    if (request.reports.isEmpty())
    {
        return failed(QObject::tr("There are no student reports to export."));
    }

    if (!request.savePdf && !request.printReports)
    {
        return failed(QObject::tr("Choose PDF saving, printing, or both."));
    }

    if (request.savePdf
        && request.outputDirectory.trimmed().isEmpty()
        && request.outputFilePath.trimmed().isEmpty())
    {
        return failed(QObject::tr("Choose a destination for the PDF reports."));
    }

    if (
        request.renderer == Renderer::PowerPoint
        && !SpeakingEvalPowerPointJobModel::usesSingleTemplate(
            request.reports
            )
        )
    {
        return failed(
            QObject::tr(
                "All reports in a PowerPoint batch must use the same template."
                )
            );
    }

    if (request.renderer == Renderer::PowerPoint)
    {
        const auto availability = request.powerPointAutomation
            ? request.powerPointAutomation->availability()
            : SpeakingEvalPowerPointAvailability{false,
                QObject::tr("The PowerPoint automation operation is unavailable.")};
        if (!availability.available) return failed(availability.message);
    }

    const bool creatingBatchArchive =
        request.savePdf && request.reports.size() > 1;
    const bool savingIndividualPdfFiles =
        request.savePdf
        && (!creatingBatchArchive
            || request.keepIndividualPdfFiles);

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

    if (request.stagingDirectoryCreated)
        request.stagingDirectoryCreated(stagingDirectory.path());

    QStringList stagedPdfPaths;
    stagedPdfPaths.reserve(request.reports.size());
    if (request.renderer == Renderer::PowerPoint)
    {
        for (int index = 0; index < request.reports.size(); ++index)
            stagedPdfPaths.append(stagingDirectory.filePath(
                QStringLiteral("report-%1.pdf").arg(index, 6, 10, QLatin1Char('0'))));
        const Result rendered = request.powerPointAutomation->renderBatch({
            request.reports,
            request.documentsRoot,
            stagedPdfPaths,
            stagingDirectory.path(),
            request.progressCallback
        });
        if (rendered.status != Status::Completed) return rendered;
        for (const QString& path : stagedPdfPaths)
            if (!QFileInfo(path).isFile() || QFileInfo(path).size() <= 0)
                return failed(QObject::tr("PowerPoint did not create every report PDF."));
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
                request.internalRenderer
                    ? request.internalRenderer(student.report, stagedPath, &errorMessage)
                    : SpeakingEvalInternalPdfRenderer::render(student.report, stagedPath, &errorMessage);

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
        const QString printTitle = printingOneReport
            ? QObject::tr("Print Speaking Evaluation Report")
            : QObject::tr("Print Speaking Evaluation Reports");
        const PdfPrintService::Result printResult = request.printCallback
            ? request.printCallback(request.parent, stagedPdfPaths, printTitle)
            : PdfPrintService::printPdfDocuments(
                  {
                      request.parent,
                      stagedPdfPaths,
                      printTitle,
                      QPageLayout::Portrait,
                      QPageSize::A4,
                      true
                  }
                  );

        const QStringList savedPdfPaths = savingIndividualPdfFiles
            ? targetPaths
            : QStringList();
        const QString savedArchivePath = creatingBatchArchive
            ? targetArchivePath
            : QString();

        if (printResult.status == PdfPrintService::Status::Canceled)
        {
            return canceled(savedPdfPaths, savedArchivePath);
        }

        if (printResult.status == PdfPrintService::Status::Failed)
        {
            return failed(
                printResult.message,
                false,
                savedPdfPaths,
                savedArchivePath
                );
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
