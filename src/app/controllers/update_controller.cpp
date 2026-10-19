#include "update_controller.h"

#include "app/mainwindow.h"
#include "core/updater/update_configuration.h"
#include "core/updater/update_downloader.h"
#include "core/updater/update_service.h"
#include "core/updater/version.h"
#include "next/application/startup_update_check_use_case.h"
#include "next/application/skipped_update_version_policy.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"
#include "next/platform/settings_manager_skipped_update_version_port.h"
#include "next/platform/update_service_manual_check_port.h"
#include "next/platform/update_service_startup_check_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QAction>
#include <QDialog>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{

ClassMngr::Next::Application::UpdateVersion toApplicationVersion(
    const Version& version
    )
{
    return {
        .major = version.majorVersion(),
        .minor = version.minorVersion(),
        .patch = version.patchVersion()
    };
}

} // namespace

UpdateController::UpdateController(
    UpdateService* service,
    QObject* parent,
    std::shared_ptr<
        ClassMngr::Next::Application::
            SkippedUpdateVersionPreferencesPort
        > skippedUpdateVersionPreferencesPort
    )
    : QObject(parent)
    , m_service(service)
    , m_skippedUpdateVersionPreferencesPort(
        std::move(skippedUpdateVersionPreferencesPort)
        )
{
    Q_ASSERT(m_service);

    if (!m_skippedUpdateVersionPreferencesPort)
    {
        m_skippedUpdateVersionPreferencesPort = std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerSkippedUpdateVersionPort
            >();
    }

    // Install the legacy/manual route before the bridge. During a startup
    // signal this route sees the active request and leaves its effects to the
    // application completion below. Same-thread Qt connections run in order.
    connect(
        m_service,
        &UpdateService::checkSucceeded,
        this,
        [this](const UpdateCheckResult& result)
        {
            if (!m_startupCheckActive)
            {
                handleCheckSucceeded(
                    ClassMngr::Next::Platform::toStartupUpdateCheckResult(result)
                    );
            }
        }
        );
    m_startupUpdateCheckPort = std::make_unique<
        ClassMngr::Next::Platform::UpdateServiceStartupCheckPort>(m_service);
    m_startupUpdateCheckUseCase = std::make_unique<
        ClassMngr::Next::Application::StartupUpdateCheckUseCase>(
            *m_startupUpdateCheckPort);
}

UpdateController::~UpdateController() = default;

void UpdateController::handleCheckSucceeded(
    const ClassMngr::Next::Application::StartupUpdateCheckResult& result
    )
{
    const bool latestVersionIsSkipped = reconcileSkippedVersion(result);
    const UpdateArtifact artifact{
        .platformKey = QString::fromStdString(result.artifact.platformKey.value()),
        .url = QUrl::fromEncoded(QByteArray::fromStdString(result.artifact.url)),
        .fileName = QString::fromStdString(result.artifact.fileName),
        .sha256 = QString::fromStdString(result.artifact.sha256),
        .sizeBytes = result.artifact.sizeBytes
    };
    UpdateDownloader::cleanupDownloads(
        result.updateAvailable
            ? UpdateDownloader::CleanupMode::KeepOnlyArtifact
            : UpdateDownloader::CleanupMode::RemoveAll,
        result.updateAvailable ? std::optional<UpdateArtifact>(artifact) : std::nullopt
        );

    if (result.updateAvailable && !hasVisibleDialog()
        && !m_automaticPromptSuppressed && automaticChecksEnabled()
        && !latestVersionIsSkipped)
    {
        showAutomaticUpdateDialog();
    }
}

bool UpdateController::automaticChecksEnabled() const
{
    const ClassMngr::Next::Platform::
        SettingsManagerAutomaticUpdatePreferencesPort
        automaticUpdatePreferencesPort;
    return m_service
        && m_service->configuration().checkOnStartup
        && automaticUpdatePreferencesPort.read().automaticChecksEnabled;
}

bool UpdateController::reconcileSkippedVersion(
    const ClassMngr::Next::Application::StartupUpdateCheckResult& result
    )
{
    const auto storedSkippedVersion =
        m_skippedUpdateVersionPreferencesPort->read().skippedVersion;

    using ClassMngr::Next::Application::
        SkippedUpdateVersionPolicyInput;
    using ClassMngr::Next::Application::
        SkippedUpdateVersionState;
    using ClassMngr::Next::Application::
        decideSkippedUpdateVersion;

    SkippedUpdateVersionPolicyInput input;
    input.currentVersion = result.currentVersion;
    input.latestVersion = result.latestVersion;
    const std::string latestVersionText =
        std::to_string(result.latestVersion.major) + "."
        + std::to_string(result.latestVersion.minor) + "."
        + std::to_string(result.latestVersion.patch);
    input.latestVersionText = std::string_view(latestVersionText);

    if (!storedSkippedVersion)
    {
        input.state = SkippedUpdateVersionState::Missing;
    }
    else
    {
        input.storedVersionText = *storedSkippedVersion;
        const auto parsedSkippedVersion = Version::parse(
            QString::fromStdString(storedSkippedVersion->value())
            );
        if (parsedSkippedVersion)
        {
            input.state = SkippedUpdateVersionState::Valid;
            input.skippedVersion =
                toApplicationVersion(*parsedSkippedVersion);
        }
        else
        {
            input.state = SkippedUpdateVersionState::Invalid;
        }
    }

    const auto decision =
        decideSkippedUpdateVersion(input);
    if (decision.clearStoredVersion)
    {
        const auto cleared =
            m_skippedUpdateVersionPreferencesPort->clear();
        if (cleared)
        {
            if (m_dialog)
            {
                m_dialog->setSkippedVersion(QString());
            }
        }
        // Prompt eligibility follows the policy decision even when storage
        // cleanup fails; the retained key can be cleared by a later check.
    }

    return decision.suppressAutomaticPrompt;
}

ClassMngr::Next::Domain::Result<void> UpdateController::skipVersion(
    const QString& version
    )
{
    const auto parsed =
        Version::parse(version);
    if (!parsed)
    {
        return ClassMngr::Next::Domain::Result<void>::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
            .message = "The selected update version is invalid.",
            .recoverable = false
        });
    }

    const auto saved = m_skippedUpdateVersionPreferencesPort->write({
        .skippedVersion =
            ClassMngr::Next::Application::SkippedUpdateVersionText(
                parsed->toString().toUtf8().toStdString()
                )
    });
    if (!saved)
    {
        return saved;
    }

    m_automaticPromptSuppressed =
        true;
    return ClassMngr::Next::Domain::Result<void>::success();
}

ClassMngr::Next::Domain::Result<void> UpdateController::unskipVersion()
{
    const auto cleared =
        m_skippedUpdateVersionPreferencesPort->clear();
    if (!cleared)
    {
        return cleared;
    }

    m_automaticPromptSuppressed =
        false;
    return ClassMngr::Next::Domain::Result<void>::success();
}

void UpdateController::attachMainWindow(
    MainWindow* window,
    ActionRegistry& actions
    )
{
    m_window =
        window;

    connect(
        window,
        &QObject::destroyed,
        this,
        [this]()
        {
            if (m_dialog)
            {
                m_dialog->close();
            }

            m_window =
                nullptr;
        }
        );

    if (actions.checkForUpdates)
    {
        connect(
            actions.checkForUpdates,
            &QAction::triggered,
            this,
            &UpdateController::showManualUpdateDialog,
            Qt::UniqueConnection
            );
    }
}

void UpdateController::runStartupMaintenance()
{
    if (m_startupMaintenanceRun)
    {
        return;
    }

    m_startupMaintenanceRun = true;
    UpdateDownloader::cleanupDownloads(
        UpdateDownloader::CleanupMode::OrphansOnly
        );
}

void UpdateController::startAutomaticCheck()
{
    if (!m_startupComplete || !m_service)
    {
        return;
    }
    runStartupMaintenance();

    const UpdateConfiguration configuration = m_service->configuration();
    const ClassMngr::Next::Platform::SettingsManagerAutomaticUpdatePreferencesPort
        automaticUpdatePreferencesPort;
    using namespace ClassMngr::Next::Application;
    const QPointer<UpdateController> guard(this);
    // Set before dispatch because a configuration failure can complete inline.
    // Do not reset an already active request on a repeated startup call.
    if (m_startupCheckActive)
    {
        return;
    }
    m_startupCheckActive = true;
    const auto dispatch = m_startupUpdateCheckUseCase->start({
        .startupComplete = m_startupComplete,
        .checkOnStartup = configuration.checkOnStartup,
        .preferences = automaticUpdatePreferencesPort.read(),
        .hasReleasesApiUrl = configuration.hasReleasesApiUrl()
    }, [guard](StartupUpdateCheckOutcome outcome)
    {
        if (!guard)
        {
            return;
        }
        guard->m_startupCheckActive = false;
        if (const auto* result = std::get_if<StartupUpdateCheckResult>(&outcome))
        {
            guard->handleCheckSucceeded(*result);
        }
        // Automatic failures remain silent. A manual dialog retains its own
        // service signal handling and retry behavior.
    });
    if (dispatch != StartupUpdateCheckDisposition::Started)
    {
        m_startupCheckActive = false;
    }
}

void UpdateController::setStartupComplete()
{
    m_startupComplete =
        true;

    if (m_dialog)
    {
        m_dialog->setStartupComplete(true);
    }
}

bool UpdateController::hasVisibleDialog() const
{
    return m_dialog
        && m_dialog->isVisible();
}

void UpdateController::showManualUpdateDialog()
{
    if (!m_service)
    {
        return;
    }

    runStartupMaintenance();

    UpdateDialog* dialog =
        ensureDialog(false);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    dialog->refreshForOpen();
}

void UpdateController::showAutomaticUpdateDialog()
{
    if (!m_service)
    {
        return;
    }

    UpdateDialog* dialog =
        ensureDialog(true);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    dialog->refreshForOpen();
}

UpdateDialog* UpdateController::ensureDialog(
    bool automaticPrompt
    )
{
    if (!m_service)
    {
        return nullptr;
    }

    const auto skipped =
        m_skippedUpdateVersionPreferencesPort->read().skippedVersion;
    const QString skippedText = skipped.has_value()
        ? QString::fromStdString(skipped->value())
        : QString();

    if (m_dialog)
    {
        m_dialog->setSkippedVersion(
            skippedText
            );
        if (automaticPrompt)
        {
            m_dialog->setProperty(
                "automaticUpdatePrompt",
                true
                );
        }

        return m_dialog;
    }

    auto* manualUpdateCheckPort = new
        ClassMngr::Next::Platform::UpdateServiceManualCheckPort(m_service);
    auto* dialog =
        new UpdateDialog(
            *manualUpdateCheckPort,
            m_startupComplete,
            m_startupComplete ? m_window.data() : nullptr,
            skippedText
            );
    manualUpdateCheckPort->setParent(dialog);

    dialog->setAttribute(
        Qt::WA_DeleteOnClose
        );
    dialog->setProperty(
        "automaticUpdatePrompt",
        automaticPrompt
        );

    m_dialog =
        dialog;

    dialog->setSkipVersionResultHandlingEnabled(true);

    connect(
        dialog,
        &UpdateDialog::skipVersionRequested,
        this,
        [this, dialog](const QString& version)
        {
            const auto saved = skipVersion(version);
            if (!saved)
            {
                dialog->showSkippedVersionSaveFailure(
                    QString::fromStdString(saved.error().message)
                    );
                return;
            }

            dialog->completeSkipVersion(version);
        }
        );
    connect(
        dialog,
        &UpdateDialog::unskipVersionRequested,
        this,
        [this, dialog]()
        {
            const auto cleared = unskipVersion();
            if (!cleared)
            {
                dialog->showSkippedVersionClearFailure(
                    QString::fromStdString(cleared.error().message)
                    );
                return;
            }

            dialog->completeUnskipVersion();
        }
        );

    connect(
        dialog,
        &QDialog::finished,
        this,
        [this, dialog](int)
        {
            if (
                dialog->property(
                    "automaticUpdatePrompt"
                    ).toBool()
                )
            {
                m_automaticPromptSuppressed =
                    true;
            }
        }
        );

    connect(
        dialog,
        &QObject::destroyed,
        this,
        [this]()
        {
            m_dialog =
                nullptr;
        }
        );

    return dialog;
}
