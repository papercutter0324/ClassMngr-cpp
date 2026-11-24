#include "core/process_memory_snapshot.h"
#include "next/application/classes_navigator_preferences.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/classes_navigator_page.h"
#include "next/ui/page_host.h"
#include "next/ui/selected_class_editor.h"
#include "next/bootstrap/qt_page_lifecycle_diagnostics.h"
#include "preferences_test_ports.h"
#include "splitter_state_test_port.h"

#include <QApplication>
#include <QCryptographicHash>
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
#include <QPointer>
#include <QTranslator>
#include <QListView>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTableWidget>

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

constexpr QSize Viewport{1068, 768};

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

QByteArray sha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256)
        .toHex();
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

int sharedDetailEditorCount(const QWidget* root)
{
    if (!root)
    {
        return 0;
    }
    return root->findChildren<QWidget*>(
        QStringLiteral("selectedClassEditor")).size();
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
    int classesFactoryCalls = 0;
    int scheduleRows = 0;

    Ui::PageHost host(loader, preferences);
    std::vector<QJsonObject> lifecycleTrace;
    QJsonArray p919SelectionSweepCheckpoints;
    Bootstrap::QtPageLifecycleDiagnosticsRecorder lifecycleRecorder(
        host, {}, [&lifecycleTrace](const QByteArray& record) {
            lifecycleTrace.push_back(QJsonDocument::fromJson(record).object());
        });
    Ui::PageDescriptor classesPage{
        .id = *Ui::PageId::fromString(QStringLiteral("classes")),
        .label = QStringLiteral("Classes"),
        .factory = [&classes, &navigation, &listReads, &navigationReads,
                    &navigationQueryClassCount, &classesFactoryCalls,
                    &lifecycleRecorder](
            const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            ++classesFactoryCalls;
            Ui::ClassesNavigatorPageActions actions;
            actions.diagnosticCheckpoint = [&lifecycleRecorder](
                const QString& checkpoint,
                const QString& outcome,
                const QJsonObject& details) {
                lifecycleRecorder.recordPageActionCheckpoint(
                    QStringLiteral("classes"), checkpoint, outcome, details);
            };
            actions.splitterStatePort = std::make_shared<
                Test::MemorySplitterStatePort>();
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
    QPointer<Ui::Page> originalPage(page);
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

    bool p919SelectionSweepSummaryMatches = classCount != 96;
    if (classCount == 96)
    {
        for (int row = 1; row < 96; ++row)
        {
            page->classList()->setCurrentIndex(page->model()->index(row, 0));
        }
        for (const QJsonObject& event : lifecycleTrace)
        {
            if (event.value(QStringLiteral("event")).toString()
                    != QStringLiteral("page-action-checkpoint")
                || event.value(QStringLiteral("page_key")).toString()
                    != QStringLiteral("classes")
                || event.value(QStringLiteral("checkpoint")).toString()
                    != QStringLiteral("p9-19-class-selection-sweep"))
            {
                continue;
            }
            const QJsonObject details = event.value(
                QStringLiteral("details")).toObject();
            p919SelectionSweepCheckpoints.append(details);
            p919SelectionSweepSummaryMatches =
                details.value(QStringLiteral("schema")).toString()
                    == QStringLiteral(
                        "classmngr-phase9-p9-19-class-selection-sweep-v1")
                && details.value(QStringLiteral("class_count")).toInt() == 96
                && details.value(QStringLiteral("visible_class_count")).toInt()
                    == 96
                && details.value(QStringLiteral(
                    "selected_unique_class_count")).toInt() == 96
                && details.value(QStringLiteral(
                    "selection_observation_count")).toInt() == 96
                && details.value(QStringLiteral(
                    "selected_class_editor_count_minimum")).toInt() == 1
                && details.value(QStringLiteral(
                    "selected_class_editor_count_maximum")).toInt() == 1
                && details.value(QStringLiteral(
                    "selected_class_editor_stable_within_sweep")).toBool()
                && details.value(QStringLiteral(
                    "selected_class_editor_stable_across_sweeps")).toBool()
                && details.value(QStringLiteral(
                    "non_roster_widget_count_maximum")).toInt() < 250
                && details.value(QStringLiteral(
                    "class_id_child_widget_count_maximum")).toInt() == 0;
        }
        p919SelectionSweepSummaryMatches = p919SelectionSweepSummaryMatches
            && p919SelectionSweepCheckpoints.size() == 1;
    }

    const ProcessMemorySnapshot activeMemory = memoryProvider.snapshot();
    const QImage image = host.grab().toImage();
    const QString key = QStringLiteral("%1_%2_%3")
        .arg(locale, theme, classCount == 96 ? QStringLiteral("96")
                                             : QStringLiteral("small"));
    const QString imagePath = QDir(outputDirectory).filePath(
        QStringLiteral("classes_%1.png").arg(key));
    const bool hasExpectedViewport = image.size() == Viewport;
    const bool imageSaved = hasExpectedViewport && image.save(imagePath, "PNG");
    if (!imageSaved)
    {
        if (error) *error = QStringLiteral(
            "A Classes visual capture could not be saved (image %1x%2, expected %3x%4, path %5).")
                .arg(image.width()).arg(image.height())
                .arg(Viewport.width()).arg(Viewport.height()).arg(imagePath);
        return false;
    }
    const QByteArray imageHash = sha256(imagePath);
    if (imageHash.isEmpty())
    {
        if (error) *error = QStringLiteral(
            "Could not hash the saved Classes capture: %1").arg(imagePath);
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
    const int editors = sharedDetailEditorCount(page);
    const int listResultRows = classRows;
    const int navigationResultRows = static_cast<int>(navigation.classes.size());
    const int initialListReads = listReads;
    const int initialNavigationReads = navigationReads;
    const int initialNavigationQueryClassCount = navigationQueryClassCount;
    const int initialClassesFactoryCalls = classesFactoryCalls;

    if (!host.activatePage(QStringLiteral("other")))
    {
        if (error) *error = QStringLiteral("The Classes page did not suspend.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot suspendedMemory = memoryProvider.snapshot();
    const bool clearedOnSuspension = page->model()->classCount() == 0
        && page->model()->visibleClassCount() == 0;
    const bool perClassWidgetsClearedOnSuspension =
        classPropertyWidgetCount(page) == 0;
    const bool editorClearedOnSuspension = page->selectedClassEditor()
        && !page->selectedClassEditor()->isLoaded()
        && !page->selectedClassEditor()->classId().has_value()
        && page->selectedClassEditor()->regularSchedule()
        && page->selectedClassEditor()->regularSchedule()->rowCount() == 0
        && page->selectedClassEditor()->intensiveSchedule()
        && page->selectedClassEditor()->intensiveSchedule()->rowCount() == 0;
    const int widgetCountAfterSuspension = widgetCount(page);
    const int perClassWidgetCountAfterSuspension = classPropertyWidgetCount(page);
    const int editorCountAfterSuspension = sharedDetailEditorCount(page);
    if (!host.releasePage(QStringLiteral("classes")))
    {
        if (error) *error = QStringLiteral("The suspended Classes page did not release.");
        return false;
    }
    const bool releasedPageDestroyed = originalPage.isNull()
        && !host.hasPageInstance(QStringLiteral("classes"));
    const ProcessMemorySnapshot releasedMemory = memoryProvider.snapshot();

    if (!host.activatePage(QStringLiteral("classes")))
    {
        if (error) *error = QStringLiteral("The Classes page did not recreate.");
        return false;
    }
    QApplication::processEvents();
    auto* const recreatedPage = dynamic_cast<Ui::ClassesNavigatorPage*>(
        host.pageInstance(QStringLiteral("classes")));
    if (!recreatedPage)
    {
        if (error) *error = QStringLiteral("The Classes page was not recreated as expected.");
        return false;
    }
    const ProcessMemorySnapshot recreatedMemory = memoryProvider.snapshot();
    const bool pageWasRecreated = originalPage.isNull()
        && recreatedPage != originalPage.data()
        && classesFactoryCalls == initialClassesFactoryCalls + 1;
    QPointer<Ui::Page> recreatedPageLifetime(recreatedPage);
    const int recreatedClassRows = recreatedPage->model()->classCount();
    const int recreatedPerClassWidgetCount = classPropertyWidgetCount(recreatedPage);
    const int recreatedEditorCount = sharedDetailEditorCount(recreatedPage);
    const bool recreationCountsMatch = pageWasRecreated
        && recreatedClassRows == classCount
        && recreatedPerClassWidgetCount == 0
        && recreatedEditorCount == 1
        && recreatedPage->selectedClassEditor()
        && recreatedPage->selectedClassEditor()->regularSchedule()
        && recreatedPage->selectedClassEditor()->intensiveSchedule();

    if (!host.activatePage(QStringLiteral("other")))
    {
        if (error) *error = QStringLiteral("The recreated Classes page did not suspend.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot recreatedSuspendedMemory = memoryProvider.snapshot();
    const bool recreatedStateCleared =
        recreatedPage->model()->classCount() == 0
        && recreatedPage->model()->visibleClassCount() == 0
        && classPropertyWidgetCount(recreatedPage) == 0
        && recreatedPage->selectedClassEditor()
        && !recreatedPage->selectedClassEditor()->isLoaded()
        && !recreatedPage->selectedClassEditor()->classId().has_value()
        && recreatedPage->selectedClassEditor()->regularSchedule()
        && recreatedPage->selectedClassEditor()->regularSchedule()->rowCount() == 0
        && recreatedPage->selectedClassEditor()->intensiveSchedule()
        && recreatedPage->selectedClassEditor()->intensiveSchedule()->rowCount() == 0;
    if (!host.releasePage(QStringLiteral("classes")))
    {
        if (error) *error = QStringLiteral("The recreated Classes page did not release.");
        return false;
    }
    const bool recreatedPageDestroyed =
        recreatedPageLifetime.isNull()
        && !host.hasPageInstance(QStringLiteral("classes"));
    const ProcessMemorySnapshot releasedAfterRecreateMemory = memoryProvider.snapshot();
    if (!host.releasePage(QStringLiteral("other")))
    {
        if (error) *error = QStringLiteral("The auxiliary page did not release.");
        return false;
    }

    QJsonArray classLifecycleStates;
    QJsonArray lifecycleTraceArray;
    bool lifecycleAvailabilityMatches = true;
    int classDestroyedEventCount = 0;
    bool destroyedMetricsUnavailable = true;
    for (const QJsonObject& event : lifecycleTrace)
    {
        lifecycleTraceArray.append(event);
        if (event.value(QStringLiteral("page_key")).toString()
            != QStringLiteral("classes"))
        {
            continue;
        }
        const QString eventName = event.value(QStringLiteral("event")).toString();
        if (eventName == QStringLiteral("page-lifecycle"))
        {
            classLifecycleStates.append(event.value(QStringLiteral("state")));
            const QString stateName = event.value(QStringLiteral("state")).toString();
            const bool instanceAvailable = event.value(QStringLiteral(
                "page_instance_available")).toBool();
            lifecycleAvailabilityMatches = lifecycleAvailabilityMatches
                && (stateName == QStringLiteral("activated")
                        || stateName == QStringLiteral("suspended")
                    ? instanceAvailable
                    : !instanceAvailable);
        }
        else if (eventName == QStringLiteral("page-instance-destroyed"))
        {
            ++classDestroyedEventCount;
            destroyedMetricsUnavailable = destroyedMetricsUnavailable
                && !event.value(QStringLiteral(
                    "page_instance_available")).toBool()
                && event.value(QStringLiteral("page")).toObject()
                    .value(QStringLiteral("widget_count")).isNull()
                && event.value(QStringLiteral("sample_available")).toBool();
        }
    }
    const QJsonArray expectedClassLifecycleStates{
        QStringLiteral("created"), QStringLiteral("activated"),
        QStringLiteral("suspended"), QStringLiteral("released"),
        QStringLiteral("created"), QStringLiteral("activated"),
        QStringLiteral("suspended"), QStringLiteral("released")
    };
    const bool lifecycleTraceMatches = classLifecycleStates
            == expectedClassLifecycleStates
        && lifecycleAvailabilityMatches;
    const bool destructionTraceMatches = classDestroyedEventCount == 2
        && destroyedMetricsUnavailable;

    *state = {
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("fixtureClassCount"), classCount},
        {QStringLiteral("image"), QFileInfo(imagePath).fileName()},
        {QStringLiteral("sha256"), QString::fromLatin1(imageHash)},
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
            {QStringLiteral("p919SelectionSweepSummaryMatches"),
             p919SelectionSweepSummaryMatches},
            {QStringLiteral("p919SelectionSweepCheckpoints"),
             p919SelectionSweepCheckpoints},
            {QStringLiteral("modelClearedOnSuspension"), clearedOnSuspension},
            {QStringLiteral("widgetCountAfterSuspension"),
             widgetCountAfterSuspension},
            {QStringLiteral("perClassWidgetCountAfterSuspension"),
             perClassWidgetCountAfterSuspension},
            {QStringLiteral("detailEditorCountAfterSuspension"),
             editorCountAfterSuspension},
            {QStringLiteral("perClassWidgetsClearedOnSuspension"),
             perClassWidgetsClearedOnSuspension},
            {QStringLiteral("editorClearedOnSuspension"),
             editorClearedOnSuspension},
            {QStringLiteral("releasedPageDestroyed"), releasedPageDestroyed},
            {QStringLiteral("pageWasRecreated"), pageWasRecreated},
            {QStringLiteral("recreatedClassRows"), recreatedClassRows},
            {QStringLiteral("recreatedPerClassWidgetCount"),
             recreatedPerClassWidgetCount},
            {QStringLiteral("recreatedEditorCount"), recreatedEditorCount},
            {QStringLiteral("recreationCountsMatch"), recreationCountsMatch},
            {QStringLiteral("recreatedStateCleared"), recreatedStateCleared},
            {QStringLiteral("recreatedPageDestroyed"), recreatedPageDestroyed},
            {QStringLiteral("pageLifecycleTraceMatches"), lifecycleTraceMatches},
            {QStringLiteral("classDestroyedEventCount"),
             classDestroyedEventCount},
            {QStringLiteral("destructionTraceMatches"), destructionTraceMatches},
            {QStringLiteral("pageLifecycleStates"), classLifecycleStates},
            {QStringLiteral("pageLifecycleTrace"), lifecycleTraceArray}
        }},
        {QStringLiteral("readSizes"), QJsonObject{
            {QStringLiteral("listReadCalls"), initialListReads},
            {QStringLiteral("listReadCallsThroughRecreate"), listReads},
            {QStringLiteral("listResultClassRows"), listResultRows},
            {QStringLiteral("listResultClassNameUtf16CodeUnits"),
             static_cast<double>(classNameCodeUnits)},
            {QStringLiteral("navigationReadCalls"), initialNavigationReads},
            {QStringLiteral("navigationReadCallsThroughRecreate"),
             navigationReads},
            {QStringLiteral("navigationQueryClassRows"),
             initialNavigationQueryClassCount},
            {QStringLiteral("navigationQueryClassRowsThroughRecreate"),
             navigationQueryClassCount},
            {QStringLiteral("classesPageFactoryCalls"), classesFactoryCalls},
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
            {QStringLiteral("suspended"), memoryJson(suspendedMemory)},
            {QStringLiteral("released"), memoryJson(releasedMemory)},
            {QStringLiteral("recreated"), memoryJson(recreatedMemory)},
            {QStringLiteral("recreatedSuspended"),
             memoryJson(recreatedSuspendedMemory)},
            {QStringLiteral("releasedAfterRecreate"),
             memoryJson(releasedAfterRecreateMemory)}
        }}
    };
    const bool countsMatch = clearedOnSuspension
        && perClassWidgetsClearedOnSuspension
        && editorClearedOnSuspension
        && releasedPageDestroyed
        && recreationCountsMatch
        && recreatedStateCleared
        && recreatedPageDestroyed
        && lifecycleTraceMatches
        && p919SelectionSweepSummaryMatches
        && destructionTraceMatches
        && modelCount == 1
        && listViewCount == 1 && perClassWidgetCount == 0 && editors == 1
        && editorCountAfterSuspension == 1;
    if (!countsMatch && error)
    {
        *error = QStringLiteral(
            "Classes capture lifecycle counts differed "
            "(cleared=%1, widgets=%2, editor=%3, released=%4, recreated=%5, "
            "rows=%6, recleared=%7, trace=%8, destroyed=%9/%10, "
            "models=%11, lists=%12, per-class widgets=%13, editors=%14).")
                .arg(clearedOnSuspension)
                .arg(perClassWidgetsClearedOnSuspension)
                .arg(editorClearedOnSuspension)
                .arg(releasedPageDestroyed)
                .arg(recreationCountsMatch)
                .arg(recreatedClassRows)
                .arg(recreatedStateCleared)
                .arg(lifecycleTraceMatches)
                .arg(classDestroyedEventCount)
                .arg(destroyedMetricsUnavailable)
                .arg(modelCount)
                .arg(listViewCount)
                .arg(perClassWidgetCount)
                .arg(editors);
    }
    return countsMatch;
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
