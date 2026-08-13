#pragma once

#include <QString>
#include <QStringList>

#include <memory>

class QTemporaryDir;

class SpeakingEvalPowerPointWorkspace final
{
public:
    SpeakingEvalPowerPointWorkspace() = default;
    ~SpeakingEvalPowerPointWorkspace();

    SpeakingEvalPowerPointWorkspace(
        const SpeakingEvalPowerPointWorkspace&
        ) = delete;
    SpeakingEvalPowerPointWorkspace& operator=(
        const SpeakingEvalPowerPointWorkspace&
        ) = delete;

    [[nodiscard]] bool prepare(
        const QString& stagingDirectory,
        QString* errorMessage
        );

    [[nodiscard]] QString automationDirectory() const;
    [[nodiscard]] QString presentationDirectory() const;
    [[nodiscard]] bool usesSeparatePresentationDirectory() const;

    [[nodiscard]] bool copyOutputFiles(
        const QStringList& sourcePaths,
        const QStringList& stagedPaths,
        const QStringList& displayNames,
        QString* errorMessage
        ) const;

    [[nodiscard]] bool removePresentationDirectory(
        QString* errorMessage
        );

private:
    void cleanup();

    QString m_automationDirectory;
    QString m_presentationDirectory;
    std::unique_ptr<QTemporaryDir> m_temporaryDirectory;
    bool m_cleanupAutomationDirectory = false;
    bool m_cleanupPresentationDirectory = false;
};
