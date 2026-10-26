#include "next/ui/qt_file_dialog_presenter.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QPushButton>
#include <QPointer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeView>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Ui;

namespace
{

class FakeDirectoryPreferences final : public FileDialogDirectoryPreferencesPort
{
public:
    DirectoryPreferencePath readDirectory(
        const FileDialogPurpose purpose
        ) const override
    {
        ++readCount;
        lastReadPurpose = purpose;
        const auto found = directories.find(purpose);
        return found == directories.end()
            ? DirectoryPreferencePath{}
            : DirectoryPreferencePath(found->second);
    }

    Domain::Result<void> writeDirectory(
        const FileDialogPurpose purpose,
        const DirectoryPreferencePath& directory
        ) const override
    {
        ++writeCount;
        lastWritePurpose = purpose;
        lastWrittenDirectory = directory.value();
        if (writeError)
        {
            return Domain::Result<void>::failure(*writeError);
        }
        directories[purpose] = directory.value();
        return Domain::Result<void>::success();
    }

    mutable int readCount = 0;
    mutable int writeCount = 0;
    mutable std::optional<FileDialogPurpose> lastReadPurpose;
    mutable std::optional<FileDialogPurpose> lastWritePurpose;
    mutable std::string lastWrittenDirectory;
    mutable std::map<FileDialogPurpose, std::string> directories;
    mutable std::optional<Domain::OperationError> writeError;
};

QFileDialog* activeFileDialog()
{
    return qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
}

QString fallbackDirectory(const FileDialogPurpose purpose)
{
    const auto location = purpose == FileDialogPurpose::SignatureImage
        ? QStandardPaths::PicturesLocation
        : QStandardPaths::DocumentsLocation;
    const QString directory = QStandardPaths::writableLocation(location);
    return directory.isEmpty() || !QDir(directory).exists()
        ? QDir::homePath()
        : directory;
}

bool createFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write("test") == 4;
}

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

void pressEscape(QFileDialog* dialog)
{
    if (dialog)
    {
        QTest::keyClick(dialog, Qt::Key_Escape);
    }
}

bool clickAcceptButton(
    QFileDialog* dialog,
    const QDialogButtonBox::StandardButton standardButton
    )
{
    if (!dialog)
    {
        return false;
    }
    QDialogButtonBox* buttonBox = dialog->findChild<QDialogButtonBox*>();
    QPushButton* acceptButton = buttonBox
        ? buttonBox->button(standardButton)
        : nullptr;
    if (!acceptButton)
    {
        dialog->reject();
        return false;
    }
    QTest::mouseClick(acceptButton, Qt::LeftButton);
    if (dialog->isVisible())
    {
        QTimer::singleShot(1000, dialog, &QFileDialog::reject);
    }
    return true;
}

} // namespace

class QtFileDialogPresenterTests final : public QObject
{
    Q_OBJECT

private slots:
    void forcedQtAndPlatformDefaultPolicyAreExplicit();
    void initialDirectoryUsesExplicitThenRememberedThenPurposeFallback();
    void openOneHonorsFiltersUtf8SelectionAndRememberedDirectory();
    void openOneUsesEscapeForCancellationWithoutWritingPreferences();
    void openManyUsesStandardOpenModeAndRemembersAcceptedSelection();
    void saveAppliesSuggestedNameSuffixOverwriteAndAccessory();
    void saveWithoutAccessoryKeepsNativePolicyAndOverwriteOptOut();
    void directorySelectionNormalizesAndRemembersAcceptedDirectory();
    void preferenceFailureKeepsAcceptedSelectionAndReturnsWarning();
    void presenterReportsWrongThreadAsTechnicalFailure();
};

void QtFileDialogPresenterTests::forcedQtAndPlatformDefaultPolicyAreExplicit()
{
    FakeDirectoryPreferences preferences;
    QtFileDialogPresenter forcedQt(
        preferences,
        nullptr,
        FileDialogBackend::Qt
        );
    QtFileDialogPresenter platformDefault(
        preferences,
        nullptr,
        FileDialogBackend::PlatformDefault
        );

    QCOMPARE(forcedQt.usesNativeDialogs(), false);
    QCOMPARE(
        platformDefault.usesNativeDialogs(),
        QtFileDialogPresenter::platformUsesNativeDialogs()
        );
#if defined(Q_OS_MACOS)
    QVERIFY(QtFileDialogPresenter::platformUsesNativeDialogs());
#else
    QVERIFY(!QtFileDialogPresenter::platformUsesNativeDialogs());
#endif

    bool observedQtDialog = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            QFileDialog* dialog = activeFileDialog();
            if (!dialog)
            {
                return;
            }
            observedQtDialog = dialog->testOption(
                QFileDialog::DontUseNativeDialog
                );
            pressEscape(dialog);
        }
        );
    const auto result = forcedQt.openOneFile(OpenFileDialogRequest{
        .title = "Open"
    });
    QVERIFY(result.hasValue());
    QVERIFY(!result.value().selection);
    QVERIFY(observedQtDialog);
}

void QtFileDialogPresenterTests::initialDirectoryUsesExplicitThenRememberedThenPurposeFallback()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString explicitDirectory = temporaryDirectory.filePath("explicit");
    const QString rememberedDirectory = temporaryDirectory.filePath("記憶");
    QVERIFY(QDir().mkpath(explicitDirectory));
    QVERIFY(QDir().mkpath(rememberedDirectory));

    FakeDirectoryPreferences preferences;
    preferences.directories[FileDialogPurpose::ImportWorkbook] = utf8(
        rememberedDirectory
        );
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Qt
        );

    QString observedDirectory;
    QTimer::singleShot(
        0,
        [&]()
        {
            if (QFileDialog* dialog = activeFileDialog())
            {
                observedDirectory = dialog->directory().absolutePath();
                pressEscape(dialog);
            }
        }
        );
    const auto explicitResult = presenter.openOneFile(OpenFileDialogRequest{
        .purpose = FileDialogPurpose::ImportWorkbook,
        .title = "Explicit directory",
        .initialDirectory = FileDialogPath(utf8(
            explicitDirectory + QStringLiteral("/unused/..")
            ))
    });
    QVERIFY(explicitResult.hasValue());
    QVERIFY(!explicitResult.value().selection);
    QCOMPARE(
        QDir::cleanPath(observedDirectory),
        QDir::cleanPath(explicitDirectory)
        );

    observedDirectory.clear();
    QTimer::singleShot(
        0,
        [&]()
        {
            if (QFileDialog* dialog = activeFileDialog())
            {
                observedDirectory = dialog->directory().absolutePath();
                pressEscape(dialog);
            }
        }
        );
    const auto rememberedResult = presenter.openOneFile(OpenFileDialogRequest{
        .purpose = FileDialogPurpose::ImportWorkbook,
        .title = "Remembered directory"
    });
    QVERIFY(rememberedResult.hasValue());
    QCOMPARE(
        QDir::cleanPath(observedDirectory),
        QDir::cleanPath(rememberedDirectory)
        );

    observedDirectory.clear();
    QTimer::singleShot(
        0,
        [&]()
        {
            if (QFileDialog* dialog = activeFileDialog())
            {
                observedDirectory = dialog->directory().absolutePath();
                pressEscape(dialog);
            }
        }
        );
    const auto fallbackResult = presenter.openOneFile(OpenFileDialogRequest{
        .purpose = FileDialogPurpose::SignatureImage,
        .title = "Pictures fallback"
    });
    QVERIFY(fallbackResult.hasValue());
    QCOMPARE(
        QDir::cleanPath(observedDirectory),
        QDir::cleanPath(fallbackDirectory(FileDialogPurpose::SignatureImage))
        );
    QCOMPARE(preferences.lastReadPurpose, FileDialogPurpose::SignatureImage);

    observedDirectory.clear();
    QTimer::singleShot(
        0,
        [&]()
        {
            if (QFileDialog* dialog = activeFileDialog())
            {
                observedDirectory = dialog->directory().absolutePath();
                pressEscape(dialog);
            }
        }
        );
    const auto documentsFallbackResult = presenter.openOneFile(
        OpenFileDialogRequest{
            .purpose = FileDialogPurpose::General,
            .title = "Documents fallback"
        }
        );
    QVERIFY(documentsFallbackResult.hasValue());
    QCOMPARE(
        QDir::cleanPath(observedDirectory),
        QDir::cleanPath(fallbackDirectory(FileDialogPurpose::General))
        );
    QCOMPARE(preferences.lastReadPurpose, FileDialogPurpose::General);
    QCOMPARE(preferences.writeCount, 0);
}

void QtFileDialogPresenterTests::openOneHonorsFiltersUtf8SelectionAndRememberedDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString realDirectory = temporaryDirectory.filePath("資料");
    QVERIFY(QDir().mkpath(realDirectory));
    const QString realFile = realDirectory + QStringLiteral("/授業📘.txt");
    QVERIFY(createFile(realFile));
    const QString aliasFile = temporaryDirectory.filePath("alias.txt");
    const bool linkCreated = QFile::link(realFile, aliasFile);
    const QString selectedFile = linkCreated ? aliasFile : realFile;
    const QString expectedFile = QFileInfo(realFile).canonicalFilePath();

    FakeDirectoryPreferences preferences;
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Qt
        );
    const OpenFileDialogRequest request{
        .purpose = FileDialogPurpose::TeacherProfile,
        .title = "選択",
        .initialDirectory = FileDialogPath(utf8(realDirectory)),
        .nameFilters = {"Text files (*.txt)", "All files (*)"}
    };

    bool observedPolicy = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            QFileDialog* dialog = activeFileDialog();
            if (!dialog)
            {
                return;
            }
            observedPolicy = dialog->acceptMode() == QFileDialog::AcceptOpen
                && dialog->fileMode() == QFileDialog::ExistingFile
                && dialog->nameFilters()
                    == QStringList({
                        QStringLiteral("Text files (*.txt)"),
                        QStringLiteral("All files (*)")
                    });
            dialog->selectFile(selectedFile);
            QTest::keyClick(dialog, Qt::Key_Return);
        }
        );

    const auto result = presenter.openOneFile(request);
    QVERIFY(result.hasValue());
    QVERIFY(observedPolicy);
    QVERIFY(result.value().selection);
    QCOMPARE(result.value().selection->value(), utf8(expectedFile));
    QVERIFY(!result.value().preferenceWarning);
    QCOMPARE(preferences.writeCount, 1);
    QCOMPARE(
        preferences.lastWritePurpose,
        std::optional<FileDialogPurpose>(FileDialogPurpose::TeacherProfile)
        );
    QCOMPARE(
        preferences.lastWrittenDirectory,
        utf8(QFileInfo(expectedFile).absolutePath())
        );
}

void QtFileDialogPresenterTests::openOneUsesEscapeForCancellationWithoutWritingPreferences()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FakeDirectoryPreferences preferences;
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Qt
        );

    bool escaped = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            if (QFileDialog* dialog = activeFileDialog())
            {
                escaped = true;
                pressEscape(dialog);
            }
        }
        );
    const auto result = presenter.openOneFile(OpenFileDialogRequest{
        .purpose = FileDialogPurpose::General,
        .title = "Cancel",
        .initialDirectory = FileDialogPath(utf8(temporaryDirectory.path()))
    });

    QVERIFY(result.hasValue());
    QVERIFY(escaped);
    QVERIFY(!result.value().selection);
    QVERIFY(!result.value().preferenceWarning);
    QCOMPARE(preferences.writeCount, 0);
}

void QtFileDialogPresenterTests::openManyUsesStandardOpenModeAndRemembersAcceptedSelection()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString firstFile = temporaryDirectory.filePath("first.txt");
    const QString secondFile = temporaryDirectory.filePath("second.txt");
    QVERIFY(createFile(firstFile));
    QVERIFY(createFile(secondFile));

    FakeDirectoryPreferences preferences;
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Qt
        );
    bool observedMode = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            QFileDialog* dialog = activeFileDialog();
            if (!dialog)
            {
                return;
            }
            observedMode = dialog->acceptMode() == QFileDialog::AcceptOpen
                && dialog->fileMode() == QFileDialog::ExistingFiles;

            // Select two rows through the Qt file view to exercise the actual
            // ExistingFiles selection path rather than a synthetic response.
            for (QTreeView* view : dialog->findChildren<QTreeView*>())
            {
                if (!view->selectionModel() || !view->model())
                {
                    continue;
                }
                auto* fileSystemModel = qobject_cast<QFileSystemModel*>(
                    view->model()
                    );
                if (!fileSystemModel)
                {
                    continue;
                }
                const QModelIndex firstIndex = fileSystemModel->index(firstFile);
                const QModelIndex secondIndex = fileSystemModel->index(secondFile);
                if (firstIndex.isValid() && secondIndex.isValid())
                {
                    view->selectionModel()->select(
                        firstIndex,
                        QItemSelectionModel::Select | QItemSelectionModel::Rows
                        );
                    view->selectionModel()->select(
                        secondIndex,
                        QItemSelectionModel::Select | QItemSelectionModel::Rows
                        );
                    break;
                }
            }
            QTest::keyClick(dialog, Qt::Key_Return);
        }
        );

    const auto result = presenter.openManyFiles(OpenFileDialogRequest{
        .purpose = FileDialogPurpose::ImportWorkbook,
        .title = "Choose files",
        .initialDirectory = FileDialogPath(utf8(temporaryDirectory.path())),
        .nameFilters = {"Text files (*.txt)"}
    });

    QVERIFY(result.hasValue());
    QVERIFY(observedMode);
    QVERIFY(result.value().selection);
    QCOMPARE(result.value().selection->size(), std::size_t(2));
    std::vector<std::string> selectedPaths;
    for (const FileDialogPath& path : *result.value().selection)
    {
        selectedPaths.push_back(path.value());
    }
    std::vector<std::string> expectedPaths{
        utf8(QFileInfo(firstFile).canonicalFilePath()),
        utf8(QFileInfo(secondFile).canonicalFilePath())
    };
    std::sort(selectedPaths.begin(), selectedPaths.end());
    std::sort(expectedPaths.begin(), expectedPaths.end());
    QCOMPARE(selectedPaths, expectedPaths);
    QCOMPARE(preferences.writeCount, 1);
    QCOMPARE(
        preferences.lastWrittenDirectory,
        utf8(QFileInfo(temporaryDirectory.path()).canonicalFilePath())
        );
}

void QtFileDialogPresenterTests::saveAppliesSuggestedNameSuffixOverwriteAndAccessory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString outputDirectory = temporaryDirectory.filePath("real-output");
    QVERIFY(QDir().mkpath(outputDirectory));
    const QString outputDirectoryAlias = temporaryDirectory.filePath("output-alias");
    const bool outputLinkCreated = QFile::link(
        outputDirectory,
        outputDirectoryAlias
        );
    const QString selectedOutputDirectory = outputLinkCreated
        ? outputDirectoryAlias
        : outputDirectory;
    FakeDirectoryPreferences preferences;
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Native
        );
    const QString requestedOutput = selectedOutputDirectory
        + QStringLiteral("/結果");
    bool observedPolicy = false;
    bool observedSuggestedName = false;
    bool acceptedWithSaveButton = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            QFileDialog* dialog = activeFileDialog();
            if (!dialog)
            {
                return;
            }
            const QStringList initialSelection = dialog->selectedFiles();
            observedSuggestedName = initialSelection.size() == 1
                && QFileInfo(initialSelection.first()).fileName()
                    == QStringLiteral("建議.pdf");
            QCheckBox* accessory = dialog->findChild<QCheckBox*>(
                QStringLiteral("classmngrNextOpenAfterSaving")
                );
            observedPolicy = dialog->acceptMode() == QFileDialog::AcceptSave
                && dialog->fileMode() == QFileDialog::AnyFile
                && dialog->defaultSuffix() == QStringLiteral("pdf")
                && dialog->testOption(QFileDialog::DontUseNativeDialog)
                && !dialog->testOption(QFileDialog::DontConfirmOverwrite)
                && dialog->nameFilters()
                    == QStringList({QStringLiteral("PDF (*.pdf)")})
                && accessory
                && !accessory->isChecked();
            if (!accessory)
            {
                dialog->reject();
                return;
            }
            accessory->setChecked(true);
            dialog->selectFile(requestedOutput);
            acceptedWithSaveButton = clickAcceptButton(
                dialog,
                QDialogButtonBox::Save
                );
        }
        );

    const auto result = presenter.saveFile(SaveFileDialogRequest{
        .purpose = FileDialogPurpose::GeneratedPdf,
        .title = "Save",
        .initialDirectory = FileDialogPath(utf8(temporaryDirectory.path())),
        .suggestedFileName = "建議.pdf",
        .nameFilters = {"PDF (*.pdf)"},
        .defaultSuffix = "pdf",
        .confirmOverwrite = true,
        .openAfterSavingText = "Open after saving"
    });

    QVERIFY(result.hasValue());
    QVERIFY(observedPolicy);
    QVERIFY(observedSuggestedName);
    QVERIFY(acceptedWithSaveButton);
    QVERIFY(result.value().selection);
    QCOMPARE(
        result.value().selection->path.value(),
        utf8(QDir(QFileInfo(outputDirectory).canonicalFilePath()).filePath(
            QStringLiteral("結果.pdf")
            ))
        );
    QVERIFY(result.value().selection->openAfterSaving);
    QCOMPARE(preferences.writeCount, 1);
}

void QtFileDialogPresenterTests::saveWithoutAccessoryKeepsNativePolicyAndOverwriteOptOut()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FakeDirectoryPreferences preferences;
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Native
        );
    bool observedPolicy = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            QFileDialog* dialog = activeFileDialog();
            if (!dialog)
            {
                return;
            }
            observedPolicy = !dialog->testOption(
                    QFileDialog::DontUseNativeDialog
                    )
                && dialog->testOption(QFileDialog::DontConfirmOverwrite);
            pressEscape(dialog);
        }
        );
    const auto result = presenter.saveFile(SaveFileDialogRequest{
        .purpose = FileDialogPurpose::ExportReport,
        .title = "Save",
        .initialDirectory = FileDialogPath(utf8(temporaryDirectory.path())),
        .confirmOverwrite = false
    });
    QVERIFY(result.hasValue());
    QVERIFY(observedPolicy);
    QVERIFY(!result.value().selection);
    QCOMPARE(preferences.writeCount, 0);
}

void QtFileDialogPresenterTests::directorySelectionNormalizesAndRemembersAcceptedDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString selectedDirectory = temporaryDirectory.filePath("選択先");
    QVERIFY(QDir().mkpath(selectedDirectory));
    const QString aliasDirectory = temporaryDirectory.filePath("alias");
    const bool linkCreated = QFile::link(selectedDirectory, aliasDirectory);
    const QString requested = linkCreated ? aliasDirectory : selectedDirectory;
    const QString expected = QFileInfo(selectedDirectory).canonicalFilePath();

    FakeDirectoryPreferences preferences;
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Qt
        );
    bool observedMode = false;
    bool acceptedWithOpenButton = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            QFileDialog* dialog = activeFileDialog();
            if (!dialog)
            {
                return;
            }
            observedMode = dialog->acceptMode() == QFileDialog::AcceptOpen
                && dialog->fileMode() == QFileDialog::Directory
                && dialog->testOption(QFileDialog::ShowDirsOnly);
            dialog->selectFile(requested);
            acceptedWithOpenButton = clickAcceptButton(
                dialog,
                QDialogButtonBox::Open
                );
        }
        );
    const auto result = presenter.selectDirectory(SelectDirectoryDialogRequest{
        .purpose = FileDialogPurpose::SubPrepPackage,
        .title = "Select directory",
        .initialDirectory = FileDialogPath(utf8(temporaryDirectory.path()))
    });

    QVERIFY(result.hasValue());
    QVERIFY(observedMode);
    QVERIFY(acceptedWithOpenButton);
    QVERIFY(result.value().selection);
    QCOMPARE(result.value().selection->value(), utf8(expected));
    QCOMPARE(preferences.writeCount, 1);
    QCOMPARE(preferences.lastWrittenDirectory, utf8(expected));
}

void QtFileDialogPresenterTests::preferenceFailureKeepsAcceptedSelectionAndReturnsWarning()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString selectedFile = temporaryDirectory.filePath("report.pdf");
    FakeDirectoryPreferences preferences;
    preferences.writeError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "Preference disk is read-only.",
        .recoverable = true
    };
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Qt
        );
    bool acceptedWithSaveButton = false;

    QTimer::singleShot(
        0,
        [&]()
        {
            if (QFileDialog* dialog = activeFileDialog())
            {
                dialog->selectFile(selectedFile);
                acceptedWithSaveButton = clickAcceptButton(
                    dialog,
                    QDialogButtonBox::Save
                    );
            }
        }
        );
    const auto result = presenter.saveFile(SaveFileDialogRequest{
        .purpose = FileDialogPurpose::ExportReport,
        .title = "Save report",
        .initialDirectory = FileDialogPath(utf8(temporaryDirectory.path()))
    });

    QVERIFY(result.hasValue());
    QVERIFY(acceptedWithSaveButton);
    QVERIFY(result.value().selection);
    QCOMPARE(
        result.value().selection->path.value(),
        utf8(QDir(QFileInfo(temporaryDirectory.path()).canonicalFilePath())
            .filePath(QStringLiteral("report.pdf")))
        );
    QVERIFY(result.value().preferenceWarning);
    QCOMPARE(
        result.value().preferenceWarning->message,
        std::string("Preference disk is read-only.")
        );
}

void QtFileDialogPresenterTests::presenterReportsWrongThreadAsTechnicalFailure()
{
    FakeDirectoryPreferences preferences;
    QtFileDialogPresenter presenter(
        preferences,
        nullptr,
        FileDialogBackend::Qt
        );
    using Response = FileDialogResponse<FileDialogPath>;
    std::optional<Domain::Result<Response>> result;
    std::thread worker([&]()
    {
        result = presenter.openOneFile(OpenFileDialogRequest{});
    });
    worker.join();

    QVERIFY(result);
    QVERIFY(!result->hasValue());
    QCOMPARE(result->error().code, Domain::ErrorCode::Technical);
    QCOMPARE(preferences.readCount, 0);
    QCOMPARE(preferences.writeCount, 0);
}

QTEST_MAIN(QtFileDialogPresenterTests)
#include "qt_file_dialog_presenter_tests.moc"
