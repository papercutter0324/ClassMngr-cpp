#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "data/data_service.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_transfer_legacy_package_adapter.h"
#include "data/repositories/class_transfer_repository.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "features/classes/ui/class_export_dialog.h"
#include "features/classes/ui/class_import_dialog.h"
#include "next/platform/workspace_store_class_transfer_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/class_transfer_workflow_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPalette>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <QTemporaryDir>
#include <QTranslator>
#include <QUuid>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};
constexpr int SelectedClassId = 1;

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

[[nodiscard]] bool captureWidget(
    QWidget& widget,
    const QString& path,
    QString* error
    )
{
    widget.resize(Viewport);
    widget.show();
    QApplication::processEvents();
    const QImage image = widget.grab(QRect(QPoint(0, 0), Viewport)).toImage();
    if (image.size() == Viewport && image.save(path, "PNG"))
    {
        return true;
    }
    if (error)
    {
        *error = QStringLiteral("Could not save %1 at %2x%3.")
            .arg(path).arg(image.width()).arg(image.height());
    }
    return false;
}

[[nodiscard]] bool copyWorkspaceFixture(
    const QString& directory,
    const QString& fileName,
    QString* destination,
    QString* error
    )
{
    *destination = QDir(directory).filePath(fileName);
    if (QFile::copy(QStringLiteral(CLASSMNGR_CLASS_TRANSFER_WORKSPACE_FIXTURE),
            *destination))
    {
        return true;
    }
    if (error)
    {
        *error = QStringLiteral("Could not copy the 96-class workspace fixture: %1")
            .arg(*destination);
    }
    return false;
}

[[nodiscard]] bool prepareTransferFixture(
    const QString& workspacePath,
    QString* error
    )
{
    const QString connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase database = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(workspacePath);
    if (!database.open())
    {
        if (error) *error = database.lastError().text();
        database = {};
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    bool valid = false;
    QString diagnostic;
    {
        QSqlQuery teacher(database);
        teacher.prepare(QStringLiteral(
            "UPDATE teachers SET teacher_kr=?, teacher_en=?, "
            "preferred_romanization=?, preferred_name=?, birthday=? WHERE id=1"));
        teacher.addBindValue(QStringLiteral("김가람"));
        teacher.addBindValue(QStringLiteral("Garam Kim"));
        teacher.addBindValue(QStringLiteral("Garam Kim"));
        teacher.addBindValue(QStringLiteral("Garam Kim"));
        teacher.addBindValue(QStringLiteral("02-02"));
        valid = teacher.exec() && teacher.numRowsAffected() == 1;
        if (!valid) diagnostic = teacher.lastError().text();
    }
    if (valid)
    {
        QSqlQuery classInfo(database);
        classInfo.prepare(QStringLiteral(
            "UPDATE class_info SET class_level=?, reading_book='', essay_book='' "
            "WHERE class_id=1"));
        classInfo.addBindValue(QStringLiteral("Theseus"));
        valid = classInfo.exec() && classInfo.numRowsAffected() == 1;
        if (!valid) diagnostic = classInfo.lastError().text();
    }
    database.close();
    database = {};
    QSqlDatabase::removeDatabase(connectionName);
    if (!valid && error)
    {
        *error = QStringLiteral("Could not seed a valid class transfer fixture: %1")
            .arg(diagnostic);
    }
    return valid;
}

[[nodiscard]] bool reduceConflictSchedules(
    const QString& workspacePath,
    QString* error
    )
{
    const QString connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase database = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(workspacePath);
    if (!database.open())
    {
        if (error) *error = database.lastError().text();
        database = {};
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    bool deleted = false;
    QString diagnostic;
    {
        QSqlQuery query(database);
        deleted = query.exec(QStringLiteral(
            "DELETE FROM class_times WHERE class_id <> 1"));
        if (!deleted) diagnostic = query.lastError().text();
    }
    database.close();
    database = {};
    QSqlDatabase::removeDatabase(connectionName);
    if (!deleted && error)
    {
        *error = QStringLiteral("Could not reduce conflict fixture schedules: %1")
            .arg(diagnostic);
    }
    return deleted;
}

[[nodiscard]] std::vector<Domain::ClassId> selectedIds()
{
    std::vector<Domain::ClassId> ids;
    ids.push_back(*Domain::ClassId::fromString(std::to_string(SelectedClassId)));
    return ids;
}

struct PageHarness final
{
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    QString path;

    [[nodiscard]] bool initialize(QString* error)
    {
        const auto loaded = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
        if (!loaded)
        {
            if (error) *error = QStringLiteral("Next resource catalog is unavailable.");
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(std::move(*loaded));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);
        Ui::PageDescriptor descriptor{
            .id = *Ui::PageId::fromString(QStringLiteral("class_transfer")),
            .label = QStringLiteral("Class Transfer"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::ClassTransferWorkflowPageActions actions;
                actions.activeWorkspacePath = [this]() -> std::optional<QString> {
                    return path.isEmpty() ? std::nullopt
                        : std::optional<QString>(path);
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
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::ClassTransferWorkflowPage>(
                        context, std::move(actions)));
            }
        };
        const auto registered = host->registerPage(std::move(descriptor));
        if (!registered && error) *error = registered.error().message;
        return static_cast<bool>(registered);
    }

    [[nodiscard]] bool activate(QString* error)
    {
        if (!host || !host->activatePage(QStringLiteral("class_transfer")))
        {
            if (error) *error = QStringLiteral("Could not activate Class Transfer.");
            return false;
        }
        host->resize(Viewport);
        host->show();
        QApplication::processEvents();
        return true;
    }

    [[nodiscard]] Ui::ClassTransferWorkflowPage* page() const
    {
        return dynamic_cast<Ui::ClassTransferWorkflowPage*>(
            host ? host->pageInstance(QStringLiteral("class_transfer")) : nullptr);
    }
};

[[nodiscard]] bool ensurePackage(
    const QString& sourcePath,
    const QString& packagePath,
    QString* error
    )
{
    const auto package = Platform::exportClassTransferAtPath(sourcePath,
        {selectedIds()});
    if (!package)
    {
        if (error) *error = QString::fromUtf8(package.error().message.data(),
            static_cast<qsizetype>(package.error().message.size()));
        return false;
    }
    const Status saved = ClassTransferJsonCodec::saveFile(packagePath, package.value());
    if (!saved)
    {
        if (error) *error = saved.error();
        return false;
    }
    return true;
}

[[nodiscard]] QString packageSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(QCryptographicHash::hash(
        file.readAll(), QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] bool captureLegacyExport(
    const QString& workspacePath,
    const QString& outputPath,
    QJsonObject* evidence,
    QString* error
    )
{
    ApplicationServices services;
    if (!services.openDatabase(workspacePath))
    {
        if (error) *error = QStringLiteral("Legacy could not open export fixture.");
        return false;
    }
    ClassExportDialog dialog(&services);
    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("classExportList"));
    if (!list)
    {
        if (error) *error = QStringLiteral("Legacy export list was not found.");
        return false;
    }
    int selected = 0;
    for (int index = 0; index < list->count(); ++index)
    {
        QListWidgetItem* item = list->item(index);
        if (item->data(Qt::UserRole).toInt() == SelectedClassId)
        {
            item->setCheckState(Qt::Checked);
            ++selected;
        }
    }
    if (selected != 1)
    {
        if (error) *error = QStringLiteral("Legacy export fixture did not select class 1.");
        return false;
    }
    if (!captureWidget(dialog, outputPath, error)) return false;
    *evidence = {
        {QStringLiteral("classRows"), list->count()},
        {QStringLiteral("selectedClassCount"), dialog.selectedClassIds().size()},
        {QStringLiteral("widgetCount"), widgetCount(&dialog)}
    };
    return true;
}

[[nodiscard]] bool captureNextExport(
    const QString& workspacePath,
    const QString& outputPath,
    QJsonObject* evidence,
    QString* error
    )
{
    PageHarness harness;
    harness.path = workspacePath;
    if (!harness.initialize(error) || !harness.activate(error)) return false;
    Ui::ClassTransferWorkflowPage* const page = harness.page();
    if (!page)
    {
        if (error) *error = QStringLiteral("Next Class Transfer page was not created.");
        return false;
    }
    int selected = 0;
    QAbstractItemModel* const model = page->exportClassList()->model();
    for (int row = 0; row < model->rowCount(); ++row)
    {
        bool validId = false;
        const int classId = model->data(model->index(row, 0), Qt::ToolTipRole)
            .toString().toInt(&validId);
        if (validId && classId == SelectedClassId)
        {
            model->setData(model->index(row, 0), Qt::Checked, Qt::CheckStateRole);
            ++selected;
        }
    }
    if (selected != 1)
    {
        if (error) *error = QStringLiteral("Next export model did not select class 1.");
        return false;
    }
    if (!captureWidget(*harness.host, outputPath, error)) return false;
    *evidence = {
        {QStringLiteral("classRows"), model->rowCount()},
        {QStringLiteral("selectedClassCount"), page->exportedClassChoiceCount()},
        {QStringLiteral("widgetCount"), widgetCount(page)},
        {QStringLiteral("detailEditorCount"), 0},
        {QStringLiteral("perClassWidgets"), 0}
    };
    return true;
}

[[nodiscard]] bool captureLegacyReview(
    const QString& workspacePath,
    const QString& packagePath,
    const QString& outputPath,
    QJsonObject* evidence,
    QString* error
    )
{
    ApplicationServices services;
    if (!services.openDatabase(workspacePath))
    {
        if (error) *error = QStringLiteral("Legacy could not open review fixture.");
        return false;
    }
    const auto parsed = ClassTransferJsonCodec::loadFile(packagePath);
    if (!parsed)
    {
        if (error) *error = parsed.error();
        return false;
    }
    auto* const session = services.dataService()->databaseSession();
    if (!session || !session->classTransferRepository())
    {
        if (error) *error = QStringLiteral("Legacy transfer repository is unavailable.");
        return false;
    }
    const auto preview = session->classTransferRepository()->previewImport(*parsed);
    if (!preview)
    {
        if (error) *error = preview.error().diagnostic;
        return false;
    }
    ClassImportDialog dialog(&services, *parsed, *preview);
    if (!captureWidget(dialog, outputPath, error)) return false;
    *evidence = {
        {QStringLiteral("packageClassRows"), parsed->classes.size()},
        {QStringLiteral("packageTeacherRows"), parsed->teachers.size()},
        {QStringLiteral("classResolutionWidgets"),
            dialog.findChildren<QComboBox*>().size()},
        {QStringLiteral("widgetCount"), widgetCount(&dialog)}
    };
    return true;
}

[[nodiscard]] bool captureNextReview(
    const QString& workspacePath,
    const QString& packagePath,
    const QString& outputPath,
    PlatformProcessMemorySnapshotProvider& memory,
    QJsonObject* evidence,
    QString* error
    )
{
    const ProcessMemorySnapshot before = memory.snapshot();
    PageHarness harness;
    harness.path = workspacePath;
    if (!harness.initialize(error) || !harness.activate(error)) return false;
    Ui::ClassTransferWorkflowPage* const page = harness.page();
    if (!page || !page->loadImportFile(packagePath))
    {
        if (error) *error = page ? page->statusLabel()->text()
            : QStringLiteral("Next Class Transfer page is unavailable.");
        return false;
    }
    const ProcessMemorySnapshot reviewed = memory.snapshot();
    if (!captureWidget(*harness.host, outputPath, error)) return false;
    const int classRows = page->classReviewTable()->model()->rowCount();
    const int teacherRows = page->teacherReviewTable()->model()->rowCount();
    const int widgets = widgetCount(page);
    page->cancelImportReview();
    const ProcessMemorySnapshot released = memory.snapshot();
    *evidence = {
        {QStringLiteral("packageClassRows"), classRows},
        {QStringLiteral("packageTeacherRows"), teacherRows},
        {QStringLiteral("reviewResolutionWidgets"), 1},
        {QStringLiteral("widgetCount"), widgets},
        {QStringLiteral("processMemoryBefore"), memoryJson(before)},
        {QStringLiteral("processMemoryReviewed"), memoryJson(reviewed)},
        {QStringLiteral("processMemoryAfterRelease"), memoryJson(released)}
    };
    return true;
}

[[nodiscard]] bool captureLegacyConflict(
    const QString& workspacePath,
    const QString& packagePath,
    const QString& outputPath,
    QJsonObject* evidence,
    QString* error
    )
{
    ApplicationServices services;
    if (!services.openDatabase(workspacePath))
    {
        if (error) *error = QStringLiteral("Legacy could not open conflict fixture.");
        return false;
    }
    const auto parsed = ClassTransferJsonCodec::loadFile(packagePath);
    if (!parsed)
    {
        if (error) *error = parsed.error();
        return false;
    }
    auto* const session = services.dataService()->databaseSession();
    const auto preview = session->classTransferRepository()->previewImport(*parsed);
    if (!preview)
    {
        if (error) *error = preview.error().diagnostic;
        return false;
    }
    ClassImportDialog review(&services, *parsed, *preview);
    const auto result = session->classTransferRepository()->importClasses(
        *parsed, review.importPlan());
    if (result)
    {
        if (error) *error = QStringLiteral("Legacy conflict fixture unexpectedly imported.");
        return false;
    }
    QMessageBox warning(QMessageBox::Warning,
        QStringLiteral("Import Classes"), result.error().diagnostic,
        QMessageBox::Ok);
    warning.setObjectName(QStringLiteral("classTransferConflictWarning"));
    if (!captureWidget(warning, outputPath, error)) return false;
    *evidence = {
        {QStringLiteral("feedbackText"), result.error().diagnostic},
        {QStringLiteral("widgetCount"), widgetCount(&warning)},
        {QStringLiteral("scheduleConflictDetected"), true}
    };
    return true;
}

[[nodiscard]] bool captureNextConflict(
    const QString& workspacePath,
    const QString& packagePath,
    const QString& outputPath,
    PlatformProcessMemorySnapshotProvider& memory,
    QJsonObject* evidence,
    QString* error
    )
{
    const ProcessMemorySnapshot before = memory.snapshot();
    PageHarness harness;
    harness.path = workspacePath;
    if (!harness.initialize(error) || !harness.activate(error)) return false;
    Ui::ClassTransferWorkflowPage* const page = harness.page();
    if (!page || !page->loadImportFile(packagePath))
    {
        if (error) *error = page ? page->statusLabel()->text()
            : QStringLiteral("Next Class Transfer page is unavailable.");
        return false;
    }
    const ProcessMemorySnapshot reviewed = memory.snapshot();
    if (page->applyImport())
    {
        if (error) *error = QStringLiteral("Next conflict fixture unexpectedly imported.");
        return false;
    }
    const ProcessMemorySnapshot failedApply = memory.snapshot();
    const QString feedbackText = page->statusLabel()->text();
    if (!feedbackText.contains(QStringLiteral("Schedule conflicts"),
            Qt::CaseInsensitive)
        && !feedbackText.contains(QStringLiteral("일정 충돌")))
    {
        if (error) *error = QStringLiteral(
            "Next page did not expose the schedule conflict: %1").arg(feedbackText);
        return false;
    }
    if (!captureWidget(*harness.host, outputPath, error)) return false;
    const int widgets = widgetCount(page);
    const bool retainsPackage = page->hasRetainedPackage();
    page->cancelImportReview();
    const ProcessMemorySnapshot released = memory.snapshot();
    *evidence = {
        {QStringLiteral("feedbackText"), feedbackText},
        {QStringLiteral("retainsPackageAfterFailure"), retainsPackage},
        {QStringLiteral("widgetCount"), widgets},
        {QStringLiteral("processMemoryBefore"), memoryJson(before)},
        {QStringLiteral("processMemoryReviewed"), memoryJson(reviewed)},
        {QStringLiteral("processMemoryFailedApply"), memoryJson(failedApply)},
        {QStringLiteral("processMemoryAfterRelease"), memoryJson(released)}
    };
    return true;
}

[[nodiscard]] bool capturePair(
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    PlatformProcessMemorySnapshotProvider& memory,
    QJsonObject* result,
    QString* error
    )
{
    QTemporaryDir temporary;
    if (!temporary.isValid())
    {
        if (error) *error = QStringLiteral("Could not create paired transfer fixture directory.");
        return false;
    }
    QString sourcePath;
    QString destinationPath;
    if (!copyWorkspaceFixture(temporary.path(), QStringLiteral("source.tps"),
            &sourcePath, error)
        || !copyWorkspaceFixture(temporary.path(), QStringLiteral("destination.tps"),
            &destinationPath, error))
    {
        return false;
    }
    if (!prepareTransferFixture(sourcePath, error)
        || !prepareTransferFixture(destinationPath, error)
        || !reduceConflictSchedules(destinationPath, error))
    {
        return false;
    }
    const QString packagePath = QDir(temporary.path()).filePath(
        QStringLiteral("paired-class-transfer.json"));
    if (!ensurePackage(sourcePath, packagePath, error)) return false;

    const QString prefix = QStringLiteral("%1_%2").arg(locale, theme);
    QJsonObject legacyExport;
    QJsonObject nextExport;
    if (!captureLegacyExport(sourcePath,
            QDir(outputDirectory).filePath(prefix + QStringLiteral("_legacy_export.png")),
            &legacyExport, error)
        || !captureNextExport(sourcePath,
            QDir(outputDirectory).filePath(prefix + QStringLiteral("_next_export.png")),
            &nextExport, error))
    {
        return false;
    }

    QJsonObject legacyReview;
    QJsonObject nextReview;
    if (!captureLegacyReview(destinationPath, packagePath,
            QDir(outputDirectory).filePath(prefix + QStringLiteral("_legacy_review.png")),
            &legacyReview, error)
        || !captureNextReview(destinationPath, packagePath,
            QDir(outputDirectory).filePath(prefix + QStringLiteral("_next_review.png")),
            memory, &nextReview, error))
    {
        return false;
    }

    QJsonObject legacyConflict;
    QJsonObject nextConflict;
    if (!captureLegacyConflict(destinationPath, packagePath,
            QDir(outputDirectory).filePath(prefix + QStringLiteral("_legacy_conflict.png")),
            &legacyConflict, error)
        || !captureNextConflict(destinationPath, packagePath,
            QDir(outputDirectory).filePath(prefix + QStringLiteral("_next_conflict.png")),
            memory, &nextConflict, error))
    {
        return false;
    }

    *result = {
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("fixtureClassCount"), 96},
        {QStringLiteral("selectedClassIds"), QJsonArray{SelectedClassId}},
        {QStringLiteral("packageFileBytes"), QFileInfo(packagePath).size()},
        {QStringLiteral("packageSha256"), packageSha256(packagePath)},
        {QStringLiteral("exportPicker"), QJsonObject{
            {QStringLiteral("legacy"), legacyExport},
            {QStringLiteral("next"), nextExport}}},
        {QStringLiteral("importReview"), QJsonObject{
            {QStringLiteral("legacy"), legacyReview},
            {QStringLiteral("next"), nextReview}}},
        {QStringLiteral("conflictFeedback"), QJsonObject{
            {QStringLiteral("legacy"), legacyConflict},
            {QStringLiteral("next"), nextConflict}}},
        {QStringLiteral("images"), QJsonObject{
            {QStringLiteral("exportPicker"), QJsonObject{
                {QStringLiteral("legacy"), prefix + QStringLiteral("_legacy_export.png")},
                {QStringLiteral("next"), prefix + QStringLiteral("_next_export.png")}}},
            {QStringLiteral("importReview"), QJsonObject{
                {QStringLiteral("legacy"), prefix + QStringLiteral("_legacy_review.png")},
                {QStringLiteral("next"), prefix + QStringLiteral("_next_review.png")}}},
            {QStringLiteral("conflictFeedback"), QJsonObject{
                {QStringLiteral("legacy"), prefix + QStringLiteral("_legacy_conflict.png")},
                {QStringLiteral("next"), prefix + QStringLiteral("_next_conflict.png")}}}}}
    };
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ClassMngrClassTransferVisualCapture"));
    if (argc < 2)
    {
        qCritical("Expected a capture output directory.");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create the class transfer capture output directory.");
        return 2;
    }

    QJsonArray pairs;
    PlatformProcessMemorySnapshotProvider memory;
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            application.setPalette(themePalette(theme));
            QTranslator translator;
            const QString translationPath = QDir(
                QStringLiteral(CLASSMNGR_NEXT_TRANSLATION_ROOT))
                .filePath(QStringLiteral("ClassMngr_%1.qm").arg(locale));
            if (!translator.load(translationPath))
            {
                qCritical("Could not load transfer capture translation: %s",
                    qPrintable(translationPath));
                return 1;
            }
            application.installTranslator(&translator);
            QLocale::setDefault(QLocale(locale));

            QJsonObject pair;
            QString error;
            const bool captured = capturePair(outputDirectory, locale, theme,
                memory, &pair, &error);
            application.removeTranslator(&translator);
            if (!captured)
            {
                qCritical("Class transfer paired capture failed: %s",
                    qPrintable(error));
                return 1;
            }
            pairs.append(pair);
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("feature"), QStringLiteral("Class Transfer")},
        {QStringLiteral("captureScope"),
            QStringLiteral("Legacy export selection, import review, and schedule-conflict feedback paired with the Next route.")},
        {QStringLiteral("shellScope"),
            QStringLiteral("Legacy export and import surfaces are standalone dialogs; Next is captured inside PageHost. Both use a 1024x768 capture and paired state, but their shell extents differ, so these images do not establish visual parity.")},
        {QStringLiteral("fixturePreparation"),
            QStringLiteral("Each pair copies the tracked 96-class .tps fixture for source and destination. Teacher 1 and class 1 use catalog-valid sample values. The destination copy retains class 1 regular and intensive schedules and clears schedules for classes 2-96 to make the conflict feedback readable; the exported package contains class 1.")},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("fixture"), QStringLiteral(CLASSMNGR_CLASS_TRANSFER_WORKSPACE_FIXTURE)},
        {QStringLiteral("memoryLimitation"),
            QStringLiteral("Process-wide memory snapshots come from one sequential capture process; resident/private values include Qt allocator retention and cumulative font/translation caches, and do not establish a Windows peak budget.")},
        {QStringLiteral("parityClaim"), false},
        {QStringLiteral("pairs"), pairs}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
            QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write class transfer capture manifest.");
        return 1;
    }
    qInfo().noquote() << "Class Transfer paired captures written to" << outputDirectory;
    return 0;
}
