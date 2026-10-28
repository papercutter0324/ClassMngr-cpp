#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/classes/ui/classes_page.h"
#include "next/platform/workspace_store_class_editor_actions.h"
#include "next/platform/workspace_store_classes_navigator_read_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/classes_navigator_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPalette>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTranslator>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};

QPalette themePalette(const QString& theme)
{
    QPalette palette;
    const bool dark = theme == QStringLiteral("dark");
    palette.setColor(QPalette::Window,
        QColor(dark ? QStringLiteral("#202327") : QStringLiteral("#f5f6f7")));
    palette.setColor(QPalette::WindowText,
        QColor(dark ? QStringLiteral("#f5f6f7") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Base,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Text,
        QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
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

int classScopedWidgetCount(const QWidget* root)
{
    if (!root)
    {
        return 0;
    }
    const auto widgets = root->findChildren<QWidget*>();
    return static_cast<int>(std::count_if(
        widgets.cbegin(),
        widgets.cend(),
        [](const QWidget* widget) {
            return widget->property("class_id").isValid();
        }));
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

bool saveImage(QWidget& widget, const QString& path, QString* error)
{
    const QImage image = widget.grab().toImage();
    if (image.size() == Viewport && image.save(path, "PNG"))
    {
        return true;
    }
    if (error)
    {
        *error = QStringLiteral("A class-details capture could not be saved: %1")
            .arg(path);
    }
    return false;
}

bool captureLegacy(
    const QString& databasePath,
    const int classId,
    const ClassesSection section,
    const QString& outputPath,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    QJsonObject* state,
    QString* error
    )
{
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

    ClassesPage page(&services);
    page.resize(Viewport);
    if (!page.openClass(classId, section))
    {
        if (error)
        {
            *error = QStringLiteral("The legacy Classes route could not open the requested section.");
        }
        return false;
    }
    page.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    if (!saveImage(page, outputPath, error))
    {
        return false;
    }

    const ClassesPageRuntimeMetrics metrics = page.runtimeMetrics();
    const int widgets = widgetCount(&page);
    const int classWidgets = classScopedWidgetCount(&page);
    page.releaseFeatureResources();
    QApplication::processEvents();
    services.closeDatabase();
    const ProcessMemorySnapshot released = memoryProvider.snapshot();
    *state = {
        {QStringLiteral("route"), QStringLiteral("legacy ClassesPage")},
        {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("width"), Viewport.width()},
        {QStringLiteral("height"), Viewport.height()},
        {QStringLiteral("classId"), classId},
        {QStringLiteral("section"), section == ClassesSection::Details
            ? QStringLiteral("Details and Times") : QStringLiteral("Co-Teacher")},
        {QStringLiteral("fixtureClassCount"), metrics.sourceClassCount},
        {QStringLiteral("widgetCountWhileActive"), widgets},
        {QStringLiteral("classScopedWidgetCount"), classWidgets},
        {QStringLiteral("instantiatedEditorCount"), metrics.instantiatedEditorCount},
        {QStringLiteral("loadedEditorClassCount"), metrics.loadedEditorClassCount},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("afterReleaseAndClose"), memoryJson(released)}
        }}
    };
    return true;
}

bool captureNext(
    const QString& databasePath,
    const Domain::ClassId& selectedClassId,
    const bool coTeacher,
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& outputPath,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
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

    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    std::optional<Domain::ClassId> activeSelection = selectedClassId;
    Ui::PageHost host(loader, preferences);
    Ui::PageDescriptor classes{
        .id = *Ui::PageId::fromString(QStringLiteral("classes")),
        .label = QStringLiteral("Classes"),
        .factory = [&databasePath, &activeSelection](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            Ui::ClassesNavigatorPageActions actions;
            actions.workspaceIdentity = [&databasePath] { return databasePath; };
            actions.activeWorkspacePath = [&databasePath] {
                return std::optional<QString>(databasePath);
            };
            actions.readClasses = [&databasePath](const QString&) {
                return Platform::readClassesNavigatorListAtPath(databasePath);
            };
            actions.readNavigation = [&databasePath](
                const QString&,
                const Application::ClassesNavigationSnapshotQuery& query
                ) {
                return Platform::readClassesNavigatorNavigationAtPath(
                    databasePath, query);
            };
            actions.readPreferences = [&databasePath](const QString&) {
                return Platform::readClassesNavigatorPreferencesAtPath(databasePath);
            };
            actions.saveScheduleMode = [&databasePath](
                const QString&,
                const Application::ScheduleDisplayMode mode
                ) {
                return Platform::saveClassesNavigatorScheduleModeAtPath(
                    databasePath, mode);
            };
            actions.saveVisibilityScope = [&databasePath](
                const QString&,
                const Application::ClassVisibilityScope scope
                ) {
                return Platform::saveClassesNavigatorVisibilityAtPath(
                    databasePath, scope);
            };
            actions.activeClassSelection = [&activeSelection] {
                return activeSelection;
            };
            actions.setActiveClassSelection = [&activeSelection](
                const std::optional<Domain::ClassId>& selected
                ) {
                activeSelection = selected;
            };
            actions.readClassDetails = [&databasePath](
                const QString&,
                const Domain::ClassId& id
                ) {
                return Platform::readClassDetailsAtPath(databasePath, id);
            };
            actions.readClassCoTeacher = [&databasePath](
                const QString&,
                const Domain::ClassId& id
                ) {
                return Platform::readClassCoTeacherAtPath(databasePath, id);
            };
            actions.readClassCoTeacherChoices = [&databasePath](const QString&) {
                return Platform::readClassCoTeacherChoicesAtPath(databasePath);
            };
            actions.readClassCoTeacherTeacherDetails = [&databasePath](
                const QString&,
                const Domain::TeacherId& id
                ) {
                return Platform::readClassCoTeacherTeacherDetailsAtPath(
                    databasePath, id);
            };
            actions.saveClassDetails = [&databasePath](
                const QString&,
                const Application::ClassDetailsSaveWorkflowRequest& request
                ) {
                return Platform::saveClassDetailsAtPath(databasePath, request);
            };
            actions.saveClassCoTeacher = [&databasePath](
                const QString&,
                const Application::ClassCoTeacherAssignmentRequest& request
                ) {
                return Platform::assignClassCoTeacherAtPath(databasePath, request);
            };
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::ClassesNavigatorPage>(context, std::move(actions)));
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
    if (!host.registerPage(std::move(classes))
        || !host.registerPage(std::move(other))
        || !host.activatePage(QStringLiteral("classes")))
    {
        if (error)
        {
            *error = QStringLiteral("The Next Classes route could not activate.");
        }
        return false;
    }
    auto* const page = dynamic_cast<Ui::ClassesNavigatorPage*>(
        host.pageInstance(QStringLiteral("classes")));
    if (!page || page->selectedClassId() != selectedClassId)
    {
        if (error)
        {
            *error = QStringLiteral("Next did not select the paired fixture class.");
        }
        return false;
    }
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    if (!editor || !editor->isLoaded())
    {
        if (error)
        {
            *error = QStringLiteral("Next selected-class Details did not load.");
        }
        return false;
    }
    if (coTeacher)
    {
        editor->sections()->setCurrentIndex(1);
    }
    host.resize(Viewport);
    host.show();
    QApplication::processEvents();
    if (coTeacher)
    {
        const auto* const teacherChoice = editor->findChild<QComboBox*>(
            QStringLiteral("classEditorCoTeacherChoice"));
        const auto* const status = editor->findChild<QLabel*>(
            QStringLiteral("classEditorCoTeacherStatus"));
        if (!teacherChoice || teacherChoice->currentData().toString().isEmpty()
            || !status || !status->text().isEmpty())
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Next Co-Teacher capture did not load the assigned teacher profile.");
            }
            return false;
        }
    }
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    if (!saveImage(host, outputPath, error))
    {
        return false;
    }

    const int classCount = page->model()->classCount();
    const int activeWidgets = widgetCount(page);
    const int editorCount = page->findChildren<QWidget*>(
        QStringLiteral("selectedClassEditor")).size();
    if (!host.activatePage(QStringLiteral("other")))
    {
        if (error)
        {
            *error = QStringLiteral("Next Classes did not suspend after capture.");
        }
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    const bool cleared = !editor->classId().has_value();
    *state = {
        {QStringLiteral("route"), QStringLiteral("Next ClassesNavigatorPage")},
        {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("width"), Viewport.width()},
        {QStringLiteral("height"), Viewport.height()},
        {QStringLiteral("classId"), QString::fromStdString(selectedClassId.value())},
        {QStringLiteral("section"), coTeacher
            ? QStringLiteral("Co-Teacher") : QStringLiteral("Details and Times")},
        {QStringLiteral("fixtureClassCount"), classCount},
        {QStringLiteral("widgetCountWhileActive"), activeWidgets},
        {QStringLiteral("selectedEditorWidgetCount"), editorCount},
        {QStringLiteral("selectedEditorDescendantCount"), widgetCount(editor) - 1},
        {QStringLiteral("classScopedWidgetCount"), classScopedWidgetCount(page)},
        {QStringLiteral("clearedOnSuspension"), cleared},
        {QStringLiteral("widgetCountAfterSuspension"), widgetCount(page)},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("scope"), QStringLiteral(
                "Whole-process observations, not page allocation or memory budgets")},
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("suspended"), memoryJson(suspended)}
        }}
    };
    return editorCount == 1 && cleared && classScopedWidgetCount(page) == 0;
}

bool captureCase(
    const QString& sourceFixture,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    QJsonArray* captures,
    QString* error
    )
{
    QTemporaryDir fixtureDirectory;
    if (!fixtureDirectory.isValid())
    {
        if (error) *error = QStringLiteral("Could not create a paired fixture directory.");
        return false;
    }
    const QString fixturePath = fixtureDirectory.filePath(QStringLiteral("paired-classes.tps"));
    QFile::remove(fixturePath);
    if (!QFile::copy(sourceFixture, fixturePath))
    {
        if (error) *error = QStringLiteral("Could not copy the tracked 96-class fixture.");
        return false;
    }
    const auto list = Platform::readClassesNavigatorListAtPath(fixturePath);
    if (!list || list.value().classes.size() != 96)
    {
        if (error) *error = QStringLiteral("The paired fixture did not return 96 classes.");
        return false;
    }
    const Domain::ClassId selectedClassId = list.value().classes.front().classId;
    bool legacyAndNextSucceeded = true;
    for (const auto& [sectionName, legacySection, isCoTeacher] : {
            std::tuple<QString, ClassesSection, bool>{
                QStringLiteral("details_times"), ClassesSection::Details, false},
            std::tuple<QString, ClassesSection, bool>{
                QStringLiteral("coteacher"), ClassesSection::CoTeacher, true}})
    {
        const QString key = QStringLiteral("%1_%2_%3")
            .arg(sectionName, locale, theme);
        const QString legacyImage = QDir(outputDirectory).filePath(
            QStringLiteral("classes_%1_legacy.png").arg(key));
        const QString nextImage = QDir(outputDirectory).filePath(
            QStringLiteral("classes_%1_next.png").arg(key));
        QJsonObject legacyState;
        if (!captureLegacy(fixturePath, std::stoi(selectedClassId.value()),
                legacySection, legacyImage, memoryProvider, &legacyState, error))
        {
            return false;
        }
        QJsonObject nextState;
        if (!captureNext(fixturePath, selectedClassId, isCoTeacher, loader,
                preferences, nextImage, memoryProvider, &nextState, error))
        {
            return false;
        }
        const bool sameSelection = legacyState.value(QStringLiteral("classId")).toInt()
            == std::stoi(selectedClassId.value());
        legacyAndNextSucceeded = legacyAndNextSucceeded && sameSelection;
        captures->append(QJsonObject{
            {QStringLiteral("locale"), locale},
            {QStringLiteral("theme"), theme},
            {QStringLiteral("section"), sectionName},
            {QStringLiteral("fixtureClassCount"), 96},
            {QStringLiteral("selectedClassId"), QString::fromStdString(selectedClassId.value())},
            {QStringLiteral("legacy"), legacyState},
            {QStringLiteral("next"), nextState},
            {QStringLiteral("sameSelectedClass"), sameSelection}
        });
    }
    return legacyAndNextSucceeded;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextClassesDetailsCoTeacherVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create Classes details capture directory");
        return 2;
    }
    const QString sourceFixture = QStringLiteral(CLASSMNGR_CLASSES_NAVIGATOR_FIXTURE);
    if (!QFileInfo::exists(sourceFixture))
    {
        qCritical().noquote() << "Missing 96-class fixture:" << sourceFixture;
        return 1;
    }
    const auto loaded = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
    if (!loaded)
    {
        qCritical().noquote() << "Could not load the Next resource catalog:"
                              << loaded.error().message;
        return 1;
    }
    Resources::ResourceLoader loader(std::move(*loaded));
    Test::MemoryPreferences preferences;
    PlatformProcessMemorySnapshotProvider memoryProvider;
    QJsonArray captures;
    const QString translations = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    const QStringList locales{QStringLiteral("en_US"), QStringLiteral("ko_KR")};
    const QStringList themes{QStringLiteral("light"), QStringLiteral("dark")};
    for (const QString& locale : locales)
    {
        for (const QString& theme : themes)
        {
            QLocale::setDefault(QLocale(locale));
            application.setPalette(themePalette(theme));
            std::unique_ptr<QTranslator> translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                translator = std::make_unique<QTranslator>();
                if (!translator->load(QDir(translations).filePath(
                        QStringLiteral("ClassMngr_ko_KR.qm")))
                    || !application.installTranslator(translator.get()))
                {
                    qCritical("Could not install the Korean capture translator");
                    return 1;
                }
            }
            QString error;
            if (!captureCase(sourceFixture, outputDirectory, locale, theme,
                    loader, preferences.model, memoryProvider, &captures, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            if (translator)
            {
                application.removeTranslator(translator.get());
            }
        }
    }
    const QJsonObject manifest{
        {QStringLiteral("scope"), QStringLiteral(
            "Paired legacy and Next Classes Details/Times and Co-Teacher surfaces")},
        {QStringLiteral("fixture"), QFileInfo(sourceFixture).fileName()},
        {QStringLiteral("fixtureClassCount"), 96},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("memoryLimitation"), QStringLiteral(
            "Measurements are whole-process working/private memory snapshots; they do not isolate widget allocations or establish a memory budget.")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("classes_details_coteacher_paired_capture.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write the paired Classes capture manifest");
        return 1;
    }
    return 0;
}
