#include "visual_scenario_registry.h"

#include "app/mainwindow.h"
#include "core/build_info.h"
#include "core/fontmanager.h"
#include "core/language_service.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QSaveFile>
#include <QScreen>
#include <QSet>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>
#include <QStringList>
#include <QWidget>
#include <QWindow>

#include <cmath>
#include <memory>

#ifndef CLASSMNGR_PHASE0_FIXTURE_DIR
#define CLASSMNGR_PHASE0_FIXTURE_DIR ""
#endif

namespace
{
constexpr int SettleMilliseconds = 250;

QString themeName(Theme theme)
{
    return theme == Theme::Dark
        ? QStringLiteral("dark")
        : QStringLiteral("light");
}

QString appLanguageName(Language language)
{
    return language == Language::Korean
        ? QStringLiteral("ko")
        : QStringLiteral("en");
}

QString inputLanguageName(Language language)
{
    return language == Language::Korean
        ? QStringLiteral("ko-KR")
        : QStringLiteral("en-US");
}

QString architectureName()
{
    const QString architecture =
        QSysInfo::buildCpuArchitecture().toLower();

    return architecture.contains(QStringLiteral("arm"))
        ? QStringLiteral("ARM64")
        : QStringLiteral("x64");
}

QSet<const QWidget*> topLevelWidgets()
{
    QSet<const QWidget*> widgets;
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (widget)
        {
            widgets.insert(widget);
        }
    }
    return widgets;
}

QString topLevelWidgetNames(
    const QSet<const QWidget*>& baseline
    )
{
    QStringList names;
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (widget && !baseline.contains(widget))
        {
            names.append(
                QStringLiteral("%1 (%2)")
                    .arg(widget->objectName(), widget->metaObject()->className())
                );
        }
    }
    return names.join(QStringLiteral(", "));
}

void drainDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

QMenu* findMenu(
    QMenuBar* menuBar,
    const QString& expectedTitle
    )
{
    if (!menuBar)
    {
        return nullptr;
    }

    for (QAction* action : menuBar->actions())
    {
        QMenu* menu = action ? action->menu() : nullptr;
        if (!menu)
        {
            continue;
        }

        QString title = menu->title();
        title.remove(QLatin1Char('&'));
        if (title.trimmed().compare(expectedTitle, Qt::CaseInsensitive) == 0)
        {
            return menu;
        }
    }

    return nullptr;
}

int displayScalePercent(const QScreen* screen)
{
    if (!screen)
    {
        return 0;
    }

    // On Windows, logicalDotsPerInchX() remains the 96-DPI design baseline
    // even when the monitor is scaled. The device-pixel ratio is the Qt value
    // that reflects the effective per-monitor scale used by the native window.
    return qRound(screen->devicePixelRatio() * 100.0);
}

QString sha256ForFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        hash.addData(file.read(1024 * 1024));
    }

    return QString::fromLatin1(hash.result().toHex());
}

QStringList resourcePackIds()
{
    return {
        QStringLiteral("campuses"),
        QStringLiteral("documents"),
        QStringLiteral("files"),
        QStringLiteral("images"),
        QStringLiteral("splash"),
        QStringLiteral("templates")
    };
}

QString workspaceTabName(WorkspaceTab tab)
{
    switch (tab)
    {
    case WorkspaceTab::Details:
        return QStringLiteral("Personal Details");

    case WorkspaceTab::Schedule:
        return QStringLiteral("My Schedule");

    case WorkspaceTab::Calendar:
        return QStringLiteral("Calendar");
    }

    return QStringLiteral("Workspace");
}
} // namespace

class WindowsQtVisualCaptureTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void scenarioRegistryIsComplete();
    void capture_data();
    void capture();
    void cleanupTestCase();

private:
    void writeCaptureMetadata(
        const WindowsQtCaptureScenario& scenario,
        const QString& imagePath,
        const QString& imageFileName,
        int actualScalePercent,
        const QSize& actualWindowSize,
        const QRect& windowBounds,
        const QStringList& actions
        );

    QTemporaryDir m_settingsRoot;
    std::unique_ptr<LanguageService> m_languageService;
    QString m_captureRoot;
    QByteArray m_previousSettingsRoot;
    QByteArray m_previousAppData;
    QByteArray m_previousLocalAppData;
    bool m_previousSettingsRootWasSet = false;
    bool m_previousAppDataWasSet = false;
    bool m_previousLocalAppDataWasSet = false;
};

void WindowsQtVisualCaptureTests::initTestCase()
{
    if (QGuiApplication::platformName().compare(
            QStringLiteral("windows"),
            Qt::CaseInsensitive
            ) != 0)
    {
        QSKIP("Phase 0 evidence requires the native Windows Qt platform plugin.");
    }

    if (!QGuiApplication::primaryScreen())
    {
        QSKIP("Phase 0 evidence requires an interactive display.");
    }

    if (!m_settingsRoot.isValid())
    {
        QFAIL("Unable to create an isolated settings directory.");
    }

    m_previousSettingsRootWasSet =
        qEnvironmentVariableIsSet("CLASSMNGR_SETTINGS_ROOT");
    m_previousSettingsRoot = qgetenv("CLASSMNGR_SETTINGS_ROOT");
    m_previousAppDataWasSet = qEnvironmentVariableIsSet("APPDATA");
    m_previousAppData = qgetenv("APPDATA");
    m_previousLocalAppDataWasSet = qEnvironmentVariableIsSet("LOCALAPPDATA");
    m_previousLocalAppData = qgetenv("LOCALAPPDATA");
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
    qputenv("APPDATA", m_settingsRoot.path().toUtf8());
    qputenv("LOCALAPPDATA", m_settingsRoot.path().toUtf8());

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();

    // ResourcePackManager stores discovered-pack metadata below
    // AppDataLocation. Test mode keeps that mutable state out of the user's
    // profile and makes every evidence run disposable.
    QStandardPaths::setTestModeEnabled(true);

    const Status resourceStatus =
        ResourcePackManager::instance().initialize();
    if (!resourceStatus)
    {
        QFAIL(qPrintable(
            QStringLiteral("%1 Storage: %2")
                .arg(
                    resourceStatus.error(),
                    ResourcePackManager::instance().storageDirectory()
                    )
            ));
    }

    m_languageService = std::make_unique<LanguageService>();

    QString captureRoot =
        qEnvironmentVariable("CLASSMNGR_PHASE0_CAPTURE_ROOT");
    if (captureRoot.trimmed().isEmpty())
    {
        const QString runId =
            QDateTime::currentDateTimeUtc().toString(
                QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'")
                )
            + QStringLiteral("-")
            + QString::number(QCoreApplication::applicationPid());
        captureRoot = QDir::current().filePath(
            QStringLiteral("artifacts/phase0/windows-qt-visual/%1")
                .arg(runId)
            );
    }

    m_captureRoot = QFileInfo(captureRoot).absoluteFilePath();
    if (!QDir().mkpath(m_captureRoot))
    {
        QFAIL(qPrintable(
            QStringLiteral("Unable to create capture root: %1")
                .arg(m_captureRoot)
            ));
    }
}

void WindowsQtVisualCaptureTests::scenarioRegistryIsComplete()
{
    const auto& scenarios = windowsQtCaptureScenarios();
    QVERIFY2(!scenarios.isEmpty(), "The Phase 0 scenario registry is empty.");

    QSet<QString> ids;
    QSet<int> registeredPages;
    QSet<int> workspaceTabs;

    for (const WindowsQtCaptureScenario& scenario : scenarios)
    {
        QVERIFY2(!scenario.id.isEmpty(), "A capture scenario has no ID.");
        QVERIFY2(!ids.contains(scenario.id), qPrintable(
            QStringLiteral("Duplicate capture scenario: %1")
                .arg(scenario.id)
            ));
        ids.insert(scenario.id);

        QVERIFY2(!scenario.ledgerId.isEmpty(), qPrintable(
            QStringLiteral("Scenario has no ledger ID: %1")
                .arg(scenario.id)
            ));
        QVERIFY2(!scenario.artifactPrefix.isEmpty(), qPrintable(
            QStringLiteral("Scenario has no artifact prefix: %1")
                .arg(scenario.id)
            ));
        QVERIFY2(!scenario.state.isEmpty(), qPrintable(
            QStringLiteral("Scenario has no state: %1")
                .arg(scenario.id)
            ));
        QVERIFY2(!scenario.fixtureId.isEmpty(), qPrintable(
            QStringLiteral("Scenario has no fixture ID: %1")
                .arg(scenario.id)
            ));
        QVERIFY2(scenario.windowSize.width() >= 800
                     && scenario.windowSize.height() >= 600,
            qPrintable(
                QStringLiteral("Scenario window is below the supported size: %1")
                    .arg(scenario.id)
                ));

        switch (scenario.surface)
        {
        case WindowsQtCaptureSurface::RegisteredPage:
            registeredPages.insert(static_cast<int>(scenario.pageType));
            QVERIFY2(scenario.pageType != PageType::MyWorkspace,
                "My Workspace must be represented through a workspace tab.");
            break;

        case WindowsQtCaptureSurface::WorkspaceTab:
            QVERIFY2(scenario.pageType == PageType::MyWorkspace,
                "Workspace-tab scenarios must target My Workspace.");
            workspaceTabs.insert(static_cast<int>(scenario.workspaceTab));
            break;

        case WindowsQtCaptureSurface::Menu:
            QVERIFY2(!scenario.menuTitle.isEmpty(), qPrintable(
                QStringLiteral("Menu scenario has no menu title: %1")
                    .arg(scenario.id)
                ));
            break;
        }
    }

    const QList<PageType> expectedRegisteredPages{
        PageType::MyClasses,
        PageType::Schedule,
        PageType::Classes,
        PageType::TestingClasses,
        PageType::TeacherInfo,
        PageType::NativeEnglishTeachers,
        PageType::GsTeam,
        PageType::CampusDashboard,
        PageType::SubPrep,
        PageType::PdfViewer
    };
    for (PageType page : expectedRegisteredPages)
    {
        QVERIFY2(registeredPages.contains(static_cast<int>(page)),
            qPrintable(
                QStringLiteral("No registered-page scenario for %1")
                    .arg(PageManager::pageTypeIdentifier(page))
                ));
    }

    const QList<WorkspaceTab> expectedWorkspaceTabs{
        WorkspaceTab::Details,
        WorkspaceTab::Schedule,
        WorkspaceTab::Calendar
    };
    for (WorkspaceTab tab : expectedWorkspaceTabs)
    {
        QVERIFY2(workspaceTabs.contains(static_cast<int>(tab)),
            qPrintable(
                QStringLiteral("No workspace-tab scenario for %1")
                    .arg(workspaceTabName(tab))
                ));
    }
}

void WindowsQtVisualCaptureTests::capture_data()
{
    QTest::addColumn<QString>("scenarioId");

    for (const WindowsQtCaptureScenario& scenario :
         windowsQtCaptureScenarios())
    {
        const QByteArray rowName = scenario.id.toUtf8();
        QTest::newRow(rowName.constData()) << scenario.id;
    }
}

void WindowsQtVisualCaptureTests::capture()
{
    QFETCH(QString, scenarioId);

    const WindowsQtCaptureScenario* scenario =
        findWindowsQtCaptureScenario(scenarioId);
    QVERIFY2(scenario, qPrintable(
        QStringLiteral("Scenario disappeared from the registry: %1")
            .arg(scenarioId)
        ));

    SettingsManager::instance().clear();
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::Theme),
        static_cast<int>(scenario->theme)
        );
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::Language),
        static_cast<int>(scenario->language)
        );
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::FontSize),
        static_cast<int>(FontSize::Normal)
        );
    SettingsManager::instance().sync();

    QVERIFY2(m_languageService->setLanguage(scenario->language), qPrintable(
        QStringLiteral("Unable to load translation for %1.")
            .arg(appLanguageName(scenario->language))
        ));

    FontManager::setSizeOffset(0);
    FontManager::applyGlobalFont(
        *qApp,
        m_languageService->loadedLocaleName()
        );

    QTemporaryDir databaseRoot;
    QVERIFY2(databaseRoot.isValid(), "Unable to create an isolated database directory.");

    QString databasePath;
    if (!scenario->fixtureFile.isEmpty())
    {
        const QString sourcePath = QDir(
            QStringLiteral(CLASSMNGR_PHASE0_FIXTURE_DIR)
            ).filePath(scenario->fixtureFile);
        QVERIFY2(QFileInfo(sourcePath).isFile(), qPrintable(
            QStringLiteral("Fixture is missing: %1")
                .arg(sourcePath)
            ));

        databasePath = QDir(databaseRoot.path()).filePath(
            QStringLiteral("capture.tps")
            );
        QVERIFY2(QFile::copy(sourcePath, databasePath), qPrintable(
            QStringLiteral("Unable to copy fixture to: %1")
                .arg(databasePath)
            ));
    }

    const QSet<const QWidget*> baselineTopLevels = topLevelWidgets();
    const QStringList baselineConnectionNames =
        QSqlDatabase::connectionNames();
    const QSet<QString> baselineConnections(
        baselineConnectionNames.cbegin(),
        baselineConnectionNames.cend()
        );
    const int baselineThreadCount =
        QThreadPool::globalInstance()->activeThreadCount();

    for (const QString& packId : resourcePackIds())
    {
        QVERIFY2(!ResourcePackManager::instance().isMounted(packId),
            qPrintable(
                QStringLiteral("Resource pack was already mounted before %1: %2")
                    .arg(scenario->id, packId)
                ));
    }

    auto startupThemeService = std::make_unique<ThemeService>();
    startupThemeService->setTheme(scenario->theme);

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = databasePath;
    startupOptions.startupThemeService = std::move(startupThemeService);

    auto window = std::make_unique<MainWindow>(
        [](const QString&) {},
        false,
        m_languageService.get(),
        nullptr,
        std::move(startupOptions)
        );

    window->resize(scenario->windowSize);
    window->show();
    window->raise();
    window->activateWindow();

    bool exposed = QTest::qWaitForWindowExposed(window.get(), 15000);
    if (!exposed)
    {
        // A first native top-level window can miss the initial expose
        // notification while the desktop creates its frame. Re-showing the
        // same production window gives the platform plugin one clean expose
        // cycle without weakening the evidence requirement.
        window->hide();
        QApplication::processEvents(QEventLoop::AllEvents, 100);
        window->show();
        window->raise();
        window->activateWindow();
        exposed = QTest::qWaitForWindowExposed(window.get(), 15000);
    }
    QVERIFY2(
        exposed,
        "MainWindow was not exposed by the native Windows platform plugin."
        );

    QTest::qWait(SettleMilliseconds);
    QApplication::processEvents(QEventLoop::AllEvents, 100);

    PageManager* pages = window->findChild<PageManager*>(
        QStringLiteral("pagesWidget")
        );
    QVERIFY2(pages, "MainWindow did not expose its production PageManager.");

    QStringList actions{
        QStringLiteral("Construct MainWindow production entry point"),
        QStringLiteral("Show window and wait for native exposure")
    };

    QMenu* openMenu = nullptr;
    switch (scenario->surface)
    {
    case WindowsQtCaptureSurface::RegisteredPage:
        pages->showPage(scenario->pageType);
        actions.append(
            QStringLiteral("Navigate to %1")
                .arg(PageManager::pageTypeIdentifier(scenario->pageType))
            );
        QTRY_COMPARE_WITH_TIMEOUT(
            pages->currentPageIdentifier(),
            PageManager::pageTypeIdentifier(scenario->pageType),
            5000
            );
        break;

    case WindowsQtCaptureSurface::WorkspaceTab:
    {
        pages->showPage(PageType::MyWorkspace);
        auto* workspace = pages->myWorkspacePage();
        QVERIFY(workspace);

        if (scenario->workspaceTab == WorkspaceTab::Calendar)
        {
            QVERIFY(workspace->ensureCalendarPage());
        }

        workspace->openTab(scenario->workspaceTab);
        QVERIFY(workspace->currentTab() == scenario->workspaceTab);
        QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
        actions.append(
            QStringLiteral("Open %1 workspace tab")
                .arg(workspaceTabName(scenario->workspaceTab))
            );
        break;
    }

    case WindowsQtCaptureSurface::Menu:
        openMenu = findMenu(window->menuBar(), scenario->menuTitle);
        QVERIFY2(openMenu, qPrintable(
            QStringLiteral("Unable to find menu: %1")
                .arg(scenario->menuTitle)
            ));
        openMenu->popup(
            window->menuBar()->mapToGlobal(
                QPoint(window->menuBar()->width() / 2, window->menuBar()->height())
                )
            );
        QTRY_VERIFY_WITH_TIMEOUT(openMenu->isVisible(), 3000);
        QVERIFY(!openMenu->actions().isEmpty());
        actions.append(
            QStringLiteral("Open %1 menu")
                .arg(scenario->menuTitle)
            );
        break;
    }

    actions.append(
        QStringLiteral("Allow layout, fonts, and images to settle for %1 ms")
            .arg(SettleMilliseconds)
        );
    QTest::qWait(SettleMilliseconds);
    QApplication::processEvents(QEventLoop::AllEvents, 100);

    QScreen* screen = openMenu && openMenu->screen()
        ? openMenu->screen()
        : window->screen();
    QVERIFY2(screen, "No screen is associated with the captured window.");

    const int actualScalePercent = displayScalePercent(screen);
    QVERIFY2(
        actualScalePercent == 100
            || actualScalePercent == 150
            || actualScalePercent == 200,
        qPrintable(
            QStringLiteral(
                "Unsupported display scale %1%; run on the fixed 100/150/200% matrix."
                ).arg(actualScalePercent)
            )
        );

    const QByteArray expectedScaleValue =
        qgetenv("CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT");
    if (!expectedScaleValue.isEmpty())
    {
        bool ok = false;
        const int expectedScale = expectedScaleValue.toInt(&ok);
        QVERIFY2(ok, "CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT is not an integer.");
        QCOMPARE(actualScalePercent, expectedScale);
    }

    const WId captureWindowId = openMenu
        ? openMenu->winId()
        : window->winId();
    QVERIFY(captureWindowId != 0);

    const QPixmap pixmap = screen->grabWindow(captureWindowId);
    QVERIFY2(!pixmap.isNull(), "Native Windows capture returned an empty pixmap.");

    const QImage image = pixmap.toImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
    QVERIFY(image.height() > 0);

    QSet<QRgb> sampleColors;
    const int xStep = qMax(1, image.width() / 10);
    const int yStep = qMax(1, image.height() / 10);
    for (int y = 0; y < image.height(); y += yStep)
    {
        for (int x = 0; x < image.width(); x += xStep)
        {
            sampleColors.insert(image.pixel(x, y));
        }
    }
    QVERIFY2(sampleColors.size() > 1, "Captured surface is uniformly blank.");

    const QString baseName = QStringLiteral("%1__%2__%3__%4__%5")
        .arg(
            scenario->artifactPrefix,
            scenario->state,
            themeName(scenario->theme),
            QString::number(actualScalePercent),
            appLanguageName(scenario->language)
            );
    const QString imageFileName = baseName + QStringLiteral(".png");
    const QString imagePath = QDir(m_captureRoot).filePath(imageFileName);
    QVERIFY2(!QFileInfo::exists(imagePath), qPrintable(
        QStringLiteral("Refusing to overwrite capture: %1")
            .arg(imagePath)
        ));

    QSaveFile imageFile(imagePath);
    QVERIFY(imageFile.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&imageFile, "PNG"));
    QVERIFY(imageFile.commit());
    actions.append(QStringLiteral("Capture native Windows window pixels"));

    const QSize capturedWindowSize = window->size();
    const QRect capturedWindowBounds = window->frameGeometry();

    if (openMenu)
    {
        openMenu->close();
        QTRY_VERIFY_WITH_TIMEOUT(!openMenu->isVisible(), 3000);
    }

    QVERIFY(window->close());
    window.reset();
    drainDeferredDeletes();
    actions.append(QStringLiteral("Close and drain deferred destruction"));

    QTRY_COMPARE_WITH_TIMEOUT(
        QThreadPool::globalInstance()->activeThreadCount(),
        baselineThreadCount,
        5000
        );

    QVERIFY2(
        topLevelWidgets() == baselineTopLevels,
        qPrintable(
            QStringLiteral("Top-level widget leak after %1: %2")
                .arg(scenario->id, topLevelWidgetNames(baselineTopLevels))
            )
        );
    QVERIFY2(
        QApplication::activeModalWidget() == nullptr,
        "A modal widget remained after the capture lifecycle."
        );

    const QStringList remainingConnectionNames =
        QSqlDatabase::connectionNames();
    const QSet<QString> remainingConnections(
        remainingConnectionNames.cbegin(),
        remainingConnectionNames.cend()
        );
    QCOMPARE(remainingConnections, baselineConnections);

    for (const QString& packId : resourcePackIds())
    {
        QVERIFY2(!ResourcePackManager::instance().isMounted(packId),
            qPrintable(
                QStringLiteral("Resource pack remained mounted after %1: %2")
                    .arg(scenario->id, packId)
                ));
    }

    if (!databasePath.isEmpty())
    {
        QVERIFY2(
            !QFileInfo::exists(databasePath) || QFile::remove(databasePath),
            qPrintable(
                QStringLiteral("Database fixture remained locked: %1")
                    .arg(databasePath)
                )
            );
    }

    const QDir databaseDirectory(databaseRoot.path());
    QCOMPARE(
        databaseDirectory.entryList(
            QDir::NoDotAndDotDot | QDir::AllEntries,
            QDir::Name
            ),
        QStringList{}
        );

    writeCaptureMetadata(
        *scenario,
        imagePath,
        imageFileName,
        actualScalePercent,
        capturedWindowSize,
        capturedWindowBounds,
        actions
        );
}

void WindowsQtVisualCaptureTests::writeCaptureMetadata(
    const WindowsQtCaptureScenario& scenario,
    const QString& imagePath,
    const QString& imageFileName,
    int actualScalePercent,
    const QSize& actualWindowSize,
    const QRect& windowBounds,
    const QStringList& actions
    )
{
    const QString metadataPath =
        QDir(m_captureRoot).filePath(
            QFileInfo(imageFileName).completeBaseName()
                + QStringLiteral(".json")
            );
    QVERIFY2(!QFileInfo::exists(metadataPath), qPrintable(
        QStringLiteral("Refusing to overwrite metadata: %1")
            .arg(metadataPath)
        ));

    QJsonObject metadata;
    metadata.insert(
        QStringLiteral("format"),
        QStringLiteral("classmngr-phase0-capture-v1")
        );
    metadata.insert(QStringLiteral("ledgerId"), scenario.ledgerId);
    metadata.insert(
        QStringLiteral("sourceRevision"),
        QString::fromLatin1(BuildInfo::GitRevision)
        );
    metadata.insert(QStringLiteral("fixtureId"), scenario.fixtureId);
    metadata.insert(QStringLiteral("architecture"), architectureName());

    QJsonObject windows;
    windows.insert(QStringLiteral("edition"), QSysInfo::prettyProductName());
    windows.insert(QStringLiteral("build"), QSysInfo::kernelVersion());
    metadata.insert(QStringLiteral("windows"), windows);

    metadata.insert(
        QStringLiteral("displayScalePercent"),
        actualScalePercent
        );
    metadata.insert(QStringLiteral("theme"), themeName(scenario.theme));
    metadata.insert(
        QStringLiteral("appLanguage"),
        appLanguageName(scenario.language)
        );
    metadata.insert(
        QStringLiteral("inputLanguage"),
        inputLanguageName(scenario.language)
        );
    metadata.insert(QStringLiteral("fontSize"), QStringLiteral("normal"));

    QJsonObject window;
    window.insert(QStringLiteral("width"), actualWindowSize.width());
    window.insert(QStringLiteral("height"), actualWindowSize.height());
    window.insert(QStringLiteral("x"), windowBounds.x());
    window.insert(QStringLiteral("y"), windowBounds.y());
    metadata.insert(QStringLiteral("window"), window);

    QJsonArray actionArray;
    for (const QString& action : actions)
    {
        actionArray.append(action);
    }
    metadata.insert(QStringLiteral("actions"), actionArray);

    QJsonObject artifact;
    artifact.insert(QStringLiteral("file"), imageFileName);
    const QString imageHash = sha256ForFile(imagePath);
    QVERIFY2(!imageHash.isEmpty(), "Unable to hash the captured PNG.");
    artifact.insert(QStringLiteral("sha256"), imageHash);
    metadata.insert(QStringLiteral("artifact"), artifact);

    QJsonObject observations;
    observations.insert(
        QStringLiteral("keyboard"),
        QStringLiteral("Not exercised by this automated visual capture.")
        );
    observations.insert(
        QStringLiteral("inputMethod"),
        QStringLiteral("Manual Korean IME evidence remains pending.")
        );
    observations.insert(
        QStringLiteral("accessibility"),
        QStringLiteral("Manual UI Automation and screen-reader review remains pending.")
        );
    observations.insert(
        QStringLiteral("notes"),
        QStringLiteral(
            "Production-entry-point capture; review the PNG before promoting it to a golden."
            )
        );
    metadata.insert(QStringLiteral("observations"), observations);
    metadata.insert(QStringLiteral("verification"), QStringLiteral("captured"));

    QSaveFile metadataFile(metadataPath);
    QVERIFY(metadataFile.open(QIODevice::WriteOnly));
    QVERIFY(metadataFile.write(
        QJsonDocument(metadata).toJson(QJsonDocument::Indented)
        ) > 0);
    QVERIFY(metadataFile.commit());
}

void WindowsQtVisualCaptureTests::cleanupTestCase()
{
    if (m_languageService)
    {
        m_languageService->setLanguage(Language::SystemDefault);
        m_languageService.reset();
    }

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();

    if (m_previousSettingsRootWasSet)
    {
        qputenv("CLASSMNGR_SETTINGS_ROOT", m_previousSettingsRoot);
    }
    else
    {
        qunsetenv("CLASSMNGR_SETTINGS_ROOT");
    }

    if (m_previousAppDataWasSet)
    {
        qputenv("APPDATA", m_previousAppData);
    }
    else
    {
        qunsetenv("APPDATA");
    }

    if (m_previousLocalAppDataWasSet)
    {
        qputenv("LOCALAPPDATA", m_previousLocalAppData);
    }
    else
    {
        qunsetenv("LOCALAPPDATA");
    }

    QStandardPaths::setTestModeEnabled(false);
}

QTEST_MAIN(WindowsQtVisualCaptureTests)

#include "windows_qt_visual_capture_tests.moc"
