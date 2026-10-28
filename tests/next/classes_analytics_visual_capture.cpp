#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "features/classes/ui/classes_page.h"
#include "next/platform/workspace_store_class_analytics_actions.h"
#include "next/platform/workspace_store_class_editor_actions.h"
#include "next/platform/workspace_store_classes_navigator_read_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/classes_navigator_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QAbstractItemModel>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPalette>
#include <QTemporaryDir>
#include <QTableView>
#include <QTranslator>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
    palette.setColor(QPalette::AlternateBase,
        QColor(dark ? QStringLiteral("#30343a") : QStringLiteral("#f0f1f2")));
    palette.setColor(QPalette::PlaceholderText,
        QColor(dark ? QStringLiteral("#b7bec6") : QStringLiteral("#68717a")));
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
        widgets.cbegin(), widgets.cend(), [](const QWidget* widget) {
            return widget->property("class_id").isValid();
        }));
}

QComboBox* evaluationChoice(QWidget& root)
{
    for (QComboBox* const choice : root.findChildren<QComboBox*>())
    {
        for (int index = 0; index < choice->count(); ++index)
        {
            if (choice->itemData(index).toString() == QStringLiteral("Winter"))
            {
                return choice;
            }
        }
    }
    return nullptr;
}

bool selectEvaluation(QComboBox* choice, const QString& name)
{
    if (!choice)
    {
        return false;
    }
    const int index = choice->findData(name);
    if (index < 0)
    {
        return false;
    }
    choice->setCurrentIndex(index);
    return choice->currentData().toString() == name;
}

bool saveImage(QWidget& widget, const QString& path, QString* error)
{
    const QImage image = widget.grab(QRect(QPoint(0, 0), Viewport)).toImage();
    if (image.size() == Viewport && image.save(path, "PNG"))
    {
        return true;
    }
    if (error)
    {
        *error = QStringLiteral("Could not save Analytics capture %1x%2: %3")
            .arg(image.width()).arg(image.height()).arg(path);
    }
    return false;
}

bool captureLegacy(
    const QString& databasePath,
    const int selectedId,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
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
        if (error) *error = toLegacyDiagnostic(opened.error());
        return false;
    }

    ClassesPage page(&services);
    page.resize(Viewport);
    if (!page.openClass(selectedId, ClassesSection::Analytics))
    {
        if (error) *error = QStringLiteral("Legacy Classes could not open Analytics.");
        return false;
    }
    page.show();
    QApplication::processEvents();
    QComboBox* const choice = evaluationChoice(page);
    if (!choice || !selectEvaluation(choice, QString{}))
    {
        if (error) *error = QStringLiteral("Legacy Analytics selector did not expose All.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot loadedAll = memoryProvider.snapshot();
    const QString base = QStringLiteral("classes_analytics_%1_%2_legacy")
        .arg(locale, theme);
    const QString allImage = QDir(outputDirectory).filePath(base + QStringLiteral("_all.png"));
    if (!saveImage(page, allImage, error))
    {
        return false;
    }
    if (!selectEvaluation(choice, QStringLiteral("Winter")))
    {
        if (error) *error = QStringLiteral("Legacy Analytics selector did not expose Winter.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot loadedWinter = memoryProvider.snapshot();
    const QString winterImage = QDir(outputDirectory).filePath(
        base + QStringLiteral("_winter.png"));
    if (!saveImage(page, winterImage, error))
    {
        return false;
    }
    const ClassesPageRuntimeMetrics metrics = page.runtimeMetrics();
    const QString selectedAll = [&] {
        selectEvaluation(choice, QString{});
        return choice->currentData().toString();
    }();
    const QString selectedWinter = [&] {
        selectEvaluation(choice, QStringLiteral("Winter"));
        return choice->currentData().toString();
    }();
    services.closeDatabase();
    const ProcessMemorySnapshot closed = memoryProvider.snapshot();
    *state = {
        {QStringLiteral("route"), QStringLiteral("Legacy ClassesPage")},
        {QStringLiteral("selectedClassId"), selectedId},
        {QStringLiteral("fixtureClassCount"), metrics.sourceClassCount},
        {QStringLiteral("pageSize"), QJsonObject{
            {QStringLiteral("width"), page.width()},
            {QStringLiteral("height"), page.height()}}},
        {QStringLiteral("widgetCountWhileActive"), widgetCount(&page)},
        {QStringLiteral("analyticsEditorsInstantiated"),
         metrics.instantiatedEditorCount},
        {QStringLiteral("selectedAll"), selectedAll.isEmpty()},
        {QStringLiteral("selectedWinter"), selectedWinter},
        {QStringLiteral("images"), QJsonArray{
            QFileInfo(allImage).fileName(), QFileInfo(winterImage).fileName()}},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("scope"), QStringLiteral(
                "Whole-process observations; they do not isolate page allocations")},
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("loadedAll"), memoryJson(loadedAll)},
            {QStringLiteral("loadedWinter"), memoryJson(loadedWinter)},
            {QStringLiteral("afterClose"), memoryJson(closed)}}}
    };
    return selectedAll.isEmpty() && selectedWinter == QStringLiteral("Winter");
}

bool captureNext(
    const QString& databasePath,
    const Domain::ClassId& selectedClassId,
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    QJsonObject* state,
    QString* error
    )
{
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    std::optional<Domain::ClassId> activeSelection = selectedClassId;
    std::vector<Domain::ClassId> analyticsReads;
    Ui::PageHost host(loader, preferences);
    Ui::PageDescriptor classes{
        .id = *Ui::PageId::fromString(QStringLiteral("classes")),
        .label = QStringLiteral("Classes"),
        .factory = [&databasePath, &activeSelection, &analyticsReads](
            const Ui::PageContext& context
            ) -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
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
            actions.readClassAnalytics = [&databasePath, &analyticsReads](
                const QString&,
                const Application::ClassAnalyticsPageReadRequest& request
                ) {
                analyticsReads.push_back(request.classId);
                return Platform::readClassAnalyticsAtPath(databasePath, request);
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
        if (error) *error = QStringLiteral("Next Classes could not activate.");
        return false;
    }
    auto* const page = dynamic_cast<Ui::ClassesNavigatorPage*>(
        host.pageInstance(QStringLiteral("classes")));
    if (!page || page->selectedClassId() != selectedClassId)
    {
        if (error) *error = QStringLiteral("Next did not select fixture class 1.");
        return false;
    }
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    if (!editor || !editor->isLoaded())
    {
        if (error) *error = QStringLiteral("Next selected-class Details did not load.");
        return false;
    }
    host.resize(Viewport);
    host.show();
    editor->sections()->setCurrentIndex(3);
    QApplication::processEvents();
    QComboBox* const choice = editor->analyticsEvaluationChoice();
    QTableView* const ranking = editor->analyticsRankingTable();
    if (!choice || !ranking || choice->currentData().toString() != QString{})
    {
        if (error) *error = QStringLiteral("Next Analytics did not default to All.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot loadedAll = memoryProvider.snapshot();
    const int allRows = ranking->model()->rowCount();
    const int activeWidgets = widgetCount(page);
    const int activeEditorWidgets = widgetCount(editor);
    const int editorCount = page->findChildren<QWidget*>(
        QStringLiteral("selectedClassEditor")).size();
    const int fixtureClassCount = page->model()->classCount();
    const int analyticsModelCount = page->findChildren<QAbstractItemModel*>(
        QStringLiteral("selectedClassAnalyticsRankingModel")).size();
    const QString base = QStringLiteral("classes_analytics_%1_%2_next")
        .arg(locale, theme);
    const QString allImage = QDir(outputDirectory).filePath(base + QStringLiteral("_all.png"));
    if (!saveImage(host, allImage, error))
    {
        return false;
    }
    if (!selectEvaluation(choice, QStringLiteral("Winter")))
    {
        if (error) *error = QStringLiteral("Next Analytics selector did not expose Winter.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot loadedWinter = memoryProvider.snapshot();
    const int winterRows = ranking->model()->rowCount();
    if (winterRows != 25)
    {
        if (error) *error = QStringLiteral("Next Winter Analytics did not show all 25 rows.");
        return false;
    }
    const QString winterImage = QDir(outputDirectory).filePath(
        base + QStringLiteral("_winter.png"));
    if (!saveImage(host, winterImage, error))
    {
        return false;
    }
    editor->sections()->setCurrentIndex(0);
    QApplication::processEvents();
    const ProcessMemorySnapshot sectionLeave = memoryProvider.snapshot();
    const int rowsAfterSectionLeave = ranking->model()->rowCount();
    page->clearForClosedWorkspace();
    QApplication::processEvents();
    const ProcessMemorySnapshot workspaceClear = memoryProvider.snapshot();
    const bool clearedOnWorkspaceClose = !editor->classId().has_value()
        && ranking->model()->rowCount() == 0;
    QJsonArray readClassIds;
    for (const Domain::ClassId& id : analyticsReads)
    {
        readClassIds.append(QString::fromStdString(id.value()));
    }
    *state = {
        {QStringLiteral("route"), QStringLiteral("Next ClassesNavigatorPage")},
        {QStringLiteral("selectedClassId"), QString::fromStdString(selectedClassId.value())},
        {QStringLiteral("fixtureClassCount"), fixtureClassCount},
        {QStringLiteral("pageSize"), QJsonObject{
            {QStringLiteral("width"), host.width()},
            {QStringLiteral("height"), host.height()}}},
        {QStringLiteral("widgetCountWhenAnalyticsActive"), activeWidgets},
        {QStringLiteral("selectedEditorWidgetCount"), editorCount},
        {QStringLiteral("selectedEditorDescendantCount"), activeEditorWidgets - 1},
        {QStringLiteral("classScopedWidgetCount"), classScopedWidgetCount(page)},
        {QStringLiteral("analyticsRankingModelCount"), analyticsModelCount},
        {QStringLiteral("analyticsRowsAll"), allRows},
        {QStringLiteral("analyticsRowsWinter"), winterRows},
        {QStringLiteral("analyticsRowsAfterSectionLeave"), rowsAfterSectionLeave},
        {QStringLiteral("analyticsReadClassIds"), readClassIds},
        {QStringLiteral("clearedOnWorkspaceClose"), clearedOnWorkspaceClose},
        {QStringLiteral("images"), QJsonArray{
            QFileInfo(allImage).fileName(), QFileInfo(winterImage).fileName()}},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("scope"), QStringLiteral(
                "Whole-process observations; they do not isolate widget allocations or establish a memory budget")},
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("loadedAll"), memoryJson(loadedAll)},
            {QStringLiteral("loadedWinter"), memoryJson(loadedWinter)},
            {QStringLiteral("afterSectionLeave"), memoryJson(sectionLeave)},
            {QStringLiteral("afterWorkspaceClear"), memoryJson(workspaceClear)}}}
    };
    return allRows >= 25 && winterRows == 25 && rowsAfterSectionLeave == 0
        && editorCount == 1 && analyticsModelCount == 1
        && analyticsReads.size() == 2
        && std::all_of(analyticsReads.cbegin(), analyticsReads.cend(),
            [&selectedClassId](const Domain::ClassId& id) {
                return id == selectedClassId;
            })
        && clearedOnWorkspaceClose;
}

bool capturePair(
    const QString& sourceFixture,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    QJsonArray* captures,
    QString* error
    )
{
    QTemporaryDir fixtureDirectory;
    if (!fixtureDirectory.isValid())
    {
        if (error) *error = QStringLiteral("Could not create paired fixture directory.");
        return false;
    }
    const QString databasePath = fixtureDirectory.filePath(
        QStringLiteral("paired-class-analytics.tps"));
    if (!QFile::copy(sourceFixture, databasePath))
    {
        if (error) *error = QStringLiteral("Could not copy tracked analytics fixture.");
        return false;
    }
    const auto list = Platform::readClassesNavigatorListAtPath(databasePath);
    if (!list || list.value().classes.size() != 96
        || list.value().classes.front().classId.value() != std::string("1"))
    {
        if (error) *error = QStringLiteral(
            "Fixture class 1 was not found in the 96-class workspace.");
        return false;
    }
    const Domain::ClassId selectedId = list.value().classes.front().classId;
    PlatformProcessMemorySnapshotProvider memoryProvider;
    QJsonObject legacyState;
    if (!captureLegacy(databasePath, std::stoi(selectedId.value()),
            outputDirectory, locale, theme, memoryProvider, &legacyState, error))
    {
        return false;
    }
    QJsonObject nextState;
    if (!captureNext(databasePath, selectedId, loader, preferences,
            outputDirectory, locale, theme, memoryProvider, &nextState, error))
    {
        return false;
    }
    captures->append(QJsonObject{
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("section"), QStringLiteral("Class Analytics")},
        {QStringLiteral("evaluationStates"), QJsonArray{
            QStringLiteral("All"), QStringLiteral("Winter")}},
        {QStringLiteral("fixtureClassCount"), 96},
        {QStringLiteral("selectedClassId"), QString::fromStdString(selectedId.value())},
        {QStringLiteral("legacy"), legacyState},
        {QStringLiteral("next"), nextState},
        {QStringLiteral("sameSelectedClass"),
         legacyState.value(QStringLiteral("selectedClassId")).toInt()
            == std::stoi(selectedId.value())
            && nextState.value(QStringLiteral("selectedClassId")).toString()
            == QString::fromStdString(selectedId.value())},
        {QStringLiteral("sameViewport"),
         legacyState.value(QStringLiteral("pageSize"))
            == nextState.value(QStringLiteral("pageSize"))}
    });
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextClassesAnalyticsVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create paired Analytics capture directory");
        return 2;
    }
    const QString sourceFixture = QStringLiteral(CLASSMNGR_SPEAKING_ANALYTICS_FIXTURE);
    if (!QFileInfo::exists(sourceFixture))
    {
        qCritical().noquote() << "Missing tracked Analytics fixture:" << sourceFixture;
        return 1;
    }
    const auto loaded = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
    if (!loaded)
    {
        qCritical().noquote() << "Could not load Next resource catalog:" << loaded.error().message;
        return 1;
    }
    Resources::ResourceLoader loader(std::move(*loaded));
    Test::MemoryPreferences preferences;
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
                    qCritical("Could not install Korean Analytics capture translator");
                    return 1;
                }
            }
            QString error;
            if (!capturePair(sourceFixture, outputDirectory, locale, theme,
                    loader, preferences.model, &captures, &error))
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
            "Paired Legacy and Next Classes Analytics, All and Winter states")},
        {QStringLiteral("fixture"), QFileInfo(sourceFixture).fileName()},
        {QStringLiteral("fixtureClassCount"), 96},
        {QStringLiteral("selectedFixtureClassId"), QStringLiteral("1")},
        {QStringLiteral("selectedWinterRows"), 25},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("captureLimitation"), QStringLiteral(
            "The images show the top-left 1024x768 viewport of each route; the Legacy Classes shell and Next navigator shell differ. This capture documents matching class and selector states and does not establish visual parity.")},
        {QStringLiteral("memoryLimitation"), QStringLiteral(
            "Memory values are whole-process working/private memory snapshots. They do not isolate widget allocations or establish a hard memory budget.")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("classes_analytics_paired_capture.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write paired Analytics capture manifest");
        return 1;
    }
    return 0;
}
