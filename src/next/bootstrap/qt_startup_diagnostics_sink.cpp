#include "next/bootstrap/qt_startup_diagnostics_sink.h"

#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cstdio>
#include <utility>

namespace ClassMngr::Next::Bootstrap
{
namespace
{

[[nodiscard]] const char* stageName(Application::StartupStage stage) noexcept
{
    switch (stage)
    {
    case Application::StartupStage::ProcessCreated:
        return "process-created";
    case Application::StartupStage::CoreServicesCreated:
        return "core-services-created";
    case Application::StartupStage::SettingsLoaded:
        return "settings-loaded";
    case Application::StartupStage::LanguageLoaded:
        return "language-loaded";
    case Application::StartupStage::ThemeLoaded:
        return "theme-loaded";
    case Application::StartupStage::RequiredFontsLoaded:
        return "required-fonts-loaded";
    case Application::StartupStage::WorkspaceOpened:
        return "workspace-opened";
    case Application::StartupStage::ShellCreated:
        return "shell-created";
    case Application::StartupStage::InitialPageReady:
        return "initial-page-ready";
    case Application::StartupStage::StartupReady:
        return "startup-ready";
    }

    return "unknown-stage";
}

[[nodiscard]] const char* outcomeName(
    Application::StartupStageOutcome outcome
    ) noexcept
{
    switch (outcome)
    {
    case Application::StartupStageOutcome::Completed:
        return "completed";
    case Application::StartupStageOutcome::Warning:
        return "warning";
    case Application::StartupStageOutcome::Failed:
        return "failed";
    }

    return "unknown-outcome";
}

[[nodiscard]] const char* diagnosticName(
    Application::StartupDiagnosticCode diagnostic
    ) noexcept
{
    switch (diagnostic)
    {
    case Application::StartupDiagnosticCode::StageWarning:
        return "stage-warning";
    case Application::StartupDiagnosticCode::StageFailed:
        return "stage-failed";
    case Application::StartupDiagnosticCode::AlreadyStarted:
        return "already-started";
    case Application::StartupDiagnosticCode::InvalidTransition:
        return "invalid-transition";
    case Application::StartupDiagnosticCode::DuplicateStage:
        return "duplicate-stage";
    case Application::StartupDiagnosticCode::BackwardStage:
        return "backward-stage";
    case Application::StartupDiagnosticCode::StartupReadyPrerequisitesIncomplete:
        return "startup-ready-prerequisites-incomplete";
    case Application::StartupDiagnosticCode::InitialExperienceUnavailable:
        return "initial-experience-unavailable";
    }

    return "unknown-diagnostic";
}

[[nodiscard]] QJsonValue memoryCounterValue(
    const bool available,
    const quint64 value
    )
{
    return available
        ? QJsonValue(static_cast<qint64>(value))
        : QJsonValue(QJsonValue::Null);
}

[[nodiscard]] QJsonObject memoryMetrics(
    const ProcessMemorySnapshot& snapshot
    )
{
    QJsonObject metrics;
    if (!snapshot.isAvailable)
    {
        return metrics;
    }

    if (snapshot.platform == QStringLiteral("windows"))
    {
        metrics.insert(
            QStringLiteral("working_set_bytes"),
            memoryCounterValue(
                snapshot.workingSetBytesAvailable,
                snapshot.workingSetBytes
                )
            );
        metrics.insert(
            QStringLiteral("peak_working_set_bytes"),
            memoryCounterValue(
                snapshot.peakWorkingSetBytesAvailable,
                snapshot.peakWorkingSetBytes
                )
            );
        metrics.insert(
            QStringLiteral("private_usage_bytes"),
            memoryCounterValue(
                snapshot.privateUsageBytesAvailable,
                snapshot.privateUsageBytes
                )
            );
        metrics.insert(
            QStringLiteral("pagefile_usage_bytes"),
            memoryCounterValue(
                snapshot.pagefileUsageBytesAvailable,
                snapshot.pagefileUsageBytes
                )
            );
        metrics.insert(
            QStringLiteral("handle_count"),
            memoryCounterValue(
                snapshot.handleCountAvailable,
                snapshot.handleCount
                )
            );
        metrics.insert(
            QStringLiteral("thread_count"),
            memoryCounterValue(
                snapshot.threadCountAvailable,
                snapshot.threadCount
                )
            );
        metrics.insert(
            QStringLiteral("private_working_set_bytes"),
            memoryCounterValue(
                snapshot.privateWorkingSetBytesAvailable,
                snapshot.privateWorkingSetBytes
                )
            );
    }
    else if (snapshot.platform == QStringLiteral("macos"))
    {
        metrics.insert(
            QStringLiteral("resident_bytes"),
            memoryCounterValue(
                snapshot.workingSetBytesAvailable,
                snapshot.workingSetBytes
                )
            );
        metrics.insert(
            QStringLiteral("peak_resident_bytes"),
            memoryCounterValue(
                snapshot.peakWorkingSetBytesAvailable,
                snapshot.peakWorkingSetBytes
                )
            );
        metrics.insert(
            QStringLiteral("physical_footprint_bytes"),
            memoryCounterValue(
                snapshot.privateUsageBytesAvailable,
                snapshot.privateUsageBytes
                )
            );
        metrics.insert(
            QStringLiteral("compressed_bytes"),
            memoryCounterValue(
                snapshot.pagefileUsageBytesAvailable,
                snapshot.pagefileUsageBytes
                )
            );
        metrics.insert(
            QStringLiteral("thread_count"),
            memoryCounterValue(
                snapshot.threadCountAvailable,
                snapshot.threadCount
                )
            );
        metrics.insert(
            QStringLiteral("internal_bytes"),
            memoryCounterValue(
                snapshot.privateWorkingSetBytesAvailable,
                snapshot.privateWorkingSetBytes
                )
            );
    }
    else if (snapshot.platform == QStringLiteral("linux"))
    {
        metrics.insert(
            QStringLiteral("rss_bytes"),
            memoryCounterValue(
                snapshot.workingSetBytesAvailable,
                snapshot.workingSetBytes
                )
            );
        metrics.insert(
            QStringLiteral("peak_rss_bytes"),
            memoryCounterValue(
                snapshot.peakWorkingSetBytesAvailable,
                snapshot.peakWorkingSetBytes
                )
            );
        metrics.insert(
            QStringLiteral("private_usage_bytes"),
            memoryCounterValue(
                snapshot.privateUsageBytesAvailable,
                snapshot.privateUsageBytes
                )
            );
        metrics.insert(
            QStringLiteral("private_clean_and_dirty_bytes"),
            memoryCounterValue(
                snapshot.pagefileUsageBytesAvailable,
                snapshot.pagefileUsageBytes
                )
            );
        metrics.insert(
            QStringLiteral("file_descriptor_count"),
            memoryCounterValue(
                snapshot.handleCountAvailable,
                snapshot.handleCount
                )
            );
        metrics.insert(
            QStringLiteral("thread_count"),
            memoryCounterValue(
                snapshot.threadCountAvailable,
                snapshot.threadCount
                )
            );
        metrics.insert(
            QStringLiteral("anonymous_resident_bytes"),
            memoryCounterValue(
                snapshot.privateWorkingSetBytesAvailable,
                snapshot.privateWorkingSetBytes
                )
            );
        metrics.insert(
            QStringLiteral("private_dirty_bytes"),
            memoryCounterValue(
                snapshot.privateDirtyBytesAvailable,
                snapshot.privateDirtyBytes
                )
            );
    }

    return metrics;
}

[[nodiscard]] QByteArray startupRecord(
    const Application::StartupEvent& event,
    const ProcessMemorySnapshot& snapshot
    )
{
    QByteArray record("ClassMngrNext startup stage=");
    record += stageName(event.stage);
    record += " outcome=";
    record += outcomeName(event.outcome);
    record += " diagnostic=";
    if (event.diagnostic.has_value())
    {
        record += diagnosticName(event.diagnostic->code);
        record += " diagnostic_stage=";
        record += event.diagnostic->stage.has_value()
            ? stageName(*event.diagnostic->stage)
            : "none";
    }
    else
    {
        record += "none diagnostic_stage=none";
    }
    record += " checkpoint=";
    record += QJsonDocument(QJsonObject{
        {QStringLiteral("stage"), stageName(event.stage)},
        {QStringLiteral("outcome"), outcomeName(event.outcome)},
        {QStringLiteral("sample_available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("metrics"), memoryMetrics(snapshot)}
    }).toJson(QJsonDocument::Compact);
    return record;
}

void writeQtStartupRecord(
    const Application::StartupEvent& event,
    const QByteArray& record
    ) noexcept
{
    switch (event.outcome)
    {
    case Application::StartupStageOutcome::Completed:
        qInfo().noquote().nospace() << record;
        break;
    case Application::StartupStageOutcome::Warning:
        qWarning().noquote().nospace() << record;
        break;
    case Application::StartupStageOutcome::Failed:
        qCritical().noquote().nospace() << record;
        break;
    }
}

class QtStartupDiagnosticsSink final : public StartupDiagnosticsSink
{
public:
    explicit QtStartupDiagnosticsSink(
        std::unique_ptr<ProcessMemorySnapshotProvider> snapshotProvider
        )
        : m_snapshotProvider(snapshotProvider
              ? std::move(snapshotProvider)
              : std::make_unique<PlatformProcessMemorySnapshotProvider>())
    {
    }

    void record(const Application::StartupEvent& event) noexcept override
    {
        try
        {
            const ProcessMemorySnapshot snapshot =
                m_snapshotProvider->snapshot();
            const QByteArray record = startupRecord(event, snapshot);
            writeQtStartupRecord(event, record);
        }
        catch (...)
        {
            std::fprintf(stderr, "ClassMngrNext startup diagnostics failed.\n");
        }
    }

private:
    std::unique_ptr<ProcessMemorySnapshotProvider> m_snapshotProvider;
};

}  // namespace

std::unique_ptr<StartupDiagnosticsSink> makeQtStartupDiagnosticsSink(
    std::unique_ptr<ProcessMemorySnapshotProvider> snapshotProvider
    )
{
    return std::make_unique<QtStartupDiagnosticsSink>(
        std::move(snapshotProvider)
        );
}

void writeEarlyQtStartupDiagnostic(
    const Application::StartupEvent& event
    ) noexcept
{
    try
    {
        const ProcessMemorySnapshot snapshot =
            PlatformProcessMemorySnapshotProvider().snapshot();
        const QByteArray record = startupRecord(event, snapshot);
        writeQtStartupRecord(event, record);
        std::fprintf(stderr, "%s\n", record.constData());
    }
    catch (...)
    {
        std::fprintf(stderr, "ClassMngrNext startup diagnostics failed.\n");
    }
    std::fflush(stderr);
}

}  // namespace ClassMngr::Next::Bootstrap
