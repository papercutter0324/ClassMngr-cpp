#include "core/process_memory_snapshot.h"
#include "next/application/classes_navigator_preferences.h"
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
#include <QLocale>
#include <QPalette>
#include <QTranslator>
#include <QListView>
#include <QTabBar>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};

Domain::ClassId classId(const int id)
{
    return *Domain::ClassId::fromString(std::to_string(id));
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

Application::ClassesListSnapshot classList(const int count)
{
    Application::ClassesListSnapshot result;
    result.classes.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index)
    {
        result.classes.push_back({
            .classId = classId(1000 + index),
            .className = QStringLiteral("Navigator Class %1").arg(index + 1)
                .toStdU16String()
        });
    }
    return result;
}

Application::ClassesNavigationSnapshot navigationSnapshot(
    const Application::ClassesListSnapshot& classes
    )
{
    Application::ClassesNavigationSnapshot snapshot;
    snapshot.classes.reserve(classes.classes.size());
    const std::array<QString, 3> grades{
        QStringLiteral("E4"), QStringLiteral("E5"), QStringLiteral("E6")
    };
    for (std::size_t index = 0; index < classes.classes.size(); ++index)
    {
        const auto& classroom = classes.classes[index];
        const QString grade = grades[index % grades.size()];
        const QString teacher = QStringLiteral("Teacher %1")
            .arg(static_cast<qulonglong>(index % 24 + 1));
        Application::ClassesNavigationDetails details{
            .grade = grade.toStdU16String(),
            .level = QStringLiteral("Level %1").arg(index % 8 + 1).toStdU16String(),
            .teacherEnglishName = teacher.toStdU16String(),
            .teacherKoreanName = QStringLiteral("교사 %1")
                .arg(static_cast<qulonglong>(index % 24 + 1)).toStdU16String()
        };
        details.regularSchedule.push_back({
            .day = (index % 4 == 0 ? QStringLiteral("Saturday")
                                   : QStringLiteral("Monday")).toStdU16String(),
            .startTime = QStringLiteral("%1:00 PM")
                .arg(static_cast<qulonglong>(index % 5 + 1)).toStdU16String(),
            .endTime = QStringLiteral("%1:50 PM")
                .arg(static_cast<qulonglong>(index % 5 + 1)).toStdU16String()
        });
        if (index % 2 == 0)
        {
            details.intensiveSchedule.push_back({
                .day = QStringLiteral("Tuesday").toStdU16String(),
                .startTime = QStringLiteral("6:00 PM").toStdU16String(),
                .endTime = QStringLiteral("6:50 PM").toStdU16String()
            });
        }
        snapshot.classes.push_back({
            .classId = classroom.classId,
            .className = classroom.className,
            .details = Application::ClassesNavigationDetailsResult::success(
                std::move(details))
        });
    }
    return snapshot;
}

int widgetCount(const QWidget* root)
{
    return root ? root->findChildren<QWidget*>().size() + 1 : 0;
}

int classPropertyWidgetCount(const QWidget* root)
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

int detailEditorCount(const QWidget* root)
{
    if (!root)
    {
        return 0;
    }
    int count = 0;
    for (QWidget* const widget : root->findChildren<QWidget*>())
    {
        const QString name = widget->objectName();
        if (name.contains(QStringLiteral("Detail"), Qt::CaseInsensitive)
            || name.contains(QStringLiteral("Editor"), Qt::CaseInsensitive))
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

bool captureCase(
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    const int classCount,
    QJsonObject* state,
    QString* error
    )
{
    const auto classes = classList(classCount);
    const auto navigation = navigationSnapshot(classes);
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot beforeMemory = memoryProvider.snapshot();
    int maximumScheduleRowsPerSourceAndClass = 0;
    int maximumSummaryTokenCodeUnits = 0;
    qint64 classNameCodeUnits = 0;
    qint64 navigationSummaryCodeUnits = 0;
    for (const auto& classroom : classes.classes)
    {
        classNameCodeUnits += static_cast<qint64>(classroom.className.size());
    }
    for (const auto& entry : navigation.classes)
    {
        navigationSummaryCodeUnits += static_cast<qint64>(entry.className.size());
        if (!entry.details)
        {
            continue;
        }
        const auto& details = entry.details.value();
        navigationSummaryCodeUnits += static_cast<qint64>(details.grade.size()
            + details.level.size() + details.teacherEnglishName.size()
            + details.teacherKoreanName.size());
        maximumScheduleRowsPerSourceAndClass = std::max(
            maximumScheduleRowsPerSourceAndClass,
            std::max(static_cast<int>(details.regularSchedule.size()),
                static_cast<int>(details.intensiveSchedule.size())));
        for (const auto* schedule : {&details.regularSchedule,
                                     &details.intensiveSchedule})
        {
            for (const auto& row : *schedule)
            {
                navigationSummaryCodeUnits += static_cast<qint64>(row.day.size()
                    + row.startTime.size() + row.endTime.size());
                maximumSummaryTokenCodeUnits = std::max(
                    maximumSummaryTokenCodeUnits,
                    std::max({static_cast<int>(row.day.size()),
                        static_cast<int>(row.startTime.size()),
                        static_cast<int>(row.endTime.size())}));
            }
        }
    }
    int listReads = 0;
    int navigationReads = 0;
    int navigationQueryClassCount = 0;
    int scheduleRows = 0;

    Ui::PageHost host(loader, preferences);
    Ui::PageDescriptor classesPage{
        .id = *Ui::PageId::fromString(QStringLiteral("classes")),
        .label = QStringLiteral("Classes"),
        .factory = [&classes, &navigation, &listReads, &navigationReads,
                    &navigationQueryClassCount](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            Ui::ClassesNavigatorPageActions actions;
            actions.workspaceIdentity = [] { return QStringLiteral("capture-workspace"); };
            actions.activeWorkspacePath = [] {
                return std::optional<QString>(QStringLiteral("capture.tps"));
            };
            actions.readClasses = [&classes, &listReads](const QString&) {
                ++listReads;
                return Application::ClassesListReadResult::success(classes);
            };
            actions.readNavigation = [&navigation, &navigationReads,
                                      &navigationQueryClassCount](
                const QString&,
                const Application::ClassesNavigationSnapshotQuery& query) {
                ++navigationReads;
                navigationQueryClassCount = static_cast<int>(query.classes.size());
                return Application::ClassesNavigationSnapshotResult::success(
                    navigation);
            };
            actions.readPreferences = [](const QString&) {
                return Application::ClassesNavigatorPreferences{
                    .scheduleDisplayMode = Application::ScheduleDisplayMode::Regular,
                    .visibilityScope = Application::ClassVisibilityScope::ActiveSchedule,
                    .dayFilterResetPolicy =
                        Application::ClassDayFilterResetPolicy::OnApplicationClose,
                    .classSelectionResetPolicy =
                        Application::ClassSelectionResetPolicy::OnApplicationClose
                };
            };
            actions.saveScheduleMode = [](
                const QString&,
                const Application::ScheduleDisplayMode) {
                return Application::ScheduleDisplayModePreferencesSaveResult::success();
            };
            actions.saveVisibilityScope = [](
                const QString&,
                const Application::ClassVisibilityScope) {
                return Application::ClassVisibilityPreferencesSaveResult::success();
            };
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::ClassesNavigatorPage>(context, std::move(actions)));
        }
    };
    Ui::PageDescriptor otherPage{
        .id = *Ui::PageId::fromString(QStringLiteral("other")),
        .label = QStringLiteral("Other"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(std::make_unique<Ui::Page>(context));
        }
    };
    if (!host.registerPage(std::move(classesPage))
        || !host.registerPage(std::move(otherPage))
        || !host.activatePage(QStringLiteral("classes")))
    {
        if (error) *error = QStringLiteral("The Classes capture page did not activate.");
        return false;
    }
    auto* const page = dynamic_cast<Ui::ClassesNavigatorPage*>(
        host.pageInstance(QStringLiteral("classes")));
    if (!page)
    {
        if (error) *error = QStringLiteral("The Classes capture factory returned another page.");
        return false;
    }
    for (const auto& entry : navigation.classes)
    {
        if (entry.details)
        {
            scheduleRows += static_cast<int>(entry.details.value().regularSchedule.size()
                + entry.details.value().intensiveSchedule.size());
        }
    }

    host.resize(Viewport);
    host.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget()) focused->clearFocus();
    QApplication::processEvents();

    const ProcessMemorySnapshot activeMemory = memoryProvider.snapshot();
    const QImage image = host.grab().toImage();
    const QString key = QStringLiteral("%1_%2_%3")
        .arg(locale, theme, classCount == 96 ? QStringLiteral("96")
                                             : QStringLiteral("small"));
    const QString imagePath = QDir(outputDirectory).filePath(
        QStringLiteral("classes_%1.png").arg(key));
    if (image.size() != Viewport || !image.save(imagePath, "PNG"))
    {
        if (error) *error = QStringLiteral("A Classes visual capture could not be saved.");
        return false;
    }

    const int classRows = page->model()->classCount();
    const int gradeGroupCount = page->model()->gradeGroupCount();
    const int allRows = static_cast<int>(page->model()->classIdsForGroup(
        Ui::ClassesNavigatorModel::allClassesGroupKey()).size());
    QJsonArray gradeGroups;
    for (const auto& group : page->model()->groups())
    {
        if (group.key == Ui::ClassesNavigatorModel::allClassesGroupKey()) continue;
        gradeGroups.append(QJsonObject{
            {QStringLiteral("key"), group.key},
            {QStringLiteral("label"), group.label},
            {QStringLiteral("rows"), static_cast<int>(
                page->model()->classIdsForGroup(group.key).size())}
        });
    }
    const int listViewCount = page->findChildren<QListView*>(
        QStringLiteral("classesNavigatorClassList")).size();
    const int modelCount = page->model()
        && page->classList()->model() == page->model() ? 1 : 0;
    const int pageCount = host.pageInstance(QStringLiteral("classes")) == page
        ? 1 : 0;
    const int activeWidgetCount = widgetCount(page);
    const int perClassWidgetCount = classPropertyWidgetCount(page);
    const int editors = detailEditorCount(page);
    const int listResultRows = classRows;
    const int navigationResultRows = static_cast<int>(navigation.classes.size());

    if (!host.activatePage(QStringLiteral("other")))
    {
        if (error) *error = QStringLiteral("The Classes page did not suspend.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot suspendedMemory = memoryProvider.snapshot();
    const bool clearedOnSuspension = page->model()->classCount() == 0
        && page->model()->visibleClassCount() == 0;

    *state = {
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("fixtureClassCount"), classCount},
        {QStringLiteral("image"), QFileInfo(imagePath).fileName()},
        {QStringLiteral("width"), image.width()},
        {QStringLiteral("height"), image.height()},
        {QStringLiteral("navigationCounts"), QJsonObject{
            {QStringLiteral("classRows"), classRows},
            {QStringLiteral("gradeGroupCount"), gradeGroupCount},
            {QStringLiteral("gradeGroups"), gradeGroups},
            {QStringLiteral("allGroupRows"), allRows},
            {QStringLiteral("listModelCount"), modelCount},
            {QStringLiteral("listViewCount"), listViewCount},
            {QStringLiteral("pageCount"), 1},
            {QStringLiteral("observedPageCount"), pageCount},
            {QStringLiteral("widgetCountWhileActive"), activeWidgetCount},
            {QStringLiteral("perClassWidgetCount"), perClassWidgetCount},
            {QStringLiteral("detailEditorCount"), editors},
            {QStringLiteral("classIdWidgetPropertyCount"), perClassWidgetCount},
            {QStringLiteral("modelClearedOnSuspension"), clearedOnSuspension},
            {QStringLiteral("widgetCountAfterSuspension"), widgetCount(page)},
            {QStringLiteral("perClassWidgetCountAfterSuspension"),
             classPropertyWidgetCount(page)},
            {QStringLiteral("detailEditorCountAfterSuspension"),
             detailEditorCount(page)}
        }},
        {QStringLiteral("readSizes"), QJsonObject{
            {QStringLiteral("listReadCalls"), listReads},
            {QStringLiteral("listResultClassRows"), listResultRows},
            {QStringLiteral("listResultClassNameUtf16CodeUnits"),
             static_cast<double>(classNameCodeUnits)},
            {QStringLiteral("navigationReadCalls"), navigationReads},
            {QStringLiteral("navigationQueryClassRows"), navigationQueryClassCount},
            {QStringLiteral("navigationQueryNameUtf16CodeUnits"),
             static_cast<double>(classNameCodeUnits)},
            {QStringLiteral("navigationResultClassRows"), navigationResultRows},
            {QStringLiteral("navigationResultSummaryUtf16CodeUnits"),
             static_cast<double>(navigationSummaryCodeUnits)},
            {QStringLiteral("maximumSummaryTokenUtf16CodeUnits"),
             maximumSummaryTokenCodeUnits},
            {QStringLiteral("navigationScheduleRowsTotal"), scheduleRows},
            {QStringLiteral("maximumScheduleRowsPerSourceAndClass"),
             maximumScheduleRowsPerSourceAndClass},
            {QStringLiteral("scheduleRowsPerSourceClassMaximum"),
             static_cast<int>(
                Application::kClassesNavigationMaximumScheduleRows)}
        }},
        {QStringLiteral("memoryProcessWideObservation"), QJsonObject{
            {QStringLiteral("scope"), QStringLiteral(
                "Whole-process observations; not page allocations or budgets")},
            {QStringLiteral("before"), memoryJson(beforeMemory)},
            {QStringLiteral("active"), memoryJson(activeMemory)},
            {QStringLiteral("suspended"), memoryJson(suspendedMemory)}
        }}
    };
    return clearedOnSuspension && modelCount == 1 && listViewCount == 1
        && perClassWidgetCount == 0 && editors == 0;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextClassesNavigatorVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create Classes capture output directory");
        return 2;
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
    const QString styles = QDir(QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT))
        .filePath(QStringLiteral("assets/styles"));
    const QString translations = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
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
                    qCritical("Could not load the production Korean translation catalog");
                    return 1;
                }
            }
            for (const int classCount : {6, 96})
            {
                QString error;
                QJsonObject state;
                if (!captureCase(loader, preferences.model, outputDirectory,
                                 locale, theme, classCount, &state, &error))
                {
                    qCritical().noquote() << error;
                    return 1;
                }
                captures.append(state);
            }
            if (translator) application.removeTranslator(translator.get());
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral(
            "Next Classes navigator visual captures and bounded navigation manifest")},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("style"), QStringLiteral("Fusion")},
        {QStringLiteral("platform"), QGuiApplication::platformName()},
        {QStringLiteral("fixtureSource"), QStringLiteral(
            "Synthetic bounded class list and navigation summaries injected through page read actions")},
        {QStringLiteral("readSizeMeaning"), QStringLiteral(
            "Callback query/result row counts; this capture harness does not issue SQL queries")},
        {QStringLiteral("translationSource"), QStringLiteral(
            "Compiled production ClassMngr_ko_KR.qm catalog")},
        {QStringLiteral("outputAdapter"), QStringLiteral("not applicable")},
        {QStringLiteral("memoryScope"), QStringLiteral(
            "Whole-process working-set/private/peak observations; not page allocations or budgets")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qCritical("Could not write the Classes capture manifest");
        return 1;
    }
    manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    return 0;
}
