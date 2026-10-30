#include "next/application/shutdown_coordinator.h"
#include "next/application/startup_coordinator.h"
#include "next/application/file_dialog_port.h"
#include "next/application/language_preferences_port.h"
#include "next/application/theme_preferences_port.h"
#include "next/application/gs_team_birthday_directory_read_port.h"
#include "next/application/korean_teacher_birthday_directory_read_port.h"
#include "next/application/native_english_teacher_birthday_directory_read_port.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/bootstrap/update_coordinator.h"
#include "next/platform/settings_manager_skipped_update_version_port.h"
#include "ui/shared/dialogs/update_dialog.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_descriptor.h"
#include "next/resources/resource_error.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/ui/application_shell.h"
#include "next/ui/document_catalog_navigation.h"
#include "next/ui/classes_navigator_page.h"
#include "next/ui/initial_page.h"
#include "next/ui/my_classes_page.h"
#include "next/ui/page_host.h"
#include "next/ui/placeholder_page.h"
#include "next/ui/schedule_page.h"
#include "next/ui/sub_prep_page.h"
#include "features/sub_prep/ui/sub_prep_class_information_list_model.h"
#include "next/ui/selected_class_editor.h"
#include "next/ui/speaking_evaluation_ai_dialogs.h"
#include "next/ui/speaking_evaluation_page.h"
#include "next/ui/teacher_profile_page.h"
#include "next/ui/shared_staff_directory_page.h"
#include "next/ui/preferences_dialog.h"
#include "next/application/startup_update_check_use_case.h"
#include "next/platform/settings_manager_preferences_port.h"
#include "next/platform/settings_manager_save_mode_preferences_port.h"
#include "next/platform/settings_manager_document_page_spacing_preferences_port.h"
#include "next/platform/settings_manager_document_viewer_background_preferences_port.h"
#include "next/platform/settings_manager_ai_comment_provider_preferences_port.h"
#include "next/platform/settings_manager_ai_comment_voice_preferences_port.h"
#include "next/platform/settings_manager_ai_comment_custom_website_port.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"
#include "next/platform/settings_manager_excel_import_timeout_port.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "core/settingsmanager.h"
#include "features/my_info/data/signature_image_processor.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "next/platform/workspace_store_personal_details_port.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/platform/workspace_interrupted_recovery_port.h"
#include "next/platform/qsettings_file_dialog_directory_preferences_adapter.h"
#include "ui/shared/state/option_state_keys.h"

#include <QApplication>
#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QComboBox>
#include <QDesktopServices>
#include <QEvent>
#include <QFontDatabase>
#include <QFormLayout>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFocusEvent>
#include <QGroupBox>
#include <QImage>
#include <QPointer>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleHints>
#include <QStyle>
#include <QPushButton>
#include <QRadioButton>
#include <QSysInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSaveFile>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QTableView>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QItemSelectionModel>
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
    void manualUpdateActionSharesCoordinatorBeforeStartupDispatch();
    void startupUpdateCheckDoesNotConstructDispatcherAfterFailedStartup();
    void startupUpdateCheckDoesNotConstructDispatcherAfterShutdownBeforeQueuedTurn();
    void loadsOnlySelectedLanguageThemeAndRequiredFontsBeforeShell();
    void loadedPreferencesReachPagesAndFontStartupRestoresOnTeardown();
    void preferencesMenuRoutePersistsAndReopensEveryPreference();
    void preferencesRouteRetranslatesActiveClassesNavigator();
    void preferencesMenuRouteVisualCapture_data();
    void preferencesMenuRouteVisualCapture();
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
    void myClassesReloadsRealWorkspaceDataWhenProfileIsReplacedWhileActive();
    void speakingEvaluationAiCompositionUsesWorkspaceAndPreferences();
    void classesNavigatorClearsAndReloadsWhenWorkspaceIsReplacedWhileActive();
    void classesNavigatorClearsWhenWorkspaceIsClosedWhileActive();
    void scheduleSelectionOpensClassDetailsAndRefreshesAfterSave();
    void subPrepRouteUsesTheActiveGatewayAndReleasesAcrossLifecycle();
    void openActionDoesNotBypassPendingOrUnreadableSetupRecovery();
    void interruptedProfileRecoveryMenuActionOpensCopyOnlyAfterSuccessfulOpen();
    void recentFilesMenuRefreshesOrderedEscapedPathsAndEmptyState();
    void recentRestoreUsesOpenFlowAndResolvesDirtyPageBeforeReplacement();
    void recentRestoreSavesDirtyWorkspaceBeforeReplacement();
    void recentRestoreRemembersDirectoryWhenHistorySaveFails();
    void clearRecentHistoryPreservesClosedAndOpenWorkspaceState();
    void teacherProfileUsesTypedOpenAndStaticPageHistory();
    void saveAsActionUsesNextGatewayAndPreservesActiveSessionIdentity();
    void exportActionUsesNextGatewayWithoutResolvingDirtyPage();
    void saveAndCloseProfileActionsUseNextGateway();
    void dirtyPageBlocksNavigationOpenNewAndWindowClose();
    void teacherImportUsesPendingStoreTransactionAndCancelRecoversBackup();
    void normalTeacherImportUsesWorkspaceStoreAndKeepsRoute();
    void normalTeacherImportNativeDirectoryRefreshFailure();
    void normalTeacherImportGsDirectoryRefreshFailure();
    void imageSignatureSaveReopensAndResumesFromFixture();
    void failedPersonalDetailsSavePreservesReplacementBackup();
    void recentOpenErrorDoesNotFallThroughToLaterCandidates();
    void missingExplicitWorkspaceDoesNotCreateFileOrParent();
    void successfulStartupRetainsSessionAndReleasesStoreOnShutdown();
    void runtimeRemainsClosedAfterTypedGatewayOpenFailure();
    void shutdownClosesThroughTheGatewayUsedForStartupOpen();

private:
    void verifyNormalTeacherImportDirectoryRefreshFailure(bool nativeDirectory);
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

QImage visualReviewSignature()
{
    QImage image(360, 120, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen ink(QColor(QStringLiteral("#202830")));
    ink.setWidth(5);
    ink.setCapStyle(Qt::RoundCap);
    ink.setJoinStyle(Qt::RoundJoin);
    painter.setPen(ink);

    QPainterPath signature;
    signature.moveTo(18, 82);
    signature.cubicTo(31, 44, 43, 34, 51, 66);
    signature.cubicTo(58, 96, 68, 36, 84, 58);
    signature.cubicTo(94, 73, 98, 77, 111, 61);
    signature.cubicTo(124, 45, 137, 43, 144, 59);
    signature.cubicTo(151, 76, 157, 77, 171, 60);
    signature.cubicTo(184, 44, 198, 48, 204, 68);
    signature.cubicTo(211, 88, 224, 90, 242, 71);
    signature.cubicTo(258, 55, 278, 52, 337, 57);
    painter.drawPath(signature);
    painter.drawLine(25, 101, 330, 101);
    painter.end();
    return image;
}

QImage fixedViewportCapture(QWidget& widget)
{
    QImage capture(QSize(1024, 720), QImage::Format_ARGB32_Premultiplied);
    capture.fill(Qt::transparent);
    QPainter painter(&capture);
    widget.render(&painter);
    painter.end();
    return capture;
}

class ScopedCaptureAppearance final
{
public:
    ScopedCaptureAppearance(QApplication& application, const bool enabled)
        : m_application(application)
        , m_enabled(enabled)
        , m_styleName(application.style()->objectName())
        , m_styleSheet(application.styleSheet())
        , m_palette(application.palette())
    {
        if (m_enabled)
        {
            m_application.setStyle(QStringLiteral("Fusion"));
            m_application.setPalette(m_application.style()->standardPalette());
            m_application.setStyleSheet({});
        }
    }

    ~ScopedCaptureAppearance()
    {
        if (m_enabled)
        {
            m_application.setStyle(m_styleName);
            m_application.setPalette(m_palette);
            m_application.setStyleSheet(m_styleSheet);
        }
    }

private:
    QApplication& m_application;
    bool m_enabled = false;
    QString m_styleName;
    QString m_styleSheet;
    QPalette m_palette;
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
    int clearCount = 0;
    bool mutateOnSave = false;
    bool failSave = false;
    bool failClear = false;
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
        if (m_probe->failSave)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected recent history save failure",
                .recoverable = true
            });
        }
        if (m_probe->mutateOnSave)
        {
            m_probe->history = history;
        }
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> clear() const override
    {
        ++m_probe->clearCount;
        if (m_probe->failClear)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected history clear failure",
                .recoverable = true
            });
        }
        m_probe->history = {};
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

struct InterruptedWorkspaceRecoveryProbe final
{
    Application::FileDialogPath directory;
    Application::InterruptedWorkspaceRecoveryCandidate candidate;
    Application::FileDialogPath lastScannedDirectory;
    std::optional<Application::WorkspaceLocation> lastDestination;
    int scanCalls = 0;
    int copyCalls = 0;
};

class RecordingInterruptedWorkspaceRecoveryPort final
    : public Application::InterruptedWorkspaceRecoveryPort
{
public:
    explicit RecordingInterruptedWorkspaceRecoveryPort(
        std::shared_ptr<InterruptedWorkspaceRecoveryProbe> probe
        )
        : m_probe(std::move(probe))
    {
    }

    Application::FileDialogPath defaultTeacherProfileDirectory()
        const override
    {
        return m_probe->directory;
    }

    Application::FileDialogPath profileDirectoryFor(
        const Application::WorkspaceLocation& profile
        ) const override
    {
        const QString path = QString::fromUtf8(
            profile.value().data(),
            static_cast<qsizetype>(profile.value().size())
            );
        return Application::FileDialogPath(
            QFileInfo(path).absolutePath().toUtf8().toStdString()
            );
    }

    Domain::Result<std::vector<
        Application::InterruptedWorkspaceRecoveryCandidate>>
    candidatesIn(const Application::FileDialogPath& directory) const override
    {
        ++m_probe->scanCalls;
        m_probe->lastScannedDirectory = directory;
        return Domain::Result<std::vector<
            Application::InterruptedWorkspaceRecoveryCandidate>>::success({
                m_probe->candidate
            });
    }

    bool pathExists(const Application::FileDialogPath& path) const override
    {
        return m_fileOperations.pathExists(path);
    }

    Domain::Result<void> copyCandidateNoReplace(
        const Application::InterruptedWorkspaceRecoveryCandidate& candidate,
        const Application::WorkspaceLocation& destination
        ) const override
    {
        ++m_probe->copyCalls;
        m_probe->lastDestination = destination;
        return m_fileOperations.copyCandidateNoReplace(candidate, destination);
    }

private:
    std::shared_ptr<InterruptedWorkspaceRecoveryProbe> m_probe;
    Platform::QtWorkspaceInterruptedRecoveryPort m_fileOperations;
};

struct InterruptedRecoveryUserPromptProbe final
{
    std::optional<Application::PromptActionId> selectedAction;
    std::vector<Application::NamedActionPromptRequest> candidateRequests;
    int unsavedPromptCalls = 0;
};

class RecordingInterruptedRecoveryUserPrompt final
    : public Application::UserPromptPort
{
public:
    explicit RecordingInterruptedRecoveryUserPrompt(
        std::shared_ptr<InterruptedRecoveryUserPromptProbe> probe
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
            Application::ConfirmationResponse::Declined
            );
    }

    Domain::Result<Application::UnsavedChangesAction> confirmUnsavedChanges(
        const Application::UnsavedChangesPromptRequest& request
        ) override
    {
        static_cast<void>(request);
        ++m_probe->unsavedPromptCalls;
        return Domain::Result<Application::UnsavedChangesAction>::success(
            Application::UnsavedChangesAction::Cancel
            );
    }

    Domain::Result<std::optional<Application::PromptActionId>> chooseAction(
        const Application::NamedActionPromptRequest& request
        ) override
    {
        m_probe->candidateRequests.push_back(request);
        return Domain::Result<std::optional<Application::PromptActionId>>::success(
            m_probe->selectedAction
            );
    }

private:
    std::shared_ptr<InterruptedRecoveryUserPromptProbe> m_probe;
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
    std::vector<std::string> workspaceOperationOrder;
    std::optional<Application::WorkspaceSession> savedSession;
    std::optional<Application::WorkspaceSession> exportedSession;
    std::optional<Application::WorkspaceLocation> exportDestination;
    std::vector<std::string>* shutdownEvents = nullptr;
};

class RecordingBootstrapWorkspaceGateway
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
        m_probe->workspaceOperationOrder.push_back("open");
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
        ++m_probe->saveCalls;
        m_probe->workspaceOperationOrder.push_back("save");
        m_probe->savedSession = request.session;
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

class RecordingBirthdayBootstrapWorkspaceGateway final
    : public RecordingBootstrapWorkspaceGateway,
      public Application::KoreanTeacherBirthdayDirectoryReadPort,
      public Application::NativeEnglishTeacherBirthdayDirectoryReadPort,
      public Application::GsTeamBirthdayDirectoryReadPort
{
public:
    using RecordingBootstrapWorkspaceGateway::RecordingBootstrapWorkspaceGateway;

    Application::KoreanTeacherBirthdayDirectoryReadResult
    readKoreanTeacherBirthdayDirectory() const override
    {
        ++koreanReads;
        return Application::KoreanTeacherBirthdayDirectoryReadResult::success({});
    }

    Application::NativeEnglishTeacherBirthdayDirectoryReadResult
    readNativeEnglishTeacherBirthdayDirectory() const override
    {
        ++nativeEnglishReads;
        return Application::NativeEnglishTeacherBirthdayDirectoryReadResult::success({});
    }

    Application::GsTeamBirthdayDirectoryReadResult
    readGsTeamBirthdayDirectory() const override
    {
        ++gsTeamReads;
        return Application::GsTeamBirthdayDirectoryReadResult::success({});
    }

    mutable int koreanReads = 0;
    mutable int nativeEnglishReads = 0;
    mutable int gsTeamReads = 0;
};

struct UnsavedChangesPromptProbe final
{
    Application::UnsavedChangesAction action =
        Application::UnsavedChangesAction::Cancel;
    std::optional<Domain::OperationError> error;
    std::vector<Application::UnsavedChangesPromptRequest> requests;
    Application::ConfirmationResponse confirmationResponse =
        Application::ConfirmationResponse::Accepted;
    std::vector<Application::ConfirmationPromptRequest> confirmationRequests;
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
        m_probe->confirmationRequests.push_back(request);
        return Domain::Result<Application::ConfirmationResponse>::success(
            m_probe->confirmationResponse
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

class GlobalPreferencesSettingsGuard final
{
public:
    GlobalPreferencesSettingsGuard()
        : m_keys{
            QString::fromUtf8(SettingsManager::Keys::THEME_PREFERENCE),
            QString::fromUtf8(SettingsManager::Keys::LANGUAGE_PREFERENCE),
            QString::fromUtf8(SettingsManager::Keys::FONT_SIZE_PREFERENCE),
            QString::fromUtf8(SettingsManager::Keys::SAVE_MODE_PREFERENCE),
            QString::fromUtf8(SettingsManager::Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED),
            QString::fromUtf8(SettingsManager::Keys::SIDEBAR_TOOLTIPS_ENABLED),
            QString::fromUtf8(SettingsManager::Keys::SIDEBAR_MARQUEE_ENABLED),
            QString::fromUtf8(SettingsManager::Keys::EXCEL_IMPORT_TIMEOUT_SECONDS),
            QString::fromUtf8(SettingsManager::Keys::SHOW_POWERPOINT_DATA_ACCESS_NOTICE),
            QString::fromUtf8(SettingsManager::Keys::DOCUMENT_PAGE_SPACING_PREFERENCE),
            QString::fromUtf8(SettingsManager::Keys::DOCUMENT_VIEWER_BACKGROUND_PREFERENCE),
            QString::fromUtf8(SettingsManager::Keys::AI_COMMENT_PROVIDER_PREFERENCE),
            QString::fromUtf8(SettingsManager::Keys::AI_COMMENT_VOICE_PREFERENCE),
            QString::fromUtf8(SettingsManager::Keys::AI_COMMENT_CUSTOM_WEBSITE_URL_PREFERENCE)
        }
    {
        for (std::size_t index = 0; index < m_keys.size(); ++index)
        {
            m_values[index] = SettingsManager::instance().get(m_keys[index]);
        }
    }

    ~GlobalPreferencesSettingsGuard()
    {
        for (std::size_t index = 0; index < m_keys.size(); ++index)
        {
            if (m_values[index].isValid())
            {
                SettingsManager::instance().set(m_keys[index], m_values[index]);
            }
            else
            {
                SettingsManager::instance().remove(m_keys[index]);
            }
        }
        SettingsManager::instance().sync();
    }

private:
    std::array<QString, 14> m_keys;
    std::array<QVariant, 14> m_values;
};

class PreferencesSaveModeProbePage final : public Ui::Page
{
public:
    using Ui::Page::Page;

    void setSaveMode(const Application::SaveMode next) override
    {
        mode = next;
        ++updates;
    }

    Application::SaveMode mode = Application::SaveMode::Automatic;
    int updates = 0;
};

class RecordingAutomaticStartupCheckPort final
    : public Application::StartupUpdateCheckPort
{
public:
    [[nodiscard]] Application::StartupUpdateCheckDispatch check(
        Application::StartupUpdateCheckCompletion completion
        ) override
    {
        ++calls;
        pending = std::move(completion);
        return Application::StartupUpdateCheckDispatch::Started;
    }

    int calls = 0;
    Application::StartupUpdateCheckCompletion pending;
};

class SpeakingEvaluationAiPreferencesGuard final
{
public:
    SpeakingEvaluationAiPreferencesGuard()
        : m_keys{
            QString::fromUtf8(OptionKeys::AiCommentProvider),
            QString::fromUtf8(OptionKeys::AiCommentVoice),
            QString::fromUtf8(OptionKeys::AiCommentCustomWebsiteUrl)
        }
    {
        for (std::size_t index = 0; index < m_keys.size(); ++index)
        {
            m_values[index] = SettingsManager::instance().get(m_keys[index]);
        }
    }

    ~SpeakingEvaluationAiPreferencesGuard()
    {
        for (std::size_t index = 0; index < m_keys.size(); ++index)
        {
            if (m_values[index].isValid())
            {
                SettingsManager::instance().set(m_keys[index], m_values[index]);
            }
            else
            {
                SettingsManager::instance().remove(m_keys[index]);
            }
        }
    }

private:
    std::array<QString, 3> m_keys;
    std::array<QVariant, 3> m_values;
};

class CapturedSpeakingEvaluationAiUrlHandler final : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> urls;

public slots:
    void capture(const QUrl& url)
    {
        urls.append(url);
    }
};

class SpeakingEvaluationAiHttpsUrlHandlerRegistration final
{
public:
    explicit SpeakingEvaluationAiHttpsUrlHandlerRegistration(
        CapturedSpeakingEvaluationAiUrlHandler* receiver
        )
    {
        QDesktopServices::setUrlHandler(
            QStringLiteral("https"), receiver, "capture");
    }

    ~SpeakingEvaluationAiHttpsUrlHandlerRegistration()
    {
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
    }

    SpeakingEvaluationAiHttpsUrlHandlerRegistration(
        const SpeakingEvaluationAiHttpsUrlHandlerRegistration&) = delete;
    SpeakingEvaluationAiHttpsUrlHandlerRegistration& operator=(
        const SpeakingEvaluationAiHttpsUrlHandlerRegistration&) = delete;
};

class StartupSaveModeGuard final
{
public:
    explicit StartupSaveModeGuard(const int mode)
        : m_key(QString::fromUtf8(OptionKeys::SaveMode))
        , m_previous(SettingsManager::instance().get(m_key))
    {
        SettingsManager::instance().set(m_key, mode);
    }

    ~StartupSaveModeGuard()
    {
        if (m_previous.isValid())
        {
            SettingsManager::instance().set(m_key, m_previous);
        }
        else
        {
            SettingsManager::instance().remove(m_key);
        }
    }

private:
    QString m_key;
    QVariant m_previous;
};

class TeacherProfileDirectoryPreferenceGuard final
{
public:
    explicit TeacherProfileDirectoryPreferenceGuard(
        const QString& settingsPath
        )
        : m_previousFormat(QSettings::defaultFormat())
        , m_previousOrganization(qApp->organizationName())
    {
        qApp->setOrganizationName(QStringLiteral("ClassMngrDirectoryPreferenceTests"));
        QSettings::setPath(
            QSettings::IniFormat,
            QSettings::UserScope,
            settingsPath
            );
        QSettings::setDefaultFormat(QSettings::IniFormat);
        m_previous = m_preferences.readDirectory(
            Application::FileDialogPurpose::TeacherProfile
            );
    }

    ~TeacherProfileDirectoryPreferenceGuard()
    {
        static_cast<void>(m_preferences.writeDirectory(
            Application::FileDialogPurpose::TeacherProfile,
            m_previous
            ));
        QSettings::setDefaultFormat(m_previousFormat);
        qApp->setOrganizationName(m_previousOrganization);
    }

    [[nodiscard]] Application::DirectoryPreferencePath read() const
    {
        return m_preferences.readDirectory(
            Application::FileDialogPurpose::TeacherProfile
            );
    }

    [[nodiscard]] Domain::Result<void> write(
        const QString& path
        ) const
    {
        return m_preferences.writeDirectory(
            Application::FileDialogPurpose::TeacherProfile,
            Application::DirectoryPreferencePath(path.toUtf8().toStdString())
            );
    }

private:
    QSettings::Format m_previousFormat;
    QString m_previousOrganization;
    Platform::QSettingsFileDialogDirectoryPreferencesAdapter m_preferences;
    Application::DirectoryPreferencePath m_previous;
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

    friend bool operator==(
        const TeacherImportPersistedRecords&,
        const TeacherImportPersistedRecords&
        ) = default;
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

std::optional<TeacherImportPersistedRecords>
readTeacherImportNamesWithoutSettings(const QString& profilePath)
{
    const QString connectionName = QStringLiteral("teacher-import-name-read-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase database = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(profilePath);
    if (!database.open())
    {
        QSqlDatabase::removeDatabase(connectionName);
        return std::nullopt;
    }

    TeacherImportPersistedRecords result;
    const auto readNames = [&database](
                               const QString& statement,
                               QStringList& values) {
        QSqlQuery query(database);
        if (!query.exec(statement))
        {
            return false;
        }
        while (query.next())
        {
            values.append(query.value(0).toString());
        }
        return true;
    };
    const bool readSucceeded = readNames(
            QStringLiteral("SELECT teacher_kr FROM teachers ORDER BY id"),
            result.koreanTeachers)
        && readNames(
            QStringLiteral("SELECT name FROM native_english_teachers ORDER BY id"),
            result.nativeEnglishTeachers)
        && readNames(
            QStringLiteral("SELECT name FROM gs_team ORDER BY id"),
            result.gsTeamMembers);
    database.close();
    database = {};
    QSqlDatabase::removeDatabase(connectionName);
    return readSucceeded
        ? std::optional<TeacherImportPersistedRecords>(std::move(result))
        : std::nullopt;
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
    QAction* const upcomingBirthdaysAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextUpcomingBirthdays")
        );
    QVERIFY(upcomingBirthdaysAction);
    QVERIFY(!upcomingBirthdaysAction->isEnabled());

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
    QCOMPARE(navigationNodes.size(), std::size_t(13));
    const auto findPageNode = [&navigationNodes](const QString& id)
        -> const Ui::NavigationNode* {
        const auto found = std::find_if(
            navigationNodes.cbegin(), navigationNodes.cend(),
            [&id](const Ui::NavigationNode& node) {
                return node.pageId && node.pageId->value() == id;
            });
        return found == navigationNodes.cend() ? nullptr : &*found;
    };
    const auto findGroupNode = [&navigationNodes](const QString& label)
        -> const Ui::NavigationNode* {
        const auto found = std::find_if(
            navigationNodes.cbegin(), navigationNodes.cend(),
            [&label](const Ui::NavigationNode& node) {
                return node.label == label;
            });
        return found == navigationNodes.cend() ? nullptr : &*found;
    };
    QVERIFY(findPageNode(QStringLiteral("my_workspace")));
    QVERIFY(findPageNode(QStringLiteral("schedule")));
    QVERIFY(pageHost.isPageRegistered(QStringLiteral("schedule")));
    QVERIFY(findPageNode(QStringLiteral("calendar")));
    QVERIFY(pageHost.isPageRegistered(QStringLiteral("calendar")));
    QVERIFY(findPageNode(QStringLiteral("my_classes")));
    QVERIFY(findPageNode(QStringLiteral("classes")));
    QVERIFY(findPageNode(QStringLiteral("sub_prep")));
    QVERIFY(findPageNode(QStringLiteral("initial_setup")));
    const Ui::NavigationNode* const coTeachers =
        findGroupNode(QStringLiteral("Co-Teachers"));
    const Ui::NavigationNode* const campusStaff =
        findGroupNode(QStringLiteral("Campus Staff"));
    const Ui::NavigationNode* const usefulLinks =
        findGroupNode(QStringLiteral("Useful Links"));
    const Ui::NavigationNode* const campusDirectory =
        findGroupNode(QStringLiteral("Campus Directory"));
    QVERIFY(coTeachers);
    QVERIFY(campusStaff);
    QVERIFY(usefulLinks);
    QVERIFY(campusDirectory);
    QCOMPARE(campusStaff->children.size(), std::size_t(3));
    QCOMPARE(campusStaff->children[0].label, QStringLiteral("Korean Teachers"));
    QCOMPARE(campusStaff->children[1].pageId->value(),
             QStringLiteral("native_english_teachers"));
    QCOMPARE(campusStaff->children[2].pageId->value(), QStringLiteral("gs_team"));
    QCOMPARE(campusDirectory->children.size(), std::size_t(5));
    QCOMPARE(campusDirectory->children[0].pageId->value(),
             QStringLiteral("campus_information"));
    QCOMPARE(campusDirectory->children[1].pageId->value(),
             QStringLiteral("campus_directions"));
    QCOMPARE(campusDirectory->children[4].pageId->value(), QStringLiteral("campus_map"));
    QCOMPARE(usefulLinks->children.size(), std::size_t(7));
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
        const Ui::NavigationNode& link = usefulLinks->children[index];
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

    const auto classesPageId = Ui::PageId::fromString(QStringLiteral("classes"));
    QVERIFY(classesPageId.has_value());
    const auto classesDescriptor = pageHost.pageDescriptor(*classesPageId);
    QVERIFY(classesDescriptor.has_value());
    QCOMPARE(classesDescriptor->resourcePolicy, Ui::PageResourcePolicy::PageScoped);
    QCOMPARE(classesDescriptor->initialDataPolicy, Ui::PageInitialDataPolicy::None);
    QVERIFY(!pageHost.hasPageInstance(QStringLiteral("classes")));
    QVERIFY(pageHost.activatePage(QStringLiteral("classes")));
    auto* const classesPage = dynamic_cast<Ui::ClassesNavigatorPage*>(
        pageHost.pageInstance(QStringLiteral("classes"))
        );
    QVERIFY(classesPage);
    QVERIFY(classesPage->isActive());
    QCOMPARE(classesPage->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"))
                 ->text(),
             QStringLiteral("No Teacher Profile is open."));
    QVERIFY(!dynamic_cast<Ui::PlaceholderPage*>(classesPage));

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

void ApplicationBootstrapTests::manualUpdateActionSharesCoordinatorBeforeStartupDispatch()
{
    class OfflinePorts final : public Application::StartupUpdateCheckPort,
                               public Application::ManualUpdateCheckPort
    {
    public:
        int manualCalls = 0;
        int startupCalls = 0;
        Application::ManualUpdateCheckSnapshot snapshot() const override { return {}; }
        void setObserver(Application::ManualUpdateCheckObserver) override {}
        Application::ManualUpdateCheckDispatch check(
            Application::ManualUpdateCheckPolicy,
            Application::ManualUpdateCheckCompletion completion) override
        {
            ++manualCalls;
            completion(Application::ManualUpdateCheckError{
                Application::ManualUpdateCheckErrorCode::Configuration,
                "Offline Next updater fixture"});
            return Application::ManualUpdateCheckDispatch::Failed;
        }
        Application::StartupUpdateCheckDispatch check(
            Application::StartupUpdateCheckCompletion completion) override
        {
            ++startupCalls;
            completion(Application::StartupUpdateCheckError{
                Application::StartupUpdateCheckErrorCode::Configuration,
                "Offline startup fixture"});
            return Application::StartupUpdateCheckDispatch::Failed;
        }
    } ports;
    GlobalPreferencesSettingsGuard globalPreferences;
    Platform::SettingsManagerAutomaticUpdatePreferencesPort automatic;
    Platform::SettingsManagerSkippedUpdateVersionPort skipped;
    QVERIFY(automatic.write({false}));
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light));
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(expectedEnglishLocale(), QStringLiteral("light")));
    int factoryCalls = 0;
    QPointer<Ui::ApplicationShell> shell;
    Bootstrap::ApplicationBootstrap bootstrap(
        *qApp, {}, silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()), std::nullopt, {},
        [&shell, &ports, &automatic, &skipped, &factoryCalls] {
            ++factoryCalls;
            return std::make_unique<Bootstrap::UpdateCoordinator>(
                ports, ports, automatic, skipped, true, true, shell.data());
        });
    QVERIFY(bootstrap.start().succeeded());
    shell = bootstrap.shell();
    auto* action = shell->findChild<QAction*>(QStringLiteral("classMngrNextCheckForUpdates"));
    QVERIFY(action && action->isEnabled());
    action->trigger();
    QCOMPARE(factoryCalls, 1);
    QCOMPARE(ports.manualCalls, 1);
    QPointer<UpdateDialog> dialog = shell->findChild<UpdateDialog*>();
    QVERIFY(dialog && dialog->isVisible());
    QCOMPARE(dialog->findChild<QLabel*>(QStringLiteral("programUpdateTitle"))->text(),
        QStringLiteral("Update Check Failed"));
    QVERIFY(dialog->findChild<QLabel*>(QStringLiteral("programUpdateDetails"))->text()
        .contains(QStringLiteral("Offline Next updater fixture")));
    QCoreApplication::processEvents();
    QCOMPARE(factoryCalls, 1);
    QCOMPARE(ports.startupCalls, 0); // Disabled preference gates only startup.
    dialog->findChild<QPushButton*>(QStringLiteral("updateCheckButton"))->click();
    QCOMPARE(ports.manualCalls, 2);
    action->trigger();
    QCOMPARE(ports.manualCalls, 3);
    QCOMPARE(factoryCalls, 1);
    QCOMPARE(shell->findChildren<UpdateDialog*>().size(), 1);
    static_cast<void>(bootstrap.shutdown());
    QVERIFY(!dialog);
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
    auto* const navigationTree = bootstrap.shell()->findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(navigationTree);
    QTreeWidgetItem* documentRoot = nullptr;
    QTreeWidgetItemIterator documentItem(navigationTree);
    int documentLeafCount = 0;
    while (*documentItem)
    {
        const int kind = (*documentItem)->data(
            0,
            static_cast<int>(Ui::DocumentCatalogItemRole::Kind)
            ).toInt();
        if (kind == static_cast<int>(Ui::DocumentCatalogItemKind::Root))
        {
            documentRoot = *documentItem;
        }
        else if (kind == static_cast<int>(Ui::DocumentCatalogItemKind::Document))
        {
            ++documentLeafCount;
        }
        ++documentItem;
    }
    QVERIFY(documentRoot);
    QCOMPARE(documentRoot->childCount(), 7);
    QCOMPARE(documentLeafCount, 30);
    QVERIFY(!QFile::exists(QDir(resources.resourceRoot()).filePath(
        QStringLiteral("assets/documents/Guides/DYB Lesson Planning Guide.pdf")
        )));
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

void ApplicationBootstrapTests::preferencesMenuRoutePersistsAndReopensEveryPreference()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    GlobalPreferencesSettingsGuard globalPreferences;
    StartupPreferencesGuard startupPreferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::ThemePreference::Light), 0);
    QVERIFY(Platform::SettingsManagerAutomaticUpdatePreferencesPort().write(
        {.automaticChecksEnabled = false}));

    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")));
    QVERIFY(resources.addTranslation(QStringLiteral("ko_KR")));
    QVERIFY(resources.addTheme(QStringLiteral("dark")));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("preferences-test")};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    Bootstrap::ApplicationBootstrap bootstrap(
        application, {}, silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        QStringList{QStringLiteral("en-US")}, std::move(seams));
    QVERIFY(bootstrap.start().succeeded());

    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    auto* const action = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextPreferences"));
    auto* const navigationTree = shell->findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree"));
    QVERIFY(action && navigationTree);
    shell->resize(560, 480);
    navigationTree->setFixedWidth(180);
    QCOMPARE(action->text(), QStringLiteral("Preferences…"));
    const int originalFontPointSize = application.font().pointSize();

    const auto saveProbeId = Ui::PageId::fromString(
        QStringLiteral("preferences_save_mode_probe"));
    QVERIFY(saveProbeId.has_value());
    Ui::PageDescriptor saveProbeDescriptor{
        .id = *saveProbeId,
        .label = QStringLiteral("Save Mode Probe"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                new PreferencesSaveModeProbePage(context));
        }
    };
    QVERIFY(shell->pageHost().registerPage(std::move(saveProbeDescriptor)));
    QVERIFY(shell->pageHost().activatePage(saveProbeId->value()));
    auto* const saveProbe = dynamic_cast<PreferencesSaveModeProbePage*>(
        shell->pageHost().pageInstance(saveProbeId->value()));
    QVERIFY(saveProbe);

    bool interacted = false;
    QTimer::singleShot(0, [&] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QStringLiteral("nextPreferencesDialog"))
        {
            return;
        }
        const auto findCombo = [dialog](const QString& name) {
            return dialog->findChild<QComboBox*>(name);
        };
        auto* const theme = findCombo(QStringLiteral("preferencesTheme"));
        auto* const language = findCombo(QStringLiteral("preferencesLanguage"));
        auto* const fontSize = findCombo(QStringLiteral("preferencesFontSize"));
        auto* const saveMode = findCombo(QStringLiteral("preferencesSaveMode"));
        auto* const timeout = findCombo(QStringLiteral("preferencesExcelImportTimeout"));
        auto* const spacing = findCombo(QStringLiteral("preferencesDocumentSpacing"));
        auto* const background = findCombo(QStringLiteral("preferencesDocumentBackground"));
        auto* const provider = findCombo(QStringLiteral("preferencesAiCommentProvider"));
        auto* const voice = findCombo(QStringLiteral("preferencesAiCommentVoice"));
        auto* const updates = dialog->findChild<QCheckBox*>(
            QStringLiteral("preferencesAutomaticUpdates"));
        auto* const tooltips = dialog->findChild<QCheckBox*>(
            QStringLiteral("preferencesSidebarTooltips"));
        auto* const animation = dialog->findChild<QCheckBox*>(
            QStringLiteral("preferencesSidebarAnimation"));
        auto* const tabs = dialog->findChild<QTabWidget*>(
            QStringLiteral("preferencesTabs"));
        auto* const customWebsite = dialog->findChild<QLabel*>(
            QStringLiteral("preferencesCustomAiWebsiteUrl"));
        if (!theme || !language || !fontSize || !saveMode || !timeout
            || !spacing || !background || !provider || !voice || !updates
            || !tooltips || !animation || !tabs || !customWebsite)
        {
            return;
        }

        const auto visibleLabelMatches = [](QWidget* field,
                                            const QString& expectedText) {
            if (!field || !field->parentWidget()) return false;
            auto* const form = qobject_cast<QFormLayout*>(
                field->parentWidget()->layout());
            auto* const label = form
                ? qobject_cast<QLabel*>(form->labelForField(field))
                : nullptr;
            return field->isVisible() && label && label->isVisible()
                && label->text() == expectedText;
        };

        tabs->setCurrentIndex(1);
        QCoreApplication::processEvents();
        const bool englishAppearanceLabels =
            visibleLabelMatches(theme, QStringLiteral("Theme:"))
            && visibleLabelMatches(language, QStringLiteral("Language:"))
            && visibleLabelMatches(fontSize, QStringLiteral("Font Size:"));
        tabs->setCurrentIndex(2);
        QCoreApplication::processEvents();
        const bool englishDocumentLabels =
            visibleLabelMatches(spacing, QStringLiteral("PDF Page Spacing:"))
            && visibleLabelMatches(background, QStringLiteral("Background Color:"));
        tabs->setCurrentIndex(3);
        QCoreApplication::processEvents();
        const bool englishAiLabels =
            visibleLabelMatches(provider, QStringLiteral("Preferred AI Website:"))
            && visibleLabelMatches(voice, QStringLiteral("Comment Voice:"));
        tabs->setCurrentIndex(0);

        theme->setCurrentIndex(theme->findData(
            static_cast<int>(Application::ThemePreference::Dark)));
        const QString tooltipProbeLabel = QStringLiteral(
            "A very long Next sidebar navigation label used to exercise the live tooltip "
            "preference when its text extends well beyond the visible navigation width");
        auto* const tooltipProbeItem = new QTreeWidgetItem(
            QStringList{tooltipProbeLabel});
        navigationTree->addTopLevelItem(tooltipProbeItem);
        fontSize->setCurrentIndex(fontSize->findData(
            static_cast<int>(Application::FontSize::Large)));
        saveMode->setCurrentIndex(saveMode->findData(
            static_cast<int>(Application::SaveMode::Manual)));
        updates->setChecked(true);
        tooltips->setChecked(false);
        tooltips->setChecked(true);
        const bool tooltipAppeared = tooltipProbeItem->toolTip(0)
            == tooltipProbeLabel;
        tooltips->setChecked(false);
        const bool tooltipCleared = tooltipProbeItem->toolTip(0).isEmpty();
        animation->setChecked(false);
        timeout->setCurrentIndex(timeout->findData(300));
        spacing->setCurrentIndex(spacing->findData(
            static_cast<int>(Application::DocumentPageSpacing::Large)));
        background->setCurrentIndex(background->findData(
            static_cast<int>(Application::DocumentViewerBackground::Black)));
        voice->setCurrentIndex(voice->findData(
            static_cast<int>(Application::AiCommentVoice::ThirdPerson)));

        QTimer::singleShot(0, [] {
            auto* const input = qobject_cast<QInputDialog*>(
                QApplication::activeModalWidget());
            if (!input)
            {
                return;
            }
            if (auto* const line = input->findChild<QLineEdit*>())
            {
                line->setText(QStringLiteral("https://school.example/ai"));
            }
            input->accept();
        });
        provider->setCurrentIndex(provider->findData(
            static_cast<int>(Application::AiCommentProvider::CustomWebsite)));
        QCoreApplication::processEvents();

        tabs->setCurrentIndex(3);
        QCoreApplication::processEvents();
        const bool englishCustomWebsiteSummary = customWebsite->isVisible()
            && customWebsite->text()
                == QStringLiteral("Custom website: https://school.example/ai");

        language->setCurrentIndex(language->findData(
            static_cast<int>(Application::LanguagePreference::Korean)));
        QCoreApplication::processEvents();
        tabs->setCurrentIndex(1);
        QCoreApplication::processEvents();
        const bool koreanAppearanceLabels =
            visibleLabelMatches(theme, QStringLiteral("테마:"))
            && visibleLabelMatches(language, QStringLiteral("언어:"))
            && visibleLabelMatches(fontSize, QStringLiteral("글꼴 크기:"));
        tabs->setCurrentIndex(2);
        QCoreApplication::processEvents();
        const bool koreanDocumentLabels =
            visibleLabelMatches(spacing, QStringLiteral("PDF 페이지 간격:"))
            && visibleLabelMatches(background, QStringLiteral("배경색:"));
        tabs->setCurrentIndex(3);
        QCoreApplication::processEvents();
        const bool koreanAiLabels =
            visibleLabelMatches(provider, QStringLiteral("선호하는 AI 웹사이트:"))
            && visibleLabelMatches(voice, QStringLiteral("댓글 어조:"));
        const bool koreanCustomWebsiteSummary = customWebsite->isVisible()
            && customWebsite->text()
                == QStringLiteral("사용자 지정 웹사이트: https://school.example/ai");

        interacted = englishAppearanceLabels && englishDocumentLabels
            && englishAiLabels && koreanAppearanceLabels && koreanDocumentLabels
            && koreanAiLabels && englishCustomWebsiteSummary
            && koreanCustomWebsiteSummary
            && action->text() == QStringLiteral("환경 설정…")
            && dialog->windowTitle() == QStringLiteral("환경 설정")
            && tooltipAppeared && tooltipCleared
            && !navigationTree->property("sidebarAnimationEnabled").toBool()
            && application.styleSheet().contains(QStringLiteral("#2b2b2b"))
            && application.font().pointSize() > originalFontPointSize
            && saveProbe->mode == Application::SaveMode::Manual;
        if (!interacted)
        {
            qWarning() << "Preferences route checks" << action->text()
                       << dialog->windowTitle()
                       << englishAppearanceLabels << englishDocumentLabels
                       << englishAiLabels << koreanAppearanceLabels
                       << koreanDocumentLabels << koreanAiLabels
                       << englishCustomWebsiteSummary << koreanCustomWebsiteSummary
                       << tooltipAppeared << tooltipCleared
                       << navigationTree->property("sidebarAnimationEnabled")
                       << application.styleSheet().contains(QStringLiteral("#2b2b2b"))
                       << application.font().pointSize() << originalFontPointSize
                       << static_cast<int>(saveProbe->mode);
        }
        dialog->accept();
    });
    action->trigger();
    QVERIFY(interacted);
    QCOMPARE(saveProbe->mode, Application::SaveMode::Manual);
    QVERIFY(saveProbe->updates > 0);
    QCOMPARE(SettingsManager::instance().excelImportTimeoutSeconds(), 300);
    QCOMPARE(Platform::SettingsManagerExcelImportTimeoutPort().read()
                 .normalizedSeconds() * 1000,
             300'000);

    Platform::SettingsManagerPreferencesPort storedPreferences;
    Application::PreferencesModel reloaded(storedPreferences, storedPreferences);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.snapshot().themePreference(), Application::ThemePreference::Dark);
    QCOMPARE(reloaded.snapshot().languagePreference(), Application::LanguagePreference::Korean);
    QCOMPARE(reloaded.snapshot().fontSize(), Application::FontSize::Large);
    QCOMPARE(reloaded.snapshot().automaticUpdateChecksEnabled(), true);
    QCOMPARE(reloaded.snapshot().sidebarTooltipsEnabled(), false);
    QCOMPARE(reloaded.snapshot().sidebarMarqueeEnabled(), false);
    QCOMPARE(reloaded.snapshot().excelImportTimeoutSeconds(), 300);
    QCOMPARE(Platform::SettingsManagerSaveModePreferencesPort().read(),
        Application::SaveMode::Manual);
    QCOMPARE(Platform::SettingsManagerDocumentPageSpacingPreferencesPort().read(),
        Application::DocumentPageSpacing::Large);
    QCOMPARE(Platform::SettingsManagerDocumentViewerBackgroundPreferencesPort().read(),
        Application::DocumentViewerBackground::Black);
    QCOMPARE(Platform::SettingsManagerAiCommentProviderPreferencesPort().read(),
        Application::AiCommentProvider::CustomWebsite);
    QCOMPARE(Platform::SettingsManagerAiCommentVoicePreferencesPort().read(),
        Application::AiCommentVoice::ThirdPerson);
    QCOMPARE(Platform::SettingsManagerAiCommentCustomWebsitePort().read(),
        std::string("https://school.example/ai"));

    bool reopenedPersistedValues = false;
    QTimer::singleShot(0, [&] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QStringLiteral("nextPreferencesDialog"))
        {
            return;
        }
        const auto findCombo = [dialog](const QString& name) {
            return dialog->findChild<QComboBox*>(name);
        };
        auto* const theme = findCombo(QStringLiteral("preferencesTheme"));
        auto* const language = findCombo(QStringLiteral("preferencesLanguage"));
        auto* const fontSize = findCombo(QStringLiteral("preferencesFontSize"));
        auto* const saveMode = findCombo(QStringLiteral("preferencesSaveMode"));
        auto* const timeout = findCombo(QStringLiteral("preferencesExcelImportTimeout"));
        auto* const spacing = findCombo(QStringLiteral("preferencesDocumentSpacing"));
        auto* const background = findCombo(QStringLiteral("preferencesDocumentBackground"));
        auto* const provider = findCombo(QStringLiteral("preferencesAiCommentProvider"));
        auto* const voice = findCombo(QStringLiteral("preferencesAiCommentVoice"));
        auto* const updates = dialog->findChild<QCheckBox*>(
            QStringLiteral("preferencesAutomaticUpdates"));
        auto* const tooltips = dialog->findChild<QCheckBox*>(
            QStringLiteral("preferencesSidebarTooltips"));
        auto* const animation = dialog->findChild<QCheckBox*>(
            QStringLiteral("preferencesSidebarAnimation"));
        reopenedPersistedValues = theme && language && fontSize && saveMode
            && timeout && spacing && background && provider && voice && updates
            && tooltips && animation
            && theme->currentData().toInt()
                == static_cast<int>(Application::ThemePreference::Dark)
            && language->currentData().toInt()
                == static_cast<int>(Application::LanguagePreference::Korean)
            && fontSize->currentData().toInt()
                == static_cast<int>(Application::FontSize::Large)
            && saveMode->currentData().toInt()
                == static_cast<int>(Application::SaveMode::Manual)
            && timeout->currentData().toInt() == 300
            && spacing->currentData().toInt()
                == static_cast<int>(Application::DocumentPageSpacing::Large)
            && background->currentData().toInt()
                == static_cast<int>(Application::DocumentViewerBackground::Black)
            && provider->currentData().toInt()
                == static_cast<int>(Application::AiCommentProvider::CustomWebsite)
            && voice->currentData().toInt()
                == static_cast<int>(Application::AiCommentVoice::ThirdPerson)
            && updates->isChecked() && !tooltips->isChecked()
            && !animation->isChecked();
        if (updates)
        {
            updates->setChecked(false);
        }
        dialog->reject();
    });
    action->trigger();
    QVERIFY(reopenedPersistedValues);

    QVERIFY(!Platform::SettingsManagerAutomaticUpdatePreferencesPort()
        .read().automaticChecksEnabled);
    RecordingAutomaticStartupCheckPort startupCheckPort;
    Application::StartupUpdateCheckUseCase startupCheck(startupCheckPort);
    const auto disabledDisposition = startupCheck.start({
        .startupComplete = true,
        .checkOnStartup = true,
        .preferences = Platform::SettingsManagerAutomaticUpdatePreferencesPort().read(),
        .hasReleasesApiUrl = true
    }, [](Application::StartupUpdateCheckOutcome) {});
    QCOMPARE(disabledDisposition,
        Application::StartupUpdateCheckDisposition::NotEligible);
    QCOMPARE(startupCheckPort.calls, 0);

    QVERIFY(Platform::SettingsManagerAutomaticUpdatePreferencesPort().write(
        {.automaticChecksEnabled = true}));
    QVERIFY(Platform::SettingsManagerAutomaticUpdatePreferencesPort()
        .read().automaticChecksEnabled);
    RecordingAutomaticStartupCheckPort enabledStartupCheckPort;
    Application::StartupUpdateCheckUseCase enabledStartupCheck(
        enabledStartupCheckPort);
    const auto enabledDisposition = enabledStartupCheck.start({
        .startupComplete = true,
        .checkOnStartup = true,
        .preferences = Platform::SettingsManagerAutomaticUpdatePreferencesPort().read(),
        .hasReleasesApiUrl = true
    }, [](Application::StartupUpdateCheckOutcome) {});
    QCOMPARE(enabledDisposition,
        Application::StartupUpdateCheckDisposition::Started);
    QCOMPARE(enabledStartupCheckPort.calls, 1);

    application.setStyleSheet(previousStyleSheet);
    QVERIFY(bootstrap.shutdown().shutdownRequested);
}

void ApplicationBootstrapTests::preferencesRouteRetranslatesActiveClassesNavigator()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    GlobalPreferencesSettingsGuard globalPreferences;
    StartupPreferencesGuard startupPreferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::ThemePreference::Light), 0);
    QVERIFY(Platform::SettingsManagerAutomaticUpdatePreferencesPort().write(
        {.automaticChecksEnabled = false}));

    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")));
    QVERIFY(resources.addTranslation(QStringLiteral("ko_KR")));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString workspacePath = temporaryDirectory.filePath(
        QStringLiteral("preferences-language-classes.tps"));
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
        workspacePath, QStringLiteral("representative_startup.sql"),
        &fixtureError), qPrintable(fixtureError));

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("preferences-language-test"), workspacePath};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.workspaceGatewayFactory = [] {
        return std::make_unique<Platform::WorkspaceStoreGateway>();
    };
    Bootstrap::ApplicationBootstrap bootstrap(
        application, {}, silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        QStringList{QStringLiteral("en-US")}, std::move(seams));
    QVERIFY(bootstrap.start().succeeded());
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());

    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    QVERIFY(shell->pageHost().activatePage(QStringLiteral("classes")));
    auto* const page = dynamic_cast<Ui::ClassesNavigatorPage*>(
        shell->pageHost().pageInstance(QStringLiteral("classes")));
    QVERIFY(page);
    QVERIFY(page->isActive());
    QVERIFY(page->isVisible());

    QLabel* const pageTitle = page->findChild<QLabel*>(
        QStringLiteral("classesNavigatorTitle"));
    QVERIFY(pageTitle);
    QLabel* scheduleControlLabel = nullptr;
    for (QLabel* const label : page->findChildren<QLabel*>())
    {
        if (label->property("translationSource").toString()
            == QStringLiteral("Schedule"))
        {
            scheduleControlLabel = label;
            break;
        }
    }
    QVERIFY(scheduleControlLabel);
    QVERIFY(scheduleControlLabel->isVisible());

    QTreeWidget* const navigationTree = shell->findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree"));
    QVERIFY(navigationTree);
    const auto sidebarClassesText = [navigationTree]() {
        QTreeWidgetItemIterator item(navigationTree);
        while (*item)
        {
            if ((*item)->data(0, Qt::UserRole + 1).toString()
                == QStringLiteral("classes"))
            {
                return (*item)->text(0);
            }
            ++item;
        }
        return QString{};
    };
    QVERIFY(navigationTree->isVisible());
    QCOMPARE(sidebarClassesText(), QStringLiteral("Classes"));
    QCOMPARE(pageTitle->text(), QStringLiteral("Classes"));
    QCOMPARE(scheduleControlLabel->text(), QStringLiteral("Schedule"));

    QAction* const preferencesAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextPreferences"));
    QVERIFY(preferencesAction);
    bool liveLanguageSwitchesCorrectly = false;
    QString liveSwitchDiagnostic;
    QTimer::singleShot(0, [&] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog
            || dialog->objectName() != QStringLiteral("nextPreferencesDialog"))
        {
            liveSwitchDiagnostic = QStringLiteral(
                "The production Preferences dialog was not opened.");
            if (dialog) dialog->reject();
            return;
        }
        auto* const language = dialog->findChild<QComboBox*>(
            QStringLiteral("preferencesLanguage"));
        if (!language)
        {
            liveSwitchDiagnostic = QStringLiteral(
                "The Preferences language control was not found.");
            dialog->reject();
            return;
        }

        const int englishIndex = language->findData(
            static_cast<int>(Application::LanguagePreference::English));
        const int koreanIndex = language->findData(
            static_cast<int>(Application::LanguagePreference::Korean));
        if (englishIndex < 0 || koreanIndex < 0)
        {
            liveSwitchDiagnostic = QStringLiteral(
                "The English and Korean choices were not available.");
            dialog->reject();
            return;
        }
        const bool openedInEnglish = language->currentData().toInt()
            == static_cast<int>(Application::LanguagePreference::English);

        language->setCurrentIndex(koreanIndex);
        QCoreApplication::processEvents();
        QTreeWidgetItem* koreanSidebarItem = nullptr;
        QTreeWidgetItemIterator koreanItem(navigationTree);
        while (*koreanItem)
        {
            if ((*koreanItem)->data(0, Qt::UserRole + 1).toString()
                == QStringLiteral("classes"))
            {
                koreanSidebarItem = *koreanItem;
                break;
            }
            ++koreanItem;
        }
        const QString koreanSidebarText = koreanSidebarItem
            ? koreanSidebarItem->text(0) : QString{};
        const bool koreanSidebarVisible = koreanSidebarItem
            && !koreanSidebarItem->isHidden();
        const QString koreanPageTitleText = pageTitle->text();
        const QString koreanControlLabelText = scheduleControlLabel->text();
        const bool koreanTexts = language->currentData().toInt()
                == static_cast<int>(Application::LanguagePreference::Korean)
            && koreanSidebarVisible
            && koreanSidebarText == QStringLiteral("수업")
            && pageTitle->isVisible()
            && koreanPageTitleText == QStringLiteral("수업")
            && scheduleControlLabel->isVisible()
            && koreanControlLabelText == QStringLiteral("일정");

        language->setCurrentIndex(englishIndex);
        QCoreApplication::processEvents();
        const QString englishSidebarText = sidebarClassesText();
        const QString englishPageTitleText = pageTitle->text();
        const QString englishControlLabelText = scheduleControlLabel->text();
        const bool englishTextsRestored = language->currentData().toInt()
                == static_cast<int>(Application::LanguagePreference::English)
            && englishSidebarText == QStringLiteral("Classes")
            && pageTitle->isVisible()
            && englishPageTitleText == QStringLiteral("Classes")
            && scheduleControlLabel->isVisible()
            && englishControlLabelText == QStringLiteral("Schedule");

        liveLanguageSwitchesCorrectly = openedInEnglish && koreanTexts
            && englishTextsRestored;
        if (!liveLanguageSwitchesCorrectly)
        {
            liveSwitchDiagnostic = QStringLiteral(
                "openedEnglish=%1 korean=%2 [sidebar=%3, title=%4, control=%5] "
                "englishRestored=%6 [sidebar=%7, title=%8, control=%9]")
                .arg(openedInEnglish)
                .arg(koreanTexts)
                .arg(koreanSidebarText)
                .arg(koreanPageTitleText)
                .arg(koreanControlLabelText)
                .arg(englishTextsRestored)
                .arg(englishSidebarText)
                .arg(englishPageTitleText)
                .arg(englishControlLabelText);
        }
        dialog->accept();
    });
    preferencesAction->trigger();
    QVERIFY2(liveLanguageSwitchesCorrectly,
        qPrintable(liveSwitchDiagnostic));

    Platform::SettingsManagerPreferencesPort storedPreferences;
    Application::PreferencesModel reloaded(storedPreferences, storedPreferences);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.snapshot().languagePreference(),
        Application::LanguagePreference::English);

    bool reopenedLanguagePersisted = false;
    QTimer::singleShot(0, [&] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog
            || dialog->objectName() != QStringLiteral("nextPreferencesDialog"))
        {
            if (dialog) dialog->reject();
            return;
        }
        auto* const language = dialog->findChild<QComboBox*>(
            QStringLiteral("preferencesLanguage"));
        reopenedLanguagePersisted = language
            && language->currentData().toInt()
                == static_cast<int>(Application::LanguagePreference::English);
        dialog->reject();
    });
    preferencesAction->trigger();
    QVERIFY(reopenedLanguagePersisted);

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::preferencesMenuRouteVisualCapture_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<int>("storedLanguage");
    QTest::addColumn<QString>("theme");
    QTest::addColumn<int>("storedTheme");
    QTest::addColumn<QString>("captureName");
    QTest::newRow("english-light")
        << QStringLiteral("en_US") << 1 << QStringLiteral("light") << 1
        << QStringLiteral("english-light.png");
    QTest::newRow("english-dark")
        << QStringLiteral("en_US") << 1 << QStringLiteral("dark") << 0
        << QStringLiteral("english-dark.png");
    QTest::newRow("korean-light")
        << QStringLiteral("ko_KR") << 5 << QStringLiteral("light") << 1
        << QStringLiteral("korean-light.png");
    QTest::newRow("korean-dark")
        << QStringLiteral("ko_KR") << 5 << QStringLiteral("dark") << 0
        << QStringLiteral("korean-dark.png");
}

void ApplicationBootstrapTests::preferencesMenuRouteVisualCapture()
{
    QFETCH(QString, locale);
    QFETCH(int, storedLanguage);
    QFETCH(QString, theme);
    QFETCH(int, storedTheme);
    QFETCH(QString, captureName);

    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    GlobalPreferencesSettingsGuard globalPreferences;
    StartupPreferencesGuard startupPreferences(storedLanguage, storedTheme, 0);
    QVERIFY(Platform::SettingsManagerAutomaticUpdatePreferencesPort().write(
        {.automaticChecksEnabled = false}));
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(locale, theme));
    if (locale != QStringLiteral("ko_KR"))
    {
        QVERIFY(resources.addTranslation(QStringLiteral("ko_KR")));
    }
    if (theme != QStringLiteral("dark"))
    {
        QVERIFY(resources.addTheme(QStringLiteral("dark")));
    }

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("preferences-capture")};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    Bootstrap::ApplicationBootstrap bootstrap(
        application, {}, silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        QStringList{QStringLiteral("en-US")}, std::move(seams));
    QVERIFY(bootstrap.start().succeeded());
    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    QAction* const action = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextPreferences"));
    QVERIFY(action);
    QCOMPARE(action->text(), locale == QStringLiteral("ko_KR")
        ? QStringLiteral("환경 설정…") : QStringLiteral("Preferences…"));

    const QString outputDirectory = qEnvironmentVariable(
        "CLASSMNGR_NEXT_PREFERENCES_CAPTURE_DIR",
        QDir::current().filePath(QStringLiteral("preferences_visual_captures")));
    QVERIFY(QDir().mkpath(outputDirectory));
    const QString capturePath = QDir(outputDirectory).filePath(captureName);
    bool captured = false;
    QTimer::singleShot(0, [&] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QStringLiteral("nextPreferencesDialog"))
        {
            return;
        }
        if (auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")))
        {
            tabs->setCurrentIndex(1);
        }
        dialog->setFixedSize(680, 620);
        dialog->show();
        QCoreApplication::processEvents();
        captured = dialog->grab().save(capturePath, "PNG");
        dialog->reject();
    });
    action->trigger();
    QVERIFY2(captured, qPrintable(QStringLiteral("could not capture %1")
        .arg(capturePath)));
    QVERIFY(QFileInfo(capturePath).size() > 0);
    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
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
    const QString visualCaptureDirectory = qEnvironmentVariable(
        "CLASSMNGR_INITIAL_SETUP_VISUAL_CAPTURE_DIR");
    const bool visualCaptureEnabled = !visualCaptureDirectory.isEmpty();
    [[maybe_unused]] const ScopedCaptureAppearance captureAppearance(
        application, visualCaptureEnabled);
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
    if (visualCaptureEnabled)
    {
        const QString signaturePath = temporaryDirectory.filePath(
            QStringLiteral("visual-review-signature.png"));
        QVERIFY(visualReviewSignature().save(signaturePath, "PNG"));
        fileDialogProbe->openPath = Application::FileDialogPath(
            signaturePath.toUtf8().toStdString());
    }

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
    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    if (visualCaptureEnabled)
    {
        shell->resize(1024, 720);
        QApplication::processEvents();
    }

    Ui::InitialPage* const page = dynamic_cast<Ui::InitialPage*>(
        shell->pageHost().pageInstance(
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

    if (visualCaptureEnabled)
    {
        auto* const pageLayout = qobject_cast<QVBoxLayout*>(page->layout());
        QVERIFY(pageLayout);
        pageLayout->setContentsMargins(8, 32, 8, 32);
        pageLayout->setStretch(0, 0);
        pageLayout->setStretch(pageLayout->count() - 1, 0);
        pageLayout->activate();

        auto* const personalName = page->findChild<QLineEdit*>(
            QStringLiteral("classMngrNextInitialSetupPersonalName"));
        QVERIFY(personalName);
        personalName->setText(QStringLiteral("Alex Kim"));
        chooseSignature->click();
        QApplication::processEvents();
        QVERIFY(!preview->pixmap().isNull());
        if (theme == QStringLiteral("dark"))
        {
            const QImage renderedPreview = preview->grab().toImage();
            QVERIFY(!renderedPreview.isNull());
            QVERIFY(renderedPreview.pixelColor(8, 8).lightness() > 240);

            bool hasDarkSignatureInk = false;
            const QRect signatureSearchArea = renderedPreview.rect().adjusted(
                36, 20, -36, -20);
            for (int y = signatureSearchArea.top();
                 y <= signatureSearchArea.bottom() && !hasDarkSignatureInk;
                 ++y)
            {
                for (int x = signatureSearchArea.left();
                     x <= signatureSearchArea.right();
                     ++x)
                {
                    const QColor pixel = renderedPreview.pixelColor(x, y);
                    if (pixel.red() < 96
                        && pixel.green() < 96
                        && pixel.blue() < 96)
                    {
                        hasDarkSignatureInk = true;
                        break;
                    }
                }
            }
            QVERIFY(hasDarkSignatureInk);
        }

        auto* const profileLocation = page->findChild<QLabel*>(
            QStringLiteral("classMngrNextInitialSetupProfileLocation"));
        QVERIFY(profileLocation);
        const QString profileLabelPrefix = profileLocation->text().section(
            QLatin1Char(':'), 0, 0);
        profileLocation->setText(
            profileLabelPrefix + QStringLiteral(": setup-profile.tps"));
        QVERIFY(!profileLocation->text().contains(temporaryDirectory.path()));

        personalName->setFocus(Qt::OtherFocusReason);
        shell->activateWindow();
        shell->repaint();
        QApplication::processEvents();
        QCOMPARE(shell->size(), QSize(1024, 720));
        const QImage capture = fixedViewportCapture(*shell);
        QVERIFY(!capture.isNull());
        QVERIFY(QDir().mkpath(visualCaptureDirectory));
        const QString filename = QStringLiteral(
            "next-initial-setup-personal-details-%1-%2.png")
            .arg(locale, theme);
        QVERIFY(capture.save(
            QDir(visualCaptureDirectory).filePath(filename), "PNG"));
    }

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
        {"my_classes", "My Classes", "내 수업"},
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
        QVERIFY((*item)->icon(0).isNull());
        if ((*item)->data(
                0,
                static_cast<int>(Ui::DocumentCatalogItemRole::Kind)
                ).isValid())
        {
            continue;
        }
        treeLabels.push_back((*item)->text(0));
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
    QTreeWidgetItem* documentRoot = nullptr;
    for (QTreeWidgetItemIterator item(tree); *item; ++item)
    {
        const QVariant documentKind = (*item)->data(
                0,
                static_cast<int>(Ui::DocumentCatalogItemRole::Kind)
                );
        if (documentKind.isValid()
            && documentKind.toInt()
            == static_cast<int>(Ui::DocumentCatalogItemKind::Root))
        {
            documentRoot = *item;
            break;
        }
    }
    QVERIFY(documentRoot);
    QCOMPARE(documentRoot->text(0), korean ? QStringLiteral("문서") : QStringLiteral("Documents"));
    QCOMPARE(documentRoot->child(0)->text(0), korean ? QStringLiteral("안내서") : QStringLiteral("Guides"));
    QCOMPARE(documentRoot->child(0)->child(0)->text(0),
             korean ? QStringLiteral("수업 계획") : QStringLiteral("Lesson Planning"));
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
    RecordingBirthdayBootstrapWorkspaceGateway* birthdayGateway = nullptr;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = journalPath;
    seams.workspaceGatewayFactory = [gatewayProbe, &birthdayGateway] {
        auto result = std::make_unique<RecordingBirthdayBootstrapWorkspaceGateway>(
            gatewayProbe);
        birthdayGateway = result.get();
        return result;
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
    QAction* const upcomingBirthdaysAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextUpcomingBirthdays")
        );
    QVERIFY(openAction);
    QVERIFY(upcomingBirthdaysAction);
    QVERIFY(!upcomingBirthdaysAction->isEnabled());

    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 1);
    QVERIFY(fileDialogProbe->lastOpenRequest.has_value());
    QCOMPARE(fileDialogProbe->lastOpenRequest->title,
             std::string("Open Teacher Profile"));
    QCOMPARE(fileDialogProbe->lastOpenRequest->nameFilters,
             std::vector<std::string>{"ClassMngr Teacher Profile (*.tps *.db)"});
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
    QVERIFY(upcomingBirthdaysAction->isEnabled());
    QVERIFY(bootstrap.shell()->navigationModel()->currentPageId().has_value());
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    QCOMPARE(historyProbe->saves.back().paths.front(), firstNormalized);
    QCOMPARE(historyProbe->saves.back().paths.at(1),
             historyProbe->history.paths.front());

    const Application::WorkspaceSession firstSession =
        *bootstrap.workspaceStateSnapshot().session();
    int birthdayDialogCount = 0;
    bool birthdayDismissalStartedUnchecked = false;
    QTimer::singleShot(0, &application, [&] {
        QDialog* const dialog = qobject_cast<QDialog*>(
            application.activeModalWidget());
        if (!dialog
            || dialog->objectName() != QStringLiteral("nextUpcomingBirthdaysDialog"))
        {
            return;
        }
        ++birthdayDialogCount;
        if (auto* const check = dialog->findChild<QCheckBox*>(
                QStringLiteral("nextUpcomingBirthdaysDismissForTodayCheck")))
        {
            birthdayDismissalStartedUnchecked = !check->isChecked();
        }
        dialog->reject();
    });
    upcomingBirthdaysAction->trigger();
    QCOMPARE(birthdayDialogCount, 1);
    QVERIFY(birthdayDismissalStartedUnchecked);
    QVERIFY(birthdayGateway);
    QCOMPARE(birthdayGateway->koreanReads, 1);
    QCOMPARE(birthdayGateway->nativeEnglishReads, 1);
    QCOMPARE(birthdayGateway->gsTeamReads, 1);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session(),
             std::optional<Application::WorkspaceSession>(firstSession));
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(gatewayProbe->saveCalls, 0);
    QCOMPARE(gatewayProbe->discardCalls, 0);
    QCOMPARE(gatewayProbe->closeCalls, 0);

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

    QVERIFY(bootstrap.shell()->pageHost().activatePage(QStringLiteral("my_classes")));
    auto* const myClasses = dynamic_cast<Ui::MyClassesPage*>(
        bootstrap.shell()->pageHost().pageInstance(QStringLiteral("my_classes")));
    QVERIFY(myClasses);
    auto* const myClassesLoadState = myClasses->findChild<QLabel*>(
        QStringLiteral("myClassesLoadState"));
    QVERIFY(myClassesLoadState);
    QCOMPARE(myClassesLoadState->text(),
             QStringLiteral("The active workspace store is unavailable."));

    gatewayProbe->openError.reset();
    openAction->trigger();
    QCOMPARE(gatewayProbe->openCalls, 3);
    QCOMPARE(historyProbe->saveCount, 2);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             failedNormalized);
    QCOMPARE(historyProbe->saves.back().paths.front(), failedNormalized);
    QCOMPARE(bootstrap.shell()->pageHost().activePageKey(),
             std::optional<QString>(QStringLiteral("my_classes")));
    QCOMPARE(myClassesLoadState->text(),
             QStringLiteral("The active workspace store is unavailable."));

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
speakingEvaluationAiCompositionUsesWorkspaceAndPreferences()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard startupPreferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light));
    SpeakingEvaluationAiPreferencesGuard aiPreferences;
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::AiCommentProvider), 4);
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::AiCommentVoice), 1);
    const QString customWebsite = QStringLiteral(
        "https://local-ai.example/speaking");
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::AiCommentCustomWebsiteUrl),
        customWebsite);

    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")));
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString workspacePath = temporaryDirectory.filePath(
        QStringLiteral("speaking-evaluation-ai.tps"));
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
        workspacePath, QStringLiteral("representative_startup.sql"),
        &fixtureError), qPrintable(fixtureError));
    QVERIFY(executeWorkspaceSql(workspacePath, QStringLiteral(
        "UPDATE class_info SET class_grade='E4' WHERE class_id=1")));

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), workspacePath};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.workspaceGatewayFactory = [] {
        return std::make_unique<Platform::WorkspaceStoreGateway>();
    };
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        std::nullopt,
        std::move(seams));
    QVERIFY(bootstrap.start().succeeded());
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QVERIFY(bootstrap.shell()->pageHost().activatePage(
        QStringLiteral("speaking_evaluation")));
    auto* const page = dynamic_cast<Ui::SpeakingEvaluationPage*>(
        bootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("speaking_evaluation")));
    QVERIFY(page);
    QVERIFY(page->matrixModel()->hasMatrix());
    QVERIFY(page->selectedClassId().has_value());
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QVERIFY(page->matrixModel()->setData(
        page->matrixModel()->index(0, 1), QStringLiteral("Ada Park")));
    QVERIFY(page->matrixModel()->setData(
        page->matrixModel()->index(0, 10),
        QStringLiteral("[Did Well]\nClear ideas\n"
                       "[Needs Improvement]\nPractice past tense")));
    page->tableView()->setCurrentIndex(page->matrixModel()->index(0, 9));

    CapturedSpeakingEvaluationAiUrlHandler urlHandler;
    SpeakingEvaluationAiHttpsUrlHandlerRegistration urlHandlerRegistration(
        &urlHandler);
    bool dialogSeen = false;
    QString prompt;
    QString copyOpenLabel;
    QTimer::singleShot(0, [&] {
        auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationAiPromptDialog*>(
            qApp->activeModalWidget());
        if (!dialog)
        {
            return;
        }
        dialogSeen = true;
        prompt = dialog->promptEditor()->toPlainText();
        copyOpenLabel = dialog->copyAndOpenButton()->text();
        dialog->copyAndOpenButton()->click();
        dialog->reject();
    });
    page->aiCommentButton()->click();

    QVERIFY(dialogSeen);
    QVERIFY(prompt.contains(QStringLiteral("4th-grade elementary ESL")));
    QVERIFY(prompt.contains(QStringLiteral("Write for a parent or guardian")));
    QVERIFY(!prompt.contains(QStringLiteral("Ada Park")));
    QCOMPARE(copyOpenLabel,
        QStringLiteral("Copy Prompt and Open Custom AI Website"));
    QCOMPARE(urlHandler.urls.size(), 1);
    QCOMPARE(urlHandler.urls.front(), QUrl(customWebsite));
    QVERIFY(page->discardChanges().has_value());
    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
myClassesReloadsRealWorkspaceDataWhenProfileIsReplacedWhileActive()
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
    const QString firstPath = temporaryDirectory.filePath(
        QStringLiteral("my-classes-first.tps"));
    const QString secondPath = temporaryDirectory.filePath(
        QStringLiteral("my-classes-second.tps"));
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
                 firstPath, QStringLiteral("representative_startup.sql"),
                 &fixtureError), qPrintable(fixtureError));
    QVERIFY2(createWorkspaceWithFixture(
                 secondPath, QStringLiteral("representative_startup.sql"),
                 &fixtureError), qPrintable(fixtureError));
    QVERIFY(executeWorkspaceSql(firstPath, QStringLiteral(
        "UPDATE classes SET name='Store A Class' WHERE id=1")));
    QVERIFY(executeWorkspaceSql(firstPath, QStringLiteral(
        "UPDATE class_info SET class_level='Store A Level', "
        "notes='Store A class notes' WHERE class_id=1")));
    QVERIFY(executeWorkspaceSql(firstPath, QStringLiteral(
        "UPDATE teachers SET teacher_en='Store A Teacher', "
        "preferred_name='Store A Teacher', room_number='A-101', "
        "notes='Store A teacher notes' WHERE id=1")));
    QVERIFY(executeWorkspaceSql(secondPath, QStringLiteral(
        "UPDATE classes SET name='Store B Class' WHERE id=1")));
    QVERIFY(executeWorkspaceSql(secondPath, QStringLiteral(
        "UPDATE class_info SET class_level='Store B Level', "
        "notes='Store B class notes' WHERE class_id=1")));
    QVERIFY(executeWorkspaceSql(secondPath, QStringLiteral(
        "UPDATE teachers SET teacher_en='Store B Teacher', "
        "preferred_name='Store B Teacher', room_number='B-202', "
        "notes='Store B teacher notes' WHERE id=1")));

    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->openPath = Application::FileDialogPath(
        secondPath.toUtf8().toStdString());
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), firstPath};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.workspaceGatewayFactory = [] {
        return std::make_unique<Platform::WorkspaceStoreGateway>();
    };
    seams.openProfileFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            fileDialogProbe);
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
    QVERIFY(bootstrap.shell()->pageHost().activatePage(
        QStringLiteral("my_classes")));
    auto* const page = dynamic_cast<Ui::MyClassesPage*>(
        bootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("my_classes")));
    QVERIFY(page);
    QCOMPARE(page->summaryModel()->summaryCount(), 8);
    const Domain::ClassId selectedId = *Domain::ClassId::fromString("1");
    QModelIndex selected = page->summaryModel()->indexForClassId(selectedId);
    QVERIFY(selected.isValid());
    page->gradeTabs()->setCurrentIndex(selected.parent().row());
    selected = page->summaryModel()->indexForClassId(selectedId);
    page->classList()->selectionModel()->setCurrentIndex(
        selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QVERIFY(page->selectedDetails().has_value());
    QCOMPARE(page->selectedDetails()->className, std::u16string(u"Store A Class"));
    QCOMPARE(page->selectedDetails()->classNotes,
             std::u16string(u"Store A class notes"));
    QVERIFY(page->selectedDetails()->teacher.has_value());
    QCOMPARE(page->selectedDetails()->teacher->englishName,
             std::u16string(u"Store A Teacher"));
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("myClassesTeacherTitle"))->text(),
             QStringLiteral("Store A Teacher - Room A-101"));
    QCOMPARE(page->findChild<QPlainTextEdit*>(
                 QStringLiteral("myClassesTeacherNotes"))->toPlainText(),
             QStringLiteral("Store A teacher notes"));

    QAction* const openAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextOpenTeacherProfile"));
    QVERIFY(openAction);
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 1);
    QCOMPARE(bootstrap.shell()->pageHost().activePageKey(),
             std::optional<QString>(QStringLiteral("my_classes")));
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             std::optional<Application::WorkspaceLocation>(
                 Application::WorkspaceLocation(
                     QFileInfo(secondPath).canonicalFilePath()
                         .toUtf8().toStdString())));
    QCOMPARE(page->summaryModel()->summaryCount(), 8);
    selected = page->summaryModel()->indexForClassId(selectedId);
    QVERIFY(selected.isValid());
    const auto* const summary = page->summaryModel()->summaryForIndex(selected);
    QVERIFY(summary);
    QCOMPARE(summary->className, std::u16string(u"Store B Class"));
    QCOMPARE(page->selectedClassId(), std::optional<Domain::ClassId>(selectedId));
    QVERIFY(page->selectedDetails().has_value());
    QCOMPARE(page->selectedDetails()->className, std::u16string(u"Store B Class"));
    QCOMPARE(page->selectedDetails()->classNotes,
             std::u16string(u"Store B class notes"));
    QVERIFY(page->selectedDetails()->teacher.has_value());
    QCOMPARE(page->selectedDetails()->teacher->englishName,
             std::u16string(u"Store B Teacher"));
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("myClassesTeacherTitle"))->text(),
             QStringLiteral("Store B Teacher - Room B-202"));
    QCOMPARE(page->findChild<QPlainTextEdit*>(
                 QStringLiteral("myClassesClassNotes"))->toPlainText(),
             QStringLiteral("Store B class notes"));
    QVERIFY(!page->findChild<QPlainTextEdit*>(
        QStringLiteral("myClassesClassNotes"))->toPlainText().contains(
            QStringLiteral("Store A")));
    QVERIFY(!page->findChild<QPlainTextEdit*>(
        QStringLiteral("myClassesTeacherNotes"))->toPlainText().contains(
            QStringLiteral("Store A")));

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
classesNavigatorClearsAndReloadsWhenWorkspaceIsReplacedWhileActive()
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
    const QString firstPath = temporaryDirectory.filePath(
        QStringLiteral("classes-navigator-first.tps"));
    const QString secondPath = temporaryDirectory.filePath(
        QStringLiteral("classes-navigator-second.tps"));
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
                 firstPath, QStringLiteral("representative_startup.sql"),
                 &fixtureError), qPrintable(fixtureError));
    QVERIFY2(createWorkspaceWithFixture(
                 secondPath, QStringLiteral("representative_startup.sql"),
                 &fixtureError), qPrintable(fixtureError));
    QVERIFY(executeWorkspaceSql(firstPath, QStringLiteral(
        "UPDATE class_info SET class_level='First Workspace Level' "
        "WHERE class_id=1")));
    QVERIFY(executeWorkspaceSql(secondPath, QStringLiteral(
        "UPDATE class_info SET class_level='Replacement Level' "
        "WHERE class_id=2")));
    QVERIFY(executeWorkspaceSql(secondPath, QStringLiteral(
        "INSERT INTO testing_classes(class_id, room) VALUES(1, 'Navigator test room')")));

    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->openPath = Application::FileDialogPath(
        secondPath.toUtf8().toStdString());
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), firstPath};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.workspaceGatewayFactory = [] {
        return std::make_unique<Platform::WorkspaceStoreGateway>();
    };
    seams.openProfileFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            fileDialogProbe);
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
    QVERIFY(bootstrap.shell()->pageHost().activatePage(
        QStringLiteral("classes")));
    auto* const page = dynamic_cast<Ui::ClassesNavigatorPage*>(
        bootstrap.shell()->pageHost().pageInstance(QStringLiteral("classes")));
    QVERIFY(page);
    QVERIFY(page->isActive());
    QCOMPARE(page->model()->classCount(), 8);

    const Domain::ClassId selectedInFirstWorkspace =
        *Domain::ClassId::fromString("1");
    const QModelIndex firstSelection =
        page->model()->indexForClassId(selectedInFirstWorkspace);
    QVERIFY(firstSelection.isValid());
    page->classList()->selectionModel()->setCurrentIndex(
        firstSelection,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
        );
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(selectedInFirstWorkspace));
    QVERIFY(page->model()->labelForClassId(selectedInFirstWorkspace).contains(
        QStringLiteral("First Workspace Level")));

    bool observedClearedProjection = false;
    bool observedReplacementProjection = false;
    QObject::connect(page->model(), &QAbstractItemModel::modelReset, page, [&] {
        if (page->model()->classCount() == 0 && !page->selectedClassId())
        {
            observedClearedProjection = true;
        }
        const auto replacementClass = Domain::ClassId::fromString("2");
        if (replacementClass
            && page->model()->classCount() == 7
            && page->model()->labelForClassId(*replacementClass).contains(
                QStringLiteral("Replacement Level")))
        {
            observedReplacementProjection = true;
        }
    });

    QAction* const openAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextOpenTeacherProfile"));
    QVERIFY(openAction);
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 1);
    QCOMPARE(bootstrap.shell()->pageHost().activePageKey(),
        std::optional<QString>(QStringLiteral("classes")));
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
        std::optional<Application::WorkspaceLocation>(
            Application::WorkspaceLocation(
                QFileInfo(secondPath).canonicalFilePath()
                    .toUtf8().toStdString())));
    QVERIFY(observedClearedProjection);
    QVERIFY(observedReplacementProjection);
    QCOMPARE(page->model()->classCount(), 7);
    QVERIFY(!page->model()->indexForClassId(selectedInFirstWorkspace).isValid());
    QVERIFY(page->selectedClassId().has_value());
    QVERIFY(*page->selectedClassId() != selectedInFirstWorkspace);
    const Domain::ClassId replacementClass = *Domain::ClassId::fromString("2");
    QVERIFY(page->model()->labelForClassId(replacementClass).contains(
        QStringLiteral("Replacement Level")));

    QVERIFY(bootstrap.shell()->pageHost().activatePage(
        QStringLiteral("my_workspace")));
    QVERIFY(!page->isActive());
    QCOMPARE(page->model()->classCount(), 0);
    QVERIFY(bootstrap.shell()->pageHost().activatePage(
        QStringLiteral("classes")));
    QVERIFY(page->isActive());
    QCOMPARE(page->model()->classCount(), 7);
    QVERIFY(!page->model()->indexForClassId(selectedInFirstWorkspace).isValid());
    QVERIFY(page->model()->labelForClassId(replacementClass).contains(
        QStringLiteral("Replacement Level")));

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
classesNavigatorClearsWhenWorkspaceIsClosedWhileActive()
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
        QStringLiteral("classes-navigator-close.tps"));
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
                 profilePath, QStringLiteral("representative_startup.sql"),
                 &fixtureError), qPrintable(fixtureError));

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), profilePath};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.workspaceGatewayFactory = [] {
        return std::make_unique<Platform::WorkspaceStoreGateway>();
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
    QVERIFY(bootstrap.shell()->pageHost().activatePage(
        QStringLiteral("classes")));
    auto* const page = dynamic_cast<Ui::ClassesNavigatorPage*>(
        bootstrap.shell()->pageHost().pageInstance(QStringLiteral("classes")));
    QVERIFY(page);
    QVERIFY(page->isActive());
    QCOMPARE(page->model()->classCount(), 8);
    QVERIFY(page->selectedClassId().has_value());

    bool observedClearedProjection = false;
    QObject::connect(page->model(), &QAbstractItemModel::modelReset, page, [&] {
        if (page->model()->classCount() == 0 && !page->selectedClassId())
        {
            observedClearedProjection = true;
        }
    });
    QAction* const closeAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextCloseWorkspace"));
    QVERIFY(closeAction);
    QVERIFY(closeAction->isEnabled());
    closeAction->trigger();

    QCOMPARE(bootstrap.workspaceStateSnapshot().lifecycle(),
        Application::WorkspaceLifecycleState::Closed);
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId()->value(),
        QStringLiteral("initial_setup"));
    QVERIFY(observedClearedProjection);
    QCOMPARE(page->model()->classCount(), 0);
    QVERIFY(!page->selectedClassId().has_value());
    QVERIFY(!page->isActive());

    QVERIFY(bootstrap.shell()->pageHost().activatePage(
        QStringLiteral("classes")));
    QVERIFY(page->isActive());
    QCOMPARE(page->model()->classCount(), 0);
    QVERIFY(!page->selectedClassId().has_value());
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"))
                 ->text(),
        QStringLiteral("No Teacher Profile is open."));

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
subPrepRouteUsesTheActiveGatewayAndReleasesAcrossLifecycle()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::ThemePreference::Light));
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        expectedEnglishLocale(), QStringLiteral("light")));
    QVERIFY(QFile::exists(QStringLiteral(":/assets/icons/combo_arrow_light.svg")));
    QVERIFY(QFile::exists(QStringLiteral(":/assets/icons/combo_arrow_dark.svg")));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("sub-prep-large-startup.tps"));
    const QString journalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
        profilePath, QStringLiteral("large_startup.sql"), &fixtureError),
        qPrintable(fixtureError));

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = {QStringLiteral("test-app"), profilePath};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = journalPath;
    seams.workspaceGatewayFactory = [] {
        return std::make_unique<Platform::WorkspaceStoreGateway>();
    };
    Bootstrap::ApplicationBootstrap bootstrap(
        application, {}, silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()), std::nullopt,
        std::move(seams));
    QVERIFY(bootstrap.start().succeeded());

    Ui::PageHost& host = bootstrap.shell()->pageHost();
    QVERIFY(host.isPageRegistered(QStringLiteral("sub_prep")));
    QVERIFY(host.activatePage(QStringLiteral("sub_prep")));
    auto* const page = dynamic_cast<Ui::SubPrepPage*>(
        host.pageInstance(QStringLiteral("sub_prep")));
    QVERIFY(page);
    QCOMPARE(host.activePageKey(), std::optional<QString>(
        QStringLiteral("sub_prep")));

    auto metrics = page->visualMetrics();
    QCOMPARE(metrics.summaryModelCount, 1);
    QCOMPARE(page->classList()->model(), page->summaryModel());
    QCOMPARE(metrics.visibleSummaryCount, 96);
    QCOMPARE(static_cast<int>(page->summaryModel()->projection().classes().size()),
             metrics.visibleSummaryCount);
    QCOMPARE(metrics.selectedDetailPanelCount, 1);
    QCOMPARE(metrics.textEditorCount, 6);
    QCOMPARE(metrics.selectedDetailReadCount, 1);
    QVERIFY(page->selectedClassId());
    QVERIFY(page->selectedDetails());
    QCOMPARE(page->selectedDetails()->classId.value(),
             page->selectedClassId()->value());

    // The compact model owns one row value for every visible summary, with
    // the active grade alone exposed through QListView at a time.
    SubPrepClassInformationListModel rowCheck;
    rowCheck.setProjection(page->summaryModel()->projection());
    int modeledRows = 0;
    for (const QString& grade : rowCheck.grades())
    {
        rowCheck.setCurrentGrade(grade);
        modeledRows += rowCheck.rowCount();
    }
    QCOMPARE(modeledRows, metrics.visibleSummaryCount);

    auto* const mode = page->findChild<QComboBox*>(
        QStringLiteral("subPrepScheduleMode"));
    QVERIFY(mode);
    const auto classFifty = Domain::ClassId::fromString("50");
    QVERIFY(classFifty);
    const int detailsBeforeSelection = metrics.selectedDetailReadCount;
    QVERIFY(page->selectClassForTesting(*classFifty));
    metrics = page->visualMetrics();
    QCOMPARE(page->selectedClassId()->value(), classFifty->value());
    QCOMPARE(page->selectedDetails()->classId.value(), classFifty->value());
    QCOMPARE(metrics.selectedDetailReadCount, detailsBeforeSelection + 1);

    const auto beforeRefresh = metrics;
    QString reloadError;
    QVERIFY2(page->reload(&reloadError), qPrintable(reloadError));
    metrics = page->visualMetrics();
    QCOMPARE(metrics.scheduleReadCount, beforeRefresh.scheduleReadCount + 1);
    QCOMPARE(metrics.summaryReadCount, beforeRefresh.summaryReadCount + 1);
    QCOMPARE(metrics.selectedDetailReadCount,
             beforeRefresh.selectedDetailReadCount + 1);
    QCOMPARE(page->selectedClassId()->value(), classFifty->value());

    mode->setCurrentIndex(mode->findData(
        static_cast<int>(Application::ScheduleViewMode::Intensive)));
    metrics = page->visualMetrics();
    QCOMPARE(metrics.visibleSummaryCount, 24);
    QVERIFY(page->selectedClassId());
    QVERIFY(page->selectedDetails());
    QVERIFY(page->selectedClassId()->value() != classFifty->value());
    QCOMPARE(page->selectedDetails()->classId.value(),
             page->selectedClassId()->value());
    mode->setCurrentIndex(mode->findData(
        static_cast<int>(Application::ScheduleViewMode::Regular)));
    metrics = page->visualMetrics();
    QCOMPARE(metrics.visibleSummaryCount, 96);

    const int summaryBeforeEmpty = metrics.summaryReadCount;
    const int scheduleBeforeEmpty = metrics.scheduleReadCount;
    for (int day = 0; day < 5; ++day)
    {
        auto* const check = page->findChild<QCheckBox*>(
            QStringLiteral("subPrepDay_%1").arg(day));
        QVERIFY(check);
        check->setChecked(false);
    }
    metrics = page->visualMetrics();
    QCOMPARE(metrics.summaryReadCount, summaryBeforeEmpty + 5);
    QCOMPARE(metrics.scheduleReadCount, scheduleBeforeEmpty + 5);
    QCOMPARE(metrics.visibleSummaryCount, 0);
    QVERIFY(!metrics.selectedDetailsRetained);
    QVERIFY(!page->selectedDetails());

    auto* const monday = page->findChild<QCheckBox*>(
        QStringLiteral("subPrepDay_0"));
    QVERIFY(monday);
    monday->setChecked(true);
    QVERIFY(page->visualMetrics().visibleSummaryCount > 0);

    // Run the production output dialog and package service against the same
    // active gateway, with a bounded Monday operation from the 96-class store.
    const QString outputRoot = temporaryDirectory.filePath(
        QStringLiteral("sub-prep-generated"));
    QVERIFY(QDir().mkpath(outputRoot));
    auto* const generate = page->findChild<QPushButton*>(
        QStringLiteral("subPrepGenerateButton"));
    QVERIFY(generate);

    // Canceling the Next-shell output dialog must leave the selected detail
    // intact and must not create the requested output directory.
    const QString canceledOutputRoot = temporaryDirectory.filePath(
        QStringLiteral("sub-prep-canceled-output"));
    QVERIFY(!QFileInfo::exists(canceledOutputRoot));
    const auto selectedBeforeCancel = page->selectedClassId();
    QVERIFY(selectedBeforeCancel);
    const auto metricsBeforeCancel = page->visualMetrics();
    bool cancelDialogSeen = false;
    bool cancelAutomationFailed = false;
    QTimer::singleShot(0, page, [&] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            cancelAutomationFailed = true;
            return;
        }
        cancelDialogSeen = true;
        auto* const folder = dialog->findChild<QLineEdit*>(
            QStringLiteral("subPrepOutputFolder"));
        auto* const cancel = dialog->findChild<QPushButton*>(
            QStringLiteral("subPrepOutputCancel"));
        if (!folder || !cancel)
        {
            cancelAutomationFailed = true;
            dialog->reject();
            return;
        }
        folder->setText(canceledOutputRoot);
        cancel->click();
    });
    QTimer cancelSafetyClose;
    cancelSafetyClose.setSingleShot(true);
    QObject::connect(&cancelSafetyClose, &QTimer::timeout, page, [&] {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            cancelAutomationFailed = true;
            modal->close();
        }
    });
    cancelSafetyClose.start(15'000);
    generate->click();
    cancelSafetyClose.stop();
    QVERIFY(cancelDialogSeen);
    QVERIFY(!cancelAutomationFailed);
    QVERIFY(!QFileInfo::exists(canceledOutputRoot));
    QCOMPARE(page->findChild<QLabel*>(
        QStringLiteral("subPrepRouteStatus"))->text(),
        QStringLiteral("Sub Prep generation canceled."));
    const auto metricsAfterCancel = page->visualMetrics();
    QCOMPARE(metricsAfterCancel.scheduleReadCount,
             metricsBeforeCancel.scheduleReadCount + 1);
    QCOMPARE(metricsAfterCancel.summaryReadCount,
             metricsBeforeCancel.summaryReadCount);
    QCOMPARE(metricsAfterCancel.selectedDetailReadCount,
             metricsBeforeCancel.selectedDetailReadCount);
    QCOMPARE(page->selectedClassId()->value(), selectedBeforeCancel->value());
    QVERIFY(page->selectedDetails());
    QCOMPARE(page->selectedDetails()->classId.value(),
             selectedBeforeCancel->value());

    const int detailsBeforeOutput =
        page->visualMetrics().selectedDetailReadCount;
    bool dialogSeen = false;
    bool automationFailed = false;
    QTimer::singleShot(0, page, [&] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            automationFailed = true;
            return;
        }
        dialogSeen = true;
        auto* const folder = dialog->findChild<QLineEdit*>(
            QStringLiteral("subPrepOutputFolder"));
        auto* const name = dialog->findChild<QLineEdit*>(
            QStringLiteral("subPrepOutputUserName"));
        auto* const openAfter = dialog->findChild<QCheckBox*>(
            QStringLiteral("subPrepOpenFolderAfterGeneration"));
        auto* const vacation = dialog->findChild<QCheckBox*>(
            QStringLiteral("subPrepNextVacationCheckBox"));
        auto* const buttons = dialog->findChild<QDialogButtonBox*>();
        if (!folder || !name || !openAfter || !buttons
            || !buttons->button(QDialogButtonBox::Ok))
        {
            automationFailed = true;
            dialog->reject();
            return;
        }
        folder->setText(outputRoot);
        name->setText(QStringLiteral("C44"));
        openAfter->setChecked(false);
        if (vacation && vacation->isEnabled())
        {
            vacation->setChecked(false);
        }
        for (int index = 0; index < 7; ++index)
        {
            if (auto* const day = dialog->findChild<QCheckBox*>(
                    QStringLiteral("subPrepOutputDay_%1").arg(index)))
            {
                day->setChecked(index == 0);
            }
        }
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    QTimer safetyClose;
    safetyClose.setSingleShot(true);
    QObject::connect(&safetyClose, &QTimer::timeout, page, [&] {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            automationFailed = true;
            modal->close();
        }
    });
    safetyClose.start(15'000);
    generate->click();
    safetyClose.stop();
    QVERIFY(dialogSeen);
    QVERIFY(!automationFailed);
    const QString outputStatus = page->findChild<QLabel*>(
        QStringLiteral("subPrepRouteStatus"))->text();
    QVERIFY2(outputStatus.contains(QStringLiteral(
        "Multiple selected classes use the Monday 4:00 PM slot.")),
        qPrintable(outputStatus));
    const QStringList folders = QDir(outputRoot).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot);
    QVERIFY2(folders.isEmpty(),
        "The rejected conflicting output must not leave a package folder.");
    QCOMPARE(page->visualMetrics().selectedDetailReadCount,
             detailsBeforeOutput);
    QVERIFY(page->selectedDetails());

    for (int day = 1; day < 5; ++day)
    {
        auto* const check = page->findChild<QCheckBox*>(
            QStringLiteral("subPrepDay_%1").arg(day));
        QVERIFY(check);
        check->setChecked(true);
    }
    QVERIFY(host.activatePage(QStringLiteral("my_workspace")));
    metrics = page->visualMetrics();
    QCOMPARE(metrics.visibleSummaryCount, 0);
    QVERIFY(!page->selectedDetails());
    QVERIFY(host.activatePage(QStringLiteral("sub_prep")));
    metrics = page->visualMetrics();
    QCOMPARE(metrics.visibleSummaryCount, 96);
    QCOMPARE(metrics.summaryModelCount, 1);
    QCOMPARE(metrics.selectedDetailPanelCount, 1);
    QVERIFY(page->selectedDetails());
    QCOMPARE(page->selectedDetails()->classId.value(),
             page->selectedClassId()->value());

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
scheduleSelectionOpensClassDetailsAndRefreshesAfterSave()
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
        QStringLiteral("schedule-class-details.tps"));
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
                 profilePath, QStringLiteral("representative_startup.sql"),
                 &fixtureError), qPrintable(fixtureError));

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"), profilePath};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.workspaceGatewayFactory = [] {
        return std::make_unique<Platform::WorkspaceStoreGateway>();
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
    Ui::PageHost& host = bootstrap.shell()->pageHost();
    QVERIFY(host.activatePage(QStringLiteral("schedule")));
    auto* const schedule = dynamic_cast<Ui::SchedulePage*>(
        host.pageInstance(QStringLiteral("schedule")));
    QVERIFY(schedule);
    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("schedule")));

    const auto classOne = Domain::ClassId::fromString("1");
    QVERIFY(classOne);
    std::optional<QModelIndex> classOneIndex;
    for (int row = 0; row < schedule->model()->rowCount() && !classOneIndex; ++row)
    {
        for (int column = 1;
             column < schedule->model()->columnCount() && !classOneIndex;
             ++column)
        {
            const Ui::SchedulePageCell* const cell =
                schedule->model()->cellAt(row, column);
            if (cell && std::any_of(cell->entries.cbegin(), cell->entries.cend(),
                    [&classOne](const Ui::SchedulePageEntry& entry) {
                        return entry.classId == *classOne;
                    }))
            {
                classOneIndex = schedule->model()->index(row, column);
            }
        }
    }
    QVERIFY(classOneIndex);
    schedule->scheduleTable()->scrollTo(*classOneIndex);
    QApplication::processEvents();
    const QRect cellRect = schedule->scheduleTable()->visualRect(*classOneIndex);
    QVERIFY(cellRect.isValid());
    QTest::mouseClick(schedule->scheduleTable()->viewport(), Qt::LeftButton,
                      Qt::NoModifier, cellRect.center());
    QApplication::processEvents();

    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("classes")));
    auto* const classes = dynamic_cast<Ui::ClassesNavigatorPage*>(
        host.pageInstance(QStringLiteral("classes")));
    QVERIFY(classes);
    QCOMPARE(classes->selectedClassId(), classOne);
    Ui::SelectedClassEditor* const editor = classes->selectedClassEditor();
    QVERIFY(editor);
    QVERIFY(editor->isLoaded());
    QCOMPARE(editor->classId(), classOne);

    QTableWidget* const regularSchedule = editor->regularSchedule();
    QVERIFY(regularSchedule);
    int mondayRow = -1;
    for (int row = 0; row < regularSchedule->rowCount(); ++row)
    {
        if (regularSchedule->item(row, 0)
            && regularSchedule->item(row, 0)->text() == QStringLiteral("Monday"))
        {
            mondayRow = row;
            break;
        }
    }
    QVERIFY(mondayRow >= 0);
    regularSchedule->item(mondayRow, 1)->setText(QStringLiteral("8:00 PM"));
    regularSchedule->item(mondayRow, 2)->setText(QStringLiteral("8:50 PM"));
    editor->gradeChoice()->setCurrentText(QStringLiteral("E4"));
    editor->levelChoice()->setCurrentText(QStringLiteral("Theseus"));
    auto* const readingBook = editor->findChild<QComboBox*>(
        QStringLiteral("classEditorReadingBook"));
    auto* const essayBook = editor->findChild<QComboBox*>(
        QStringLiteral("classEditorEssayBook"));
    QVERIFY(readingBook);
    QVERIFY(essayBook);
    readingBook->setCurrentText(QStringLiteral("Reading Explorer 1"));
    essayBook->setCurrentText(QStringLiteral("4A"));
    QString saveError;
    QVERIFY2(editor->saveChanges(&saveError), qPrintable(saveError));

    QVERIFY(host.activatePage(QStringLiteral("schedule")));
    bool foundUpdatedClass = false;
    for (int row = 0; row < schedule->model()->rowCount() && !foundUpdatedClass; ++row)
    {
        for (int column = 1;
             column < schedule->model()->columnCount() && !foundUpdatedClass;
             ++column)
        {
            const Ui::SchedulePageCell* const cell =
                schedule->model()->cellAt(row, column);
            foundUpdatedClass = cell
                && cell->day == QStringLiteral("Monday")
                && cell->startMinute == 20 * 60
                && std::any_of(cell->entries.cbegin(), cell->entries.cend(),
                    [&classOne](const Ui::SchedulePageEntry& entry) {
                        return entry.classId == *classOne;
                    });
        }
    }
    QVERIFY(foundUpdatedClass);
    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
interruptedProfileRecoveryMenuActionOpensCopyOnlyAfterSuccessfulOpen()
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
    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral(
            ".Original.tps.workspace-create-12345678-1234-1234-1234-123456789abc.recovery"
            )
        );
    const QString targetPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("Original.tps")
        );
    const QString failedDestination = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("failed-recovery.tps")
        );
    const QString successfulDestination =
        QDir(temporaryDirectory.path()).filePath(
            QStringLiteral("Recovered-Original.tps")
            );
    const QByteArray recoveryBytes("recovery profile bytes");
    const auto writeFile = [](const QString& path, const QByteArray& bytes) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(bytes) == bytes.size();
    };
    const auto readFile = [](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    };
    QVERIFY(writeFile(sourcePath, recoveryBytes));

    const QString preferencePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("settings")
        );
    QVERIFY(QDir().mkpath(preferencePath));
    TeacherProfileDirectoryPreferenceGuard directoryPreference(preferencePath);
    QVERIFY(directoryPreference.write(temporaryDirectory.path()));

    auto recoveryProbe =
        std::make_shared<InterruptedWorkspaceRecoveryProbe>();
    recoveryProbe->directory = Application::FileDialogPath(
        temporaryDirectory.path().toUtf8().toStdString()
        );
    recoveryProbe->candidate = Application::InterruptedWorkspaceRecoveryCandidate{
        .sourcePath = Application::FileDialogPath(
            sourcePath.toUtf8().toStdString()
            ),
        .targetPath = Application::FileDialogPath(
            targetPath.toUtf8().toStdString()
            ),
        .targetFileName = "Original.tps"
    };
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        failedDestination.toUtf8().toStdString()
        );
    auto promptProbe =
        std::make_shared<InterruptedRecoveryUserPromptProbe>();
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    historyProbe->mutateOnSave = true;
    historyProbe->history.paths = {
        Application::RecentWorkspacePath("older-profile.tps")
    };
    const Application::RecentWorkspaceHistory historyBefore =
        historyProbe->history;
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = journalPath;
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(
            gatewayProbe
            );
    };
    seams.interruptedProfileRecoveryFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            fileDialogProbe
            );
    };
    seams.interruptedProfileRecoveryRecentWorkspaceHistoryPortFactory =
        [historyProbe] {
            return std::make_unique<RecordingRecentWorkspaceHistoryPort>(
                historyProbe
                );
        };
    seams.interruptedWorkspaceRecoveryPortFactory = [recoveryProbe] {
        return std::make_unique<RecordingInterruptedWorkspaceRecoveryPort>(
            recoveryProbe
            );
    };
    seams.workspaceLifecycleUserPromptPortFactory = [promptProbe] {
        return std::make_unique<RecordingInterruptedRecoveryUserPrompt>(
            promptProbe
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
    QAction* const recoveryAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextRecoverInterruptedTeacherProfile")
        );
    QVERIFY(recoveryAction);
    QCOMPARE(recoveryAction->text(),
             QStringLiteral("Recover Interrupted Teacher Profile…"));
    const QString pageBeforeRecovery =
        bootstrap.shell()->navigationModel()->currentPageId()->value();
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QVERIFY(!QFileInfo::exists(failedDestination));
    QVERIFY(!QFileInfo::exists(successfulDestination));

    // Cancel the candidate chooser. Nothing is copied, opened, recorded, or
    // navigated away from the initial page.
    recoveryAction->trigger();
    QCOMPARE(recoveryProbe->scanCalls, 1);
    QCOMPARE(recoveryProbe->lastScannedDirectory.value(),
             temporaryDirectory.path().toUtf8().toStdString());
    QCOMPARE(promptProbe->candidateRequests.size(), std::size_t(1));
    QVERIFY(promptProbe->candidateRequests.front().defaultActionId.has_value());
    QCOMPARE(promptProbe->candidateRequests.front().defaultActionId->value(),
             std::string("cancel"));
    QVERIFY(promptProbe->candidateRequests.front().informativeText.find(
                sourcePath.toUtf8().toStdString()
                ) != std::string::npos);
    QVERIFY(promptProbe->candidateRequests.front().informativeText.find(
                targetPath.toUtf8().toStdString()
                ) != std::string::npos);
    QCOMPARE(fileDialogProbe->saveCount, 0);
    QCOMPARE(recoveryProbe->copyCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 0);
    QCOMPARE(historyProbe->saveCount, 0);
    QCOMPARE(historyProbe->history, historyBefore);
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId()->value(),
             pageBeforeRecovery);

    // A failed open leaves the source and copied artifact in place while the
    // active workspace, navigation, and history remain unchanged.
    promptProbe->selectedAction = *Application::PromptActionId::create(
        "candidate-0"
        );
    auto gatewayOpenError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected recovered-profile open failure",
        .recoverable = true
    };
    gatewayProbe->openError = gatewayOpenError;
    recoveryAction->trigger();
    QCOMPARE(recoveryProbe->scanCalls, 2);
    QCOMPARE(fileDialogProbe->saveCount, 1);
    QVERIFY(fileDialogProbe->lastSaveRequest.has_value());
    QCOMPARE(fileDialogProbe->lastSaveRequest->initialDirectory.value(),
             temporaryDirectory.path().toUtf8().toStdString());
    QCOMPARE(fileDialogProbe->lastSaveRequest->suggestedFileName,
             std::string("Original.tps"));
    QCOMPARE(fileDialogProbe->lastSaveRequest->defaultSuffix,
             std::string("tps"));
    QVERIFY(!fileDialogProbe->lastSaveRequest->confirmOverwrite);
    QCOMPARE(recoveryProbe->copyCalls, 1);
    QCOMPARE(gatewayProbe->openCalls, 1);
    QVERIFY(QFileInfo::exists(sourcePath));
    QVERIFY(QFileInfo::exists(failedDestination));
    QCOMPARE(readFile(sourcePath), recoveryBytes);
    QCOMPARE(readFile(failedDestination), recoveryBytes);
    QCOMPARE(historyProbe->saveCount, 0);
    QCOMPARE(historyProbe->history, historyBefore);
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId()->value(),
             pageBeforeRecovery);
    QCOMPARE(directoryPreference.read().value(),
             temporaryDirectory.path().toUtf8().toStdString());

    // The same action records and navigates only after the copied profile opens.
    gatewayProbe->openError.reset();
    fileDialogProbe->selectedPath = Application::FileDialogPath(
        successfulDestination.toUtf8().toStdString()
        );
    recoveryAction->trigger();
    QCOMPARE(recoveryProbe->scanCalls, 3);
    QCOMPARE(recoveryProbe->copyCalls, 2);
    QCOMPARE(gatewayProbe->openCalls, 2);
    QCOMPARE(historyProbe->saveCount, 1);
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    const Application::WorkspaceLocation expectedLocation(
        QFileInfo(successfulDestination).canonicalFilePath()
            .toUtf8().toStdString()
        );
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             expectedLocation);
    QCOMPARE(historyProbe->history.paths.front(), expectedLocation);
    QCOMPARE(bootstrap.shell()->navigationModel()->workspaceState(),
             Application::WorkspaceLifecycleState::Open);
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    QVERIFY(QFileInfo::exists(sourcePath));
    QVERIFY(QFileInfo::exists(failedDestination));
    QVERIFY(QFileInfo::exists(successfulDestination));
    QCOMPARE(readFile(sourcePath), recoveryBytes);
    QCOMPARE(readFile(successfulDestination), recoveryBytes);
    QCOMPARE(directoryPreference.read().value(),
             QFileInfo(successfulDestination).canonicalPath()
                 .toUtf8().toStdString());

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
    historyProbe->history.paths = {
        Application::RecentWorkspacePath(profilePath.toUtf8().toStdString())
    };
    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
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
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
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
    Ui::Page* const initialPage = bootstrap.shell()->pageHost().pageInstance(
        QStringLiteral("initial_setup")
        );
    QVERIFY(initialPage);
    initialPage->markUnsavedChanges();
    QAction* const openAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextOpenTeacherProfile")
        );
    QVERIFY(openAction);
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 0);
    QCOMPARE(openHistoryFactoryCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 0);
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(promptProbe->requests.empty());
    QVERIFY(initialPage->hasUnsavedChanges());
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());

    QMenu* const recentMenu = bootstrap.shell()->recentWorkspaceMenu();
    QVERIFY(recentMenu);
    QVERIFY(QMetaObject::invokeMethod(
        recentMenu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    QVERIFY(!recentMenu->actions().empty());
    recentMenu->actions().front()->trigger();
    QCOMPARE(openHistoryFactoryCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 0);
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(promptProbe->requests.empty());
    QVERIFY(initialPage->hasUnsavedChanges());
    QFile journalBefore(journalPath);
    QVERIFY(journalBefore.open(QIODevice::ReadOnly));
    QCOMPARE(journalBefore.readAll(), journalPayload);
    journalBefore.close();

    QVERIFY(writeFile(journalPath, QByteArrayLiteral("{ unreadable recovery journal")));
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 0);
    QCOMPARE(openHistoryFactoryCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 0);
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(promptProbe->requests.empty());
    QFile journalAfter(journalPath);
    QVERIFY(journalAfter.open(QIODevice::ReadOnly));
    QCOMPARE(journalAfter.readAll(), QByteArrayLiteral("{ unreadable recovery journal"));

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
recentFilesMenuRefreshesOrderedEscapedPathsAndEmptyState()
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
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    const QString firstPath = temporaryDirectory.filePath(
        QStringLiteral("Teacher & One.tps")
        );
    const QString secondPath = temporaryDirectory.filePath(
        QStringLiteral("Second.tps")
        );
    const QString fallbackPath = temporaryDirectory.filePath(
        QStringLiteral("legacy-fallback.tps")
        );
    const auto workspacePath = [](const QString& path) {
        return Application::RecentWorkspacePath(path.toUtf8().toStdString());
    };
    historyProbe->history.paths = {
        workspacePath(firstPath),
        workspacePath(secondPath)
    };
    historyProbe->history.lastPath = workspacePath(fallbackPath);

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
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

    QMenu* const menu = bootstrap.shell()->recentWorkspaceMenu();
    QVERIFY(menu);
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    QCOMPARE(historyProbe->loadCount, 1);
    QCOMPARE(menu->actions().size(), 4);
    const QString nativeFirstPath = QDir::toNativeSeparators(firstPath);
    QString escapedFirstPath = nativeFirstPath;
    escapedFirstPath.replace(QLatin1Char('&'), QStringLiteral("&&"));
    QCOMPARE(menu->actions().at(0)->text(),
             QStringLiteral("&1 %1").arg(escapedFirstPath));
    QCOMPARE(menu->actions().at(0)->data().toString(), firstPath);
    QCOMPARE(menu->actions().at(0)->toolTip(), nativeFirstPath);
    QCOMPARE(menu->actions().at(1)->data().toString(), secondPath);
    QVERIFY(menu->actions().at(2)->isSeparator());
    QCOMPARE(menu->actions().at(3)->text(), QStringLiteral("Clear Recent Files"));
    QCOMPARE(
        bootstrap.shell()->menuBar()->actions().front()->menu()->actions().at(3),
        menu->menuAction()
        );

    const QString refreshedPath = temporaryDirectory.filePath(
        QStringLiteral("Refreshed.tps")
        );
    historyProbe->history.paths = {workspacePath(refreshedPath)};
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    QCOMPARE(historyProbe->loadCount, 2);
    QCOMPARE(menu->actions().size(), 3);
    QCOMPARE(menu->actions().at(0)->data().toString(), refreshedPath);
    QVERIFY(menu->actions().at(1)->isSeparator());
    QCOMPARE(menu->actions().at(2)->text(), QStringLiteral("Clear Recent Files"));

    historyProbe->history.paths.clear();
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    QCOMPARE(historyProbe->loadCount, 3);
    QCOMPARE(menu->actions().size(), 1);
    QCOMPARE(menu->actions().front()->text(), QStringLiteral("(No Recent Files)"));
    QVERIFY(!menu->actions().front()->isEnabled());

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
recentRestoreUsesOpenFlowAndResolvesDirtyPageBeforeReplacement()
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
    const QString firstPath = temporaryDirectory.filePath(
        QStringLiteral("first-recent.tps")
        );
    const QString secondPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("second/second-recent.tps")
        );
    const auto touch = [](const QString& path) {
        if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        {
            return false;
        }
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(QByteArrayLiteral("profile remains on disk")) > 0;
    };
    QVERIFY(touch(firstPath));
    QVERIFY(touch(secondPath));
    const Application::WorkspaceLocation normalizedFirst(
        QFileInfo(firstPath).canonicalFilePath().toUtf8().toStdString()
        );
    const Application::WorkspaceLocation normalizedSecond(
        QFileInfo(secondPath).canonicalFilePath().toUtf8().toStdString()
        );
    const auto parentDirectoryOf = [](
        const Application::WorkspaceLocation& location
        ) {
        const QString normalizedPath = QString::fromUtf8(
            location.value().data(),
            static_cast<qsizetype>(location.value().size())
            );
        return QFileInfo(normalizedPath).absolutePath();
    };
    const QString failedPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("failed/failed-recent.tps")
        );
    QVERIFY(touch(failedPath));

    const QString settingsPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("settings")
        );
    QVERIFY(QDir().mkpath(settingsPath));
    TeacherProfileDirectoryPreferenceGuard directoryPreference(settingsPath);
    const Domain::Result<void> initialDirectoryWrite = directoryPreference.write(
        temporaryDirectory.filePath(QStringLiteral("remembered-before-open"))
        );
    QVERIFY2(
        initialDirectoryWrite,
        initialDirectoryWrite
            ? ""
            : initialDirectoryWrite.error().message.c_str()
        );

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    historyProbe->history.paths = {
        Application::RecentWorkspacePath(firstPath.toUtf8().toStdString())
    };
    historyProbe->mutateOnSave = true;
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->openPath = Application::FileDialogPath(
        firstPath.toUtf8().toStdString()
        );
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
    int dialogFactoryCalls = 0;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.openProfileFileDialogPortFactory = [&, fileDialogProbe] {
        ++dialogFactoryCalls;
        return std::make_unique<RecordingInitialSetupFileDialogPort>(fileDialogProbe);
    };
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
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
    QMenu* const menu = bootstrap.shell()->recentWorkspaceMenu();
    QVERIFY(menu);
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    QCOMPARE(menu->actions().at(0)->data().toString(), firstPath);
    menu->actions().at(0)->trigger();

    QCOMPARE(dialogFactoryCalls, 0);
    QCOMPARE(fileDialogProbe->openCount, 0);
    QCOMPARE(gatewayProbe->openCalls, 1);
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             normalizedFirst);
    QCOMPARE(directoryPreference.read().value(),
             parentDirectoryOf(normalizedFirst).toUtf8().toStdString());
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));

    Ui::Page* const page = bootstrap.shell()->pageHost().pageInstance(
        QStringLiteral("my_workspace")
        );
    QVERIFY(page);
    page->markUnsavedChanges();
    historyProbe->history.paths = {
        Application::RecentWorkspacePath(secondPath.toUtf8().toStdString())
    };
    promptProbe->action = Application::UnsavedChangesAction::Cancel;
    const QString directoryBeforeCancel = temporaryDirectory.filePath(
        QStringLiteral("remembered-before-cancel")
        );
    QVERIFY(directoryPreference.write(directoryBeforeCancel));
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    menu->actions().at(0)->trigger();
    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QCOMPARE(dialogFactoryCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 1);
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(historyProbe->history.paths.front().value(),
             secondPath.toUtf8().toStdString());
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             normalizedFirst);
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(directoryPreference.read().value(),
             directoryBeforeCancel.toUtf8().toStdString());

    promptProbe->action = Application::UnsavedChangesAction::Discard;
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    menu->actions().at(0)->trigger();
    QCOMPARE(promptProbe->requests.size(), std::size_t(2));
    QCOMPARE(gatewayProbe->openCalls, 2);
    QCOMPARE(historyProbe->saveCount, 2);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             normalizedSecond);
    QCOMPARE(directoryPreference.read().value(),
             parentDirectoryOf(normalizedSecond).toUtf8().toStdString());
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(fileDialogProbe->openCount, 0);
    QVERIFY(QFileInfo::exists(firstPath));
    QVERIFY(QFileInfo::exists(secondPath));

    historyProbe->history.paths = {
        Application::RecentWorkspacePath(failedPath.toUtf8().toStdString())
    };
    gatewayProbe->openError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected profile open failure",
        .recoverable = true
    };
    const std::size_t historySaveCount = historyProbe->saves.size();
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    QCOMPARE(menu->actions().at(0)->data().toString(), failedPath);
    menu->actions().at(0)->trigger();
    QCOMPARE(gatewayProbe->openCalls, 3);
    QCOMPARE(historyProbe->saves.size(), historySaveCount);
    QCOMPARE(historyProbe->history.paths.front().value(),
             failedPath.toUtf8().toStdString());
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             normalizedSecond);
    QCOMPARE(directoryPreference.read().value(),
             parentDirectoryOf(normalizedSecond).toUtf8().toStdString());
    QCOMPARE(dialogFactoryCalls, 0);
    QCOMPARE(fileDialogProbe->openCount, 0);

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
recentRestoreSavesDirtyWorkspaceBeforeReplacement()
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
    const QString activePath = temporaryDirectory.filePath(
        QStringLiteral("active.tps")
        );
    const QString selectedPath = temporaryDirectory.filePath(
        QStringLiteral("selected.tps")
        );
    const auto touch = [](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(QByteArrayLiteral("profile data")) > 0;
    };
    QVERIFY(touch(activePath));
    QVERIFY(touch(selectedPath));
    const Application::WorkspaceLocation normalizedActive(
        QFileInfo(activePath).absoluteFilePath().toUtf8().toStdString()
        );
    const Application::WorkspaceLocation normalizedSelected(
        QFileInfo(selectedPath).canonicalFilePath().toUtf8().toStdString()
        );

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    historyProbe->history.paths = {
        Application::RecentWorkspacePath(selectedPath.toUtf8().toStdString())
    };
    historyProbe->mutateOnSave = true;
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->openPath = Application::FileDialogPath(
        selectedPath.toUtf8().toStdString()
        );
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
    int dialogFactoryCalls = 0;
    Application::WorkspaceState* startupWorkspaceState = nullptr;
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"),
        activePath
    };
    seams.recentAutoOpenEnabled = false;
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.afterStartupWorkspaceOpenedForTesting = [&startupWorkspaceState](
        Application::WorkspaceState& state
        ) {
        startupWorkspaceState = &state;
    };
    seams.openProfileFileDialogPortFactory = [&, fileDialogProbe] {
        ++dialogFactoryCalls;
        return std::make_unique<RecordingInitialSetupFileDialogPort>(fileDialogProbe);
    };
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
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
    QVERIFY(startupWorkspaceState);
    QVERIFY(QMetaObject::invokeMethod(
        &application,
        [startupWorkspaceState] {
            static_cast<void>(startupWorkspaceState->markDirty());
        },
        Qt::QueuedConnection
        ));
    QCoreApplication::processEvents();
    QCOMPARE(
        bootstrap.workspaceStateSnapshot().unsavedState(),
        Application::WorkspaceUnsavedState::Dirty
        );
    const Application::WorkspaceSession activeSession =
        *bootstrap.workspaceStateSnapshot().session();
    QCOMPARE(activeSession.location(), normalizedActive);
    QCOMPARE(gatewayProbe->workspaceOperationOrder,
             std::vector<std::string>{"open"});

    // Command-line startup records its profile, so select the second recent
    // path explicitly to isolate this restore operation's history update.
    historyProbe->history.paths = {
        Application::RecentWorkspacePath(selectedPath.toUtf8().toStdString())
    };
    QMenu* const menu = bootstrap.shell()->recentWorkspaceMenu();
    QVERIFY(menu);
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    QCOMPARE(menu->actions().at(0)->data().toString(), selectedPath);

    promptProbe->action = Application::UnsavedChangesAction::Save;
    menu->actions().at(0)->trigger();

    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QCOMPARE(gatewayProbe->saveCalls, 1);
    QCOMPARE(gatewayProbe->savedSession,
             std::optional<Application::WorkspaceSession>(activeSession));
    QCOMPARE(gatewayProbe->openCalls, 2);
    QCOMPARE(gatewayProbe->workspaceOperationOrder,
             (std::vector<std::string>{"open", "save", "open"}));
    QCOMPARE(gatewayProbe->openedSession->location(), normalizedSelected);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             normalizedSelected);
    QCOMPARE(bootstrap.workspaceStateSnapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Clean);
    QCOMPARE(dialogFactoryCalls, 0);
    QCOMPARE(fileDialogProbe->openCount, 0);
    QCOMPARE(historyProbe->saveCount, 2);
    QCOMPARE(historyProbe->saves.back().paths.front(), normalizedSelected);
    QCOMPARE(historyProbe->saves.back().lastPath,
             std::optional<Application::RecentWorkspacePath>(normalizedSelected));

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
recentRestoreRemembersDirectoryWhenHistorySaveFails()
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
    const QString selectedPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("selected/selected.tps")
        );
    QVERIFY(QDir().mkpath(QFileInfo(selectedPath).absolutePath()));
    QFile selectedFile(selectedPath);
    QVERIFY(selectedFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(selectedFile.write(QByteArrayLiteral("profile data")), qint64{12});
    selectedFile.close();
    const Application::WorkspaceLocation normalizedSelected(
        QFileInfo(selectedPath).canonicalFilePath().toUtf8().toStdString()
        );
    const QString normalizedDirectory = QFileInfo(
        QString::fromUtf8(
            normalizedSelected.value().data(),
            static_cast<qsizetype>(normalizedSelected.value().size())
            )
        ).absolutePath();

    const QString settingsPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("settings")
        );
    QVERIFY(QDir().mkpath(settingsPath));
    TeacherProfileDirectoryPreferenceGuard directoryPreference(settingsPath);
    const QString directoryBeforeRestore = temporaryDirectory.filePath(
        QStringLiteral("remembered-before-restore")
        );
    QVERIFY(directoryPreference.write(directoryBeforeRestore));

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    const Application::RecentWorkspacePath recentPath(
        selectedPath.toUtf8().toStdString()
        );
    historyProbe->history.paths = {recentPath};
    historyProbe->history.lastPath = recentPath;
    historyProbe->failSave = true;
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
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
    QMenu* const menu = bootstrap.shell()->recentWorkspaceMenu();
    QVERIFY(menu);
    QVERIFY(QMetaObject::invokeMethod(
        menu,
        "aboutToShow",
        Qt::DirectConnection
        ));
    QCOMPARE(menu->actions().at(0)->data().toString(), selectedPath);
    menu->actions().at(0)->trigger();

    QCOMPARE(gatewayProbe->openCalls, 1);
    QCOMPARE(bootstrap.workspaceStateSnapshot().session()->location(),
             normalizedSelected);
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(historyProbe->saves.size(), std::size_t(1));
    QCOMPARE(historyProbe->history.paths.front(), recentPath);
    QCOMPARE(directoryPreference.read().value(),
             normalizedDirectory.toUtf8().toStdString());
    QVERIFY(bootstrap.shell()->statusBar()->currentMessage().contains(
        QStringLiteral("recent profile history could not be saved")
        ));

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::
clearRecentHistoryPreservesClosedAndOpenWorkspaceState()
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
        QStringLiteral("profile.tps")
        );
    const QByteArray profileBytes = QByteArrayLiteral("profile file remains intact");
    auto writeProfile = [&] {
        QFile file(profilePath);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(profileBytes) == profileBytes.size();
    };
    auto profileContents = [&] {
        QFile file(profilePath);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    };
    QVERIFY(writeProfile());

    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    const Application::RecentWorkspacePath recentPath(
        profilePath.toUtf8().toStdString()
        );
    historyProbe->history.paths = {recentPath};
    historyProbe->history.lastPath = recentPath;
    historyProbe->mutateOnSave = true;
    auto fileDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    fileDialogProbe->openPath = Application::FileDialogPath(
        profilePath.toUtf8().toStdString()
        );
    auto gatewayProbe = std::make_shared<BootstrapWorkspaceGatewayProbe>();
    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app")};
    seams.recentAutoOpenEnabled = false;
    seams.workspaceGatewayFactory = [gatewayProbe] {
        return std::make_unique<RecordingBootstrapWorkspaceGateway>(gatewayProbe);
    };
    seams.openProfileFileDialogPortFactory = [fileDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(fileDialogProbe);
    };
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
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
    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    QMenu* const menu = shell->recentWorkspaceMenu();
    QVERIFY(menu);

    QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection));
    QCOMPARE(menu->actions().size(), 3);
    menu->actions().constLast()->trigger();
    QCOMPARE(historyProbe->clearCount, 1);
    QVERIFY(historyProbe->history.paths.empty());
    QVERIFY(!historyProbe->history.lastPath.has_value());
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(profileContents(), profileBytes);

    historyProbe->history.paths = {recentPath};
    historyProbe->history.lastPath = recentPath;
    historyProbe->failClear = true;
    QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection));
    menu->actions().constLast()->trigger();
    QCOMPARE(historyProbe->clearCount, 2);
    QVERIFY(historyProbe->history.paths == Application::RecentWorkspacePaths({recentPath}));
    QVERIFY(shell->statusBar()->currentMessage().contains(
        QStringLiteral("history could not be cleared")
        ));
    QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection));
    QCOMPARE(menu->actions().at(0)->data().toString(), profilePath);
    QCOMPARE(menu->actions().constLast()->text(), QStringLiteral("Clear Recent Files"));
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(profileContents(), profileBytes);

    QAction* const openAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextOpenTeacherProfile")
        );
    QVERIFY(openAction);
    openAction->trigger();
    QCOMPARE(fileDialogProbe->openCount, 1);
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    const Application::WorkspaceSession openSession =
        *bootstrap.workspaceStateSnapshot().session();
    const int closeCallsBeforeClear = gatewayProbe->closeCalls;
    const auto currentPage = shell->navigationModel()->currentPageId();
    QVERIFY(currentPage.has_value());
    Ui::Page* const activePage = shell->pageHost().pageInstance(
        QStringLiteral("my_workspace")
        );
    QVERIFY(activePage);
    activePage->markUnsavedChanges();

    historyProbe->failClear = false;
    QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection));
    menu->actions().constLast()->trigger();
    QCOMPARE(historyProbe->clearCount, 3);
    QVERIFY(historyProbe->history.paths.empty());
    QVERIFY(!historyProbe->history.lastPath.has_value());
    QCOMPARE(bootstrap.workspaceStateSnapshot().session(),
             std::optional<Application::WorkspaceSession>(openSession));
    QCOMPARE(gatewayProbe->closeCalls, closeCallsBeforeClear);
    QCOMPARE(shell->navigationModel()->currentPageId(), currentPage);
    QVERIFY(activePage->hasUnsavedChanges());
    QVERIFY(promptProbe->requests.empty());
    QCOMPARE(profileContents(), profileBytes);

    QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection));
    QCOMPARE(menu->actions().size(), 1);
    QCOMPARE(menu->actions().front()->text(), QStringLiteral("(No Recent Files)"));
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
    int importDialogFactoryCalls = 0;
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
    seams.normalTeacherImportFileDialogPortFactory = [&importDialogFactoryCalls] {
        ++importDialogFactoryCalls;
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
    QAction* const importAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextImportTeachers")
        );
    QVERIFY(openAction);
    QVERIFY(newAction);
    QVERIFY(importAction);
    QVERIFY(importAction->isEnabled());

    promptProbe->action = Application::UnsavedChangesAction::Cancel;
    importAction->trigger();
    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QCOMPARE(importDialogFactoryCalls, 0);
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(shell->navigationModel()->currentPageId()->value(),
             QStringLiteral("my_workspace"));
    promptProbe->requests.clear();

    openAction->trigger();
    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QCOMPARE(openDialogFactoryCalls, 0);
    QCOMPARE(gatewayProbe->openCalls, 1);
    QVERIFY(page->hasUnsavedChanges());

    newAction->trigger();
    QCOMPARE(promptProbe->requests.size(), std::size_t(2));
    QCOMPARE(newDialogFactoryCalls, 0);
    QVERIFY(page->hasUnsavedChanges());

    QVERIFY(shell->findChild<QMenu*>(
        QStringLiteral("classMngrNextAdminMenu")
        ) == nullptr);
    shell->setCampusEditCapability(Application::CampusEditCapability::enabled());
    QMenu* const adminMenu = shell->findChild<QMenu*>(
        QStringLiteral("classMngrNextAdminMenu")
        );
    QAction* const manageCampuses = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextManageCampuses")
        );
    QVERIFY(adminMenu != nullptr);
    QVERIFY(manageCampuses != nullptr);
    manageCampuses->trigger();
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
    QAction* const normalImportAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextImportTeachers"));
    QVERIFY(normalImportAction);
    QVERIFY(!normalImportAction->isEnabled());
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
    QVERIFY(!normalImportAction->isEnabled());
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

void ApplicationBootstrapTests::normalTeacherImportUsesWorkspaceStoreAndKeepsRoute()
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
        QStringLiteral("normal-teacher-import.tps"));
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json"));
    const QString workbookPath = QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral(
            "tests/fixtures/teacher_import/sectioned_review_matching_profile.xlsx"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
                 profilePath,
                 QStringLiteral("representative_startup.sql"),
                 &fixtureError),
             qPrintable(fixtureError));
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral(R"(
            CREATE TRIGGER fail_normal_teacher_import_gs_insert
            BEFORE INSERT ON gs_team
            BEGIN
                SELECT RAISE(ABORT, 'forced normal teacher import failure');
            END
        )")));
    const auto beforeImport = readTeacherImportPersistedRecords(
        profilePath, &fixtureError);
    QVERIFY2(beforeImport.has_value(), qPrintable(fixtureError));

    auto workbookDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    workbookDialogProbe->openPath = Application::FileDialogPath(
        workbookPath.toUtf8().toStdString());
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app"), profilePath};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.normalTeacherImportFileDialogPortFactory = [workbookDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            workbookDialogProbe);
    };
    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
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
    const auto selectedTeacherId = Domain::TeacherId::fromString("3");
    QVERIFY(selectedTeacherId.has_value());
    QVERIFY(bootstrap.openTeacherProfile(*selectedTeacherId));
    const auto nativeDirectoryId = Ui::PageId::fromString(
        QStringLiteral("native_english_teachers"));
    const auto gsDirectoryId = Ui::PageId::fromString(QStringLiteral("gs_team"));
    const auto teacherProfilePageId = Ui::PageId::fromString(
        QStringLiteral("teacher_profile"));
    QVERIFY(nativeDirectoryId && gsDirectoryId && teacherProfilePageId);
    QVERIFY(bootstrap.shell()->navigateTo(*nativeDirectoryId));
    QVERIFY(bootstrap.shell()->navigateTo(*gsDirectoryId));
    QVERIFY(bootstrap.shell()->navigateTo(*teacherProfilePageId));
    auto* const activeTeacherPage = dynamic_cast<Ui::TeacherProfilePage*>(
        bootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("teacher_profile")));
    auto* const cachedNativeDirectory = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        bootstrap.shell()->pageHost().pageInstance(
            QStringLiteral("native_english_teachers")));
    auto* const cachedGsDirectory = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        bootstrap.shell()->pageHost().pageInstance(QStringLiteral("gs_team")));
    QVERIFY(activeTeacherPage && cachedNativeDirectory && cachedGsDirectory);
    QCOMPARE(activeTeacherPage->findChild<QLineEdit*>(
                 QStringLiteral("phoneNumberEdit"))->text(),
             QStringLiteral("010-1000-0003"));
    QCOMPARE(cachedNativeDirectory->directoryModel()->rowCount(), 0);
    QCOMPARE(cachedGsDirectory->directoryModel()->rowCount(), 0);
    const auto routeBefore = bootstrap.shell()->navigationModel()->currentPageId();
    QVERIFY(routeBefore.has_value());
    QVERIFY(routeBefore->value() != QStringLiteral("initial_setup"));
    QAction* const importAction = bootstrap.shell()->findChild<QAction*>(
        QStringLiteral("classMngrNextImportTeachers"));
    QVERIFY(importAction);
    QVERIFY(importAction->isEnabled());

    QString interactionError;
    const auto runImportReview = [&]() {
        int phase = 0;
        QTimer interactionTimer;
        interactionTimer.setInterval(10);
        QObject::connect(
            &interactionTimer,
            &QTimer::timeout,
            &interactionTimer,
            [&] {
                auto* const dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                if (!dialog)
                {
                    return;
                }
                if (phase == 0)
                {
                    auto* const browse = dialog->findChild<QPushButton*>(
                        QStringLiteral("classMngrNextTeacherImportBrowse"));
                    if (!browse)
                    {
                        interactionError = QStringLiteral(
                            "The normal import dialog did not expose Browse.");
                        return;
                    }
                    phase = 1;
                    browse->click();
                    return;
                }
                auto* const status = dialog->findChild<QLabel*>(
                    QStringLiteral("classMngrNextTeacherImportStatus"));
                if (status && status->text().contains(QStringLiteral("Review")))
                {
                    auto* const apply = dialog->findChild<QPushButton*>(
                        QStringLiteral("classMngrNextTeacherImportApply"));
                    if (!apply || !apply->isEnabled())
                    {
                        interactionError = QStringLiteral(
                            "The normal import review was not ready to apply.");
                        return;
                    }
                    phase = 2;
                    apply->click();
                }
            });
        QTimer::singleShot(10000, &interactionTimer, [&] {
            if (phase >= 2)
            {
                return;
            }
            interactionError = QStringLiteral(
                "The normal import dialog did not reach review.");
            if (QWidget* const modal = QApplication::activeModalWidget())
            {
                if (auto* const cancel = modal->findChild<QPushButton*>(
                        QStringLiteral("classMngrNextTeacherImportCancel")))
                {
                    cancel->click();
                }
            }
        });
        interactionTimer.start();
        importAction->trigger();
        interactionTimer.stop();
        return phase == 2 && interactionError.isEmpty();
    };

    const auto cancelBeforeWorkbookSelection = [&]() {
        bool canceled = false;
        QTimer interactionTimer;
        interactionTimer.setInterval(10);
        QObject::connect(
            &interactionTimer,
            &QTimer::timeout,
            &interactionTimer,
            [&] {
                QWidget* const modal = QApplication::activeModalWidget();
                auto* const cancel = modal
                    ? modal->findChild<QPushButton*>(
                          QStringLiteral("classMngrNextTeacherImportCancel"))
                    : nullptr;
                if (cancel)
                {
                    canceled = true;
                    cancel->click();
                }
            });
        QTimer::singleShot(10000, &interactionTimer, [&] {
            if (QWidget* const modal = QApplication::activeModalWidget())
            {
                modal->close();
            }
        });
        interactionTimer.start();
        importAction->trigger();
        interactionTimer.stop();
        return canceled;
    };

    // Export-like read-only workflows still resolve page drafts before they
    // show the workbook selector. Save persists the draft; Discard restores
    // that saved profile; both can then cancel the review without choosing a
    // workbook.
    auto* const profilePhone = activeTeacherPage->findChild<QLineEdit*>(
        QStringLiteral("phoneNumberEdit"));
    QVERIFY(profilePhone);
    profilePhone->setText(QStringLiteral("010-0000-0000"));
    QVERIFY(activeTeacherPage->hasUnsavedChanges());
    promptProbe->action = Application::UnsavedChangesAction::Save;
    QVERIFY(cancelBeforeWorkbookSelection());
    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QVERIFY(!activeTeacherPage->hasUnsavedChanges());
    QCOMPARE(profilePhone->text(), QStringLiteral("010-0000-0000"));
    QCOMPARE(workbookDialogProbe->openCount, 0);

    profilePhone->setText(QStringLiteral("010-9999-9999"));
    QVERIFY(activeTeacherPage->hasUnsavedChanges());
    promptProbe->action = Application::UnsavedChangesAction::Discard;
    QVERIFY(cancelBeforeWorkbookSelection());
    QCOMPARE(promptProbe->requests.size(), std::size_t(2));
    QVERIFY(!activeTeacherPage->hasUnsavedChanges());
    QCOMPARE(profilePhone->text(), QStringLiteral("010-0000-0000"));
    QCOMPARE(workbookDialogProbe->openCount, 0);
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId(), routeBefore);

    // A transaction failure closes this normal review. A second menu action
    // must open a fresh review and pick the workbook again.
    const std::size_t confirmationsBeforeMissingSourceDate =
        promptProbe->confirmationRequests.size();
    QCOMPARE(confirmationsBeforeMissingSourceDate, std::size_t(0));
    QVERIFY(runImportReview());
    QCOMPARE(promptProbe->confirmationRequests.size(),
             confirmationsBeforeMissingSourceDate);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QVERIFY(interactionError.isEmpty());
    QCOMPARE(workbookDialogProbe->openCount, 1);
    QVERIFY(workbookDialogProbe->lastOpenRequest.has_value());
    QCOMPARE(workbookDialogProbe->lastOpenRequest->purpose,
             Application::FileDialogPurpose::ImportWorkbook);
    const auto afterFailedImport = readTeacherImportPersistedRecords(
        profilePath, &fixtureError);
    QVERIFY2(afterFailedImport.has_value(), qPrintable(fixtureError));
    QCOMPARE(*afterFailedImport, *beforeImport);
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("DROP TRIGGER fail_normal_teacher_import_gs_insert")));

    interactionError.clear();
    QVERIFY(runImportReview());
    QCOMPARE(promptProbe->confirmationRequests.size(),
             confirmationsBeforeMissingSourceDate);
    QCOMPARE(workbookDialogProbe->openCount, 2);
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId(), routeBefore);
    auto importedRecords = readTeacherImportPersistedRecords(profilePath, &fixtureError);
    QVERIFY2(importedRecords.has_value(), qPrintable(fixtureError));
    QVERIFY(importedRecords->koreanTeachers.contains(QString::fromUtf8("박민서")));
    QVERIFY(importedRecords->koreanTeachers.contains(QString::fromUtf8("김하늘")));
    QVERIFY(importedRecords->koreanTeachers.contains(QString::fromUtf8("이서연")));
    QVERIFY(importedRecords->koreanTeachers.contains(QString::fromUtf8("박민준")));
    QVERIFY(importedRecords->nativeEnglishTeachers.contains(QStringLiteral("Alex")));
    QVERIFY(importedRecords->gsTeamMembers.contains(QStringLiteral("Taylor")));
    QCOMPARE(importedRecords->latestSourceDate,
             std::optional<QString>(QStringLiteral("2026-09-01")));
    QCOMPARE(activeTeacherPage->findChild<QLineEdit*>(
                 QStringLiteral("phoneNumberEdit"))->text(),
             QStringLiteral("010-1111-1111"));
    QCOMPARE(cachedNativeDirectory->directoryModel()->rowCount(), 1);
    QCOMPARE(cachedGsDirectory->directoryModel()->rowCount(), 1);
    const QString importSummary = bootstrap.shell()->statusBar()->currentMessage();
    QVERIFY(importSummary.contains(
        QStringLiteral("Korean Teachers: 3 created, 1 updated, 0 unchanged")));
    QVERIFY(importSummary.contains(
        QStringLiteral("Native English Teachers: 1 created, 0 updated, 0 unchanged")));
    QVERIFY(importSummary.contains(
        QStringLiteral("GS Team: 1 created, 0 updated, 0 unchanged")));
    auto* const navigationTree = bootstrap.shell()->findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree"));
    QVERIFY(navigationTree);
    QVERIFY(!navigationTree->findItems(
                 QString::fromUtf8("김하늘"),
                 Qt::MatchExactly | Qt::MatchRecursive).isEmpty());

    // A newer source date applies without a version prompt and advances the
    // stored date after the transaction succeeds.
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("INSERT OR REPLACE INTO app_settings(key,value) "
                       "VALUES('teacher_import/latest_source_date','2026-08-01')")));
    const auto promptCountBeforeNewer = promptProbe->confirmationRequests.size();
    interactionError.clear();
    QVERIFY(runImportReview());
    QCOMPARE(promptProbe->confirmationRequests.size(), promptCountBeforeNewer);
    importedRecords = readTeacherImportPersistedRecords(profilePath, &fixtureError);
    QVERIFY2(importedRecords.has_value(), qPrintable(fixtureError));
    QCOMPARE(importedRecords->latestSourceDate,
             std::optional<QString>(QStringLiteral("2026-09-01")));

    // Equal versions prompt after review and decline without writes.
    const auto afterSuccessfulImport = *importedRecords;
    promptProbe->confirmationResponse = Application::ConfirmationResponse::Declined;
    interactionError.clear();
    QVERIFY(runImportReview());
    QCOMPARE(promptProbe->confirmationRequests.size(), promptCountBeforeNewer + 1);
    QVERIFY(promptProbe->confirmationRequests.front().message.find("2026-09-01")
            != std::string::npos);
    importedRecords = readTeacherImportPersistedRecords(profilePath, &fixtureError);
    QVERIFY2(importedRecords.has_value(), qPrintable(fixtureError));
    QCOMPARE(*importedRecords, afterSuccessfulImport);

    // An older file can be accepted, but the transaction keeps the newer
    // stored source date.
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("INSERT OR REPLACE INTO app_settings(key,value) "
                       "VALUES('teacher_import/latest_source_date','2026-10-01')")));
    promptProbe->confirmationResponse = Application::ConfirmationResponse::Accepted;
    interactionError.clear();
    QVERIFY(runImportReview());
    QCOMPARE(promptProbe->confirmationRequests.size(), promptCountBeforeNewer + 2);
    QVERIFY(promptProbe->confirmationRequests.back().message.find("2026-10-01")
            != std::string::npos);
    importedRecords = readTeacherImportPersistedRecords(profilePath, &fixtureError);
    QVERIFY2(importedRecords.has_value(), qPrintable(fixtureError));
    QCOMPARE(importedRecords->latestSourceDate,
             std::optional<QString>(QStringLiteral("2026-10-01")));
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId(), routeBefore);

    // A settings read failure stops after review and before any transaction.
    const TeacherImportPersistedRecords dataBeforeReadFailure = *importedRecords;
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("DROP TABLE app_settings")));
    interactionError.clear();
    QVERIFY(runImportReview());
    QCOMPARE(promptProbe->confirmationRequests.size(), promptCountBeforeNewer + 2);
    const auto namesAfterReadFailure = readTeacherImportNamesWithoutSettings(
        profilePath);
    QVERIFY(namesAfterReadFailure.has_value());
    QCOMPARE(namesAfterReadFailure->koreanTeachers,
             dataBeforeReadFailure.koreanTeachers);
    QCOMPARE(namesAfterReadFailure->nativeEnglishTeachers,
             dataBeforeReadFailure.nativeEnglishTeachers);
    QCOMPARE(namesAfterReadFailure->gsTeamMembers,
             dataBeforeReadFailure.gsTeamMembers);
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId(), routeBefore);

    // A post-commit profile read failure must surface a warning and lock the
    // cached profile form so stale values cannot be saved over the store.
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("CREATE TABLE app_settings(key TEXT PRIMARY KEY, value TEXT NOT NULL)")));
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("INSERT INTO app_settings(key,value) "
                       "VALUES('teacher_import/latest_source_date','2026-10-01')")));
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("UPDATE teachers SET phone_number='010-0000-9999' WHERE id=3")));
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("CREATE TRIGGER remove_imported_profile_after_update "
                       "AFTER UPDATE ON teachers BEGIN "
                       "DELETE FROM teachers WHERE id=NEW.id; END")));
    const std::size_t promptCountBeforeRefreshFailure =
        promptProbe->confirmationRequests.size();
    interactionError.clear();
    QVERIFY(runImportReview());
    QCOMPARE(promptProbe->confirmationRequests.size(),
             promptCountBeforeRefreshFailure + 1);
    importedRecords = readTeacherImportPersistedRecords(profilePath, &fixtureError);
    QVERIFY2(importedRecords.has_value(), qPrintable(fixtureError));
    QVERIFY(!importedRecords->koreanTeachers.contains(QString::fromUtf8("박민서")));
    QVERIFY(!profilePhone->isEnabled());
    QVERIFY(bootstrap.shell()->statusBar()->currentMessage().contains(
        QStringLiteral("refresh failed")));
    QCOMPARE(bootstrap.shell()->navigationModel()->currentPageId(), routeBefore);
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("DROP TRIGGER remove_imported_profile_after_update")));

    QVERIFY(QFileInfo::exists(profilePath));
    QVERIFY(!QFileInfo::exists(recoveryJournalPath));
    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

void ApplicationBootstrapTests::normalTeacherImportNativeDirectoryRefreshFailure()
{
    verifyNormalTeacherImportDirectoryRefreshFailure(true);
}

void ApplicationBootstrapTests::normalTeacherImportGsDirectoryRefreshFailure()
{
    verifyNormalTeacherImportDirectoryRefreshFailure(false);
}

void ApplicationBootstrapTests::verifyNormalTeacherImportDirectoryRefreshFailure(
    const bool nativeDirectory
    )
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
        nativeDirectory
            ? QStringLiteral("native-refresh-failure.tps")
            : QStringLiteral("gs-refresh-failure.tps"));
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
                 profilePath,
                 QStringLiteral("representative_startup.sql"),
                 &fixtureError),
             qPrintable(fixtureError));
    const QString targetTable = nativeDirectory
        ? QStringLiteral("native_english_teachers")
        : QStringLiteral("gs_team");
    const QString idColumn = QStringLiteral("id");
    const QString triggerName = nativeDirectory
        ? QStringLiteral("invalidate_imported_native_teacher_id")
        : QStringLiteral("invalidate_imported_gs_member_id");
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("CREATE TRIGGER %1 AFTER INSERT ON %2 BEGIN "
                       "UPDATE %2 SET %3=-NEW.%3 WHERE %3=NEW.%3; END")
            .arg(triggerName, targetTable, idColumn)));

    auto workbookDialogProbe = std::make_shared<InitialSetupFileDialogProbe>();
    const QString workbookPath = QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral(
            "tests/fixtures/teacher_import/sectioned_review_matching_profile.xlsx"));
    workbookDialogProbe->openPath = Application::FileDialogPath(
        workbookPath.toUtf8().toStdString());
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("test-app"), profilePath};
    seams.recentAutoOpenEnabled = false;
    seams.normalTeacherImportFileDialogPortFactory = [workbookDialogProbe] {
        return std::make_unique<RecordingInitialSetupFileDialogPort>(
            workbookDialogProbe);
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
    const auto targetPageId = Ui::PageId::fromString(targetTable);
    const auto otherPageId = Ui::PageId::fromString(
        nativeDirectory
            ? QStringLiteral("gs_team")
            : QStringLiteral("native_english_teachers"));
    const auto homePageId = Ui::PageId::fromString(QStringLiteral("my_workspace"));
    QVERIFY(targetPageId && otherPageId && homePageId);
    QVERIFY(shell->navigateTo(*targetPageId));
    QVERIFY(shell->navigateTo(*otherPageId));
    QVERIFY(shell->navigateTo(*homePageId));
    auto* const targetPage = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        shell->pageHost().pageInstance(targetTable));
    auto* const otherPage = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        shell->pageHost().pageInstance(
            nativeDirectory
                ? QStringLiteral("gs_team")
                : QStringLiteral("native_english_teachers")));
    QVERIFY(targetPage && otherPage);
    QCOMPARE(targetPage->directoryModel()->rowCount(), 0);
    QCOMPARE(otherPage->directoryModel()->rowCount(), 0);

    QAction* const importAction = shell->findChild<QAction*>(
        QStringLiteral("classMngrNextImportTeachers"));
    QVERIFY(importAction && importAction->isEnabled());
    QString interactionError;
    int phase = 0;
    QTimer interactionTimer;
    interactionTimer.setInterval(10);
    QObject::connect(
        &interactionTimer,
        &QTimer::timeout,
        &interactionTimer,
        [&] {
            auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget());
            if (!dialog)
            {
                return;
            }
            if (phase == 0)
            {
                auto* const browse = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportBrowse"));
                if (!browse)
                {
                    interactionError = QStringLiteral(
                        "Normal import dialog did not expose Browse.");
                    return;
                }
                phase = 1;
                browse->click();
                return;
            }
            auto* const status = dialog->findChild<QLabel*>(
                QStringLiteral("classMngrNextTeacherImportStatus"));
            if (status && status->text().contains(QStringLiteral("Review")))
            {
                auto* const apply = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportApply"));
                if (!apply || !apply->isEnabled())
                {
                    interactionError = QStringLiteral(
                        "Normal import review did not enable Apply.");
                    return;
                }
                phase = 2;
                apply->click();
            }
        });
    QTimer::singleShot(15000, &interactionTimer, [&] {
        if (phase >= 2)
        {
            return;
        }
        interactionError = QStringLiteral(
            "Normal import did not reach and apply its review.");
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            modal->close();
        }
    });
    interactionTimer.start();
    importAction->trigger();
    interactionTimer.stop();
    QVERIFY2(phase == 2, qPrintable(interactionError));
    QVERIFY(interactionError.isEmpty());
    QCOMPARE(workbookDialogProbe->openCount, 1);
    QCOMPARE(shell->navigationModel()->currentPageId(),
             std::optional<Ui::PageId>(*homePageId));

    const auto persisted = readTeacherImportPersistedRecords(profilePath, &fixtureError);
    QVERIFY2(persisted.has_value(), qPrintable(fixtureError));
    QVERIFY(persisted->nativeEnglishTeachers.contains(QStringLiteral("Alex")));
    QVERIFY(persisted->gsTeamMembers.contains(QStringLiteral("Taylor")));
    QVERIFY(shell->statusBar()->currentMessage().contains(
        QStringLiteral("refresh failed")));
    QVERIFY(shell->statusBar()->currentMessage().contains(
        nativeDirectory
            ? QStringLiteral("Native English Teachers directory")
            : QStringLiteral("GS Team directory")));

    const QString addButtonName = QStringLiteral("staffDirectoryAddButton");
    auto* const addButton = targetPage->findChild<QPushButton*>(addButtonName);
    QVERIFY(addButton);
    QCOMPARE(targetPage->directoryModel()->rowCount(), 0);
    QVERIFY(!addButton->isEnabled());
    QCOMPARE(targetPage->tableView()->editTriggers(), QAbstractItemView::NoEditTriggers);
    QVERIFY(!targetPage->saveChanges());
    QCOMPARE(otherPage->directoryModel()->rowCount(), 1);
    QVERIFY(otherPage->findChild<QPushButton*>(addButtonName)->isEnabled());

    // Repair the committed synthetic invalid ID and activate the cached route.
    // Normal activation reloads the directory in place and unlocks editing.
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("UPDATE %1 SET id=abs(id) WHERE id<0").arg(targetTable)));
    QVERIFY(shell->navigateTo(*targetPageId));
    QCOMPARE(targetPage->directoryModel()->rowCount(), 1);
    QVERIFY(addButton->isEnabled());
    QVERIFY(targetPage->tableView()->editTriggers() != QAbstractItemView::NoEditTriggers);
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
        QStringLiteral("startup-profile.DB")
        );
    const QString siblingTpsPath = QDir(temporaryDirectory.path()).filePath(
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
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("test-app"),
        QStringLiteral("--startup-performance-workflow"),
        QStringLiteral("--startup-performance-output"),
        performanceOutputPath,
        workspacePath
    };
    seams.recentAutoOpenEnabled = false;
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
    QVERIFY(!QFileInfo::exists(siblingTpsPath));
    QCOMPARE(historyProbe->saveCount, 1);
    QCOMPARE(historyProbe->saves.back().paths.front(), opened->session.location());
    QCOMPARE(historyProbe->saves.back().lastPath,
             std::optional<Application::RecentWorkspacePath>(opened->session.location()));
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

void ApplicationBootstrapTests::teacherProfileUsesTypedOpenAndStaticPageHistory()
{
    QApplication& application = *qApp;
    const QString previousStyleSheet = application.styleSheet();
    StartupPreferencesGuard preferences(
        static_cast<int>(Application::LanguagePreference::English),
        static_cast<int>(Application::Theme::Light)
        );
    StartupSaveModeGuard saveMode(static_cast<int>(Application::SaveMode::Automatic));
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        QStringLiteral("en_US"), QStringLiteral("light")));
    QVERIFY(resources.addTranslation(QStringLiteral("ko_KR")));
    QVERIFY(resources.addResource(QStringLiteral("assets/icons/keyboard_light.svg")));
    QVERIFY(resources.addResource(QStringLiteral("assets/icons/keyboard_dark.svg")));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("teacher-profile-page.tps")
        );
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    QString fixtureError;
    QVERIFY2(createWorkspaceWithFixture(
                 profilePath,
                 QStringLiteral("representative_startup.sql"),
                 &fixtureError
                 ),
        qPrintable(fixtureError));

    const QString triggerConnectionName = QUuid::createUuid().toString(
        QUuid::WithoutBraces
        );
    {
        QSqlDatabase triggerDatabase = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), triggerConnectionName
            );
        triggerDatabase.setDatabaseName(profilePath);
        QVERIFY(triggerDatabase.open());
        QSqlQuery trigger(triggerDatabase);
        QVERIFY2(trigger.exec(QStringLiteral(
            "CREATE TRIGGER reject_teacher_profile_update "
            "BEFORE UPDATE OF teacher_en ON teachers "
            "WHEN NEW.teacher_en = 'Update Failure' "
            "BEGIN SELECT RAISE(ABORT, 'injected profile update failure'); END"
            )), qPrintable(trigger.lastError().text()));
        QVERIFY2(trigger.exec(QStringLiteral(
            "CREATE TRIGGER remove_teacher_after_profile_update "
            "AFTER UPDATE OF teacher_en ON teachers "
            "WHEN NEW.teacher_en = 'Reload Failure' "
            "BEGIN DELETE FROM teachers WHERE id = NEW.id; END"
            )), qPrintable(trigger.lastError().text()));
        QSqlQuery largeTeacher(triggerDatabase);
        QVERIFY(largeTeacher.prepare(QStringLiteral(
            "INSERT INTO teachers ("
            "id, teacher_kr, teacher_en, preferred_romanization, "
            "preferred_name, room_number, birthday, phone_number, "
            "wifi_name, wifi_password, internet_type, zoom_id, "
            "zoom_password, projection_type, notes) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
            )));
        largeTeacher.addBindValue(qint64{2147483650});
        largeTeacher.addBindValue(QStringLiteral("홍길동"));
        largeTeacher.addBindValue(QStringLiteral("Hong Gildong"));
        largeTeacher.addBindValue(QStringLiteral("Hong Gil-dong"));
        largeTeacher.addBindValue(QStringLiteral("Hong Gildong"));
        largeTeacher.addBindValue(QStringLiteral("Room 7"));
        largeTeacher.addBindValue(QStringLiteral("04-18"));
        largeTeacher.addBindValue(QStringLiteral("010-1111-2222"));
        largeTeacher.addBindValue(QStringLiteral("Old WiFi"));
        largeTeacher.addBindValue(QStringLiteral("old-wifi-password"));
        largeTeacher.addBindValue(QStringLiteral("WiFi"));
        largeTeacher.addBindValue(QStringLiteral("old-zoom-id"));
        largeTeacher.addBindValue(QStringLiteral("old-zoom-password"));
        largeTeacher.addBindValue(QStringLiteral("HDMI"));
        largeTeacher.addBindValue(QStringLiteral("Old profile notes"));
        QVERIFY2(largeTeacher.exec(), qPrintable(largeTeacher.lastError().text()));
        triggerDatabase.close();
    }
    QSqlDatabase::removeDatabase(triggerConnectionName);

    auto promptProbe = std::make_shared<UnsavedChangesPromptProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("teacher-profile-test"), profilePath
    };
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.pathExists = [](const Application::WorkspaceLocation& location) {
        return QFileInfo::exists(QString::fromUtf8(
            location.value().data(),
            static_cast<qsizetype>(location.value().size())
            ));
    };
    seams.workspaceLifecycleUserPromptPortFactory = [promptProbe] {
        return std::make_unique<RecordingWorkspaceLifecycleUserPrompt>(promptProbe);
    };
    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        silentStartupDiagnosticsSinkFactory(),
        resolverForRoot(resources.resourceRoot()),
        QStringList{QStringLiteral("en-US")},
        std::move(seams)
        );
    QVERIFY(bootstrap.start().succeeded());

    Ui::ApplicationShell* const shell = bootstrap.shell();
    QVERIFY(shell);
    Ui::NavigationModel* const navigation = shell->navigationModel();
    QVERIFY(navigation);
    const Ui::PageId profilePageId = *Ui::PageId::fromString(
        QStringLiteral("teacher_profile")
        );
    QVERIFY(navigation->isPageVisible(profilePageId));
    QVERIFY(shell->pageHost().isPageRegistered(QStringLiteral("teacher_profile")));
    QVERIFY(shell->pageHost().isPageRegistered(
        QStringLiteral("native_english_teachers")));
    QVERIFY(shell->pageHost().isPageRegistered(QStringLiteral("gs_team")));
    QVERIFY(shell->pageHost().activatePage(
        QStringLiteral("native_english_teachers")));
    auto* const nativeStaffPage = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        shell->pageHost().pageInstance(QStringLiteral("native_english_teachers")));
    QVERIFY(nativeStaffPage);
    QCOMPARE(nativeStaffPage->kind(), Ui::StaffDirectoryKind::NativeEnglishTeachers);
    QCOMPARE(nativeStaffPage->directoryModel()->columnCount(), 6);
    QVERIFY(shell->pageHost().activatePage(QStringLiteral("gs_team")));
    auto* const gsStaffPage = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        shell->pageHost().pageInstance(QStringLiteral("gs_team")));
    QVERIFY(gsStaffPage);
    QCOMPARE(gsStaffPage->kind(), Ui::StaffDirectoryKind::GsTeam);
    QCOMPARE(gsStaffPage->directoryModel()->columnCount(), 5);
    QVERIFY(shell->pageHost().activatePage(QStringLiteral("my_workspace")));
    auto* const nextSidebarTree = shell->findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(nextSidebarTree);
    const auto findNodeById = [nextSidebarTree](const QString& id) {
        QTreeWidgetItemIterator iterator(nextSidebarTree);
        while (*iterator)
        {
            if ((*iterator)->data(0, Qt::UserRole + 2).toString() == id)
            {
                return *iterator;
            }
            ++iterator;
        }
        return static_cast<QTreeWidgetItem*>(nullptr);
    };
    QTreeWidgetItem* const koreanTeachers = findNodeById(
        QStringLiteral("teachers_all_korean")
        );
    QVERIFY(koreanTeachers);
    QCOMPARE(koreanTeachers->childCount(), 5);
    const auto findTeacherRow = [koreanTeachers](const QString& id) {
        for (int index = 0; index < koreanTeachers->childCount(); ++index)
        {
            QTreeWidgetItem* const item = koreanTeachers->child(index);
            if (item->data(0, Qt::UserRole + 3).toString() == id)
            {
                return item;
            }
        }
        return static_cast<QTreeWidgetItem*>(nullptr);
    };
    QTreeWidgetItem* const firstTeacherRow = findTeacherRow(QStringLiteral("1"));
    QTreeWidgetItem* const secondTeacherRow = findTeacherRow(QStringLiteral("2"));
    QTreeWidgetItem* const unreadableTeacherRow = findTeacherRow(
        QStringLiteral("4")
        );
    QVERIFY(firstTeacherRow);
    QVERIFY(secondTeacherRow);
    QVERIFY(unreadableTeacherRow);

    // A direct route selection has a defined empty state and no feature ID.
    QVERIFY(shell->navigateTo(profilePageId));
    auto* page = dynamic_cast<Ui::TeacherProfilePage*>(
        shell->pageHost().pageInstance(QStringLiteral("teacher_profile"))
        );
    QVERIFY(page);
    QVERIFY(!page->selectedTeacherId().has_value());
    QLabel* const noSelection = page->findChild<QLabel*>(
        QStringLiteral("teacherProfileNoSelectionState")
        );
    QVERIFY(noSelection);
    QVERIFY(noSelection->isVisible());
    QVERIFY(shell->navigateBack());
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("my_workspace"));

    // A single sidebar row click reads and opens the static profile route.
    nextSidebarTree->setCurrentItem(firstTeacherRow);
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("teacher_profile"));
    page = dynamic_cast<Ui::TeacherProfilePage*>(
        shell->pageHost().pageInstance(QStringLiteral("teacher_profile"))
        );
    QVERIFY(page);
    QCOMPARE(
        page->selectedTeacherId(),
        std::optional{Domain::TeacherId::fromString("1").value()}
        );
    QCOMPARE(navigation->history().size(), std::size_t(2));

    // A dirty profile canceled from a second sidebar row keeps the original
    // profile draft, selected row, and route history.
    QLineEdit* const rowGuardEnglishName = page->findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit")
        );
    QVERIFY(rowGuardEnglishName);
    rowGuardEnglishName->setText(QStringLiteral("Dirty row switch draft"));
    QVERIFY(page->hasUnsavedChanges());
    const std::vector<Ui::PageId> historyBeforeCanceledRowSwitch =
        navigation->history();
    const std::size_t promptsBeforeCanceledRowSwitch =
        promptProbe->requests.size();
    promptProbe->action = Application::UnsavedChangesAction::Cancel;
    nextSidebarTree->setCurrentItem(secondTeacherRow);
    QCOMPARE(
        promptProbe->requests.size(),
        promptsBeforeCanceledRowSwitch + std::size_t(1)
        );
    QCOMPARE(page->selectedTeacherId(),
        std::optional{Domain::TeacherId::fromString("1").value()});
    QCOMPARE(rowGuardEnglishName->text(),
        QStringLiteral("Dirty row switch draft"));
    QCOMPARE(navigation->history(), historyBeforeCanceledRowSwitch);
    QCOMPARE(
        nextSidebarTree->currentItem()->data(0, Qt::UserRole + 3).toString(),
        QStringLiteral("1")
        );
    QVERIFY(page->discardChanges());
    promptProbe->requests.clear();

    // Switching IDs changes page state without adding a teacher-specific route.
    const std::vector<Ui::PageId> historyBeforeSecondTeacher =
        navigation->history();
    nextSidebarTree->setCurrentItem(secondTeacherRow);
    QCOMPARE(navigation->history(), historyBeforeSecondTeacher);
    QCOMPARE(
        page->selectedTeacherId(),
        std::optional{Domain::TeacherId::fromString("2").value()}
        );

    QVERIFY(shell->navigateBack());
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("my_workspace"));
    nextSidebarTree->setCurrentItem(secondTeacherRow);
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("teacher_profile"));
    QCOMPARE(navigation->history().size(), std::size_t(2));

    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral(
            "CREATE TRIGGER reject_teacher_delete "
            "BEFORE DELETE ON teachers WHEN OLD.id=2 "
            "BEGIN SELECT RAISE(ABORT, 'injected teacher delete failure'); END"
            )
        ));
    bool deleteActionTriggered = false;
    bool deleteConfirmationAccepted = false;
    QTimer confirmationTimer;
    confirmationTimer.setInterval(10);
    QObject::connect(
        &confirmationTimer,
        &QTimer::timeout,
        shell,
        [&] {
            auto* const confirmation = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()
                );
            if (!confirmation)
            {
                return;
            }
            QAbstractButton* const yesButton = confirmation->button(QMessageBox::Yes);
            if (!yesButton)
            {
                return;
            }
            deleteConfirmationAccepted = true;
            confirmationTimer.stop();
            yesButton->click();
        }
        );
    confirmationTimer.start();
    const QRect selectedTeacherRect =
        nextSidebarTree->visualItemRect(secondTeacherRow);
    QTimer::singleShot(0, shell, [&] {
        auto* const menu = qobject_cast<QMenu*>(
            QApplication::activePopupWidget()
            );
        if (!menu)
        {
            return;
        }
        QAction* const deleteAction = menu->findChild<QAction*>(
            QStringLiteral("koreanTeacherDeleteContextAction")
            );
        if (!deleteAction || !deleteAction->isEnabled())
        {
            menu->close();
            return;
        }
        QObject::connect(
            deleteAction,
            &QAction::triggered,
            shell,
            [&] { deleteActionTriggered = true; }
            );
        QTest::mouseClick(
            menu,
            Qt::LeftButton,
            Qt::NoModifier,
            menu->actionGeometry(deleteAction).center()
            );
    });
    const QPoint selectedTeacherPosition = selectedTeacherRect.center();
    QContextMenuEvent deleteContextEvent(
        QContextMenuEvent::Mouse,
        selectedTeacherPosition,
        nextSidebarTree->viewport()->mapToGlobal(selectedTeacherPosition)
        );
    QApplication::sendEvent(nextSidebarTree->viewport(), &deleteContextEvent);
    QVERIFY(deleteActionTriggered);
    QVERIFY(deleteConfirmationAccepted);
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("teacher_profile"));
    QCOMPARE(nextSidebarTree->currentItem(), secondTeacherRow);
    QCOMPARE(nextSidebarTree->selectedItems().size(), 1);
    QCOMPARE(nextSidebarTree->selectedItems().front(), secondTeacherRow);
    QCOMPARE(
        secondTeacherRow->data(0, Qt::UserRole + 3).toString(),
        QStringLiteral("2")
        );
    QCOMPARE(
        page->selectedTeacherId(),
        std::optional{Domain::TeacherId::fromString("2").value()}
        );
    QVERIFY(shell->statusBar()->currentMessage().contains(QStringLiteral("Error:")));
    QVERIFY(shell->notificationPort().clear());

    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("UPDATE class_info SET teacher_id=NULL WHERE teacher_id=4")
        ));
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("DELETE FROM teachers WHERE id=4")
        ));
    QLineEdit* const failedRowDraftField = page->findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit")
        );
    QVERIFY(failedRowDraftField);
    failedRowDraftField->setText(
        QStringLiteral("Unsaved draft after failed row read")
        );
    QVERIFY(page->hasUnsavedChanges());
    const std::vector<Ui::PageId> historyBeforeFailedRow = navigation->history();
    const std::size_t promptsBeforeFailedRow = promptProbe->requests.size();
    QVERIFY(shell->notificationPort().clear());
    QVERIFY(shell->statusBar()->currentMessage().isEmpty());
    nextSidebarTree->setCurrentItem(unreadableTeacherRow);
    QCOMPARE(
        navigation->currentPageId()->value(),
        QStringLiteral("teacher_profile")
        );
    QCOMPARE(navigation->history(), historyBeforeFailedRow);
    QCOMPARE(
        nextSidebarTree->currentItem()->data(0, Qt::UserRole + 3).toString(),
        QStringLiteral("2")
        );
    QCOMPARE(
        page->selectedTeacherId(),
        std::optional{Domain::TeacherId::fromString("2").value()}
        );
    QCOMPARE(failedRowDraftField->text(),
        QStringLiteral("Unsaved draft after failed row read"));
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(promptProbe->requests.size(), promptsBeforeFailedRow);
    QVERIFY(shell->statusBar()->currentMessage().isEmpty());
    QVERIFY(!findNodeById(QStringLiteral("teacher_profile")));
    QVERIFY(page->discardChanges());

    const Domain::TeacherId firstTeacher =
        Domain::TeacherId::fromString("1").value();
    const Domain::TeacherId secondTeacher =
        Domain::TeacherId::fromString("2").value();
    const Domain::TeacherId thirdTeacher =
        Domain::TeacherId::fromString("3").value();
    QVERIFY(bootstrap.openTeacherProfile(firstTeacher));
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("teacher_profile"));
    QCOMPARE(page->selectedTeacherId(), std::optional{firstTeacher});
    auto* const englishName = page->findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit")
        );
    QVERIFY(englishName);
    QCOMPARE(englishName->text(), QStringLiteral("Dana Kim"));
    auto* const keyboardButton = page->findChild<QPushButton*>(
        QStringLiteral("teacherProfileKoreanKeyboardButton")
        );
    auto* const onScreenKeyboard = page->findChild<OnScreenKeyboard*>(
        QStringLiteral("onScreenKeyboard")
        );
    QVERIFY(keyboardButton);
    QVERIFY(onScreenKeyboard);
    const QString originalEnglishName = englishName->text();
    QTest::mouseClick(keyboardButton, Qt::LeftButton);
    englishName->setFocus();
    QFocusEvent englishNameFocus(QEvent::FocusIn, Qt::OtherFocusReason);
    QCoreApplication::sendEvent(englishName, &englishNameFocus);
    QTRY_COMPARE(onScreenKeyboard->target(), static_cast<QWidget*>(englishName));
    auto* const keyboardRKey = onScreenKeyboard->findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        );
    auto* const keyboardKKey = onScreenKeyboard->findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        );
    auto* const keyboardEnterKey = onScreenKeyboard->findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardEnter")
        );
    QVERIFY(keyboardRKey);
    QVERIFY(keyboardKKey);
    QVERIFY(keyboardEnterKey);
    keyboardRKey->click();
    keyboardKKey->click();
    keyboardEnterKey->click();
    QCOMPARE(englishName->text(), originalEnglishName + QStringLiteral("가"));
    englishName->setText(originalEnglishName);
    onScreenKeyboard->close();
    auto* const saveButton = page->findChild<QPushButton*>(
        QStringLiteral("teacherProfileSaveButton")
        );
    QVERIFY(saveButton);
    QVERIFY(saveButton->isHidden());

    // The default save mode is automatic and runs through the direct-store
    // profile edit use case after the established debounce interval.
    englishName->setText(QStringLiteral("Dana Kim Auto Saved"));
    auto* const birthday = page->findChild<QLineEdit*>(
        QStringLiteral("birthdayEdit")
        );
    QVERIFY(birthday);
    QTest::qWait(1000);
    QString validationMessages;
    for (QLabel* const label : page->findChildren<QLabel*>())
    {
        if (label->objectName().endsWith(QStringLiteral("ValidationMessage"))
            && label->isVisible())
        {
            validationMessages += label->text() + QStringLiteral("; ");
        }
    }
    auto* const saveStatus = page->findChild<QLabel*>(
        QStringLiteral("teacherProfileSaveStatus")
        );
    QVERIFY2(!page->hasUnsavedChanges(),
        qPrintable((saveStatus ? saveStatus->text() : QString{})
            + QStringLiteral(" | ") + validationMessages));
    QCOMPARE(englishName->text(), QStringLiteral("Dana Kim Auto Saved"));
    QCOMPARE(
        birthday->text(),
        QLocale().toString(QDate(2000, 1, 14), QStringLiteral("MMMM d"))
        );

    // An automatic persistence failure keeps the live draft and reports the
    // failure without blocking the UI thread on a dialog.
    englishName->setText(QStringLiteral("Update Failure"));
    QTest::qWait(1000);
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(englishName->text(), QStringLiteral("Update Failure"));
    QVERIFY(saveStatus->text().contains(QStringLiteral("draft is still here")));
    QVERIFY(page->discardChanges());
    QCOMPARE(englishName->text(), QStringLiteral("Dana Kim Auto Saved"));

    // Back and Forward track only the single static page route. The selected
    // teacher remains page state when the page is suspended and restored.
    QVERIFY(shell->navigateBack());
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("my_workspace"));
    QVERIFY(shell->navigateForward());
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("teacher_profile"));
    QCOMPARE(page->selectedTeacherId(), std::optional{firstTeacher});

    page->setSaveMode(Application::SaveMode::Manual);
    auto* const phoneNumber = page->findChild<QLineEdit*>(
        QStringLiteral("phoneNumberEdit")
        );
    QVERIFY(phoneNumber);
    const QString savedPhoneNumber = phoneNumber->text();
    phoneNumber->setText(QString(33, QLatin1Char('1')));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(!saveButton->isEnabled());
    QVERIFY(!page->saveChanges());
    QVERIFY(page->hasUnsavedChanges());
    phoneNumber->setText(savedPhoneNumber);

    englishName->setText(QStringLiteral("Unsaved draft kept on cancel"));
    const std::vector<Ui::PageId> historyBeforeSwitch = navigation->history();
    QVERIFY(!bootstrap.openTeacherProfile(
        Domain::TeacherId::fromString("999").value()
        ));
    QVERIFY(shell->statusBar()->currentMessage().contains(QStringLiteral("Error:")));
    QVERIFY(shell->notificationPort().clear());
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(page->selectedTeacherId(), std::optional{firstTeacher});
    QCOMPARE(englishName->text(), QStringLiteral("Unsaved draft kept on cancel"));
    QCOMPARE(promptProbe->requests.size(), std::size_t(0));

    // The second ID is read before dirty resolution. Cancel retains the old
    // profile draft and selection because ApplicationShell sees one static ID.
    promptProbe->action = Application::UnsavedChangesAction::Cancel;
    QVERIFY(!bootstrap.openTeacherProfile(secondTeacher));
    QCOMPARE(promptProbe->requests.size(), std::size_t(1));
    QCOMPARE(page->selectedTeacherId(), std::optional{firstTeacher});
    QCOMPARE(englishName->text(), QStringLiteral("Unsaved draft kept on cancel"));
    QCOMPARE(navigation->history(), historyBeforeSwitch);

    promptProbe->action = Application::UnsavedChangesAction::Discard;
    QVERIFY(bootstrap.openTeacherProfile(secondTeacher));
    QCOMPARE(promptProbe->requests.size(), std::size_t(2));
    QCOMPARE(page->selectedTeacherId(), std::optional{secondTeacher});
    QCOMPARE(englishName->text(), QStringLiteral("Jordan Lee"));
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(navigation->history(), historyBeforeSwitch);

    // Saving during an ID switch completes the update and reload before the
    // requested third profile replaces the visible draft.
    englishName->setText(QStringLiteral("Jordan Lee Saved"));
    promptProbe->action = Application::UnsavedChangesAction::Save;
    QVERIFY(bootstrap.openTeacherProfile(thirdTeacher));
    QCOMPARE(promptProbe->requests.size(), std::size_t(3));
    QCOMPARE(page->selectedTeacherId(), std::optional{thirdTeacher});
    QCOMPARE(englishName->text(), QStringLiteral("Minseo Park"));

    // A completed write followed by a failed reload has a distinct locked
    // state: the submitted values remain visible and are no longer dirty.
    englishName->setText(QStringLiteral("Reload Failure"));
    bool saveWarningDismissed = false;
    QTimer::singleShot(0, [&saveWarningDismissed] {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            saveWarningDismissed = true;
            modal->close();
        }
    });
    QVERIFY(!page->saveChanges());
    QVERIFY(saveWarningDismissed);
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(englishName->text(), QStringLiteral("Reload Failure"));
    QVERIFY(!englishName->isEnabled());
    QVERIFY(page->findChild<QLabel*>(
        QStringLiteral("teacherProfileSaveStatus")
        )->property("teacherProfileReloadRequired").toBool());

    QVERIFY(bootstrap.openTeacherProfile(secondTeacher));
    QCOMPARE(page->selectedTeacherId(), std::optional{secondTeacher});
    QCOMPARE(englishName->text(), QStringLiteral("Jordan Lee Saved"));
    QCOMPARE(navigation->history(), historyBeforeSwitch);
    QVERIFY(shell->navigateBack());
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("my_workspace"));
    QVERIFY(shell->navigateForward());
    QCOMPARE(navigation->currentPageId()->value(), QStringLiteral("teacher_profile"));
    QCOMPARE(page->selectedTeacherId(), std::optional{secondTeacher});

    // Exercise the full typed-ID read/update/reload path with a value above
    // the legacy int range, and round-trip every TeacherProfileFields member.
    const Domain::TeacherId largeTeacherId =
        Domain::TeacherId::fromString("2147483650").value();
    QVERIFY(bootstrap.openTeacherProfile(largeTeacherId));
    QCOMPARE(page->selectedTeacherId(), std::optional{largeTeacherId});
    QCOMPARE(englishName->text(), QStringLiteral("Hong Gildong"));

    auto* const koreanName = page->findChild<QLineEdit*>(
        QStringLiteral("teacherKrEdit")
        );
    auto* const romanization = page->findChild<QLineEdit*>(
        QStringLiteral("preferredRomanizationEdit")
        );
    auto* const preferredName = page->findChild<QComboBox*>(
        QStringLiteral("preferredNameCombo")
        );
    auto* const roomNumber = page->findChild<QLineEdit*>(
        QStringLiteral("roomNumberEdit")
        );
    auto* const phoneNumberEdit = page->findChild<QLineEdit*>(
        QStringLiteral("phoneNumberEdit")
        );
    auto* const wifiName = page->findChild<QLineEdit*>(
        QStringLiteral("wifiNameEdit")
        );
    auto* const wifiPassword = page->findChild<QLineEdit*>(
        QStringLiteral("wifiPasswordEdit")
        );
    auto* const internetType = page->findChild<QComboBox*>(
        QStringLiteral("internetTypeCombo")
        );
    auto* const zoomId = page->findChild<QLineEdit*>(
        QStringLiteral("zoomIdEdit")
        );
    auto* const zoomPassword = page->findChild<QLineEdit*>(
        QStringLiteral("zoomPasswordEdit")
        );
    auto* const projectionType = page->findChild<QComboBox*>(
        QStringLiteral("projectionTypeCombo")
        );
    auto* const notes = page->findChild<QPlainTextEdit*>(
        QStringLiteral("teacherNotesEdit")
        );
    QVERIFY(koreanName);
    QVERIFY(romanization);
    QVERIFY(preferredName);
    QVERIFY(roomNumber);
    QVERIFY(phoneNumberEdit);
    QVERIFY(wifiName);
    QVERIFY(wifiPassword);
    QVERIFY(internetType);
    QVERIFY(zoomId);
    QVERIFY(zoomPassword);
    QVERIFY(projectionType);
    QVERIFY(notes);

    koreanName->setText(QStringLiteral("김가나다"));
    englishName->setText(QStringLiteral("Alexandra"));
    romanization->setText(QStringLiteral("Alexandra Kim"));
    QVERIFY(preferredName->findText(QStringLiteral("Alexandra")) >= 0);
    preferredName->setCurrentText(QStringLiteral("Alexandra"));
    roomNumber->setText(QStringLiteral("Room 42"));
    phoneNumberEdit->setText(QStringLiteral("010-9999-8888"));
    wifiName->setText(QStringLiteral("Studio WiFi"));
    wifiPassword->setText(QStringLiteral("wifi-new-password"));
    internetType->setCurrentIndex(internetType->findData(QStringLiteral("LAN")));
    zoomId->setText(QStringLiteral("new-zoom-id"));
    zoomPassword->setText(QStringLiteral("new-zoom-password"));
    projectionType->setCurrentIndex(
        projectionType->findData(QStringLiteral("Zoom"))
        );
    notes->setPlainText(QStringLiteral("Round-trip notes for every profile field."));

    QTest::mouseClick(birthday, Qt::LeftButton);
    QApplication::processEvents();
    QWidget* const birthdayPopup = QApplication::activePopupWidget();
    QVERIFY(birthdayPopup);
    auto* const calendar = birthdayPopup->findChild<QCalendarWidget*>(
        QStringLiteral("teacherBirthdayCalendar")
        );
    QVERIFY(calendar);
    const QDate selectedBirthday(2000, 10, 5);
    calendar->setFirstDayOfWeek(Qt::Monday);
    calendar->setCurrentPage(selectedBirthday.year(), selectedBirthday.month());
    calendar->setSelectedDate(selectedBirthday);
    auto* const calendarView = calendar->findChild<QTableView*>();
    QVERIFY(calendarView);
    const QDate firstOfMonth(selectedBirthday.year(), selectedBirthday.month(), 1);
    const int firstDayOffset = (firstOfMonth.dayOfWeek()
        - static_cast<int>(Qt::Monday) + 7) % 7;
    const int selectedDayIndex = firstDayOffset + selectedBirthday.day() - 1;
    // The calendar's internal table includes a weekday-header row at index 0.
    const QModelIndex selectedDay = calendarView->model()->index(
        1 + selectedDayIndex / 7,
        selectedDayIndex % 7
        );
    QVERIFY(selectedDay.isValid());
    const QRect selectedDayRect = calendarView->visualRect(selectedDay);
    QVERIFY(selectedDayRect.isValid());
    QTest::mouseClick(
        calendarView->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        selectedDayRect.center()
        );
    QCOMPARE(
        birthday->text(),
        QLocale().toString(selectedBirthday, QStringLiteral("MMMM d"))
        );

    auto* const koreanNameWarning = page->findChild<QLabel*>(
        QStringLiteral("teacherKrValidationMessage")
        );
    QVERIFY(koreanNameWarning);
    QCOMPARE(
        koreanNameWarning->property("formValidationSeverity").toString(),
        QStringLiteral("warning")
        );
    QVERIFY(saveButton->isEnabled());
    QVERIFY(page->hasUnsavedChanges());
    const Ui::PageOperationResult<void> largeProfileSave = page->saveChanges();
    QString profileValidationDiagnostics;
    for (QLabel* const label : page->findChildren<QLabel*>())
    {
        if (label->objectName().endsWith(QStringLiteral("ValidationMessage"))
            && !label->isHidden())
        {
            profileValidationDiagnostics += label->objectName()
                + QStringLiteral(": ") + label->text()
                + QStringLiteral("; ");
        }
    }
    QVERIFY2(largeProfileSave.has_value(), qPrintable(
        saveStatus->text() + QStringLiteral(" | ")
            + profileValidationDiagnostics
        ));
    QVERIFY(!page->hasUnsavedChanges());

    const auto readStoredBirthday = [&profilePath]()
        -> std::optional<QString> {
        const QString connectionName = QUuid::createUuid().toString(
            QUuid::WithoutBraces
            );
        std::optional<QString> value;
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName
                );
            database.setDatabaseName(profilePath);
            if (database.open())
            {
                QSqlQuery query(database);
                if (query.exec(QStringLiteral(
                        "SELECT birthday FROM teachers "
                        "WHERE id = 2147483650"
                        ))
                    && query.next())
                {
                    value = query.value(0).toString();
                }
            }
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
        return value;
    };
    QCOMPARE(readStoredBirthday(), std::optional{QStringLiteral("10-05")});

    // Switch away and reopen so the assertions read the repository result,
    // rather than only inspecting the form after its update callback.
    QVERIFY(bootstrap.openTeacherProfile(firstTeacher));
    QVERIFY(bootstrap.openTeacherProfile(largeTeacherId));
    QCOMPARE(page->selectedTeacherId(), std::optional{largeTeacherId});
    QCOMPARE(koreanName->text(), QStringLiteral("김가나다"));
    QCOMPARE(englishName->text(), QStringLiteral("Alexandra"));
    QCOMPARE(romanization->text(), QStringLiteral("Alexandra Kim"));
    QCOMPARE(preferredName->currentText(), QStringLiteral("Alexandra"));
    QCOMPARE(roomNumber->text(), QStringLiteral("Room 42"));
    QCOMPARE(
        birthday->text(),
        QLocale().toString(selectedBirthday, QStringLiteral("MMMM d"))
        );
    QCOMPARE(phoneNumberEdit->text(), QStringLiteral("010-9999-8888"));
    QCOMPARE(wifiName->text(), QStringLiteral("Studio WiFi"));
    QCOMPARE(wifiPassword->text(), QStringLiteral("wifi-new-password"));
    QCOMPARE(internetType->currentData().toString(), QStringLiteral("LAN"));
    QCOMPARE(zoomId->text(), QStringLiteral("new-zoom-id"));
    QCOMPARE(zoomPassword->text(), QStringLiteral("new-zoom-password"));
    QCOMPARE(projectionType->currentData().toString(), QStringLiteral("Zoom"));
    QCOMPARE(notes->toPlainText(), QStringLiteral("Round-trip notes for every profile field."));

    auto* const birthdayClearButton = birthday->findChild<QToolButton*>();
    QVERIFY(birthdayClearButton);
    QVERIFY(birthdayClearButton->isVisible());
    QVERIFY(birthdayClearButton->isEnabled());
    QTest::mouseClick(birthdayClearButton, Qt::LeftButton);
    QVERIFY(birthday->text().isEmpty());
    QVERIFY(page->saveChanges());
    QCOMPARE(readStoredBirthday(), std::optional{QString{}});
    QVERIFY(bootstrap.openTeacherProfile(firstTeacher));
    QVERIFY(bootstrap.openTeacherProfile(largeTeacherId));
    QVERIFY(birthday->text().isEmpty());

    // Install the real Korean QM through the page ResourceLoader, then remove
    // it to return to the English translator loaded at application startup.
    englishName->setText(QStringLiteral("Locale draft"));
    QVERIFY(page->hasUnsavedChanges());
    const auto koreanTranslationId = Resources::ResourceId::fromString(
        QStringLiteral("resource:assets/translations/ClassMngr_ko_KR.ts")
        );
    QVERIFY(koreanTranslationId.has_value());
    const auto koreanTranslationRequest = Resources::ResourceRequest::create(
        *koreanTranslationId,
        Resources::ResourceScopeKind::Operation,
        QStringLiteral("teacher-profile-language-test")
        );
    QVERIFY(koreanTranslationRequest.has_value());
    auto loadedKoreanTranslation = page->resourceContext().resourceLoader()
        .loadTranslation(*koreanTranslationRequest, application);
    QVERIFY(loadedKoreanTranslation.has_value());
    std::optional<Resources::ScopedResource<Resources::ResourceTranslation>>
        installedKoreanTranslation;
    installedKoreanTranslation.emplace(std::move(*loadedKoreanTranslation));
    QLabel* const profileTitle = page->findChild<QLabel*>(QStringLiteral("pageTitle"));
    QVERIFY(profileTitle);
    QLabel* const pageSubtitle = page->findChild<QLabel*>(
        QStringLiteral("pageSubtitle")
        );
    QVERIFY(pageSubtitle);
    QEvent koreanLanguageChange(QEvent::LanguageChange);
    QApplication::sendEvent(page, &koreanLanguageChange);
    QCOMPARE(
        page->findChild<QLabel*>(QStringLiteral("teacherKrLabel"))->text(),
        QStringLiteral("한국어 이름")
        );
    QCOMPARE(
        profileTitle->text(),
        QStringLiteral("Locale draft 선생님 정보")
        );
    QCOMPARE(
        pageSubtitle->text(),
        QStringLiteral("교사 세부 정보를 확인하고 관리합니다.")
        );
    QCOMPARE(
        page->findChild<QLabel*>(
            QStringLiteral("preferredRomanizationLabel")
            )->text(),
        QStringLiteral("선호 표기")
        );
    QCOMPARE(
        page->findChild<QLabel*>(
            QStringLiteral("projectionTypeLabel")
            )->text(),
        QStringLiteral("프로젝션 유형")
        );
    auto* const notesCard = page->findChild<QWidget*>(
        QStringLiteral("teacherProfileNotesCard")
        );
    QVERIFY(notesCard);
    QCOMPARE(
        notesCard->findChild<QLabel*>(QStringLiteral("sectionTitle"))->text(),
        QStringLiteral("노트")
        );
    QCOMPARE(
        page->findChild<QPushButton*>(
            QStringLiteral("teacherProfileKoreanKeyboardButton")
            )->toolTip(),
        QStringLiteral("한/영 화면 키보드 열기")
        );
    QCOMPARE(
        saveStatus->text(),
        QStringLiteral("강조 표시된 경고를 확인하세요. 계속 저장할 수 있습니다.")
        );
    QCOMPARE(englishName->text(), QStringLiteral("Locale draft"));
    QVERIFY(page->hasUnsavedChanges());

    installedKoreanTranslation.reset();
    QEvent englishLanguageChange(QEvent::LanguageChange);
    QApplication::sendEvent(page, &englishLanguageChange);
    QCOMPARE(
        page->findChild<QLabel*>(QStringLiteral("teacherKrLabel"))->text(),
        QStringLiteral("Korean Name")
        );
    QCOMPARE(
        profileTitle->text(),
        QStringLiteral("Teacher Information for Locale draft")
        );
    QCOMPARE(
        notesCard->findChild<QLabel*>(QStringLiteral("sectionTitle"))->text(),
        QStringLiteral("Notes")
        );
    QCOMPARE(
        saveStatus->text(),
        QStringLiteral("Review the highlighted warnings. You can still save.")
        );
    QCOMPARE(englishName->text(), QStringLiteral("Locale draft"));
    QVERIFY(page->hasUnsavedChanges());

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    application.setStyleSheet(previousStyleSheet);
}

QTEST_MAIN(ApplicationBootstrapTests)

#include "application_bootstrap_tests.moc"
