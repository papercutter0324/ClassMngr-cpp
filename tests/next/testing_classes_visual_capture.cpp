#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/classes/ui/testing_classes_page.h"
#include "next/platform/workspace_store_testing_classes_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/testing_classes_manager_page.h"
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
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTranslator>
#include <QWidget>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};
constexpr int TestingClassCount = 96;
constexpr Domain::ScheduleTime::StartMinute PendingStartMinute{10 * 60};
const QString PendingDay = QStringLiteral("Thursday");
const QString FixtureNotes = QStringLiteral("Paired Testing Classes notes fixture");

enum class CaptureState
{
    Details,
    Notes,
    PendingSlot
};

[[nodiscard]] QString stateName(const CaptureState state)
{
    switch (state)
    {
    case CaptureState::Details: return QStringLiteral("details");
    case CaptureState::Notes: return QStringLiteral("notes");
    case CaptureState::PendingSlot: return QStringLiteral("pending_slot");
    }
    return QStringLiteral("unknown");
}

[[nodiscard]] QJsonValue pendingScheduleSlotJson(
    const std::optional<Application::TestingClassPendingScheduleSlot>& slot
    )
{
    if (!slot)
    {
        return QJsonValue(QJsonValue::Null);
    }

    QString weekday;
    switch (slot->weekday)
    {
    case Domain::Weekday::Monday: weekday = QStringLiteral("Monday"); break;
    case Domain::Weekday::Tuesday: weekday = QStringLiteral("Tuesday"); break;
    case Domain::Weekday::Wednesday: weekday = QStringLiteral("Wednesday"); break;
    case Domain::Weekday::Thursday: weekday = QStringLiteral("Thursday"); break;
    case Domain::Weekday::Friday: weekday = QStringLiteral("Friday"); break;
    case Domain::Weekday::Saturday: weekday = QStringLiteral("Saturday"); break;
    case Domain::Weekday::Sunday: weekday = QStringLiteral("Sunday"); break;
    }

    const int startMinute = slot->startMinute.rawValue();
    const QString startTime = QStringLiteral("%1:%2")
        .arg(startMinute / 60, 2, 10, QLatin1Char('0'))
        .arg(startMinute % 60, 2, 10, QLatin1Char('0'));
    return QJsonObject{
        {QStringLiteral("weekday"), weekday},
        {QStringLiteral("startMinute"), startMinute},
        {QStringLiteral("startTime"), startTime}
    };
}

[[nodiscard]] QPalette themePalette(const QString& theme)
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

[[nodiscard]] int widgetCount(const QWidget* widget)
{
    return widget ? widget->findChildren<QWidget*>().size() + 1 : 0;
}

[[nodiscard]] QJsonObject memoryJson(const ProcessMemorySnapshot& snapshot)
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

[[nodiscard]] bool saveImage(
    QWidget& widget,
    const QString& path,
    QString* error
    )
{
    const QImage image = widget.grab(QRect(QPoint(0, 0), Viewport)).toImage();
    if (image.size() == Viewport && image.save(path, "PNG"))
    {
        return true;
    }
    if (error)
    {
        *error = QStringLiteral("Could not save Testing Classes capture: %1")
            .arg(path);
    }
    return false;
}

[[nodiscard]] bool captureLegacy(
    const QString& databasePath,
    const int selectedClassId,
    const CaptureState state,
    const QString& outputPath,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    QJsonObject* result,
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

    TestingClassesPage page(&services);
    page.resize(Viewport);
    if (state == CaptureState::PendingSlot)
    {
        // Legacy carries this typed entry context privately in the page until
        // the new class is saved; its editor has no visible slot label.
        page.openTestingClass(-1, PendingDay, QStringLiteral("10:00"));
    }
    else
    {
        page.openTestingClass(selectedClassId);
    }
    page.refresh();
    if (QTabWidget* const tabs = page.findChild<QTabWidget*>(
            QStringLiteral("testingClassesTabs")))
    {
        tabs->setCurrentIndex(state == CaptureState::Notes ? 2 : 0);
    }
    page.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();

    if (state == CaptureState::Notes)
    {
        const QTextEdit* const notes = page.findChild<QTextEdit*>(
            QStringLiteral("testingClassNotesEdit"));
        if (!notes || notes->toPlainText() != FixtureNotes)
        {
            if (error) *error = QStringLiteral("Legacy Testing Notes did not load the seeded value.");
            return false;
        }
    }

    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    if (!saveImage(page, outputPath, error))
    {
        return false;
    }
    const int activeWidgets = widgetCount(&page);
    const QTabWidget* const tabs = page.findChild<QTabWidget*>(
        QStringLiteral("testingClassesTabs"));
    const int sectionIndex = tabs ? tabs->currentIndex() : -1;
    page.clearDatabaseState();
    services.closeDatabase();
    const ProcessMemorySnapshot cleared = memoryProvider.snapshot();
    *result = {
        {QStringLiteral("route"), QStringLiteral("Legacy TestingClassesPage")},
        {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("state"), stateName(state)},
        {QStringLiteral("viewportWidth"), Viewport.width()},
        {QStringLiteral("viewportHeight"), Viewport.height()},
        {QStringLiteral("selectedClassId"), state == CaptureState::PendingSlot
            ? QJsonValue(QJsonValue::Null) : QJsonValue(selectedClassId)},
        {QStringLiteral("testingClassCount"), TestingClassCount},
        {QStringLiteral("widgetCountWhileActive"), activeWidgets},
        {QStringLiteral("editorWidgetCount"), 1},
        {QStringLiteral("sectionIndex"), sectionIndex},
        {QStringLiteral("pendingScheduleSlot"), state == CaptureState::PendingSlot
            ? QJsonValue(QStringLiteral("Thursday 10:00; held in page state, no visible label"))
            : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("scope"), QStringLiteral(
                "Whole-process observations, not page allocation or memory budgets")},
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("afterClearAndClose"), memoryJson(cleared)}
        }}
    };
    return true;
}

[[nodiscard]] bool captureNext(
    const QString& databasePath,
    const Domain::ClassId& selectedClassId,
    const CaptureState state,
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& outputPath,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    QJsonObject* result,
    QString* error
    )
{
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    Ui::PageHost host(loader, preferences);
    Ui::PageDescriptor manager{
        .id = *Ui::PageId::fromString(QStringLiteral("testing_classes")),
        .label = QStringLiteral("Testing Classes"),
        .factory = [&databasePath](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            Ui::TestingClassesManagerPageActions actions;
            actions.workspaceIdentity = [&databasePath] { return databasePath; };
            actions.activeWorkspacePath = [&databasePath] {
                return std::optional<QString>(databasePath);
            };
            actions.readClasses = [](const QString& path) {
                return Platform::readTestingClassesAtPath(path);
            };
            actions.readDetails = [](const QString& path,
                const Domain::ClassId& id) {
                return Platform::readTestingClassDetailsAtPath(path, id);
            };
            actions.readTeacherChoices = [](const QString& path) {
                return Platform::readTestingTeacherChoicesAtPath(path);
            };
            actions.updateDetails = [](const QString& path,
                const Application::TestingClassDetailsUpdateRequest& request) {
                return Platform::updateTestingClassDetailsAtPath(path, request);
            };
            actions.createClass = [](const QString& path,
                const Application::TestingClassCreateRequest& request) {
                return Platform::createTestingClassAtPath(path, request);
            };
            actions.deleteClass = [](const QString& path,
                const Application::TestingClassDeleteRequest& request) {
                return Platform::deleteTestingClassAtPath(path, request);
            };
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::TestingClassesManagerPage>(
                    context, std::move(actions)));
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
    if (!host.registerPage(std::move(manager))
        || !host.registerPage(std::move(other))
        || !host.activatePage(QStringLiteral("testing_classes")))
    {
        if (error) *error = QStringLiteral("Next Testing Classes could not activate.");
        return false;
    }
    auto* const page = dynamic_cast<Ui::TestingClassesManagerPage*>(
        host.pageInstance(QStringLiteral("testing_classes")));
    if (!page)
    {
        if (error) *error = QStringLiteral("Next Testing Classes page is missing.");
        return false;
    }
    if (state != CaptureState::PendingSlot
        && page->selectedClassId() != std::optional<Domain::ClassId>(selectedClassId))
    {
        if (error) *error = QStringLiteral("Next selected a different fixture class.");
        return false;
    }
    if (state == CaptureState::Notes)
    {
        page->sections()->setCurrentIndex(1);
    }
    const std::optional<Application::TestingClassPendingScheduleSlot> pendingSlot =
        state == CaptureState::PendingSlot
            ? std::optional<Application::TestingClassPendingScheduleSlot>(
                Application::TestingClassPendingScheduleSlot{
                    .weekday = Domain::Weekday::Thursday,
                    .startMinute = PendingStartMinute
                })
            : std::nullopt;
    if (pendingSlot)
    {
        page->openNewClassForPendingSlot(pendingSlot);
    }
    host.resize(Viewport);
    host.show();
    QApplication::processEvents();
    if (state != CaptureState::PendingSlot
        && (page->nameEdit()->text().isEmpty() || !page->hasLoadedDetails()))
    {
        if (error) *error = QStringLiteral("Next selected class details did not load.");
        return false;
    }
    if (state == CaptureState::Notes
        && page->notesEdit()->toPlainText() != FixtureNotes)
    {
        if (error) *error = QStringLiteral("Next Testing Notes did not load the seeded value.");
        return false;
    }
    if (state == CaptureState::PendingSlot
        && page->pendingSlotLabel()->text().isEmpty())
    {
        if (error) *error = QStringLiteral("Next pending slot context is not visible.");
        return false;
    }
    const QString pendingSlotDisplayText = state == CaptureState::PendingSlot
        ? page->pendingSlotLabel()->text() : QString{};
    bool koreanLabelsVerified = false;
    if (QLocale().name().startsWith(QStringLiteral("ko")))
    {
        const auto labelText = [page](const QString& objectName) {
            const QLabel* const label = page->findChild<QLabel*>(objectName);
            return label ? label->text() : QString{};
        };
        const bool labelsTranslated =
            labelText(QStringLiteral("testingClassesTitle")) == QStringLiteral("테스트 수업")
            && labelText(QStringLiteral("testingClassesSubtitle"))
                == QStringLiteral("테스트 일정에서 사용할 수업을 관리합니다.")
            && labelText(QStringLiteral("testingClassNameLabel")) == QStringLiteral("수업 이름")
            && labelText(QStringLiteral("testingClassGradeLabel")) == QStringLiteral("학년")
            && labelText(QStringLiteral("testingClassLevelLabel")) == QStringLiteral("레벨")
            && labelText(QStringLiteral("testingClassRoomLabel")) == QStringLiteral("교실")
            && labelText(QStringLiteral("testingClassTeacherLabel")) == QStringLiteral("한국인 교사")
            && labelText(QStringLiteral("testingClassColorLabel")) == QStringLiteral("수업 색상")
            && labelText(QStringLiteral("testingClassFontColorLabel")) == QStringLiteral("글자 색상")
            && page->sections()->tabText(0) == QStringLiteral("세부 정보")
            && page->sections()->tabText(1) == QStringLiteral("노트")
            && page->addButton()->text() == QStringLiteral("수업 추가")
            && page->deleteButton()->text() == QStringLiteral("수업 삭제")
            && page->saveButton()->text() == QStringLiteral("변경 내용 저장")
            && page->teacherChoice()->itemText(0) == QStringLiteral("교사 없음")
            && page->nameEdit()->placeholderText()
                == QStringLiteral("테스트 일정에 표시됩니다")
            && page->roomEdit()->placeholderText() == QStringLiteral("교실")
            && page->notesEdit()->placeholderText()
                == QStringLiteral("안내 및 기타 노트");
        const bool pendingTranslated = state != CaptureState::PendingSlot
            || page->pendingSlotLabel()->text()
                == QStringLiteral("목요일 10:00에 수업 추가");
        if (!labelsTranslated || !pendingTranslated)
        {
            if (error) *error = QStringLiteral(
                "The Korean Next Testing Classes labels or pending-slot text remained untranslated.");
            return false;
        }
        koreanLabelsVerified = true;
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

    const int activeWidgets = widgetCount(page);
    const int editorCount = page->findChildren<QWidget*>(
        QStringLiteral("testingClassesEditor")).size();
    const QString selectedId = page->selectedClassId()
        ? QString::fromStdString(page->selectedClassId()->value()) : QString{};
    const bool newClassDraftWhileVisible = page->isNewDraft();
    if (!host.activatePage(QStringLiteral("other")))
    {
        if (error) *error = QStringLiteral("Next page did not suspend after capture.");
        return false;
    }
    QApplication::processEvents();
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    const bool clearedOnSuspension = !page->hasLoadedDetails()
        && page->nameEdit()->text().isEmpty();
    *result = {
        {QStringLiteral("route"), QStringLiteral("Next TestingClassesManagerPage")},
        {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("state"), stateName(state)},
        {QStringLiteral("viewportWidth"), Viewport.width()},
        {QStringLiteral("viewportHeight"), Viewport.height()},
        {QStringLiteral("selectedClassId"), selectedId.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(selectedId)},
        {QStringLiteral("testingClassCount"), page->model()->classCount()},
        {QStringLiteral("widgetCountWhileActive"), activeWidgets},
        {QStringLiteral("selectedEditorWidgetCount"), editorCount},
        {QStringLiteral("selectedEditorDescendantCount"),
            page->editorWidget()->findChildren<QWidget*>().size()},
        {QStringLiteral("koreanLabelsVerified"), koreanLabelsVerified},
        {QStringLiteral("newClassDraft"), newClassDraftWhileVisible},
        {QStringLiteral("pendingScheduleSlot"), pendingScheduleSlotJson(pendingSlot)},
        {QStringLiteral("pendingScheduleSlotDisplayText"), pendingSlotDisplayText.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(pendingSlotDisplayText)},
        {QStringLiteral("detailsSnapshotClearedOnSuspension"), clearedOnSuspension},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("scope"), QStringLiteral(
                "Whole-process observations, not page allocation or memory budgets")},
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("suspended"), memoryJson(suspended)}
        }}
    };
    return editorCount == 1 && clearedOnSuspension;
}

[[nodiscard]] bool seedFixture(const QString& path, QString* error)
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        if (error) *error = QStringLiteral("Could not open paired fixture workspace.");
        return false;
    }
    QSqlQuery query(store.database());
    if (!query.exec(QStringLiteral(
            "INSERT INTO testing_classes(class_id,room) "
            "SELECT id,'Room 101' FROM classes")))
    {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral(
            "UPDATE class_info SET notes='Paired Testing Classes notes fixture'")))
    {
        if (error) *error = query.lastError().text();
        return false;
    }
    query.finish();
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM testing_classes"))
        || !query.next() || query.value(0).toInt() != TestingClassCount)
    {
        if (error) *error = QStringLiteral(
            "The paired source fixture did not contain the expected 96 testing-class rows.");
        return false;
    }
    return true;
}

[[nodiscard]] bool capturePair(
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
        QStringLiteral("testing-classes-paired.tps"));
    if (!QFile::copy(sourceFixture, databasePath)
        || !seedFixture(databasePath, error))
    {
        return false;
    }
    const auto seededChoices = Platform::readTestingClassesAtPath(databasePath);
    if (!seededChoices || seededChoices.value().choices.size() != TestingClassCount)
    {
        if (error) *error = QStringLiteral("Could not load seeded testing class choices.");
        return false;
    }
    Ui::TestingClassesModel orderingModel;
    orderingModel.setSnapshot(seededChoices.value());
    const auto selected = orderingModel.classIdAt(orderingModel.index(0, 0));
    if (!selected)
    {
        if (error) *error = QStringLiteral("Could not choose a testing class for capture.");
        return false;
    }
    const Domain::ClassId selectedClassId = *selected;
    bool validLegacyId = false;
    const int legacySelectedId = QString::fromStdString(
        selectedClassId.value()).toInt(&validLegacyId);
    if (!validLegacyId || legacySelectedId <= 0)
    {
        if (error) *error = QStringLiteral("The selected testing class ID is invalid.");
        return false;
    }

    for (const CaptureState state : {
            CaptureState::Details, CaptureState::Notes, CaptureState::PendingSlot})
    {
        const QString key = QStringLiteral("%1_%2_%3")
            .arg(stateName(state), locale, theme);
        const QString legacyImage = QDir(outputDirectory).filePath(
            QStringLiteral("testing_classes_%1_legacy.png").arg(key));
        const QString nextImage = QDir(outputDirectory).filePath(
            QStringLiteral("testing_classes_%1_next.png").arg(key));
        QJsonObject legacy;
        if (!captureLegacy(databasePath, legacySelectedId, state, legacyImage,
                memoryProvider, &legacy, error))
        {
            return false;
        }
        QJsonObject next;
        if (!captureNext(databasePath, selectedClassId, state, loader, preferences,
                nextImage, memoryProvider, &next, error))
        {
            return false;
        }
        const bool sameSelectedClass = state == CaptureState::PendingSlot
            || (legacy.value(QStringLiteral("selectedClassId")).toInt()
                == QString::fromStdString(selectedClassId.value()).toInt());
        captures->append(QJsonObject{
            {QStringLiteral("locale"), locale},
            {QStringLiteral("theme"), theme},
            {QStringLiteral("state"), stateName(state)},
            {QStringLiteral("fixture"), QFileInfo(sourceFixture).fileName()},
            {QStringLiteral("fixtureRegularClassCount"), TestingClassCount},
            {QStringLiteral("seededTestingClassCount"), TestingClassCount},
            {QStringLiteral("selectedClassId"), state == CaptureState::PendingSlot
                ? QJsonValue(QJsonValue::Null)
                : QJsonValue(QString::fromStdString(selectedClassId.value()))},
            {QStringLiteral("legacy"), legacy},
            {QStringLiteral("next"), next},
            {QStringLiteral("sameSelectedClass"), sameSelectedClass}
        });
        if (!sameSelectedClass)
        {
            if (error) *error = QStringLiteral("Paired routes selected different class IDs.");
            return false;
        }
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
        qCritical("Usage: ClassMngrNextTestingClassesVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create Testing Classes capture directory");
        return 2;
    }
    const QString sourceFixture = QStringLiteral(CLASSMNGR_TESTING_CLASSES_FIXTURE);
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
        qCritical().noquote() << "Could not load Next resource catalog:"
                              << loaded.error().message;
        return 1;
    }
    Resources::ResourceLoader loader(std::move(*loaded));
    Test::MemoryPreferences preferences;
    PlatformProcessMemorySnapshotProvider memoryProvider;
    QJsonArray captures;
    const QString translations = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
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
                    qCritical("Could not install Korean Testing Classes translator");
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
            "Paired Legacy and Next Testing Classes Details, Notes, and pending-slot entry states")},
        {QStringLiteral("fixture"), QFileInfo(sourceFixture).fileName()},
        {QStringLiteral("fixtureRegularClassCount"), TestingClassCount},
        {QStringLiteral("seededTestingClassCount"), TestingClassCount},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("captureLimitation"), QStringLiteral(
            "The legacy manager retains pending weekday/start only inside page state; Next displays the pending tuple. Both screenshots use the same Thursday 10:00 create context and 1024x768 viewport. Screenshots are evidence for independent visual inspection, not a parity claim.")},
        {QStringLiteral("memoryLimitation"), QStringLiteral(
            "Memory snapshots are whole-process working/private observations; they do not isolate widget allocation or establish a memory budget.")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("testing_classes_paired_capture.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || manifestFile.write(QJsonDocument(manifest).toJson(
            QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write the paired Testing Classes capture manifest");
        return 1;
    }
    return 0;
}
