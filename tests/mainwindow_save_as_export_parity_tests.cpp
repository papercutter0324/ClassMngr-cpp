#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QFileInfo>
#include <QPdfDocument>
#include <QPushButton>
#include <QTemporaryDir>
#include <QStringList>
#include <QTimer>
#include <QtTest>

#include <optional>
#include <utility>

namespace
{
class FileDialogServiceScope final
{
public:
    explicit FileDialogServiceScope(IFileDialogService* service)
    {
        DialogServices::setFileDialogServiceForTesting(service);
    }

    ~FileDialogServiceScope()
    {
        DialogServices::setFileDialogServiceForTesting(nullptr);
    }
};

class UserPromptServiceScope final
{
public:
    explicit UserPromptServiceScope(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~UserPromptServiceScope()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};
}

class MainWindowSaveAsExportParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void saveAsActionMovesTheOpenSessionAndUpdatesRecentHistory();
    void saveAsActionCancellationPreservesOpenProfile();
    void exportActionPreservesWorkspaceAndUpdatesExportDirectoryPreference();
    void saveCurrentPageAsActionWritesSchedulePdf();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowSaveAsExportParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
}

void MainWindowSaveAsExportParityTests::init()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowSaveAsExportParityTests::cleanup()
{
    DialogServices::setFileDialogServiceForTesting(nullptr);
    DialogServices::setUserPromptServiceForTesting(nullptr);
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowSaveAsExportParityTests::
saveAsActionMovesTheOpenSessionAndUpdatesRecentHistory()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("source-workspace.tps"))
        ).absoluteFilePath();
    const QString requestedDestination = workspaceRoot.filePath(
        QStringLiteral("save-as-destination")
        );
    const QString destinationPath = QFileInfo(
        requestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(requestedDestination)
        );
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = sourcePath;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), sourcePath);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    SettingsManager& settings = SettingsManager::instance();
    QCOMPARE(settings.getRecentFiles(), QStringList{sourcePath});
    QCOMPARE(settings.getLastFile(), sourcePath);

    QVERIFY(window.actions().saveAsFile);
    QVERIFY(window.actions().saveAsFile->isEnabled());
    window.actions().saveAsFile->trigger();

    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    const SaveFileRequest request =
        fileDialogs.saveFileRequests.constFirst();
    QVERIFY(request.parent == &window);
    QCOMPARE(request.title, QStringLiteral("Save Teacher Profile"));
    QCOMPARE(request.defaultSuffix, QStringLiteral("tps"));

    QVERIFY(QFileInfo::exists(destinationPath));
    QCOMPARE(QFileInfo(destinationPath).suffix(), QStringLiteral("tps"));
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), destinationPath);
    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{destinationPath, sourcePath})
        );
    QCOMPARE(settings.getLastFile(), destinationPath);

    QVERIFY(window.actions().saveFile->isEnabled());
    QVERIFY(window.actions().saveAsFile->isEnabled());
    QVERIFY(window.actions().exportAsFile->isEnabled());
    QVERIFY(window.actions().closeFile->isEnabled());
}

void MainWindowSaveAsExportParityTests::
saveAsActionCancellationPreservesOpenProfile()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("source-workspace.tps"))
        ).absoluteFilePath();
    const QString destinationPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("cancelled-destination.tps"))
        ).absoluteFilePath();
    const QString sourceDirectory = QFileInfo(sourcePath).absolutePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    QCOMPARE(fileDialogs.scriptedSaveFiles.size(), 1);
    QVERIFY(!fileDialogs.scriptedSaveFiles.head().has_value());
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = sourcePath;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), sourcePath);
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QWidget* const currentWidget = pages->currentWidget();
    QVERIFY(currentWidget);
    const QString currentPageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!currentPageIdentifier.isEmpty());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList sidebarKeys = sidebar->selectedKeys();
    QCOMPARE(sidebarKeys, QStringList{QStringLiteral("my_workspace")});

    SettingsManager& settings = SettingsManager::instance();
    const QStringList sourceHistory{sourcePath};
    QCOMPARE(settings.getRecentFiles(), sourceHistory);
    QCOMPARE(settings.getLastFile(), sourcePath);
    QCOMPARE(settings.getLastDatabaseDirectory(), sourceDirectory);

    QVERIFY(!QFileInfo::exists(destinationPath));
    QAction* const saveAsAction = window.actions().saveAsFile;
    QVERIFY(saveAsAction);
    QVERIFY(saveAsAction->isEnabled());
    saveAsAction->trigger();
    QApplication::processEvents();

    QCOMPARE(fileDialogs.scriptedSaveFiles.size(), 0);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(fileDialogs.openFileRequests.size(), 0);
    QCOMPARE(fileDialogs.openFilesRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileWithOptionsRequests.size(), 0);
    QCOMPARE(fileDialogs.directoryRequests.size(), 0);

    const SaveFileRequest request =
        fileDialogs.saveFileRequests.constFirst();
    QVERIFY(request.parent == &window);
    QCOMPARE(request.title, QStringLiteral("Save Teacher Profile"));
    QVERIFY(request.purpose == FileDialogPurpose::TeacherProfile);
    QCOMPARE(request.initialDirectory, sourceDirectory);
    QCOMPARE(
        request.nameFilters,
        QStringList{QStringLiteral("ClassMngr Teacher Profile (*.tps)")}
        );
    QCOMPARE(request.defaultSuffix, QStringLiteral("tps"));

    QVERIFY(!QFileInfo::exists(destinationPath));
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), sourcePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), currentWidget);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QCOMPARE(sidebar->selectedKeys(), sidebarKeys);
    QCOMPARE(settings.getRecentFiles(), sourceHistory);
    QCOMPARE(settings.getLastFile(), sourcePath);
    QCOMPARE(settings.getLastDatabaseDirectory(), sourceDirectory);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void MainWindowSaveAsExportParityTests::
exportActionPreservesWorkspaceAndUpdatesExportDirectoryPreference()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("source-workspace.tps"))
        ).absoluteFilePath();
    const QString requestedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/workspace-export")
        );
    const QString destinationPath = QFileInfo(
        requestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString sourceDirectory = QFileInfo(sourcePath).absolutePath();
    const QString exportDirectory = QFileInfo(destinationPath).absolutePath();
    QVERIFY(sourceDirectory != exportDirectory);

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(requestedDestination)
        );
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = sourcePath;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), sourcePath);
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Details);
    QApplication::processEvents();
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList selectedSidebarKeys = sidebar->selectedKeys();
    QCOMPARE(
        selectedSidebarKeys,
        QStringList{QStringLiteral("my_workspace")}
        );

    const QString activePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!activePageIdentifier.isEmpty());

    SettingsManager& settings = SettingsManager::instance();
    const QStringList sourceHistory{sourcePath};
    QCOMPARE(settings.getRecentFiles(), sourceHistory);
    QCOMPARE(settings.getLastFile(), sourcePath);
    QCOMPARE(settings.getLastDatabaseDirectory(), sourceDirectory);

    QVERIFY(window.actions().exportAsFile);
    QVERIFY(window.actions().exportAsFile->isEnabled());
    window.actions().exportAsFile->trigger();

    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    const SaveFileRequest request =
        fileDialogs.saveFileRequests.constFirst();
    QVERIFY(request.parent == &window);
    QCOMPARE(request.title, QStringLiteral("Export Teacher Profile As"));
    QCOMPARE(request.defaultSuffix, QStringLiteral("tps"));

    QVERIFY(QFileInfo::exists(destinationPath));
    QCOMPARE(QFileInfo(destinationPath).suffix(), QStringLiteral("tps"));
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), sourcePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), workspace);
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QCOMPARE(sidebar->selectedKeys(), selectedSidebarKeys);
    QCOMPARE(settings.getRecentFiles(), sourceHistory);
    QCOMPARE(settings.getLastFile(), sourcePath);
    QCOMPARE(settings.getLastDatabaseDirectory(), exportDirectory);
}

void MainWindowSaveAsExportParityTests::
saveCurrentPageAsActionWritesSchedulePdf()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("schedule-workspace.tps"))
        ).absoluteFilePath();
    const QString pdfPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("schedule-output.pdf"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(std::optional<QString>(pdfPath));
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = sourcePath;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), sourcePath);
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QWidget* const currentWidget = pages->currentWidget();
    QVERIFY(currentWidget);
    const QString currentPageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!currentPageIdentifier.isEmpty());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList sidebarKeys = sidebar->selectedKeys();
    QCOMPARE(sidebarKeys, QStringList{QStringLiteral("my_workspace")});

    QAction* const saveCurrentPageAsAction =
        window.actions().saveCurrentPageAs;
    QVERIFY(saveCurrentPageAsAction);
    QVERIFY(saveCurrentPageAsAction->isEnabled());

    bool dialogTimerRan = false;
    bool schedulePrintDialogObserved = false;
    bool saveAsButtonFound = false;
    bool saveAsButtonClicked = false;
    QString dialogTimerError;
    QTimer dialogActionTimer;
    dialogActionTimer.setSingleShot(true);
    QObject::connect(
        &dialogActionTimer,
        &QTimer::timeout,
        &window,
        [&]()
        {
            dialogTimerRan = true;
            QWidget* const activeModal = QApplication::activeModalWidget();
            auto* const dialog = qobject_cast<SchedulePrintDialog*>(activeModal);
            if (!dialog)
            {
                dialogTimerError = QStringLiteral(
                    "Save Current Page As did not open SchedulePrintDialog."
                    );
                if (auto* wrongDialog = qobject_cast<QDialog*>(activeModal))
                {
                    wrongDialog->reject();
                }
                return;
            }
            schedulePrintDialogObserved = true;

            QPushButton* const saveAsButton = dialog->findChild<QPushButton*>(
                QStringLiteral("schedulePrintSaveAsButton")
                );
            if (!saveAsButton)
            {
                dialogTimerError = QStringLiteral(
                    "SchedulePrintDialog has no schedulePrintSaveAsButton."
                    );
                dialog->reject();
                return;
            }
            saveAsButtonFound = true;
            QTest::mouseClick(saveAsButton, Qt::LeftButton);
            saveAsButtonClicked = true;
        }
        );
    dialogActionTimer.start(0);

    saveCurrentPageAsAction->trigger();
    QApplication::processEvents();

    QVERIFY2(dialogTimerRan, qPrintable(dialogTimerError));
    QVERIFY2(schedulePrintDialogObserved, qPrintable(dialogTimerError));
    QVERIFY2(saveAsButtonFound, qPrintable(dialogTimerError));
    QVERIFY2(saveAsButtonClicked, qPrintable(dialogTimerError));
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(fileDialogs.openFileRequests.size(), 0);
    QCOMPARE(fileDialogs.openFilesRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileWithOptionsRequests.size(), 0);
    QCOMPARE(fileDialogs.directoryRequests.size(), 0);

    const SaveFileRequest request = fileDialogs.saveFileRequests.constFirst();
    QCOMPARE(request.title, QStringLiteral("Save Schedule As"));
    QVERIFY(request.purpose == FileDialogPurpose::ExportReport);
    QCOMPARE(request.suggestedFileName, QStringLiteral("Schedule.pdf"));
    QCOMPARE(
        request.nameFilters,
        QStringList{QStringLiteral("PDF Documents (*.pdf)")}
        );
    QCOMPARE(request.defaultSuffix, QStringLiteral("pdf"));

    QVERIFY(QFileInfo::exists(pdfPath));
    QVERIFY(QFileInfo(pdfPath).size() > 0);
    QPdfDocument document;
    QCOMPARE(document.load(pdfPath), QPdfDocument::Error::None);
    QCOMPARE(document.status(), QPdfDocument::Status::Ready);
    QVERIFY(document.pageCount() >= 1);

    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), sourcePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), currentWidget);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QCOMPARE(sidebar->selectedKeys(), sidebarKeys);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

QTEST_MAIN(MainWindowSaveAsExportParityTests)

#include "mainwindow_save_as_export_parity_tests.moc"
