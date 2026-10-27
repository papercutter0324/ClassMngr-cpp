#include "next/bootstrap/application_bootstrap.h"

#include "next/bootstrap/qt_startup_diagnostics_sink.h"
#include "next/application/recent_workspace_history_use_case.h"
#include "next/application/normal_workspace_creation_flow.h"
#include "next/application/open_existing_workspace_flow.h"
#include "next/application/workspace_save_as_flow.h"
#include "next/application/workspace_export_flow.h"
#include "next/application/unsaved_changes_resolution_flow.h"
#include "next/application/initial_setup_class_details_requirements.h"
#include "next/application/positive_decimal_id_validation.h"
#include "core/settingsmanager.h"
#include "next/platform/settings_manager_recent_workspace_history_port.h"
#include "next/platform/settings_manager_excel_import_timeout_port.h"
#include "next/platform/settings_manager_preferences_port.h"
#include "next/platform/settings_manager_save_mode_preferences_port.h"
#include "next/platform/resource_document_catalog_port.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/platform/workspace_store_creation_lifecycle_adapter.h"
#include "next/platform/workspace_profile_path_policy.h"
#include "next/platform/workspace_interrupted_recovery_port.h"
#include "next/platform/workspace_store_initial_setup_lifecycle_adapter.h"
#include "next/platform/qsettings_file_dialog_directory_preferences_adapter.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/ui/application_shell.h"
#include "next/ui/font_size_presenter.h"
#include "next/ui/initial_page.h"
#include "next/ui/page_host.h"
#include "next/ui/page_host_active_page_changes_adapter.h"
#include "next/ui/placeholder_page.h"
#include "next/ui/qt_file_dialog_presenter.h"
#include "next/ui/qt_user_prompt_presenter.h"
#include "next/ui/theme_presenter.h"
#include "next/ui/teacher_profile_page.h"
#include "next/platform/teacher_validator_teacher_profile_validation_policy.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStringList>
#include <QTimer>

#include <cstdio>
#include <cstring>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ClassMngr::Next::Bootstrap
{
namespace
{

[[nodiscard]] Resources::ResourceResult<Resources::ResourceRequest> startupRequest(
    const Resources::ResourceScope& scope,
    const QString& logicalId
    )
{
    const auto id = Resources::ResourceId::fromString(logicalId);
    if (!id)
    {
        return std::unexpected(Resources::ResourceError{
            Resources::ResourceErrorCode::InvalidArgument,
            QStringLiteral("startup resource ID is invalid")
        });
    }
    return Resources::ResourceRequest::create(*id, scope);
}

[[nodiscard]] QString englishLocaleFor(const QStringList& uiLanguages)
{
    static const QStringList supportedLocales{
        QStringLiteral("en_AU"),
        QStringLiteral("en_CA"),
        QStringLiteral("en_GB"),
        QStringLiteral("en_US")
    };

    for (const QString& uiLanguage : uiLanguages)
    {
        const QLocale locale(uiLanguage);
        const QString localeName = locale.name();
        if (locale.language() == QLocale::English
            && supportedLocales.contains(localeName))
        {
            return localeName;
        }
    }

    return QStringLiteral("en_US");
}

[[nodiscard]] QString supportedSystemLocale(const QStringList& uiLanguages)
{
    static const QStringList supportedLocales{
        QStringLiteral("en_AU"),
        QStringLiteral("en_CA"),
        QStringLiteral("en_GB"),
        QStringLiteral("en_US"),
        QStringLiteral("ko_KR")
    };

    for (const QString& uiLanguage : uiLanguages)
    {
        const QString localeName = QLocale(uiLanguage).name();
        if (supportedLocales.contains(localeName))
        {
            return localeName;
        }
    }

    return {};
}

[[nodiscard]] bool isSupportedStartupWorkspacePath(const QString& path)
{
    return path.endsWith(QStringLiteral(".tps"), Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive);
}

[[nodiscard]] QString normalizedStartupWorkspacePath(const QString& path)
{
    QString supportedPath = path;
    if (QFileInfo(supportedPath).suffix().isEmpty())
    {
        supportedPath += QStringLiteral(".tps");
    }
    return QFileInfo(supportedPath).absoluteFilePath();
}

[[nodiscard]] Application::StartupWorkspaceOpenFailed startupWorkspaceException()
{
    return Application::StartupWorkspaceOpenFailed{
        Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Startup workspace composition failed.",
            .recoverable = false
        }
    };
}

[[nodiscard]] QString navigationLabel(const char* sourceText)
{
    return QCoreApplication::translate("NextNavigation", sourceText);
}

[[nodiscard]] Ui::PageId pageId(const QString& value)
{
    return *Ui::PageId::fromString(value);
}

[[nodiscard]] Ui::PageDescriptor placeholderDescriptor(
    const QString& id,
    const QString& label
    )
{
    return Ui::PageDescriptor{
        .id = pageId(id),
        .label = label,
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = [label](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            std::unique_ptr<Ui::Page> page =
                std::make_unique<Ui::PlaceholderPage>(context, label);
            return Ui::PageOperationResult<std::unique_ptr<Ui::Page>>{
                std::move(page)
            };
        },
        .resourcePolicy = Ui::PageResourcePolicy::PageScoped,
        .initialDataPolicy = Ui::PageInitialDataPolicy::None
    };
}

[[nodiscard]] std::vector<Ui::PageDescriptor> placeholderPageDescriptors()
{
    return {
        placeholderDescriptor(
            QStringLiteral("my_workspace"), navigationLabel("My Workspace")
            ),
        placeholderDescriptor(QStringLiteral("classes"), navigationLabel("Classes")),
        placeholderDescriptor(QStringLiteral("sub_prep"), navigationLabel("Sub Prep")),
        placeholderDescriptor(
            QStringLiteral("native_english_teachers"),
            navigationLabel("Native English Teachers")
            ),
        placeholderDescriptor(QStringLiteral("gs_team"), navigationLabel("GS Team")),
        placeholderDescriptor(
            QStringLiteral("campus_information"), navigationLabel("Information")
            ),
        placeholderDescriptor(
            QStringLiteral("campus_directions"), navigationLabel("Directions")
            ),
        placeholderDescriptor(QStringLiteral("campus_address"), navigationLabel("Address")),
        placeholderDescriptor(QStringLiteral("campus_housing"), navigationLabel("Housing")),
        placeholderDescriptor(QStringLiteral("campus_map"), navigationLabel("Maps"))
    };
}

[[nodiscard]] Ui::PageDescriptor initialSetupPageDescriptor(
    std::function<Ui::InitialSetupPageActions()> actionsFactory
    )
{
    return Ui::PageDescriptor{
        .id = pageId(QStringLiteral("initial_setup")),
        .label = navigationLabel("Initial Setup"),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = [actionsFactory = std::move(actionsFactory)](
                       const Ui::PageContext& context
                       ) mutable
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            std::unique_ptr<Ui::Page> page =
                std::make_unique<Ui::InitialPage>(context, actionsFactory());
            return Ui::PageOperationResult<std::unique_ptr<Ui::Page>>{
                std::move(page)
            };
        },
        .resourcePolicy = Ui::PageResourcePolicy::PageScoped,
        .initialDataPolicy = Ui::PageInitialDataPolicy::None
    };
}

[[nodiscard]] Ui::PageDescriptor teacherProfilePageDescriptor(
    std::function<Ui::TeacherProfilePageActions()> actionsFactory
    )
{
    return Ui::PageDescriptor{
        .id = pageId(QStringLiteral("teacher_profile")),
        .label = navigationLabel("Teacher Profile"),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = [actionsFactory = std::move(actionsFactory)](
                       const Ui::PageContext& context
                       ) mutable
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            std::unique_ptr<Ui::Page> page =
                std::make_unique<Ui::TeacherProfilePage>(
                    context,
                    actionsFactory()
                    );
            return Ui::PageOperationResult<std::unique_ptr<Ui::Page>>{
                std::move(page)
            };
        },
        .resourcePolicy = Ui::PageResourcePolicy::PageScoped,
        .initialDataPolicy = Ui::PageInitialDataPolicy::None
    };
}

[[nodiscard]] std::vector<Ui::NavigationNode> placeholderNavigationTree()
{
    using Ui::NavigationNode;
    using Ui::NavigationWorkspaceAvailability;
    constexpr auto workspace = NavigationWorkspaceAvailability::WorkspaceOpen;

    return {
        NavigationNode::page(pageId(QStringLiteral("my_workspace")), workspace),
        NavigationNode::page(pageId(QStringLiteral("classes")), workspace),
        NavigationNode::page(pageId(QStringLiteral("sub_prep")), workspace),
        NavigationNode::group(
            QStringLiteral("co_teachers"),
            navigationLabel("Co-Teachers"),
            {},
            workspace,
            true
            ),
        NavigationNode::group(
            QStringLiteral("campus_staff"),
            navigationLabel("Campus Staff"),
            {
                NavigationNode::group(
                    QStringLiteral("teachers_all_korean"),
                    navigationLabel("Korean Teachers"),
                    {
                        NavigationNode::page(
                            pageId(QStringLiteral("teacher_profile")),
                            workspace
                            )
                    },
                    workspace,
                    true
                    ),
                NavigationNode::page(
                    pageId(QStringLiteral("native_english_teachers")),
                    workspace
                    ),
                NavigationNode::page(pageId(QStringLiteral("gs_team")), workspace)
            },
            workspace
            ),
        NavigationNode::group(
            QStringLiteral("useful_links"),
            navigationLabel("Useful Links"),
            {
                NavigationNode::placeholder(
                    QStringLiteral("useful_dropbox"), navigationLabel("Dropbox")
                    ),
                NavigationNode::placeholder(
                    QStringLiteral("useful_vacation_calendar"),
                    navigationLabel("Vacation Calendar")
                    ),
                NavigationNode::placeholder(
                    QStringLiteral("useful_yearly_calendar"),
                    navigationLabel("Yearly Calendar")
                    ),
                NavigationNode::placeholder(
                    QStringLiteral("useful_training_website"),
                    navigationLabel("Training Website")
                    ),
                NavigationNode::placeholder(
                    QStringLiteral("useful_net_website"), navigationLabel("NET Website")
                    ),
                NavigationNode::placeholder(
                    QStringLiteral("useful_lms_website"), navigationLabel("LMS Website")
                    ),
                NavigationNode::placeholder(
                    QStringLiteral("useful_highlights_library"),
                    navigationLabel("Highlights Library")
                    )
            },
            NavigationWorkspaceAvailability::Always,
            false
            ),
        NavigationNode::group(
            QStringLiteral("campus_info"),
            navigationLabel("Campus Directory"),
            {
                NavigationNode::page(pageId(QStringLiteral("campus_information"))),
                NavigationNode::page(pageId(QStringLiteral("campus_directions"))),
                NavigationNode::page(pageId(QStringLiteral("campus_address"))),
                NavigationNode::page(pageId(QStringLiteral("campus_housing"))),
                NavigationNode::page(pageId(QStringLiteral("campus_map")))
            }
            ),
        NavigationNode::page(pageId(QStringLiteral("initial_setup")))
    };
}

}  // namespace

struct ApplicationBootstrap::CoreServices final
{
    CoreServices(
        Resources::ResourceCatalog catalog,
        Resources::ResourceScope startupScope
        )
        : resourceLoader(std::move(catalog))
        , startupResourceScope(std::move(startupScope))
        , preferences(preferencesPort, preferencesPort)
    {
    }

    Resources::ResourceLoader resourceLoader;
    Resources::ResourceScope startupResourceScope;
    // Declared before the borrowing model and presenters; destroyed after them.
    Platform::SettingsManagerPreferencesPort preferencesPort;
    Application::PreferencesModel preferences;
    QString loadedLocale;
    std::optional<Resources::ScopedResource<Resources::ResourceTranslation>> translation;
    std::optional<Resources::ScopedResource<Resources::RegisteredFont>> interFont;
    std::optional<Resources::ScopedResource<Resources::RegisteredFont>> pretendardFont;
    std::unique_ptr<Ui::ThemePresenter> themePresenter;
    std::unique_ptr<Ui::FontSizePresenter> fontSizePresenter;
};

int ApplicationBootstrap::run(int argc, char* argv[])
{
    // Keep launch probes and support scripts independent of Qt platform plugin
    // initialization and application storage setup.
    if (argc == 2 && argv != nullptr && argv[1] != nullptr
        && std::strcmp(argv[1], "--version") == 0)
    {
        std::printf("ClassMngrNext launch version=%s\n", CLASSMNGR_NEXT_VERSION);
        return 0;
    }

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ClassMngrNext"));
    QApplication::setApplicationVersion(QStringLiteral(CLASSMNGR_NEXT_VERSION));

    ApplicationBootstrap bootstrap(application);
    const Application::StartupResult startup = bootstrap.start();
    if (!startup.succeeded())
    {
        std::fprintf(stderr, "ClassMngrNext could not start its initial experience.\n");
        return 1;
    }

    std::printf("ClassMngrNext launch version=%s\n", CLASSMNGR_NEXT_VERSION);
    const int eventLoopExitCode = application.exec();
    static_cast<void>(bootstrap.shutdown());
    return eventLoopExitCode;
}

ApplicationBootstrap::ApplicationBootstrap(
    QApplication& application,
    StartupEventObserver startupEventObserver,
    StartupDiagnosticsSinkFactory startupDiagnosticsSinkFactory,
    ResourcePackagePathsResolver resourcePackagePathsResolver,
    std::optional<QStringList> systemUiLanguages,
    StartupWorkspaceSeams startupWorkspaceSeams,
    StartupUpdateDispatcherFactory startupUpdateDispatcherFactory,
    ShutdownSeams shutdownSeams
    )
    : m_application(application),
      m_startupEventObserver(std::move(startupEventObserver)),
      m_startupDiagnosticsSinkFactory(std::move(startupDiagnosticsSinkFactory)),
      m_resourcePackagePathsResolver(std::move(resourcePackagePathsResolver)),
      m_systemUiLanguages(std::move(systemUiLanguages)),
      m_startupWorkspaceSeams(std::move(startupWorkspaceSeams)),
      m_startupUpdateDispatcherFactory(
          std::move(startupUpdateDispatcherFactory)
          ),
      m_shutdownSeams(std::move(shutdownSeams)),
      m_shutdownCoordinator([this] {
          return runShutdownSequence();
      })
{
}

ApplicationBootstrap::~ApplicationBootstrap()
{
    m_callbackLifetime.reset();
    if (m_startupUpdateDispatcher)
    {
        m_startupUpdateDispatcher->cancel();
        m_startupUpdateDispatcher.reset();
    }
    m_shell.reset();
    static_cast<void>(releaseWorkspace());
    m_coreServices.reset();
    m_startupDiagnosticsSink.reset();
    if (m_quitOnLastWindowClosedChanged)
    {
        m_application.setQuitOnLastWindowClosed(
            m_previousQuitOnLastWindowClosed
            );
    }
}

Application::StartupResult ApplicationBootstrap::start()
{
    const Application::StartupResult beginResult = m_startupCoordinator.begin();
    if (beginResult.status != Application::StartupStatus::InProgress)
    {
        return beginResult;
    }

    const Application::StartupResult processCreated =
        m_startupCoordinator.recordStage(Application::StartupStage::ProcessCreated);
    observeStartupResult(processCreated);
    if (processCreated.status == Application::StartupStatus::Failed)
    {
        return processCreated;
    }

    try
    {
        m_startupDiagnosticsSink = m_startupDiagnosticsSinkFactory
            ? m_startupDiagnosticsSinkFactory()
            : makeQtStartupDiagnosticsSink();
        if (!m_startupDiagnosticsSink)
        {
            throw std::runtime_error("Startup diagnostics sink unavailable.");
        }
    }
    catch (...)
    {
        if (processCreated.latestStageResult.has_value())
        {
            writeEarlyQtStartupDiagnostic(*processCreated.latestStageResult);
        }
        const Application::StartupResult failure = m_startupCoordinator.recordStage(
            Application::StartupStage::CoreServicesCreated,
            Application::StartupStageOutcome::Failed,
            Application::StartupFailureReason::StageFailed
            );
        if (failure.latestStageResult.has_value())
        {
            writeEarlyQtStartupDiagnostic(*failure.latestStageResult);
        }
        observeStartupResult(failure);
        return failure;
    }

    // ProcessCreated predates the diagnostics owner. Retain event order by
    // flushing it through the newly-created sink before recording its owner.
    if (processCreated.latestStageResult.has_value())
    {
        m_startupDiagnosticsSink->record(*processCreated.latestStageResult);
    }

    // Core services own metadata and the named request scope. ResourceLoader
    // stays lazy until each selected startup preference is composed below.
    bool coreServicesReady = false;
    try
    {
        coreServicesReady = createCoreServices();
    }
    catch (...)
    {
        // Report construction failures through the typed startup-stage result.
    }
    if (!coreServicesReady)
    {
        return failStartupStage(Application::StartupStage::CoreServicesCreated);
    }

    const Application::StartupResult coreServicesCreated =
        m_startupCoordinator.recordStage(
            Application::StartupStage::CoreServicesCreated
            );
    observeStartupResult(coreServicesCreated);
    if (coreServicesCreated.status == Application::StartupStatus::Failed)
    {
        rollbackStartupComposition();
        return coreServicesCreated;
    }

    bool settingsLoaded = false;
    try
    {
        settingsLoaded = loadStartupPreferences();
    }
    catch (const std::exception& exception)
    {
        qCritical().noquote() << "ClassMngrNext could not read startup settings:"
                              << exception.what();
    }
    catch (...)
    {
        qCritical() << "ClassMngrNext could not read startup settings.";
    }
    if (!settingsLoaded)
    {
        return failStartupStage(Application::StartupStage::SettingsLoaded);
    }
    const Application::StartupResult settingsLoadedResult =
        m_startupCoordinator.recordStage(Application::StartupStage::SettingsLoaded);
    observeStartupResult(settingsLoadedResult);
    if (settingsLoadedResult.status == Application::StartupStatus::Failed)
    {
        rollbackStartupComposition();
        return settingsLoadedResult;
    }

    Application::StartupStageOutcome languageOutcome =
        Application::StartupStageOutcome::Failed;
    try
    {
        languageOutcome = loadStartupLanguage();
    }
    catch (const std::exception& exception)
    {
        qCritical().noquote() << "ClassMngrNext could not load startup language:"
                              << exception.what();
    }
    catch (...)
    {
        qCritical() << "ClassMngrNext could not load startup language.";
    }
    if (languageOutcome == Application::StartupStageOutcome::Failed)
    {
        return failStartupStage(Application::StartupStage::LanguageLoaded);
    }
    const Application::StartupResult languageLoadedResult =
        m_startupCoordinator.recordStage(
            Application::StartupStage::LanguageLoaded,
            languageOutcome
            );
    observeStartupResult(languageLoadedResult);
    if (languageLoadedResult.status == Application::StartupStatus::Failed)
    {
        rollbackStartupComposition();
        return languageLoadedResult;
    }

    bool themeLoaded = false;
    try
    {
        themeLoaded = loadStartupTheme();
    }
    catch (const std::exception& exception)
    {
        qCritical().noquote() << "ClassMngrNext could not apply startup theme:"
                              << exception.what();
    }
    catch (...)
    {
        qCritical() << "ClassMngrNext could not apply startup theme.";
    }
    if (!themeLoaded)
    {
        return failStartupStage(Application::StartupStage::ThemeLoaded);
    }
    const Application::StartupResult themeLoadedResult =
        m_startupCoordinator.recordStage(Application::StartupStage::ThemeLoaded);
    observeStartupResult(themeLoadedResult);
    if (themeLoadedResult.status == Application::StartupStatus::Failed)
    {
        rollbackStartupComposition();
        return themeLoadedResult;
    }

    bool requiredFontsLoaded = false;
    try
    {
        requiredFontsLoaded = loadRequiredFonts();
    }
    catch (const std::exception& exception)
    {
        qCritical().noquote() << "ClassMngrNext could not load required UI fonts:"
                              << exception.what();
    }
    catch (...)
    {
        qCritical() << "ClassMngrNext could not load required UI fonts.";
    }
    if (!requiredFontsLoaded)
    {
        return failStartupStage(Application::StartupStage::RequiredFontsLoaded);
    }
    const Application::StartupResult requiredFontsLoadedResult =
        m_startupCoordinator.recordStage(
            Application::StartupStage::RequiredFontsLoaded
            );
    observeStartupResult(requiredFontsLoadedResult);
    if (requiredFontsLoadedResult.status == Application::StartupStatus::Failed)
    {
        rollbackStartupComposition();
        return requiredFontsLoadedResult;
    }

    Application::StartupStageOutcome workspaceOutcome =
        Application::StartupStageOutcome::Warning;
    try
    {
        workspaceOutcome = openStartupWorkspace();
    }
    catch (...)
    {
        if (m_applicationRuntime
            && m_applicationRuntime->workspaceState().snapshot().session().has_value())
        {
            static_cast<void>(m_applicationRuntime->workspaceCoordinator().closeWorkspace());
        }
        m_startupWorkspaceOutcome = startupWorkspaceException();
    }
    const Application::StartupResult workspaceOpenedResult =
        m_startupCoordinator.recordStage(
            Application::StartupStage::WorkspaceOpened,
            workspaceOutcome
            );
    observeStartupResult(workspaceOpenedResult);
    if (workspaceOpenedResult.status == Application::StartupStatus::Failed)
    {
        rollbackStartupComposition();
        return workspaceOpenedResult;
    }

    bool documentCatalogLoaded = false;
    try
    {
        documentCatalogLoaded = loadRequiredDocumentCatalog();
    }
    catch (const std::exception& exception)
    {
        qCritical().noquote() << "ClassMngrNext could not load the startup document catalog:"
                              << exception.what();
    }
    catch (...)
    {
        qCritical() << "ClassMngrNext could not load the startup document catalog.";
    }
    if (!documentCatalogLoaded)
    {
        return failStartupStage(
            Application::StartupStage::InitialPageReady,
            Application::StartupFailureReason::InitialExperienceUnavailable
            );
    }

    m_previousQuitOnLastWindowClosed = m_application.quitOnLastWindowClosed();
    m_application.setQuitOnLastWindowClosed(false);
    m_quitOnLastWindowClosedChanged = true;

    try
    {
        m_shell = std::make_unique<Ui::ApplicationShell>(
            m_coreServices->resourceLoader,
            m_coreServices->preferences
            );
        m_shell->setCloseHandler([this] {
            return requestWindowClose();
        });
        m_shell->setNewTeacherProfileHandler([this] {
            createNewTeacherProfile();
        });
        m_shell->setOpenTeacherProfileHandler([this] {
            openExistingTeacherProfile();
        });
        m_shell->setRecoverInterruptedTeacherProfileHandler([this] {
            recoverInterruptedTeacherProfile();
        });
        m_shell->setRecentWorkspaceMenuHandlers(
            [this] {
                std::unique_ptr<Application::RecentWorkspaceHistoryPort> historyPort =
                    m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory
                        ? m_startupWorkspaceSeams
                              .recentWorkspaceHistoryPortFactory()
                        : std::make_unique<
                              Platform::SettingsManagerRecentWorkspaceHistoryPort
                              >();
                return historyPort
                    ? historyPort->load()
                    : Application::RecentWorkspaceHistory{};
            },
            [this](const Application::RecentWorkspacePath& path) {
                openRecentTeacherProfile(path);
            },
            [this] {
                clearRecentWorkspaceHistory();
            }
            );
        m_shell->setSaveWorkspaceHandler([this] {
            saveWorkspaceFromMenu();
        });
        m_shell->setSaveWorkspaceAsHandler([this] {
            saveWorkspaceAsFromMenu();
        });
        m_shell->setExportWorkspaceHandler([this] {
            exportWorkspaceFromMenu();
        });
        m_shell->setCloseWorkspaceHandler([this] {
            closeWorkspaceFromMenu();
        });
        m_shell->setNavigationGuard([this](
            const Ui::NavigationGuardRequest& request
            ) {
            static_cast<void>(request);
            return resolveActivePageChangesBeforeLeaving("leaving this page")
                ? Ui::NavigationGuardDecision::Allow
                : Ui::NavigationGuardDecision::Reject;
        });
    }
    catch (const std::exception&)
    {
        return failStartupStage(
            Application::StartupStage::ShellCreated,
            Application::StartupFailureReason::InitialExperienceUnavailable
            );
    }

    const Application::StartupResult shellCreated = m_startupCoordinator.recordStage(
        Application::StartupStage::ShellCreated
        );
    observeStartupResult(shellCreated);
    if (shellCreated.status == Application::StartupStatus::Failed)
    {
        rollbackStartupComposition();
        return shellCreated;
    }

    bool initialPageActivated = false;
    QString initialPageFailure;
    try
    {
        for (Ui::PageDescriptor& descriptor : placeholderPageDescriptors())
        {
            const Ui::PageHostResult<void> registration =
                m_shell->pageHost().registerPage(std::move(descriptor));
            if (!registration)
            {
                initialPageFailure = registration.error().message;
                break;
            }
        }

        if (initialPageFailure.isEmpty())
        {
            const Ui::PageHostResult<void> registration =
                m_shell->pageHost().registerPage(teacherProfilePageDescriptor([this] {
                    Ui::TeacherProfilePageActions actions;
                    actions.saveMode =
                        Platform::SettingsManagerSaveModePreferencesPort().read();
                    actions.validate = [this](
                        const Domain::TeacherProfile& profile
                        ) {
                        if (auto* const gateway =
                                dynamic_cast<Platform::WorkspaceStoreGateway*>(
                                    m_workspaceGateway.get()))
                        {
                            return gateway->validateTeacherProfile(profile);
                        }
                        Platform::TeacherValidatorTeacherProfileValidationPolicy
                            validation;
                        return validation.validate(profile);
                    };
                    actions.edit = [this](
                        const Application::TeacherProfileEditRequest& request
                        ) {
                        if (auto* const gateway =
                                dynamic_cast<Platform::WorkspaceStoreGateway*>(
                                    m_workspaceGateway.get()))
                        {
                            return gateway->editTeacherProfile(request);
                        }
                        return Application::TeacherProfileEditResult{
                            std::unexpected(Application::TeacherProfileEditFailure{
                                .kind = Application::TeacherProfileEditFailureKind::UpdateFailed,
                                .cause = {
                                    .code = Domain::ErrorCode::NotFound,
                                    .message = "The active workspace store is unavailable.",
                                    .recoverable = true
                                }
                            })
                        };
                    };
                    return actions;
                }));
            if (!registration)
            {
                initialPageFailure = registration.error().message;
            }
        }

        if (initialPageFailure.isEmpty())
        {
            const Ui::PageHostResult<void> registration =
                m_shell->pageHost().registerPage(initialSetupPageDescriptor([this] {
                    return Ui::InitialSetupPageActions{
                        .state = [this] { return initialSetupPageState(); },
                        .createProfile = [this] { beginInitialSetupProfile(); },
                        .resumeSetup = [this] { resumeInitialSetupProfile(); },
                        .restoreSetup = [this] { restoreInitialSetupProfile(); },
                        .cancelSetup = [this] { cancelInitialSetupProfile(); },
                        .loadPersonalDetails = [this] {
                            return loadInitialSetupPersonalDetails();
                        },
                        .savePersonalDetails = [this](
                            const Application::PersonalDetailsSaveRequest& request
                            ) {
                            return saveInitialSetupPersonalDetails(request);
                        },
                        .loadTeacherChoices = [this] {
                            return loadInitialSetupTeacherChoices();
                        },
                        .createTeacher = [this](
                            const Application::TeacherCreateRequest& request
                            ) {
                            return saveInitialSetupTeacher(request);
                        },
                        .readClassColorPalette = [this] {
                            return readInitialSetupClassColorPalette();
                        },
                        .saveClassColorPalette = [this](
                            const Application::CustomColorPalette& palette
                            ) {
                            return saveInitialSetupClassColorPalette(palette);
                        },
                        .captureClassDetailsDraft = [this](
                            const Application::InitialSetupClassDetailsDraft& draft
                            ) {
                            return captureInitialSetupClassDetailsDraft(draft);
                        },
                        .saveClassTimes = [this](
                            const Application::InitialSetupClassTimes& times
                            ) {
                            return saveInitialSetupClassTimes(times);
                        },
                        .scheduleImport = {
                            .chooseWorkbook = [this] {
                                return chooseInitialSetupScheduleImportWorkbook();
                            },
                            .readState = [this] {
                                auto* const storeGateway =
                                    dynamic_cast<Platform::WorkspaceStoreGateway*>(
                                        m_workspaceGateway.get()
                                        );
                                return storeGateway
                                    ? storeGateway->loadScheduleImportState()
                                    : Application::ScheduleImportStateSnapshotOutcome{
                                          Application::ScheduleImportStateSnapshotFailure{
                                              Application::ScheduleImportStateSnapshotFailureKind::ActiveSessionUnavailable,
                                              Application::ScheduleImportStateSnapshotFailureSource::Session,
                                              "The setup profile schedule store is unavailable."
                                          }
                                      };
                            },
                            .apply = [this](
                                const Application::ScheduleImportApplyRequest& request
                                ) {
                                auto* const storeGateway =
                                    dynamic_cast<Platform::WorkspaceStoreGateway*>(
                                        m_workspaceGateway.get()
                                        );
                                if (!storeGateway)
                                {
                                    return Application::ScheduleImportApplyResult{
                                        std::unexpected(
                                            Application::ScheduleImportApplyFailure{
                                                u"The setup profile schedule store is unavailable.",
                                                std::nullopt,
                                                std::nullopt,
                                                std::nullopt,
                                                Domain::ErrorCode::Technical
                                            }
                                            )
                                    };
                                }
                                return storeGateway->applyScheduleImport(request);
                            },
                            .validationTimeoutMilliseconds = [] {
                                const Platform::SettingsManagerExcelImportTimeoutPort
                                    timeoutPreferences;
                                return timeoutPreferences.read().normalizedSeconds()
                                    * 1000;
                            },
                            .cancelSetup = [this] { cancelInitialSetupProfile(); }
                        },
                        .finishSetup = [this] { finishInitialSetupProfile(); },
                        .teacherImport = {
                            .chooseWorkbook = [this] {
                                return chooseInitialSetupTeacherImportWorkbook();
                            },
                            .validateWorkbook = [](const QString& path) {
                                return validateTeacherImportFile(path);
                            },
                            .validationTimeoutMilliseconds = [] {
                                const Platform::SettingsManagerExcelImportTimeoutPort
                                    timeoutPreferences;
                                return timeoutPreferences.read().normalizedSeconds()
                                    * 1000;
                            },
                            .apply = [this](
                                const Application::TeacherImportUseCaseRequest& request
                                ) {
                                return applyInitialSetupTeacherImport(request);
                            },
                            .cancelSetup = [this] { cancelInitialSetupProfile(); }
                        },
                        .chooseSignatureImage = [this] {
                            return chooseInitialSetupSignatureImage();
                        }
                    };
                }));
            if (!registration)
            {
                initialPageFailure = registration.error().message;
            }
        }

        if (initialPageFailure.isEmpty())
        {
            const auto navigationConfiguration = m_shell->configureNavigation(
                placeholderNavigationTree(),
                pageId(QStringLiteral("initial_setup")),
                pageId(QStringLiteral("my_workspace")),
                m_applicationRuntime->workspaceState().snapshot().lifecycle()
                );
            if (!navigationConfiguration)
            {
                initialPageFailure = navigationConfiguration.error().message;
            }
            else
            {
                const std::optional<Ui::PageId> initialPage =
                    m_shell->navigationModel()->initialPageId();
                if (!initialPage
                    || !m_shell->navigateTo(*initialPage, Ui::NavigationCause::Initial))
                {
                    initialPageFailure = QStringLiteral("initial navigation failed");
                }
                else
                {
                    initialPageActivated = true;
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        initialPageFailure = QString::fromUtf8(exception.what());
    }
    catch (...)
    {
        initialPageFailure = QStringLiteral("initial page activation failed");
    }
    if (!initialPageActivated)
    {
        qCritical().noquote()
            << "ClassMngrNext could not activate the initial page:"
            << initialPageFailure;
        return failStartupStage(
            Application::StartupStage::InitialPageReady,
            Application::StartupFailureReason::InitialExperienceUnavailable
            );
    }

    m_shell->show();

    const Application::StartupResult initialPageReady =
        m_startupCoordinator.recordStage(Application::StartupStage::InitialPageReady);
    observeStartupResult(initialPageReady);
    if (initialPageReady.status == Application::StartupStatus::Failed)
    {
        return failStartupStage(
            Application::StartupStage::InitialPageReady,
            Application::StartupFailureReason::InitialExperienceUnavailable
            );
    }
    const Application::StartupResult startupReady = m_startupCoordinator.recordStage(
        Application::StartupStage::StartupReady
        );
    observeStartupResult(startupReady);
    if (startupReady.status == Application::StartupStatus::Failed)
    {
        rollbackStartupComposition();
        return startupReady;
    }

    if (startupReady.status == Application::StartupStatus::StartupReady)
    {
        scheduleStartupUpdateCheck();
    }

    return startupReady;
}

bool ApplicationBootstrap::loadStartupPreferences()
{
    if (!m_coreServices)
    {
        return false;
    }

    const auto loaded = m_coreServices->preferences.load();
    if (!loaded)
    {
        qCritical().noquote() << "ClassMngrNext could not load preferences:"
                              << QString::fromStdString(loaded.error().message);
        return false;
    }
    return true;
}

Application::StartupStageOutcome ApplicationBootstrap::loadStartupLanguage()
{
    if (!m_coreServices)
    {
        return Application::StartupStageOutcome::Failed;
    }

    const QStringList systemLanguages = m_systemUiLanguages.has_value()
        ? *m_systemUiLanguages
        : QLocale::system().uiLanguages();
    QString locale;
    switch (m_coreServices->preferences.snapshot().languagePreference())
    {
    case Application::LanguagePreference::English:
        locale = englishLocaleFor(systemLanguages);
        break;
    case Application::LanguagePreference::Korean:
        locale = QStringLiteral("ko_KR");
        break;
    case Application::LanguagePreference::SystemDefault:
        locale = supportedSystemLocale(systemLanguages);
        break;
    }

    if (locale.isEmpty())
    {
        qWarning().noquote()
            << "ClassMngrNext found no supported system UI locale; using English source strings."
            << "uiLanguages=" << systemLanguages.join(QLatin1Char(','));
        return Application::StartupStageOutcome::Warning;
    }

    const auto request = startupRequest(
        m_coreServices->startupResourceScope,
        QStringLiteral("resource:assets/translations/ClassMngr_%1.ts").arg(locale)
        );
    if (!request)
    {
        qCritical().noquote() << "ClassMngrNext could not identify the selected translation:"
                              << request.error().message;
        return Application::StartupStageOutcome::Failed;
    }

    auto translation = m_coreServices->resourceLoader.loadTranslation(
        *request,
        m_application
        );
    if (!translation)
    {
        qCritical().noquote() << "ClassMngrNext could not load selected translation"
                              << locale << ':' << translation.error().message;
        return Application::StartupStageOutcome::Failed;
    }

    m_coreServices->translation.emplace(std::move(*translation));
    m_coreServices->loadedLocale = locale;
    return Application::StartupStageOutcome::Completed;
}

bool ApplicationBootstrap::loadStartupTheme()
{
    if (!m_coreServices || !m_coreServices->themePresenter)
    {
        return false;
    }

    const Domain::Result<void> applied =
        m_coreServices->themePresenter->applyStartupPreference();
    if (!applied)
    {
        qCritical().noquote() << "ClassMngrNext could not apply selected theme:"
                              << QString::fromStdString(applied.error().message);
        return false;
    }
    return true;
}

bool ApplicationBootstrap::loadRequiredFonts()
{
    if (!m_coreServices)
    {
        return false;
    }

    const auto interRequest = startupRequest(
        m_coreServices->startupResourceScope,
        QStringLiteral("resource:assets/fonts/Inter.ttc")
        );
    if (!interRequest)
    {
        qCritical().noquote() << "ClassMngrNext could not identify Inter font resource:"
                              << interRequest.error().message;
        return false;
    }
    auto interFont = m_coreServices->resourceLoader.loadFont(*interRequest);
    if (!interFont)
    {
        qCritical().noquote() << "ClassMngrNext could not register Inter font:"
                              << interFont.error().message;
        return false;
    }
    m_coreServices->interFont.emplace(std::move(*interFont));

    const auto pretendardRequest = startupRequest(
        m_coreServices->startupResourceScope,
        QStringLiteral("resource:assets/fonts/PretendardVariable.ttf")
        );
    if (!pretendardRequest)
    {
        qCritical().noquote() << "ClassMngrNext could not identify Pretendard font resource:"
                              << pretendardRequest.error().message;
        return false;
    }
    auto pretendardFont = m_coreServices->resourceLoader.loadFont(*pretendardRequest);
    if (!pretendardFont)
    {
        qCritical().noquote() << "ClassMngrNext could not register Pretendard font:"
                              << pretendardFont.error().message;
        return false;
    }
    m_coreServices->pretendardFont.emplace(std::move(*pretendardFont));
    m_coreServices->fontSizePresenter->applyStartupPreference(
        m_coreServices->preferences.snapshot().fontSize(), m_coreServices->loadedLocale,
        m_coreServices->interFont->get().familyNames().value(0),
        m_coreServices->pretendardFont->get().familyNames().value(0));
    return true;
}

bool ApplicationBootstrap::loadRequiredDocumentCatalog()
{
    if (!m_coreServices)
    {
        return false;
    }

    // The runtime owns startup application data regardless of whether a
    // workspace was selected or opened successfully.
    ensureWorkspaceComposition();
    if (!m_applicationRuntime)
    {
        return false;
    }

    const Platform::ResourceDocumentCatalogPort catalogPort(
        m_coreServices->resourceLoader
        );
    Domain::Result<Application::DocumentCatalogProjection> projection =
        catalogPort.projection(m_coreServices->loadedLocale);
    if (!projection)
    {
        qCritical().noquote()
            << "ClassMngrNext could not project startup document catalog metadata:"
            << QString::fromStdString(projection.error().message);
        return false;
    }

    m_applicationRuntime->retainStartupDocumentCatalogProjection(
        std::move(projection.value())
        );
    return true;
}

Application::StartupStageOutcome ApplicationBootstrap::openStartupWorkspace()
{
    // The application runtime is available to every successful startup, even
    // when policy selects no workspace. Constructing the store is side-effect
    // free; only the selected open operation touches a workspace file.
    ensureWorkspaceComposition();

    // An interrupted setup must stay visible as a recovery choice. Do not
    // auto-open its target from command-line or recent history while the user
    // is deciding whether to resume the new profile or restore the original.
    const auto pendingSetup = m_initialSetupLifecyclePort->pendingRecovery();
    if (!pendingSetup || pendingSetup.value().has_value())
    {
        m_startupWorkspaceOutcome = Application::StartupWorkspaceNotOpened{
            Application::StartupNoWorkspaceReason::NoCandidate
        };
        return Application::StartupStageOutcome::Warning;
    }

    const std::optional<Application::WorkspaceLocation> commandLineLocation =
        commandLineStartupWorkspaceLocation();
    const bool recentAutoOpenEnabled =
        m_startupWorkspaceSeams.recentAutoOpenEnabled.value_or(true);

    std::unique_ptr<Application::RecentWorkspaceHistoryPort> historyPort;
    Application::RecentWorkspaceHistory history;
    const auto makeHistoryPort = [&]()
    {
        if (historyPort)
        {
            return;
        }
        historyPort = m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory
            ? m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory()
            : std::make_unique<Platform::SettingsManagerRecentWorkspaceHistoryPort>();
    };

    if (!commandLineLocation.has_value() && recentAutoOpenEnabled)
    {
        makeHistoryPort();
        if (historyPort)
        {
            history = historyPort->load();
        }
    }

    const Application::StartupWorkspaceSelection selection =
        Application::StartupWorkspacePolicy::select(
            commandLineLocation,
            recentAutoOpenEnabled,
            history
            );
    const auto* const noCandidate =
        std::get_if<Application::StartupNoWorkspaceCandidate>(&selection);
    if (noCandidate != nullptr)
    {
        m_startupWorkspaceOutcome = Application::StartupWorkspaceNotOpened{
            noCandidate->reason
        };
        return Application::StartupStageOutcome::Warning;
    }

    const Application::StartupWorkspaceCandidate& candidate =
        std::get<Application::StartupWorkspaceCandidate>(selection);
    if (candidate.source == Application::StartupWorkspaceSource::CommandLine)
    {
        // Legacy startup records a successfully opened command-line profile
        // in recent history, even though it does not consult that history to
        // select the profile.
        makeHistoryPort();
        if (historyPort)
        {
            history = historyPort->load();
        }
    }

    const QString selectedPath =
        normalizedStartupWorkspacePath(QString::fromUtf8(
            candidate.location.value().data(),
            static_cast<qsizetype>(candidate.location.value().size())
            ));
    const Application::WorkspaceLocation selectedLocation(
        selectedPath.toUtf8().toStdString()
        );

    if (candidate.source != Application::StartupWorkspaceSource::CommandLine)
    {
        const bool exists = m_startupWorkspaceSeams.pathExists
            ? m_startupWorkspaceSeams.pathExists(selectedLocation)
            : QFileInfo::exists(selectedPath);
        if (!exists)
        {
            std::optional<Domain::OperationError> historyError;
            if (historyPort)
            {
                const Application::RecentWorkspaceHistory pruned =
                    Application::RecentWorkspaceHistoryUseCase::prune(
                        history,
                        candidate.location,
                        selectedLocation
                        );
                const Domain::Result<void> saved = historyPort->save(pruned);
                if (!saved)
                {
                    historyError = saved.error();
                }
            }
            m_startupWorkspaceOutcome = Application::StartupWorkspaceNotOpened{
                Application::StartupNoWorkspaceReason::StaleRecentPathPruned,
                std::move(historyError)
            };
            return Application::StartupStageOutcome::Warning;
        }
    }

    const Domain::Result<Application::WorkspaceSession> opened =
        m_applicationRuntime->workspaceCoordinator().openWorkspace(
            Application::OpenWorkspaceRequest{selectedLocation}
            );
    if (!opened)
    {
        m_startupWorkspaceOutcome = Application::StartupWorkspaceOpenFailed{
            opened.error()
        };
        return Application::StartupStageOutcome::Warning;
    }

    if (m_startupWorkspaceSeams.afterStartupWorkspaceOpenedForTesting)
    {
        m_startupWorkspaceSeams.afterStartupWorkspaceOpenedForTesting(
            m_applicationRuntime->workspaceState()
            );
    }

    std::optional<Domain::OperationError> historyError;
    if (historyPort)
    {
        const Application::RecentWorkspaceHistory recorded =
            Application::RecentWorkspaceHistoryUseCase::record(
                history,
                candidate.location,
                opened.value().location()
                );
        const Domain::Result<void> saved = historyPort->save(recorded);
        if (!saved)
        {
            historyError = saved.error();
        }
    }

    m_startupWorkspaceOutcome = Application::StartupWorkspaceOpened{
        opened.value(),
        std::move(historyError)
    };
    return std::get<Application::StartupWorkspaceOpened>(
            *m_startupWorkspaceOutcome
            ).historyError.has_value()
        ? Application::StartupStageOutcome::Warning
        : Application::StartupStageOutcome::Completed;
}

std::optional<Application::WorkspaceLocation>
ApplicationBootstrap::commandLineStartupWorkspaceLocation() const
{
    const QStringList arguments = m_startupWorkspaceSeams.commandLineArguments
        ? *m_startupWorkspaceSeams.commandLineArguments
        : m_application.arguments();

    for (int index = 1; index < arguments.size(); ++index)
    {
        const QString& argument = arguments.at(index);
        if (argument == QStringLiteral("--startup-performance-workflow"))
        {
            continue;
        }

        if (argument == QStringLiteral("--startup-performance-output")
            || argument == QStringLiteral("--startup-performance-scenario")
            || argument == QStringLiteral("--startup-performance-settle-ms")
            || argument == QStringLiteral("--startup-visual-capture-output")
            || argument == QStringLiteral("--startup-visual-capture-language")
            || argument == QStringLiteral("--startup-visual-capture-theme"))
        {
            ++index;
            continue;
        }

        if (argument.startsWith(QLatin1Char('-'))
            || !isSupportedStartupWorkspacePath(argument))
        {
            continue;
        }

        const QByteArray encoded = argument.toUtf8();
        return Application::WorkspaceLocation(std::string(
            encoded.constData(),
            static_cast<std::size_t>(encoded.size())
            ));
    }
    return std::nullopt;
}

void ApplicationBootstrap::ensureWorkspaceComposition()
{
    if (m_applicationRuntime)
    {
        return;
    }

    if (m_startupWorkspaceSeams.workspaceGatewayFactory)
    {
        m_workspaceGateway =
            m_startupWorkspaceSeams.workspaceGatewayFactory();
    }
    else
    {
        m_workspaceGateway =
            std::make_unique<Platform::WorkspaceStoreGateway>();
    }
    if (!m_workspaceGateway)
    {
        throw std::runtime_error("Startup workspace gateway unavailable.");
    }

    m_applicationRuntime = std::make_unique<Application::ApplicationRuntime>(
        *m_workspaceGateway
        );
    m_initialSetupLifecyclePort =
        std::make_unique<Platform::WorkspaceStoreInitialSetupLifecycleAdapter>(
            m_applicationRuntime->workspaceCoordinator(),
            m_applicationRuntime->workspaceState(),
            m_startupWorkspaceSeams.initialSetupRecoveryJournalPath.value_or(
                QString{}
                )
            );
    m_initialSetupLifecycle =
        std::make_unique<Application::InitialSetupLifecycle>(
            *m_initialSetupLifecyclePort
            );
}

Ui::InitialSetupPageState ApplicationBootstrap::initialSetupPageState() const
{
    Ui::InitialSetupPageState state;
    state.statusMessage = m_initialSetupStatusMessage;
    if (m_initialSetupLifecycle
        && m_initialSetupLifecycle->hasPendingSetup())
    {
        state.setupPending = true;
        state.profileLocation = QString::fromUtf8(
            m_initialSetupLifecycle->pendingProfile().value().data(),
            static_cast<qsizetype>(
                m_initialSetupLifecycle->pendingProfile().value().size()
                )
            );
        state.recoveryHasOriginalProfile =
            !m_initialSetupLifecycle->pendingOriginalBackup().empty();
        state.classDetailsReady = m_initialSetupClassDetailsDraft.has_value();
        state.classTimesSaved = m_initialSetupClassTimesSaved;
        return state;
    }

    if (!m_initialSetupLifecyclePort)
    {
        state.recoveryError = true;
        return state;
    }

    const auto recovery = m_initialSetupLifecyclePort->pendingRecovery();
    if (!recovery)
    {
        state.recoveryError = true;
        state.statusMessage = QCoreApplication::translate(
            "InitialPage",
            "The setup recovery record could not be read. Keep the recovery files in place and restart after resolving the issue."
            );
        return state;
    }
    if (!recovery.value().has_value())
    {
        return state;
    }

    state.recoveryAvailable = true;
    state.recoveryProfileLocation = QString::fromUtf8(
        recovery.value()->profile.value().data(),
        static_cast<qsizetype>(recovery.value()->profile.value().size())
        );
    state.recoveryHasOriginalProfile =
        !recovery.value()->originalBackup.empty();
    return state;
}

void ApplicationBootstrap::beginInitialSetupProfile()
{
    if (!m_initialSetupLifecycle || !m_shell)
    {
        return;
    }

    const Application::SaveFileDialogRequest request{
        .purpose = Application::FileDialogPurpose::General,
        .title = QCoreApplication::translate(
            "InitialPage",
            "Choose a profile destination"
            ).toUtf8().toStdString(),
        .suggestedFileName = "ClassMngr.tps",
        .nameFilters = {
            QCoreApplication::translate(
                "InitialPage",
                "ClassMngr profiles (*.tps)"
                ).toUtf8().toStdString()
        },
        .defaultSuffix = "tps",
        .confirmOverwrite = true
    };
    using SaveDialogResult = Domain::Result<
        Application::FileDialogResponse<Application::SavedFileDialogSelection>>;
    const SaveDialogResult selected = [&]() -> SaveDialogResult {
        if (m_startupWorkspaceSeams.initialSetupFileDialogPortFactory)
        {
            std::unique_ptr<Application::FileDialogPort> fileDialog =
                m_startupWorkspaceSeams.initialSetupFileDialogPortFactory();
            if (!fileDialog)
            {
                return SaveDialogResult::failure(Domain::OperationError{
                    .code = Domain::ErrorCode::Technical,
                    .message = "Initial setup file dialog port is unavailable.",
                    .recoverable = false
                });
            }
            return fileDialog->saveFile(request);
        }

        Platform::QSettingsFileDialogDirectoryPreferencesAdapter directoryPreferences;
        Ui::QtFileDialogPresenter fileDialog(directoryPreferences, m_shell.get());
        return fileDialog.saveFile(request);
    }();
    if (!selected)
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "The profile save dialog could not be opened: %1"
            ).arg(QString::fromUtf8(
                selected.error().message.data(),
                static_cast<qsizetype>(selected.error().message.size())
                ));
        return;
    }
    if (!selected.value().selection.has_value())
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "Profile creation was canceled."
            );
        return;
    }

    const auto location = Platform::
        WorkspaceStoreInitialSetupLifecycleAdapter::profileLocationForSetupPath(
            selected.value().selection->path
            );
    if (!location)
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "The profile destination is invalid: %1"
            ).arg(QString::fromUtf8(
                location.error().message.data(),
                static_cast<qsizetype>(location.error().message.size())
                ));
        return;
    }

    const auto started = m_initialSetupLifecycle->begin(location.value());
    if (!started.started())
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "Profile setup could not begin: %1"
            ).arg(QString::fromUtf8(
                started.error.message.data(),
                static_cast<qsizetype>(started.error.message.size())
                ));
        return;
    }
    m_initialSetupClassDetailsDraft.reset();
    m_initialSetupClassId.reset();
    m_initialSetupClassTimesSaved = false;
    m_initialSetupStatusMessage.clear();
}

std::unique_ptr<Application::UserPromptPort>
ApplicationBootstrap::createWorkspaceLifecycleUserPromptPort()
{
    if (m_startupWorkspaceSeams.workspaceLifecycleUserPromptPortFactory)
    {
        return m_startupWorkspaceSeams.workspaceLifecycleUserPromptPortFactory();
    }
    if (!m_shell)
    {
        return {};
    }
    return std::make_unique<Ui::QtUserPromptPresenter>(m_shell.get());
}

bool ApplicationBootstrap::resolveActivePageChangesBeforeLeaving(
    const std::string& destination
    )
{
    if (!m_shell || !m_applicationRuntime)
    {
        return true;
    }

    Ui::PageHostActivePageChangesAdapter activePageChanges(m_shell->pageHost());
    Application::WorkspaceCoordinator& workspaceCoordinator =
        m_applicationRuntime->workspaceCoordinator();
    if (!activePageChanges.hasUnsavedChanges()
        && !workspaceCoordinator.hasUnsavedChanges())
    {
        return true;
    }

    std::unique_ptr<Application::UserPromptPort> userPrompt =
        createWorkspaceLifecycleUserPromptPort();
    if (!userPrompt)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The unsaved-changes prompt is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return false;
    }

    Application::UnsavedChangesResolutionFlow flow(
        activePageChanges,
        workspaceCoordinator,
        *userPrompt
        );
    const Application::UnsavedChangesResolution resolution = flow.resolve(
        Application::UnsavedChangesPromptRequest{
            .title = "Unsaved Changes",
            .message = "The current page or profile has unsaved changes.",
            .question = "Save changes before " + destination + "?",
            .saveText = "Save",
            .discardText = "Discard",
            .cancelText = "Cancel"
        }
        );
    switch (resolution.status)
    {
    case Application::UnsavedChangesResolutionStatus::Ready:
        return true;
    case Application::UnsavedChangesResolutionStatus::Canceled:
        return false;
    case Application::UnsavedChangesResolutionStatus::Failed:
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = resolution.message,
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return false;
    }
    return false;
}

void ApplicationBootstrap::createNewTeacherProfile()
{
    if (!m_shell || !m_applicationRuntime || !m_initialSetupLifecycle)
    {
        return;
    }
    if (!resolveActivePageChangesBeforeLeaving(
            "creating a new Teacher Profile"
            ))
    {
        return;
    }

    Platform::QSettingsFileDialogDirectoryPreferencesAdapter directoryPreferences;
    std::unique_ptr<Application::FileDialogPort> ownedFileDialog;
    if (m_startupWorkspaceSeams.newProfileFileDialogPortFactory)
    {
        ownedFileDialog =
            m_startupWorkspaceSeams.newProfileFileDialogPortFactory();
    }
    else
    {
        ownedFileDialog = std::make_unique<Ui::QtFileDialogPresenter>(
            directoryPreferences,
            m_shell.get()
            );
    }
    if (!ownedFileDialog)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The profile save dialog is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    std::unique_ptr<Application::UserPromptPort> ownedUserPrompt;
    if (m_startupWorkspaceSeams.newProfileUserPromptPortFactory)
    {
        ownedUserPrompt =
            m_startupWorkspaceSeams.newProfileUserPromptPortFactory();
    }
    else
    {
        ownedUserPrompt = std::make_unique<Ui::QtUserPromptPresenter>(
            m_shell.get()
            );
    }
    if (!ownedUserPrompt)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The current page confirmation is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    std::unique_ptr<Application::RecentWorkspaceHistoryPort> ownedRecentHistory;
    if (m_startupWorkspaceSeams.newProfileRecentWorkspaceHistoryPortFactory)
    {
        ownedRecentHistory =
            m_startupWorkspaceSeams.newProfileRecentWorkspaceHistoryPortFactory();
    }
    else
    {
        ownedRecentHistory =
            std::make_unique<Platform::SettingsManagerRecentWorkspaceHistoryPort>();
    }
    if (!ownedRecentHistory)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Recent profile history is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Platform::QtWorkspaceCreationFileOperationsPort fileOperations;
    Platform::WorkspaceStoreCreationLifecycleAdapter lifecyclePort(
        m_applicationRuntime->workspaceCoordinator(),
        m_applicationRuntime->workspaceState(),
        fileOperations
        );
    Application::WorkspaceCreationLifecycle lifecycle(lifecyclePort);
    Application::NormalWorkspaceCreationFlow flow(
        *ownedFileDialog,
        *ownedUserPrompt,
        pathPolicy,
        lifecyclePort,
        lifecycle,
        m_applicationRuntime->workspaceCoordinator(),
        m_applicationRuntime->workspaceState(),
        *ownedRecentHistory,
        m_shell->notificationPort(),
        [this] {
            if (m_initialSetupLifecycle->hasPendingSetup())
            {
                return true;
            }
            const Ui::InitialSetupPageState setupState = initialSetupPageState();
            return setupState.setupPending
                || setupState.recoveryAvailable
                || setupState.recoveryError;
        }
        );
    const Application::NormalWorkspaceCreationOutcome outcome = flow.run();
    if (outcome.status == Application::NormalWorkspaceCreationStatus::Created)
    {
        m_shell->updateNavigationAccessContext(
            m_applicationRuntime->workspaceState().snapshot().lifecycle(),
            std::nullopt
            );
    }
}

void ApplicationBootstrap::openExistingTeacherProfile()
{
    openTeacherProfile(std::nullopt);
}

void ApplicationBootstrap::openRecentTeacherProfile(
    const Application::RecentWorkspacePath& path
    )
{
    openTeacherProfile(Application::FileDialogPath(path.value()));
}

void ApplicationBootstrap::openTeacherProfile(
    std::optional<Application::FileDialogPath> selectedPath
    )
{
    if (!m_shell || !m_applicationRuntime || !m_initialSetupLifecycle)
    {
        return;
    }

    const auto setupPending = [this] {
        if (m_initialSetupLifecycle->hasPendingSetup())
        {
            return true;
        }
        const Ui::InitialSetupPageState setupState = initialSetupPageState();
        return setupState.setupPending
            || setupState.recoveryAvailable
            || setupState.recoveryError;
    };

    // Setup and recovery state must be resolved before prompting to save or
    // discard workspace edits. In particular, an unreadable recovery journal
    // blocks replacement so its recovery data and history remain available.
    if (setupPending())
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "A profile setup or recovery is in progress. Finish or resolve it before opening another profile.",
                .severity = Application::NotificationSeverity::Warning
            }
            ));
        return;
    }

    if (!resolveActivePageChangesBeforeLeaving(
            "opening another Teacher Profile"
            ))
    {
        return;
    }

    Platform::QSettingsFileDialogDirectoryPreferencesAdapter directoryPreferences;
    std::unique_ptr<Application::FileDialogPort> ownedFileDialog;
    if (!selectedPath && m_startupWorkspaceSeams.openProfileFileDialogPortFactory)
    {
        ownedFileDialog =
            m_startupWorkspaceSeams.openProfileFileDialogPortFactory();
    }
    else if (!selectedPath)
    {
        ownedFileDialog = std::make_unique<Ui::QtFileDialogPresenter>(
            directoryPreferences,
            m_shell.get()
            );
    }
    if (!selectedPath && !ownedFileDialog)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The profile open dialog is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    std::unique_ptr<Application::RecentWorkspaceHistoryPort> ownedRecentHistory;
    if (m_startupWorkspaceSeams.openProfileRecentWorkspaceHistoryPortFactory)
    {
        ownedRecentHistory =
            m_startupWorkspaceSeams.openProfileRecentWorkspaceHistoryPortFactory();
    }
    else if (m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory)
    {
        ownedRecentHistory =
            m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory();
    }
    else
    {
        ownedRecentHistory =
            std::make_unique<Platform::SettingsManagerRecentWorkspaceHistoryPort>();
    }
    if (!ownedRecentHistory)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Recent profile history is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::OpenExistingWorkspaceFlow flow = selectedPath.has_value()
        ? Application::OpenExistingWorkspaceFlow(
              pathPolicy,
              m_applicationRuntime->workspaceCoordinator(),
              *ownedRecentHistory,
              m_shell->notificationPort(),
              setupPending
              )
        : Application::OpenExistingWorkspaceFlow(
              *ownedFileDialog,
              pathPolicy,
              m_applicationRuntime->workspaceCoordinator(),
              *ownedRecentHistory,
              m_shell->notificationPort(),
              setupPending
              );
    const Application::OpenExistingWorkspaceOutcome outcome = selectedPath
        ? flow.openPath(*selectedPath)
        : flow.run();
    if (outcome.status != Application::OpenExistingWorkspaceStatus::Opened)
    {
        return;
    }

    if (selectedPath.has_value() && outcome.session.has_value())
    {
        const std::string& openedPath = outcome.session->location().value();
        const QString openedPathText = QString::fromUtf8(
            openedPath.data(),
            static_cast<qsizetype>(openedPath.size())
            );
        const QString openedDirectory = QFileInfo(openedPathText).absolutePath();
        const Domain::Result<void> directoryPreferenceSaved =
            directoryPreferences.writeDirectory(
                Application::FileDialogPurpose::TeacherProfile,
                Application::DirectoryPreferencePath(
                    openedDirectory.toUtf8().toStdString()
                    )
                );
        if (!directoryPreferenceSaved)
        {
            static_cast<void>(m_shell->notificationPort().show(
                Application::NotificationRequest{
                    .message = "The profile was opened, but its directory could not be remembered: "
                        + directoryPreferenceSaved.error().message,
                    .severity = Application::NotificationSeverity::Warning
                }
                ));
        }
    }

    m_shell->updateNavigationAccessContext(
        m_applicationRuntime->workspaceState().snapshot().lifecycle(),
        std::nullopt
        );
    if (!m_shell->navigateTo(
            pageId(QStringLiteral("my_workspace")),
            Ui::NavigationCause::Selection
            ))
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The profile opened, but the workspace page could not be selected.",
                .severity = Application::NotificationSeverity::Warning
            }
            ));
    }
}

void ApplicationBootstrap::recoverInterruptedTeacherProfile()
{
    if (!m_shell || !m_applicationRuntime || !m_initialSetupLifecycle)
    {
        return;
    }

    const auto setupPending = [this] {
        if (m_initialSetupLifecycle->hasPendingSetup())
        {
            return true;
        }
        const Ui::InitialSetupPageState setupState = initialSetupPageState();
        return setupState.setupPending
            || setupState.recoveryAvailable
            || setupState.recoveryError;
    };

    // Resolve Initial Setup's durable state before constructing dialog and
    // history adapters. The application flow checks again before scanning to
    // cover a state change while the command is being prepared.
    if (setupPending())
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "A profile setup or recovery is in progress. Finish or resolve it before recovering another profile.",
                .severity = Application::NotificationSeverity::Warning
            }
            ));
        return;
    }

    Platform::QSettingsFileDialogDirectoryPreferencesAdapter directoryPreferences;
    std::unique_ptr<Application::FileDialogPort> ownedFileDialog;
    if (m_startupWorkspaceSeams
            .interruptedProfileRecoveryFileDialogPortFactory)
    {
        ownedFileDialog = m_startupWorkspaceSeams
                              .interruptedProfileRecoveryFileDialogPortFactory();
    }
    else
    {
        ownedFileDialog = std::make_unique<Ui::QtFileDialogPresenter>(
            directoryPreferences,
            m_shell.get()
            );
    }
    if (!ownedFileDialog)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The recovered profile save dialog is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    std::unique_ptr<Application::UserPromptPort> ownedUserPrompt =
        createWorkspaceLifecycleUserPromptPort();
    if (!ownedUserPrompt)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The interrupted profile recovery prompt is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    std::unique_ptr<Application::RecentWorkspaceHistoryPort> ownedRecentHistory;
    if (m_startupWorkspaceSeams
            .interruptedProfileRecoveryRecentWorkspaceHistoryPortFactory)
    {
        ownedRecentHistory = m_startupWorkspaceSeams
                                 .interruptedProfileRecoveryRecentWorkspaceHistoryPortFactory();
    }
    else if (m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory)
    {
        ownedRecentHistory =
            m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory();
    }
    else
    {
        ownedRecentHistory =
            std::make_unique<Platform::SettingsManagerRecentWorkspaceHistoryPort>();
    }
    if (!ownedRecentHistory)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Recent profile history is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    std::unique_ptr<Application::InterruptedWorkspaceRecoveryPort>
        ownedRecoveryPort;
    if (m_startupWorkspaceSeams.interruptedWorkspaceRecoveryPortFactory)
    {
        ownedRecoveryPort =
            m_startupWorkspaceSeams.interruptedWorkspaceRecoveryPortFactory();
    }
    else
    {
        ownedRecoveryPort = std::make_unique<
            Platform::QtWorkspaceInterruptedRecoveryPort>();
    }
    if (!ownedRecoveryPort)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Interrupted profile recovery is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::OpenExistingWorkspaceFlow openFlow(
        pathPolicy,
        m_applicationRuntime->workspaceCoordinator(),
        *ownedRecentHistory,
        m_shell->notificationPort(),
        setupPending
        );
    Ui::PageHostActivePageChangesAdapter activePageChanges(m_shell->pageHost());
    Application::InterruptedWorkspaceRecoveryFlow flow(
        *ownedFileDialog,
        *ownedUserPrompt,
        directoryPreferences,
        *ownedRecoveryPort,
        pathPolicy,
        activePageChanges,
        m_applicationRuntime->workspaceCoordinator(),
        m_applicationRuntime->workspaceState(),
        openFlow,
        m_shell->notificationPort(),
        setupPending
        );
    const Application::InterruptedWorkspaceRecoveryOutcome outcome = flow.run();
    if (outcome.status != Application::InterruptedWorkspaceRecoveryStatus::Recovered
        || !outcome.session.has_value())
    {
        return;
    }

    const QString destination = QString::fromUtf8(
        outcome.session->location().value().data(),
        static_cast<qsizetype>(outcome.session->location().value().size())
        );
    const QString destinationDirectory =
        QFileInfo(destination).absolutePath();
    const Domain::Result<void> directoryPreferenceSaved =
        directoryPreferences.writeDirectory(
            Application::FileDialogPurpose::TeacherProfile,
            Application::DirectoryPreferencePath(
                destinationDirectory.toUtf8().toStdString()
                )
            );
    if (!directoryPreferenceSaved)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The recovered profile opened, but its directory could not be remembered: "
                    + directoryPreferenceSaved.error().message,
                .severity = Application::NotificationSeverity::Warning
            }
            ));
    }

    m_shell->updateNavigationAccessContext(
        m_applicationRuntime->workspaceState().snapshot().lifecycle(),
        std::nullopt
        );
    if (!m_shell->navigateTo(
            pageId(QStringLiteral("my_workspace")),
            Ui::NavigationCause::Selection
            ))
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The recovered profile opened, but the workspace page could not be selected.",
                .severity = Application::NotificationSeverity::Warning
            }
            ));
    }
}

void ApplicationBootstrap::clearRecentWorkspaceHistory()
{
    if (!m_shell)
    {
        return;
    }

    std::unique_ptr<Application::RecentWorkspaceHistoryPort> historyPort =
        m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory
            ? m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory()
            : std::make_unique<Platform::SettingsManagerRecentWorkspaceHistoryPort>();
    if (!historyPort)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Recent profile history could not be cleared because history storage is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    const Domain::Result<void> cleared = historyPort->clear();
    if (!cleared)
    {
        const QString errorMessage = QString::fromUtf8(
            cleared.error().message.data(),
            static_cast<qsizetype>(cleared.error().message.size())
            );
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Recent profile history could not be cleared: "
                    + errorMessage.toStdString(),
                .severity = Application::NotificationSeverity::Warning
            }
            ));
    }
}

void ApplicationBootstrap::saveWorkspaceFromMenu()
{
    if (!m_shell || !m_applicationRuntime)
    {
        return;
    }
    if (!m_applicationRuntime->workspaceState().snapshot().session().has_value())
    {
        return;
    }

    if (!m_applicationRuntime->workspaceCoordinator().hasUnsavedChanges())
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Teacher Profile has no unsaved workspace changes.",
                .severity = Application::NotificationSeverity::Information
            }
            ));
        return;
    }

    const Domain::Result<void> saved =
        m_applicationRuntime->workspaceCoordinator().saveWorkspace();
    if (!saved)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The Teacher Profile could not be saved: "
                    + saved.error().message,
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }
    static_cast<void>(m_shell->notificationPort().show(
        Application::NotificationRequest{
            .message = "Teacher Profile saved.",
            .severity = Application::NotificationSeverity::Information
        }
        ));
}

void ApplicationBootstrap::saveWorkspaceAsFromMenu()
{
    if (!m_shell || !m_applicationRuntime
        || !m_applicationRuntime->workspaceState().snapshot().session().has_value())
    {
        return;
    }

    const bool setupLifecyclePending = m_initialSetupLifecycle
        && m_initialSetupLifecycle->hasPendingSetup();
    const Ui::InitialSetupPageState setupPageState = initialSetupPageState();
    if (setupLifecyclePending
        || setupPageState.setupPending
        || setupPageState.recoveryAvailable
        || setupPageState.recoveryError)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Resolve the pending setup or recovery state before using Save As. Setup and recovery data were left unchanged.",
                .severity = Application::NotificationSeverity::Warning
            }
            ));
        return;
    }

    if (!resolveActivePageChangesBeforeLeaving(
            "saving a copy of this Teacher Profile"
            ))
    {
        return;
    }

    const Application::WorkspaceSession session =
        *m_applicationRuntime->workspaceState().snapshot().session();
    const QString activePath = QString::fromUtf8(
        session.location().value().data(),
        static_cast<qsizetype>(session.location().value().size())
        );

    Platform::QSettingsFileDialogDirectoryPreferencesAdapter
        directoryPreferences;
    std::unique_ptr<Application::FileDialogPort> ownedFileDialog;
    if (m_startupWorkspaceSeams.saveAsProfileFileDialogPortFactory)
    {
        ownedFileDialog =
            m_startupWorkspaceSeams.saveAsProfileFileDialogPortFactory();
    }
    else
    {
        ownedFileDialog = std::make_unique<Ui::QtFileDialogPresenter>(
            directoryPreferences,
            m_shell.get()
            );
    }
    if (!ownedFileDialog)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The profile save dialog is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    std::unique_ptr<Application::RecentWorkspaceHistoryPort> ownedRecentHistory;
    if (m_startupWorkspaceSeams.saveAsRecentWorkspaceHistoryPortFactory)
    {
        ownedRecentHistory =
            m_startupWorkspaceSeams.saveAsRecentWorkspaceHistoryPortFactory();
    }
    else
    {
        ownedRecentHistory = std::make_unique<
            Platform::SettingsManagerRecentWorkspaceHistoryPort>();
    }

    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceSaveAsFlow flow(
        *ownedFileDialog,
        pathPolicy,
        m_applicationRuntime->workspaceCoordinator(),
        ownedRecentHistory.get(),
        m_shell->notificationPort(),
        Application::FileDialogPath(
            QFileInfo(activePath).absolutePath().toUtf8().toStdString()
            ),
        QFileInfo(activePath).fileName().toUtf8().toStdString(),
        [this] {
            if (m_initialSetupLifecycle
                && m_initialSetupLifecycle->hasPendingSetup())
            {
                return true;
            }
            const Ui::InitialSetupPageState state = initialSetupPageState();
            return state.setupPending
                || state.recoveryAvailable
                || state.recoveryError;
        }
        );
    static_cast<void>(flow.run());
}

void ApplicationBootstrap::exportWorkspaceFromMenu()
{
    if (!m_shell || !m_applicationRuntime || !m_initialSetupLifecycle
        || !m_applicationRuntime->workspaceState().snapshot().session().has_value())
    {
        return;
    }

    const bool setupLifecyclePending = m_initialSetupLifecycle->hasPendingSetup();
    const Ui::InitialSetupPageState setupPageState = initialSetupPageState();
    if (setupLifecyclePending
        || setupPageState.setupPending
        || setupPageState.recoveryAvailable
        || setupPageState.recoveryError)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Resolve the pending setup or recovery state before exporting this Teacher Profile. Setup and recovery data were left unchanged.",
                .severity = Application::NotificationSeverity::Warning
            }
            ));
        return;
    }

    const Application::WorkspaceSession session =
        *m_applicationRuntime->workspaceState().snapshot().session();
    const QString activePath = QString::fromUtf8(
        session.location().value().data(),
        static_cast<qsizetype>(session.location().value().size())
        );

    Platform::QSettingsFileDialogDirectoryPreferencesAdapter
        directoryPreferences;
    std::unique_ptr<Application::FileDialogPort> ownedFileDialog;
    if (m_startupWorkspaceSeams.exportProfileFileDialogPortFactory)
    {
        ownedFileDialog =
            m_startupWorkspaceSeams.exportProfileFileDialogPortFactory();
    }
    else
    {
        ownedFileDialog = std::make_unique<Ui::QtFileDialogPresenter>(
            directoryPreferences,
            m_shell.get()
            );
    }
    if (!ownedFileDialog)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The profile export dialog is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceExportFlow flow(
        *ownedFileDialog,
        pathPolicy,
        m_applicationRuntime->workspaceCoordinator(),
        m_shell->notificationPort(),
        Application::FileDialogPath(
            QFileInfo(activePath).absolutePath().toUtf8().toStdString()
            ),
        [this] {
            if (m_initialSetupLifecycle
                && m_initialSetupLifecycle->hasPendingSetup())
            {
                return true;
            }
            const Ui::InitialSetupPageState state = initialSetupPageState();
            return state.setupPending
                || state.recoveryAvailable
                || state.recoveryError;
        }
        );
    static_cast<void>(flow.run());
}

void ApplicationBootstrap::closeWorkspaceFromMenu()
{
    if (!m_shell || !m_applicationRuntime
        || !m_applicationRuntime->workspaceState().snapshot().session().has_value())
    {
        return;
    }

    const bool setupLifecyclePending = m_initialSetupLifecycle
        && m_initialSetupLifecycle->hasPendingSetup();
    const Ui::InitialSetupPageState setupPageState = initialSetupPageState();
    if (setupLifecyclePending
        || setupPageState.setupPending
        || setupPageState.recoveryAvailable
        || setupPageState.recoveryError)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "Resolve the pending setup or recovery state before closing this Teacher Profile. Setup and recovery data were left unchanged.",
                .severity = Application::NotificationSeverity::Warning
            }
            ));
        return;
    }

    if (!resolveActivePageChangesBeforeLeaving(
            "closing this Teacher Profile"
            ))
    {
        return;
    }

    const Domain::Result<void> closed =
        m_applicationRuntime->workspaceCoordinator().closeWorkspace();
    if (!closed)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The Teacher Profile could not be closed: "
                    + closed.error().message,
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return;
    }

    m_shell->updateNavigationAccessContext(
        Application::WorkspaceLifecycleState::Closed,
        std::nullopt
        );
    if (!m_shell->navigateTo(
            pageId(QStringLiteral("initial_setup")),
            Ui::NavigationCause::Selection
            ))
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The Teacher Profile closed, but the initial page could not be selected.",
                .severity = Application::NotificationSeverity::Warning
            }
            ));
        return;
    }

    static_cast<void>(m_shell->notificationPort().show(
        Application::NotificationRequest{
            .message = "Teacher Profile closed.",
            .severity = Application::NotificationSeverity::Information
        }
        ));
}

bool ApplicationBootstrap::requestWindowClose()
{
    if (!m_shell)
    {
        return false;
    }
    if (!resolveActivePageChangesBeforeLeaving("closing ClassMngr"))
    {
        return false;
    }

    const Application::ShutdownResult result =
        m_shutdownCoordinator.requestShutdown();
    if (!result.shutdownRequested)
    {
        QString message = "ClassMngr could not close safely.";
        if (result.failure == Application::ShutdownFailureReason::SettingsFlushFailed)
        {
            message = "Settings could not be saved. ClassMngr remains open.";
        }
        else if (result.failure == Application::ShutdownFailureReason::WorkspaceCloseFailed)
        {
            message = "The Teacher Profile could not be closed. ClassMngr remains open.";
        }
        else if (result.failure == Application::ShutdownFailureReason::AlreadyInProgress)
        {
            message = "ClassMngr is already closing.";
        }
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = message.toStdString(),
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return false;
    }

    if (result.newlyRequested)
    {
        scheduleShutdownFinalization();
    }
    return true;
}

void ApplicationBootstrap::resumeInitialSetupProfile()
{
    if (!m_initialSetupLifecyclePort || !m_initialSetupLifecycle)
    {
        return;
    }
    const auto recovery = m_initialSetupLifecyclePort->pendingRecovery();
    if (!recovery || !recovery.value().has_value())
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "The interrupted setup could not be found."
            );
        return;
    }

    const auto resumed = m_initialSetupLifecycle->resume(*recovery.value());
    if (!resumed.started())
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "Setup could not resume: %1"
            ).arg(QString::fromUtf8(
                resumed.error.message.data(),
                static_cast<qsizetype>(resumed.error.message.size())
                ));
        return;
    }
    m_initialSetupClassDetailsDraft.reset();
    m_initialSetupClassId.reset();
    m_initialSetupClassTimesSaved = false;
    m_initialSetupStatusMessage.clear();
}

void ApplicationBootstrap::restoreInitialSetupProfile()
{
    if (!m_initialSetupLifecyclePort || !m_initialSetupLifecycle)
    {
        return;
    }
    const auto recovery = m_initialSetupLifecyclePort->pendingRecovery();
    if (!recovery || !recovery.value().has_value())
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "The interrupted setup could not be found."
            );
        return;
    }

    const auto restored = m_initialSetupLifecycle->restore(*recovery.value());
    if (restored.status == Application::InitialSetupCancelStatus::Canceled)
    {
        m_initialSetupClassDetailsDraft.reset();
        m_initialSetupClassId.reset();
        m_initialSetupClassTimesSaved = false;
    }
    m_initialSetupStatusMessage = restored.status
            == Application::InitialSetupCancelStatus::Canceled
        ? QCoreApplication::translate(
              "InitialPage",
              "The previous profile was restored."
              )
        : QCoreApplication::translate(
              "InitialPage",
              "The profile could not be restored. The recovery files are still available."
              );
}

void ApplicationBootstrap::cancelInitialSetupProfile()
{
    if (!m_initialSetupLifecycle)
    {
        return;
    }
    const auto canceled = m_initialSetupLifecycle->cancel();
    if (canceled.status == Application::InitialSetupCancelStatus::Canceled)
    {
        m_initialSetupClassDetailsDraft.reset();
        m_initialSetupClassId.reset();
        m_initialSetupClassTimesSaved = false;
    }
    m_initialSetupStatusMessage = canceled.status
            == Application::InitialSetupCancelStatus::Canceled
        ? QString{}
        : QCoreApplication::translate(
              "InitialPage",
              "The setup could not be canceled. The recovery files are still available."
              );
}

void ApplicationBootstrap::finishInitialSetupProfile()
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return;
    }

    const Application::WorkspaceLocation profile =
        m_initialSetupLifecycle->pendingProfile();
    const auto finished = m_initialSetupLifecycle->finish();
    m_initialSetupClassDetailsDraft.reset();
    m_initialSetupClassId.reset();
    m_initialSetupClassTimesSaved = false;

    if (!finished.wasActive)
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage", "Setup was not active."
            );
        return;
    }
    if (finished.originalBackupRemovalFailed)
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "Setup data was saved, but the previous profile backup could not be removed. Recovery files were kept."
            );
        return;
    }

    std::unique_ptr<Application::RecentWorkspaceHistoryPort> historyPort =
        m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory
        ? m_startupWorkspaceSeams.recentWorkspaceHistoryPortFactory()
        : std::make_unique<Platform::SettingsManagerRecentWorkspaceHistoryPort>();
    if (!historyPort)
    {
        m_initialSetupStatusMessage = QCoreApplication::translate(
            "InitialPage",
            "Setup is complete, but recent profile history could not be updated."
            );
        return;
    }

    const QString rawProfilePath = QString::fromUtf8(
        profile.value().data(),
        static_cast<qsizetype>(profile.value().size())
        );
    const QString normalizedPath = normalizedStartupWorkspacePath(rawProfilePath);
    const Application::RecentWorkspaceHistory history =
        Application::RecentWorkspaceHistoryUseCase::record(
            historyPort->load(),
            Application::RecentWorkspacePath(profile.value()),
            Application::RecentWorkspacePath(normalizedPath.toUtf8().toStdString())
            );
    const Domain::Result<void> saved = historyPort->save(history);
    m_initialSetupStatusMessage = saved
        ? QCoreApplication::translate("InitialPage", "Setup is complete.")
        : QCoreApplication::translate(
              "InitialPage",
              "Setup is complete, but recent profile history could not be updated: %1"
              ).arg(QString::fromUtf8(
                  saved.error().message.data(),
                  static_cast<qsizetype>(saved.error().message.size())
                  ));
}

Application::PersonalDetailsReadResult
ApplicationBootstrap::loadInitialSetupPersonalDetails() const
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Application::PersonalDetailsReadResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    auto* const storeGateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!storeGateway)
    {
        return Application::PersonalDetailsReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The setup profile settings store is unavailable.",
            .recoverable = true
        });
    }

    return storeGateway->readPersonalDetails();
}

Application::PersonalDetailsSaveResult
ApplicationBootstrap::saveInitialSetupPersonalDetails(
    const Application::PersonalDetailsSaveRequest& request
    ) const
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Application::PersonalDetailsSaveResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    auto* const storeGateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!storeGateway)
    {
        return Application::PersonalDetailsSaveResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The setup profile settings store is unavailable.",
            .recoverable = true
        });
    }

    return storeGateway->savePersonalDetails(request);
}

Application::InitialSetupTeacherChoicesResult
ApplicationBootstrap::loadInitialSetupTeacherChoices() const
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Application::InitialSetupTeacherChoicesResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    auto* const storeGateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!storeGateway)
    {
        return Application::InitialSetupTeacherChoicesResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The setup profile teacher store is unavailable.",
            .recoverable = true
        });
    }

    return storeGateway->loadInitialSetupTeacherChoices();
}

Domain::Result<Application::CustomColorPalette>
ApplicationBootstrap::readInitialSetupClassColorPalette() const
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Domain::Result<Application::CustomColorPalette>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    auto* const storeGateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!storeGateway)
    {
        return Domain::Result<Application::CustomColorPalette>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The setup profile settings store is unavailable.",
            .recoverable = true
        });
    }

    return storeGateway->readInitialSetupClassColorPalette();
}

Domain::Result<void> ApplicationBootstrap::saveInitialSetupClassColorPalette(
    const Application::CustomColorPalette& palette
    ) const
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    auto* const storeGateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!storeGateway)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The setup profile settings store is unavailable.",
            .recoverable = true
        });
    }

    return storeGateway->saveInitialSetupClassColorPalette(palette);
}

Application::InitialSetupClassDetailsDraftCaptureResult
ApplicationBootstrap::captureInitialSetupClassDetailsDraft(
    const Application::InitialSetupClassDetailsDraft& draft
    )
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Application::InitialSetupClassDetailsDraftCaptureResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    const Application::InitialSetupClassDetailsRequirements requirements =
        Application::initialSetupClassDetailsRequirements(
            Application::isCanonicalPositiveDecimalId64(
                draft.koreanTeacherId.value()
                ),
            draft.classColorChosen && !draft.classColor.empty(),
            !draft.classGrade.empty(),
            !draft.classLevel.empty()
            );
    if (!requirements.complete())
    {
        return Application::InitialSetupClassDetailsDraftCaptureResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = "Korean teacher, chosen color, Grade, and Level are required.",
            .recoverable = true
        });
    }

    if (!m_initialSetupClassDetailsDraft.has_value()
        || *m_initialSetupClassDetailsDraft != draft)
    {
        m_initialSetupClassId.reset();
        m_initialSetupClassTimesSaved = false;
    }
    m_initialSetupClassDetailsDraft = draft;
    return Application::InitialSetupClassDetailsDraftCaptureResult::success();
}

Application::InitialSetupClassTimesSaveResult
ApplicationBootstrap::saveInitialSetupClassTimes(
    const Application::InitialSetupClassTimes& times
    )
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup()
        || !m_initialSetupClassDetailsDraft.has_value())
    {
        return Application::InitialSetupClassTimesSaveResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "Class details are not ready for setup.",
            .recoverable = true
        });
    }
    if (!Application::initialSetupClassTimesRequirementMet(
            !times.regularTimes.empty(),
            !times.intensiveTimes.empty()
            ))
    {
        return Application::InitialSetupClassTimesSaveResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = "At least one class time is required.",
            .recoverable = true
        });
    }
    if (m_initialSetupClassTimesSaved)
    {
        return Application::InitialSetupClassTimesSaveResult::success();
    }

    auto* const storeGateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!storeGateway)
    {
        return Application::InitialSetupClassTimesSaveResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The setup profile class store is unavailable.",
            .recoverable = true
        });
    }

    if (!m_initialSetupClassId.has_value())
    {
        const Application::ClassCreateResult created =
            storeGateway->createInitialSetupClass();
        if (!created)
        {
            m_initialSetupClassId.reset();
            return Application::InitialSetupClassTimesSaveResult::failure(
                created.error()
                );
        }
        m_initialSetupClassId = created.value();
    }

    const Application::InitialSetupClassDetailsDraft& draft =
        *m_initialSetupClassDetailsDraft;
    const Application::InitialSetupClassTimesSaveRequest request{
        .classId = *m_initialSetupClassId,
        .teacherId = draft.koreanTeacherId,
        .classColor = draft.classColor,
        .fontColor = draft.fontColor,
        .classGrade = draft.classGrade,
        .classLevel = draft.classLevel,
        .readingBook = draft.readingBook,
        .essayBook = draft.essayBook,
        .regularTimes = times.regularTimes,
        .intensiveTimes = times.intensiveTimes
    };
    const Application::InitialSetupClassTimesSaveResult saved =
        storeGateway->saveInitialSetupClassTimes(request);
    if (!saved)
    {
        return saved;
    }
    m_initialSetupClassTimesSaved = true;
    return Application::InitialSetupClassTimesSaveResult::success();
}

Application::TeacherCreateResult ApplicationBootstrap::saveInitialSetupTeacher(
    const Application::TeacherCreateRequest& request
    ) const
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Application::TeacherCreateResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    auto* const storeGateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!storeGateway)
    {
        return Application::TeacherCreateResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The setup profile teacher store is unavailable.",
            .recoverable = true
        });
    }

    return storeGateway->createInitialSetupTeacher(request);
}

Domain::Result<std::optional<QString>>
ApplicationBootstrap::chooseInitialSetupTeacherImportWorkbook()
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Domain::Result<std::optional<QString>>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    const QByteArray title = QCoreApplication::translate(
        "InitialSetupTeacherImportDialog",
        "Select Teacher Import File"
        ).toUtf8();
    const QByteArray filter = QCoreApplication::translate(
        "InitialSetupTeacherImportDialog",
        "Excel Workbooks (*.xlsx)"
        ).toUtf8();
    const Application::OpenFileDialogRequest request{
        .purpose = Application::FileDialogPurpose::ImportWorkbook,
        .title = std::string(title.constData(),
                             static_cast<std::size_t>(title.size())),
        .nameFilters = {
            std::string(filter.constData(),
                        static_cast<std::size_t>(filter.size()))
        }
    };

    using Response = Application::FileDialogResponse<
        Application::FileDialogPath>;
    const Domain::Result<Response> selected = [&]() -> Domain::Result<Response> {
        if (m_startupWorkspaceSeams.initialSetupFileDialogPortFactory)
        {
            std::unique_ptr<Application::FileDialogPort> fileDialog =
                m_startupWorkspaceSeams.initialSetupFileDialogPortFactory();
            if (!fileDialog)
            {
                return Domain::Result<Response>::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "The teacher import file dialog is unavailable.",
                    .recoverable = true
                });
            }
            return fileDialog->openOneFile(request);
        }

        if (!m_shell)
        {
            return Domain::Result<Response>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The teacher import file dialog is unavailable.",
                .recoverable = true
            });
        }

        Platform::QSettingsFileDialogDirectoryPreferencesAdapter
            directoryPreferences;
        Ui::QtFileDialogPresenter fileDialog(directoryPreferences, m_shell.get());
        return fileDialog.openOneFile(request);
    }();

    if (!selected)
    {
        return Domain::Result<std::optional<QString>>::failure(
            selected.error());
    }
    if (!selected.value().selection.has_value())
    {
        return Domain::Result<std::optional<QString>>::success(std::nullopt);
    }

    const std::string& pathBytes = selected.value().selection->value();
    return Domain::Result<std::optional<QString>>::success(QString::fromUtf8(
        pathBytes.data(), static_cast<qsizetype>(pathBytes.size())));
}

Domain::Result<std::optional<QString>>
ApplicationBootstrap::chooseInitialSetupScheduleImportWorkbook()
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Domain::Result<std::optional<QString>>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    const QByteArray title = QCoreApplication::translate(
        "InitialSetupScheduleImportDialog",
        "Select Schedule Import File"
        ).toUtf8();
    const QByteArray filter = QCoreApplication::translate(
        "InitialSetupScheduleImportDialog",
        "Excel Workbooks (*.xlsx)"
        ).toUtf8();
    const Application::OpenFileDialogRequest request{
        .purpose = Application::FileDialogPurpose::ImportWorkbook,
        .title = std::string(title.constData(),
                             static_cast<std::size_t>(title.size())),
        .nameFilters = {
            std::string(filter.constData(),
                        static_cast<std::size_t>(filter.size()))
        }
    };

    using Response = Application::FileDialogResponse<
        Application::FileDialogPath>;
    const Domain::Result<Response> selected = [&]() -> Domain::Result<Response> {
        if (m_startupWorkspaceSeams.initialSetupFileDialogPortFactory)
        {
            std::unique_ptr<Application::FileDialogPort> fileDialog =
                m_startupWorkspaceSeams.initialSetupFileDialogPortFactory();
            if (!fileDialog)
            {
                return Domain::Result<Response>::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "The schedule import file dialog is unavailable.",
                    .recoverable = true
                });
            }
            return fileDialog->openOneFile(request);
        }

        if (!m_shell)
        {
            return Domain::Result<Response>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The schedule import file dialog is unavailable.",
                .recoverable = true
            });
        }

        Platform::QSettingsFileDialogDirectoryPreferencesAdapter
            directoryPreferences;
        Ui::QtFileDialogPresenter fileDialog(directoryPreferences, m_shell.get());
        return fileDialog.openOneFile(request);
    }();

    if (!selected)
    {
        return Domain::Result<std::optional<QString>>::failure(selected.error());
    }
    if (!selected.value().selection.has_value())
    {
        return Domain::Result<std::optional<QString>>::success(std::nullopt);
    }

    const std::string& pathBytes = selected.value().selection->value();
    return Domain::Result<std::optional<QString>>::success(QString::fromUtf8(
        pathBytes.data(), static_cast<qsizetype>(pathBytes.size())));
}

std::expected<
    Application::TeacherImportUseCaseResult,
    Application::TeacherImportUseCaseError
    > ApplicationBootstrap::applyInitialSetupTeacherImport(
        const Application::TeacherImportUseCaseRequest& request
        ) const
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return std::unexpected(Application::TeacherImportUseCaseError{
            .kind = Application::TeacherImportUseCaseErrorKind::InvalidRequest
        });
    }

    auto* const storeGateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!storeGateway)
    {
        return std::unexpected(Application::TeacherImportUseCaseError{
            .kind = Application::TeacherImportUseCaseErrorKind::InvalidRequest
        });
    }

    return storeGateway->applyTeacherImportUseCase(request);
}

Domain::Result<std::optional<QByteArray>>
ApplicationBootstrap::chooseInitialSetupSignatureImage()
{
    if (!m_initialSetupLifecycle
        || !m_initialSetupLifecycle->hasPendingSetup())
    {
        return Domain::Result<std::optional<QByteArray>>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "No setup profile is active.",
            .recoverable = true
        });
    }

    const QByteArray title = QCoreApplication::translate(
        "PersonalDetailsPage",
        "Choose Signature Image"
        ).toUtf8();
    const QByteArray supportedImages = QCoreApplication::translate(
        "PersonalDetailsPage",
        "Supported Images (%1)"
        ).arg(QStringLiteral("*.png *.jpg *.jpeg *.bmp *.gif *.webp")).toUtf8();
    const Application::OpenFileDialogRequest request{
        .purpose = Application::FileDialogPurpose::SignatureImage,
        .title = std::string(title.constData(),
                             static_cast<std::size_t>(title.size())),
        .nameFilters = {
            std::string(
                supportedImages.constData(),
                static_cast<std::size_t>(supportedImages.size())
                )
        }
    };

    using Response = Application::FileDialogResponse<
        Application::FileDialogPath>;
    const Domain::Result<Response> selected = [&]() -> Domain::Result<Response> {
        if (m_startupWorkspaceSeams.initialSetupFileDialogPortFactory)
        {
            std::unique_ptr<Application::FileDialogPort> fileDialog =
                m_startupWorkspaceSeams.initialSetupFileDialogPortFactory();
            if (!fileDialog)
            {
                return Domain::Result<Response>::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "The signature image file dialog is unavailable.",
                    .recoverable = true
                });
            }
            return fileDialog->openOneFile(request);
        }

        if (!m_shell)
        {
            return Domain::Result<Response>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The signature image file dialog is unavailable.",
                .recoverable = true
            });
        }

        Platform::QSettingsFileDialogDirectoryPreferencesAdapter
            directoryPreferences;
        Ui::QtFileDialogPresenter fileDialog(directoryPreferences, m_shell.get());
        return fileDialog.openOneFile(request);
    }();

    if (!selected)
    {
        return Domain::Result<std::optional<QByteArray>>::failure(
            selected.error()
            );
    }
    if (!selected.value().selection.has_value())
    {
        return Domain::Result<std::optional<QByteArray>>::success(std::nullopt);
    }

    const std::string& pathBytes = selected.value().selection->value();
    const QString path = QString::fromUtf8(
        pathBytes.data(),
        static_cast<qsizetype>(pathBytes.size())
        );
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        const QByteArray diagnostic = file.errorString().toUtf8();
        std::string message = "The selected signature image could not be opened.";
        if (!diagnostic.isEmpty())
        {
            message.append(" ");
            message.append(
                diagnostic.constData(),
                static_cast<std::size_t>(diagnostic.size())
                );
        }
        return Domain::Result<std::optional<QByteArray>>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = true
        });
    }

    return Domain::Result<std::optional<QByteArray>>::success(file.readAll());
}

bool ApplicationBootstrap::releaseWorkspace() noexcept
{
    if (m_applicationRuntime
        && m_applicationRuntime->workspaceState().snapshot().session().has_value())
    {
        try
        {
            const Domain::Result<void> closeResult =
                m_applicationRuntime->workspaceCoordinator().closeWorkspace();
            if (!closeResult)
            {
#ifndef NDEBUG
                qWarning().noquote()
                    << "ClassMngrNext could not close the workspace during teardown:"
                    << QString::fromUtf8(closeResult.error().message.data(),
                                         static_cast<qsizetype>(closeResult.error().message.size()));
#endif
                return false;
            }
        }
        catch (...)
        {
#ifndef NDEBUG
            qWarning() << "ClassMngrNext could not close the workspace during teardown.";
#endif
            return false;
        }
    }

    // Setup and the runtime borrow the gateway; release borrowers first.
    m_initialSetupLifecycle.reset();
    m_initialSetupLifecyclePort.reset();
    m_applicationRuntime.reset();
    m_workspaceGateway.reset();
    return true;
}

void ApplicationBootstrap::scheduleStartupUpdateCheck()
{
    if (m_startupUpdateCheckScheduled
        || m_startupUpdateCheckConsumed
        || m_shutdownCoordinator.shutdownRequested()
        || m_startupCoordinator.result().status
            != Application::StartupStatus::StartupReady
        || !m_shell)
    {
        return;
    }

    m_startupUpdateCheckScheduled = true;
    const std::weak_ptr<int> lifetime(m_callbackLifetime);
    QTimer::singleShot(0, &m_application, [this, lifetime]
    {
        if (lifetime.expired())
        {
            return;
        }
        tryDispatchStartupUpdateCheck();
    });
}

void ApplicationBootstrap::tryDispatchStartupUpdateCheck()
{
    if (!m_startupUpdateCheckScheduled
        || m_startupUpdateCheckConsumed
        || m_shutdownCoordinator.shutdownRequested()
        || m_startupCoordinator.result().status
            != Application::StartupStatus::StartupReady
        || !m_shell)
    {
        return;
    }

    const std::optional<QString> activePageKey =
        m_shell->pageHost().activePageKey();
    const Ui::NavigationModel* const navigationModel =
        m_shell->navigationModel();
    if (!m_shell->isVisible()
        || !navigationModel
        || !navigationModel->initialPageId()
        || !activePageKey.has_value()
        || *activePageKey
            != navigationModel->initialPageId()->value())
    {
        return;
    }

    m_startupUpdateCheckConsumed = true;

    try
    {
        m_startupUpdateDispatcher = m_startupUpdateDispatcherFactory
            ? m_startupUpdateDispatcherFactory()
            : makeStartupUpdateDispatcher();
        if (!m_startupUpdateDispatcher)
        {
            qWarning() << "ClassMngrNext startup update dispatcher is unavailable.";
            return;
        }
        m_startupUpdateDispatcher->dispatch();
    }
    catch (const std::exception& exception)
    {
        qWarning().noquote()
            << "ClassMngrNext startup update check could not be dispatched:"
            << exception.what();
    }
    catch (...)
    {
        qWarning() << "ClassMngrNext startup update check could not be dispatched.";
    }
}

Application::StartupResult ApplicationBootstrap::failStartupStage(
    const Application::StartupStage stage,
    const Application::StartupFailureReason reason
    )
{
    const Application::StartupResult failure = m_startupCoordinator.recordStage(
        stage,
        Application::StartupStageOutcome::Failed,
        reason
        );
    observeStartupResult(failure);
    rollbackStartupComposition();
    return failure;
}

void ApplicationBootstrap::rollbackStartupComposition() noexcept
{
    m_shell.reset();
    static_cast<void>(releaseWorkspace());

    if (m_coreServices)
    {
        if (m_coreServices->themePresenter)
        {
            m_coreServices->themePresenter->restore();
        }
        if (m_coreServices->fontSizePresenter)
        {
            m_coreServices->fontSizePresenter->restore();
        }
        m_coreServices->translation.reset();
        m_coreServices->pretendardFont.reset();
        m_coreServices->interFont.reset();
    }

    if (m_quitOnLastWindowClosedChanged)
    {
        m_application.setQuitOnLastWindowClosed(
            m_previousQuitOnLastWindowClosed
            );
        m_quitOnLastWindowClosedChanged = false;
    }
}

bool ApplicationBootstrap::createCoreServices()
{
    const Resources::ResourceResult<Resources::ResourcePackagePaths> packagePaths =
        m_resourcePackagePathsResolver
            ? m_resourcePackagePathsResolver()
            : Resources::ResourceRootResolver::resolve();
    if (!packagePaths)
    {
        return false;
    }

    auto catalog = Resources::ResourceCatalog::load(
        packagePaths->catalogFilePath,
        packagePaths->resourceRoot
        );
    if (!catalog)
    {
        return false;
    }

    auto startupScope = Resources::ResourceScope::create(
        Resources::ResourceScopeKind::Startup,
        QStringLiteral("application-bootstrap")
        );
    if (!startupScope)
    {
        return false;
    }

    m_coreServices = std::make_unique<CoreServices>(
        std::move(*catalog),
        std::move(*startupScope)
        );
    m_coreServices->themePresenter = std::make_unique<Ui::ThemePresenter>(
        m_application,
        m_coreServices->resourceLoader,
        m_coreServices->startupResourceScope,
        m_coreServices->preferences
        );
    m_coreServices->fontSizePresenter = std::make_unique<Ui::FontSizePresenter>(m_application);
    return true;
}

Application::ShutdownResult ApplicationBootstrap::shutdown()
{
    const Application::ShutdownResult result =
        m_shutdownCoordinator.requestShutdown();
    if (result.shutdownRequested && !m_shutdownFinalizationScheduled)
    {
        finalizeShutdown();
    }
    return result;
}

std::optional<Application::ShutdownFailureReason>
ApplicationBootstrap::runShutdownSequence() noexcept
{
#ifndef NDEBUG
    qInfo() << "ClassMngrNext shutdown: flushing settings.";
#endif
    bool settingsFlushed = false;
    try
    {
        if (m_shutdownSeams.flushSettings)
        {
            settingsFlushed = m_shutdownSeams.flushSettings();
        }
        else
        {
            SettingsManager::instance().sync();
            settingsFlushed = true;
        }
    }
    catch (...)
    {
        settingsFlushed = false;
    }
    if (!settingsFlushed)
    {
#ifndef NDEBUG
        qWarning() << "ClassMngrNext shutdown stopped because settings could not be flushed.";
#endif
        return Application::ShutdownFailureReason::SettingsFlushFailed;
    }

    if (m_startupUpdateDispatcher)
    {
#ifndef NDEBUG
        qInfo() << "ClassMngrNext shutdown: cancelling the startup update check.";
#endif
        m_startupUpdateDispatcher->cancel();
        m_startupUpdateDispatcher.reset();
    }

    if (m_applicationRuntime
        && m_applicationRuntime->workspaceState().snapshot().session().has_value())
    {
#ifndef NDEBUG
        qInfo() << "ClassMngrNext shutdown: closing the workspace session.";
#endif
        try
        {
            const Domain::Result<void> closeResult =
                m_applicationRuntime->workspaceCoordinator().closeWorkspace();
            if (!closeResult)
            {
#ifndef NDEBUG
                qWarning().noquote()
                    << "ClassMngrNext shutdown stopped because the workspace could not be closed:"
                    << QString::fromUtf8(closeResult.error().message.data(),
                                         static_cast<qsizetype>(closeResult.error().message.size()));
#endif
                return Application::ShutdownFailureReason::WorkspaceCloseFailed;
            }
        }
        catch (...)
        {
#ifndef NDEBUG
            qWarning() << "ClassMngrNext shutdown stopped because the workspace close threw.";
#endif
            return Application::ShutdownFailureReason::WorkspaceCloseFailed;
        }
    }

#ifndef NDEBUG
    qInfo() << "ClassMngrNext shutdown: owned work and workspace are settled.";
#endif
    return std::nullopt;
}

void ApplicationBootstrap::scheduleShutdownFinalization()
{
    if (m_shutdownFinalized || m_shutdownFinalizationScheduled)
    {
        return;
    }
    m_shutdownFinalizationScheduled = true;
    const std::weak_ptr<int> lifetime(m_callbackLifetime);
    QTimer::singleShot(0, &m_application, [this, lifetime]
    {
        if (lifetime.expired())
        {
            return;
        }
        m_shutdownFinalizationScheduled = false;
        finalizeShutdown();
    });
}

void ApplicationBootstrap::finalizeShutdown()
{
    if (m_shutdownFinalized)
    {
        return;
    }
    m_shutdownFinalized = true;
    m_callbackLifetime.reset();

#ifndef NDEBUG
    qInfo() << "ClassMngrNext shutdown: releasing page and startup resources.";
#endif
    // CloseEvent must return before its shell can be destroyed. The accepted
    // close path queues this finalizer for the next event-loop turn; direct
    // shutdown can release it synchronously.
    m_shell.reset();
    static_cast<void>(releaseWorkspace());
    m_coreServices.reset();
    m_startupDiagnosticsSink.reset();

    if (m_quitOnLastWindowClosedChanged)
    {
        m_application.setQuitOnLastWindowClosed(
            m_previousQuitOnLastWindowClosed
            );
        m_quitOnLastWindowClosedChanged = false;
    }

#ifndef NDEBUG
    qInfo() << "ClassMngrNext shutdown: requesting application quit.";
#endif
    m_application.quit();
}

bool ApplicationBootstrap::shutdownRequested() const noexcept
{
    return m_shutdownCoordinator.shutdownRequested();
}

Ui::ApplicationShell* ApplicationBootstrap::shell() noexcept
{
    return m_shell.get();
}

const Ui::ApplicationShell* ApplicationBootstrap::shell() const noexcept
{
    return m_shell.get();
}

bool ApplicationBootstrap::openTeacherProfile(
    const Domain::TeacherId& teacherId
    )
{
    if (!m_shell)
    {
        return false;
    }

    auto* const gateway = dynamic_cast<Platform::WorkspaceStoreGateway*>(
        m_workspaceGateway.get()
        );
    if (!gateway)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The active workspace store is unavailable.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return false;
    }

    // Read first so an invalid or missing requested profile cannot disturb the
    // currently selected page or its in-memory draft.
    const Application::TeacherProfileReadResult loaded =
        gateway->readTeacherProfile(teacherId);
    if (!loaded)
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = loaded.error().message,
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return false;
    }

    const QString profilePageKey = QStringLiteral("teacher_profile");
    const auto activePageKey = m_shell->pageHost().activePageKey();
    auto* profilePage = dynamic_cast<Ui::TeacherProfilePage*>(
        m_shell->pageHost().pageInstance(profilePageKey)
        );
    if (activePageKey == std::optional<QString>(profilePageKey))
    {
        if (!profilePage)
        {
            static_cast<void>(m_shell->notificationPort().show(
                Application::NotificationRequest{
                    .message = "The Teacher Profile page is unavailable.",
                    .severity = Application::NotificationSeverity::Error
                }
                ));
            return false;
        }
        if (profilePage->selectedTeacherId()
            == std::optional<Domain::TeacherId>(teacherId))
        {
            return true;
        }

        // ApplicationShell intentionally short-circuits same-page navigation,
        // so resolve a dirty profile explicitly before replacing its ID.
        if (!resolveActivePageChangesBeforeLeaving(
                "opening another teacher profile"))
        {
            return false;
        }
        return profilePage->displayProfile(loaded.value());
    }

    // For a different active page, the normal shell guard owns Save/Discard/
    // Cancel. No profile state is committed until the guarded switch succeeds.
    if (!m_shell->navigateTo(
            pageId(profilePageKey),
            Ui::NavigationCause::Selection
            ))
    {
        return false;
    }

    profilePage = dynamic_cast<Ui::TeacherProfilePage*>(
        m_shell->pageHost().pageInstance(profilePageKey)
        );
    if (!profilePage || !profilePage->displayProfile(loaded.value()))
    {
        static_cast<void>(m_shell->notificationPort().show(
            Application::NotificationRequest{
                .message = "The Teacher Profile page could not display the selected teacher.",
                .severity = Application::NotificationSeverity::Error
            }
            ));
        return false;
    }
    return true;
}

const std::optional<Application::StartupWorkspaceOutcome>&
ApplicationBootstrap::startupWorkspaceOutcome() const noexcept
{
    return m_startupWorkspaceOutcome;
}

Application::WorkspaceStateSnapshot
ApplicationBootstrap::workspaceStateSnapshot() const
{
    return m_applicationRuntime
        ? m_applicationRuntime->workspaceState().snapshot()
        : Application::WorkspaceStateSnapshot{};
}

const std::optional<Application::InitialSetupClassDetailsDraft>&
ApplicationBootstrap::initialSetupClassDetailsDraft() const noexcept
{
    return m_initialSetupClassDetailsDraft;
}

void ApplicationBootstrap::observeStartupResult(
    const Application::StartupResult& result
    ) const
{
    if (m_startupEventObserver && result.latestStageResult.has_value())
    {
        m_startupEventObserver(*result.latestStageResult);
    }
    if (m_startupDiagnosticsSink && result.latestStageResult.has_value())
    {
        m_startupDiagnosticsSink->record(*result.latestStageResult);
    }
}

}  // namespace ClassMngr::Next::Bootstrap
