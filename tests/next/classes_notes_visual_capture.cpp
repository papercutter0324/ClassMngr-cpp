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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTranslator>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};
const QString NotesText = QStringLiteral("Notes fixture for paired capture");
const QString ActivitiesText = QStringLiteral("Activities fixture for paired capture");

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
        widgets.cbegin(), widgets.cend(), [](const QWidget* widget) {
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
    const QImage image = widget.grab(QRect(QPoint(0, 0), Viewport)).toImage();
    if (image.size() == Viewport && image.save(path, "PNG"))
    {
        return true;
    }
    if (error)
    {
        *error = QStringLiteral(
            "Could not save paired Notes capture at %1x%2 (expected %3x%4): %5")
            .arg(image.width()).arg(image.height())
            .arg(Viewport.width()).arg(Viewport.height()).arg(path);
    }
    return false;
}

bool captureLegacy(
    const QString& databasePath,
    const int selectedId,
    const QString& imagePath,
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
    if (!page.openClass(selectedId, ClassesSection::Notes))
    {
        if (error) *error = QStringLiteral("Legacy Classes could not open Notes.");
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
    if (!saveImage(page, imagePath, error))
    {
        return false;
    }

    const ClassesPageRuntimeMetrics metrics = page.runtimeMetrics();
    const int activeWidgets = widgetCount(&page);
    const QRect viewport(QPoint(0, 0), Viewport);
    QJsonArray saveButtonCandidates;
    bool saveButtonFound = false;
    bool visibleSaveButtonFound = false;
    bool visibleSaveButtonInViewport = false;
    for (QPushButton* const button : page.findChildren<QPushButton*>(
             QStringLiteral("classNotesSaveButton")))
    {
        const QRect buttonRect(button->mapTo(&page, QPoint(0, 0)), button->size());
        const bool widgetVisibleToPage = button->isVisibleTo(&page);
        const bool visibleInViewport = widgetVisibleToPage
            && viewport.contains(buttonRect);
        saveButtonCandidates.append(QJsonObject{
            {QStringLiteral("widgetVisibleToPage"), widgetVisibleToPage},
            {QStringLiteral("visibleInViewport"), visibleInViewport},
            {QStringLiteral("x"), buttonRect.x()},
            {QStringLiteral("y"), buttonRect.y()},
            {QStringLiteral("width"), buttonRect.width()},
            {QStringLiteral("height"), buttonRect.height()},
            {QStringLiteral("parentObjectName"), button->parentWidget()
                ? button->parentWidget()->objectName() : QString{}},
            {QStringLiteral("isVisible"), button->isVisible()},
            {QStringLiteral("isHidden"), button->isHidden()}
        });
        saveButtonFound = true;
        visibleSaveButtonFound = visibleSaveButtonFound || widgetVisibleToPage;
        visibleSaveButtonInViewport =
            visibleSaveButtonInViewport || visibleInViewport;
    }
    const QJsonObject saveButtonState{
        {QStringLiteral("found"), saveButtonFound},
        {QStringLiteral("widgetVisibleToPage"), saveButtonFound
            ? QJsonValue(visibleSaveButtonFound)
            : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("visibleInViewport"), saveButtonFound
            ? QJsonValue(visibleSaveButtonInViewport)
            : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("candidates"), saveButtonCandidates}
    };
    page.releaseFeatureResources();
    QApplication::processEvents();
    services.closeDatabase();
    const ProcessMemorySnapshot released = memoryProvider.snapshot();
    *state = {
        {QStringLiteral("route"), QStringLiteral("legacy ClassesPage")},
        {QStringLiteral("image"), QFileInfo(imagePath).fileName()},
        {QStringLiteral("selectedClassId"), selectedId},
        {QStringLiteral("fixtureClassCount"), metrics.sourceClassCount},
        {QStringLiteral("widgetCountWhileActive"), activeWidgets},
        {QStringLiteral("pageSize"), QJsonObject{
            {QStringLiteral("width"), page.width()},
            {QStringLiteral("height"), page.height()}}},
        {QStringLiteral("saveButton"), saveButtonState},
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
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& imagePath,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    QJsonObject* state,
    QString* error
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        if (error) *error = toLegacyDiagnostic(opened.error());
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
            actions.readClassNotes = [&databasePath](
                const QString&,
                const Domain::ClassId& id
                ) {
                return Platform::readClassNotesAtPath(databasePath, id);
            };
            actions.saveClassDetails = [&databasePath](
                const QString&,
                const Application::ClassDetailsSaveWorkflowRequest& request
                ) {
                return Platform::saveClassDetailsAtPath(databasePath, request);
            };
            actions.saveClassNotes = [&databasePath](
                const QString&,
                const Application::ClassNotesSaveRequest& request
                ) {
                return Platform::saveClassNotesAtPath(databasePath, request);
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
        if (error) *error = QStringLiteral("Next did not select the paired class.");
        return false;
    }
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    if (!editor || !editor->isLoaded())
    {
        if (error) *error = QStringLiteral("Next selected-class Details did not load.");
        return false;
    }
    editor->sections()->setCurrentIndex(2);
    host.resize(Viewport);
    host.show();
    QApplication::processEvents();
    const auto* const notes = editor->findChild<QTextEdit*>(
        QStringLiteral("classEditorClassNotes"));
    const auto* const activities = editor->findChild<QTextEdit*>(
        QStringLiteral("classEditorTimeFillerActivities"));
    if (!notes || !activities || notes->toPlainText() != NotesText
        || activities->toPlainText() != ActivitiesText)
    {
        if (error) *error = QStringLiteral("Next Notes did not read the seeded fixture values.");
        return false;
    }
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    if (!saveImage(host, imagePath, error))
    {
        return false;
    }

    const int classCount = page->model()->classCount();
    const int activeWidgets = widgetCount(page);
    const int activeEditorWidgets = widgetCount(editor);
    const int editorCount = page->findChildren<QWidget*>(
        QStringLiteral("selectedClassEditor")).size();
    if (!host.activatePage(QStringLiteral("other")))
    {
        if (error) *error = QStringLiteral("Next Classes did not suspend after Notes capture.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    const bool cleared = !editor->classId().has_value();
    *state = {
        {QStringLiteral("route"), QStringLiteral("Next ClassesNavigatorPage")},
        {QStringLiteral("image"), QFileInfo(imagePath).fileName()},
        {QStringLiteral("selectedClassId"), QString::fromStdString(selectedClassId.value())},
        {QStringLiteral("fixtureClassCount"), classCount},
        {QStringLiteral("widgetCountWhileActive"), activeWidgets},
        {QStringLiteral("selectedEditorWidgetCount"), editorCount},
        {QStringLiteral("selectedEditorDescendantCount"), activeEditorWidgets - 1},
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

bool capturePair(
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
        if (error) *error = QStringLiteral("Could not create paired fixture directory.");
        return false;
    }
    const QString databasePath = fixtureDirectory.filePath(
        QStringLiteral("paired-classes.tps"));
    QFile::remove(databasePath);
    if (!QFile::copy(sourceFixture, databasePath))
    {
        if (error) *error = QStringLiteral("Could not copy tracked 96-class fixture.");
        return false;
    }
    const auto list = Platform::readClassesNavigatorListAtPath(databasePath);
    if (!list || list.value().classes.size() != 96)
    {
        if (error) *error = QStringLiteral("Paired fixture did not return 96 classes.");
        return false;
    }
    const Domain::ClassId selectedId = list.value().classes.front().classId;
    const Application::ClassNotesSaveRequest request{
        .classId = selectedId,
        .notes = NotesText.toStdU16String(),
        .timeFillerActivities = ActivitiesText.toStdU16String()
    };
    const auto seeded = Platform::saveClassNotesAtPath(databasePath, request);
    if (!seeded)
    {
        if (error) *error = QString::fromStdString(seeded.error().message);
        return false;
    }

    const QString key = QStringLiteral("%1_%2").arg(locale, theme);
    const QString legacyImage = QDir(outputDirectory).filePath(
        QStringLiteral("classes_notes_%1_legacy.png").arg(key));
    const QString nextImage = QDir(outputDirectory).filePath(
        QStringLiteral("classes_notes_%1_next.png").arg(key));
    QJsonObject legacyState;
    if (!captureLegacy(databasePath, std::stoi(selectedId.value()), legacyImage,
            memoryProvider, &legacyState, error))
    {
        return false;
    }
    QJsonObject nextState;
    if (!captureNext(databasePath, selectedId, loader, preferences,
            nextImage, memoryProvider, &nextState, error))
    {
        return false;
    }
    captures->append(QJsonObject{
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("section"), QStringLiteral("Notes and Time Filler")},
        {QStringLiteral("fixtureClassCount"), 96},
        {QStringLiteral("selectedClassId"), QString::fromStdString(selectedId.value())},
        {QStringLiteral("seededNotesLength"), NotesText.size()},
        {QStringLiteral("seededActivitiesLength"), ActivitiesText.size()},
        {QStringLiteral("legacy"), legacyState},
        {QStringLiteral("next"), nextState},
        {QStringLiteral("sameSelectedClass"),
         legacyState.value(QStringLiteral("selectedClassId")).toInt()
            == std::stoi(selectedId.value())}
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
        qCritical("Usage: ClassMngrNextClassesNotesVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create paired Notes capture directory");
        return 2;
    }
    const QString sourceFixture = QStringLiteral(CLASSMNGR_CLASSES_NAVIGATOR_FIXTURE);
    if (!QFileInfo::exists(sourceFixture))
    {
        qCritical().noquote() << "Missing tracked 96-class fixture:" << sourceFixture;
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
                    qCritical("Could not install Korean Notes capture translator");
                    return 1;
                }
            }
            QString error;
            if (!capturePair(sourceFixture, outputDirectory, locale, theme,
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
            "Paired Legacy and Next Classes Notes and Time Filler surfaces")},
        {QStringLiteral("fixture"), QFileInfo(sourceFixture).fileName()},
        {QStringLiteral("fixtureClassCount"), 96},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("captureLimitation"), QStringLiteral(
            "The legacy Notes page expanded to 1024x844 under the offscreen layout. Both captures use the top-left 1024x768 viewport crop; each legacy entry records the Save widget's actual visibility and geometry. visibleInViewport is null only when the stable Save widget cannot be located.")},
        {QStringLiteral("memoryLimitation"), QStringLiteral(
            "Measurements are whole-process working/private memory snapshots; they do not isolate widget allocations or establish a memory budget.")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("classes_notes_paired_capture.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write paired Notes capture manifest");
        return 1;
    }
    return 0;
}
