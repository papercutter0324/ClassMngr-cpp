#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/styles/file_dialog_icon_style.h"

#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWidget>

namespace
{

template<typename Dialog>
Dialog* findServiceDialog(
    const QString& objectName
    )
{
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (widget->objectName() == objectName)
        {
            return qobject_cast<Dialog*>(widget);
        }
    }

    return nullptr;
}

}

class DialogServicesTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void fakePromptServiceRecordsRequestsAndScriptsChoices();
    void fakeFileDialogServiceRecordsRequestsAndScriptsResults();
    void applicationFileDialogAccessCanBeOverridden();
    void messageMapsSeverityDetailsAndParent_data();
    void messageMapsSeverityDetailsAndParent();
    void confirmationMapsButtonsAndResults_data();
    void confirmationMapsButtonsAndResults();
    void platformFileDialogPolicyIsExplicit();
    void qtFileDialogAppliesSharedPolicy();
    void saveFileReturnsAccessoryChoice();
    void openFileCanonicalizesAndRemembersPurposeDirectory();
};

void DialogServicesTests::cleanup()
{
    DialogServices::setFileDialogServiceForTesting(nullptr);
}

void DialogServicesTests::fakePromptServiceRecordsRequestsAndScriptsChoices()
{
    FakeUserPromptService service;
    QWidget parent;
    PromptRequest message{
        .parent = &parent,
        .title = QStringLiteral("Saved"),
        .message = QStringLiteral("The report was saved."),
        .severity = PromptSeverity::Information
    };
    service.showMessage(message);

    PromptRequest confirmation{
        .parent = &parent,
        .title = QStringLiteral("Delete report"),
        .message = QStringLiteral("Delete this report?"),
        .acceptText = QStringLiteral("Delete"),
        .rejectText = QStringLiteral("Keep Report"),
        .destructive = true
    };
    service.scriptedChoices.enqueue(PromptChoice::Destructive);

    QCOMPARE(service.confirm(confirmation), PromptChoice::Destructive);
    QCOMPARE(service.messages.size(), 1);
    QCOMPARE(service.messages.first().parent, &parent);
    QCOMPARE(service.messages.first().title, QStringLiteral("Saved"));
    QCOMPARE(service.confirmations.size(), 1);
    QCOMPARE(
        service.confirmations.first().rejectText,
        QStringLiteral("Keep Report")
        );

    QCOMPARE(
        service.confirm(confirmation),
        PromptChoice::Rejected
        );
}

void DialogServicesTests::fakeFileDialogServiceRecordsRequestsAndScriptsResults()
{
    FakeFileDialogService service;
    OpenFileRequest request{
        .title = QStringLiteral("Import Schedule"),
        .purpose = FileDialogPurpose::ImportWorkbook,
        .nameFilters = {QStringLiteral("Workbooks (*.xlsx)")}
    };
    service.scriptedOpenFiles.enqueue(
        QStringLiteral("/tmp/schedule.xlsx")
        );

    const std::optional<QString> result = service.openFile(request);
    QVERIFY(result.has_value());
    QCOMPARE(*result, QStringLiteral("/tmp/schedule.xlsx"));
    QCOMPARE(service.openFileRequests.size(), 1);
    QCOMPARE(
        service.openFileRequests.first().purpose,
        FileDialogPurpose::ImportWorkbook
        );

    QVERIFY(!service.saveFile(SaveFileRequest()).has_value());
    QCOMPARE(service.saveFileRequests.size(), 1);
}

void DialogServicesTests::applicationFileDialogAccessCanBeOverridden()
{
    FakeFileDialogService fake;
    fake.scriptedDirectories.enqueue(QStringLiteral("/tmp/reports"));
    DialogServices::setFileDialogServiceForTesting(&fake);

    const std::optional<QString> result =
        DialogServices::fileDialogs().selectDirectory(
            DirectoryRequest{
                .title = QStringLiteral("Choose Reports Folder"),
                .purpose = FileDialogPurpose::ExportReport
            }
            );

    QVERIFY(result.has_value());
    QCOMPARE(*result, QStringLiteral("/tmp/reports"));
    QCOMPARE(fake.directoryRequests.size(), 1);
    QCOMPARE(
        fake.directoryRequests.first().purpose,
        FileDialogPurpose::ExportReport
        );
}

void DialogServicesTests::messageMapsSeverityDetailsAndParent_data()
{
    QTest::addColumn<int>("severity");
    QTest::addColumn<int>("icon");

    QTest::newRow("information")
        << static_cast<int>(PromptSeverity::Information)
        << static_cast<int>(QMessageBox::Information);
    QTest::newRow("warning")
        << static_cast<int>(PromptSeverity::Warning)
        << static_cast<int>(QMessageBox::Warning);
    QTest::newRow("error")
        << static_cast<int>(PromptSeverity::Error)
        << static_cast<int>(QMessageBox::Critical);
}

void DialogServicesTests::messageMapsSeverityDetailsAndParent()
{
    QFETCH(int, severity);
    QFETCH(int, icon);

    QWidget parent;
    QtUserPromptService service;
    bool inspected = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            auto* dialog = findServiceDialog<QMessageBox>(
                QStringLiteral("classmngrUserPrompt")
                );
            QVERIFY(dialog);
            QCOMPARE(dialog->parentWidget(), &parent);
            QCOMPARE(
                static_cast<int>(dialog->icon()),
                icon
                );
#if !defined(Q_OS_MACOS)
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Policy title"));
#endif
            QCOMPARE(dialog->text(), QStringLiteral("Primary message"));
            QCOMPARE(dialog->detailedText(), QStringLiteral("Technical detail"));
            QCOMPARE(dialog->textFormat(), Qt::PlainText);
            QCOMPARE(dialog->windowModality(), Qt::WindowModal);

            auto* acceptButton = dialog->findChild<QPushButton*>(
                QStringLiteral("promptAcceptButton")
                );
            QVERIFY(acceptButton);
            QCOMPARE(dialog->defaultButton(), acceptButton);
            QCOMPARE(dialog->escapeButton(), acceptButton);
            inspected = true;
            acceptButton->click();
        }
        );

    service.showMessage(
        PromptRequest{
            .parent = &parent,
            .title = QStringLiteral("Policy title"),
            .message = QStringLiteral("Primary message"),
            .details = QStringLiteral("Technical detail"),
            .severity = static_cast<PromptSeverity>(severity)
        }
        );
    QVERIFY(inspected);
}

void DialogServicesTests::confirmationMapsButtonsAndResults_data()
{
    QTest::addColumn<bool>("destructive");
    QTest::addColumn<QString>("buttonToUse");
    QTest::addColumn<int>("expectedChoice");

    QTest::newRow("accept")
        << false << QStringLiteral("accept")
        << static_cast<int>(PromptChoice::Accepted);
    QTest::newRow("reject")
        << false << QStringLiteral("reject")
        << static_cast<int>(PromptChoice::Rejected);
    QTest::newRow("close")
        << false << QStringLiteral("close")
        << static_cast<int>(PromptChoice::Canceled);
    QTest::newRow("destructive")
        << true << QStringLiteral("accept")
        << static_cast<int>(PromptChoice::Destructive);
}

void DialogServicesTests::confirmationMapsButtonsAndResults()
{
    QFETCH(bool, destructive);
    QFETCH(QString, buttonToUse);
    QFETCH(int, expectedChoice);

    QWidget parent;
    QtUserPromptService service;
    bool inspected = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            auto* dialog = findServiceDialog<QMessageBox>(
                QStringLiteral("classmngrUserPrompt")
                );
            QVERIFY(dialog);

            auto* acceptButton = dialog->findChild<QPushButton*>(
                destructive
                    ? QStringLiteral("promptDestructiveButton")
                    : QStringLiteral("promptAcceptButton")
                );
            auto* rejectButton = dialog->findChild<QPushButton*>(
                QStringLiteral("promptRejectButton")
                );
            QVERIFY(acceptButton);
            QVERIFY(rejectButton);
            QCOMPARE(
                dialog->buttonRole(acceptButton),
                destructive
                    ? QMessageBox::DestructiveRole
                    : QMessageBox::AcceptRole
                );
            QCOMPARE(
                dialog->buttonRole(rejectButton),
                QMessageBox::RejectRole
                );
            QCOMPARE(dialog->escapeButton(), rejectButton);
            QCOMPARE(
                dialog->defaultButton(),
                destructive
                    ? rejectButton
                    : acceptButton
                );
            inspected = true;

            if (buttonToUse == QStringLiteral("accept"))
            {
                acceptButton->click();
            }
            else if (buttonToUse == QStringLiteral("reject"))
            {
                rejectButton->click();
            }
            else
            {
                dialog->reject();
            }
        }
        );

    const PromptChoice choice = service.confirm(
        PromptRequest{
            .parent = &parent,
            .title = QStringLiteral("Confirm"),
            .message = QStringLiteral("Continue?"),
            .severity = PromptSeverity::Warning,
            .acceptText = QStringLiteral("Proceed"),
            .rejectText = QStringLiteral("Go Back"),
            .destructive = destructive
        }
        );
    QVERIFY(inspected);
    QCOMPARE(static_cast<int>(choice), expectedChoice);
}

void DialogServicesTests::platformFileDialogPolicyIsExplicit()
{
#if defined(Q_OS_MACOS)
    QVERIFY(QtFileDialogService::platformUsesNativeDialogs());
#else
    QVERIFY(!QtFileDialogService::platformUsesNativeDialogs());
#endif
}

void DialogServicesTests::qtFileDialogAppliesSharedPolicy()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QSettings settings(
        temporaryDirectory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat
        );
    QWidget parent;
    QtFileDialogService service(&settings, FileDialogBackend::Qt);
    struct ObservedPolicy
    {
        bool found = false;
        QWidget* parent = nullptr;
        Qt::WindowModality modality = Qt::NonModal;
        bool nonNative = false;
        QFileDialog::AcceptMode acceptMode = QFileDialog::AcceptOpen;
        QFileDialog::FileMode fileMode = QFileDialog::ExistingFile;
        QString defaultSuffix;
        bool skipsOverwriteConfirmation = false;
        QStringList nameFilters;
        bool usesIconStyle = false;
    } observed;
    QTimer::singleShot(
        0,
        [&]()
        {
            auto* dialog = findServiceDialog<QFileDialog>(
                QStringLiteral("classmngrFileDialog")
                );
            if (!dialog)
            {
                return;
            }

            observed.found = true;
            observed.parent = dialog->parentWidget();
            observed.modality = dialog->windowModality();
            observed.nonNative = dialog->testOption(
                QFileDialog::DontUseNativeDialog
                );
            observed.acceptMode = dialog->acceptMode();
            observed.fileMode = dialog->fileMode();
            observed.defaultSuffix = dialog->defaultSuffix();
            observed.skipsOverwriteConfirmation = dialog->testOption(
                QFileDialog::DontConfirmOverwrite
                );
            observed.nameFilters = dialog->nameFilters();
            observed.usesIconStyle =
                dynamic_cast<FileDialogIconStyle*>(dialog->style());
            dialog->reject();
        }
        );

    const std::optional<QString> result = service.saveFile(
        SaveFileRequest{
            .parent = &parent,
            .title = QStringLiteral("Export Report"),
            .purpose = FileDialogPurpose::ExportReport,
            .initialDirectory = temporaryDirectory.path(),
            .suggestedFileName = QStringLiteral("report"),
            .nameFilters = {QStringLiteral("PDF files (*.pdf)")},
            .defaultSuffix = QStringLiteral("pdf")
        }
        );
    QVERIFY(observed.found);
    QCOMPARE(observed.parent, &parent);
    QCOMPARE(observed.modality, Qt::WindowModal);
    QVERIFY(observed.nonNative);
    QCOMPARE(observed.acceptMode, QFileDialog::AcceptSave);
    QCOMPARE(observed.fileMode, QFileDialog::AnyFile);
    QCOMPARE(observed.defaultSuffix, QStringLiteral("pdf"));
    QVERIFY(!observed.skipsOverwriteConfirmation);
    QCOMPARE(
        observed.nameFilters,
        QStringList({QStringLiteral("PDF files (*.pdf)")})
        );
    QVERIFY(observed.usesIconStyle);
    QVERIFY(!result.has_value());
}

void DialogServicesTests::saveFileReturnsAccessoryChoice()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QSettings settings(
        temporaryDirectory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat
        );
    QtFileDialogService service(&settings, FileDialogBackend::Qt);
    const QString filePath = temporaryDirectory.filePath(
        QStringLiteral("export.pdf")
        );
    QTimer::singleShot(
        0,
        [filePath]()
        {
            auto* dialog = findServiceDialog<QFileDialog>(
                QStringLiteral("classmngrFileDialog")
                );
            QVERIFY(dialog);
            auto* openAfterSaving = dialog->findChild<QCheckBox*>(
                QStringLiteral("fileDialogOpenAfterSaving")
                );
            QVERIFY(openAfterSaving);
            openAfterSaving->setChecked(true);
            dialog->selectFile(filePath);
            static_cast<QDialog*>(dialog)->accept();
        }
        );

    const std::optional<SaveFileSelection> selection =
        service.saveFileWithOptions(
            SaveFileRequest{
                .title = QStringLiteral("Export"),
                .purpose = FileDialogPurpose::GeneratedPdf,
                .initialDirectory = temporaryDirectory.path(),
                .suggestedFileName = QStringLiteral("export.pdf"),
                .nameFilters = {QStringLiteral("PDF files (*.pdf)")},
                .defaultSuffix = QStringLiteral("pdf"),
                .openAfterSavingText = QStringLiteral("Open after saving")
            }
            );

    QVERIFY(selection.has_value());
    QCOMPARE(
        selection->path,
        QDir(QFileInfo(temporaryDirectory.path()).canonicalFilePath())
            .filePath(QStringLiteral("export.pdf"))
        );
    QVERIFY(selection->openAfterSaving);
}

void DialogServicesTests::openFileCanonicalizesAndRemembersPurposeDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString filePath = temporaryDirectory.filePath(
        QStringLiteral("schedule.xlsx")
        );
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QSettings settings(
        temporaryDirectory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat
        );
    QtFileDialogService service(&settings, FileDialogBackend::Qt);
    QTimer::singleShot(
        0,
        [filePath]()
        {
            auto* dialog = findServiceDialog<QFileDialog>(
                QStringLiteral("classmngrFileDialog")
                );
            QVERIFY(dialog);
            dialog->selectFile(filePath);
            static_cast<QDialog*>(dialog)->accept();
        }
        );

    const std::optional<QString> result = service.openFile(
        OpenFileRequest{
            .title = QStringLiteral("Import Schedule"),
            .purpose = FileDialogPurpose::ImportWorkbook,
            .initialDirectory = temporaryDirectory.path(),
            .nameFilters = {QStringLiteral("Workbooks (*.xlsx)")}
        }
        );
    QVERIFY(result.has_value());
    QCOMPARE(*result, QFileInfo(filePath).canonicalFilePath());
    QCOMPARE(
        settings.value(
            QStringLiteral(
                "file-dialog/directories/import-workbook"
                )
            ).toString(),
        QFileInfo(temporaryDirectory.path()).canonicalFilePath()
        );
}

QTEST_MAIN(DialogServicesTests)

#include "dialog_services_tests.moc"
