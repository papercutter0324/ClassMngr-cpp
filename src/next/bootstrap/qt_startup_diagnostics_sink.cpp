#include "next/bootstrap/qt_startup_diagnostics_sink.h"

#include <QByteArray>
#include <QDebug>

#include <cstdio>

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

[[nodiscard]] QByteArray startupRecord(const Application::StartupEvent& event)
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
    void record(const Application::StartupEvent& event) noexcept override
    {
        try
        {
            const QByteArray record = startupRecord(event);
            writeQtStartupRecord(event, record);
        }
        catch (...)
        {
            std::fprintf(stderr, "ClassMngrNext startup diagnostics failed.\n");
        }
    }
};

}  // namespace

std::unique_ptr<StartupDiagnosticsSink> makeQtStartupDiagnosticsSink()
{
    return std::make_unique<QtStartupDiagnosticsSink>();
}

void writeEarlyQtStartupDiagnostic(
    const Application::StartupEvent& event
    ) noexcept
{
    try
    {
        const QByteArray record = startupRecord(event);
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
