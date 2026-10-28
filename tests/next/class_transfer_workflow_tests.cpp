#include "data/database/workspace_store.h"
#include "data/repositories/class_transfer_legacy_package_adapter.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"
#include "core/process_memory_snapshot.h"
#include "domain/models/teacher.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "next/platform/workspace_store_class_transfer_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/class_transfer_workflow_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QListView>
#include <QJsonDocument>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

[[nodiscard]] int createCompleteSourceClass(WorkspaceStore& store)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김가람");
    teacher.teacherEn = QStringLiteral("Garam Kim");
    teacher.preferredRomanization = QStringLiteral("Garam Kim");
    teacher.preferredName = QStringLiteral("Garam Kim");
    teacher.roomNumber = QStringLiteral("Room 12");
    teacher.birthday = QStringLiteral("03-04");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.wifiName = QStringLiteral("Transfer WiFi");
    teacher.wifiPassword = QStringLiteral("test-wifi-secret");
    teacher.internetType = QStringLiteral("WiFi");
    teacher.zoomId = QStringLiteral("transfer-zoom");
    teacher.zoomPassword = QStringLiteral("test-zoom-secret");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.notes = QStringLiteral("Teacher package note");
    constexpr qint64 wideTeacherId = 4'000'000'001LL;
    QSqlQuery insertTeacher(store.database());
    insertTeacher.prepare(QStringLiteral(
        "INSERT INTO teachers (id,teacher_kr,teacher_en,preferred_romanization,"
        "preferred_name,room_number,birthday,phone_number,wifi_name,wifi_password,"
        "internet_type,zoom_id,zoom_password,projection_type,notes) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    insertTeacher.addBindValue(wideTeacherId);
    insertTeacher.addBindValue(teacher.teacherKr);
    insertTeacher.addBindValue(teacher.teacherEn);
    insertTeacher.addBindValue(teacher.preferredRomanization);
    insertTeacher.addBindValue(teacher.preferredName);
    insertTeacher.addBindValue(teacher.roomNumber);
    insertTeacher.addBindValue(teacher.birthday);
    insertTeacher.addBindValue(teacher.phoneNumber);
    insertTeacher.addBindValue(teacher.wifiName);
    insertTeacher.addBindValue(teacher.wifiPassword);
    insertTeacher.addBindValue(teacher.internetType);
    insertTeacher.addBindValue(teacher.zoomId);
    insertTeacher.addBindValue(teacher.zoomPassword);
    insertTeacher.addBindValue(teacher.projectionType);
    insertTeacher.addBindValue(teacher.notes);
    if (!insertTeacher.exec())
    {
        return -1;
    }

    const auto created = store.classRepository()->createClass(
        QStringLiteral("Portable class"));
    if (!created)
    {
        return -1;
    }
    ClassInfo info;
    info.classId = created.value();
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.readingBook = QStringLiteral("Reading Explorer 1");
    info.essayBook = QStringLiteral("4A");
    info.classColor = QStringLiteral("#DDEBFF");
    info.fontColor = QStringLiteral("#000000");
    info.notes = QStringLiteral("Class notes round-trip");
    info.timeFillerActivities = QStringLiteral("Filler activity round-trip");
    info.teacherEn = teacher.teacherEn;
    info.teacherKr = teacher.teacherKr;
    info.classTimes = {{QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")}};
    info.intensiveTimes = {{QStringLiteral("Tuesday"), QStringLiteral("5:00 PM"),
        QStringLiteral("5:50 PM")}};
    if (!store.classInfoRepository()->saveClassDetailsDetailed(info, wideTeacherId))
    {
        return -1;
    }

    Roster roster;
    roster.columns = {QStringLiteral("English Name"), QStringLiteral("Korean Name"),
        QStringLiteral("Notes")};
    roster.columnWidths = {145, 145, 220};
    roster.rows = {{QStringLiteral("Ada Student"), QStringLiteral("학생 에이다"),
        QStringLiteral("Roster cell round-trip")}};
    if (!store.rosterRepository()->saveRoster(created.value(), roster))
    {
        return -1;
    }

    SpeakingEvalRows evaluation = SpeakingEval::emptyRows();
    evaluation[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Ada Student");
    evaluation[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("학생 에이다");
    evaluation[0][SpeakingEval::toInt(SpeakingEvalColumn::Grammar)] =
        QStringLiteral("A");
    evaluation[0][SpeakingEval::toInt(SpeakingEvalColumn::Comments)] =
        QStringLiteral("Evaluation round-trip");
    if (!store.speakingEvalRepository()->saveSpeakingEval(
            created.value(), QStringLiteral("Winter"), evaluation))
    {
        return -1;
    }
    return created.value();
}

[[nodiscard]] int countRows(WorkspaceStore& store, const QString& table)
{
    QSqlQuery query(store.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

[[nodiscard]] int createScheduleConflictAnchor(WorkspaceStore& store)
{
    const auto created = store.classRepository()->createClass(
        QStringLiteral("Conflict anchor"));
    if (!created)
    {
        return -1;
    }
    ClassInfo info;
    info.classId = created.value();
    info.classGrade = QStringLiteral("M2");
    info.classLevel = QStringLiteral("Ursa");
    info.classColor = QStringLiteral("#FFFFFF");
    info.fontColor = QStringLiteral("#000000");
    info.classTimes = {{QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")}};
    return store.classInfoRepository()->saveClassDetailsDetailed(info, -1)
        ? created.value() : -1;
}

[[nodiscard]] Teacher matchingTeacher()
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김가람");
    teacher.teacherEn = QStringLiteral("Garam Kim");
    teacher.preferredRomanization = QStringLiteral("Garam Kim");
    teacher.preferredName = QStringLiteral("Garam Kim");
    teacher.roomNumber = QStringLiteral("Room 12");
    teacher.birthday = QStringLiteral("03-04");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.wifiName = QStringLiteral("Transfer WiFi");
    teacher.wifiPassword = QStringLiteral("test-wifi-secret");
    teacher.internetType = QStringLiteral("WiFi");
    teacher.zoomId = QStringLiteral("transfer-zoom");
    teacher.zoomPassword = QStringLiteral("test-zoom-secret");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.notes = QStringLiteral("Teacher package note");
    return teacher;
}

struct PageHarness final
{
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    QString path;
    std::optional<Domain::ClassId> selectedClass;

    [[nodiscard]] bool initialize()
    {
        const auto loaded = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
        if (!loaded)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(std::move(*loaded));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);
        Ui::PageDescriptor transfer{
            .id = *Ui::PageId::fromString(QStringLiteral("class_transfer")),
            .label = QStringLiteral("Class Transfer"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::ClassTransferWorkflowPageActions actions;
                actions.activeWorkspacePath = [this]() -> std::optional<QString> {
                    return path.isEmpty() ? std::nullopt
                        : std::optional<QString>(path);
                };
                actions.chooseImportFile = [] {
                    return Domain::Result<std::optional<QString>>::success(
                        std::nullopt);
                };
                actions.chooseExportFile = [] {
                    return Domain::Result<std::optional<QString>>::success(
                        std::nullopt);
                };
                actions.readClasses = [](const QString& activePath) {
                    return Platform::readClassTransferClassesAtPath(activePath);
                };
                actions.exportClasses = [](
                    const QString& activePath,
                    const Application::ClassTransferExportRequest& request) {
                    return Platform::exportClassTransferAtPath(activePath, request);
                };
                actions.previewImport = [](
                    const QString& activePath,
                    const Application::ClassTransferPreviewRequest& request) {
                    return Platform::previewClassTransferAtPath(activePath, request);
                };
                actions.applyImport = [](
                    const QString& activePath,
                    Application::ClassTransferApplyCommand request) {
                    return Platform::applyClassTransferAtPath(
                        activePath, std::move(request));
                };
                actions.selectImportedClass = [this](const Domain::ClassId& id) {
                    selectedClass = id;
                };
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::ClassTransferWorkflowPage>(
                        context, std::move(actions)));
            }
        };
        return static_cast<bool>(host->registerPage(std::move(transfer)));
    }

    [[nodiscard]] Ui::ClassTransferWorkflowPage* page() const
    {
        return dynamic_cast<Ui::ClassTransferWorkflowPage*>(
            host ? host->pageInstance(QStringLiteral("class_transfer")) : nullptr);
    }

    [[nodiscard]] bool activate()
    {
        if (!host || !host->activatePage(QStringLiteral("class_transfer")))
        {
            return false;
        }
        host->resize(1024, 768);
        host->show();
        QApplication::processEvents();
        return true;
    }
};

} // namespace

class ClassTransferWorkflowTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void workspaceStoreTransferRoundTripsFullPackageAndRemapsTeacherId();
    void visiblePageExportsReviewsAppliesAndClearsOnWorkspaceChange();
    void applyFailureKeepsReviewAndDestinationTransactionRollsBack();
    void importRejectsMalformedUnsupportedAndUnreadableFiles();
    void historicalLargePackageLoadsIntoOneReviewProjectionWithinLimit();
    void createReplaceSkipAndAmbiguousTeacherChoicesWorkInVisiblePage();
    void scheduleConflictAndStaleReplacementKeepReviewRetryable();
};

void ClassTransferWorkflowTests::workspaceStoreTransferRoundTripsFullPackageAndRemapsTeacherId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.tps"));
    const QString destinationPath = directory.filePath(QStringLiteral("destination.tps"));
    int sourceClassId = -1;
    qint64 sourceTeacherId = -1;
    {
        WorkspaceStore source;
        QVERIFY(source.open(sourcePath));
        sourceClassId = createCompleteSourceClass(source);
        QVERIFY(sourceClassId > 0);
        const auto details = source.classInfoRepository()->loadClassDetailsSaveInfo(
            sourceClassId);
        QVERIFY(details);
        sourceTeacherId = details->teacherId;
        QVERIFY(sourceTeacherId > std::numeric_limits<int>::max());
        QCOMPARE(countRows(source, QStringLiteral("classes")), 1);
        QCOMPARE(countRows(source, QStringLiteral("teachers")), 1);
    }

    const auto exported = Platform::exportClassTransferAtPath(
        sourcePath, {{*Domain::ClassId::fromString(std::to_string(sourceClassId))}});
    QVERIFY2(exported, exported ? "" : exported.error().message.c_str());
    QCOMPARE(exported.value().classes.size(), std::size_t(1));
    QCOMPARE(exported.value().teachers.size(), std::size_t(1));
    const auto& portable = exported.value().classes.front();
    QCOMPARE(portable.info.notes, std::u16string(u"Class notes round-trip"));
    QCOMPARE(portable.info.timeFillerActivities,
        std::u16string(u"Filler activity round-trip"));
    QCOMPARE(portable.info.classGrade, std::u16string(u"E4"));
    QCOMPARE(portable.info.classLevel, std::u16string(u"Theseus"));
    QCOMPARE(portable.info.readingBook, std::u16string(u"Reading Explorer 1"));
    QCOMPARE(portable.info.essayBook, std::u16string(u"4A"));
    QCOMPARE(portable.info.classColor, std::u16string(u"#DDEBFF"));
    QCOMPARE(portable.info.fontColor, std::u16string(u"#000000"));
    QCOMPARE(portable.info.classTimes.size(), std::size_t(1));
    QCOMPARE(portable.info.intensiveTimes.size(), std::size_t(1));
    QCOMPARE(portable.info.classTimes.front().day, std::u16string(u"Monday"));
    QCOMPARE(portable.info.intensiveTimes.front().startTime, std::u16string(u"5:00 PM"));
    QCOMPARE(portable.roster.rows.size(), std::size_t(1));
    QCOMPARE(portable.roster.columns.size(), std::size_t(3));
    QCOMPARE(portable.roster.columnWidths.size(), std::size_t(3));
    QCOMPARE(portable.roster.columnWidths[2].rawValue(), 220);
    QCOMPARE(portable.evaluations.size(), std::size_t(1));
    QCOMPARE(portable.evaluations.front().name, std::u16string(u"Winter"));
    QCOMPARE(portable.evaluations.front().rows.front()[
        SpeakingEval::toInt(SpeakingEvalColumn::Comments)],
        std::u16string(u"Evaluation round-trip"));
    QCOMPARE(exported.value().teachers.front().teacher.teacherKr,
        std::u16string(u"김가람"));
    QCOMPARE(exported.value().teachers.front().teacher.preferredRomanization,
        std::u16string(u"Garam Kim"));
    QCOMPARE(exported.value().teachers.front().teacher.phoneNumber,
        std::u16string(u"010-1234-5678"));
    QCOMPARE(exported.value().teachers.front().teacher.wifiName,
        std::u16string(u"Transfer WiFi"));
    QCOMPARE(exported.value().teachers.front().teacher.zoomId,
        std::u16string(u"transfer-zoom"));
    QCOMPARE(exported.value().teachers.front().teacher.projectionType,
        std::u16string(u"HDMI"));
    QCOMPARE(exported.value().teachers.front().teacher.notes,
        std::u16string(u"Teacher package note"));
    QCOMPARE(portable.teacherKey.value(), exported.value().teachers.front().key.value());

    const QString packagePath = directory.filePath(QStringLiteral("portable.json"));
    QVERIFY(ClassTransferJsonCodec::saveFile(packagePath, exported.value()));
    const auto parsed = ClassTransferJsonCodec::loadFile(packagePath);
    QVERIFY(parsed);

    {
        WorkspaceStore destination;
        QVERIFY(destination.open(destinationPath));
    }
    const auto preview = Platform::previewClassTransferAtPath(destinationPath, {
        .teachers = {{
            Application::ClassTransferTeacherKey(
                QString::fromStdU16String(exported.value().teachers.front().key.value())
                    .toStdString()),
            {"garam kim", "김가람"}
        }},
        .classes = {{
            Application::ClassTransferPackageClassIndex{0},
            Application::ClassTransferTeacherKey(
                QString::fromStdU16String(portable.teacherKey.value()).toStdString()),
            "e4", "theseus",
            Application::ClassTransferClassKey(
                QString::fromStdU16String(portable.key.value()).toStdString())
        }}
    });
    QVERIFY(preview);
    const auto applied = Platform::applyClassTransferAtPath(destinationPath, {
        exported.value(), {
            .classes = {{Application::ClassTransferPackageClassIndex{0},
                Application::ClassTransferReviewClassAction::Create, std::nullopt}},
            .teachers = {{
                Application::ClassTransferTeacherKey(
                    QString::fromStdU16String(exported.value().teachers.front().key.value())
                        .toStdString()),
                Application::ClassTransferReviewTeacherAction::Create,
                std::nullopt
            }}
        }
    });
    QVERIFY2(applied, applied ? "" : applied.error().message.c_str());
    QCOMPARE(applied.value().createdClassIds.size(), std::size_t(1));

    {
        WorkspaceStore source;
        QVERIFY(source.openExisting(sourcePath));
        QCOMPARE(countRows(source, QStringLiteral("classes")), 1);
        QCOMPARE(countRows(source, QStringLiteral("teachers")), 1);
        const auto original = source.classInfoRepository()->loadClassDetailsSaveInfo(
            sourceClassId);
        QVERIFY(original);
        QCOMPARE(original->teacherId, sourceTeacherId);
        QCOMPARE(original->info.notes, QStringLiteral("Class notes round-trip"));
    }
    {
        WorkspaceStore destination;
        QVERIFY(destination.openExisting(destinationPath));
        QCOMPARE(countRows(destination, QStringLiteral("classes")), 1);
        QCOMPARE(countRows(destination, QStringLiteral("teachers")), 1);
        const int importedId = std::stoi(applied.value().createdClassIds.front().value());
        const auto details = destination.classInfoRepository()->loadClassDetailsSaveInfo(
            importedId);
        QVERIFY(details);
        QVERIFY(details->teacherId > 0);
        QVERIFY(details->teacherId != sourceTeacherId);
        QCOMPARE(details->info.classGrade, QStringLiteral("E4"));
        QCOMPARE(details->info.classLevel, QStringLiteral("Theseus"));
        QCOMPARE(details->info.readingBook, QStringLiteral("Reading Explorer 1"));
        QCOMPARE(details->info.essayBook, QStringLiteral("4A"));
        QCOMPARE(details->info.classColor, QStringLiteral("#DDEBFF"));
        QCOMPARE(details->info.fontColor, QStringLiteral("#000000"));
        QCOMPARE(details->info.notes, QStringLiteral("Class notes round-trip"));
        QCOMPARE(details->info.timeFillerActivities,
            QStringLiteral("Filler activity round-trip"));
        QCOMPARE(details->info.classTimes.size(), 1);
        QCOMPARE(details->info.intensiveTimes.size(), 1);
        QCOMPARE(details->info.classTimes.front().day, QStringLiteral("Monday"));
        QCOMPARE(details->info.classTimes.front().startTime, QStringLiteral("4:00 PM"));
        QCOMPARE(details->info.intensiveTimes.front().day, QStringLiteral("Tuesday"));
        QCOMPARE(details->info.intensiveTimes.front().endTime, QStringLiteral("5:50 PM"));
        const auto roster = destination.rosterRepository()->loadRoster(importedId);
        QVERIFY(roster);
        QCOMPARE(roster->columns.size(), 3);
        QCOMPARE(roster->columnWidths[2], 220);
        QCOMPARE(roster->rows.front().at(2), QStringLiteral("Roster cell round-trip"));
        const auto evaluations = destination.speakingEvalRepository()->loadSpeakingEvalBatch(
            importedId, {QStringLiteral("Winter")});
        QVERIFY(evaluations);
        QCOMPARE(evaluations->size(), 1);
        QCOMPARE(evaluations->front().rows[0][SpeakingEval::toInt(
            SpeakingEvalColumn::Comments)], QStringLiteral("Evaluation round-trip"));
        QCOMPARE(evaluations->front().rows[0][SpeakingEval::toInt(
            SpeakingEvalColumn::Grammar)], QStringLiteral("A"));
        const auto teacher = destination.teacherRepository()->getTeacher(
            static_cast<int>(details->teacherId));
        QVERIFY(teacher);
        QCOMPARE(teacher->teacherKr, QStringLiteral("김가람"));
        QCOMPARE(teacher->teacherEn, QStringLiteral("Garam Kim"));
        QCOMPARE(teacher->preferredRomanization, QStringLiteral("Garam Kim"));
        QCOMPARE(teacher->preferredName, QStringLiteral("Garam Kim"));
        QCOMPARE(teacher->roomNumber, QStringLiteral("Room 12"));
        QCOMPARE(teacher->birthday, QStringLiteral("03-04"));
        QCOMPARE(teacher->phoneNumber, QStringLiteral("010-1234-5678"));
        QCOMPARE(teacher->wifiName, QStringLiteral("Transfer WiFi"));
        QCOMPARE(teacher->wifiPassword, QStringLiteral("test-wifi-secret"));
        QCOMPARE(teacher->internetType, QStringLiteral("WiFi"));
        QCOMPARE(teacher->zoomId, QStringLiteral("transfer-zoom"));
        QCOMPARE(teacher->zoomPassword, QStringLiteral("test-zoom-secret"));
        QCOMPARE(teacher->projectionType, QStringLiteral("HDMI"));
        QCOMPARE(teacher->notes, QStringLiteral("Teacher package note"));
    }
}

void ClassTransferWorkflowTests::visiblePageExportsReviewsAppliesAndClearsOnWorkspaceChange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("page-source.tps"));
    const QString destinationPath = directory.filePath(QStringLiteral("page-destination.tps"));
    {
        WorkspaceStore source;
        QVERIFY(source.open(sourcePath));
        QVERIFY(createCompleteSourceClass(source) > 0);
    }
    {
        WorkspaceStore destination;
        QVERIFY(destination.open(destinationPath));
    }

    PageHarness harness;
    harness.path = sourcePath;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    Ui::ClassTransferWorkflowPage* const page = harness.page();
    QVERIFY(page);
    QCOMPARE(page->exportClassList()->model()->rowCount(), 1);
    const QModelIndex classRow = page->exportClassList()->model()->index(0, 0);
    QVERIFY(page->exportClassList()->model()->setData(
        classRow, Qt::Checked, Qt::CheckStateRole));
    page->exportButton()->click();
    QVERIFY(page->statusLabel()->text().contains(QStringLiteral("Export cancelled")));
    page->importButton()->click();
    QVERIFY(page->statusLabel()->text().contains(QStringLiteral("Import cancelled")));
    QVERIFY(!page->hasRetainedPackage());
    const QString packagePath = directory.filePath(QStringLiteral("page-package.json"));
    QVERIFY(page->exportSelectedTo(packagePath));
    QVERIFY(QFileInfo::exists(packagePath));

    harness.path = destinationPath;
    QVERIFY(page->loadImportFile(packagePath));
    QVERIFY(page->hasRetainedPackage());
    QVERIFY(page->applyButton()->isEnabled());
    const QString selectedDestination = destinationPath;
    QVERIFY(!selectedDestination.isEmpty());

    // Path changes invalidate a review before it can reach the repository.
    harness.path = directory.filePath(QStringLiteral("other-destination.tps"));
    QVERIFY(!page->applyImport());
    QVERIFY(!page->hasRetainedPackage());
    QVERIFY(!harness.selectedClass.has_value());

    harness.path = destinationPath;
    QVERIFY(page->onActivating());
    QVERIFY(page->loadImportFile(packagePath));
    QVERIFY2(page->applyImport(), qPrintable(page->statusLabel()->text()));
    QVERIFY(!page->hasRetainedPackage());
    QVERIFY(harness.selectedClass.has_value());
    QVERIFY(page->statusLabel()->text().contains(QStringLiteral("Import complete")));
    QCOMPARE(page->exportClassList()->model()->rowCount(), 1);
    QVERIFY(page->loadImportFile(packagePath));
    page->clearForClosedWorkspace();
    QVERIFY(!page->hasRetainedPackage());
    QCOMPARE(page->exportClassList()->model()->rowCount(), 0);
}

void ClassTransferWorkflowTests::applyFailureKeepsReviewAndDestinationTransactionRollsBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("rollback-source.tps"));
    const QString destinationPath = directory.filePath(QStringLiteral("rollback-destination.tps"));
    {
        WorkspaceStore source;
        QVERIFY(source.open(sourcePath));
        QVERIFY(createCompleteSourceClass(source) > 0);
    }
    {
        WorkspaceStore destination;
        QVERIFY(destination.open(destinationPath));
        QSqlQuery trigger(destination.database());
        QVERIFY2(trigger.exec(QStringLiteral(
            "CREATE TRIGGER reject_transfer_row BEFORE INSERT ON speaking_eval_data "
            "BEGIN SELECT RAISE(ABORT, 'forced transfer failure'); END")),
            qPrintable(trigger.lastError().text()));
    }
    const auto exported = Platform::exportClassTransferAtPath(
        sourcePath, {{*Domain::ClassId::fromString("1")}});
    QVERIFY(exported);
    const QString packagePath = directory.filePath(QStringLiteral("rollback.json"));
    QVERIFY(ClassTransferJsonCodec::saveFile(packagePath, exported.value()));

    PageHarness harness;
    harness.path = destinationPath;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    QVERIFY(page);
    QVERIFY(page->loadImportFile(packagePath));
    QVERIFY(page->applyButton()->isEnabled());
    QVERIFY(!page->applyImport());
    QVERIFY(page->hasRetainedPackage());
    QVERIFY(page->statusLabel()->property("classTransferError").toBool());
    {
        WorkspaceStore destination;
        QVERIFY(destination.openExisting(destinationPath));
        QCOMPARE(countRows(destination, QStringLiteral("classes")), 0);
        QCOMPARE(countRows(destination, QStringLiteral("teachers")), 0);
        QSqlQuery drop(destination.database());
        QVERIFY(drop.exec(QStringLiteral("DROP TRIGGER reject_transfer_row")));
    }
    QVERIFY(page->applyImport());
    QVERIFY(!page->hasRetainedPackage());
}

void ClassTransferWorkflowTests::createReplaceSkipAndAmbiguousTeacherChoicesWorkInVisiblePage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("choices-source.tps"));
    const QString matchedPath = directory.filePath(QStringLiteral("choices-matched.tps"));
    const QString ambiguousPath = directory.filePath(QStringLiteral("choices-ambiguous.tps"));
    int destinationClassId = -1;
    {
        WorkspaceStore source;
        QVERIFY(source.open(sourcePath));
        QVERIFY(createCompleteSourceClass(source) > 0);
    }
    const auto exported = Platform::exportClassTransferAtPath(
        sourcePath, {{*Domain::ClassId::fromString("1")}});
    QVERIFY(exported);
    const QString packagePath = directory.filePath(QStringLiteral("choices.json"));
    QVERIFY(ClassTransferJsonCodec::saveFile(packagePath, exported.value()));
    {
        WorkspaceStore matched;
        QVERIFY(matched.open(matchedPath));
        destinationClassId = createCompleteSourceClass(matched);
        QVERIFY(destinationClassId > 0);
        QVERIFY(matched.classInfoRepository()->saveClassNotesDetailed(
            destinationClassId, QStringLiteral("Local notes before replacement"),
            QStringLiteral("Local filler before replacement")));
    }

    PageHarness harness;
    harness.path = matchedPath;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    QVERIFY(page);
    QVERIFY(page->loadImportFile(packagePath));
    QCOMPARE(page->classReviewTable()->model()->rowCount(), 1);
    QVERIFY(page->applyButton()->isEnabled());
    // One class candidate produces Create another, Replace, and Skip.
    QCOMPARE(page->resolutionChoice()->count(), 3);
    page->resolutionChoice()->setCurrentIndex(1);
    page->reviewTabs()->setCurrentIndex(1);
    page->teacherReviewTable()->selectRow(0);
    QVERIFY(page->applyButton()->isEnabled());
    // A unique teacher match defaults to keeping the local teacher.
    QCOMPARE(page->resolutionChoice()->currentIndex(), 0);
    QVERIFY(page->applyImport());
    QVERIFY(page->statusLabel()->text().contains(QStringLiteral("0 created, 1 replaced, 0 skipped")));
    {
        WorkspaceStore matched;
        QVERIFY(matched.openExisting(matchedPath));
        QCOMPARE(countRows(matched, QStringLiteral("classes")), 1);
        QCOMPARE(countRows(matched, QStringLiteral("teachers")), 1);
        const auto details = matched.classInfoRepository()->loadClassDetailsSaveInfo(
            destinationClassId);
        QVERIFY(details);
        QCOMPARE(details->info.notes, QStringLiteral("Class notes round-trip"));
    }

    QVERIFY(page->loadImportFile(packagePath));
    page->reviewTabs()->setCurrentIndex(0);
    page->classReviewTable()->selectRow(0);
    page->resolutionChoice()->setCurrentIndex(2);
    QVERIFY(page->applyButton()->isEnabled());
    QVERIFY2(page->applyImport(), qPrintable(page->statusLabel()->text()));
    QVERIFY(page->statusLabel()->text().contains(QStringLiteral("0 created, 0 replaced, 1 skipped")));
    {
        WorkspaceStore matched;
        QVERIFY(matched.openExisting(matchedPath));
        QCOMPARE(countRows(matched, QStringLiteral("classes")), 1);
        const auto details = matched.classInfoRepository()->loadClassDetailsSaveInfo(
            destinationClassId);
        QVERIFY(details);
        QCOMPARE(details->info.notes, QStringLiteral("Class notes round-trip"));
    }

    {
        WorkspaceStore ambiguous;
        QVERIFY(ambiguous.open(ambiguousPath));
        const Teacher teacher = matchingTeacher();
        QVERIFY(ambiguous.teacherRepository()->createTeacherWithWideId(teacher));
        QVERIFY(ambiguous.teacherRepository()->createTeacherWithWideId(teacher));
    }
    harness.path = ambiguousPath;
    QVERIFY(page->onActivating());
    QVERIFY(page->loadImportFile(packagePath));
    QVERIFY(!page->applyButton()->isEnabled());
    page->reviewTabs()->setCurrentIndex(1);
    page->teacherReviewTable()->selectRow(0);
    QCOMPARE(page->resolutionChoice()->count(), 6);
    QCOMPARE(page->resolutionChoice()->currentIndex(), 0);
    page->resolutionChoice()->setCurrentIndex(1); // Explicitly create a third teacher.
    QVERIFY(page->applyButton()->isEnabled());
    QVERIFY(page->applyImport());
    {
        WorkspaceStore ambiguous;
        QVERIFY(ambiguous.openExisting(ambiguousPath));
        QCOMPARE(countRows(ambiguous, QStringLiteral("classes")), 1);
        QCOMPARE(countRows(ambiguous, QStringLiteral("teachers")), 3);
    }
}

void ClassTransferWorkflowTests::scheduleConflictAndStaleReplacementKeepReviewRetryable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("conflict-source.tps"));
    const QString conflictPath = directory.filePath(QStringLiteral("conflict-destination.tps"));
    const QString stalePath = directory.filePath(QStringLiteral("stale-destination.tps"));
    {
        WorkspaceStore source;
        QVERIFY(source.open(sourcePath));
        QVERIFY(createCompleteSourceClass(source) > 0);
    }
    const auto exported = Platform::exportClassTransferAtPath(
        sourcePath, {{*Domain::ClassId::fromString("1")}});
    QVERIFY(exported);
    const QString packagePath = directory.filePath(QStringLiteral("conflict.json"));
    QVERIFY(ClassTransferJsonCodec::saveFile(packagePath, exported.value()));
    int conflictAnchorId = -1;
    {
        WorkspaceStore conflict;
        QVERIFY(conflict.open(conflictPath));
        conflictAnchorId = createScheduleConflictAnchor(conflict);
        QVERIFY(conflictAnchorId > 0);
    }

    PageHarness harness;
    harness.path = conflictPath;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    QVERIFY(page);
    QVERIFY(page->loadImportFile(packagePath));
    QVERIFY(page->applyButton()->isEnabled());
    QVERIFY(!page->applyImport());
    QVERIFY(page->hasRetainedPackage());
    QVERIFY(page->statusLabel()->text().contains(QStringLiteral("Schedule conflicts")));
    {
        WorkspaceStore conflict;
        QVERIFY(conflict.openExisting(conflictPath));
        QCOMPARE(countRows(conflict, QStringLiteral("classes")), 1);
        QCOMPARE(countRows(conflict, QStringLiteral("teachers")), 0);
        QSqlQuery clear(conflict.database());
        clear.prepare(QStringLiteral("DELETE FROM class_times WHERE class_id=?"));
        clear.addBindValue(conflictAnchorId);
        QVERIFY(clear.exec());
    }
    QVERIFY(page->applyImport());
    QVERIFY(!page->hasRetainedPackage());
    {
        WorkspaceStore conflict;
        QVERIFY(conflict.openExisting(conflictPath));
        QCOMPARE(countRows(conflict, QStringLiteral("classes")), 2);
        QCOMPARE(countRows(conflict, QStringLiteral("teachers")), 1);
    }

    int staleClassId = -1;
    {
        WorkspaceStore stale;
        QVERIFY(stale.open(stalePath));
        staleClassId = createCompleteSourceClass(stale);
        QVERIFY(staleClassId > 0);
    }
    harness.path = stalePath;
    QVERIFY(page->onActivating());
    QVERIFY(page->loadImportFile(packagePath));
    QCOMPARE(page->resolutionChoice()->count(), 3);
    page->resolutionChoice()->setCurrentIndex(1); // Review a replacement for this ID.
    {
        WorkspaceStore stale;
        QVERIFY(stale.openExisting(stalePath));
        QVERIFY(stale.classRepository()->deleteClass(staleClassId));
        QCOMPARE(countRows(stale, QStringLiteral("classes")), 0);
        QCOMPARE(countRows(stale, QStringLiteral("teachers")), 1);
    }
    QVERIFY(!page->applyImport());
    QVERIFY(page->hasRetainedPackage());
    QVERIFY(page->statusLabel()->property("classTransferError").toBool());
    page->refreshPreviewButton()->click();
    QVERIFY(page->applyButton()->isEnabled());
    QVERIFY(page->applyImport());
    {
        WorkspaceStore stale;
        QVERIFY(stale.openExisting(stalePath));
        QCOMPARE(countRows(stale, QStringLiteral("classes")), 1);
        QCOMPARE(countRows(stale, QStringLiteral("teachers")), 1);
    }
}

void ClassTransferWorkflowTests::importRejectsMalformedUnsupportedAndUnreadableFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString workspacePath = directory.filePath(QStringLiteral("empty.tps"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(workspacePath));
    }
    PageHarness harness;
    harness.path = workspacePath;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    QVERIFY(page);

    const QString malformedPath = directory.filePath(QStringLiteral("malformed.json"));
    QFile malformed(malformedPath);
    QVERIFY(malformed.open(QIODevice::WriteOnly));
    QVERIFY(malformed.write("not json") > 0);
    malformed.close();
    QVERIFY(!page->loadImportFile(malformedPath));
    QVERIFY(!page->hasRetainedPackage());

    const QString unsupportedPath = directory.filePath(QStringLiteral("unsupported.json"));
    QFile unsupported(unsupportedPath);
    QVERIFY(unsupported.open(QIODevice::WriteOnly));
    QVERIFY(unsupported.write("{\"version\":99,\"classes\":[],\"teachers\":[]}") > 0);
    unsupported.close();
    QVERIFY(!page->loadImportFile(unsupportedPath));
    QVERIFY(!page->hasRetainedPackage());

    QVERIFY(!page->loadImportFile(directory.filePath(QStringLiteral("missing.json"))));
    QVERIFY(!page->hasRetainedPackage());

    const QString oversizedPath = directory.filePath(QStringLiteral("oversized.json"));
    QFile oversized(oversizedPath);
    QVERIFY(oversized.open(QIODevice::WriteOnly));
    QVERIFY(oversized.resize(kClassTransferMaximumEncodedFileBytes + 1));
    oversized.close();
    QVERIFY(!page->loadImportFile(oversizedPath));
    QVERIFY(!page->hasRetainedPackage());
    QVERIFY(page->statusLabel()->text().contains(QStringLiteral("16 MiB")));
}

void ClassTransferWorkflowTests::historicalLargePackageLoadsIntoOneReviewProjectionWithinLimit()
{
    constexpr qint64 historicalPackageBytes = 1'716'291;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString workspacePath = directory.filePath(QStringLiteral("large-review.tps"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(workspacePath));
    }

    const auto legacy = ClassTransferJsonCodec::loadFile(
        QStringLiteral(CLASSMNGR_CLASS_TRANSFER_LARGE_PACKAGE_FIXTURE));
    QVERIFY(legacy);
    auto portable = Data::Legacy::toPortableClassTransferPackage(
        std::move(*legacy));
    QVERIFY(!portable.classes.empty());
    QByteArray encoded = QJsonDocument(ClassTransferJsonCodec::toJson(portable))
        .toJson(QJsonDocument::Indented);
    QVERIFY(encoded.size() <= historicalPackageBytes);
    const qint64 paddingBytes = historicalPackageBytes - encoded.size();
    portable.classes.front().info.notes.append(
        static_cast<std::size_t>(paddingBytes), u'x');
    encoded = QJsonDocument(ClassTransferJsonCodec::toJson(portable))
        .toJson(QJsonDocument::Indented);
    QCOMPARE(encoded.size(), historicalPackageBytes);
    const QString packagePath = directory.filePath(QStringLiteral("historical.json"));
    QVERIFY(ClassTransferJsonCodec::saveFile(packagePath, portable));
    QCOMPARE(QFileInfo(packagePath).size(), historicalPackageBytes);

    PageHarness harness;
    harness.path = workspacePath;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    QVERIFY(page);
    PlatformProcessMemorySnapshotProvider memory;
    const ProcessMemorySnapshot before = memory.snapshot();
    QVERIFY(page->loadImportFile(packagePath));
    const ProcessMemorySnapshot reviewed = memory.snapshot();
    QVERIFY(page->hasRetainedPackage());
    QCOMPARE(page->classReviewTable()->model()->rowCount(),
        static_cast<int>(portable.classes.size()));
    QCOMPARE(page->teacherReviewTable()->model()->rowCount(),
        static_cast<int>(portable.teachers.size()));
    QVERIFY(page->applyButton()->isEnabled());
    const int pageWidgetCount = page->findChildren<QWidget*>().size() + 1;
    qInfo().noquote()
        << "ClassTransfer historical package bytes=" << historicalPackageBytes
        << "classes=" << portable.classes.size()
        << "teachers=" << portable.teachers.size()
        << "reviewTableRows="
        << page->classReviewTable()->model()->rowCount()
        << "pageWidgets=" << pageWidgetCount
        << "memoryBeforeAvailable=" << before.isAvailable
        << "memoryBeforePlatform=" << before.platform
        << "memoryBeforeRSS=" << before.workingSetBytes
        << "memoryBeforePrivate=" << before.privateUsageBytes
        << "memoryReviewedRSS=" << reviewed.workingSetBytes
        << "memoryReviewedPrivate=" << reviewed.privateUsageBytes;
    page->cancelImportReview();
    QVERIFY(!page->hasRetainedPackage());
    const ProcessMemorySnapshot released = memory.snapshot();
    qInfo().noquote()
        << "ClassTransfer afterCancelRSS=" << released.workingSetBytes
        << "afterCancelPrivate=" << released.privateUsageBytes;
}

QTEST_MAIN(ClassTransferWorkflowTests)
#include "class_transfer_workflow_tests.moc"
