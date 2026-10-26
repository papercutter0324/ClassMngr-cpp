#include "next/bootstrap/application_bootstrap.h"

#include "next/bootstrap/qt_startup_diagnostics_sink.h"
#include "next/platform/settings_manager_language_preferences_port.h"
#include "next/platform/settings_manager_theme_preferences_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/ui/application_shell.h"

#include <QApplication>
#include <QDebug>
#include <QLocale>
#include <QStringList>
#include <QStyleHints>

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

}  // namespace

struct ApplicationBootstrap::CoreServices final
{
    CoreServices(
        Resources::ResourceCatalog catalog,
        Resources::ResourceScope startupScope
        )
        : resourceLoader(std::move(catalog))
        , startupResourceScope(std::move(startupScope))
    {
    }

    Resources::ResourceLoader resourceLoader;
    Resources::ResourceScope startupResourceScope;
    Application::LanguagePreference languagePreference =
        Application::LanguagePreference::SystemDefault;
    Application::Theme themePreference = Application::Theme::SystemDefault;
    Application::Theme resolvedTheme = Application::Theme::Light;
    QString loadedLocale;
    std::optional<Resources::ScopedResource<Resources::ResourceTranslation>> translation;
    std::optional<Resources::ScopedResource<QString>> selectedStylesheet;
    std::optional<Resources::ScopedResource<Resources::RegisteredFont>> interFont;
    std::optional<Resources::ScopedResource<Resources::RegisteredFont>> pretendardFont;
    QString previousApplicationStylesheet;
    bool applicationStylesheetChanged = false;
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
    std::optional<QStringList> systemUiLanguages
    )
    : m_application(application),
      m_startupEventObserver(std::move(startupEventObserver)),
      m_startupDiagnosticsSinkFactory(std::move(startupDiagnosticsSinkFactory)),
      m_resourcePackagePathsResolver(std::move(resourcePackagePathsResolver)),
      m_systemUiLanguages(std::move(systemUiLanguages)),
      m_shutdownCoordinator([&application] {
          application.quit();
      })
{
}

ApplicationBootstrap::~ApplicationBootstrap()
{
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

    m_previousQuitOnLastWindowClosed = m_application.quitOnLastWindowClosed();
    m_application.setQuitOnLastWindowClosed(false);
    m_quitOnLastWindowClosedChanged = true;

    try
    {
        m_shell = std::make_unique<Ui::ApplicationShell>();
        m_shell->setCloseHandler([this] {
            static_cast<void>(shutdown());
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
    if (m_coreServices)
    {
        m_coreServices->previousApplicationStylesheet.clear();
        m_coreServices->applicationStylesheetChanged = false;
    }
    return initialPageReady;
}

bool ApplicationBootstrap::loadStartupPreferences()
{
    if (!m_coreServices)
    {
        return false;
    }

    m_coreServices->languagePreference =
        Platform::SettingsManagerLanguagePreferencesPort{}.read();
    m_coreServices->themePreference =
        Platform::SettingsManagerThemePreferencesPort{}.read();
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
    switch (m_coreServices->languagePreference)
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
    if (!m_coreServices)
    {
        return false;
    }

    Application::Theme resolvedTheme = m_coreServices->themePreference;
    if (resolvedTheme == Application::Theme::SystemDefault)
    {
        resolvedTheme = m_application.styleHints()->colorScheme()
                == Qt::ColorScheme::Dark
            ? Application::Theme::Dark
            : Application::Theme::Light;
    }

    const QString themeName = resolvedTheme == Application::Theme::Dark
        ? QStringLiteral("dark")
        : QStringLiteral("light");
    const auto request = startupRequest(
        m_coreServices->startupResourceScope,
        QStringLiteral("resource:assets/styles/%1.qss").arg(themeName)
        );
    if (!request)
    {
        qCritical().noquote() << "ClassMngrNext could not identify the selected stylesheet:"
                              << request.error().message;
        return false;
    }

    auto stylesheet = m_coreServices->resourceLoader.loadStylesheet(*request);
    if (!stylesheet)
    {
        qCritical().noquote() << "ClassMngrNext could not load selected stylesheet"
                              << themeName << ':' << stylesheet.error().message;
        return false;
    }

    m_coreServices->selectedStylesheet.emplace(std::move(*stylesheet));
    m_coreServices->resolvedTheme = resolvedTheme;
    m_coreServices->previousApplicationStylesheet = m_application.styleSheet();
    m_coreServices->applicationStylesheetChanged = true;
    m_application.setStyleSheet(m_coreServices->selectedStylesheet->get());
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
    return true;
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

    if (m_coreServices)
    {
        if (m_coreServices->applicationStylesheetChanged)
        {
            m_application.setStyleSheet(
                m_coreServices->previousApplicationStylesheet
                );
            m_coreServices->applicationStylesheetChanged = false;
        }
        m_coreServices->translation.reset();
        m_coreServices->pretendardFont.reset();
        m_coreServices->interFont.reset();
        m_coreServices->selectedStylesheet.reset();
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
    return true;
}

Application::ShutdownResult ApplicationBootstrap::shutdown()
{
    return m_shutdownCoordinator.requestShutdown();
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
