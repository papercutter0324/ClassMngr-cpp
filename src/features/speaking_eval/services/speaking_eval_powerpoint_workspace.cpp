#include "speaking_eval_powerpoint_workspace.h"

#include "speaking_eval_report_asset_resolver.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace
{
bool removeDirectory(
    const QString& path
    )
{
    if (path.trimmed().isEmpty())
    {
        return false;
    }

    QDir directory(path);
    return !directory.exists() || directory.removeRecursively();
}

bool resetDirectory(
    const QString& path
    )
{
    return removeDirectory(path) && QDir().mkpath(path);
}
}

SpeakingEvalPowerPointWorkspace::~SpeakingEvalPowerPointWorkspace()
{
    cleanup();
}

bool SpeakingEvalPowerPointWorkspace::prepare(
    const QString& stagingDirectory,
    QString* errorMessage
    )
{
#ifdef Q_OS_MACOS
    Q_UNUSED(stagingDirectory);

    m_automationDirectory =
        QDir(
            QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation
                )
            ).filePath(QStringLiteral("PowerPointBatch"));
    m_presentationDirectory =
        QDir(
            QStandardPaths::writableLocation(
                QStandardPaths::HomeLocation
                )
            ).filePath(
                QStringLiteral(
                    "Library/Containers/com.microsoft.Powerpoint/Data/Documents/ClassMngr/PowerPointBatch"
                    )
                );

    m_cleanupAutomationDirectory = true;
    if (!resetDirectory(m_automationDirectory))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "ClassMngr's PowerPoint workspace could not be prepared."
                );
        }
        cleanup();
        return false;
    }

    m_cleanupPresentationDirectory = true;
    if (!resetDirectory(m_presentationDirectory))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "PowerPoint's private workspace could not be prepared."
                );
        }
        cleanup();
        return false;
    }
#else
    m_temporaryDirectory =
        std::make_unique<QTemporaryDir>(
            QDir(stagingDirectory).filePath(
                QStringLiteral("powerpoint-batch-XXXXXX")
                )
            );
    if (!m_temporaryDirectory->isValid())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "A temporary PowerPoint batch folder could not be created."
                );
        }
        return false;
    }

    m_automationDirectory = m_temporaryDirectory->path();
    m_presentationDirectory = m_automationDirectory;
#endif

    return true;
}

QString SpeakingEvalPowerPointWorkspace::automationDirectory() const
{
    return m_automationDirectory;
}

QString SpeakingEvalPowerPointWorkspace::presentationDirectory() const
{
    return m_presentationDirectory;
}

bool SpeakingEvalPowerPointWorkspace::usesSeparatePresentationDirectory() const
{
    return m_automationDirectory != m_presentationDirectory;
}

bool SpeakingEvalPowerPointWorkspace::copyOutputFiles(
    const QStringList& sourcePaths,
    const QStringList& stagedPaths,
    const QStringList& displayNames,
    QString* errorMessage
    ) const
{
    if (
        sourcePaths.size() != stagedPaths.size()
        || sourcePaths.size() != displayNames.size()
        )
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "The PowerPoint batch output is incomplete."
                );
        }
        return false;
    }

    for (int index = 0; index < sourcePaths.size(); ++index)
    {
        const QString stagedPath = stagedPaths.at(index);
        if (!SpeakingEvalReportAssetResolver::copyFileReplacing(
                sourcePaths.at(index),
                stagedPath,
                QObject::tr(
                    "The PowerPoint PDF for %1 could not be copied into ClassMngr's workspace."
                    ).arg(displayNames.at(index)),
                errorMessage
                )
            || !QFileInfo::exists(stagedPath)
            || QFileInfo(stagedPath).size() <= 0)
        {
            if (errorMessage && errorMessage->isEmpty())
            {
                *errorMessage = QObject::tr(
                    "The staged PowerPoint PDF for %1 is incomplete."
                    ).arg(displayNames.at(index));
            }
            return false;
        }
    }

    return true;
}

bool SpeakingEvalPowerPointWorkspace::removePresentationDirectory(
    QString* errorMessage
    )
{
#ifdef Q_OS_MACOS
    if (
        m_cleanupPresentationDirectory
        && !removeDirectory(m_presentationDirectory)
        )
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "PowerPoint's temporary report files could not be removed."
                );
        }
        return false;
    }
    m_cleanupPresentationDirectory = false;
#else
    Q_UNUSED(errorMessage);
#endif
    return true;
}

void SpeakingEvalPowerPointWorkspace::cleanup()
{
#ifdef Q_OS_MACOS
    if (m_cleanupPresentationDirectory)
    {
        removeDirectory(m_presentationDirectory);
        m_cleanupPresentationDirectory = false;
    }
    if (m_cleanupAutomationDirectory)
    {
        removeDirectory(m_automationDirectory);
        m_cleanupAutomationDirectory = false;
    }
#endif
}
