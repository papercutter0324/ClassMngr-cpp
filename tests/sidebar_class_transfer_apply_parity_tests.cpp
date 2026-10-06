#include "app/controllers/sidebar_controller.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "domain/models/class_transfer.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "features/classes/ui/class_import_dialog.h"
#include "features/classes/ui/classes_page.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

#include <memory>

namespace
{
ClassTransferPackage transferPackage()
{
    ClassTransferPackage package;
    ClassTransferClass transferClass;
    transferClass.key = QStringLiteral("source-class-1");
    transferClass.name = QStringLiteral("Imported Class");
    transferClass.info.classGrade = QStringLiteral("E4");
    transferClass.info.classLevel = QStringLiteral("Theseus");
    package.classes.append(transferClass);
    return package;
}

QString boolJson(const bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

int rowCount(ApplicationServices& services, const QString& table)
{
    QSqlQuery query(services.databaseSession()->database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

void emitTranscript(const QString& json)
{
    qInfo().noquote() << QStringLiteral("F367_TRANSCRIPT=") + json;
}

class ImportFixture final
{
public:
    enum class DialogAction
    {
        Accept,
        Cancel,
        Skip
    };

    [[nodiscard]] bool open()
    {
        if (!services.openDatabase(directory.filePath(
                QStringLiteral("class-transfer-apply-parity.tps"))))
        {
            return false;
        }

        const auto existing = services.databaseSession()->classRepository()
            ->createClass(QStringLiteral("Existing Class"));
        if (!existing)
        {
            return false;
        }
        existingClassId = *existing;

        if (!ClassTransferJsonCodec::saveFile(
                packagePath,
                transferPackage()))
        {
            return false;
        }

        pages.initialize(&services, false);
        pages.setDatabaseOpen(true);
        pages.showPage(PageType::Classes);
        page = pages.classesPage();
        if (!page)
        {
            return false;
        }
        sidebar.selectByKeys({QStringLiteral("my_workspace")});
        controller = std::make_unique<SidebarController>(
            &services,
            &sidebar,
            &pages
            );
        DialogServices::setFileDialogServiceForTesting(&fileDialogs);
        DialogServices::setUserPromptServiceForTesting(&prompts);
        return true;
    }

    void selectPackageFile()
    {
        fileDialogs.scriptedOpenFiles.enqueue(
            std::optional<QString>(packagePath));
    }

    [[nodiscard]] bool invokeImport(
        const DialogAction action,
        bool* dialogSeen = nullptr,
        bool* dialogAccepted = nullptr
        )
    {
        bool seen = false;
        bool accepted = false;
        QTimer::singleShot(0, controller.get(), [&]
        {
            auto* dialog = qobject_cast<ClassImportDialog*>(
                QApplication::activeModalWidget());
            if (!dialog)
            {
                return;
            }
            seen = true;
            if (action == DialogAction::Cancel)
            {
                dialog->reject();
                return;
            }

            if (action == DialogAction::Skip)
            {
                auto* const choice = dialog->findChild<QComboBox*>(
                    QStringLiteral("classImportChoice_0"));
                const int skip = choice
                    ? choice->findData(static_cast<int>(ClassImportAction::Skip))
                    : -1;
                if (!choice || skip < 0)
                {
                    dialog->reject();
                    return;
                }
                choice->setCurrentIndex(skip);
            }

            auto* const accept = dialog->findChild<QPushButton*>(
                QStringLiteral("importClassesButton"));
            if (!accept || !accept->isEnabled())
            {
                dialog->reject();
                return;
            }
            accept->click();
            accepted = dialog->result() == QDialog::Accepted;
        });

        const bool invoked = QMetaObject::invokeMethod(
            controller.get(),
            "importClasses",
            Qt::DirectConnection
            );
        QCoreApplication::processEvents();
        if (dialogSeen)
        {
            *dialogSeen = seen;
        }
        if (dialogAccepted)
        {
            *dialogAccepted = accepted;
        }
        return invoked;
    }

    [[nodiscard]] bool installClassInsertFailure() const
    {
        QSqlQuery trigger(services.databaseSession()->database());
        return trigger.exec(QStringLiteral(
            "CREATE TRIGGER fail_class_transfer_insert "
            "BEFORE INSERT ON classes "
            "BEGIN SELECT RAISE(ABORT, 'F367 apply failure'); END"));
    }

    QTemporaryDir directory;
    ApplicationServices services;
    PageManager pages;
    Sidebar sidebar;
    FakeFileDialogService fileDialogs;
    FakeUserPromptService prompts;
    std::unique_ptr<SidebarController> controller;
    ClassesPage* page = nullptr;
    QString packagePath = directory.filePath(QStringLiteral("package.json"));
    int existingClassId = -1;
};
}

class SidebarClassTransferApplyParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void pickerCancelHasNoSideEffects();
    void dialogCancelHasNoSideEffects();
    void writeFailureWarnsWithoutRefreshOrNavigation();
    void successRefreshesNavigatesAndReportsCounts();
    void skippedOnlyRestoresPriorSelection();
};

void SidebarClassTransferApplyParityTests::cleanup()
{
    DialogServices::setFileDialogServiceForTesting(nullptr);
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void SidebarClassTransferApplyParityTests::pickerCancelHasNoSideEffects()
{
    ImportFixture fixture;
    QVERIFY(fixture.open());
    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    const QStringList selectionBefore = fixture.sidebar.selectedKeys();

    QVERIFY(QMetaObject::invokeMethod(
        fixture.controller.get(), "importClasses", Qt::DirectConnection));

    QCOMPARE(fixture.fileDialogs.openFileRequests.size(), 1);
    QVERIFY(fixture.prompts.confirmations.isEmpty());
    QVERIFY(fixture.prompts.messages.isEmpty());
    QCOMPARE(rowCount(fixture.services, QStringLiteral("classes")), 1);
    QCOMPARE(fixture.page->runtimeMetrics().classQueryCount - queriesBefore, 0);
    QCOMPARE(fixture.pages.currentPageIdentifier(),
        PageManager::pageTypeIdentifier(PageType::Classes));
    QCOMPARE(fixture.sidebar.selectedKeys(), selectionBefore);

    emitTranscript(QStringLiteral(
        "{\"case\":\"picker_cancel\",\"picker_calls\":1,"
        "\"warnings\":0,\"info\":0,\"class_count\":1,"
        "\"page_refreshes\":0,\"sidebar_unchanged\":%1}"
        ).arg(boolJson(fixture.sidebar.selectedKeys() == selectionBefore)));
}

void SidebarClassTransferApplyParityTests::dialogCancelHasNoSideEffects()
{
    ImportFixture fixture;
    QVERIFY(fixture.open());
    fixture.selectPackageFile();
    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    const QStringList selectionBefore = fixture.sidebar.selectedKeys();
    bool dialogSeen = false;

    QVERIFY(fixture.invokeImport(ImportFixture::DialogAction::Cancel,
        &dialogSeen));

    QVERIFY(dialogSeen);
    QCOMPARE(fixture.fileDialogs.openFileRequests.size(), 1);
    QVERIFY(fixture.prompts.confirmations.isEmpty());
    QVERIFY(fixture.prompts.messages.isEmpty());
    QCOMPARE(rowCount(fixture.services, QStringLiteral("classes")), 1);
    QCOMPARE(fixture.page->runtimeMetrics().classQueryCount - queriesBefore, 0);
    QCOMPARE(fixture.sidebar.selectedKeys(), selectionBefore);

    emitTranscript(QStringLiteral(
        "{\"case\":\"dialog_cancel\",\"picker_calls\":1,"
        "\"dialog_seen\":true,\"warnings\":0,\"info\":0,"
        "\"class_count\":1,\"page_refreshes\":0,"
        "\"sidebar_unchanged\":%1}"
        ).arg(boolJson(fixture.sidebar.selectedKeys() == selectionBefore)));
}

void SidebarClassTransferApplyParityTests::
writeFailureWarnsWithoutRefreshOrNavigation()
{
    ImportFixture fixture;
    QVERIFY(fixture.open());
    fixture.selectPackageFile();
    QVERIFY(fixture.installClassInsertFailure());
    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    const QStringList selectionBefore = fixture.sidebar.selectedKeys();
    bool dialogSeen = false;
    bool dialogAccepted = false;

    QVERIFY(fixture.invokeImport(ImportFixture::DialogAction::Accept,
        &dialogSeen, &dialogAccepted));

    QVERIFY(dialogSeen);
    QVERIFY(dialogAccepted);
    QCOMPARE(fixture.prompts.messages.size(), 1);
    const PromptRequest& warning = fixture.prompts.messages.first();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Import Classes"));
    QVERIFY(warning.message.startsWith(
        QStringLiteral("Creating an imported class failed:")));
    QVERIFY(warning.message.contains(QStringLiteral("F367 apply failure")));
    QCOMPARE(rowCount(fixture.services, QStringLiteral("classes")), 1);
    QCOMPARE(rowCount(fixture.services, QStringLiteral("teachers")), 0);
    QCOMPARE(fixture.page->runtimeMetrics().classQueryCount - queriesBefore, 0);
    QCOMPARE(fixture.pages.currentPageIdentifier(),
        PageManager::pageTypeIdentifier(PageType::Classes));
    QCOMPARE(fixture.sidebar.selectedKeys(), selectionBefore);

    QJsonObject transcript{
        {QStringLiteral("case"), QStringLiteral("write_failure")},
        {QStringLiteral("dialog_accepted"), true},
        {QStringLiteral("warnings"), 1},
        {QStringLiteral("warning_title"), warning.title},
        {QStringLiteral("warning_message"), warning.message},
        {QStringLiteral("class_count"), 1},
        {QStringLiteral("teacher_count"), 0},
        {QStringLiteral("page_refreshes"), 0},
        {QStringLiteral("sidebar_unchanged"),
            fixture.sidebar.selectedKeys() == selectionBefore}
    };
    emitTranscript(QString::fromUtf8(
        QJsonDocument(transcript).toJson(QJsonDocument::Compact)));
}

void SidebarClassTransferApplyParityTests::
successRefreshesNavigatesAndReportsCounts()
{
    ImportFixture fixture;
    QVERIFY(fixture.open());
    fixture.selectPackageFile();
    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    bool dialogSeen = false;
    bool dialogAccepted = false;

    QVERIFY(fixture.invokeImport(ImportFixture::DialogAction::Accept,
        &dialogSeen, &dialogAccepted));

    QVERIFY(dialogSeen);
    QVERIFY(dialogAccepted);
    QCOMPARE(fixture.prompts.messages.size(), 1);
    const PromptRequest& information = fixture.prompts.messages.first();
    QCOMPARE(information.severity, PromptSeverity::Information);
    QCOMPARE(information.title, QStringLiteral("Import Classes"));
    QCOMPARE(information.message,
        QStringLiteral("Import complete. Created: 1, replaced: 0, skipped: 0."));
    QCOMPARE(rowCount(fixture.services, QStringLiteral("classes")), 2);
    QCOMPARE(rowCount(fixture.services, QStringLiteral("teachers")), 0);
    QVERIFY(fixture.page->runtimeMetrics().classQueryCount > queriesBefore);
    QCOMPARE(fixture.page->runtimeMetrics().sourceClassCount, 2);
    QVERIFY(fixture.page->currentClassId() > 0);
    QVERIFY(fixture.page->currentClassId() != fixture.existingClassId);
    QVERIFY(fixture.pages.isCurrentPage(PageType::Classes));
    QCOMPARE(fixture.sidebar.selectedKeys(),
        QStringList{QStringLiteral("classes")});

    emitTranscript(QStringLiteral(
        "{\"case\":\"success\",\"dialog_accepted\":true,"
        "\"created\":1,\"replaced\":0,\"skipped\":0,"
        "\"class_count\":2,\"page_refreshed\":true,"
        "\"destination_opened\":true,\"sidebar\":\"classes\","
        "\"prompt\":\"Import complete. Created: 1, replaced: 0, skipped: 0.\"}"
        ));
}

void SidebarClassTransferApplyParityTests::skippedOnlyRestoresPriorSelection()
{
    ImportFixture fixture;
    QVERIFY(fixture.open());
    fixture.selectPackageFile();
    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    const QStringList selectionBefore = fixture.sidebar.selectedKeys();
    bool dialogSeen = false;
    bool dialogAccepted = false;

    QVERIFY(fixture.invokeImport(ImportFixture::DialogAction::Skip,
        &dialogSeen, &dialogAccepted));

    QVERIFY(dialogSeen);
    QVERIFY(dialogAccepted);
    QCOMPARE(fixture.prompts.messages.size(), 1);
    QCOMPARE(fixture.prompts.messages.first().severity,
        PromptSeverity::Information);
    QCOMPARE(fixture.prompts.messages.first().message,
        QStringLiteral("Import complete. Created: 0, replaced: 0, skipped: 1."));
    QCOMPARE(rowCount(fixture.services, QStringLiteral("classes")), 1);
    QVERIFY(fixture.page->runtimeMetrics().classQueryCount > queriesBefore);
    QCOMPARE(fixture.page->runtimeMetrics().sourceClassCount, 1);
    QCOMPARE(fixture.page->currentClassId(), -1);
    QCOMPARE(fixture.sidebar.selectedKeys(), selectionBefore);
    QVERIFY(fixture.pages.isCurrentPage(PageType::Classes));

    emitTranscript(QStringLiteral(
        "{\"case\":\"skipped_only\",\"created\":0,"
        "\"replaced\":0,\"skipped\":1,\"class_count\":1,"
        "\"page_refreshed\":true,\"current_class\":\"none\","
        "\"sidebar_restored\":%1}"
        ).arg(boolJson(fixture.sidebar.selectedKeys() == selectionBefore)));
}

QTEST_MAIN(SidebarClassTransferApplyParityTests)

#include "sidebar_class_transfer_apply_parity_tests.moc"
