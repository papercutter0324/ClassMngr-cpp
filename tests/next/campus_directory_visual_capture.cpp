#include "core/process_memory_snapshot.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "next/application/campus_dashboard_selected_campus_read_query.h"
#include "next/application/campus_directory_read_query.h"
#include "next/platform/campus_directory_repository_adapter.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/campus_directory_route_factories.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPixmap>
#include <QResource>
#include <QTemporaryDir>
#include <QTranslator>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};

QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}

Domain::CampusId campusId()
{
    return *Domain::CampusId::fromString("bundang");
}

QString routeId(const QString& section)
{
    if (section == QStringLiteral("information")) return QStringLiteral("campus_information");
    if (section == QStringLiteral("directions")) return QStringLiteral("campus_directions");
    if (section == QStringLiteral("address")) return QStringLiteral("campus_address");
    if (section == QStringLiteral("housing")) return QStringLiteral("campus_housing");
    return QStringLiteral("campus_map");
}

void selectLegacySection(CampusDashboardPage& page, const QString& section)
{
    if (section == QStringLiteral("directions")) page.showDirections();
    else if (section == QStringLiteral("address")) page.showAddress();
    else if (section == QStringLiteral("housing")) page.showHousing();
    else if (section == QStringLiteral("maps")) page.showMap();
    else page.showInformation();
}

QJsonObject memorySnapshotJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("workingSetBytes"), static_cast<double>(snapshot.workingSetBytes)},
        {QStringLiteral("privateUsageBytes"), static_cast<double>(snapshot.privateUsageBytes)},
        {QStringLiteral("peakWorkingSetBytes"), static_cast<double>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("capturedAt"), snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

struct MapState final
{
    int count = 0;
    QJsonArray decodedSizes;
};

MapState mapState(const Ui::CampusDirectoryPage* page)
{
    MapState result;
    if (!page) return result;
    const auto labels = page->findChildren<QLabel*>(QStringLiteral("campusMapImage"));
    for (const QLabel* label : labels)
    {
        const QPixmap pixmap = label->pixmap(Qt::ReturnByValue);
        if (pixmap.isNull()) continue;
        ++result.count;
        result.decodedSizes.append(QJsonObject{
            {QStringLiteral("width"), pixmap.width()},
            {QStringLiteral("height"), pixmap.height()}
        });
    }
    return result;
}

QJsonObject memoryPoint(
    const ProcessMemorySnapshot& snapshot,
    const MapState& maps,
    int loadEvents
    )
{
    return {
        {QStringLiteral("processMemory"), memorySnapshotJson(snapshot)},
        {QStringLiteral("loadedMapResourceCount"), maps.count},
        {QStringLiteral("decodedMapDimensions"), maps.decodedSizes},
        {QStringLiteral("campusPngLoadEvents"), loadEvents}
    };
}

bool saveWidget(QWidget& widget, const QString& path)
{
    widget.resize(Viewport);
    widget.show();
    QApplication::processEvents();
    if (QWidget* focused = QApplication::focusWidget()) focused->clearFocus();
    QApplication::processEvents();
    const QImage image = widget.grab().toImage();
    return image.size() == Viewport && image.save(path, "PNG");
}

bool captureLegacy(
    const QString& section,
    const QString& path,
    QString* error
    )
{
    CampusDashboardPage page(false, CampusDashboardPageDependencies{
        .campusDirectory = QStringLiteral(":/resource-packs/campuses")
    });
    page.refresh();
    const QList<QComboBox*> campusSelectors = page.findChildren<QComboBox*>();
    if (campusSelectors.isEmpty())
    {
        if (error) *error = QStringLiteral("Legacy campus selector was not created.");
        return false;
    }
    const int bundangIndex = campusSelectors.front()->findData(QStringLiteral("bundang"));
    if (bundangIndex < 0)
    {
        if (error) *error = QStringLiteral("Bundang was not found in the mounted Legacy catalog.");
        return false;
    }
    campusSelectors.front()->setCurrentIndex(bundangIndex);
    selectLegacySection(page, section);
    return saveWidget(page, path);
}

bool captureNext(
    Resources::ResourceLoader& loader,
    std::vector<Resources::ResourceDiagnostic>& diagnostics,
    const QString& section,
    const QString& path,
    QJsonObject* memoryMeasurement,
    QString* error
    )
{
    QTemporaryDir temporaryOverrides;
    if (!temporaryOverrides.isValid())
    {
        if (error) *error = QStringLiteral("Could not create isolated read-only capture storage.");
        return false;
    }
    Platform::CampusDirectoryRepositoryAdapter repository(loader, temporaryOverrides.path());
    Test::MemoryPreferences preferences;
    Ui::PageHost host(loader, preferences.model);

    const auto descriptors = Ui::campusDirectoryPageDescriptors([&repository] {
        Ui::CampusDirectoryPageActions actions;
        actions.editCapability = Application::CampusEditCapability::readOnly();
        actions.loadCampuses = [&repository] {
            return Application::CampusDirectoryReadQuery::execute(repository);
        };
        actions.loadCampus = [&repository](const Domain::CampusId& id) {
            return Application::CampusDashboardSelectedCampusReadQuery::execute(
                Application::CampusDashboardSelectedCampusReadRequest{id}, repository);
        };
        actions.saveCampus = [](const Application::CampusDashboardCampusSnapshot&,
                                Application::CampusDashboardCampusSaveIntent) {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Visual capture is read-only.",
                .recoverable = false
            });
        };
        actions.readLastSelectedCampus = [] { return std::optional<Domain::CampusId>{campusId()}; };
        actions.saveLastSelectedCampus = [](const std::optional<Domain::CampusId>&) {
            return Domain::Result<void>::success();
        };
        actions.importLegacyCampuses = [] { return Domain::Result<std::size_t>::success(0); };
        actions.mapResourcesFor = [&repository](
                                      const Application::CampusDashboardCampusSnapshot& campus) {
            return repository.mapResourcesFor(campus);
        };
        return actions;
    });
    for (const Ui::PageDescriptor& descriptor : descriptors)
    {
        if (!host.registerPage(descriptor))
        {
            if (error) *error = QStringLiteral("Could not register Next campus route.");
            return false;
        }
    }

    const QString informationId = QStringLiteral("campus_information");
    const QString mapsId = QStringLiteral("campus_map");
    if (!host.activatePage(informationId))
    {
        if (error) *error = QStringLiteral("Could not activate the Next information route.");
        return false;
    }

    host.resize(Viewport);
    host.show();
    QApplication::processEvents();

    Ui::CampusDirectoryPage* page = nullptr;
    if (section == QStringLiteral("maps"))
    {
        PlatformProcessMemorySnapshotProvider memoryProvider;
        const auto campusPngLoads = [&diagnostics] {
            return static_cast<int>(std::count_if(
                diagnostics.cbegin(), diagnostics.cend(),
                [](const Resources::ResourceDiagnostic& item) {
                    return item.succeeded
                        && item.resourceId.value().endsWith(QStringLiteral(".png"))
                        && item.resourceId.value().contains(QStringLiteral("/campuses/"));
                }));
        };
        const auto baseline = memoryProvider.snapshot();
        const int baselineLoads = campusPngLoads();
        if (!host.activatePage(mapsId))
        {
            if (error) *error = QStringLiteral("Could not activate the Next Maps route.");
            return false;
        }
        page = dynamic_cast<Ui::CampusDirectoryPage*>(host.pageInstance(mapsId));
        if (!page)
        {
            if (error) *error = QStringLiteral("Next Maps page instance is missing.");
            return false;
        }
        QApplication::processEvents();
        const auto entered = memoryProvider.snapshot();
        const MapState enteredMaps = mapState(page);
        const int enteredLoads = campusPngLoads();
        if (!entered.isAvailable || enteredMaps.count != 2 || enteredLoads < 2)
        {
            if (error) *error = QStringLiteral("Next Maps memory measurement did not load both packaged images.");
            return false;
        }
        for (const QJsonValue& dimension : enteredMaps.decodedSizes)
        {
            const QJsonObject size = dimension.toObject();
            if (size.value(QStringLiteral("width")).toInt() > 1520
                || size.value(QStringLiteral("height")).toInt() > 1520)
            {
                if (error) *error = QStringLiteral("A decoded Next map image exceeded the 1520-pixel bound.");
                return false;
            }
        }
        if (!host.activatePage(informationId))
        {
            if (error) *error = QStringLiteral("Could not leave Next Maps for lifecycle measurement.");
            return false;
        }
        if (!host.releasePage(mapsId) || host.hasPageInstance(mapsId))
        {
            if (error) *error = QStringLiteral("Next Maps page did not release after suspension.");
            return false;
        }
        const auto suspended = memoryProvider.snapshot();
        const MapState suspendedMaps;
        const int suspendedLoads = campusPngLoads();
        if (suspendedMaps.count != 0)
        {
            if (error) *error = QStringLiteral("Next Maps retained map images after suspension.");
            return false;
        }
        if (!host.activatePage(mapsId))
        {
            if (error) *error = QStringLiteral("Could not reopen Next Maps for lifecycle measurement.");
            return false;
        }
        page = dynamic_cast<Ui::CampusDirectoryPage*>(host.pageInstance(mapsId));
        if (!page)
        {
            if (error) *error = QStringLiteral("Next Maps page was not recreated after release.");
            return false;
        }
        const auto reopened = memoryProvider.snapshot();
        const MapState reopenedMaps = mapState(page);
        const int reopenedLoads = campusPngLoads();
        if (reopenedMaps.count != 2 || reopenedLoads < enteredLoads + 2)
        {
            if (error) *error = QStringLiteral("Next Maps did not reload both images after reopening.");
            return false;
        }
        if (memoryMeasurement && memoryMeasurement->isEmpty())
        {
            *memoryMeasurement = {
                {QStringLiteral("metricSource"), QStringLiteral("PlatformProcessMemorySnapshotProvider (whole-process resident working set and private usage)")},
                {QStringLiteral("platform"), baseline.platform},
                {QStringLiteral("configuration"), QJsonObject{
                     {QStringLiteral("qt"), QStringLiteral(QT_VERSION_STR)},
                     {QStringLiteral("qtPlatformPlugin"), QStringLiteral("offscreen")},
                     {QStringLiteral("viewportWidth"), Viewport.width()},
                     {QStringLiteral("viewportHeight"), Viewport.height()},
                     {QStringLiteral("locale"), QStringLiteral("en_US")},
                     {QStringLiteral("theme"), QStringLiteral("light")},
                     {QStringLiteral("catalog"), QStringLiteral("Bundang; packaged read-only JSON and PNG resources")}
                 }},
                {QStringLiteral("observations"), QJsonObject{
                     {QStringLiteral("beforeMapEntry"), memoryPoint(baseline, MapState{}, baselineLoads)},
                     {QStringLiteral("afterMapEntry"), memoryPoint(entered, enteredMaps, enteredLoads)},
                     {QStringLiteral("afterLeaveSuspensionRelease"), memoryPoint(suspended, suspendedMaps, suspendedLoads)},
                     {QStringLiteral("afterReopen"), memoryPoint(reopened, reopenedMaps, reopenedLoads)}
                 }},
                {QStringLiteral("scope"), QStringLiteral("Whole-process observations; not page-attributed memory and not a memory budget.")}
            };
        }
    }
    else
    {
        if (!host.activatePage(routeId(section)))
        {
            if (error) *error = QStringLiteral("Could not activate the requested Next campus route.");
            return false;
        }
        page = dynamic_cast<Ui::CampusDirectoryPage*>(host.pageInstance(routeId(section)));
    }

    if (!page)
    {
        if (error) *error = QStringLiteral("Next campus page instance is missing.");
        return false;
    }
    if (!saveWidget(*page, path))
    {
        if (error) *error = QStringLiteral("Could not save the Next campus screenshot.");
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextCampusDirectoryVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create campus evidence directory");
        return 2;
    }

    const QString resourceRoot = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString catalogPath = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
    const QString campusPack = QString::fromUtf8(CLASSMNGR_CAMPUS_DIRECTORY_LEGACY_PACK);
    const auto catalog = Resources::ResourceCatalog::load(catalogPath, resourceRoot);
    if (!catalog)
    {
        qCritical().noquote() << "Could not load Next resource catalog:" << catalog.error().message;
        return 1;
    }
    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const Resources::ResourceDiagnostic& item) {
            diagnostics.push_back(item);
        }
        );

    if (!QResource::registerResource(campusPack))
    {
        qCritical().noquote() << "Could not mount Legacy campus resource pack:" << campusPack;
        return 1;
    }

    for (const QString& font : {
             QStringLiteral("Inter.ttc"),
             QStringLiteral("PretendardVariable.ttf")
         })
    {
        const QString fontPath = QDir(resourceRoot).filePath(QStringLiteral("assets/fonts/") + font);
        if (QFile::exists(fontPath)) static_cast<void>(QFontDatabase::addApplicationFont(fontPath));
    }

    const QStringList sections{
        QStringLiteral("information"), QStringLiteral("directions"),
        QStringLiteral("address"), QStringLiteral("housing"), QStringLiteral("maps")
    };
    const QStringList locales{QStringLiteral("en_US"), QStringLiteral("ko_KR")};
    const QStringList themes{QStringLiteral("light"), QStringLiteral("dark")};
    QJsonArray captures;
    QJsonObject memoryMeasurement;

    for (const QString& locale : locales)
    {
        std::unique_ptr<QTranslator> translator;
        if (locale == QStringLiteral("ko_KR"))
        {
            translator = std::make_unique<QTranslator>();
            const QString translationPath = QDir(QString::fromUtf8(
                CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)).filePath(QStringLiteral("ClassMngr_ko_KR.qm"));
            if (!translator->load(translationPath)
                || !application.installTranslator(translator.get()))
            {
                qCritical().noquote() << "Could not load Korean translation:" << translationPath;
                return 1;
            }
        }

        for (const QString& theme : themes)
        {
            const QString stylesheet = readText(QDir(resourceRoot).filePath(
                QStringLiteral("assets/styles/%1.qss").arg(theme)));
            if (stylesheet.isEmpty())
            {
                qCritical().noquote() << "Could not load theme:" << theme;
                return 1;
            }
            application.setStyleSheet(stylesheet);
            for (const QString& section : sections)
            {
                for (const QString& implementation : {QStringLiteral("legacy"), QStringLiteral("next")})
                {
                    const QString fileName = QStringLiteral("campus_%1_%2_%3_%4.png")
                        .arg(section, locale, theme, implementation);
                    const QString path = QDir(outputDirectory).filePath(fileName);
                    QString error;
                    const bool saved = implementation == QStringLiteral("legacy")
                        ? captureLegacy(section, path, &error)
                        : captureNext(loader, diagnostics, section, path,
                                      locale == QStringLiteral("en_US")
                                          && theme == QStringLiteral("light")
                                          && section == QStringLiteral("maps")
                                          ? &memoryMeasurement : nullptr,
                                      &error);
                    if (!saved)
                    {
                        qCritical().noquote() << "Capture failed:" << fileName << error;
                        return 1;
                    }
                    captures.append(QJsonObject{
                        {QStringLiteral("section"), section},
                        {QStringLiteral("locale"), locale},
                        {QStringLiteral("theme"), theme},
                        {QStringLiteral("implementation"), implementation},
                        {QStringLiteral("file"), fileName},
                        {QStringLiteral("width"), Viewport.width()},
                        {QStringLiteral("height"), Viewport.height()},
                        {QStringLiteral("campusId"), QStringLiteral("bundang")}
                    });
                }
            }
        }
        if (translator) application.removeTranslator(translator.get());
    }

    if (memoryMeasurement.isEmpty())
    {
        qCritical("Campus map lifecycle memory measurements were not produced");
        return 1;
    }
    QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral("Paired Legacy and Next Campus Directory page captures")},
        {QStringLiteral("viewport"), QJsonObject{
             {QStringLiteral("width"), Viewport.width()},
             {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("qtPlatformPlugin"), QStringLiteral("offscreen")},
        {QStringLiteral("style"), QStringLiteral("Fusion with packaged light/dark stylesheet")},
        {QStringLiteral("dataSource"), QStringLiteral("The same packaged Bundang campus JSON and map PNGs; Legacy uses the read-only campuses RCC and Next uses its packaged resource catalog.")},
        {QStringLiteral("pairedCaptureLimitations"), QStringLiteral("The Legacy dashboard keeps all campus tabs in one page while Next provides one route per section; paired images use the same campus, locale, theme, and viewport but do not establish pixel parity. Legacy JSON keeps historical :/assets/campuses map references while the current campus RCC uses :/resource-packs/campuses, so this capture target supplies a read-only qrc alias for the same Bundang map PNG bytes to make both map previews visible. The alias exists only in this capture target.")},
        {QStringLiteral("memoryMeasurement"), memoryMeasurement},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("campus_directory_paired_capture.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write campus evidence manifest");
        return 1;
    }
    static_cast<void>(QResource::unregisterResource(campusPack));
    qInfo().noquote() << "Captured paired Campus Directory evidence in" << outputDirectory;
    return 0;
}
