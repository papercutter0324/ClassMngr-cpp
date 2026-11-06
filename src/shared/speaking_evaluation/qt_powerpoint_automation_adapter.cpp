#include "shared/speaking_evaluation/qt_powerpoint_automation_adapter.h"
#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/services/speaking_eval_report_asset_resolver.h"
#include "features/speaking_eval/services/speaking_eval_powerpoint_job_model.h"
#include "features/speaking_eval/services/speaking_eval_powerpoint_scripts.h"
#include "shared/speaking_evaluation/qt_powerpoint_automation_workspace.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QObject>
#include <QSaveFile>
#include <exception>
#include <utility>

namespace
{
constexpr int PowerPointTimeoutMs = 5 * 60 * 1000;
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


PowerPointBatchStatus renderPowerPointBatch(
    const SpeakingEvalPowerPointJobModel::BatchJob& batch,
    const QString& automationDirectory,
    const QString& presentationDirectory,
    const std::function<
        bool(int completed, int total, const QString& studentName)
        >& progressCallback,
    QString* errorMessage,
    const QtSpeakingEvalPowerPoint::Actions& actions
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
            actions.host == QtSpeakingEvalPowerPoint::Host::Windows
                ? QStringLiteral("export-batch.ps1")
                : QStringLiteral("export-batch.applescript")
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

    if (actions.host == QtSpeakingEvalPowerPoint::Host::Windows)
    {
        executable = actions.executable ? actions.executable()
            : QtSpeakingEvalPowerPoint::executable();
        if (executable.isEmpty())
        {
            if (errorMessage)
            {
                *errorMessage = QtSpeakingEvalPowerPoint::availabilityMessage();
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
    }
    else if (actions.host == QtSpeakingEvalPowerPoint::Host::MacOS)
    {
        executable = actions.executable ? actions.executable()
            : QtSpeakingEvalPowerPoint::executable();
        if (executable.isEmpty())
        {
            if (errorMessage)
            {
                *errorMessage = QtSpeakingEvalPowerPoint::availabilityMessage();
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
    }
    else
    {
        Q_UNUSED(jobPath);
        Q_UNUSED(scriptPath);
        Q_UNUSED(cancelPath);
        if (errorMessage)
        {
            *errorMessage = QtSpeakingEvalPowerPoint::availabilityMessage();
        }
        return PowerPointBatchStatus::Failed;
    }

    QList<QtSpeakingEvalPowerPoint::ReportMarker> reports;
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

    const QtSpeakingEvalPowerPoint::ProcessRequest automationRequest{
        executable,
        arguments,
        automationDirectory,
        cancelPath,
        reports,
        PowerPointTimeoutMs,
        progressCallback
    };
    const auto status = actions.run
        ? actions.run(automationRequest, errorMessage)
        : QtSpeakingEvalPowerPoint::run(automationRequest, errorMessage);
    switch (status)
    {
    case QtSpeakingEvalPowerPoint::Status::Completed:
        return PowerPointBatchStatus::Completed;
    case QtSpeakingEvalPowerPoint::Status::Canceled:
        return PowerPointBatchStatus::Canceled;
    case QtSpeakingEvalPowerPoint::Status::Failed:
    default:
        return PowerPointBatchStatus::Failed;
    }
}


class QtPowerPointAutomation final : public SpeakingEvalPowerPointAutomationPort
{
public:
    explicit QtPowerPointAutomation(QtSpeakingEvalPowerPoint::Actions actions)
        : m_actions(std::move(actions)) {}

    SpeakingEvalPowerPointAvailability availability() const override
    {
        return m_actions.availability ? m_actions.availability()
            : QtSpeakingEvalPowerPoint::availability();
    }

    DocumentOutputResult renderBatch(
        const SpeakingEvalPowerPointBatchRequest& request) override
    {
        using Status = DocumentOutputStatus;
        const auto available = availability();
        if (!available.available) return {Status::Failed, available.message};
        if (request.reports.isEmpty()
            || request.reports.size() != request.outputPaths.size()
            || request.documentsRoot.trimmed().isEmpty())
            return {Status::Failed, QObject::tr("The PowerPoint template resources are unavailable.")};

        SpeakingEvalPowerPointWorkspace workspace;
        QString error;
        if (!workspace.prepare(request.stagingDirectory, &error, m_actions.workspaceRoot))
            return {Status::Failed, error};
        const auto finish = [&](DocumentOutputResult result) {
            QString cleanupError;
            bool released = false;
            try
            {
                released = m_actions.releaseWorkspace
                    ? m_actions.releaseWorkspace(workspace, &cleanupError)
                    : workspace.release(&cleanupError);
            }
            catch (...)
            {
                cleanupError = QObject::tr("PowerPoint's temporary report files could not be removed.");
            }
            if (!released && cleanupError.trimmed().isEmpty())
                cleanupError = QObject::tr("PowerPoint's temporary report files could not be removed.");
            if (!released)
                return DocumentOutputResult{Status::Failed,
                    result.message.isEmpty() ? cleanupError
                        : result.message + QLatin1Char('\n') + cleanupError};
            return result;
        };
        try
        {
            if (m_actions.workspaceCreated)
                m_actions.workspaceCreated(workspace.automationDirectory(),
                    workspace.presentationDirectory());
            QStringList presentationPaths;
            for (int index = 0; index < request.reports.size(); ++index)
                presentationPaths.append(QDir(workspace.presentationDirectory()).filePath(
                    QStringLiteral("report-%1.pdf").arg(index, 6, 10, QLatin1Char('0'))));
            const auto batch = SpeakingEvalPowerPointJobModel::build(
                request.reports, presentationPaths, workspace.automationDirectory(),
                request.documentsRoot);
            const auto status = renderPowerPointBatch(batch,
                workspace.automationDirectory(), workspace.presentationDirectory(),
                request.progressCallback, &error, m_actions);
            if (status == PowerPointBatchStatus::Canceled)
                return finish({Status::Canceled, QObject::tr("Report export was canceled.")});
            if (status == PowerPointBatchStatus::Failed)
                return finish({Status::Failed, error});
            QStringList displayNames;
            for (const auto& report : request.reports) displayNames.append(report.displayName);
            if (!workspace.copyOutputFiles(presentationPaths, request.outputPaths,
                    displayNames, &error))
                return finish({Status::Failed, error});
            return finish({Status::Completed, {}});
        }
        catch (const std::exception&)
        {
            return finish({Status::Failed, QObject::tr("PowerPoint could not export the reports.")});
        }
        catch (...)
        {
            return finish({Status::Failed, QObject::tr("PowerPoint could not export the reports.")});
        }
    }
private:
    QtSpeakingEvalPowerPoint::Actions m_actions;
};
}

std::shared_ptr<SpeakingEvalPowerPointAutomationPort>
makeQtSpeakingEvalPowerPointAutomationPort(QtSpeakingEvalPowerPoint::Actions actions)
{
    return std::make_shared<QtPowerPointAutomation>(std::move(actions));
}
