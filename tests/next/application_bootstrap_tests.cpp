#include "next/application/shutdown_coordinator.h"
#include "next/application/startup_coordinator.h"
#include "next/application/file_dialog_port.h"
#include "next/application/language_preferences_port.h"
#include "next/application/theme_preferences_port.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_descriptor.h"
#include "next/resources/resource_error.h"
#include "next/ui/application_shell.h"
#include "next/ui/initial_page.h"
#include "next/ui/page_host.h"
#include "next/ui/placeholder_page.h"
#include "core/settingsmanager.h"
#include "features/my_info/data/signature_image_processor.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "next/platform/workspace_store_personal_details_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QApplication>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QComboBox>
#include <QEvent>
#include <QFontDatabase>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QImage>
#include <QPointer>
#include <QPixmap>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QLocale>
#include <QStyleHints>
#include <QStyle>
#include <QPushButton>
#include <QRadioButton>
#include <QSysInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSaveFile>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWidget>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

class ApplicationBootstrapTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void createsVisibleShellAndReportsOnlyCompletedStartupStage();
    void startupUpdateCheckWaitsForVisibleInitialPageAndLivesUntilShutdown();
    void startupUpdateCheckDoesNotConstructDispatcherAfterFailedStartup();
    void startupUpdateCheckDoesNotConstructDispatcherAfterShutdownBeforeQueuedTurn();
    void loadsOnlySelectedLanguageThemeAndRequiredFontsBeforeShell();
    void loadedPreferencesReachPagesAndFontStartupRestoresOnTeardown();
    void shellAppearanceUsesSelectedThemeAndLanguage_data();
    void shellAppearanceUsesSelectedThemeAndLanguage();
    void initialSetupPersonalDetailsAppearance_data();
    void initialSetupPersonalDetailsAppearance();
    void systemDefaultUsesFirstSupportedLocaleFromOrderedList();
    void systemDefaultFallsBackToSourceEnglishForUnsupportedLocales();
    void systemDefaultThemeFollowsCurrentStyleColorScheme();
    void explicitEnglishSelectsCanadianTranslation();
    void explicitEnglishFallsBackToUsWhenNoEnglishRegionMatches();
    void translationFailureStopsBeforeThemeAndShell();
    void themeFailureRollsBackInstalledTranslation();
    void requiredFontFailureRollsBackTranslationThemeAndFirstFont();
    void diagnosticsSinkConstructionFailureStopsBeforeShellCreation();
    void generatedCatalogStartsWithOnlySelectedStartupPayloads();
    void packagePathResolutionFailureDoesNotUseCurrentDirectoryFallback();
    void missingResourcePackageFailsCoreServicesBeforeShellCreation();
    void invalidResourceCatalogFailsCoreServicesBeforeShellCreation();
    void documentCatalogMetadataFailureStopsBeforeShellCreation();
    void startupCoordinatorAcceptsOnlyOrderedInitialPageFailureBeforeShell();
    void startupCoordinatorAcceptsEveryPhaseFiveStageInOrder();
    void startupCoordinatorRejectsInvalidTransitionsWithoutRecordingThem();
    void startupCoordinatorRetainsTypedWarningAndTerminalFailure();
    void closeRequestsShutdownOnceAndRestoresApplicationPolicy();
    void shutdownRetriesSettingsAndWorkspaceFailuresBeforeReleasingOwners();
    void startupFailureAndShutdownActionAreReportedPrecisely();
    void startupWorkspacePolicyPreservesLegacyCandidateOrder();
    void noWorkspaceRemainsAnExplicitNonFatalWarning();
    void staleFirstRecentPathIsPrunedWithoutTryingLaterCandidates();
    void interruptedSetupOffersRecoveryBeforeAutoOpenOrRecentHistory();
    void createActionUsesSelectedPathAndDoesNotRecordPendingProfile();
    void openActionUpdatesWorkspaceNavigationAndHistoryOnlyAfterSuccess();
    void openActionDoesNotBypassPendingOrUnreadableSetupRecovery();
    void saveAsActionUsesNextGatewayAndPreservesActiveSessionIdentity();
    void exportActionUsesNextGatewayWithoutResolvingDirtyPage();
    void saveAndCloseProfileActionsUseNextGateway();
    void dirtyPageBlocksNavigationOpenNewAndWindowClose();
    void teacherImportUsesPendingStoreTransactionAndCancelRecoversBackup();
    void imageSignatureSaveReopensAndResumesFromFixture();
    void failedPersonalDetailsSavePreservesReplacementBackup();
    void recentOpenErrorDoesNotFallThroughToLaterCandidates();
    void missingExplicitWorkspaceDoesNotCreateFileOrParent();
    void successfulStartupRetainsSessionAndReleasesStoreOnShutdown();
    void runtimeRemainsClosedAfterTypedGatewayOpenFailure();
    void shutdownClosesThroughTheGatewayUsedForStartupOpen();

private:
    QTemporaryDir m_settingsRoot;
    QJsonArray m_appearanceCaptures;
};

namespace
{

class SilentStartupDiagnosticsSink final : public Bootstrap::StartupDiagnosticsSink
{
public:
    void record(const Application::StartupEvent&) noexcept override
    {
    }
};

class SilentStartupUpdateDispatcher final : public Bootstrap::StartupUpdateDispatcher
{
public:
    void dispatch() override
    {
    }
};

struct StartupUpdateDispatcherProbe final
{
    int factoryCalls = 0;
    int dispatchCalls = 0;
    int asynchronousCompletions = 0;
    int destroyed = 0;
    bool visibleAtDispatch = false;
    bool initialPageActiveAtDispatch = false;
    bool holdCompletionUntilShutdown = false;
    int cancelCalls = 0;
    int suppressedCompletions = 0;
    std::vector<std::string>* shutdownEvents = nullptr;
    std::function<void()> pendingCompletion;
};

class RecordingStartupUpdateDispatcher final
    : public Bootstrap::StartupUpdateDispatcher
{
public:
    RecordingStartupUpdateDispatcher(
        QApplication& application,
        QPointer<Ui::ApplicationShell>& shell,
        std::shared_ptr<StartupUpdateDispatcherProbe> probe
        )
        : m_application(application)
        , m_shell(shell)
        , m_probe(std::move(probe))
    {
    }

    ~RecordingStartupUpdateDispatcher() override
    {
        ++m_probe->destroyed;
        if (m_probe->shutdownEvents)
        {
            m_probe->shutdownEvents->push_back("update-destroy");
        }
        m_lifetime.reset();
    }

    void dispatch() override
    {
        ++m_probe->dispatchCalls;
        m_probe->visibleAtDispatch = m_shell && m_shell->isVisible();
        const Ui::NavigationModel* const model = m_shell
            ? m_shell->navigationModel()
            : nullptr;
        m_probe->initialPageActiveAtDispatch = model
            && model->currentPageId() == model->initialPageId();

        if (m_probe->holdCompletionUntilShutdown)
        {
            const std::weak_ptr<int> lifetime(m_lifetime);
            const QPointer<Ui::ApplicationShell> shell = m_shell;
            const std::shared_ptr<StartupUpdateDispatcherProbe> probe = m_probe;
            m_probe->pendingCompletion = [lifetime, shell, probe] {
                if (lifetime.expired() || !shell)
                {
                    ++probe->suppressedCompletions;
                    return;
                }
                ++probe->asynchronousCompletions;
            };
            return;
        }

        // Model an asynchronous updater completion without issuing network I/O.
        const std::shared_ptr<StartupUpdateDispatcherProbe> probe = m_probe;
        QTimer::singleShot(0, &m_application, [probe] {
            ++probe->asynchronousCompletions;
        });
    }

    void cancel() noexcept override
    {
        ++m_probe->cancelCalls;
        if (m_probe->shutdownEvents)
        {
            m_probe->shutdownEvents->push_back("update-cancel");
        }
        m_lifetime.reset();
    }

private:
    QApplication& m_application;
    QPointer<Ui::ApplicationShell>& m_shell;
    std::shared_ptr<StartupUpdateDispatcherProbe> m_probe;
    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
};

class InitialPageShowOrderProbe final : public QObject
{
public:
    explicit InitialPageShowOrderProbe(QApplication& application)
        : m_application(application)
    {
        m_application.installEventFilter(this);
    }

    ~InitialPageShowOrderProbe() override
    {
        m_application.removeEventFilter(this);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::Show)
        {
            return false;
        }
        auto* const shell = dynamic_cast<Ui::ApplicationShell*>(watched);
        if (!shell)
        {
            return false;
        }

        shellShowObserved = true;
        const Ui::PageHost& host = shell->pageHost();
        initialPageRegisteredAtShow = host.isPageRegistered(QStringLiteral("initial_setup"));
        activePageAtShow = host.activePageKey()
            == std::optional<QString>(QStringLiteral("initial_setup"));
        initialPageInstanceAtShow = dynamic_cast<const Ui::InitialPage*>(
            host.pageInstance(QStringLiteral("initial_setup"))
            ) != nullptr;
        return false;
    }

    bool shellShowObserved = false;
    bool initialPageRegisteredAtShow = false;
    bool activePageAtShow = false;
    bool initialPageInstanceAtShow = false;
    bool initialPageReadyObservedAfterShow = false;

private:
    QApplication& m_application;
};

struct RecentWorkspaceHistoryProbe final
{
    Application::RecentWorkspaceHistory history;
    std::vector<Application::RecentWorkspaceHistory> saves;
    int loadCount = 0;
    int saveCount = 0;
};

class RecordingRecentWorkspaceHistoryPort final
    : public Application::RecentWorkspaceHistoryPort
{
public:
    explicit RecordingRecentWorkspaceHistoryPort(
        std::shared_ptr<RecentWorkspaceHistoryProbe> probe
        )
        : m_probe(std::move(probe))
    {
    }

    [[nodiscard]] Application::RecentWorkspaceHistory load() const override
    {
        ++m_probe->loadCount;
        return m_probe->history;
    }

    [[nodiscard]] Domain::Result<void> save(
        const Application::RecentWorkspaceHistory& history
        ) const override
    {
        ++m_probe->saveCount;
        m_probe->saves.push_back(history);
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> clear() const override
    {
        return Domain::Result<void>::success();
    }

private:
    std::shared_ptr<RecentWorkspaceHistoryProbe> m_probe;
};

struct InitialSetupFileDialogProbe final
{
    Application::FileDialogPath selectedPath;
    std::optional<Application::FileDialogPath> openPath;
    std::optional<Application::SaveFileDialogRequest> lastSaveRequest;
    std::optional<Application::OpenFileDialogRequest> lastOpenRequest;
    int saveCount = 0;
    int openCount = 0;
};

class RecordingInitialSetupFileDialogPort final
    : public Application::FileDialogPort
{
public:
    explicit RecordingInitialSetupFileDialogPort(
        std::shared_ptr<InitialSetupFileDialogProbe> probe
        )
        : m_probe(std::move(probe))
    {
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::FileDialogPath>>
    openOneFile(const Application::OpenFileDialogRequest& request) override
    {
        ++m_probe->openCount;
        m_probe->lastOpenRequest = request;
        return Domain::Result<
            Application::FileDialogResponse<Application::FileDialogPath>>::success(
                Application::FileDialogResponse<Application::FileDialogPath>{
                    .selection = m_probe->openPath
                }
                );
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<std::vector<Application::FileDialogPath>>>
    openManyFiles(const Application::OpenFileDialogRequest&) override
    {
        return unexpectedCall<
            Application::FileDialogResponse<std::vector<Application::FileDialogPath>>>();
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::SavedFileDialogSelection>>
    saveFile(const Application::SaveFileDialogRequest& request) override
    {
        ++m_probe->saveCount;
        m_probe->lastSaveRequest = request;
        return Domain::Result<
            Application::FileDialogResponse<
                Application::SavedFileDialogSelection>>::success(
                    Application::FileDialogResponse<
                        Application::SavedFileDialogSelection>{
                        .selection = Application::SavedFileDialogSelection{
                            .path = m_probe->selectedPath,
                            .openAfterSaving = false
                        }
                    }
                    );
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::FileDialogPath>>
    selectDirectory(const Application::SelectDirectoryDialogRequest&) override
    {
        return unexpectedCall<
            Application::FileDialogResponse<Application::FileDialogPath>>();
    }

private:
    template <typename Response>
    [[nodiscard]] static Domain::Result<Response> unexpectedCall()
    {
        return Domain::Result<Response>::failure(Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected file dialog call.",
            .recoverable = false
        });
    }

    std::shared_ptr<InitialSetupFileDialogProbe> m_probe;
};

struct BootstrapWorkspaceGatewayProbe final
{
    std::vector<std::string> events;
    std::optional<Domain::OperationError> openError;
    std::optional<Domain::OperationError> closeError;
    std::optional<Domain::OperationError> saveError;
    std::optional<Domain::OperationError> discardError;
    std::optional<Application::WorkspaceSession> openedSession;
    std::optional<Application::WorkspaceSession> closedSession;
    int factoryCalls = 0;
    int constructed = 0;
    int destroyed = 0;
    int openCalls = 0;
    int closeCalls = 0;
    int saveCalls = 0;
    int discardCalls = 0;
    int exportCalls = 0;
    std::optional<Application::WorkspaceSession> exportedSession;
    std::optional<Application::WorkspaceLocation> exportDestination;
    std::vector<std::string>* shutdownEvents = nullptr;
};

class RecordingBootstrapWorkspaceGateway final
    : public Application::WorkspaceGateway
{
public:
    explicit RecordingBootstrapWorkspaceGateway(
        std::shared_ptr<BootstrapWorkspaceGatewayProbe> probe
        )
        : m_probe(std::move(probe))
    {
        ++m_probe->constructed;
        m_probe->events.push_back("constructed");
        if (m_probe->shutdownEvents)
        {
            m_probe->shutdownEvents->push_back("workspace-constructed");
        }
    }

    ~RecordingBootstrapWorkspaceGateway() override
    {
        ++m_probe->destroyed;
        m_probe->events.push_back("destroyed");
        if (m_probe->shutdownEvents)
        {
            m_probe->shutdownEvents->push_back("workspace-destroy");
        }
    }

    Domain::Result<Application::WorkspaceSession> createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override
    {
        return Domain::Result<Application::WorkspaceSession>::success(
            makeSession(request.location)
            );
    }

    Domain::Result<Application::WorkspaceSession> openWorkspace(
        const Application::OpenWorkspaceRequest& request
        ) override
    {
        ++m_probe->openCalls;
        m_probe->events.push_back("open");
        if (m_probe->openError.has_value())
        {
            return Domain::Result<Application::WorkspaceSession>::failure(
                *m_probe->openError
                );
        }

        m_probe->openedSession = makeSession(request.location);
        return Domain::Result<Application::WorkspaceSession>::success(
            *m_probe->openedSession
            );
    }

    Domain::Result<void> closeWorkspace(
        const Application::CloseWorkspaceRequest& request
        ) override
    {
        ++m_probe->closeCalls;
        m_probe->events.push_back("close");
        m_probe->closedSession = request.session;
        if (m_probe->shutdownEvents)
        {
            m_probe->shutdownEvents->push_back("workspace-close");
        }
        if (m_probe->closeError.has_value())
        {
            return Domain::Result<void>::failure(*m_probe->closeError);
        }
        return Domain::Result<void>::success();
    }

    Domain::Result<void> saveWorkspace(
        const Application::SaveWorkspaceRequest& request
        ) override
    {
        static_cast<void>(request);
        ++m_probe->saveCalls;
        if (m_probe->saveError.has_value())
        {
            return Domain::Result<void>::failure(*m_probe->saveError);
        }
        return Domain::Result<void>::success();
    }

    Domain::Result<void> discardWorkspaceChanges(
        const Application::DiscardWorkspaceChangesRequest& request
        ) override
    {
        static_cast<void>(request);
        ++m_probe->discardCalls;
        if (m_probe->discardError.has_value())
        {
            return Domain::Result<void>::failure(*m_probe->discardError);
        }
        return Domain::Result<void>::success();
    }

    Domain::Result<Application::WorkspaceLocation> saveWorkspaceAs(
        const Application::SaveWorkspaceAsRequest& request
        ) override
    {
        return Domain::Result<Application::WorkspaceLocation>::success(
            request.destination
            );
    }

    Domain::Result<Application::WorkspaceLocation> exportWorkspace(
        const Application::ExportWorkspaceRequest& request
        ) override
    {
        ++m_probe->exportCalls;
        m_probe->exportedSession = request.session;
        m_probe->exportDestination = request.destination;
        return Domain::Result<Application::WorkspaceLocation>::success(
            request.destination
            );
    }

private:
    [[nodiscard]] static Application::WorkspaceSession makeSession(
        const Application::WorkspaceLocation& location
        )
    {
        return Application::WorkspaceSession(
            Domain::WorkspaceId::fromString("bootstrap-test-session").value(),
            location
            );
    }

    std::shared_ptr<BootstrapWorkspaceGatewayProbe> m_probe;
};

struct UnsavedChangesPromptProbe final
{
    Application::UnsavedChangesAction action =
        Application::UnsavedChangesAction::Cancel;
    std::optional<Domain::OperationError> error;
    std::vector<Application::UnsavedChangesPromptRequest> requests;
};

class RecordingWorkspaceLifecycleUserPrompt final
    : public Application::UserPromptPort
{
public:
    explicit RecordingWorkspaceLifecycleUserPrompt(
        std::shared_ptr<UnsavedChangesPromptProbe> probe
        )
        : m_probe(std::move(probe))
    {
    }

    Domain::Result<void> showMessage(
        const Application::MessagePromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<void>::success();
    }

    Domain::Result<Application::ConfirmationResponse> confirm(
        const Application::ConfirmationPromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<Application::ConfirmationResponse>::success(
            Application::ConfirmationResponse::Accepted
            );
    }

    Domain::Result<Application::UnsavedChangesAction> confirmUnsavedChanges(
        const Application::UnsavedChangesPromptRequest& request
        ) override
    {
        m_probe->requests.push_back(request);
        if (m_probe->error.has_value())
        {
            return Domain::Result<Application::UnsavedChangesAction>::failure(
                *m_probe->error
                );
        }
        return Domain::Result<Application::UnsavedChangesAction>::success(
            m_probe->action
            );
    }

    Domain::Result<std::optional<Application::PromptActionId>> chooseAction(
        const Application::NamedActionPromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<std::optional<Application::PromptActionId>>::success(
            std::nullopt
            );
    }

private:
    std::shared_ptr<UnsavedChangesPromptProbe> m_probe;
};

Bootstrap::ApplicationBootstrap::StartupDiagnosticsSinkFactory
silentStartupDiagnosticsSinkFactory()
{
    return [] {
        return std::make_unique<SilentStartupDiagnosticsSink>();
    };
}

Application::WorkspaceLocation workspaceLocation(const QString& path)
{
    return Application::WorkspaceLocation(
        QFileInfo(path).absoluteFilePath().toUtf8().toStdString()
        );
}

void verifyWorkspaceStartupOrder(
    const Application::StartupResult& result,
    const std::vector<Application::StartupEvent>& observedEvents,
    const Bootstrap::ApplicationBootstrap& bootstrap,
    const Application::StartupStageOutcome workspaceOutcome
    )
{
    QVERIFY(result.succeeded());
    QCOMPARE(result.status, Application::StartupStatus::StartupReady);
    constexpr std::array expectedStages{
        Application::StartupStage::ProcessCreated,
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };
    QCOMPARE(observedEvents.size(), expectedStages.size());
    QCOMPARE(result.stageResults.size(), expectedStages.size());
    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        QCOMPARE(observedEvents[index].stage, expectedStages[index]);
        QCOMPARE(result.stageResults[index].stage, expectedStages[index]);
    }
    QCOMPARE(observedEvents[6].outcome, workspaceOutcome);
    QCOMPARE(result.stageResults[6].outcome, workspaceOutcome);
    QCOMPARE(result.stageResults.back().stage, Application::StartupStage::StartupReady);
    QVERIFY(bootstrap.shell() != nullptr);
    QVERIFY(bootstrap.shell()->isVisible());
}

void ignoreCoreServicesFailureMessages()
{
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=process-created outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtCriticalMsg,
        "ClassMngrNext startup stage=core-services-created outcome=failed "
        "diagnostic=stage-failed diagnostic_stage=core-services-created"
        );
}

void verifyCoreServicesFailure(
    const Application::StartupResult& result,
    const std::vector<Application::StartupEvent>& observedEvents,
    const Bootstrap::ApplicationBootstrap& bootstrap,
    const QApplication& application,
    bool previousQuitPolicy
    )
{
    QCOMPARE(result.status, Application::StartupStatus::Failed);
    QVERIFY(!result.succeeded());
    QVERIFY(result.failure.has_value());
    QCOMPARE(*result.failure, Application::StartupFailureReason::StageFailed);
    QVERIFY(result.diagnostic.has_value());
    QCOMPARE(result.diagnostic->code, Application::StartupDiagnosticCode::StageFailed);
    QVERIFY(result.diagnostic->stage.has_value());
    QCOMPARE(*result.diagnostic->stage, Application::StartupStage::CoreServicesCreated);

    QCOMPARE(result.stageResults.size(), std::size_t{2});
    QCOMPARE(result.stageResults[0].stage, Application::StartupStage::ProcessCreated);
    QCOMPARE(result.stageResults[0].outcome, Application::StartupStageOutcome::Completed);
    QVERIFY(!result.stageResults[0].diagnostic.has_value());
    QCOMPARE(result.stageResults[1].stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(result.stageResults[1].outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(result.stageResults[1].diagnostic.has_value());
    QCOMPARE(
        result.stageResults[1].diagnostic->code,
        Application::StartupDiagnosticCode::StageFailed
        );
    QCOMPARE(result.completedEvents.size(), std::size_t{1});
    QCOMPARE(result.completedEvents[0].stage, Application::StartupStage::ProcessCreated);
    QCOMPARE(result.completedEvents[0].outcome, Application::StartupStageOutcome::Completed);

    QVERIFY(result.latestStageResult.has_value());
    QCOMPARE(result.latestStageResult->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(result.latestStageResult->outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(result.latestStageResult->diagnostic.has_value());
    QCOMPARE(
        result.latestStageResult->diagnostic->code,
        Application::StartupDiagnosticCode::StageFailed
        );
    QVERIFY(result.latestStageResult->diagnostic->stage.has_value());
    QCOMPARE(
        *result.latestStageResult->diagnostic->stage,
        Application::StartupStage::CoreServicesCreated
        );

    QCOMPARE(observedEvents.size(), std::size_t{2});
    QCOMPARE(observedEvents[0].stage, Application::StartupStage::ProcessCreated);
    QCOMPARE(observedEvents[0].outcome, Application::StartupStageOutcome::Completed);
    QCOMPARE(observedEvents[1].stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(observedEvents[1].outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(observedEvents[1].diagnostic.has_value());
    QCOMPARE(observedEvents[1].diagnostic->code, Application::StartupDiagnosticCode::StageFailed);
    QVERIFY(observedEvents[1].diagnostic->stage.has_value());
    QCOMPARE(*observedEvents[1].diagnostic->stage, Application::StartupStage::CoreServicesCreated);

    QVERIFY(bootstrap.shell() == nullptr);
    QCOMPARE(application.quitOnLastWindowClosed(), previousQuitPolicy);
}

QString configuredResourceCatalogPath()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
}

QString configuredResourceRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
}

QString configuredResourceRelativeRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT);
}

Bootstrap::ApplicationBootstrap::ResourcePackagePathsResolver resolverForEmptyRoot(
    QString resourceRoot
    )
{
    return [resourceRoot = std::move(resourceRoot)]()
        -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
        return Resources::ResourcePackagePaths{
            resourceRoot,
            configuredResourceCatalogPath()
        };
    };
}

Bootstrap::ApplicationBootstrap::ResourcePackagePathsResolver resolverForRoot(
    QString resourceRoot
    )
{
    return [resourceRoot = std::move(resourceRoot)]()
        -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
        return Resources::ResourcePackagePaths{
            resourceRoot,
            configuredResourceCatalogPath()
        };
    };
}

class StartupResourceFixture final
{
public:
    StartupResourceFixture()
        : m_resourceRoot(QDir(m_directory.path()).filePath(QStringLiteral("resources")))
    {
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return m_directory.isValid() && QDir().mkpath(m_resourceRoot);
    }

    [[nodiscard]] const QString& resourceRoot() const noexcept
    {
        return m_resourceRoot;
    }

    [[nodiscard]] bool addResource(const QString& relativePath) const
    {
        const QString sourcePath = QDir(configuredResourceRoot()).filePath(relativePath);
        const QString destinationPath = QDir(m_resourceRoot).filePath(relativePath);
        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath()))
        {
            return false;
        }
        return QFile::copy(sourcePath, destinationPath);
    }

    [[nodiscard]] bool addTranslation(const QString& locale) const
    {
        const QString relativePath =
            QStringLiteral("assets/translations/ClassMngr_%1.qm").arg(locale);
        const QString sourcePath = QDir(
            QFileInfo(configuredResourceCatalogPath()).absolutePath()
            ).filePath(relativePath);
        const QString destinationPath = QDir(m_resourceRoot).filePath(relativePath);
        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath()))
        {
            return false;
        }
        return QFile::copy(sourcePath, destinationPath);
    }

    [[nodiscard]] bool addTheme(const QString& theme) const
    {
        return addResource(QStringLiteral("assets/styles/%1.qss").arg(theme));
    }

    [[nodiscard]] bool addRequiredFonts() const
    {
        return addResource(QStringLiteral("assets/fonts/Inter.ttc"))
            && addResource(QStringLiteral("assets/fonts/PretendardVariable.ttf"));
    }

    [[nodiscard]] bool addDocumentCatalogMetadata() const
    {
        return addResource(QStringLiteral("assets/documents/documents.json"));
    }

    [[nodiscard]] bool removeDocumentCatalogMetadata() const
    {
        return QFile::remove(QDir(m_resourceRoot).filePath(
            QStringLiteral("assets/documents/documents.json")
            ));
    }

    [[nodiscard]] bool addStartupResources(
        const QString& locale,
        const QString& theme
        ) const
    {
        return addTranslation(locale)
            && addTheme(theme)
            && addRequiredFonts()
            && addDocumentCatalogMetadata();
    }

private:
    QTemporaryDir m_directory;
    QString m_resourceRoot;
};

class StartupPreferencesGuard final
{
public:
    StartupPreferencesGuard(
        const int language,
        const int theme,
        const std::optional<int> fontOffset = std::nullopt
        )
        : m_languageKey(QString::fromUtf8(OptionKeys::Language))
        , m_themeKey(QString::fromUtf8(OptionKeys::Theme))
        , m_fontKey(QString::fromUtf8(OptionKeys::FontSize))
        , m_previousLanguage(SettingsManager::instance().get(m_languageKey))
        , m_previousTheme(SettingsManager::instance().get(m_themeKey))
        , m_previousFont(SettingsManager::instance().get(m_fontKey))
        , m_fontOffset(fontOffset)
    {
        SettingsManager::instance().set(m_languageKey, language);
        SettingsManager::instance().set(m_themeKey, theme);
        if (m_fontOffset)
        {
            SettingsManager::instance().set(m_fontKey, *m_fontOffset);
        }
    }

    ~StartupPreferencesGuard()
    {
        restore(m_languageKey, m_previousLanguage);
        restore(m_themeKey, m_previousTheme);
        if (m_fontOffset)
        {
            restore(m_fontKey, m_previousFont);
        }
    }

private:
    QString m_languageKey;
    QString m_themeKey;
    QString m_fontKey;
    QVariant m_previousLanguage;
    QVariant m_previousTheme;
    QVariant m_previousFont;
    std::optional<int> m_fontOffset;

    static void restore(const QString& key, const QVariant& value)
    {
        if (value.isValid())
        {
            SettingsManager::instance().set(key, value);
        }
        else
        {
            SettingsManager::instance().remove(key);
        }
    }
};

class LanguageChangeProbe final : public QWidget
{
public:
    [[nodiscard]] int languageChangeCount() const noexcept
    {
        return m_languageChangeCount;
    }

    void resetLanguageChangeCount() noexcept
    {
        m_languageChangeCount = 0;
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::LanguageChange)
        {
            ++m_languageChangeCount;
        }
        return QWidget::event(event);
    }

private:
    int m_languageChangeCount = 0;
};

QString expectedEnglishLocale()
{
    for (const QString& language : QLocale::system().uiLanguages())
    {
        const QLocale locale(language);
        if (locale.language() != QLocale::English)
        {
            continue;
        }
        switch (locale.territory())
        {
        case QLocale::UnitedStates: return QStringLiteral("en_US");
        case QLocale::Canada: return QStringLiteral("en_CA");
        case QLocale::UnitedKingdom: return QStringLiteral("en_GB");
        case QLocale::Australia: return QStringLiteral("en_AU");
        default: break;
        }
    }
    return QStringLiteral("en_US");
}

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool containsStage(
    const std::vector<Application::StartupEvent>& events,
    const Application::StartupStage stage
    )
{
    return std::any_of(
        events.begin(),
        events.end(),
        [stage](const Application::StartupEvent& event) {
            return event.stage == stage;
        }
        );
}

void verifyTypedStageFailure(
    const Application::StartupResult& result,
    const Application::StartupStage expectedStage
    )
{
    QCOMPARE(result.status, Application::StartupStatus::Failed);
    QVERIFY(result.failure.has_value());
    QCOMPARE(*result.failure, Application::StartupFailureReason::StageFailed);
    QVERIFY(result.diagnostic.has_value());
    QCOMPARE(result.diagnostic->code, Application::StartupDiagnosticCode::StageFailed);
    QVERIFY(result.diagnostic->stage.has_value());
    QCOMPARE(*result.diagnostic->stage, expectedStage);
    QVERIFY(result.latestStageResult.has_value());
    QCOMPARE(result.latestStageResult->stage, expectedStage);
    QCOMPARE(result.latestStageResult->outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(result.latestStageResult->diagnostic.has_value());
    QCOMPARE(
        result.latestStageResult->diagnostic->code,
        Application::StartupDiagnosticCode::StageFailed
        );
    QVERIFY(result.latestStageResult->diagnostic->stage.has_value());
    QCOMPARE(*result.latestStageResult->diagnostic->stage, expectedStage);
}

bool containsFontFamilyLike(const QStringList& families, const QString& name)
{
    return std::any_of(
        families.begin(),
        families.end(),
        [&name](const QString& family) {
            return family.contains(name, Qt::CaseInsensitive);
        }
        );
}

bool executeWorkspaceSql(const QString& path, const QString& statement)
{
    const QString connectionName = QStringLiteral("setup-test-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            executed = query.exec(statement);
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}

QString workspaceFixturePath(const QString& fixtureName)
{
    return QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/workspaces/")
        + fixtureName;
}

bool executeWorkspaceFixture(
    const QString& profilePath,
    const QString& fixtureName,
    QString* errorMessage
    )
{
    QFile fixture(workspaceFixturePath(fixtureName));
    if (!fixture.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = fixture.errorString();
        }
        return false;
    }
    const QStringList statements = QString::fromUtf8(fixture.readAll())
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);

    const QString connectionName = QStringLiteral("setup-fixture-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(profilePath);
        if (!database.open())
        {
            if (errorMessage)
            {
                *errorMessage = database.lastError().text();
            }
        }
        else
        {
            executed = true;
            QSqlQuery query(database);
            for (const QString& rawStatement : statements)
            {
                const QString statement = rawStatement.trimmed();
                if (statement.isEmpty())
                {
                    continue;
                }
                if (!query.exec(statement))
                {
                    if (errorMessage)
                    {
                        *errorMessage = QStringLiteral("%1: %2")
                            .arg(statement.simplified().left(160),
                                 query.lastError().text());
                    }
                    executed = false;
                    break;
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}


bool createWorkspaceWithFixture(
    const QString& profilePath,
    const QString& fixtureName,
    QString* errorMessage
    )
{
    {
        WorkspaceStore store;
        const RepositoryStatus opened = store.open(profilePath);
        if (!opened)
        {
            if (errorMessage)
            {
                *errorMessage = opened.error().diagnostic;
            }
            return false;
        }
        store.close();
    }
    return executeWorkspaceFixture(profilePath, fixtureName, errorMessage);
}

struct RepresentativeProfileRecords final
{
    QStringList teachers;
    QStringList classes;

    friend bool operator==(
        const RepresentativeProfileRecords&,
        const RepresentativeProfileRecords&)
        = default;
};

std::optional<RepresentativeProfileRecords> readRepresentativeProfileRecords(
    const QString& profilePath,
    QString* errorMessage
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(profilePath);
    if (!opened)
    {
        if (errorMessage)
        {
            *errorMessage = opened.error().diagnostic;
        }
        return std::nullopt;
    }

    RepresentativeProfileRecords records;
    {
        QSqlQuery query(store.database());
        if (!query.exec(QStringLiteral(
                "SELECT teacher_en FROM teachers ORDER BY id"))
            )
        {
            if (errorMessage)
            {
                *errorMessage = query.lastError().text();
            }
            return std::nullopt;
        }
        while (query.next())
        {
            records.teachers.push_back(query.value(0).toString());
        }

        if (!query.exec(QStringLiteral(
                "SELECT name FROM classes ORDER BY id"))
            )
        {
            if (errorMessage)
            {
                *errorMessage = query.lastError().text();
            }
            return std::nullopt;
        }
        while (query.next())
        {
            records.classes.push_back(query.value(0).toString());
        }
    }
    store.close();
    return records;
}

std::optional<QByteArray> fileSha256(
    const QString& path,
    QString* errorMessage
    )
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return std::nullopt;
    }
    return QCryptographicHash::hash(
        file.readAll(),
        QCryptographicHash::Sha256
        );
}

struct TeacherImportPersistedRecords final
{
    QStringList koreanTeachers;
    QStringList nativeEnglishTeachers;
    QStringList gsTeamMembers;
    std::optional<QString> latestSourceDate;
};

std::optional<TeacherImportPersistedRecords> readTeacherImportPersistedRecords(
    const QString& profilePath,
    QString* errorMessage
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(profilePath);
    if (!opened)
    {
        if (errorMessage)
        {
            *errorMessage = opened.error().diagnostic;
        }
        return std::nullopt;
    }

    TeacherImportPersistedRecords result;
    const auto readNames = [&store, errorMessage](
                               const QString& statement,
                               QStringList& destination) {
        QSqlQuery query(store.database());
        if (!query.exec(statement))
        {
            if (errorMessage)
            {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
        while (query.next())
        {
            destination.append(query.value(0).toString());
        }
        return true;
    };
    if (!readNames(
            QStringLiteral("SELECT teacher_kr FROM teachers ORDER BY id"),
            result.koreanTeachers)
        || !readNames(
            QStringLiteral("SELECT name FROM native_english_teachers ORDER BY id"),
            result.nativeEnglishTeachers)
        || !readNames(
            QStringLiteral("SELECT name FROM gs_team ORDER BY id"),
            result.gsTeamMembers))
    {
        store.close();
        return std::nullopt;
    }

    {
        QSqlQuery query(store.database());
        query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key=?"));
        query.addBindValue(QStringLiteral("teacher_import/latest_source_date"));
        if (!query.exec())
        {
            if (errorMessage)
            {
                *errorMessage = query.lastError().text();
            }
            store.close();
            return std::nullopt;
        }
        if (query.next())
        {
            result.latestSourceDate = query.value(0).toString();
        }
    }
    store.close();
    return result;
}

}

void ApplicationBootstrapTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QVERIFY(qputenv("CLASSMNGR_SETTINGS_ROOT", m_settingsRoot.path().toUtf8()));
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void ApplicationBootstrapTests::cleanupTestCase()
{
    const QString outputDirectory = qEnvironmentVariable(
        "CLASSMNGR_NEXT_APPEARANCE_CAPTURE_DIR");
    if (!outputDirectory.isEmpty())
    {
        QCOMPARE(m_appearanceCaptures.size(), 4);
        const QString sourceRevision = qEnvironmentVariable(
            "CLASSMNGR_NEXT_APPEARANCE_REVISION");
        QVERIFY(!sourceRevision.isEmpty());
        const QJsonObject manifest{
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("kind"), QStringLiteral("human-review-baselines")},
            {QStringLiteral("sourceRevision"), sourceRevision},
            {QStringLiteral("sourceState"), qEnvironmentVariable(
                "CLASSMNGR_NEXT_APPEARANCE_SOURCE_STATE", "unspecified")},
            {QStringLiteral("scope"), QStringLiteral("Next placeholder shell client area")},
            {QStringLiteral("captures"), m_appearanceCaptures}
        };
        QFile file(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray contents = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
        QCOMPARE(file.write(contents), contents.size());
    }
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void ApplicationBootstrapTests::
createsVisibleShellAndReportsOnlyCompletedStartupStage()
{
    QApplication& application = *qApp;
    std::vector<Application::StartupEvent> observedEvents;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    const QString locale = expectedEnglishLocale();
    QVERIFY2(resources.addStartupResources(locale, QStringLiteral("light")),
             qPrintable(QStringLiteral("could not stage selected startup resources for %1").arg(locale)));
    InitialPageShowOrderProbe showOrder(application);

    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=process-created outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=core-services-created outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=shell-created outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=initial-page-ready outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=startup-ready outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents, &showOrder](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
            if (event.stage == Application::StartupStage::InitialPageReady)
            {
                showOrder.initialPageReadyObservedAfterShow = showOrder.shellShowObserved;
            }
        },
        {},
        resolverForRoot(resources.resourceRoot())
        );

    const Application::StartupResult result = bootstrap.start();

    QVERIFY(showOrder.shellShowObserved);
    QVERIFY(showOrder.initialPageRegisteredAtShow);
    QVERIFY(showOrder.activePageAtShow);
    QVERIFY(showOrder.initialPageInstanceAtShow);
    QVERIFY(showOrder.initialPageReadyObservedAfterShow);
    QCOMPARE(result.status, Application::StartupStatus::StartupReady);
    QVERIFY(result.succeeded());
    QVERIFY(!result.failure.has_value());
    QCOMPARE(result.stageResults.size(), std::size_t{10});
    QCOMPARE(result.completedEvents.size(), std::size_t{9});
    QVERIFY(!result.diagnostic.has_value());

    constexpr std::array expectedStages{
        Application::StartupStage::ProcessCreated,
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };
    QCOMPARE(observedEvents.size(), expectedStages.size());
    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        QCOMPARE(result.stageResults[index].stage, expectedStages[index]);
        QCOMPARE(observedEvents[index].stage, expectedStages[index]);
        if (expectedStages[index] == Application::StartupStage::WorkspaceOpened)
        {
            QCOMPARE(result.stageResults[index].outcome, Application::StartupStageOutcome::Warning);
            QVERIFY(result.stageResults[index].diagnostic.has_value());
            QCOMPARE(result.stageResults[index].diagnostic->code,
                     Application::StartupDiagnosticCode::StageWarning);
            QCOMPARE(observedEvents[index].outcome, Application::StartupStageOutcome::Warning);
            QVERIFY(observedEvents[index].diagnostic.has_value());
            QVERIFY(observedEvents[index].diagnostic->stage.has_value());
            QCOMPARE(*observedEvents[index].diagnostic->stage, expectedStages[index]);
            continue;
        }

        QCOMPARE(result.stageResults[index].outcome, Application::StartupStageOutcome::Completed);
        QVERIFY(!result.stageResults[index].diagnostic.has_value());
        QCOMPARE(observedEvents[index].outcome, Application::StartupStageOutcome::Completed);
        QVERIFY(!observedEvents[index].diagnostic.has_value());
    }
    for (std::size_t index = 0; index < result.completedEvents.size(); ++index)
    {
        const std::size_t stageIndex = index < 6 ? index : index + 1;
        QCOMPARE(result.completedEvents[index].stage, expectedStages[stageIndex]);
    }
    QVERIFY(!result.stageResults.empty());
    QCOMPARE(result.stageResults.back().stage, Application::StartupStage::StartupReady);
    QCOMPARE(result.stageResults.back().outcome, Application::StartupStageOutcome::Completed);

    const Application::StartupResult repeatedStart = bootstrap.start();
    QCOMPARE(repeatedStart.status, Application::StartupStatus::Failed);
    QVERIFY(repeatedStart.failure.has_value());
    QCOMPARE(*repeatedStart.failure, Application::StartupFailureReason::AlreadyStarted);
    QCOMPARE(observedEvents.size(), expectedStages.size());

    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell != nullptr);
    QVERIFY(shell->isVisible());
    for (QObject* const object : shell->findChildren<QObject*>())
    {
        const QByteArray className(object->metaObject()->className());
        QVERIFY2(!className.startsWith("QPdf"), className.constData());
    }
    QVERIFY(!shell->isFullScreen());
    QVERIFY(!(shell->windowState() & Qt::WindowFullScreen));
    QCOMPARE(shell->windowTitle(), QStringLiteral("ClassMngr Next"));

    Ui::PageHost& pageHost = shell->pageHost();
    QVERIFY(shell->centralWidget() != static_cast<QWidget*>(&pageHost));
    QVERIFY(pageHost.isPageRegistered(QStringLiteral("initial_setup")));
    QVERIFY(pageHost.hasPageInstance(QStringLiteral("initial_setup")));
    QCOMPARE(pageHost.activePageKey(),
             std::optional<QString>(QStringLiteral("initial_setup")));
    QVERIFY(shell->navigationModel() != nullptr);
    QCOMPARE(shell->navigationModel()->initialPageId()->value(),
             QStringLiteral("initial_setup"));
    QCOMPARE(shell->navigationModel()->selectedPageId()->value(),
             QStringLiteral("initial_setup"));
    const std::vector<Ui::NavigationNode>& navigationNodes =
        shell->navigationModel()->nodes();
    QCOMPARE(navigationNodes.size(), std::size_t(8));
    QCOMPARE(navigationNodes[0].pageId->value(), QStringLiteral("my_workspace"));
    QCOMPARE(navigationNodes[1].pageId->value(), QStringLiteral("classes"));
    QCOMPARE(navigationNodes[2].pageId->value(), QStringLiteral("sub_prep"));
    QCOMPARE(navigationNodes[3].label, QStringLiteral("Co-Teachers"));
    QCOMPARE(navigationNodes[4].label, QStringLiteral("Campus Staff"));
    QCOMPARE(navigationNodes[5].label, QStringLiteral("Useful Links"));
    QCOMPARE(navigationNodes[6].label, QStringLiteral("Campus Directory"));
    QCOMPARE(navigationNodes[7].pageId->value(), QStringLiteral("initial_setup"));
    QCOMPARE(navigationNodes[4].children.size(), std::size_t(3));
    QCOMPARE(navigationNodes[4].children[0].label, QStringLiteral("Korean Teachers"));
    QCOMPARE(navigationNodes[4].children[1].pageId->value(),
             QStringLiteral("native_english_teachers"));
    QCOMPARE(navigationNodes[4].children[2].pageId->value(), QStringLiteral("gs_team"));
    QCOMPARE(navigationNodes[6].children.size(), std::size_t(5));
    QCOMPARE(navigationNodes[6].children[0].pageId->value(),
             QStringLiteral("campus_information"));
    QCOMPARE(navigationNodes[6].children[1].pageId->value(),
             QStringLiteral("campus_directions"));
    QCOMPARE(navigationNodes[6].children[4].pageId->value(), QStringLiteral("campus_map"));
    QCOMPARE(navigationNodes[5].children.size(), std::size_t(7));
    const QStringList usefulLinkLabels{
        QStringLiteral("Dropbox"),
        QStringLiteral("Vacation Calendar"),
        QStringLiteral("Yearly Calendar"),
        QStringLiteral("Training Website"),
        QStringLiteral("NET Website"),
        QStringLiteral("LMS Website"),
        QStringLiteral("Highlights Library")
    };
    for (std::size_t index = 0; index < usefulLinkLabels.size(); ++index)
    {
        const Ui::NavigationNode& link = navigationNodes[5].children[index];
        QCOMPARE(link.kind, Ui::NavigationNode::Kind::Placeholder);
        QCOMPARE(link.label, usefulLinkLabels[static_cast<qsizetype>(index)]);
        QVERIFY(!link.pageId.has_value());
        QVERIFY(link.children.empty());
    }

    const auto initialPageId = Ui::PageId::fromString(QStringLiteral("initial_setup"));
    QVERIFY(initialPageId.has_value());
    const auto initialDescriptor = pageHost.pageDescriptor(*initialPageId);
    QVERIFY(initialDescriptor.has_value());
    QCOMPARE(initialDescriptor->id.value(), QStringLiteral("initial_setup"));
    QCOMPARE(initialDescriptor->label, QStringLiteral("Initial Setup"));
    QVERIFY(!initialDescriptor->iconResourceId.has_value());
    QVERIFY(initialDescriptor->requiredPermissions.isEmpty());
    QVERIFY(initialDescriptor->resourcePolicy == Ui::PageResourcePolicy::PageScoped);
    QVERIFY(initialDescriptor->initialDataPolicy == Ui::PageInitialDataPolicy::None);
    for (const Ui::PageDescriptor& descriptor : pageHost.pageDescriptors())
    {
        QVERIFY(!descriptor.iconResourceId.has_value());
    }

    auto* const page = dynamic_cast<Ui::InitialPage*>(
        pageHost.pageInstance(QStringLiteral("initial_setup"))
        );
    QVERIFY(page != nullptr);
    QVERIFY(page->isVisible());
    QCOMPARE(page->objectName(), QStringLiteral("classMngrNextInitialSetupPage"));
    QCOMPARE(page->resourceContext().pageKey(), QStringLiteral("initial_setup"));
    QCOMPARE(page->resourceContext().pageScope().kind(), Resources::ResourceScopeKind::Page);
    QCOMPARE(page->resourceContext().pageScope().ownerLabel(),
             QStringLiteral("initial_setup"));

    QLabel* const title = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTitle")
        );
    QVERIFY(title != nullptr);
    QCOMPARE(title->text(), QStringLiteral("Initial Setup"));

    QLabel* const description = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupDescription")
        );
    QVERIFY(description != nullptr);
    QCOMPARE(
        description->text(),
        QStringLiteral("Create a ClassMngr profile to begin setup. Your profile is saved as a .tps file.")
        );

    QTreeWidget* const navigationTree = shell->findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(navigationTree != nullptr);
    QTreeWidgetItemIterator navigationItem(navigationTree);
    std::size_t usefulPlaceholderCount = 0;
    while (*navigationItem)
    {
        const Ui::NavigationNode::Kind kind = static_cast<Ui::NavigationNode::Kind>(
            (*navigationItem)->data(0, Qt::UserRole).toInt()
            );
        if (kind == Ui::NavigationNode::Kind::Page)
        {
            QVERIFY((*navigationItem)->icon(0).isNull());
        }
        else if (kind == Ui::NavigationNode::Kind::Placeholder)
        {
            ++usefulPlaceholderCount;
            QVERIFY(usefulLinkLabels.contains((*navigationItem)->text(0)));
            QVERIFY((*navigationItem)->data(0, Qt::UserRole + 1).toString().isEmpty());
            QVERIFY((*navigationItem)->icon(0).isNull());
        }
        ++navigationItem;
    }
    QCOMPARE(usefulPlaceholderCount, std::size_t(7));

    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
startupUpdateCheckWaitsForVisibleInitialPageAndLivesUntilShutdown()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    const auto probe = std::make_shared<StartupUpdateDispatcherProbe>();
    QPointer<Ui::ApplicationShell> shell;
    bool factoryUnusedAtStartupReady = false;
    bool dispatcherCreatedBeforeStartupReady = false;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&probe, &factoryUnusedAtStartupReady,
         &dispatcherCreatedBeforeStartupReady](const Application::StartupEvent& event) {
            if (event.stage == Application::StartupStage::StartupReady)
            {
                factoryUnusedAtStartupReady = probe->factoryCalls == 0;
            }
            else if (probe->factoryCalls != 0)
            {
                dispatcherCreatedBeforeStartupReady = true;
            }
        },
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        {},
        [&application, &shell, probe] {
            ++probe->factoryCalls;
            return std::make_unique<RecordingStartupUpdateDispatcher>(
                application, shell, probe
                );
        }
        );

    const Application::StartupResult result = bootstrap.start();
    QCOMPARE(result.status, Application::StartupStatus::StartupReady);
    QVERIFY(result.succeeded());
    QVERIFY(factoryUnusedAtStartupReady);
    QVERIFY(!dispatcherCreatedBeforeStartupReady);
    QCOMPARE(probe->factoryCalls, 0);
    QCOMPARE(probe->dispatchCalls, 0);
    QCOMPARE(probe->destroyed, 0);

    shell = bootstrap.shell();
    QVERIFY(shell != nullptr);
    QVERIFY(shell->isVisible());
    QCOMPARE(
        shell->pageHost().activePageKey(),
        std::optional<QString>(QStringLiteral("initial_setup"))
        );

    // StartupReady has queued the callback, but it must not run inside start().
    // The first event-loop turn sees the visible shell and selected default page.
    QCOMPARE(probe->factoryCalls, 0);
    QCOMPARE(probe->dispatchCalls, 0);
    QCoreApplication::processEvents();
    QCOMPARE(probe->dispatchCalls, 1);
    QCOMPARE(probe->factoryCalls, 1);
    QVERIFY(probe->visibleAtDispatch);
    QVERIFY(probe->initialPageActiveAtDispatch);
    QTRY_COMPARE(probe->asynchronousCompletions, 1);
    QCOMPARE(probe->destroyed, 0);

    // Later visibility and activation changes cannot dispatch a second time.
    shell->hide();
    shell->show();
    shell->activateWindow();
    QCoreApplication::processEvents();
    QCOMPARE(probe->factoryCalls, 1);
    QCOMPARE(probe->dispatchCalls, 1);
    QCOMPARE(probe->destroyed, 0);

    static_cast<void>(bootstrap.shutdown());
    QCOMPARE(probe->cancelCalls, 1);
    QCOMPARE(probe->destroyed, 1);
    QVERIFY(!shell);
    QCoreApplication::processEvents();
    QCOMPARE(probe->factoryCalls, 1);
    QCOMPARE(probe->dispatchCalls, 1);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
startupUpdateCheckDoesNotConstructDispatcherAfterFailedStartup()
{
    QApplication& application = *qApp;
    const auto probe = std::make_shared<StartupUpdateDispatcherProbe>();
    ignoreCoreServicesFailureMessages();
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        []() -> std::unique_ptr<Bootstrap::StartupDiagnosticsSink> {
            throw std::runtime_error("injected diagnostics sink construction failure");
        },
        {},
        std::nullopt,
        {},
        [probe] {
            ++probe->factoryCalls;
            return std::unique_ptr<Bootstrap::StartupUpdateDispatcher>{};
        }
        );

    const Application::StartupResult result = bootstrap.start();
    QCOMPARE(result.status, Application::StartupStatus::Failed);
    QVERIFY(!containsStage(result.completedEvents, Application::StartupStage::StartupReady));
    QCOMPARE(probe->factoryCalls, 0);
    QCOMPARE(probe->dispatchCalls, 0);
    QCoreApplication::processEvents();
    QCOMPARE(probe->factoryCalls, 0);
    QCOMPARE(probe->dispatchCalls, 0);
}

void ApplicationBootstrapTests::
startupUpdateCheckDoesNotConstructDispatcherAfterShutdownBeforeQueuedTurn()
{
    QApplication& application = *qApp;
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));
    const auto probe = std::make_shared<StartupUpdateDispatcherProbe>();
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        {},
        [probe] {
            ++probe->factoryCalls;
            return std::unique_ptr<Bootstrap::StartupUpdateDispatcher>{};
        }
        );

    const Application::StartupResult result = bootstrap.start();
    QCOMPARE(result.status, Application::StartupStatus::StartupReady);
    QCOMPARE(probe->factoryCalls, 0);
    QCOMPARE(probe->dispatchCalls, 0);
    static_cast<void>(bootstrap.shutdown());
    QCoreApplication::processEvents();
    QCOMPARE(probe->factoryCalls, 0);
    QCOMPARE(probe->dispatchCalls, 0);
}

void ApplicationBootstrapTests::
loadsOnlySelectedLanguageThemeAndRequiredFontsBeforeShell()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Dark)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("dark")));

    LanguageChangeProbe languageProbe;
    languageProbe.show();
    languageProbe.resetLanguageChangeCount();

    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot())
        );

    const Application::StartupResult result = bootstrap.start();
    QCoreApplication::processEvents();

    QVERIFY(result.succeeded());
    QCOMPARE(result.status, Application::StartupStatus::StartupReady);
    constexpr std::array expectedStages{
        Application::StartupStage::ProcessCreated,
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };
    QCOMPARE(observedEvents.size(), expectedStages.size());
    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        QCOMPARE(observedEvents[index].stage, expectedStages[index]);
        QCOMPARE(
            observedEvents[index].outcome,
            index == 6
                ? Application::StartupStageOutcome::Warning
                : Application::StartupStageOutcome::Completed
            );
    }
    QCOMPARE(result.stageResults.size(), expectedStages.size());
    QCOMPARE(result.completedEvents.size(), expectedStages.size() - 1);
    QCOMPARE(languageProbe.languageChangeCount(), 1);
    QCOMPARE(
        QCoreApplication::translate("AboutDialog", "Inter Font License"),
        QStringLiteral("Inter 폰트 라이선스")
        );
    QCOMPARE(
        application.styleSheet(),
        readTextFile(QDir(resources.resourceRoot()).filePath(QStringLiteral("assets/styles/dark.qss")))
        );
    QVERIFY(bootstrap.shell() != nullptr);
    QVERIFY(bootstrap.shell()->isVisible());
    const QStringList registeredFamilies = QFontDatabase::families();
    QVERIFY(containsFontFamilyLike(registeredFamilies, QStringLiteral("Inter")));
    QVERIFY(containsFontFamilyLike(registeredFamilies, QStringLiteral("Pretendard")));

    // Only the selected translation, selected theme, two required fonts, and
    // document catalog metadata exist in this package root. Startup succeeds
    // without staging any document bodies.
    languageProbe.close();
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
systemDefaultUsesFirstSupportedLocaleFromOrderedList()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::SystemDefault),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addTranslation(QStringLiteral("en_CA")));
    QVERIFY(resources.addTheme(QStringLiteral("light")));
    QVERIFY(resources.addRequiredFonts());
    QVERIFY(resources.addDocumentCatalogMetadata());

    LanguageChangeProbe languageProbe;
    languageProbe.show();
    languageProbe.resetLanguageChangeCount();

    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot()),
        std::optional<QStringList>{QStringList{
            QStringLiteral("fr-FR"),
            QStringLiteral("en-CA"),
            QStringLiteral("ko-KR"),
            QStringLiteral("en-US")
        }}
        );
    const Application::StartupResult result = bootstrap.start();
    QCoreApplication::processEvents();

    QVERIFY(result.succeeded());
    QCOMPARE(result.status, Application::StartupStatus::StartupReady);
    QCOMPARE(observedEvents.size(), std::size_t{10});
    QCOMPARE(observedEvents[2].stage, Application::StartupStage::SettingsLoaded);
    QCOMPARE(observedEvents[3].stage, Application::StartupStage::LanguageLoaded);
    QCOMPARE(observedEvents[3].outcome, Application::StartupStageOutcome::Completed);
    QCOMPARE(languageProbe.languageChangeCount(), 1);
    QCOMPARE(
        QCoreApplication::translate("AboutDialog", "Inter Font License"),
        QStringLiteral("Inter Font Licence")
        );
    QCOMPARE(observedEvents[4].stage, Application::StartupStage::ThemeLoaded);
    QCOMPARE(observedEvents[5].stage, Application::StartupStage::RequiredFontsLoaded);
    QCOMPARE(observedEvents[6].stage, Application::StartupStage::WorkspaceOpened);
    QCOMPARE(observedEvents[6].outcome, Application::StartupStageOutcome::Warning);
    QCOMPARE(observedEvents[7].stage, Application::StartupStage::ShellCreated);
    QCOMPARE(observedEvents[8].stage, Application::StartupStage::InitialPageReady);
    QCOMPARE(observedEvents[9].stage, Application::StartupStage::StartupReady);
    QCOMPARE(observedEvents[9].outcome, Application::StartupStageOutcome::Completed);

    languageProbe.close();
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
systemDefaultFallsBackToSourceEnglishForUnsupportedLocales()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::SystemDefault),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addTheme(QStringLiteral("light")));
    QVERIFY(resources.addRequiredFonts());
    QVERIFY(resources.addDocumentCatalogMetadata());

    LanguageChangeProbe languageProbe;
    languageProbe.show();
    languageProbe.resetLanguageChangeCount();
    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot()),
        std::optional<QStringList>{QStringList{}}
        );
    const Application::StartupResult result = bootstrap.start();
    QCoreApplication::processEvents();

    QVERIFY(result.succeeded());
    QCOMPARE(result.status, Application::StartupStatus::StartupReady);
    QCOMPARE(result.stageResults.size(), std::size_t{10});
    QCOMPARE(result.completedEvents.size(), std::size_t{8});
    QCOMPARE(observedEvents.size(), std::size_t{10});
    QCOMPARE(observedEvents[3].stage, Application::StartupStage::LanguageLoaded);
    QCOMPARE(observedEvents[3].outcome, Application::StartupStageOutcome::Warning);
    QVERIFY(observedEvents[3].diagnostic.has_value());
    QCOMPARE(observedEvents[3].diagnostic->code, Application::StartupDiagnosticCode::StageWarning);
    QVERIFY(result.stageResults[3].diagnostic.has_value());
    QCOMPARE(result.stageResults[3].diagnostic->code, Application::StartupDiagnosticCode::StageWarning);
    QVERIFY(result.stageResults[3].diagnostic->stage.has_value());
    QCOMPARE(*result.stageResults[3].diagnostic->stage, Application::StartupStage::LanguageLoaded);
    QCOMPARE(languageProbe.languageChangeCount(), 0);
    QCOMPARE(
        QCoreApplication::translate("AboutDialog", "Inter Font License"),
        QStringLiteral("Inter Font License")
        );
    QVERIFY(bootstrap.shell() != nullptr);
    QVERIFY(bootstrap.shell()->isVisible());
    QCOMPARE(observedEvents[6].stage, Application::StartupStage::WorkspaceOpened);
    QCOMPARE(observedEvents[6].outcome, Application::StartupStageOutcome::Warning);
    QVERIFY(observedEvents[6].diagnostic.has_value());
    QCOMPARE(observedEvents[6].diagnostic->code,
             Application::StartupDiagnosticCode::StageWarning);
    QCOMPARE(observedEvents[9].stage, Application::StartupStage::StartupReady);

    languageProbe.close();
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::loadedPreferencesReachPagesAndFontStartupRestoresOnTeardown()
{
    auto& application = *qApp;
    const QFont previousFont = application.font();
    StartupPreferencesGuard preferences(5, 0);
    auto& settings = SettingsManager::instance();
    const auto fontKey = QString::fromUtf8(OptionKeys::FontSize);
    const auto previousStoredFont = settings.get(fontKey);
    settings.set(fontKey, 4);
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addTranslation(QStringLiteral("ko_KR")));
    QVERIFY(resources.addTheme(QStringLiteral("dark")));
    QVERIFY(resources.addRequiredFonts());
    QVERIFY(resources.addDocumentCatalogMetadata());
    {
        Bootstrap::ApplicationBootstrap bootstrap(application, {}, {}, resolverForRoot(resources.resourceRoot()));
        QVERIFY(bootstrap.start().succeeded());
        auto& host = bootstrap.shell()->pageHost();
        QVERIFY(host.activePageKey());
        auto* page = host.pageInstance(*host.activePageKey());
        QVERIFY(page);
        auto& model = page->resourceContext().preferences();
        QCOMPARE(model.snapshot().languagePreference(), Application::LanguagePreference::Korean);
        QCOMPARE(model.snapshot().themePreference(), Application::ThemePreference::Dark);
        QCOMPARE(model.snapshot().fontSize(), Application::FontSize::ExtraLarge);
        QVERIFY(!model.isDirty());
        int expectedSize = 14 + 4 + 1;
#ifdef Q_OS_MACOS
        ++expectedSize;
#endif
        QCOMPARE(application.font().pointSize(), expectedSize);
        QCOMPARE(bootstrap.shell()->font().pointSize(), expectedSize);
        QCOMPARE(settings.get(fontKey).toInt(), 4);
        QCOMPARE(settings.get(QString::fromUtf8(OptionKeys::Theme)).toInt(), 0);
        QVERIFY(bootstrap.shutdown().shutdownRequested);
        QCOMPARE(application.font(), previousFont);
    }
    QCOMPARE(application.font(), previousFont);
    if (previousStoredFont.isValid()) settings.set(fontKey, previousStoredFont);
    else settings.remove(fontKey);
}

void ApplicationBootstrapTests::shellAppearanceUsesSelectedThemeAndLanguage_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<int>("storedLanguage");
    QTest::addColumn<QString>("theme");
    QTest::addColumn<int>("storedTheme");
    QTest::newRow("english-light") << QStringLiteral("en_US") << 1
        << QStringLiteral("light") << static_cast<int>(Application::Theme::Light);
    QTest::newRow("english-dark") << QStringLiteral("en_US") << 1
        << QStringLiteral("dark") << static_cast<int>(Application::Theme::Dark);
    QTest::newRow("korean-light") << QStringLiteral("ko_KR") << 5
        << QStringLiteral("light") << static_cast<int>(Application::Theme::Light);
    QTest::newRow("korean-dark") << QStringLiteral("ko_KR") << 5
        << QStringLiteral("dark") << static_cast<int>(Application::Theme::Dark);
}

void ApplicationBootstrapTests::initialSetupPersonalDetailsAppearance_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<int>("storedLanguage");
    QTest::addColumn<QString>("theme");
    QTest::addColumn<int>("storedTheme");
    QTest::newRow("english-light") << QStringLiteral("en_US") << 1
        << QStringLiteral("light") << static_cast<int>(Application::Theme::Light);
    QTest::newRow("english-dark") << QStringLiteral("en_US") << 1
        << QStringLiteral("dark") << static_cast<int>(Application::Theme::Dark);
    QTest::newRow("korean-light") << QStringLiteral("ko_KR") << 5
        << QStringLiteral("light") << static_cast<int>(Application::Theme::Light);
    QTest::newRow("korean-dark") << QStringLiteral("ko_KR") << 5
        << QStringLiteral("dark") << static_cast<int>(Application::Theme::Dark);
}

void ApplicationBootstrapTests::initialSetupPersonalDetailsAppearance()
{
    QFETCH(QString, locale);
    QFETCH(int, storedLanguage);
    QFETCH(QString, theme);
    QFETCH(int, storedTheme);

    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(storedLanguage, storedTheme, 0);
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(locale, theme));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString selectedProfilePath = temporaryDirectory.filePath(
        QStringLiteral("appearance-setup-profile.tps")
        );
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        selectedProfilePath.toUtf8().toStdString()
        );

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams workspace;
    workspace.commandLineArguments = QStringList{QStringLiteral("appearance-test")};
    workspace.recentAutoOpenEnabled = false;
    workspace.initialSetupRecoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    workspace.initialSetupFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            fileDialogProbe
            );
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        QStringList{QStringLiteral("en-US")},
        std::move(workspace),
        [] { return std::make_unique<SilentStartupUpdateDispatcher>(); }
        );
    QVERIFY(bootstrap.start().succeeded());

    Ui::InitialPage* const page = dynamic_cast<Ui::InitialPage*>(
        bootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("initial_setup")
            )
        );
    QVERIFY(page);
    auto* const createButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    QVERIFY(createButton);
    createButton->click();

    const bool korean = locale == QStringLiteral("ko_KR");
    auto* const detailsGroup = page->findChild<QGroupBox*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetails")
        );
    auto* const nameLabel = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupPersonalNameLabel")
        );
    auto* const imageMode = page->findChild<QRadioButton*>(
        QStringLiteral("classMngrNextInitialSetupSignatureImageMode")
        );
    auto* const typeMode = page->findChild<QRadioButton*>(
        QStringLiteral("classMngrNextInitialSetupSignatureTypeMode")
        );
    auto* const chooseSignature = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupChooseSignature")
        );
    auto* const saveDetails = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsSave")
        );
    auto* const typedSignature = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTypedSignature")
        );
    auto* const preview = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupSignaturePreview")
        );
    QVERIFY(detailsGroup && nameLabel && imageMode && typeMode);
    QVERIFY(chooseSignature && saveDetails && typedSignature && preview);
    QVERIFY(!detailsGroup->isHidden());
    QCOMPARE(
        detailsGroup->title(),
        korean ? QStringLiteral("정보 입력") : QStringLiteral("Add Your Information")
        );
    QCOMPARE(
        nameLabel->text(),
        korean ? QStringLiteral("내 이름 *") : QStringLiteral("My Name *")
        );
    QCOMPARE(
        imageMode->text(),
        korean ? QStringLiteral("이미지") : QStringLiteral("Image")
        );
    QCOMPARE(
        typeMode->text(),
        korean ? QStringLiteral("직접 입력") : QStringLiteral("Type")
        );
    QCOMPARE(
        chooseSignature->text(),
        korean ? QStringLiteral("서명 이미지 추가...")
               : QStringLiteral("Add Signature Image...")
        );
    QCOMPARE(
        saveDetails->text(),
        korean ? QStringLiteral("변경 사항 저장") : QStringLiteral("Save Changes")
        );

    auto* const teacherEntryGroup = page->findChild<QGroupBox*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntry")
        );
    auto* const teacherEnglishName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEnglishName")
        );
    auto* const addAnotherTeacher = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupAddAnotherTeacher")
        );
    auto* const continueTeacherEntry = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    auto* const teacherKoreanName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherKoreanName")
        );
    auto* const teacherPreferredSpelling = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherPreferredSpelling")
        );
    QVERIFY(teacherEntryGroup && teacherEnglishName);
    QVERIFY(addAnotherTeacher && continueTeacherEntry);
    QVERIFY(teacherKoreanName && teacherPreferredSpelling);
    QCOMPARE(
        teacherEntryGroup->title(),
        korean ? QStringLiteral("교사 정보") : QStringLiteral("Teacher Information")
        );
    QCOMPARE(
        addAnotherTeacher->text(),
        korean ? QStringLiteral("다른 교사 추가") : QStringLiteral("Add Another Teacher")
        );
    QCOMPARE(
        continueTeacherEntry->text(),
        korean ? QStringLiteral("계속") : QStringLiteral("Continue")
        );
    const QList<QGroupBox*> teacherGroups = page->findChildren<QGroupBox*>();
    const bool hasRequiredTeacherGroup = std::any_of(
        teacherGroups.cbegin(), teacherGroups.cend(), [korean](const QGroupBox* group) {
            return group->title() == (korean
                ? QStringLiteral("필수 정보")
                : QStringLiteral("Required Information"));
        });
    const bool hasOptionalTeacherGroup = std::any_of(
        teacherGroups.cbegin(), teacherGroups.cend(), [korean](const QGroupBox* group) {
            return group->title() == (korean
                ? QStringLiteral("선택 정보")
                : QStringLiteral("Optional Details"));
        });
    QVERIFY(hasRequiredTeacherGroup);
    QVERIFY(hasOptionalTeacherGroup);
    QStringList teacherLabels;
    for (const QLabel* label : teacherEntryGroup->findChildren<QLabel*>())
    {
        teacherLabels.append(label->text());
    }
    QVERIFY(teacherLabels.contains(
        korean ? QStringLiteral("영어 이름 *") : QStringLiteral("English Name *")
        ));
    QVERIFY(teacherLabels.contains(
        korean ? QStringLiteral("한국어 이름 *") : QStringLiteral("Korean Name *")
        ));
    QVERIFY(teacherLabels.contains(
        korean ? QStringLiteral("선호 표기 *") : QStringLiteral("Preferred Spelling *")
        ));
    QVERIFY(teacherLabels.contains(
        korean ? QStringLiteral("교실 번호") : QStringLiteral("Room Number")
        ));

    auto* const classDetailsGroup = page->findChild<QGroupBox*>(
        QStringLiteral("classMngrNextInitialSetupClassDetails")
        );
    QVERIFY(classDetailsGroup);
    QVERIFY(classDetailsGroup->isHidden());
    QCOMPARE(
        teacherEnglishName->palette().color(QPalette::Text).name(),
        theme == QStringLiteral("dark") ? QStringLiteral("#f0f0f0")
                                          : QStringLiteral("#27313a")
        );
    teacherEnglishName->setText(QStringLiteral("Alex Kim"));
    teacherKoreanName->setText(QString::fromUtf8("김선생님"));
    teacherPreferredSpelling->setText(QStringLiteral("Kim Seonsaeng"));
    continueTeacherEntry->click();
    QVERIFY(!classDetailsGroup->isHidden());
    QCOMPARE(
        classDetailsGroup->title(),
        korean ? QStringLiteral("학급 정보") : QStringLiteral("Class Details")
        );
    auto* const classTeacher = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassTeacher")
        );
    auto* const chooseClassColor = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassColorChoose")
        );
    auto* const classGrade = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassGrade")
        );
    auto* const classLevel = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassLevel")
        );
    auto* const colorPreview = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupClassColorPreview")
        );
    QVERIFY(classTeacher && chooseClassColor && classGrade && classLevel);
    QVERIFY(colorPreview);
    QCOMPARE(classTeacher->count(), 1);
    QCOMPARE(classTeacher->currentData().toString(), QStringLiteral("1"));
    QCOMPARE(chooseClassColor->text(),
             korean ? QStringLiteral("색상 선택") : QStringLiteral("Choose Color"));
    QCOMPARE(classGrade->currentText(), QStringLiteral("E4"));
    QCOMPARE(classLevel->currentText(), QStringLiteral("Theseus"));
    QVERIFY(colorPreview->styleSheet().contains(
        QStringLiteral("background-color:#FFFFFF;color:#000000")
        ));
    QStringList classDetailsLabels;
    for (const QLabel* label : classDetailsGroup->findChildren<QLabel*>())
    {
        classDetailsLabels.append(label->text());
    }
    QVERIFY(classDetailsLabels.contains(
        korean ? QStringLiteral("한국인 교사 *")
               : QStringLiteral("Korean Teacher *")
        ));
    QVERIFY(classDetailsLabels.contains(
        korean ? QStringLiteral("수업 색상 *")
               : QStringLiteral("Class Color *")
        ));
    QVERIFY(classDetailsLabels.contains(
        korean ? QStringLiteral("읽기 교재") : QStringLiteral("Reading Book")
        ));
    QCOMPARE(
        application.styleSheet(),
        readTextFile(QDir(resources.resourceRoot()).filePath(
            QStringLiteral("assets/styles/%1.qss").arg(theme)
            ))
        );

    typeMode->click();
    QCOMPARE(
        typedSignature->placeholderText(),
        korean ? QStringLiteral("서명을 입력하세요")
               : QStringLiteral("Type your signature")
        );
    typedSignature->setText(QString::fromUtf8("김선생님"));
    QVERIFY(preview->text().isEmpty());

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::shellAppearanceUsesSelectedThemeAndLanguage()
{
    QFETCH(QString, locale);
    QFETCH(int, storedLanguage);
    QFETCH(QString, theme);
    QFETCH(int, storedTheme);
    QApplication& application = *qApp;
    const bool korean = locale == QStringLiteral("ko_KR");
    const QString baseStyleName = application.style()->objectName();
    StartupPreferencesGuard preferences(storedLanguage, storedTheme, 0);
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(locale, theme));
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams workspace;
    workspace.commandLineArguments = QStringList{QStringLiteral("appearance-test")};
    workspace.recentAutoOpenEnabled = false;
    Bootstrap::ApplicationBootstrap bootstrap(
        application, {}, silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()), QStringList{QStringLiteral("en-US")},
        std::move(workspace), [] {
            return std::make_unique<SilentStartupUpdateDispatcher>();
        });
    QVERIFY(bootstrap.start().succeeded());
    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    QCOMPARE(shell->windowTitle(), QStringLiteral("ClassMngr Next"));
    shell->resize(1024, 720);
    const QString expectedQss = readTextFile(QDir(resources.resourceRoot()).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme)));
    QCOMPARE(application.styleSheet(), expectedQss);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);
    const auto selectedPage = *Ui::PageId::fromString(QStringLiteral("initial_setup"));
    QCOMPARE(shell->navigationModel()->currentPageId(), std::optional{selectedPage});
    QCOMPARE(shell->navigationModel()->selectedPageId(), std::optional{selectedPage});
    QCOMPARE(shell->pageHost().activePageKey(), std::optional{selectedPage.value()});

    auto* const setupPage = dynamic_cast<Ui::InitialPage*>(
        shell->pageHost().pageInstance(QStringLiteral("initial_setup"))
        );
    QVERIFY(setupPage);
    auto* const setupTitle = setupPage->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTitle")
        );
    auto* const createProfileButton = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    QVERIFY(setupTitle);
    QVERIFY(createProfileButton);
    QCOMPARE(
        setupTitle->text(),
        korean ? QStringLiteral("초기 설정") : QStringLiteral("Initial Setup")
        );
    QCOMPARE(
        createProfileButton->text(),
        korean ? QStringLiteral("프로필 만들기…") : QStringLiteral("Create Profile…")
        );

    struct Label final { const char* id; const char* english; const char* korean; };
    const Label pageLabels[] = {
        {"my_workspace", "My Workspace", "내 작업 공간"},
        {"classes", "Classes", "수업"},
        {"sub_prep", "Sub Prep", "대체 수업 준비"},
        {"native_english_teachers", "Native English Teachers", "원어민 영어 교사"},
        {"gs_team", "GS Team", "GS 팀"},
        {"campus_information", "Information", "정보"},
        {"initial_setup", "Initial Setup", "초기 설정"},
        {"campus_directions", "Directions", "오시는 길"},
        {"campus_address", "Address", "주소"},
        {"campus_housing", "Housing", "숙소"},
        {"campus_map", "Maps", "지도"}
    };
    for (const Label& expected : pageLabels)
    {
        const auto descriptor = shell->pageHost().pageDescriptor(
            *Ui::PageId::fromString(QString::fromLatin1(expected.id)));
        QVERIFY(descriptor);
        QCOMPARE(descriptor->label, QString::fromUtf8(korean ? expected.korean : expected.english));
        QVERIFY(!descriptor->iconResourceId);
    }
    const Label nodeLabels[] = {
        {"co_teachers", "Co-Teachers", "공동 교사"},
        {"campus_staff", "Campus Staff", "캠퍼스 직원"},
        {"teachers_all_korean", "Korean Teachers", "한국인 교사"},
        {"useful_links", "Useful Links", "유용한 링크"},
        {"useful_dropbox", "Dropbox", "Dropbox"},
        {"useful_vacation_calendar", "Vacation Calendar", "방학 캘린더"},
        {"useful_yearly_calendar", "Yearly Calendar", "연간 캘린더"},
        {"useful_training_website", "Training Website", "교육 웹사이트"},
        {"useful_net_website", "NET Website", "NET 웹사이트"},
        {"useful_lms_website", "LMS Website", "LMS 웹사이트"},
        {"useful_highlights_library", "Highlights Library", "Highlights 라이브러리"},
        {"campus_info", "Campus Directory", "캠퍼스 디렉터리"}
    };
    QMap<QString, QString> actualNodeLabels;
    std::function<void(const Ui::NavigationNode&)> collectLabels;
    collectLabels = [&](const Ui::NavigationNode& node) {
        if (node.kind != Ui::NavigationNode::Kind::Page)
        {
            actualNodeLabels.insert(node.id, node.label);
        }
        for (const auto& child : node.children) collectLabels(child);
    };
    for (const auto& node : shell->navigationModel()->nodes()) collectLabels(node);
    QCOMPARE(actualNodeLabels.size(), qsizetype(std::size(nodeLabels)));
    for (const Label& expected : nodeLabels)
    {
        QCOMPARE(actualNodeLabels.value(QString::fromLatin1(expected.id)),
                 QString::fromUtf8(korean ? expected.korean : expected.english));
    }

    auto* const tree = shell->findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree"));
    auto* const back = shell->findChild<QPushButton*>(
        QStringLiteral("classMngrNextNavigationBack"));
    auto* const forward = shell->findChild<QPushButton*>(
        QStringLiteral("classMngrNextNavigationForward"));
    QVERIFY(tree && back && forward);
    QCOMPARE(back->text(), korean ? QStringLiteral("뒤로") : QStringLiteral("Back"));
    QCOMPARE(forward->text(), korean ? QStringLiteral("앞으로") : QStringLiteral("Forward"));
    QVERIFY(!back->isEnabled());
    QVERIFY(!forward->isEnabled());
    QCOMPARE(tree->selectedItems().size(), 1);
    QVERIFY(tree->currentItem());
    QCOMPARE(tree->currentItem()->text(0), korean ? QStringLiteral("초기 설정") : QStringLiteral("Initial Setup"));
    QVERIFY(tree->currentItem()->isSelected());
    QVERIFY(!tree->currentItem()->parent());
    QStringList treeLabels;
    for (QTreeWidgetItemIterator item(tree); *item; ++item)
    {
        treeLabels.push_back((*item)->text(0));
        QVERIFY((*item)->icon(0).isNull());
    }
    const QStringList englishTreeLabels{
        QStringLiteral("Useful Links"), QStringLiteral("Dropbox"),
        QStringLiteral("Vacation Calendar"), QStringLiteral("Yearly Calendar"),
        QStringLiteral("Training Website"), QStringLiteral("NET Website"),
        QStringLiteral("LMS Website"), QStringLiteral("Highlights Library"),
        QStringLiteral("Campus Directory"), QStringLiteral("Information"),
        QStringLiteral("Directions"), QStringLiteral("Address"),
        QStringLiteral("Housing"), QStringLiteral("Maps"),
        QStringLiteral("Initial Setup")};
    const QStringList koreanTreeLabels{
        QStringLiteral("유용한 링크"), QStringLiteral("Dropbox"),
        QStringLiteral("방학 캘린더"), QStringLiteral("연간 캘린더"),
        QStringLiteral("교육 웹사이트"), QStringLiteral("NET 웹사이트"),
        QStringLiteral("LMS 웹사이트"), QStringLiteral("Highlights 라이브러리"),
        QStringLiteral("캠퍼스 디렉터리"), QStringLiteral("정보"),
        QStringLiteral("오시는 길"), QStringLiteral("주소"),
        QStringLiteral("숙소"), QStringLiteral("지도"),
        QStringLiteral("초기 설정")};
    QCOMPARE(treeLabels, korean ? koreanTreeLabels : englishTreeLabels);
    auto* const page = shell->pageHost().pageInstance(selectedPage.value());
    QVERIFY(page);
    auto* const title = page->findChild<QLabel*>(QStringLiteral("classMngrNextInitialSetupTitle"));
    auto* const description = page->findChild<QLabel*>(QStringLiteral("classMngrNextInitialSetupDescription"));
    QVERIFY(title && description);
    QCOMPARE(title->text(), tree->currentItem()->text(0));
    QCOMPARE(description->text(), korean
        ? QStringLiteral("ClassMngr 프로필을 만들어 설정을 시작하세요. 프로필은 .tps 파일로 저장됩니다.")
        : QStringLiteral("Create a ClassMngr profile to begin setup. Your profile is saved as a .tps file."));

    shell->activateWindow();
    tree->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(tree->hasFocus());
    QTRY_COMPARE(shell->size(), QSize(1024, 720));
    shell->repaint();
    QApplication::processEvents();
    QCOMPARE(tree->frameWidth(), 2);
    const int descriptionHeightForWidth = description->heightForWidth(description->width());
    QVERIFY2(
        description->height() >= descriptionHeightForWidth,
        qPrintable(QStringLiteral("description geometry is %1x%2; heightForWidth=%3")
                       .arg(description->width())
                       .arg(description->height())
                       .arg(descriptionHeightForWidth))
        );
    const QPixmap capture = shell->grab();
    QVERIFY(!capture.isNull());
    QCOMPARE(capture.size() / capture.devicePixelRatio(), QSize(1024, 720));
    const QString outputDirectory = qEnvironmentVariable("CLASSMNGR_NEXT_APPEARANCE_CAPTURE_DIR");
    if (!outputDirectory.isEmpty())
    {
        QCOMPARE(capture.devicePixelRatio(), qreal(1));
        QVERIFY(QDir().mkpath(outputDirectory));
        const QString filename = QStringLiteral("shell-%1-%2.png").arg(locale, theme);
        QVERIFY(capture.save(QDir(outputDirectory).filePath(filename), "PNG"));
        QFile translation(QDir(resources.resourceRoot()).filePath(
            QStringLiteral("assets/translations/ClassMngr_%1.qm").arg(locale)));
        QVERIFY(translation.open(QIODevice::ReadOnly));
        m_appearanceCaptures.append(QJsonObject{
            {QStringLiteral("file"), filename},
            {QStringLiteral("locale"), locale},
            {QStringLiteral("themePreference"), theme},
            {QStringLiteral("resolvedQss"), QStringLiteral("resource:assets/styles/%1.qss").arg(theme)},
            {QStringLiteral("qssSha256"), QString::fromLatin1(QCryptographicHash::hash(
                expectedQss.toUtf8(), QCryptographicHash::Sha256).toHex())},
            {QStringLiteral("translationQmSha256"), QString::fromLatin1(QCryptographicHash::hash(
                translation.readAll(), QCryptographicHash::Sha256).toHex())},
            {QStringLiteral("route"), selectedPage.value()},
            {QStringLiteral("workspaceState"), QStringLiteral("closed")},
            {QStringLiteral("focusTarget"), tree->objectName()},
            {QStringLiteral("selectedLabel"), tree->currentItem()->text(0)},
            {QStringLiteral("logicalSize"), QJsonArray{shell->width(), shell->height()}},
            {QStringLiteral("pixelSize"), QJsonArray{capture.width(), capture.height()}},
            {QStringLiteral("devicePixelRatio"), capture.devicePixelRatio()},
            {QStringLiteral("fontFamily"), application.font().family()},
            {QStringLiteral("fontPointSize"), application.font().pointSize()},
            {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
            {QStringLiteral("qpaPlatform"), QGuiApplication::platformName()},
            {QStringLiteral("baseStyle"), baseStyleName},
            {QStringLiteral("styleClass"), QString::fromLatin1(application.style()->metaObject()->className())},
            {QStringLiteral("operatingSystem"), QSysInfo::prettyProductName()}
        });
    }
    QVERIFY(bootstrap.shutdown().shutdownRequested);
}

void ApplicationBootstrapTests::systemDefaultThemeFollowsCurrentStyleColorScheme()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::SystemDefault)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addTranslation(QStringLiteral("ko_KR")));
    const QString theme = application.styleHints()->colorScheme() == Qt::ColorScheme::Dark
        ? QStringLiteral("dark")
        : QStringLiteral("light");
    QVERIFY(resources.addTheme(theme));
    QVERIFY(resources.addRequiredFonts());
    QVERIFY(resources.addDocumentCatalogMetadata());

    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot())
        );
    const Application::StartupResult result = bootstrap.start();

    QVERIFY(result.succeeded());
    QCOMPARE(observedEvents.size(), std::size_t{10});
    QCOMPARE(observedEvents[4].stage, Application::StartupStage::ThemeLoaded);
    QCOMPARE(observedEvents[4].outcome, Application::StartupStageOutcome::Completed);
    QCOMPARE(
        application.styleSheet(),
        readTextFile(QDir(resources.resourceRoot()).filePath(
            QStringLiteral("assets/styles/%1.qss").arg(theme)
            ))
        );
    QCOMPARE(observedEvents[6].stage, Application::StartupStage::WorkspaceOpened);
    QCOMPARE(observedEvents[6].outcome, Application::StartupStageOutcome::Warning);
    QCOMPARE(observedEvents[7].stage, Application::StartupStage::ShellCreated);
    QCOMPARE(observedEvents[8].stage, Application::StartupStage::InitialPageReady);
    QCOMPARE(observedEvents[9].stage, Application::StartupStage::StartupReady);

    const Application::ShutdownResult shutdown = bootstrap.shutdown();
    QVERIFY(shutdown.shutdownRequested);
    QCOMPARE(application.styleSheet(), previousStyleSheet);
}

void ApplicationBootstrapTests::
explicitEnglishSelectsCanadianTranslation()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addTranslation(QStringLiteral("en_CA")));
    QVERIFY(resources.addTheme(QStringLiteral("light")));
    QVERIFY(resources.addRequiredFonts());
    QVERIFY(resources.addDocumentCatalogMetadata());

    LanguageChangeProbe languageProbe;
    languageProbe.show();
    languageProbe.resetLanguageChangeCount();

    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot()),
        std::optional<QStringList>{QStringList{QStringLiteral("en-CA")}}
        );
    const Application::StartupResult result = bootstrap.start();
    QCoreApplication::processEvents();

    QVERIFY(result.succeeded());
    QCOMPARE(observedEvents[3].stage, Application::StartupStage::LanguageLoaded);
    QCOMPARE(observedEvents[3].outcome, Application::StartupStageOutcome::Completed);
    QCOMPARE(languageProbe.languageChangeCount(), 1);
    QCOMPARE(
        QCoreApplication::translate("AboutDialog", "Inter Font License"),
        QStringLiteral("Inter Font Licence")
        );

    languageProbe.close();
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
explicitEnglishFallsBackToUsWhenNoEnglishRegionMatches()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addTranslation(QStringLiteral("en_US")));
    QVERIFY(resources.addTheme(QStringLiteral("light")));
    QVERIFY(resources.addRequiredFonts());
    QVERIFY(resources.addDocumentCatalogMetadata());

    LanguageChangeProbe languageProbe;
    languageProbe.show();
    languageProbe.resetLanguageChangeCount();
    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot()),
        std::optional<QStringList>{QStringList{QStringLiteral("fr-FR")}}
        );
    const Application::StartupResult result = bootstrap.start();
    QCoreApplication::processEvents();

    QVERIFY(result.succeeded());
    QCOMPARE(observedEvents[3].stage, Application::StartupStage::LanguageLoaded);
    QCOMPARE(observedEvents[3].outcome, Application::StartupStageOutcome::Completed);
    QCOMPARE(languageProbe.languageChangeCount(), 1);
    // The en_US catalog intentionally has no differing string for this key;
    // the install event proves the US fallback catalog was still selected.
    QCOMPARE(
        QCoreApplication::translate("AboutDialog", "Inter Font License"),
        QStringLiteral("Inter Font License")
        );

    languageProbe.close();
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::translationFailureStopsBeforeThemeAndShell()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    const QStringList fontsBefore = QFontDatabase::families();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Dark)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());

    LanguageChangeProbe languageProbe;
    languageProbe.show();
    languageProbe.resetLanguageChangeCount();
    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot())
        );

    const Application::StartupResult result = bootstrap.start();
    QCoreApplication::processEvents();

    verifyTypedStageFailure(result, Application::StartupStage::LanguageLoaded);
    QCOMPARE(observedEvents.size(), std::size_t{4});
    QCOMPARE(observedEvents.back().stage, Application::StartupStage::LanguageLoaded);
    QCOMPARE(observedEvents.back().outcome, Application::StartupStageOutcome::Failed);
    QCOMPARE(languageProbe.languageChangeCount(), 0);
    QCOMPARE(application.styleSheet(), previousStyleSheet);
    QVERIFY(bootstrap.shell() == nullptr);
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::ThemeLoaded));
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::RequiredFontsLoaded));
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::WorkspaceOpened));
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::StartupReady));
    QCOMPARE(
        containsFontFamilyLike(QFontDatabase::families(), QStringLiteral("Inter")),
        containsFontFamilyLike(fontsBefore, QStringLiteral("Inter"))
        );
    QCOMPARE(
        containsFontFamilyLike(QFontDatabase::families(), QStringLiteral("Pretendard")),
        containsFontFamilyLike(fontsBefore, QStringLiteral("Pretendard"))
        );

    languageProbe.close();
}

void ApplicationBootstrapTests::themeFailureRollsBackInstalledTranslation()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Dark)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addTranslation(QStringLiteral("ko_KR")));

    LanguageChangeProbe languageProbe;
    languageProbe.show();
    languageProbe.resetLanguageChangeCount();
    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot())
        );

    const Application::StartupResult result = bootstrap.start();
    QCoreApplication::processEvents();

    verifyTypedStageFailure(result, Application::StartupStage::ThemeLoaded);
    QCOMPARE(observedEvents.size(), std::size_t{5});
    QCOMPARE(observedEvents[3].stage, Application::StartupStage::LanguageLoaded);
    QCOMPARE(observedEvents[3].outcome, Application::StartupStageOutcome::Completed);
    QCOMPARE(observedEvents.back().stage, Application::StartupStage::ThemeLoaded);
    QCOMPARE(observedEvents.back().outcome, Application::StartupStageOutcome::Failed);
    QCOMPARE(languageProbe.languageChangeCount(), 1);
    QCOMPARE(
        QCoreApplication::translate("AboutDialog", "Inter Font License"),
        QStringLiteral("Inter Font License")
        );
    QCOMPARE(application.styleSheet(), previousStyleSheet);
    QVERIFY(bootstrap.shell() == nullptr);
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::RequiredFontsLoaded));
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::WorkspaceOpened));
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::StartupReady));

    languageProbe.close();
}

void ApplicationBootstrapTests::
requiredFontFailureRollsBackTranslationThemeAndFirstFont()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    const QStringList fontsBefore = QFontDatabase::families();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Dark)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addTranslation(QStringLiteral("ko_KR")));
    QVERIFY(resources.addTheme(QStringLiteral("dark")));
    QVERIFY(resources.addResource(QStringLiteral("assets/fonts/Inter.ttc")));

    LanguageChangeProbe languageProbe;
    languageProbe.show();
    languageProbe.resetLanguageChangeCount();
    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot())
        );

    const Application::StartupResult result = bootstrap.start();
    QCoreApplication::processEvents();

    verifyTypedStageFailure(result, Application::StartupStage::RequiredFontsLoaded);
    QCOMPARE(observedEvents.size(), std::size_t{6});
    QCOMPARE(observedEvents[3].stage, Application::StartupStage::LanguageLoaded);
    QCOMPARE(observedEvents[4].stage, Application::StartupStage::ThemeLoaded);
    QCOMPARE(observedEvents.back().stage, Application::StartupStage::RequiredFontsLoaded);
    QCOMPARE(languageProbe.languageChangeCount(), 1);
    QCOMPARE(
        QCoreApplication::translate("AboutDialog", "Inter Font License"),
        QStringLiteral("Inter Font License")
        );
    QCOMPARE(application.styleSheet(), previousStyleSheet);
    QVERIFY(bootstrap.shell() == nullptr);
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::WorkspaceOpened));
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::StartupReady));
    QCOMPARE(
        containsFontFamilyLike(QFontDatabase::families(), QStringLiteral("Inter")),
        containsFontFamilyLike(fontsBefore, QStringLiteral("Inter"))
        );
    QCOMPARE(
        containsFontFamilyLike(QFontDatabase::families(), QStringLiteral("Pretendard")),
        containsFontFamilyLike(fontsBefore, QStringLiteral("Pretendard"))
        );

    languageProbe.close();
}

void ApplicationBootstrapTests::
diagnosticsSinkConstructionFailureStopsBeforeShellCreation()
{
    QApplication& application = *qApp;
    const bool previousQuitPolicy = application.quitOnLastWindowClosed();
    std::vector<Application::StartupEvent> observedEvents;

    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=process-created outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtCriticalMsg,
        "ClassMngrNext startup stage=core-services-created outcome=failed "
        "diagnostic=stage-failed diagnostic_stage=core-services-created"
        );
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        []() -> std::unique_ptr<Bootstrap::StartupDiagnosticsSink> {
            throw std::runtime_error("injected diagnostics sink construction failure");
        }
        );

    const Application::StartupResult result = bootstrap.start();

    QCOMPARE(result.status, Application::StartupStatus::Failed);
    QVERIFY(!result.succeeded());
    QVERIFY(result.failure.has_value());
    QCOMPARE(*result.failure, Application::StartupFailureReason::StageFailed);
    QVERIFY(result.diagnostic.has_value());
    QCOMPARE(result.diagnostic->code, Application::StartupDiagnosticCode::StageFailed);
    QVERIFY(result.diagnostic->stage.has_value());
    QCOMPARE(*result.diagnostic->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(result.stageResults.size(), std::size_t{2});
    QCOMPARE(result.completedEvents.size(), std::size_t{1});
    QVERIFY(result.latestStageResult.has_value());
    QCOMPARE(result.latestStageResult->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(result.latestStageResult->outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(result.latestStageResult->diagnostic.has_value());
    QCOMPARE(
        result.latestStageResult->diagnostic->code,
        Application::StartupDiagnosticCode::StageFailed
        );
    QCOMPARE(result.stageResults[0].stage, Application::StartupStage::ProcessCreated);
    QCOMPARE(result.stageResults[0].outcome, Application::StartupStageOutcome::Completed);
    QCOMPARE(result.stageResults[1].stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(result.stageResults[1].outcome, Application::StartupStageOutcome::Failed);
    QCOMPARE(result.completedEvents[0].stage, Application::StartupStage::ProcessCreated);

    QCOMPARE(observedEvents.size(), std::size_t{2});
    QCOMPARE(observedEvents[0].stage, Application::StartupStage::ProcessCreated);
    QCOMPARE(observedEvents[0].outcome, Application::StartupStageOutcome::Completed);
    QCOMPARE(observedEvents[1].stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(observedEvents[1].outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(observedEvents[1].diagnostic.has_value());
    QCOMPARE(observedEvents[1].diagnostic->code, Application::StartupDiagnosticCode::StageFailed);
    QVERIFY(observedEvents[1].diagnostic->stage.has_value());
    QCOMPARE(*observedEvents[1].diagnostic->stage, Application::StartupStage::CoreServicesCreated);

    QVERIFY(bootstrap.shell() == nullptr);
    QCOMPARE(application.quitOnLastWindowClosed(), previousQuitPolicy);

    const Application::StartupResult repeatedStart = bootstrap.start();
    QCOMPARE(repeatedStart.status, Application::StartupStatus::Failed);
    QVERIFY(repeatedStart.failure.has_value());
    QCOMPARE(*repeatedStart.failure, Application::StartupFailureReason::AlreadyStarted);
    QCOMPARE(observedEvents.size(), std::size_t{2});
    QVERIFY(bootstrap.shell() == nullptr);
}

void ApplicationBootstrapTests::generatedCatalogStartsWithOnlySelectedStartupPayloads()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));

    auto catalog = Resources::ResourceCatalog::load(
        configuredResourceCatalogPath(),
        resources.resourceRoot()
        );
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    QVERIFY(!catalog->records().empty());
    const std::array expectedStartupPayloads{
        QStringLiteral("assets/translations/ClassMngr_ko_KR.qm"),
        QStringLiteral("assets/styles/light.qss"),
        QStringLiteral("assets/fonts/Inter.ttc"),
        QStringLiteral("assets/fonts/PretendardVariable.ttf"),
        QStringLiteral("assets/documents/documents.json")
    };
    std::size_t foundStartupPayloads = 0;
    for (const Resources::ResourceDescriptor& record : catalog->records())
    {
        if (record.location() != Resources::ResourceLocation::PackageFile)
        {
            continue;
        }
        const QString payloadPath = QDir(resources.resourceRoot()).filePath(
            record.packageRelativePath()
            );
        const bool expected = std::find(
            expectedStartupPayloads.begin(),
            expectedStartupPayloads.end(),
            record.packageRelativePath()
            ) != expectedStartupPayloads.end();
        QCOMPARE(QFileInfo::exists(payloadPath), expected);
        if (expected)
        {
            ++foundStartupPayloads;
        }
    }
    QCOMPARE(foundStartupPayloads, expectedStartupPayloads.size());

    std::vector<Application::StartupEvent> observedEvents;
    int resolverCalls = 0;
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=process-created outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=core-services-created outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=settings-loaded outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=language-loaded outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=theme-loaded outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=required-fonts-loaded outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=shell-created outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=initial-page-ready outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    QTest::ignoreMessage(
        QtInfoMsg,
        "ClassMngrNext startup stage=startup-ready outcome=completed "
        "diagnostic=none diagnostic_stage=none"
        );
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        [&resolverCalls, resourceRoot = resources.resourceRoot()]()
            -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
            ++resolverCalls;
            return Resources::ResourcePackagePaths{
                resourceRoot,
                configuredResourceCatalogPath()
            };
        }
        );

    const Application::StartupResult result = bootstrap.start();

    QVERIFY(result.succeeded());
    QCOMPARE(result.status, Application::StartupStatus::StartupReady);
    QVERIFY(!result.failure.has_value());
    QVERIFY(!result.diagnostic.has_value());
    QCOMPARE(resolverCalls, 1);
    QCOMPARE(result.completedEvents.size(), std::size_t{9});
    verifyWorkspaceStartupOrder(
        result,
        observedEvents,
        bootstrap,
        Application::StartupStageOutcome::Warning
        );

    QVERIFY(bootstrap.shell() != nullptr);
    QVERIFY(bootstrap.shell()->isVisible());
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::missingResourcePackageFailsCoreServicesBeforeShellCreation()
{
    QApplication& application = *qApp;
    const bool previousQuitPolicy = application.quitOnLastWindowClosed();
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString missingRoot = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("missing-resources")
        );

    std::vector<Application::StartupEvent> observedEvents;
    int resolverCalls = 0;
    ignoreCoreServicesFailureMessages();
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        [&resolverCalls, missingRoot]()
            -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
            ++resolverCalls;
            return Resources::ResourcePackagePaths{
                missingRoot,
                configuredResourceCatalogPath()
            };
        }
        );

    const Application::StartupResult result = bootstrap.start();

    verifyCoreServicesFailure(result, observedEvents, bootstrap, application, previousQuitPolicy);
    QCOMPARE(resolverCalls, 1);
}

void ApplicationBootstrapTests::packagePathResolutionFailureDoesNotUseCurrentDirectoryFallback()
{
    QApplication& application = *qApp;
    const bool previousQuitPolicy = application.quitOnLastWindowClosed();
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString workingDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("working")
        );
    QVERIFY(QDir().mkpath(workingDirectory));
    const QString fallbackRoot = QDir(workingDirectory).filePath(
        configuredResourceRelativeRoot()
        );
    QVERIFY(QDir().mkpath(fallbackRoot));
    const QString fallbackCatalogPath = QDir(fallbackRoot).filePath(
        QStringLiteral("resource-catalog.json")
        );
    QVERIFY(QFile::copy(configuredResourceCatalogPath(), fallbackCatalogPath));

    const QString previousCurrentDirectory = QDir::currentPath();
    std::vector<Application::StartupEvent> observedEvents;
    int resolverCalls = 0;
    ignoreCoreServicesFailureMessages();
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        [&resolverCalls]() -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
            ++resolverCalls;
            return std::unexpected(Resources::ResourceError{
                Resources::ResourceErrorCode::MissingFile,
                QStringLiteral("injected missing package")
            });
        }
        );

    QVERIFY(QDir::setCurrent(workingDirectory));
    const Application::StartupResult result = bootstrap.start();
    QVERIFY(QDir::setCurrent(previousCurrentDirectory));

    verifyCoreServicesFailure(result, observedEvents, bootstrap, application, previousQuitPolicy);
    QCOMPARE(resolverCalls, 1);
    QCOMPARE(QDir::currentPath(), previousCurrentDirectory);
}

void ApplicationBootstrapTests::invalidResourceCatalogFailsCoreServicesBeforeShellCreation()
{
    QApplication& application = *qApp;
    const bool previousQuitPolicy = application.quitOnLastWindowClosed();
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString resourceRoot = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("resources")
        );
    QVERIFY(QDir().mkpath(resourceRoot));
    const QString invalidCatalogPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("invalid-resource-catalog.json")
        );
    QFile invalidCatalog(invalidCatalogPath);
    QVERIFY(invalidCatalog.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(invalidCatalog.write("{ invalid json\n"), qint64{15});
    invalidCatalog.close();

    std::vector<Application::StartupEvent> observedEvents;
    int resolverCalls = 0;
    ignoreCoreServicesFailureMessages();
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        [&resolverCalls, resourceRoot, invalidCatalogPath]()
            -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
            ++resolverCalls;
            return Resources::ResourcePackagePaths{
                resourceRoot,
                invalidCatalogPath
            };
        }
        );

    const Application::StartupResult result = bootstrap.start();

    verifyCoreServicesFailure(result, observedEvents, bootstrap, application, previousQuitPolicy);
    QCOMPARE(resolverCalls, 1);
}

void ApplicationBootstrapTests::documentCatalogMetadataFailureStopsBeforeShellCreation()
{
    QApplication& application = *qApp;
    const QFont previousFont = application.font();
    const bool previousQuitPolicy = application.quitOnLastWindowClosed();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(expectedEnglishLocale(), QStringLiteral("light")));
    QVERIFY(resources.removeDocumentCatalogMetadata());
    QVERIFY(!QFileInfo::exists(QDir(resources.resourceRoot()).filePath(
        QStringLiteral("assets/documents/documents.json")
        )));

    std::vector<Application::StartupEvent> observedEvents;
    QTest::ignoreMessage(
        QtCriticalMsg,
        "ClassMngrNext could not project startup document catalog metadata: "
        "The document catalog metadata resource could not be loaded."
        );
    QTest::ignoreMessage(
        QtCriticalMsg,
        "ClassMngrNext startup stage=initial-page-ready outcome=failed "
        "diagnostic=initial-experience-unavailable "
        "diagnostic_stage=initial-page-ready"
        );
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        {},
        resolverForRoot(resources.resourceRoot())
        );

    const Application::StartupResult result = bootstrap.start();

    QCOMPARE(result.status, Application::StartupStatus::Failed);
    QVERIFY(!result.succeeded());
    QVERIFY(result.failure.has_value());
    QCOMPARE(*result.failure, Application::StartupFailureReason::InitialExperienceUnavailable);
    QVERIFY(result.diagnostic.has_value());
    QCOMPARE(
        result.diagnostic->code,
        Application::StartupDiagnosticCode::InitialExperienceUnavailable
        );
    QVERIFY(result.diagnostic->stage.has_value());
    QCOMPARE(*result.diagnostic->stage, Application::StartupStage::InitialPageReady);
    QCOMPARE(result.stageResults.size(), std::size_t{8});
    QCOMPARE(result.completedEvents.size(), std::size_t{6});

    constexpr std::array expectedStages{
        Application::StartupStage::ProcessCreated,
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::InitialPageReady
    };
    QCOMPARE(observedEvents.size(), expectedStages.size());
    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        QCOMPARE(observedEvents[index].stage, expectedStages[index]);
        QCOMPARE(result.stageResults[index].stage, expectedStages[index]);
    }
    QCOMPARE(observedEvents[6].outcome, Application::StartupStageOutcome::Warning);
    QCOMPARE(result.stageResults[6].outcome, Application::StartupStageOutcome::Warning);
    QCOMPARE(observedEvents.back().stage, Application::StartupStage::InitialPageReady);
    QCOMPARE(observedEvents.back().outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(observedEvents.back().diagnostic.has_value());
    QCOMPARE(
        observedEvents.back().diagnostic->code,
        Application::StartupDiagnosticCode::InitialExperienceUnavailable
        );
    QVERIFY(result.latestStageResult.has_value());
    QCOMPARE(result.latestStageResult->stage, observedEvents.back().stage);
    QCOMPARE(result.latestStageResult->outcome, observedEvents.back().outcome);
    QVERIFY(result.latestStageResult->diagnostic.has_value());
    QCOMPARE(
        result.latestStageResult->diagnostic->code,
        observedEvents.back().diagnostic->code
        );
    QVERIFY(result.latestStageResult->diagnostic->stage.has_value());
    QVERIFY(observedEvents.back().diagnostic->stage.has_value());
    QCOMPARE(
        *result.latestStageResult->diagnostic->stage,
        *observedEvents.back().diagnostic->stage
        );
    QVERIFY(bootstrap.shell() == nullptr);
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::ShellCreated));
    QVERIFY(!containsStage(observedEvents, Application::StartupStage::StartupReady));
    QCOMPARE(application.quitOnLastWindowClosed(), previousQuitPolicy);
    QCOMPARE(application.font(), previousFont);
}

void ApplicationBootstrapTests::
startupCoordinatorAcceptsOnlyOrderedInitialPageFailureBeforeShell()
{
    constexpr std::array stagesBeforeShell{
        Application::StartupStage::ProcessCreated,
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened
    };
    auto prepareBeforeShell = [&stagesBeforeShell](Application::StartupCoordinator& coordinator) {
        if (coordinator.begin().status != Application::StartupStatus::InProgress)
        {
            return false;
        }
        for (const Application::StartupStage stage : stagesBeforeShell)
        {
            const Application::StartupResult result = coordinator.recordStage(
                stage,
                stage == Application::StartupStage::WorkspaceOpened
                    ? Application::StartupStageOutcome::Warning
                    : Application::StartupStageOutcome::Completed
                );
            if (!result.latestStageResult.has_value())
            {
                return false;
            }
        }
        return true;
    };

    Application::StartupCoordinator failureCoordinator;
    QVERIFY(prepareBeforeShell(failureCoordinator));
    const Application::StartupResult pageFailure = failureCoordinator.recordStage(
        Application::StartupStage::InitialPageReady,
        Application::StartupStageOutcome::Failed,
        Application::StartupFailureReason::InitialExperienceUnavailable
        );
    QCOMPARE(pageFailure.status, Application::StartupStatus::Failed);
    QCOMPARE(pageFailure.stageResults.size(), stagesBeforeShell.size() + 1);
    QVERIFY(pageFailure.latestStageResult.has_value());
    QCOMPARE(pageFailure.latestStageResult->stage, Application::StartupStage::InitialPageReady);
    QCOMPARE(pageFailure.latestStageResult->outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(pageFailure.latestStageResult->diagnostic.has_value());
    QCOMPARE(pageFailure.latestStageResult->diagnostic->code,
             Application::StartupDiagnosticCode::InitialExperienceUnavailable);
    const Application::StartupResult shellAfterPageFailure = failureCoordinator.recordStage(
        Application::StartupStage::ShellCreated
        );
    QVERIFY(shellAfterPageFailure.failure.has_value());
    QCOMPARE(*shellAfterPageFailure.failure, Application::StartupFailureReason::InvalidTransition);
    QCOMPARE(shellAfterPageFailure.stageResults.size(), pageFailure.stageResults.size());
    QVERIFY(!shellAfterPageFailure.latestStageResult.has_value());

    for (const Application::StartupStageOutcome outcome : {
             Application::StartupStageOutcome::Completed,
             Application::StartupStageOutcome::Warning
         })
    {
        Application::StartupCoordinator successCoordinator;
        QVERIFY(prepareBeforeShell(successCoordinator));
        const Application::StartupResult pageWithoutShell = successCoordinator.recordStage(
            Application::StartupStage::InitialPageReady,
            outcome
            );
        QCOMPARE(pageWithoutShell.status, Application::StartupStatus::Failed);
        QVERIFY(pageWithoutShell.failure.has_value());
        QCOMPARE(*pageWithoutShell.failure, Application::StartupFailureReason::InvalidTransition);
        QVERIFY(pageWithoutShell.diagnostic.has_value());
        QCOMPARE(pageWithoutShell.diagnostic->code,
                 Application::StartupDiagnosticCode::InvalidTransition);
        QVERIFY(pageWithoutShell.diagnostic->stage.has_value());
        QCOMPARE(*pageWithoutShell.diagnostic->stage,
                 Application::StartupStage::InitialPageReady);
        QCOMPARE(pageWithoutShell.stageResults.size(), stagesBeforeShell.size());
        QVERIFY(!pageWithoutShell.latestStageResult.has_value());
        QCOMPARE(successCoordinator.result().status, Application::StartupStatus::InProgress);
    }
}

void ApplicationBootstrapTests::
startupCoordinatorAcceptsEveryPhaseFiveStageInOrder()
{
    Application::StartupCoordinator coordinator;
    const Application::StartupResult begun = coordinator.begin();
    QCOMPARE(begun.status, Application::StartupStatus::InProgress);
    QVERIFY(begun.stageResults.empty());
    QVERIFY(begun.completedEvents.empty());

    constexpr std::array expectedStages{
        Application::StartupStage::ProcessCreated,
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };

    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        const Application::StartupResult accepted = coordinator.recordStage(
            expectedStages[index]
            );
        QVERIFY(accepted.latestStageResult.has_value());
        QCOMPARE(accepted.latestStageResult->stage, expectedStages[index]);
        QCOMPARE(
            accepted.latestStageResult->outcome,
            Application::StartupStageOutcome::Completed
            );
        QCOMPARE(accepted.stageResults.size(), index + 1);
        QCOMPARE(accepted.completedEvents.size(), index + 1);
        QCOMPARE(accepted.stageResults.back().stage, expectedStages[index]);
        QCOMPARE(accepted.completedEvents.back().stage, expectedStages[index]);
        QVERIFY(!accepted.failure.has_value());
        QVERIFY(!accepted.diagnostic.has_value());

        if (expectedStages[index] == Application::StartupStage::InitialPageReady)
        {
            QCOMPARE(
                accepted.status,
                Application::StartupStatus::InitialExperienceAvailable
                );
            QVERIFY(accepted.succeeded());
        }
        else if (expectedStages[index] == Application::StartupStage::StartupReady)
        {
            QCOMPARE(accepted.status, Application::StartupStatus::StartupReady);
            QVERIFY(accepted.succeeded());
        }
        else
        {
            QCOMPARE(accepted.status, Application::StartupStatus::InProgress);
        }
    }

    const Application::StartupResult finalResult = coordinator.result();
    QCOMPARE(finalResult.stageResults.size(), expectedStages.size());
    QCOMPARE(finalResult.completedEvents.size(), expectedStages.size());
    for (std::size_t index = 0; index < expectedStages.size(); ++index)
    {
        QCOMPARE(finalResult.stageResults[index].stage, expectedStages[index]);
        QCOMPARE(
            finalResult.stageResults[index].outcome,
            Application::StartupStageOutcome::Completed
            );
        QCOMPARE(finalResult.completedEvents[index].stage, expectedStages[index]);
    }
    QVERIFY(!finalResult.latestStageResult.has_value());
}

void ApplicationBootstrapTests::
startupCoordinatorRejectsInvalidTransitionsWithoutRecordingThem()
{
    Application::StartupCoordinator coordinator;

    const Application::StartupResult beforeBegin = coordinator.recordStage(
        Application::StartupStage::ProcessCreated
        );
    QCOMPARE(beforeBegin.status, Application::StartupStatus::Failed);
    QVERIFY(beforeBegin.failure.has_value());
    QCOMPARE(*beforeBegin.failure, Application::StartupFailureReason::InvalidTransition);
    QVERIFY(beforeBegin.stageResults.empty());
    QVERIFY(!beforeBegin.latestStageResult.has_value());
    QCOMPARE(coordinator.result().status, Application::StartupStatus::NotStarted);

    QVERIFY(coordinator.begin().status == Application::StartupStatus::InProgress);

    const Application::StartupResult wrongFirstStage = coordinator.recordStage(
        Application::StartupStage::CoreServicesCreated
        );
    QCOMPARE(wrongFirstStage.status, Application::StartupStatus::Failed);
    QVERIFY(wrongFirstStage.failure.has_value());
    QCOMPARE(*wrongFirstStage.failure, Application::StartupFailureReason::InvalidTransition);
    QVERIFY(wrongFirstStage.diagnostic.has_value());
    QCOMPARE(wrongFirstStage.diagnostic->code, Application::StartupDiagnosticCode::InvalidTransition);
    QVERIFY(wrongFirstStage.diagnostic->stage.has_value());
    QCOMPARE(*wrongFirstStage.diagnostic->stage, Application::StartupStage::CoreServicesCreated);
    QVERIFY(wrongFirstStage.stageResults.empty());
    QVERIFY(!wrongFirstStage.latestStageResult.has_value());
    QCOMPARE(coordinator.result().status, Application::StartupStatus::InProgress);

    const Application::StartupResult invalidStage = coordinator.recordStage(
        static_cast<Application::StartupStage>(255)
        );
    QVERIFY(invalidStage.failure.has_value());
    QCOMPARE(*invalidStage.failure, Application::StartupFailureReason::InvalidTransition);
    QVERIFY(invalidStage.stageResults.empty());
    QVERIFY(!invalidStage.latestStageResult.has_value());

    const Application::StartupResult processCreated = coordinator.recordStage(
        Application::StartupStage::ProcessCreated
        );
    QVERIFY(processCreated.latestStageResult.has_value());

    const Application::StartupResult duplicate = coordinator.recordStage(
        Application::StartupStage::ProcessCreated
        );
    QVERIFY(duplicate.failure.has_value());
    QCOMPARE(*duplicate.failure, Application::StartupFailureReason::DuplicateStage);
    QVERIFY(duplicate.diagnostic.has_value());
    QCOMPARE(duplicate.diagnostic->code, Application::StartupDiagnosticCode::DuplicateStage);
    QCOMPARE(duplicate.stageResults.size(), std::size_t{1});
    QVERIFY(!duplicate.latestStageResult.has_value());

    // Skipping a stage is allowed for compositions that do not own it, but
    // subsequent accepted outcomes must still move strictly forward.
    const Application::StartupResult settingsLoaded = coordinator.recordStage(
        Application::StartupStage::SettingsLoaded
        );
    QVERIFY(settingsLoaded.latestStageResult.has_value());
    QCOMPARE(settingsLoaded.stageResults.size(), std::size_t{2});

    const Application::StartupResult backward = coordinator.recordStage(
        Application::StartupStage::CoreServicesCreated
        );
    QVERIFY(backward.failure.has_value());
    QCOMPARE(*backward.failure, Application::StartupFailureReason::BackwardStage);
    QVERIFY(backward.diagnostic.has_value());
    QCOMPARE(backward.diagnostic->code, Application::StartupDiagnosticCode::BackwardStage);
    QVERIFY(backward.diagnostic->stage.has_value());
    QCOMPARE(*backward.diagnostic->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(backward.stageResults.size(), std::size_t{2});
    QVERIFY(!backward.latestStageResult.has_value());
    QCOMPARE(coordinator.result().stageResults.size(), std::size_t{2});

    Application::StartupCoordinator pageCoordinator;
    QVERIFY(pageCoordinator.begin().status == Application::StartupStatus::InProgress);
    QVERIFY(pageCoordinator.recordStage(Application::StartupStage::ProcessCreated)
                .latestStageResult.has_value());
    const Application::StartupResult pageWithoutShell = pageCoordinator.recordStage(
        Application::StartupStage::InitialPageReady
        );
    QVERIFY(pageWithoutShell.failure.has_value());
    QCOMPARE(*pageWithoutShell.failure, Application::StartupFailureReason::InvalidTransition);
    QCOMPARE(pageWithoutShell.stageResults.size(), std::size_t{1});
    QVERIFY(!pageWithoutShell.latestStageResult.has_value());

    QVERIFY(pageCoordinator.recordStage(Application::StartupStage::ShellCreated)
                .latestStageResult.has_value());
    const Application::StartupResult pageReady = pageCoordinator.recordStage(
        Application::StartupStage::InitialPageReady
        );
    QVERIFY(pageReady.latestStageResult.has_value());
    QCOMPARE(pageReady.status, Application::StartupStatus::InitialExperienceAvailable);

    Application::StartupCoordinator readinessCoordinator;
    QVERIFY(readinessCoordinator.begin().status == Application::StartupStatus::InProgress);
    QVERIFY(readinessCoordinator.recordStage(Application::StartupStage::ProcessCreated)
                .latestStageResult.has_value());
    const Application::StartupResult prematureReady = readinessCoordinator.recordStage(
        Application::StartupStage::StartupReady
        );
    QVERIFY(prematureReady.failure.has_value());
    QCOMPARE(
        *prematureReady.failure,
        Application::StartupFailureReason::StartupReadyPrerequisitesIncomplete
        );
    QVERIFY(prematureReady.diagnostic.has_value());
    QCOMPARE(
        prematureReady.diagnostic->code,
        Application::StartupDiagnosticCode::StartupReadyPrerequisitesIncomplete
        );
    QCOMPARE(prematureReady.stageResults.size(), std::size_t{1});
    QVERIFY(!prematureReady.latestStageResult.has_value());
    QCOMPARE(readinessCoordinator.result().status, Application::StartupStatus::InProgress);

    constexpr std::array remainingStages{
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };
    Application::StartupResult recovered;
    for (const Application::StartupStage stage : remainingStages)
    {
        recovered = readinessCoordinator.recordStage(stage);
        QVERIFY(recovered.latestStageResult.has_value());
    }
    QCOMPARE(recovered.status, Application::StartupStatus::StartupReady);
    QCOMPARE(recovered.stageResults.size(), std::size_t{10});
}

void ApplicationBootstrapTests::
startupCoordinatorRetainsTypedWarningAndTerminalFailure()
{
    Application::StartupCoordinator warningCoordinator;
    QVERIFY(warningCoordinator.begin().status == Application::StartupStatus::InProgress);
    QVERIFY(warningCoordinator.recordStage(Application::StartupStage::ProcessCreated)
                .latestStageResult.has_value());

    const Application::StartupResult warning = warningCoordinator.recordStage(
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStageOutcome::Warning
        );
    QCOMPARE(warning.status, Application::StartupStatus::InProgress);
    QVERIFY(warning.latestStageResult.has_value());
    QCOMPARE(warning.latestStageResult->outcome, Application::StartupStageOutcome::Warning);
    QVERIFY(warning.latestStageResult->diagnostic.has_value());
    QCOMPARE(warning.latestStageResult->diagnostic->code, Application::StartupDiagnosticCode::StageWarning);
    QVERIFY(warning.latestStageResult->diagnostic->stage.has_value());
    QCOMPARE(*warning.latestStageResult->diagnostic->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(warning.stageResults.size(), std::size_t{2});
    QCOMPARE(warning.completedEvents.size(), std::size_t{1});
    QCOMPARE(warning.stageResults.back().outcome, Application::StartupStageOutcome::Warning);
    QVERIFY(warning.diagnostic.has_value());
    QCOMPARE(warning.diagnostic->code, Application::StartupDiagnosticCode::StageWarning);
    QVERIFY(!warning.failure.has_value());

    constexpr std::array remainingStages{
        Application::StartupStage::SettingsLoaded,
        Application::StartupStage::LanguageLoaded,
        Application::StartupStage::ThemeLoaded,
        Application::StartupStage::RequiredFontsLoaded,
        Application::StartupStage::WorkspaceOpened,
        Application::StartupStage::ShellCreated,
        Application::StartupStage::InitialPageReady,
        Application::StartupStage::StartupReady
    };
    Application::StartupResult afterWarning;
    for (const Application::StartupStage stage : remainingStages)
    {
        afterWarning = warningCoordinator.recordStage(stage);
        QVERIFY(afterWarning.latestStageResult.has_value());
    }
    QCOMPARE(afterWarning.status, Application::StartupStatus::StartupReady);
    QVERIFY(afterWarning.succeeded());
    QCOMPARE(afterWarning.stageResults.size(), std::size_t{10});
    QCOMPARE(afterWarning.completedEvents.size(), std::size_t{9});
    QCOMPARE(afterWarning.stageResults[1].outcome, Application::StartupStageOutcome::Warning);
    QCOMPARE(afterWarning.completedEvents[1].stage, Application::StartupStage::SettingsLoaded);

    Application::StartupCoordinator failureCoordinator;
    QVERIFY(failureCoordinator.begin().status == Application::StartupStatus::InProgress);
    QVERIFY(failureCoordinator.recordStage(Application::StartupStage::ProcessCreated)
                .latestStageResult.has_value());
    const Application::StartupResult stageFailure = failureCoordinator.recordStage(
        Application::StartupStage::CoreServicesCreated,
        Application::StartupStageOutcome::Failed,
        Application::StartupFailureReason::StageFailed
        );
    QCOMPARE(stageFailure.status, Application::StartupStatus::Failed);
    QVERIFY(!stageFailure.succeeded());
    QVERIFY(stageFailure.failure.has_value());
    QCOMPARE(*stageFailure.failure, Application::StartupFailureReason::StageFailed);
    QCOMPARE(stageFailure.stageResults.size(), std::size_t{2});
    QCOMPARE(stageFailure.completedEvents.size(), std::size_t{1});
    QVERIFY(stageFailure.latestStageResult.has_value());
    QCOMPARE(stageFailure.latestStageResult->stage, Application::StartupStage::CoreServicesCreated);
    QCOMPARE(stageFailure.latestStageResult->outcome, Application::StartupStageOutcome::Failed);
    QVERIFY(stageFailure.latestStageResult->diagnostic.has_value());
    QCOMPARE(stageFailure.latestStageResult->diagnostic->code, Application::StartupDiagnosticCode::StageFailed);
    QVERIFY(stageFailure.latestStageResult->diagnostic->stage.has_value());
    QCOMPARE(*stageFailure.latestStageResult->diagnostic->stage, Application::StartupStage::CoreServicesCreated);

    const Application::StartupResult afterTerminalFailure = failureCoordinator.recordStage(
        Application::StartupStage::SettingsLoaded
        );
    QCOMPARE(afterTerminalFailure.status, Application::StartupStatus::Failed);
    QVERIFY(afterTerminalFailure.failure.has_value());
    QCOMPARE(*afterTerminalFailure.failure, Application::StartupFailureReason::InvalidTransition);
    QCOMPARE(afterTerminalFailure.stageResults.size(), std::size_t{2});
    QVERIFY(!afterTerminalFailure.latestStageResult.has_value());
    const Application::StartupResult terminalState = failureCoordinator.result();
    QCOMPARE(terminalState.status, Application::StartupStatus::Failed);
    QVERIFY(terminalState.failure.has_value());
    QCOMPARE(*terminalState.failure, Application::StartupFailureReason::StageFailed);
    QCOMPARE(terminalState.stageResults.size(), std::size_t{2});
}

void ApplicationBootstrapTests::
closeRequestsShutdownOnceAndRestoresApplicationPolicy()
{
    QApplication& application = *qApp;
    const bool previousQuitPolicy = application.quitOnLastWindowClosed();
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));
    {
        Bootstrap::ApplicationBootstrap bootstrap(
            application,
            {},
            {},
            resolverForRoot(resources.resourceRoot())
            );
        const Application::StartupResult startup = bootstrap.start();
        QVERIFY(startup.succeeded());
        QVERIFY(!bootstrap.shutdownRequested());
        QVERIFY(!application.quitOnLastWindowClosed());

        QPointer<Ui::ApplicationShell> shell = bootstrap.shell();
        QVERIFY(shell);
        QVERIFY(shell->close());

        QTRY_VERIFY(bootstrap.shutdownRequested());
        QTRY_VERIFY(!shell);

        const Application::ShutdownResult repeated = bootstrap.shutdown();
        QVERIFY(repeated.shutdownRequested);
        QVERIFY(!repeated.newlyRequested);

        const Application::ShutdownResult repeatedAgain = bootstrap.shutdown();
        QVERIFY(repeatedAgain.shutdownRequested);
        QVERIFY(!repeatedAgain.newlyRequested);
    }

    QCOMPARE(application.quitOnLastWindowClosed(), previousQuitPolicy);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
shutdownRetriesSettingsAndWorkspaceFailuresBeforeReleasingOwners()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    std::vector<std::string> shutdownEvents;
    auto workspaceProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    workspaceProbe->shutdownEvents = &shutdownEvents;
    auto updateProbe = std::make_shared<StartupUpdateDispatcherProbe>();
    updateProbe->shutdownEvents = &shutdownEvents;
    updateProbe->holdCompletionUntilShutdown = true;

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams workspaceSeams;
    workspaceSeams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), QStringLiteral("existing-workspace.tps")
    };
    workspaceSeams.recentAutoOpenEnabled = false;
    workspaceSeams.pathExists = [](const Application::WorkspaceLocation&) {
        return true;
    };
    workspaceSeams.workspaceGatewayFactory = [workspaceProbe] {
        ++workspaceProbe->factoryCalls;
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(workspaceProbe);
    };

    bool settingsFlushSucceeds = false;
    QPointer<Ui::ApplicationShell> shell;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(workspaceSeams),
        [&application, &shell, updateProbe] {
            ++updateProbe->factoryCalls;
            return std::make_unique<RecordingStartupUpdateDispatcher>(
                application, shell, updateProbe
                );
        },
        Bootstrap::ApplicationBootstrap::ShutdownSeams{
            [&shutdownEvents, &settingsFlushSucceeds] {
                shutdownEvents.push_back("settings-flush");
                return settingsFlushSucceeds;
            }
        }
        );

    const Application::StartupResult startup = bootstrap.start();
    QVERIFY(startup.succeeded());
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    shell = bootstrap.shell();
    QVERIFY(shell);
    QCOMPARE(shell->navigationModel()->initialPageId()->value(),
             QStringLiteral("my_workspace"));
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    QPointer<Ui::Page> initialPage =
        shell->pageHost().pageInstance(QStringLiteral("my_workspace"));
    QVERIFY(initialPage);
    QCoreApplication::processEvents();
    QCOMPARE(updateProbe->dispatchCalls, 1);
    QVERIFY(static_cast<bool>(updateProbe->pendingCompletion));
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());

    // A settings flush failure rejects close before cancelling the pending
    // updater or attempting to close the workspace.
    QVERIFY(!shell->close());
    QVERIFY(shell);
    QVERIFY(shell->isVisible());
    QVERIFY(!bootstrap.shutdownRequested());
    QVERIFY(initialPage);
    QCOMPARE(updateProbe->cancelCalls, 0);
    QCOMPARE(workspaceProbe->closeCalls, 0);
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());

    settingsFlushSucceeds = true;
    workspaceProbe->closeError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected workspace close failure",
        .recoverable = true
    };

    // The update is cancelled before workspace close is attempted. A failed
    // close keeps both shell and runtime/session alive for another attempt.
    QVERIFY(!shell->close());
    QVERIFY(shell);
    QVERIFY(shell->isVisible());
    QVERIFY(!bootstrap.shutdownRequested());
    QVERIFY(initialPage);
    QCOMPARE(updateProbe->cancelCalls, 1);
    QCOMPARE(updateProbe->destroyed, 1);
    QCOMPARE(workspaceProbe->closeCalls, 1);
    QCOMPARE(workspaceProbe->destroyed, 0);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());

    workspaceProbe->closeError.reset();
    QVERIFY(shell->close());
    QTRY_VERIFY(bootstrap.shutdownRequested());
    QTRY_VERIFY(!shell);
    QTRY_VERIFY(!initialPage);
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());

    // The pending completion now runs only its expired-lifetime branch. It
    // cannot touch a page or any other owner released by shutdown.
    updateProbe->pendingCompletion();
    QCOMPARE(updateProbe->asynchronousCompletions, 0);
    QCOMPARE(updateProbe->suppressedCompletions, 1);

    const auto eventIndex = [&shutdownEvents](const std::string& event) {
        const auto found = std::find(
            shutdownEvents.begin(), shutdownEvents.end(), event
            );
        return found == shutdownEvents.end()
            ? shutdownEvents.size()
            : static_cast<std::size_t>(std::distance(shutdownEvents.begin(), found));
    };
    QVERIFY(eventIndex("settings-flush") < eventIndex("update-cancel"));
    QVERIFY(eventIndex("update-cancel") < eventIndex("update-destroy"));
    QVERIFY(eventIndex("update-destroy") < eventIndex("workspace-close"));
    QVERIFY(eventIndex("workspace-close") < eventIndex("workspace-destroy"));
    QCOMPARE(updateProbe->cancelCalls, 1);
    QCOMPARE(updateProbe->destroyed, 1);
    QCOMPARE(workspaceProbe->destroyed, 1);

    const Application::ShutdownResult repeated = bootstrap.shutdown();
    QVERIFY(repeated.shutdownRequested);
    QVERIFY(!repeated.newlyRequested);
    QCOMPARE(updateProbe->cancelCalls, 1);
    QCOMPARE(workspaceProbe->closeCalls, 2);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
startupFailureAndShutdownActionAreReportedPrecisely()
{
    Application::StartupCoordinator startupCoordinator;
    const Application::StartupResult inProgress = startupCoordinator.begin();
    QCOMPARE(inProgress.status, Application::StartupStatus::InProgress);
    QVERIFY(inProgress.stageResults.empty());
    QVERIFY(inProgress.completedEvents.empty());
    QVERIFY(!inProgress.failure.has_value());

    const Application::StartupResult failed = startupCoordinator.fail(
        Application::StartupFailureReason::InitialExperienceUnavailable
        );
    QCOMPARE(failed.status, Application::StartupStatus::Failed);
    QVERIFY(!failed.succeeded());
    QVERIFY(failed.stageResults.empty());
    QVERIFY(failed.completedEvents.empty());
    QVERIFY(failed.failure.has_value());
    QCOMPARE(
        *failed.failure,
        Application::StartupFailureReason::InitialExperienceUnavailable
        );
    QVERIFY(failed.diagnostic.has_value());
    QCOMPARE(
        failed.diagnostic->code,
        Application::StartupDiagnosticCode::InitialExperienceUnavailable
        );

    const Application::StartupResult afterFailure = startupCoordinator.recordStage(
        Application::StartupStage::ProcessCreated
        );
    QCOMPARE(afterFailure.status, Application::StartupStatus::Failed);
    QVERIFY(afterFailure.failure.has_value());
    QCOMPARE(*afterFailure.failure, Application::StartupFailureReason::InvalidTransition);
    QVERIFY(afterFailure.stageResults.empty());
    QVERIFY(!afterFailure.latestStageResult.has_value());
    const Application::StartupResult failureState = startupCoordinator.result();
    QVERIFY(failureState.failure.has_value());
    QCOMPARE(*failureState.failure, Application::StartupFailureReason::InitialExperienceUnavailable);

    int shutdownActionCount = 0;
    Application::ShutdownCoordinator shutdownCoordinator(
        [&shutdownActionCount]()
            -> std::optional<Application::ShutdownFailureReason> {
            ++shutdownActionCount;
            if (shutdownActionCount == 1)
            {
                return std::optional<Application::ShutdownFailureReason>(
                    Application::ShutdownFailureReason::WorkspaceCloseFailed
                    );
            }
            return std::nullopt;
        }
        );
    const Application::ShutdownResult first =
        shutdownCoordinator.requestShutdown();
    QVERIFY(!first.shutdownRequested);
    QVERIFY(!first.newlyRequested);
    QVERIFY(first.failure.has_value());
    QCOMPARE(*first.failure, Application::ShutdownFailureReason::WorkspaceCloseFailed);
    QCOMPARE(shutdownActionCount, 1);

    const Application::ShutdownResult second =
        shutdownCoordinator.requestShutdown();
    QVERIFY(second.shutdownRequested);
    QVERIFY(second.newlyRequested);
    QVERIFY(!second.failure.has_value());
    QCOMPARE(shutdownActionCount, 2);

    const Application::ShutdownResult repeated =
        shutdownCoordinator.requestShutdown();
    QVERIFY(repeated.shutdownRequested);
    QVERIFY(!repeated.newlyRequested);
    QCOMPARE(shutdownActionCount, 2);
}

void ApplicationBootstrapTests::
startupWorkspacePolicyPreservesLegacyCandidateOrder()
{
    const Application::WorkspaceLocation commandLine("command-line.db");
    const Application::WorkspaceLocation firstRecent("first-recent.tps");
    const Application::WorkspaceLocation secondRecent("second-recent.db");
    const Application::WorkspaceLocation lastFile("legacy-last.tps");
    Application::RecentWorkspaceHistory history;
    history.paths = {firstRecent, secondRecent};
    history.lastPath = lastFile;

    const Application::StartupWorkspaceSelection commandLineSelection =
        Application::StartupWorkspacePolicy::select(
            commandLine,
            false,
            history
            );
    const auto* const commandLineCandidate =
        std::get_if<Application::StartupWorkspaceCandidate>(&commandLineSelection);
    QVERIFY(commandLineCandidate != nullptr);
    QCOMPARE(commandLineCandidate->location.value(), commandLine.value());
    QCOMPARE(
        commandLineCandidate->source,
        Application::StartupWorkspaceSource::CommandLine
        );

    const Application::StartupWorkspaceSelection recentSelection =
        Application::StartupWorkspacePolicy::select(
            std::nullopt,
            true,
            history
            );
    const auto* const recentCandidate =
        std::get_if<Application::StartupWorkspaceCandidate>(&recentSelection);
    QVERIFY(recentCandidate != nullptr);
    QCOMPARE(recentCandidate->location.value(), firstRecent.value());
    QCOMPARE(recentCandidate->source, Application::StartupWorkspaceSource::Recent);

    history.paths.clear();
    const Application::StartupWorkspaceSelection lastFileSelection =
        Application::StartupWorkspacePolicy::select(
            std::nullopt,
            true,
            history
            );
    const auto* const lastFileCandidate =
        std::get_if<Application::StartupWorkspaceCandidate>(&lastFileSelection);
    QVERIFY(lastFileCandidate != nullptr);
    QCOMPARE(lastFileCandidate->location.value(), lastFile.value());
    QCOMPARE(lastFileCandidate->source, Application::StartupWorkspaceSource::LastFile);

    const Application::StartupWorkspaceSelection disabledSelection =
        Application::StartupWorkspacePolicy::select(
            std::nullopt,
            false,
            history
            );
    const auto* const disabledOutcome =
        std::get_if<Application::StartupNoWorkspaceCandidate>(&disabledSelection);
    QVERIFY(disabledOutcome != nullptr);
    QCOMPARE(
        disabledOutcome->reason,
        Application::StartupNoWorkspaceReason::RecentAutoOpenDisabled
        );
}

void ApplicationBootstrapTests::
noWorkspaceRemainsAnExplicitNonFatalWarning()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));

    std::vector<Application::StartupEvent> observedEvents;
    int historyFactoryCalls = 0;
    int gatewayFactoryCalls = 0;
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    const QStringList connectionsBefore = QSqlDatabase::connectionNames();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.recentWorkspaceHistoryPortFactory = [&historyFactoryCalls] {
        ++historyFactoryCalls;
        return std::unique_ptr<Application::RecentWorkspaceHistoryPort>{};
    };
    seams.workspaceGatewayFactory = [&gatewayFactoryCalls, gatewayProbe] {
        ++gatewayFactoryCalls;
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    const Application::StartupResult result = bootstrap.start();

    verifyWorkspaceStartupOrder(
        result,
        observedEvents,
        bootstrap,
        Application::StartupStageOutcome::Warning
        );
    QVERIFY(bootstrap.startupWorkspaceOutcome().has_value());
    const auto* const notOpened = std::get_if<Application::StartupWorkspaceNotOpened>(
        &bootstrap.startupWorkspaceOutcome().value()
        );
    QVERIFY(notOpened != nullptr);
    QCOMPARE(
        notOpened->reason,
        Application::StartupNoWorkspaceReason::RecentAutoOpenDisabled
        );
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);
    QCOMPARE(historyFactoryCalls, 0);
    QCOMPARE(gatewayFactoryCalls, 1);
    QCOMPARE(gatewayProbe->constructed, 1);
    QCOMPARE(gatewayProbe->openCalls, 0);
    QCOMPARE(gatewayProbe->closeCalls, 0);
    QCOMPARE(QSqlDatabase::connectionNames(), connectionsBefore);
    const Application::ShutdownResult shutdown = bootstrap.shutdown();
    QVERIFY(shutdown.shutdownRequested);
    QCOMPARE(gatewayProbe->destroyed, 1);
    QCOMPARE(gatewayProbe->events,
             (std::vector<std::string>{"constructed", "destroyed"}));

    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
staleFirstRecentPathIsPrunedWithoutTryingLaterCandidates()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));

    const QString stalePath = QStringLiteral("stale-first-profile");
    const Application::WorkspaceLocation secondRecent("second-good-profile.db");
    const Application::WorkspaceLocation lastFile("legacy-last-profile.tps");
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    historyProbe->history.paths = {
        Application::WorkspaceLocation(stalePath.toUtf8().toStdString()),
        secondRecent
    };
    historyProbe->history.lastPath = lastFile;

    int gatewayFactoryCalls = 0;
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    std::vector<std::string> checkedPaths;
    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = true;
    seams.pathExists = [&checkedPaths](const Application::WorkspaceLocation& location) {
        checkedPaths.push_back(location.value());
        return false;
    };
    seams.workspaceGatewayFactory = [&gatewayFactoryCalls, gatewayProbe] {
        ++gatewayFactoryCalls;
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    const Application::StartupResult result = bootstrap.start();

    verifyWorkspaceStartupOrder(
        result,
        observedEvents,
        bootstrap,
        Application::StartupStageOutcome::Warning
        );
    QVERIFY(bootstrap.startupWorkspaceOutcome().has_value());
    const auto* const notOpened = std::get_if<Application::StartupWorkspaceNotOpened>(
        &bootstrap.startupWorkspaceOutcome().value()
        );
    QVERIFY(notOpened != nullptr);
    QCOMPARE(notOpened->reason, Application::StartupNoWorkspaceReason::StaleRecentPathPruned);
    QVERIFY(!notOpened->historyError.has_value());
    QCOMPARE(checkedPaths.size(), std::size_t{1});
    QCOMPARE(
        checkedPaths.front(),
        QFileInfo(stalePath + QStringLiteral(".tps")).absoluteFilePath().toUtf8().toStdString()
        );
    QCOMPARE(historyProbe->loadCount, 1);
    QCOMPARE(historyProbe->saves.size(), std::size_t{1});
    QCOMPARE(historyProbe->saves.front().paths.size(), std::size_t{1});
    QCOMPARE(
        historyProbe->saves.front().paths.front().value(),
        secondRecent.value()
        );
    QVERIFY(historyProbe->saves.front().lastPath.has_value());
    QCOMPARE(historyProbe->saves.front().lastPath->value(), lastFile.value());
    QCOMPARE(gatewayFactoryCalls, 1);
    QCOMPARE(gatewayProbe->constructed, 1);
    QCOMPARE(gatewayProbe->openCalls, 0);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);

    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
interruptedSetupOffersRecoveryBeforeAutoOpenOrRecentHistory()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("interrupted.tps")
        );
    const QString backupPath = temporaryDirectory.filePath(
        QStringLiteral(".interrupted.initial-setup-backup")
        );
    const QString journalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    const auto writeFile = [](const QString& path, const QByteArray& contents) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(contents) == contents.size();
    };
    QVERIFY(writeFile(profilePath, QByteArrayLiteral("incomplete profile")));
    QVERIFY(writeFile(backupPath, QByteArrayLiteral("original profile")));
    QSaveFile recoveryJournal(journalPath);
    QVERIFY(recoveryJournal.open(QIODevice::WriteOnly));
    const QByteArray recoveryPayload = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("profile"), QFileInfo(profilePath).absoluteFilePath()},
        {QStringLiteral("originalBackup"), QFileInfo(backupPath).absoluteFilePath()}
    }).toJson(QJsonDocument::Compact);
    QCOMPARE(recoveryJournal.write(recoveryPayload), qint64(recoveryPayload.size()));
    QVERIFY(recoveryJournal.commit());

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    historyProbe->history.paths = {workspaceLocation(profilePath)};
    int historyFactoryCalls = 0;
    int gatewayFactoryCalls = 0;
    int pathExistsCalls = 0;
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), profilePath
    };
    seams.recentAutoOpenEnabled = true;
    seams.initialSetupRecoveryJournalPath = journalPath;
    seams.pathExists = [&pathExistsCalls](const Application::WorkspaceLocation&) {
        ++pathExistsCalls;
        return true;
    };
    seams.recentWorkspaceHistoryPortFactory = [&] {
        ++historyFactoryCalls;
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };
    seams.workspaceGatewayFactory = [&] {
        ++gatewayFactoryCalls;
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    const Application::StartupResult result = bootstrap.start();
    QVERIFY(result.succeeded());
    QCOMPARE(result.stageResults[6].outcome, Application::StartupStageOutcome::Warning);
    const auto* const notOpened = std::get_if<Application::StartupWorkspaceNotOpened>(
        &bootstrap.startupWorkspaceOutcome().value()
        );
    QVERIFY(notOpened);
    QCOMPARE(notOpened->reason, Application::StartupNoWorkspaceReason::NoCandidate);
    QCOMPARE(historyFactoryCalls, 0);
    QCOMPARE(historyProbe->loadCount, 0);
    QVERIFY(historyProbe->saves.empty());
    QCOMPARE(pathExistsCalls, 0);
    QCOMPARE(gatewayFactoryCalls, 1);
    QCOMPARE(gatewayProbe->openCalls, 0);

    auto* const setupPage = dynamic_cast<Ui::InitialPage*>(
        bootstrap.shell()->pageHost().pageInstance(QStringLiteral("initial_setup"))
        );
    QVERIFY(setupPage);
    auto* const resumeButton = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupResume")
        );
    auto* const restoreButton = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupRestore")
        );
    QVERIFY(resumeButton);
    QVERIFY(restoreButton);
    QVERIFY(!resumeButton->isHidden());
    QVERIFY(!restoreButton->isHidden());

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    QCOMPARE(gatewayProbe->destroyed, 1);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
createActionUsesSelectedPathAndDoesNotRecordPendingProfile()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString selectedProfilePath = temporaryDirectory.filePath(
        QStringLiteral("selected-from-setup-action.tps")
        );
    const QString selectedSignatureImagePath = temporaryDirectory.filePath(
        QStringLiteral("selected-signature.png")
        );
    QImage signatureSource(4, 2, QImage::Format_RGB32);
    signatureSource.fill(Qt::white);
    signatureSource.setPixelColor(1, 0, Qt::black);
    signatureSource.setPixelColor(2, 1, Qt::black);
    QVERIFY(signatureSource.save(selectedSignatureImagePath, "PNG"));
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        selectedProfilePath.toUtf8().toStdString()
        );
    fileDialogProbe->openPath = Application::FileDialogPath(
        selectedSignatureImagePath.toUtf8().toStdString()
        );

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = true;
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };
    seams.initialSetupFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(fileDialogProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    QCOMPARE(historyProbe->loadCount, 1);
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(historyProbe->history.paths.empty());
    QVERIFY(historyProbe->saves.empty());

    auto* const setupPage = dynamic_cast<Ui::InitialPage*>(
        bootstrap.shell()->pageHost().pageInstance(QStringLiteral("initial_setup"))
        );
    QVERIFY(setupPage);
    auto* const createButton = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    QVERIFY(createButton);
    QVERIFY(!createButton->isHidden());
    createButton->click();

    QCOMPARE(fileDialogProbe->saveCount, 1);
    QVERIFY(fileDialogProbe->lastSaveRequest.has_value());
    const Application::SaveFileDialogRequest& request =
        *fileDialogProbe->lastSaveRequest;
    QCOMPARE(request.purpose, Application::FileDialogPurpose::General);
    QCOMPARE(request.title, std::string("Choose a profile destination"));
    QCOMPARE(request.suggestedFileName, std::string("ClassMngr.tps"));
    QCOMPARE(request.nameFilters, std::vector<std::string>{
        "ClassMngr profiles (*.tps)"
    });
    QCOMPARE(request.defaultSuffix, std::string("tps"));
    QVERIFY(request.confirmOverwrite);
    QCOMPARE(
        fileDialogProbe->selectedPath.value(),
        selectedProfilePath.toUtf8().toStdString()
        );
    QVERIFY(QFileInfo::exists(selectedProfilePath));
    QVERIFY(QFileInfo(selectedProfilePath).size() > 0);

    auto* const statusLabel = setupPage->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupStatus")
        );
    auto* const profileLabel = setupPage->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupProfileLocation")
        );
    QVERIFY(statusLabel);
    QVERIFY(profileLabel);
    QCOMPARE(statusLabel->text(), QStringLiteral("Profile created. Setup is pending."));
    QCOMPARE(
        profileLabel->text(),
        QStringLiteral("Profile: %1").arg(QFileInfo(selectedProfilePath).absoluteFilePath())
        );

    QAction* const closeProfileAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextCloseWorkspace")
        );
    QVERIFY(closeProfileAction);
    QFile journalBeforeClose(recoveryJournalPath);
    QVERIFY(journalBeforeClose.open(QIODevice::ReadOnly));
    const QByteArray pendingRecoveryJournal = journalBeforeClose.readAll();
    QVERIFY(!pendingRecoveryJournal.isEmpty());
    const Application::WorkspaceStateSnapshot activeSetupWorkspace =
        bootstrap.workspaceStateSnapshot();
    QVERIFY(activeSetupWorkspace.session().has_value());
    QCOMPARE(activeSetupWorkspace.lifecycle(),
             Application::WorkspaceLifecycleState::Open);

    // Exercise the handler guard independently of action enablement policy.
    closeProfileAction->setEnabled(true);
    closeProfileAction->trigger();

    const Application::WorkspaceStateSnapshot afterRejectedClose =
        bootstrap.workspaceStateSnapshot();
    QVERIFY(afterRejectedClose.session().has_value());
    QCOMPARE(afterRejectedClose.lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    QCOMPARE(afterRejectedClose.session()->location(),
             activeSetupWorkspace.session()->location());
    QCOMPARE(statusLabel->text(), QStringLiteral("Profile created. Setup is pending."));
    QFile journalAfterClose(recoveryJournalPath);
    QVERIFY(journalAfterClose.open(QIODevice::ReadOnly));
    QCOMPARE(journalAfterClose.readAll(), pendingRecoveryJournal);
    QVERIFY(bootstrap.shell()->statusBar()->currentMessage().contains(
        QStringLiteral("Resolve the pending setup or recovery state")
        ));

    auto* const personalName = setupPage->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupPersonalName")
        );
    auto* const typedMode = setupPage->findChild<QRadioButton*>(
        QStringLiteral("classMngrNextInitialSetupSignatureTypeMode")
        );
    auto* const imageMode = setupPage->findChild<QRadioButton*>(
        QStringLiteral("classMngrNextInitialSetupSignatureImageMode")
        );
    auto* const chooseSignature = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupChooseSignature")
        );
    auto* const removeSignature = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupRemoveSignature")
        );
    auto* const signaturePreview = setupPage->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupSignaturePreview")
        );
    auto* const typedSignature = setupPage->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTypedSignature")
        );
    auto* const signatureFont = setupPage->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupSignatureFont")
        );
    auto* const savePersonalDetails = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsSave")
        );
    auto* const personalDetailsStatus = setupPage->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsStatus")
        );
    auto* const retryPersonalDetails = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsRetry")
        );
    auto* const cancelSetup = setupPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );
    QVERIFY(personalName);
    QVERIFY(typedMode);
    QVERIFY(imageMode);
    QVERIFY(chooseSignature);
    QVERIFY(removeSignature);
    QVERIFY(signaturePreview);
    QVERIFY(typedSignature);
    QVERIFY(signatureFont);
    QVERIFY(savePersonalDetails);
    QVERIFY(personalDetailsStatus);
    QVERIFY(retryPersonalDetails);
    QVERIFY(cancelSetup);

    savePersonalDetails->click();
    QCOMPARE(
        personalDetailsStatus->text(),
        QStringLiteral("Enter your name before saving.")
        );

    const QString savedName = QString::fromUtf8("김선생님 / 🧭");
    const QString savedSignature = QString::fromUtf8("김선생님");
    personalName->setText(savedName);
    QVERIFY(imageMode->isChecked());
    chooseSignature->click();
    QCOMPARE(fileDialogProbe->openCount, 1);
    QVERIFY(fileDialogProbe->lastOpenRequest.has_value());
    QCOMPARE(
        fileDialogProbe->lastOpenRequest->purpose,
        Application::FileDialogPurpose::SignatureImage
        );
    QVERIFY(!signaturePreview->pixmap().isNull());
    QVERIFY(removeSignature->isEnabled());
    removeSignature->click();
    QCOMPARE(
        signaturePreview->text(),
        QStringLiteral("No signature image added")
        );
    QVERIFY(!removeSignature->isEnabled());
    chooseSignature->click();
    QCOMPARE(fileDialogProbe->openCount, 2);
    QVERIFY(!signaturePreview->pixmap().isNull());
    QVERIFY(removeSignature->isEnabled());

    typedMode->click();
    typedSignature->setText(savedSignature);
    QVERIFY(!signaturePreview->pixmap().isNull());
    signatureFont->setCurrentIndex(2);
    QVERIFY(executeWorkspaceSql(
        selectedProfilePath,
        QStringLiteral(
            "ALTER TABLE app_settings RENAME TO app_settings_read_failure"
            )
        ));
    retryPersonalDetails->click();
    QVERIFY(personalDetailsStatus->text().startsWith(
        QStringLiteral(
            "Personal details could not be loaded. Your form is unchanged; retry to load saved values."
            )
        ));
    QVERIFY(!retryPersonalDetails->isHidden());
    QCOMPARE(personalName->text(), savedName);
    QCOMPARE(typedSignature->text(), savedSignature);
    QVERIFY(typedMode->isChecked());
    QCOMPARE(signatureFont->currentIndex(), 2);
    QVERIFY(!cancelSetup->isHidden());

    QVERIFY(executeWorkspaceSql(
        selectedProfilePath,
        QStringLiteral(
            "ALTER TABLE app_settings_read_failure RENAME TO app_settings"
            )
        ));
    retryPersonalDetails->click();
    QVERIFY(personalDetailsStatus->text().isEmpty());
    QVERIFY(retryPersonalDetails->isHidden());
    QCOMPARE(personalName->text(), savedName);
    QCOMPARE(typedSignature->text(), savedSignature);
    QVERIFY(typedMode->isChecked());
    QCOMPARE(signatureFont->currentIndex(), 2);

    QVERIFY(executeWorkspaceSql(
        selectedProfilePath,
        QStringLiteral(R"(
            CREATE TRIGGER fail_initial_setup_ui_save
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/typedSignatureText'
            BEGIN
                SELECT RAISE(ABORT, 'forced setup details failure');
            END
        )")
        ));

    savePersonalDetails->click();
    QVERIFY(personalDetailsStatus->text().startsWith(
        QStringLiteral(
            "Personal details could not be saved. Setup remains pending; retry to save."
            )
        ));
    QCOMPARE(personalName->text(), savedName);
    QCOMPARE(typedSignature->text(), savedSignature);
    QVERIFY(typedMode->isChecked());
    QCOMPARE(signatureFont->currentIndex(), 2);
    QVERIFY(!cancelSetup->isHidden());

    QVERIFY(executeWorkspaceSql(
        selectedProfilePath,
        QStringLiteral("DROP TRIGGER fail_initial_setup_ui_save")
        ));
    savePersonalDetails->click();
    QCOMPARE(
        personalDetailsStatus->text(),
        QStringLiteral("Personal details saved. Setup is still pending.")
        );
    QVERIFY(!cancelSetup->isHidden());

    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(historyProbe->saves.empty());
    QVERIFY(historyProbe->history.paths.empty());
    const std::string selectedWorkspacePath = selectedProfilePath.toUtf8().toStdString();
    for (const Application::RecentWorkspaceHistory& savedHistory : historyProbe->saves)
    {
        QVERIFY(std::none_of(
            savedHistory.paths.begin(),
            savedHistory.paths.end(),
            [&selectedWorkspacePath](const Application::WorkspaceLocation& location) {
                return location.value() == selectedWorkspacePath;
            }
            ));
        QVERIFY(!savedHistory.lastPath.has_value()
            || savedHistory.lastPath->value() != selectedWorkspacePath);
    }

    QVERIFY(bootstrap.shutdown().shutdownRequested);

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams resumeSeams;
    resumeSeams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    resumeSeams.recentAutoOpenEnabled = false;
    resumeSeams.initialSetupRecoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    Bootstrap::ApplicationBootstrap resumedBootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(resumeSeams)
        );
    QVERIFY(resumedBootstrap.start().succeeded());
    auto* const resumedPage = dynamic_cast<Ui::InitialPage*>(
        resumedBootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("initial_setup")
            )
        );
    QVERIFY(resumedPage);
    auto* const resumeButton = resumedPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupResume")
        );
    QVERIFY(resumeButton);
    QVERIFY(!resumeButton->isHidden());
    resumeButton->click();

    auto* const resumedName = resumedPage->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupPersonalName")
        );
    auto* const resumedSignature = resumedPage->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTypedSignature")
        );
    auto* const resumedMode = resumedPage->findChild<QRadioButton*>(
        QStringLiteral("classMngrNextInitialSetupSignatureTypeMode")
        );
    auto* const resumedCancel = resumedPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );
    QVERIFY(resumedName);
    QVERIFY(resumedSignature);
    QVERIFY(resumedMode);
    QVERIFY(resumedCancel);
    QCOMPARE(resumedName->text(), savedName);
    QCOMPARE(resumedSignature->text(), savedSignature);
    QVERIFY(resumedMode->isChecked());
    QVERIFY(!resumedCancel->isHidden());
    QVERIFY(resumedBootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
openActionUpdatesWorkspaceNavigationAndHistoryOnlyAfterSuccess()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString journalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    const QString firstPath = temporaryDirectory.filePath(
        QStringLiteral("first.tps")
        );
    const QString failedPath = temporaryDirectory.filePath(
        QStringLiteral("failed.tps")
        );
    const auto touch = [](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly)
            && file.write(QByteArrayLiteral("selected profile")) > 0;
    };
    QVERIFY(touch(firstPath));
    QVERIFY(touch(failedPath));
    const Application::WorkspaceLocation firstNormalized(
        QFileInfo(firstPath).canonicalFilePath().toUtf8().toStdString()
        );
    const Application::WorkspaceLocation failedNormalized(
        QFileInfo(failedPath).canonicalFilePath().toUtf8().toStdString()
        );

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    historyProbe->history.paths = {workspaceLocation(
        temporaryDirectory.filePath(QStringLiteral("older.tps"))
        )};
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->openPath = Application::FileDialogPath(
        firstPath.toUtf8().toStdString()
        );
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = journalPath;
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.openProfileFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(fileDialogProbe);
    };
    seams.openProfileRecentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    QAction* const openAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextOpenTeacherProfile")
        );
    QVERIFY(openAction);

    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 1);
    QVERIFY(fileDialogProbe->lastOpenRequest.has_value());
    QCOMPARE(fileDialogProbe->lastOpenRequest->title,
             std::string("Open Teacher Profile"));
    QCOMPARE(fileDialogProbe->lastOpenRequest->nameFilters,
             std::vector<std::string>{"ClassMngr Teacher Profile (*.tps)"});
    QCOMPARE(gatewayProbe->openCalls, 1);
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(QString::fromUtf8(
                 bootstrap.workspaceStateSnapshot().session()->location().value().data(),
                 static_cast<qsizetype>(bootstrap.workspaceStateSnapshot().session()
                     ->location().value().size())
                 ),
             QFileInfo(firstPath).canonicalFilePath());
    QCOMPARE(bootstrap.shell()->navigationModel()->workspaceState(),
             Application::WorkspaceLifecycleState::Open);
    QVERIFY(bootstrap.shell()->navigationModel()->currentPageId().has_value());
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    QCOMPARE(historyProbe->saves.back().paths.front(), firstNormalized);
    QCOMPARE(historyProbe->saves.back().paths.at(1),
             historyProbe->history.paths.front());

    const Application::WorkspaceSession firstSession =
        *bootstrap.workspaceStateSnapshot().session();
    fileDialogProbe->openPath.reset();
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 2);
    QCOMPARE(gatewayProbe->openCalls, 1);
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session(),
             std::optional<Application::WorkspaceSession>(firstSession));

    gatewayProbe->openError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected invalid/corrupt profile failure",
        .recoverable = true
    };
    fileDialogProbe->openPath = Application::FileDialogPath(
        failedPath.toUtf8().toStdString()
        );
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 3);
    QCOMPARE(gatewayProbe->openCalls, 2);
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session(),
             std::optional<Application::WorkspaceSession>(firstSession));

    gatewayProbe->openError.reset();
    openAction->trigger();
    QCOMPARE(gatewayProbe->openCalls, 3);
    QCOMPARE(historyProbe->saveCount, 2);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             failedNormalized);
    QCOMPARE(historyProbe->saves.back().paths.front(), failedNormalized);

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
openActionDoesNotBypassPendingOrUnreadableSetupRecovery()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("pending.tps")
        );
    const QString backupPath = temporaryDirectory.filePath(
        QStringLiteral(".pending.initial-setup-backup")
        );
    const QString journalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    const auto writeFile = [](const QString& path, const QByteArray& contents) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(contents) == contents.size();
    };
    QVERIFY(writeFile(profilePath, QByteArrayLiteral("pending profile")));
    QVERIFY(writeFile(backupPath, QByteArrayLiteral("original profile")));
    const QByteArray journalPayload = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("profile"), QFileInfo(profilePath).absoluteFilePath()},
        {QStringLiteral("originalBackup"), QFileInfo(backupPath).absoluteFilePath()}
    }).toJson(QJsonDocument::Compact);
    QVERIFY(writeFile(journalPath, journalPayload));

    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->openPath = Application::FileDialogPath(
        profilePath.toUtf8().toStdString()
        );
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    int openHistoryFactoryCalls = 0;
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = true;
    seams.initialSetupRecoveryJournalPath = journalPath;
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.openProfileFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(fileDialogProbe);
    };
    seams.openProfileRecentWorkspaceHistoryPortFactory = [&] {
        ++openHistoryFactoryCalls;
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    QAction* const openAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextOpenTeacherProfile")
        );
    QVERIFY(openAction);
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 0);
    QCOMPARE(openHistoryFactoryCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 0);
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QFile journalBefore(journalPath);
    QVERIFY(journalBefore.open(QIODevice::ReadOnly));
    QCOMPARE(journalBefore.readAll(), journalPayload);
    journalBefore.close();

    QVERIFY(writeFile(journalPath, QByteArrayLiteral("{ unreadable recovery journal")));
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 0);
    QCOMPARE(openHistoryFactoryCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 0);
    QFile journalAfter(journalPath);
    QVERIFY(journalAfter.open(QIODevice::ReadOnly));
    QCOMPARE(journalAfter.readAll(), QByteArrayLiteral("{ unreadable recovery journal"));

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
saveAsActionUsesNextGatewayAndPreservesActiveSessionIdentity()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourcePath = temporaryDirectory.filePath(
        QStringLiteral("source.tps")
        );
    const QString destinationPath = temporaryDirectory.filePath(
        QStringLiteral("saved-as.tps")
        );
    {
        WorkspaceStore store;
        QVERIFY(store.open(sourcePath));
        store.close();
    }

    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        destinationPath.toUtf8().toStdString()
        );
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), sourcePath
    };
    seams.recentAutoOpenEnabled = false;
    seams.saveAsProfileFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            fileDialogProbe
            );
    };
    seams.saveAsRecentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(
            historyProbe
            );
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));

    QAction* const saveAsAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextSaveWorkspaceAs")
        );
    QAction* const closeAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextCloseWorkspace")
        );
    QVERIFY(saveAsAction);
    QVERIFY(closeAction);
    QVERIFY(saveAsAction->isEnabled());
    QCOMPARE(saveAsAction->shortcut(), QKeySequence::SaveAs);

    const Application::WorkspaceSession originalSession =
        *bootstrap.workspaceStateSnapshot().session();
    saveAsAction->trigger();

    QCOMPARE(fileDialogProbe->saveCount, 1);
    QVERIFY(fileDialogProbe->lastSaveRequest.has_value());
    QCOMPARE(fileDialogProbe->lastSaveRequest->title,
             std::string("Save Teacher Profile"));
    QCOMPARE(fileDialogProbe->lastSaveRequest->nameFilters,
             std::vector<std::string>{"ClassMngr Teacher Profile (*.tps)"});
    QCOMPARE(fileDialogProbe->lastSaveRequest->defaultSuffix, std::string("tps"));
    QVERIFY(fileDialogProbe->lastSaveRequest->confirmOverwrite);
    QCOMPARE(
        QString::fromUtf8(
            fileDialogProbe->lastSaveRequest->initialDirectory.value().data(),
            static_cast<qsizetype>(
                fileDialogProbe->lastSaveRequest->initialDirectory.value().size()
                )
            ),
        QFileInfo(sourcePath).absolutePath()
        );
    QVERIFY(QFileInfo::exists(sourcePath));
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    const Application::WorkspaceSession savedSession =
        *bootstrap.workspaceStateSnapshot().session();
    QCOMPARE(savedSession.workspaceId(), originalSession.workspaceId());
    QCOMPARE(
        savedSession.location(),
        workspaceLocation(QFileInfo(destinationPath).canonicalFilePath())
        );
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    QCOMPARE(historyProbe->loadCount, 1);
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(historyProbe->saves.back().paths.front(), savedSession.location());

    closeAction->trigger();
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);
    QVERIFY(!saveAsAction->isEnabled());

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
exportActionUsesNextGatewayWithoutResolvingDirtyPage()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourcePath = temporaryDirectory.filePath(
        QStringLiteral("source.tps")
        );
    const QString selectedPath = temporaryDirectory.filePath(
        QStringLiteral("exported.db")
        );
    const QString expectedPath = temporaryDirectory.filePath(
        QStringLiteral("exported.tps")
        );
    const QString recoveryProfilePath = temporaryDirectory.filePath(
        QStringLiteral("interrupted.tps")
        );
    const QString recoveryBackupPath = temporaryDirectory.filePath(
        QStringLiteral(".interrupted.initial-setup-backup")
        );
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        selectedPath.toUtf8().toStdString()
        );
    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), sourcePath
    };
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.pathExists = [](const Application::WorkspaceLocation&) {
        return true;
    };
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };
    int exportDialogFactoryCalls = 0;
    seams.exportProfileFileDialogPortFactory = [fileDialogProbe, &exportDialogFactoryCalls] {
        ++exportDialogFactoryCalls;
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            fileDialogProbe
            );
    };
    seams.workspaceLifecycleUserPromptPortFactory = [promptProbe] {
        return std::make_unique<RecordingWorkspaceLifecycleUserPrompt>(promptProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    const int historySavesAfterStartup = historyProbe->saveCount;
    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    QAction* const exportAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextExportWorkspace")
        );
    QVERIFY(exportAction);
    QVERIFY(exportAction->isEnabled());
    QCOMPARE(exportAction->shortcut(), QKeySequence());
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));

    auto* const page = shell->pageHost().pageInstance(
        QStringLiteral("my_workspace")
        );
    QVERIFY(page);
    page->markUnsavedChanges();
    const Application::WorkspaceSession originalSession =
        *bootstrap.workspaceStateSnapshot().session();
    exportAction->trigger();

    QCOMPARE(fileDialogProbe->saveCount, 1);
    QVERIFY(fileDialogProbe->lastSaveRequest.has_value());
    QCOMPARE(fileDialogProbe->lastSaveRequest->purpose,
             Application::FileDialogPurpose::ExportTeacherProfile);
    QCOMPARE(fileDialogProbe->lastSaveRequest->title,
             std::string("Export Teacher Profile"));
    QCOMPARE(fileDialogProbe->lastSaveRequest->nameFilters,
             std::vector<std::string>{"ClassMngr Teacher Profile (*.tps)"});
    QCOMPARE(fileDialogProbe->lastSaveRequest->defaultSuffix, std::string("tps"));
    QVERIFY(fileDialogProbe->lastSaveRequest->confirmOverwrite);
    QCOMPARE(fileDialogProbe->lastSaveRequest->suggestedFileName, std::string{});
    QCOMPARE(QString::fromUtf8(
                 fileDialogProbe->lastSaveRequest->initialDirectory.value().data(),
                 static_cast<qsizetype>(
                     fileDialogProbe->lastSaveRequest->initialDirectory.value().size()
                     )
                 ),
             QFileInfo(sourcePath).absolutePath());
    QCOMPARE(gatewayProbe->exportCalls, 1);
    QCOMPARE(historyProbe->saveCount, historySavesAfterStartup);
    QVERIFY(gatewayProbe->exportedSession.has_value());
    QCOMPARE(*gatewayProbe->exportedSession, originalSession);
    QVERIFY(gatewayProbe->exportDestination.has_value());
    const QString exportedPath = QString::fromUtf8(
        gatewayProbe->exportDestination->value().data(),
        static_cast<qsizetype>(gatewayProbe->exportDestination->value().size())
        );
    QCOMPARE(QFileInfo(exportedPath).fileName(), QStringLiteral("exported.tps"));
    QCOMPARE(QFileInfo(exportedPath).canonicalPath(),
             QFileInfo(sourcePath).canonicalPath());
    QCOMPARE(bootstrap.workspaceStateSnapshot().session(),
             std::optional<Application::WorkspaceSession>(originalSession));
    QCOMPARE(bootstrap.workspaceStateSnapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Clean);
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(promptProbe->requests.empty());
    QCOMPARE(exportDialogFactoryCalls, 1);
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));

    const auto writeFile = [](const QString& path, const QByteArray& contents) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(contents) == contents.size();
    };
    QVERIFY(writeFile(recoveryProfilePath, QByteArrayLiteral("incomplete profile")));
    QVERIFY(writeFile(recoveryBackupPath, QByteArrayLiteral("original profile")));
    const QByteArray recoveryPayload = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("profile"), QFileInfo(recoveryProfilePath).absoluteFilePath()},
        {QStringLiteral("originalBackup"), QFileInfo(recoveryBackupPath).absoluteFilePath()}
    }).toJson(QJsonDocument::Compact);
    QVERIFY(writeFile(recoveryJournalPath, recoveryPayload));
    exportAction->trigger();
    QCOMPARE(exportDialogFactoryCalls, 1);
    QCOMPARE(fileDialogProbe->saveCount, 1);
    QCOMPARE(gatewayProbe->exportCalls, 1);
    QCOMPARE(historyProbe->saveCount, historySavesAfterStartup);
    QFile readableRecoveryJournal(recoveryJournalPath);
    QVERIFY(readableRecoveryJournal.open(QIODevice::ReadOnly));
    QCOMPARE(readableRecoveryJournal.readAll(), recoveryPayload);
    readableRecoveryJournal.close();

    const QByteArray unreadableRecoveryPayload =
        QByteArrayLiteral("{ unreadable recovery journal");
    QVERIFY(writeFile(recoveryJournalPath, unreadableRecoveryPayload));
    exportAction->trigger();
    QCOMPARE(exportDialogFactoryCalls, 1);
    QCOMPARE(fileDialogProbe->saveCount, 1);
    QCOMPARE(gatewayProbe->exportCalls, 1);
    QCOMPARE(historyProbe->saveCount, historySavesAfterStartup);
    QFile unreadableRecoveryJournal(recoveryJournalPath);
    QVERIFY(unreadableRecoveryJournal.open(QIODevice::ReadOnly));
    QCOMPARE(unreadableRecoveryJournal.readAll(), unreadableRecoveryPayload);

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::saveAndCloseProfileActionsUseNextGateway()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("workspace-actions.tps")
        );
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
    Application::WorkspaceState* workspaceStateForTest = nullptr;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), profilePath
    };
    seams.recentAutoOpenEnabled = false;
    seams.pathExists = [](const Application::WorkspaceLocation&) {
        return true;
    };
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.afterStartupWorkspaceOpenedForTesting = [&workspaceStateForTest](
        Application::WorkspaceState& workspaceState
        ) {
        workspaceStateForTest = &workspaceState;
    };
    seams.workspaceLifecycleUserPromptPortFactory = [promptProbe] {
        return std::make_unique<RecordingWorkspaceLifecycleUserPrompt>(promptProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));

    QAction* const saveAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextSaveWorkspace")
        );
    QAction* const closeAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextCloseWorkspace")
        );
    QVERIFY(saveAction);
    QVERIFY(closeAction);
    QVERIFY(saveAction->isEnabled());
    QVERIFY(closeAction->isEnabled());

    auto* const page = shell->pageHost().pageInstance(
        QStringLiteral("my_workspace")
        );
    QVERIFY(page);

    saveAction->trigger();
    QCOMPARE(gatewayProbe->saveCalls, 0);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             workspaceLocation(profilePath));

    QVERIFY(workspaceStateForTest);
    QVERIFY(workspaceStateForTest->markDirty());
    saveAction->trigger();
    QCOMPARE(gatewayProbe->saveCalls, 1);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    QCOMPARE(bootstrap.workspaceStateSnapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Clean);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             workspaceLocation(profilePath));

    gatewayProbe->saveError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected menu save failure",
        .recoverable = true
    };
    QVERIFY(workspaceStateForTest->markDirty());
    saveAction->trigger();
    QCOMPARE(gatewayProbe->saveCalls, 2);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    QCOMPARE(bootstrap.workspaceStateSnapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Dirty);
    QVERIFY(shell->statusBar()->currentMessage().contains(
        QStringLiteral("could not be saved")
        ));

    gatewayProbe->saveError.reset();
    saveAction->trigger();
    QCOMPARE(gatewayProbe->saveCalls, 3);
    QCOMPARE(bootstrap.workspaceStateSnapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Clean);

    gatewayProbe->closeError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected close failure",
        .recoverable = true
    };
    page->markUnsavedChanges();
    promptProbe->action = Application::UnsavedChangesAction::Save;
    closeAction->trigger();
    QCOMPARE(gatewayProbe->closeCalls, 1);
    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    QVERIFY(closeAction->isEnabled());

    gatewayProbe->closeError.reset();
    closeAction->trigger();
    QCOMPARE(gatewayProbe->closeCalls, 2);
    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("initial_setup"));
    QVERIFY(!saveAction->isEnabled());
    QVERIFY(!closeAction->isEnabled());

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
dirtyPageBlocksNavigationOpenNewAndWindowClose()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("dirty-navigation.tps")
        );
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
    int openDialogFactoryCalls = 0;
    int newDialogFactoryCalls = 0;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), profilePath
    };
    seams.recentAutoOpenEnabled = false;
    seams.pathExists = [](const Application::WorkspaceLocation&) {
        return true;
    };
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.workspaceLifecycleUserPromptPortFactory = [promptProbe] {
        return std::make_unique<RecordingWorkspaceLifecycleUserPrompt>(promptProbe);
    };
    seams.openProfileFileDialogPortFactory = [&openDialogFactoryCalls] {
        ++openDialogFactoryCalls;
        return std::unique_ptr<Application::FileDialogPort>{};
    };
    seams.newProfileFileDialogPortFactory = [&newDialogFactoryCalls] {
        ++newDialogFactoryCalls;
        return std::unique_ptr<Application::FileDialogPort>{};
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    auto* const page = shell->pageHost().pageInstance(
        QStringLiteral("my_workspace")
        );
    QVERIFY(page);
    page->markUnsavedChanges();

    QAction* const openAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextOpenTeacherProfile")
        );
    QAction* const newAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextNewTeacherProfile")
        );
    QVERIFY(openAction);
    QVERIFY(newAction);

    openAction->trigger();
    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QCOMPARE(openDialogFactoryCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 1);
    QVERIFY(page->hasUnsavedChanges());

    newAction->trigger();
    QCOMPARE(promptProbe->requests.size(), std::size_t(2));
    QCOMPARE(newDialogFactoryCalls, 0);
    QVERIFY(page->hasUnsavedChanges());

    const auto campusPage = Ui::PageId::fromString(
        QStringLiteral("campus_information")
        );
    QVERIFY(campusPage.has_value());
    QVERIFY(!shell->navigateTo(*campusPage, Ui::NavigationCause::Selection));
    QCOMPARE(promptProbe->requests.size(), std::size_t(3));
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    QVERIFY(page->hasUnsavedChanges());

    QVERIFY(!shell->close());
    QCOMPARE(promptProbe->requests.size(), std::size_t(4));
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QVERIFY(!bootstrap.shutdownRequested());
    QCOMPARE(gatewayProbe->closeCalls, 0);

    promptProbe->action = Application::UnsavedChangesAction::Discard;
    gatewayProbe->closeError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected shutdown close failure",
        .recoverable = true
    };
    QVERIFY(!shell->close());
    QCOMPARE(promptProbe->requests.size(), std::size_t(5));
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QVERIFY(!bootstrap.shutdownRequested());
    QCOMPARE(gatewayProbe->closeCalls, 1);

    gatewayProbe->closeError.reset();
    QVERIFY(shell->close());
    QCOMPARE(promptProbe->requests.size(), std::size_t(5));
    QVERIFY(bootstrap.shutdownRequested());
    QCOMPARE(gatewayProbe->closeCalls, 2);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
teacherImportUsesPendingStoreTransactionAndCancelRecoversBackup()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("teacher-import-replacement.tps")
        );
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    const QString workbookPath = QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/teacher_import/sectioned_review.xlsx")
        );
    QString fixtureError;
    QVERIFY2(
        createWorkspaceWithFixture(
            profilePath,
            QStringLiteral("representative_startup.sql"),
            &fixtureError
            ),
        qPrintable(fixtureError)
        );
    const auto originalRecords = readRepresentativeProfileRecords(
        profilePath,
        &fixtureError
        );
    QVERIFY2(originalRecords.has_value(), qPrintable(fixtureError));
    const auto originalProfileHash = fileSha256(profilePath, &fixtureError);
    QVERIFY2(originalProfileHash.has_value(), qPrintable(fixtureError));

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        profilePath.toUtf8().toStdString()
        );
    fileDialogProbe->openPath = Application::FileDialogPath(
        workbookPath.toUtf8().toStdString()
        );
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };
    seams.initialSetupFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(fileDialogProbe);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        bootstrap.shell()->pageHost().pageInstance(QStringLiteral("initial_setup"))
        );
    QVERIFY(page);
    auto* const createButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    QVERIFY(createButton);
    createButton->click();
    QCOMPARE(fileDialogProbe->saveCount, 1);
    QVERIFY(QFileInfo::exists(profilePath));
    QVERIFY(QFileInfo::exists(recoveryJournalPath));

    QFile journalBeforeFile(recoveryJournalPath);
    QVERIFY(journalBeforeFile.open(QIODevice::ReadOnly));
    const QByteArray journalBefore = journalBeforeFile.readAll();
    journalBeforeFile.close();
    const QJsonDocument journalDocument = QJsonDocument::fromJson(journalBefore);
    QVERIFY(journalDocument.isObject());
    const QString backupPath = journalDocument.object()
        .value(QStringLiteral("originalBackup")).toString();
    QVERIFY(!backupPath.isEmpty());
    QVERIFY(QFileInfo::exists(backupPath));
    const auto backupHashBefore = fileSha256(backupPath, &fixtureError);
    QVERIFY2(backupHashBefore.has_value(), qPrintable(fixtureError));
    QCOMPARE(*backupHashBefore, *originalProfileHash);

    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral(R"(
            CREATE TRIGGER fail_teacher_import_gs_insert
            BEFORE INSERT ON gs_team
            BEGIN
                SELECT RAISE(ABORT, 'forced teacher import failure');
            END
        )")
        ));

    auto* const importButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupImportTeachers")
        );
    auto* const setupStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupStatus")
        );
    auto* const teacherStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    QVERIFY(importButton && setupStatus && teacherStatus);
    QCOMPARE(setupStatus->text(), QStringLiteral("Profile created. Setup is pending."));
    QVERIFY(!importButton->isHidden());

    QString importInteractionError;
    int importInteractionPhase = 0;
    bool sawFailureWithSelectionsRetained = false;
    std::optional<TeacherImportPersistedRecords> recordsAfterFailedTransaction;
    QTimer importInteractionTimer;
    importInteractionTimer.setInterval(10);
    QObject::connect(
        &importInteractionTimer,
        &QTimer::timeout,
        &importInteractionTimer,
        [&] {
            auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                return;
            }
            auto* const status = dialog->findChild<QLabel*>(
                QStringLiteral("classMngrNextTeacherImportStatus")
                );
            const QString statusText = status ? status->text() : QString();
            if (importInteractionPhase == 0)
            {
                auto* const browse = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportBrowse")
                    );
                if (!browse)
                {
                    importInteractionError = QStringLiteral(
                        "The import dialog did not expose Browse."
                        );
                }
                else
                {
                    importInteractionPhase = 1;
                    browse->click();
                }
            }
            else if (importInteractionPhase == 1
                     && statusText.contains(QStringLiteral("Review")))
            {
                const QString expectedPath = QFileInfo(workbookPath).absoluteFilePath();
                auto* const pathEdit = dialog->findChild<QLineEdit*>(
                    QStringLiteral("classMngrNextTeacherImportFilePath")
                    );
                auto* const selectM1 = dialog->findChild<QRadioButton*>(
                    QStringLiteral("classMngrNextTeacherImportSelect_0")
                    );
                auto* const noneM2 = dialog->findChild<QRadioButton*>(
                    QStringLiteral("classMngrNextTeacherImportNone_1")
                    );
                auto* const allH1 = dialog->findChild<QRadioButton*>(
                    QStringLiteral("classMngrNextTeacherImportAll_2")
                    );
                auto* const firstM1 = dialog->findChild<QCheckBox*>(
                    QStringLiteral("classMngrNextTeacherImportCandidate_0_0")
                    );
                auto* const secondM1 = dialog->findChild<QCheckBox*>(
                    QStringLiteral("classMngrNextTeacherImportCandidate_0_1")
                    );
                auto* const apply = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportApply")
                    );
                if (!pathEdit || !selectM1 || !noneM2 || !allH1
                    || !firstM1 || !secondM1 || !apply
                    || pathEdit->text() != expectedPath
                    || !firstM1->isChecked() || secondM1->isChecked())
                {
                    importInteractionError = QStringLiteral(
                        "The imported workbook path or group defaults were incorrect."
                        );
                    return;
                }
                selectM1->setChecked(true);
                noneM2->setChecked(true);
                allH1->setChecked(true);
                importInteractionPhase = 2;
                apply->click();
            }
            else if (importInteractionPhase == 2
                     && statusText.contains(QStringLiteral("Import failed")))
            {
                auto* const pathEdit = dialog->findChild<QLineEdit*>(
                    QStringLiteral("classMngrNextTeacherImportFilePath")
                    );
                auto* const selectM1 = dialog->findChild<QRadioButton*>(
                    QStringLiteral("classMngrNextTeacherImportSelect_0")
                    );
                auto* const noneM2 = dialog->findChild<QRadioButton*>(
                    QStringLiteral("classMngrNextTeacherImportNone_1")
                    );
                auto* const allH1 = dialog->findChild<QRadioButton*>(
                    QStringLiteral("classMngrNextTeacherImportAll_2")
                    );
                auto* const firstM1 = dialog->findChild<QCheckBox*>(
                    QStringLiteral("classMngrNextTeacherImportCandidate_0_0")
                    );
                auto* const secondM1 = dialog->findChild<QCheckBox*>(
                    QStringLiteral("classMngrNextTeacherImportCandidate_0_1")
                    );
                auto* const apply = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportApply")
                    );
                if (!pathEdit || !selectM1 || !noneM2 || !allH1
                    || !firstM1 || !secondM1 || !apply
                    || pathEdit->text() != QFileInfo(workbookPath).absoluteFilePath()
                    || !selectM1->isChecked() || !noneM2->isChecked()
                    || !allH1->isChecked() || !firstM1->isChecked()
                    || secondM1->isChecked())
                {
                    importInteractionError = QStringLiteral(
                        "A retry did not retain the workbook and review choices."
                        );
                    return;
                }
                sawFailureWithSelectionsRetained = true;
                recordsAfterFailedTransaction = readTeacherImportPersistedRecords(
                    profilePath,
                    &importInteractionError
                    );
                if (!recordsAfterFailedTransaction)
                {
                    return;
                }
                if (!recordsAfterFailedTransaction->koreanTeachers.isEmpty()
                    || !recordsAfterFailedTransaction->nativeEnglishTeachers.isEmpty()
                    || !recordsAfterFailedTransaction->gsTeamMembers.isEmpty()
                    || recordsAfterFailedTransaction->latestSourceDate.has_value())
                {
                    importInteractionError = QStringLiteral(
                        "A failed import transaction left teacher rows or a source date."
                        );
                    return;
                }
                if (!executeWorkspaceSql(
                        profilePath,
                        QStringLiteral("DROP TRIGGER fail_teacher_import_gs_insert")))
                {
                    importInteractionError = QStringLiteral(
                        "The test could not remove its transaction failure trigger."
                        );
                    return;
                }
                importInteractionPhase = 3;
                apply->click();
            }
        }
        );
    QTimer::singleShot(5000, &importInteractionTimer, [&] {
        if (importInteractionPhase >= 3)
        {
            return;
        }
        importInteractionError = QStringLiteral(
            "The import dialog did not reach the retryable success path."
            );
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            if (auto* const cancel = modal->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportCancel")))
            {
                cancel->click();
            }
        }
    });
    importInteractionTimer.start();
    importButton->click();
    importInteractionTimer.stop();

    QVERIFY2(importInteractionError.isEmpty(), qPrintable(importInteractionError));
    QCOMPARE(importInteractionPhase, 3);
    QVERIFY(sawFailureWithSelectionsRetained);
    QVERIFY(recordsAfterFailedTransaction.has_value());
    QCOMPARE(fileDialogProbe->openCount, 1);
    QVERIFY(fileDialogProbe->lastOpenRequest.has_value());
    QCOMPARE(
        fileDialogProbe->lastOpenRequest->purpose,
        Application::FileDialogPurpose::ImportWorkbook
        );
    QVERIFY(teacherStatus->text().contains(QStringLiteral("2026-09-01")));
    QVERIFY(teacherStatus->text().contains(QStringLiteral("2 created")));
    QCOMPARE(setupStatus->text(), QStringLiteral("Profile created. Setup is pending."));
    QVERIFY(QFileInfo::exists(backupPath));
    QFile journalAfterImportFile(recoveryJournalPath);
    QVERIFY(journalAfterImportFile.open(QIODevice::ReadOnly));
    QCOMPARE(journalAfterImportFile.readAll(), journalBefore);
    journalAfterImportFile.close();
    const auto backupHashAfterImport = fileSha256(backupPath, &fixtureError);
    QVERIFY2(backupHashAfterImport.has_value(), qPrintable(fixtureError));
    QCOMPARE(*backupHashAfterImport, *backupHashBefore);

    const auto importedRecords = readTeacherImportPersistedRecords(
        profilePath,
        &fixtureError
        );
    QVERIFY2(importedRecords.has_value(), qPrintable(fixtureError));
    QCOMPARE(
        importedRecords->koreanTeachers,
        QStringList({QString::fromUtf8("홍길동"), QString::fromUtf8("박민준")})
        );
    QCOMPARE(importedRecords->nativeEnglishTeachers, QStringList{QStringLiteral("Alex")});
    QCOMPARE(importedRecords->gsTeamMembers, QStringList{QStringLiteral("Taylor")});
    QVERIFY(importedRecords->latestSourceDate.has_value());
    QCOMPARE(*importedRecords->latestSourceDate, QStringLiteral("2026-09-01"));
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(historyProbe->saves.empty());

    const QString invalidWorkbookPath = temporaryDirectory.filePath(
        QStringLiteral("invalid-teacher-import.xlsx")
        );
    QFile invalidWorkbook(invalidWorkbookPath);
    QVERIFY(invalidWorkbook.open(QIODevice::WriteOnly));
    QCOMPARE(invalidWorkbook.write(QByteArrayLiteral("not an xlsx")), qint64(11));
    invalidWorkbook.close();
    fileDialogProbe->openPath = Application::FileDialogPath(
        invalidWorkbookPath.toUtf8().toStdString()
        );

    int cancelInteractionCount = 0;
    bool sawInvalidWorkbookError = false;
    int cancelInteractionPhase = 0;
    QTimer cancelInteractionTimer;
    cancelInteractionTimer.setInterval(10);
    QObject::connect(
        &cancelInteractionTimer,
        &QTimer::timeout,
        &cancelInteractionTimer,
        [&] {
            auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                return;
            }
            if (cancelInteractionPhase == 0)
            {
                auto* const browse = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportBrowse")
                    );
                if (browse)
                {
                    cancelInteractionPhase = 1;
                    browse->click();
                }
                return;
            }
            auto* const status = dialog->findChild<QLabel*>(
                QStringLiteral("classMngrNextTeacherImportStatus")
                );
            if (cancelInteractionPhase == 1 && status
                && status->text().contains(QStringLiteral("invalid")))
            {
                const auto* const apply = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportApply")
                    );
                auto* const cancel = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportCancel")
                    );
                auto* const pathEdit = dialog->findChild<QLineEdit*>(
                    QStringLiteral("classMngrNextTeacherImportFilePath")
                    );
                if (!apply || !cancel || !pathEdit
                    || apply->isEnabled()
                    || pathEdit->text() != QFileInfo(invalidWorkbookPath).absoluteFilePath())
                {
                    return;
                }
                sawInvalidWorkbookError = true;
                ++cancelInteractionCount;
                cancel->click();
            }
        }
        );
    QTimer::singleShot(3000, &cancelInteractionTimer, [&] {
        if (cancelInteractionCount != 0)
        {
            return;
        }
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            if (auto* const cancel = modal->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportCancel")))
            {
                cancel->click();
            }
        }
    });
    cancelInteractionTimer.start();
    importButton->click();
    cancelInteractionTimer.stop();
    QCOMPARE(cancelInteractionCount, 1);
    QCOMPARE(fileDialogProbe->openCount, 2);
    QVERIFY(sawInvalidWorkbookError);
    QVERIFY(!QFileInfo::exists(recoveryJournalPath));
    QVERIFY(!QFileInfo::exists(backupPath));
    QVERIFY(QFileInfo::exists(profilePath));
    const auto restoredRecords = readRepresentativeProfileRecords(
        profilePath,
        &fixtureError
        );
    QVERIFY2(restoredRecords.has_value(), qPrintable(fixtureError));
    QCOMPARE(*restoredRecords, *originalRecords);
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(historyProbe->saves.empty());
    QCOMPARE(setupStatus->text(), QStringLiteral("Choose where to save your profile."));
    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::imageSignatureSaveReopensAndResumesFromFixture()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("image-signature.tps")
        );
    const QString imagePath = temporaryDirectory.filePath(
        QStringLiteral("fixture-signature.png")
        );
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    QImage sourceImage(4, 2, QImage::Format_RGB32);
    sourceImage.fill(Qt::white);
    sourceImage.setPixelColor(1, 0, Qt::black);
    sourceImage.setPixelColor(2, 1, Qt::black);
    QVERIFY(sourceImage.save(imagePath, "PNG"));

    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        profilePath.toUtf8().toStdString()
        );
    fileDialogProbe->openPath = Application::FileDialogPath(
        imagePath.toUtf8().toStdString()
        );

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.initialSetupFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            fileDialogProbe
            );
    };
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());

    auto* const page = dynamic_cast<Ui::InitialPage*>(
        bootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("initial_setup")
            )
        );
    QVERIFY(page);
    auto* const createButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    QVERIFY(createButton);
    createButton->click();
    QVERIFY(QFileInfo::exists(profilePath));
    QVERIFY(QFileInfo::exists(recoveryJournalPath));

    QString fixtureError;
    QVERIFY2(
        executeWorkspaceFixture(
            profilePath,
            QStringLiteral("initial_setup_personal_details.sql"),
            &fixtureError
            ),
        qPrintable(fixtureError)
        );
    page->refreshPersonalDetails();

    auto* const nameEdit = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupPersonalName")
        );
    auto* const imageMode = page->findChild<QRadioButton*>(
        QStringLiteral("classMngrNextInitialSetupSignatureImageMode")
        );
    auto* const chooseSignature = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupChooseSignature")
        );
    auto* const saveDetails = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsSave")
        );
    auto* const detailsStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsStatus")
        );
    auto* const preview = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupSignaturePreview")
        );
    QVERIFY(nameEdit && imageMode && chooseSignature && saveDetails);
    QVERIFY(detailsStatus && preview);
    QCOMPARE(nameEdit->text(), QStringLiteral("Fixture Teacher"));
    QVERIFY(imageMode->isChecked());

    chooseSignature->click();
    QCOMPARE(fileDialogProbe->openCount, 1);
    QVERIFY(!preview->pixmap().isNull());
    saveDetails->click();
    QCOMPARE(
        detailsStatus->text(),
        QStringLiteral("Personal details saved. Setup is still pending.")
        );
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QVERIFY(bootstrap.shutdown().shutdownRequested);

    QFile sourceFile(imagePath);
    QVERIFY(sourceFile.open(QIODevice::ReadOnly));
    const QByteArray sourceBytes = sourceFile.readAll();
    const QByteArray pagePreparedImage =
        SignatureImage::prepareForEmbedding(sourceBytes);
    const QByteArray persistedImage =
        SignatureImage::prepareForEmbedding(pagePreparedImage);
    const QByteArray reloadedImage =
        SignatureImage::prepareForEmbedding(persistedImage);
    QVERIFY(!reloadedImage.isEmpty());

    WorkspaceStore reopened;
    QVERIFY(reopened.openExisting(profilePath));
    Platform::WorkspaceStorePersonalDetailsPort detailsPort(reopened);
    const Application::PersonalDetailsReadQuery readDetails(detailsPort);
    const auto savedDetails = readDetails.execute();
    QVERIFY(savedDetails);
    QCOMPARE(savedDetails.value().name, std::string("Fixture Teacher"));
    QCOMPARE(
        savedDetails.value().campus.value(),
        std::string("Fixture Campus")
        );
    QCOMPARE(
        savedDetails.value().zoomLoginId,
        std::string("fixture.zoom@example.test")
        );
    QCOMPARE(savedDetails.value().zoomPassword, std::string("fixture-only"));
    QVERIFY(!savedDetails.value().zoomNotAvailable);
    QCOMPARE(
        savedDetails.value().signatureMode,
        Application::PersonalSignatureMode::Image
        );
    QCOMPARE(
        savedDetails.value().signatureImage.value(),
        std::string(
            reloadedImage.constData(),
            static_cast<std::size_t>(reloadedImage.size())
            )
        );
    SettingsRepository* const settings = reopened.settingsRepository();
    QVERIFY(settings);
    const auto storedSignatureMode = settings->loadSetting(
        QStringLiteral("myInfo/signatureMode")
        );
    const auto storedSignatureImage = settings->loadSetting(
        QStringLiteral("myInfo/signatureImage")
        );
    QVERIFY(storedSignatureMode);
    QVERIFY(storedSignatureImage);
    QCOMPARE(storedSignatureMode->toInt(), 0);
    QCOMPARE(
        storedSignatureImage->toString(),
        QString::fromLatin1(persistedImage.toBase64())
        );
    reopened.close();

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams resumeSeams;
    resumeSeams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    resumeSeams.recentAutoOpenEnabled = false;
    resumeSeams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    Bootstrap::ApplicationBootstrap resumedBootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(resumeSeams)
        );
    QVERIFY(resumedBootstrap.start().succeeded());
    auto* const resumedPage = dynamic_cast<Ui::InitialPage*>(
        resumedBootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("initial_setup")
            )
        );
    QVERIFY(resumedPage);
    auto* const resumeButton = resumedPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupResume")
        );
    QVERIFY(resumeButton);
    QVERIFY(!resumeButton->isHidden());
    resumeButton->click();
    auto* const resumedName = resumedPage->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupPersonalName")
        );
    auto* const resumedImageMode = resumedPage->findChild<QRadioButton*>(
        QStringLiteral("classMngrNextInitialSetupSignatureImageMode")
        );
    auto* const resumedPreview = resumedPage->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupSignaturePreview")
        );
    auto* const cancelSetup = resumedPage->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );
    QVERIFY(resumedName && resumedImageMode && resumedPreview && cancelSetup);
    QCOMPARE(resumedName->text(), QStringLiteral("Fixture Teacher"));
    QVERIFY(resumedImageMode->isChecked());
    QVERIFY(!resumedPreview->pixmap().isNull());
    QVERIFY(!cancelSetup->isHidden());
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QVERIFY(resumedBootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
failedPersonalDetailsSavePreservesReplacementBackup()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")
        ));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("representative-original.tps")
        );
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    QString fixtureError;
    QVERIFY2(
        createWorkspaceWithFixture(
            profilePath,
            QStringLiteral("representative_startup.sql"),
            &fixtureError
            ),
        qPrintable(fixtureError)
        );

    const RepresentativeProfileRecords expectedRecords{
        .teachers = {
            QStringLiteral("Dana Kim"),
            QStringLiteral("Jordan Lee"),
            QStringLiteral("Minseo Park"),
            QStringLiteral("Eugene Choi")
        },
        .classes = {
            QStringLiteral("Bluebird E4"),
            QStringLiteral("Falcon E5"),
            QStringLiteral("Hawk M1"),
            QStringLiteral("Owl M2"),
            QStringLiteral("Robin E6"),
            QStringLiteral("Swift M3"),
            QStringLiteral("Cedar Intensive"),
            QStringLiteral("Maple Intensive")
        }
    };
    const auto originalRecords = readRepresentativeProfileRecords(
        profilePath,
        &fixtureError
        );
    QVERIFY2(originalRecords.has_value(), qPrintable(fixtureError));
    QCOMPARE(*originalRecords, expectedRecords);

    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        profilePath.toUtf8().toStdString()
        );
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), profilePath
    };
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.initialSetupFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            fileDialogProbe
            );
    };
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());
    const auto initialSetupPageId = Ui::PageId::fromString(
        QStringLiteral("initial_setup")
        );
    QVERIFY(initialSetupPageId);
    QVERIFY(bootstrap.shell()->navigateTo(*initialSetupPageId));

    auto* const page = dynamic_cast<Ui::InitialPage*>(
        bootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("initial_setup")
            )
        );
    QVERIFY(page);
    auto* const createButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    QVERIFY(createButton);
    QVERIFY(!createButton->isHidden());
    createButton->click();
    QCOMPARE(fileDialogProbe->saveCount, 1);
    QVERIFY(QFileInfo::exists(profilePath));
    QVERIFY(QFileInfo::exists(recoveryJournalPath));

    QFile journalBeforeFile(recoveryJournalPath);
    QVERIFY(journalBeforeFile.open(QIODevice::ReadOnly));
    const QByteArray journalBefore = journalBeforeFile.readAll();
    journalBeforeFile.close();
    const QJsonDocument journalDocument = QJsonDocument::fromJson(journalBefore);
    QVERIFY(journalDocument.isObject());
    const QJsonObject journalRecord = journalDocument.object();
    QCOMPARE(
        journalRecord.value(QStringLiteral("profile")).toString(),
        QFileInfo(profilePath).absoluteFilePath()
        );
    const QString backupPath = journalRecord.value(
        QStringLiteral("originalBackup")
        ).toString();
    QVERIFY(!backupPath.isEmpty());
    QVERIFY(QFileInfo::exists(backupPath));

    const auto backupHashBefore = fileSha256(backupPath, &fixtureError);
    QVERIFY2(backupHashBefore.has_value(), qPrintable(fixtureError));
    const auto backupRecordsBefore = readRepresentativeProfileRecords(
        backupPath,
        &fixtureError
        );
    QVERIFY2(backupRecordsBefore.has_value(), qPrintable(fixtureError));
    QCOMPARE(*backupRecordsBefore, expectedRecords);

    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral(R"(
            CREATE TRIGGER fail_replacement_personal_details_save
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/name'
            BEGIN
                SELECT RAISE(ABORT, 'forced replacement details failure');
            END
        )")
        ));
    auto* const nameEdit = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupPersonalName")
        );
    auto* const saveDetails = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsSave")
        );
    auto* const detailsStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsStatus")
        );
    auto* const setupStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupStatus")
        );
    auto* const cancelSetup = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );
    QVERIFY(nameEdit && saveDetails && detailsStatus && setupStatus && cancelSetup);
    nameEdit->setText(QString::fromUtf8("교체 시도"));
    saveDetails->click();
    QVERIFY(detailsStatus->text().startsWith(
        QStringLiteral(
            "Personal details could not be saved. Setup remains pending; retry to save."
            )
        ));
    QCOMPARE(setupStatus->text(), QStringLiteral("Profile created. Setup is pending."));
    QCOMPARE(nameEdit->text(), QString::fromUtf8("교체 시도"));
    QVERIFY(!cancelSetup->isHidden());
    QVERIFY(QFileInfo::exists(profilePath));
    QVERIFY(QFileInfo::exists(backupPath));
    QVERIFY(QFileInfo::exists(recoveryJournalPath));

    QFile journalAfterFile(recoveryJournalPath);
    QVERIFY(journalAfterFile.open(QIODevice::ReadOnly));
    QCOMPARE(journalAfterFile.readAll(), journalBefore);
    journalAfterFile.close();
    const auto backupHashAfter = fileSha256(backupPath, &fixtureError);
    QVERIFY2(backupHashAfter.has_value(), qPrintable(fixtureError));
    QCOMPARE(*backupHashAfter, *backupHashBefore);
    const auto backupRecordsAfter = readRepresentativeProfileRecords(
        backupPath,
        &fixtureError
        );
    QVERIFY2(backupRecordsAfter.has_value(), qPrintable(fixtureError));
    QCOMPARE(*backupRecordsAfter, expectedRecords);

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    QVERIFY(QFileInfo::exists(backupPath));
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QFile journalAfterShutdown(recoveryJournalPath);
    QVERIFY(journalAfterShutdown.open(QIODevice::ReadOnly));
    QCOMPARE(journalAfterShutdown.readAll(), journalBefore);
    journalAfterShutdown.close();
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
recentOpenErrorDoesNotFallThroughToLaterCandidates()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString firstRecentPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("invalid-first-recent.tps")
        );
    QFile invalidWorkspace(firstRecentPath);
    QVERIFY(invalidWorkspace.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(invalidWorkspace.write("not a workspace database\n"), qint64{25});
    invalidWorkspace.close();

    const QString secondRecentPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("second-recent.db")
        );
    const QString lastFilePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("legacy-last.tps")
        );
    const auto createWorkspaceFile = [](const QString& path) {
        WorkspaceStore store;
        const RepositoryStatus created = store.open(path);
        if (!created)
        {
            return false;
        }
        store.close();
        return true;
    };
    QVERIFY(createWorkspaceFile(secondRecentPath));
    QVERIFY(createWorkspaceFile(lastFilePath));
    const QStringList connectionsBefore = QSqlDatabase::connectionNames();

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    historyProbe->history.paths = {
        workspaceLocation(firstRecentPath),
        workspaceLocation(secondRecentPath)
    };
    historyProbe->history.lastPath = workspaceLocation(lastFilePath);
    std::vector<std::string> checkedPaths;
    std::vector<Application::StartupEvent> observedEvents;

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = true;
    seams.pathExists = [&checkedPaths](const Application::WorkspaceLocation& location) {
        checkedPaths.push_back(location.value());
        return true;
    };
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    const Application::StartupResult result = bootstrap.start();

    verifyWorkspaceStartupOrder(
        result,
        observedEvents,
        bootstrap,
        Application::StartupStageOutcome::Warning
        );
    QVERIFY(bootstrap.startupWorkspaceOutcome().has_value());
    const auto* const failed = std::get_if<Application::StartupWorkspaceOpenFailed>(
        &bootstrap.startupWorkspaceOutcome().value()
        );
    QVERIFY(failed != nullptr);
    QVERIFY(!failed->error.message.empty());
    QCOMPARE(checkedPaths.size(), std::size_t{1});
    QCOMPARE(checkedPaths.front(), workspaceLocation(firstRecentPath).value());
    QCOMPARE(historyProbe->loadCount, 1);
    QVERIFY(historyProbe->saves.empty());
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);
    QCOMPARE(QSqlDatabase::connectionNames(), connectionsBefore);
    QVERIFY(QFileInfo::exists(secondRecentPath));
    QVERIFY(QFileInfo::exists(lastFilePath));

    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
missingExplicitWorkspaceDoesNotCreateFileOrParent()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString missingParent = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("missing-parent")
        );
    const QString missingPath = QDir(missingParent).filePath(
        QStringLiteral("missing-profile.tps")
        );
    const QString firstRecentPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("first-recent.tps")
        );
    const QString secondRecentPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("second-recent.db")
        );
    const QString lastFilePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("legacy-last.tps")
        );
    QVERIFY(!QFileInfo::exists(missingParent));
    QVERIFY(!QFileInfo::exists(missingPath));
    const auto createWorkspaceFile = [](const QString& path) {
        WorkspaceStore store;
        const RepositoryStatus created = store.open(path);
        if (!created)
        {
            return false;
        }
        store.close();
        return true;
    };
    QVERIFY(createWorkspaceFile(firstRecentPath));
    QVERIFY(createWorkspaceFile(secondRecentPath));
    QVERIFY(createWorkspaceFile(lastFilePath));
    const QStringList connectionsBefore = QSqlDatabase::connectionNames();

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    historyProbe->history.paths = {
        workspaceLocation(firstRecentPath),
        workspaceLocation(secondRecentPath)
    };
    historyProbe->history.lastPath = workspaceLocation(lastFilePath);

    std::vector<Application::StartupEvent> observedEvents;
    int pathExistsCalls = 0;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"),
        QStringLiteral("--startup-performance-workflow"),
        QStringLiteral("--startup-performance-output"),
        QStringLiteral("ignored-output.json"),
        QStringLiteral("notes.txt"),
        missingPath,
        firstRecentPath
    };
    seams.recentAutoOpenEnabled = true;
    seams.pathExists = [&pathExistsCalls](const Application::WorkspaceLocation&) {
        ++pathExistsCalls;
        return true;
    };
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    const Application::StartupResult result = bootstrap.start();

    verifyWorkspaceStartupOrder(
        result,
        observedEvents,
        bootstrap,
        Application::StartupStageOutcome::Warning
        );
    QVERIFY(bootstrap.startupWorkspaceOutcome().has_value());
    const auto* const failed = std::get_if<Application::StartupWorkspaceOpenFailed>(
        &bootstrap.startupWorkspaceOutcome().value()
        );
    QVERIFY(failed != nullptr);
    QCOMPARE(failed->error.code, Domain::ErrorCode::NotFound);
    QVERIFY(!failed->error.message.empty());
    QVERIFY(!QFileInfo::exists(missingParent));
    QVERIFY(!QFileInfo::exists(missingPath));
    QCOMPARE(pathExistsCalls, 0);
    QCOMPARE(historyProbe->loadCount, 1);
    QVERIFY(historyProbe->saves.empty());
    QCOMPARE(QSqlDatabase::connectionNames(), connectionsBefore);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);

    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
successfulStartupRetainsSessionAndReleasesStoreOnShutdown()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString workspacePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("startup-profile.tps")
        );
    const QString performanceOutputPath =
        QDir(temporaryDirectory.path()).filePath(
            QStringLiteral("metrics.db")
            );
    for (const QString& seedPath : {workspacePath, performanceOutputPath})
    {
        WorkspaceStore seedStore;
        const RepositoryStatus created = seedStore.open(seedPath);
        QVERIFY2(created.has_value(), "could not create a valid workspace fixture");
        seedStore.close();
    }

    const QStringList connectionsBefore = QSqlDatabase::connectionNames();
    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"),
        QStringLiteral("--startup-performance-workflow"),
        QStringLiteral("--startup-performance-output"),
        performanceOutputPath,
        workspacePath
    };
    seams.recentAutoOpenEnabled = false;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    const Application::StartupResult result = bootstrap.start();

    verifyWorkspaceStartupOrder(
        result,
        observedEvents,
        bootstrap,
        Application::StartupStageOutcome::Completed
        );
    QVERIFY(bootstrap.startupWorkspaceOutcome().has_value());
    const auto* const opened = std::get_if<Application::StartupWorkspaceOpened>(
        &bootstrap.startupWorkspaceOutcome().value()
        );
    QVERIFY(opened != nullptr);
    QCOMPARE(
        opened->session.location().value(),
        workspaceLocation(workspacePath).value()
        );
    QVERIFY(!opened->historyError.has_value());
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(
        bootstrap.workspaceStateSnapshot().session()->workspaceId().value(),
        opened->session.workspaceId().value()
        );
    QCOMPARE(
        bootstrap.workspaceStateSnapshot().session()->location().value(),
        opened->session.location().value()
        );
    QVERIFY(QFileInfo::exists(workspacePath));
    QCOMPARE(QSqlDatabase::connectionNames().size(), connectionsBefore.size() + 1);

    const Application::ShutdownResult shutdown = bootstrap.shutdown();
    QVERIFY(shutdown.shutdownRequested);
    QVERIFY(shutdown.newlyRequested);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);
    QCOMPARE(QSqlDatabase::connectionNames(), connectionsBefore);

    const Application::ShutdownResult repeatedShutdown = bootstrap.shutdown();
    QVERIFY(repeatedShutdown.shutdownRequested);
    QVERIFY(!repeatedShutdown.newlyRequested);
    QCOMPARE(QSqlDatabase::connectionNames(), connectionsBefore);

    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
runtimeRemainsClosedAfterTypedGatewayOpenFailure()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString missingParent = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("typed-error-parent")
        );
    const QString missingPath = QDir(missingParent).filePath(
        QStringLiteral("typed-error-profile.tps")
        );
    QVERIFY(!QFileInfo::exists(missingParent));

    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    gatewayProbe->openError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected typed open failure",
        .recoverable = true
    };
    int gatewayFactoryCalls = 0;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"),
        missingPath
    };
    seams.recentWorkspaceHistoryPortFactory = [] {
        return std::unique_ptr<Application::RecentWorkspaceHistoryPort>{};
    };
    seams.workspaceGatewayFactory = [&gatewayFactoryCalls, gatewayProbe] {
        ++gatewayFactoryCalls;
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };

    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    const Application::StartupResult result = bootstrap.start();

    verifyWorkspaceStartupOrder(
        result,
        observedEvents,
        bootstrap,
        Application::StartupStageOutcome::Warning
        );
    QVERIFY(bootstrap.startupWorkspaceOutcome().has_value());
    const auto* const failed = std::get_if<Application::StartupWorkspaceOpenFailed>(
        &bootstrap.startupWorkspaceOutcome().value()
        );
    QVERIFY(failed != nullptr);
    QCOMPARE(failed->error.code, Domain::ErrorCode::Technical);
    QCOMPARE(failed->error.message, std::string("injected typed open failure"));
    QVERIFY(failed->error.recoverable);
    QCOMPARE(gatewayFactoryCalls, 1);
    QCOMPARE(gatewayProbe->constructed, 1);
    QCOMPARE(gatewayProbe->openCalls, 1);
    QCOMPARE(gatewayProbe->closeCalls, 0);
    QCOMPARE(gatewayProbe->events,
             (std::vector<std::string>{"constructed", "open"}));
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QVERIFY(!QFileInfo::exists(missingParent));
    QVERIFY(!QFileInfo::exists(missingPath));

    const Application::ShutdownResult shutdown = bootstrap.shutdown();
    QVERIFY(shutdown.shutdownRequested);
    QCOMPARE(gatewayProbe->destroyed, 1);
    QCOMPARE(gatewayProbe->events,
             (std::vector<std::string>{"constructed", "open", "destroyed"}));
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
shutdownClosesThroughTheGatewayUsedForStartupOpen()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        5,
        static_cast<int>(Application::Theme::Light)
        );
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(QStringLiteral("ko_KR"), QStringLiteral("light")));
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString workspacePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("gateway-backed-profile.tps")
        );
    QVERIFY(!QFileInfo::exists(workspacePath));

    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    int gatewayFactoryCalls = 0;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"),
        workspacePath
    };
    seams.recentWorkspaceHistoryPortFactory = [] {
        return std::unique_ptr<Application::RecentWorkspaceHistoryPort>{};
    };
    seams.workspaceGatewayFactory = [&gatewayFactoryCalls, gatewayProbe] {
        ++gatewayFactoryCalls;
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };

    std::vector<Application::StartupEvent> observedEvents;
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        [&observedEvents](const Application::StartupEvent& event) {
            observedEvents.push_back(event);
        },
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams)
        );
    const Application::StartupResult result = bootstrap.start();

    verifyWorkspaceStartupOrder(
        result,
        observedEvents,
        bootstrap,
        Application::StartupStageOutcome::Completed
        );
    QVERIFY(bootstrap.startupWorkspaceOutcome().has_value());
    const auto* const opened = std::get_if<Application::StartupWorkspaceOpened>(
        &bootstrap.startupWorkspaceOutcome().value()
        );
    QVERIFY(opened != nullptr);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(*bootstrap.workspaceStateSnapshot().session(), opened->session);
    QCOMPARE(gatewayFactoryCalls, 1);
    QCOMPARE(gatewayProbe->constructed, 1);
    QCOMPARE(gatewayProbe->openCalls, 1);
    QCOMPARE(gatewayProbe->closeCalls, 0);
    QCOMPARE(gatewayProbe->openedSession, opened->session);
    QVERIFY(!QFileInfo::exists(workspacePath));

    const Application::ShutdownResult shutdown = bootstrap.shutdown();
    QVERIFY(shutdown.shutdownRequested);
    QVERIFY(shutdown.newlyRequested);
    QCOMPARE(gatewayProbe->closeCalls, 1);
    QCOMPARE(gatewayProbe->closedSession, gatewayProbe->openedSession);
    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Closed);
    QCOMPARE(gatewayProbe->destroyed, 1);
    QCOMPARE(gatewayProbe->events,
             (std::vector<std::string>{"constructed", "open", "close", "destroyed"}));

    application.setStyleSheet(previousStyleSheet);
}

QTEST_MAIN(ApplicationBootstrapTests)

#include "application_bootstrap_tests.moc"
