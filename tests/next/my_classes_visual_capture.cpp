#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/my_info/ui/my_classes_page.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "next/application/my_classes_page_read_query.h"
#include "next/platform/workspace_store_my_classes_page_read_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/my_classes_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QPalette>
#include <QTemporaryDir>
#include <QTranslator>
#include <QListView>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};

QString utf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString readText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll())
        : QString{};
}

QPalette themePalette(const QString& theme)
{
    QPalette palette;
    const bool dark = theme == QStringLiteral("dark");
    palette.setColor(QPalette::Button,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#fcfcfc")));
    palette.setColor(QPalette::ButtonText,
        QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Mid,
        QColor(dark ? QStringLiteral("#4b5056") : QStringLiteral("#b0b5bb")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    return palette;
}

int widgetCount(const QWidget* root)
{
    return root ? root->findChildren<QWidget*>().size() + 1 : 0;
}

int classShellCount(const QWidget* root)
{
    if (!root)
    {
        return 0;
    }
    int count = 0;
    for (QWidget* const widget : root->findChildren<QWidget*>())
    {
        if (widget->property("class_id").isValid())
        {
            ++count;
        }
    }
    return count;
}

QJsonObject memoryJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("workingSetBytes"),
         static_cast<double>(snapshot.workingSetBytes)},
        {QStringLiteral("peakWorkingSetBytes"),
         static_cast<double>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"),
         static_cast<double>(snapshot.privateUsageBytes)},
        {QStringLiteral("privateWorkingSetBytes"),
         static_cast<double>(snapshot.privateWorkingSetBytes)},
        {QStringLiteral("capturedAt"),
         snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

bool copyFixture(const QString& destination, QString* error)
{
    QFile::remove(destination);
    if (QFile::copy(QStringLiteral(CLASSMNGR_NEXT_MY_CLASSES_LARGE_FIXTURE),
                    destination))
    {
        return true;
    }
    if (error)
    {
        *error = QStringLiteral("The tracked 96-class fixture could not be copied.");
    }
    return false;
}

bool captureLegacy(
    const QString& databasePath,
    const QString& outputPath,
    QJsonObject* state,
    QString* error
    )
{
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    ApplicationServices services;
    const RepositoryStatus opened = services.openDatabase(databasePath);
    if (!opened)
    {
        if (error)
        {
            *error = toLegacyDiagnostic(opened.error());
        }
        return false;
    }

    MyClassesPage page(&services);
    page.resize(Viewport);
    page.refresh();
    page.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();

    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const QImage image = page.grab().toImage();
    if (image.size() != Viewport || !image.save(outputPath, "PNG"))
    {
        if (error)
        {
            *error = QStringLiteral("The legacy My Classes capture could not be saved.");
        }
        return false;
    }
    const auto* const tabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("myInfoClassTabs"));
    const auto* const gradeTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("myInfoGradeTabs"));
    const MyClassesPageRuntimeMetrics metrics = page.runtimeMetrics();
    const int classShells = classShellCount(&page);
    const int widgets = widgetCount(&page);
    page.releaseFeatureResources();
    QApplication::processEvents();
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    *state = {
        {QStringLiteral("page"), QStringLiteral("legacy")},
        {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("width"), image.width()},
        {QStringLiteral("height"), image.height()},
        {QStringLiteral("summaryListReadCount"), metrics.classSummaryListQueryCount},
        {QStringLiteral("gradeTabCount"), gradeTabs ? gradeTabs->count() : 0},
        {QStringLiteral("visibleClassTabCount"), tabs ? tabs->count() : 0},
        {QStringLiteral("classWidgetShellCount"), classShells},
        {QStringLiteral("widgetCountWhileActive"), widgets},
        {QStringLiteral("widgetCountAfterFeatureRelease"), widgetCount(&page)},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("suspended"), memoryJson(suspended)}
        }}
    };
    return true;
}

bool captureNext(
    const QString& databasePath,
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& outputPath,
    QJsonObject* state,
    QString* error
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        if (error)
        {
            *error = toLegacyDiagnostic(opened.error());
        }
        return false;
    }

    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    Ui::PageHost host(loader, preferences);
    Ui::PageDescriptor myClasses{
        .id = *Ui::PageId::fromString(QStringLiteral("my_classes")),
        .label = QStringLiteral("My Classes"),
        .factory = [&store](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            Ui::MyClassesPageActions actions;
            actions.workspaceIdentity = [] { return QStringLiteral("large-fixture"); };
            actions.readSummaries = [&store] {
                Platform::WorkspaceStoreMyClassesPageReadPort port(store);
                return Application::MyClassesPageReadQuery(port).readSummaries();
            };
            actions.readSelectedDetails = [&store](const Domain::ClassId& id) {
                Platform::WorkspaceStoreMyClassesPageReadPort port(store);
                return Application::MyClassesPageReadQuery(port)
                    .readSelectedDetails(id);
            };
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::MyClassesPage>(context, std::move(actions)));
        }
    };
    Ui::PageDescriptor other{
        .id = *Ui::PageId::fromString(QStringLiteral("other")),
        .label = QStringLiteral("Other"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(std::make_unique<Ui::Page>(context));
        }
    };
    if (!host.registerPage(std::move(myClasses))
        || !host.registerPage(std::move(other))
        || !host.activatePage(QStringLiteral("my_classes")))
    {
        if (error)
        {
            *error = QStringLiteral("The Next My Classes page could not activate.");
        }
        return false;
    }
    auto* const page = dynamic_cast<Ui::MyClassesPage*>(
        host.pageInstance(QStringLiteral("my_classes")));
    if (!page)
    {
        if (error)
        {
            *error = QStringLiteral("The Next page factory returned the wrong page.");
        }
        return false;
    }

    host.resize(Viewport);
    host.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const QImage image = host.grab().toImage();
    if (image.size() != Viewport || !image.save(outputPath, "PNG"))
    {
        if (error)
        {
            *error = QStringLiteral("The Next My Classes capture could not be saved.");
        }
        return false;
    }

    const int modelCount = page->summaryModel()
        && page->classList()->model() == page->summaryModel() ? 1 : 0;
    const int classListViewCount = page->findChildren<QListView*>(
        QStringLiteral("myClassesSummaryList")).size();
    const int gradeTabCount = page->gradeTabs()->count();
    const int activeGradeSummaryCount = page->classList()->model()->rowCount(
        page->classList()->rootIndex());
    const int classCardCount = page->findChildren<QFrame*>(
        QStringLiteral("myClassesClassCard")).size();
    const int summaryCount = page->summaryModel()->summaryCount();
    const int detailReadClassCount = store.classRepository()->readMetrics()
        .getClassByIdCallCount;
    const int detailReadInfoClassCount = store.classInfoRepository()
        ->myClassesClassInformationBatchReadMetrics().requestedClassCount;
    const int detailReadRosterClassCount = store.rosterRepository()
        ->myClassesStudentCountBatchReadMetrics().requestedClassCount;
    const int widgets = widgetCount(page);

    static_cast<void>(host.activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    const bool released = page->detailsReleased()
        && page->summaryModel()->summaryCount() == 0;
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    *state = {
        {QStringLiteral("page"), QStringLiteral("next")},
        {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("width"), image.width()},
        {QStringLiteral("height"), image.height()},
        {QStringLiteral("summaryModelCount"), modelCount},
        {QStringLiteral("summaryRowCountWhileActive"), summaryCount},
        {QStringLiteral("gradeTabCount"), gradeTabCount},
        {QStringLiteral("classListViewCount"), classListViewCount},
        {QStringLiteral("activeGradeSummaryCount"), activeGradeSummaryCount},
        {QStringLiteral("selectedDetailPanelCount"), classCardCount},
        {QStringLiteral("selectedDetailReadClassCount"), detailReadClassCount},
        {QStringLiteral("selectedDetailInfoReadClassCount"), detailReadInfoClassCount},
        {QStringLiteral("selectedDetailRosterCountReadClassCount"), detailReadRosterClassCount},
        {QStringLiteral("widgetCountWhileActive"), widgets},
        {QStringLiteral("releasedAfterSuspension"), released},
        {QStringLiteral("summaryRowCountAfterSuspension"),
         page->summaryModel()->summaryCount()},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("suspended"), memoryJson(suspended)}
        }}
    };
    return released;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextMyClassesVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create capture output directory");
        return 2;
    }

    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const auto loadedCatalog = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG), resourceRoot);
    if (!loadedCatalog)
    {
        qCritical().noquote() << "Could not load Next resource catalog:"
                              << loadedCatalog.error().message;
        return 1;
    }
    Resources::ResourceLoader loader(std::move(*loadedCatalog));
    Test::MemoryPreferences preferences;
    const QString translations = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    const QString styles = QDir(resourceRoot).filePath(QStringLiteral("assets/styles"));
    const QStringList locales{QStringLiteral("en_US"), QStringLiteral("ko_KR")};
    const QStringList themes{QStringLiteral("light"), QStringLiteral("dark")};
    QJsonArray captures;

    for (const QString& locale : locales)
    {
        for (const QString& theme : themes)
        {
            QLocale::setDefault(QLocale(locale));
            application.setPalette(themePalette(theme));
            application.setStyleSheet(readText(
                QDir(styles).filePath(theme + QStringLiteral(".qss"))));
            std::unique_ptr<QTranslator> translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                translator = std::make_unique<QTranslator>();
                if (!translator->load(QDir(translations).filePath(
                        QStringLiteral("ClassMngr_ko_KR.qm")))
                    || !application.installTranslator(translator.get()))
                {
                    qCritical("Could not load the compiled Korean translation");
                    return 1;
                }
            }

            QTemporaryDir temporary;
            if (!temporary.isValid())
            {
                qCritical("Could not create a temporary fixture directory");
                return 1;
            }
            QString error;
            const QString legacyDb = temporary.filePath(QStringLiteral("legacy.tps"));
            const QString nextDb = temporary.filePath(QStringLiteral("next.tps"));
            if (!copyFixture(legacyDb, &error) || !copyFixture(nextDb, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            const QString key = locale + QLatin1Char('_') + theme;
            QJsonObject legacyState;
            QJsonObject nextState;
            const QString legacyImage = QDir(outputDirectory).filePath(
                QStringLiteral("legacy_%1.png").arg(key));
            const QString nextImage = QDir(outputDirectory).filePath(
                QStringLiteral("next_%1.png").arg(key));
            if (!captureLegacy(legacyDb, legacyImage, &legacyState, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            if (!captureNext(nextDb, loader, preferences.model, nextImage,
                             &nextState, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            captures.append(QJsonObject{
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("legacy"), legacyState},
                {QStringLiteral("next"), nextState}
            });
            if (translator)
            {
                application.removeTranslator(translator.get());
            }
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral(
            "Matched legacy and Next My Classes page captures")},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("style"), QStringLiteral("Fusion")},
        {QStringLiteral("platform"), QStringLiteral("offscreen")},
        {QStringLiteral("fixture"), QStringLiteral(
            "tracked 96-class large-classes visual fixture")},
        {QStringLiteral("memoryScope"), QStringLiteral(
            "Process-wide samples; not per-page allocation measurements")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qCritical("Could not write the capture manifest");
        return 1;
    }
    manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    return 0;
}
