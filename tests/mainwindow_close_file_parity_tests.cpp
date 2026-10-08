#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDate>
#include <QDialog>
#include <QFileInfo>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>

#include <optional>
#include <utility>

namespace
{
constexpr int KeyRole = Qt::UserRole + 4;

const QStringList& databaseSidebarSectionKeys()
{
    static const QStringList keys{
        QStringLiteral("my_workspace"),
        QStringLiteral("sub_prep"),
        QStringLiteral("classes"),
        QStringLiteral("co_teachers"),
        QStringLiteral("campus_staff")
    };
    return keys;
}

QTreeWidgetItem* findItemByKey(
    QTreeWidgetItem* parent,
    const QString& key
    )
{
    if (!parent)
    {
        return nullptr;
    }

    if (parent->data(0, KeyRole).toString() == key)
    {
        return parent;
    }

    for (int index = 0; index < parent->childCount(); ++index)
    {
        if (QTreeWidgetItem* const found = findItemByKey(
                parent->child(index),
                key
                ))
        {
            return found;
        }
    }

    return nullptr;
}

QTreeWidgetItem* findItemByKey(
    QTreeWidget* tree,
    const QString& key
    )
{
    if (!tree)
    {
        return nullptr;
    }

    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        if (QTreeWidgetItem* const found = findItemByKey(
                tree->topLevelItem(index),
                key
                ))
        {
            return found;
        }
    }

    return nullptr;
}

QLineEdit* personalNameEditor(PersonalDetailsPage* page)
{
    if (!page)
    {
        return nullptr;
    }

    const QList<QLineEdit*> editors = page->findChildren<QLineEdit*>();
    return editors.isEmpty() ? nullptr : editors.constFirst();
}

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
}

class MainWindowCloseFileParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void closeFileCancelPreservesDraftBeforeDiscardClosesWorkspace();
    void closeFileSavePersistsDraftBeforeClosingWorkspace();
    void upcomingBirthdaysActionShowsEntriesFromAllStaffDirectories();
    void importClassesActionRequestsJsonAndCancellationIsSilent();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowCloseFileParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
    qRegisterMetaType<NavigationData>();
}

void MainWindowCloseFileParityTests::
closeFileCancelPreservesDraftBeforeDiscardClosesWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

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
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Details);
    pages->setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);
    const QString draft = QStringLiteral("Unsaved workspace draft");
    nameEditor->setText(draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    for (const QString& key : databaseSidebarSectionKeys())
    {
        QTreeWidgetItem* const item = findItemByKey(tree, key);
        QVERIFY(item);
        QVERIFY(!item->isHidden());
    }

    const QList<QAction*> databaseBackedActions{
        window.actions().saveFile,
        window.actions().saveAsFile,
        window.actions().exportAsFile,
        window.actions().closeFile,
        window.actions().newClass,
        window.actions().deleteClass,
        window.actions().importClasses,
        window.actions().exportClasses,
        window.actions().newTeacher,
        window.actions().deleteTeacher,
        window.actions().upcomingBirthdays
    };
    // Entity-dependent actions may already be disabled in this empty workspace.
    QList<bool> databaseBackedActionEnabledStates;
    databaseBackedActionEnabledStates.reserve(databaseBackedActions.size());
    for (QAction* const action : databaseBackedActions)
    {
        QVERIFY(action);
        databaseBackedActionEnabledStates.append(action->isEnabled());
    }
    QVERIFY(window.actions().saveFile->isEnabled());
    QVERIFY(window.actions().saveAsFile->isEnabled());
    QVERIFY(window.actions().exportAsFile->isEnabled());
    QVERIFY(window.actions().closeFile->isEnabled());

    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
    const QString activePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!activePageIdentifier.isEmpty());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    QVERIFY(window.actions().closeFile->isEnabled());
    window.actions().closeFile->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QCOMPARE(nameEditor->text(), draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
    for (const QString& key : databaseSidebarSectionKeys())
    {
        QTreeWidgetItem* const item = findItemByKey(tree, key);
        QVERIFY(item);
        QVERIFY(!item->isHidden());
    }
    for (int index = 0; index < databaseBackedActions.size(); ++index)
    {
        QCOMPARE(
            databaseBackedActions.at(index)->isEnabled(),
            databaseBackedActionEnabledStates.at(index)
            );
    }

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    window.actions().closeFile->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 2);
    QVERIFY(!services->hasOpenDatabase());
    QVERIFY(services->currentDatabasePath().isEmpty());
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QVERIFY(pages->campusDashboard());
    QCOMPARE(
        pages->campusDashboard()->currentSectionKey(),
        QStringLiteral("campus_information")
        );
    QCOMPARE(
        sidebar->selectedKeys(),
        (QStringList{
            QStringLiteral("campus_info"),
            QStringLiteral("campus_information")
        })
        );

    for (const QString& key : databaseSidebarSectionKeys())
    {
        QTreeWidgetItem* const item = findItemByKey(tree, key);
        QVERIFY(item);
        QVERIFY(item->isHidden());
    }
    for (QAction* const action : databaseBackedActions)
    {
        QVERIFY(!action->isEnabled());
    }
}

void MainWindowCloseFileParityTests::
closeFileSavePersistsDraftBeforeClosingWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("save-close-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

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
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Details);
    pages->setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);
    const QString draft = QStringLiteral("Saved workspace profile draft");
    nameEditor->setText(draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );

    QAction* const closeFileAction = window.actions().closeFile;
    QVERIFY(closeFileAction);
    QVERIFY(closeFileAction->isEnabled());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Save
        );
    closeFileAction->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QCOMPARE(fileDialogs.openFileRequests.size(), 0);
    QCOMPARE(fileDialogs.openFilesRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileWithOptionsRequests.size(), 0);
    QCOMPARE(fileDialogs.directoryRequests.size(), 0);

    QVERIFY(!services->hasOpenDatabase());
    QVERIFY(services->currentDatabasePath().isEmpty());
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QVERIFY(pages->campusDashboard());
    QCOMPARE(
        pages->campusDashboard()->currentSectionKey(),
        QStringLiteral("campus_information")
        );
    QCOMPARE(
        sidebar->selectedKeys(),
        (QStringList{
            QStringLiteral("campus_info"),
            QStringLiteral("campus_information")
        })
        );

    const QList<QAction*> databaseBackedActions{
        window.actions().saveFile,
        window.actions().saveAsFile,
        window.actions().exportAsFile,
        window.actions().closeFile,
        window.actions().newClass,
        window.actions().deleteClass,
        window.actions().importClasses,
        window.actions().exportClasses,
        window.actions().newTeacher,
        window.actions().deleteTeacher,
        window.actions().upcomingBirthdays
    };
    for (QAction* const action : databaseBackedActions)
    {
        QVERIFY(action);
        QVERIFY(!action->isEnabled());
    }

    ApplicationServices reopenedServices;
    QVERIFY(reopenedServices.openDatabase(workspacePath));
    QVERIFY(reopenedServices.settingsService());
    QCOMPARE(
        reopenedServices.settingsService()->loadOrDefault(
            QStringLiteral("myInfo/name"),
            QString()
            ).toString(),
        draft
        );
    reopenedServices.closeDatabase();
}

void MainWindowCloseFileParityTests::
upcomingBirthdaysActionShowsEntriesFromAllStaffDirectories()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("upcoming-birthdays.tps"))
        ).absoluteFilePath();

    const QDate today = QDate::currentDate();
    QVERIFY(today.isValid());
    const auto birthdayForOffset = [&today](const int daysFromToday)
    {
        return today.addDays(daysFromToday).toString(
            QStringLiteral("MM-dd")
            );
    };

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));

    Teacher koreanTeacher;
    koreanTeacher.teacherKr = QStringLiteral("F425 Korean Teacher");
    koreanTeacher.teacherEn = QStringLiteral("F425 Korean Teacher");
    koreanTeacher.preferredName = QStringLiteral("F425 Korean Birthday");
    koreanTeacher.roomNumber = QStringLiteral("Room 2");
    koreanTeacher.birthday = birthdayForOffset(0);
    koreanTeacher.phoneNumber = QStringLiteral("010-1234-5678");
    koreanTeacher.wifiName = QStringLiteral("Teacher Wi-Fi");
    koreanTeacher.wifiPassword = QStringLiteral("Teacher password");
    koreanTeacher.internetType = QStringLiteral("Both");
    koreanTeacher.zoomId = QStringLiteral("teacher.zoom");
    koreanTeacher.zoomPassword = QStringLiteral("Zoom password");
    koreanTeacher.projectionType = QStringLiteral("Zoom");
    koreanTeacher.notes = QStringLiteral("Teacher notes");
    const auto koreanTeacherId = seedServices.databaseSession()
        ->teacherRepository()->createTeacher(koreanTeacher);
    QVERIFY(koreanTeacherId);
    QVERIFY(*koreanTeacherId > 0);

    const auto nativeEnglishSaved =
        seedServices.teacherService()->saveNativeEnglishTeacherDirectory(
            {{
                .name = QStringLiteral("F425 Native Birthday"),
                .position = QStringLiteral("NET"),
                .birthday = birthdayForOffset(1)
            }},
            {}
            );
    QVERIFY(nativeEnglishSaved);

    const auto gsTeamSaved = seedServices.teacherService()->saveGsTeamDirectory(
        {{
            .name = QStringLiteral("F425 GS Birthday"),
            .position = QStringLiteral("M1"),
            .birthday = birthdayForOffset(2)
        }},
        {}
        );
    QVERIFY(gsTeamSaved);
    seedServices.closeDatabase();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

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
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    auto* const activeSession = services->databaseSession();
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

    QAction* const upcomingBirthdaysAction =
        window.actions().upcomingBirthdays;
    QVERIFY(upcomingBirthdaysAction);
    QVERIFY(upcomingBirthdaysAction->isEnabled());

    const QString dismissalDateKey = QString::fromLatin1(
        SettingsManager::Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE
        );
    const QVariant dismissalDateBefore = SettingsManager::instance().get(
        dismissalDateKey
        );
    QVERIFY(!dismissalDateBefore.isValid());

    bool observerRan = false;
    bool modalFound = false;
    bool dialogWasRejected = false;
    bool dismissalCheckboxFound = false;
    bool dismissalCheckboxWasChecked = true;
    QString dialogObjectName;
    QString observerError;
    QStringList entryNames;
    QHash<QString, QString> entryDetailsByName;

    QTimer dialogObserver;
    dialogObserver.setSingleShot(true);
    QObject::connect(
        &dialogObserver,
        &QTimer::timeout,
        &window,
        [&]()
        {
            observerRan = true;
            auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                observerError = QStringLiteral(
                    "The Upcoming Birthdays action did not open a modal dialog."
                    );
                return;
            }
            modalFound = true;
            dialogObjectName = dialog->objectName();

            auto* const dismissForToday = dialog->findChild<QCheckBox*>(
                QStringLiteral("upcomingBirthdaysDismissForTodayCheck")
                );
            dismissalCheckboxFound = dismissForToday != nullptr;
            if (dismissForToday)
            {
                dismissalCheckboxWasChecked =
                    dismissForToday->isChecked();
            }

            if (dialogObjectName
                == QStringLiteral("upcomingBirthdaysDialog"))
            {
                for (QLabel* const nameLabel : dialog->findChildren<QLabel*>())
                {
                    if (!nameLabel->objectName().endsWith(
                            QStringLiteral("Name")))
                    {
                        continue;
                    }

                    entryNames.append(nameLabel->text());
                    QString detailObjectName = nameLabel->objectName();
                    detailObjectName.replace(
                        QStringLiteral("Name"),
                        QStringLiteral("Detail")
                        );
                    if (QLabel* const detailLabel = dialog->findChild<QLabel*>(
                            detailObjectName))
                    {
                        entryDetailsByName.insert(
                            nameLabel->text(),
                            detailLabel->text()
                            );
                    }
                }
            }
            else
            {
                observerError = QStringLiteral(
                    "The modal dialog was not Upcoming Birthdays."
                    );
            }

            dialog->reject();
            dialogWasRejected = true;
        }
        );
    dialogObserver.start(0);

    upcomingBirthdaysAction->trigger();
    QApplication::processEvents();
    dialogObserver.stop();

    QVERIFY2(observerRan, qPrintable(observerError));
    QVERIFY2(modalFound, qPrintable(observerError));
    QVERIFY2(dialogWasRejected, qPrintable(observerError));
    QCOMPARE(dialogObjectName, QStringLiteral("upcomingBirthdaysDialog"));
    QVERIFY(dismissalCheckboxFound);
    QVERIFY(!dismissalCheckboxWasChecked);

    QCOMPARE(entryNames.size(), 3);
    QVERIFY(entryNames.contains(QStringLiteral("F425 Korean Birthday")));
    QVERIFY(entryNames.contains(QStringLiteral("F425 Native Birthday")));
    QVERIFY(entryNames.contains(QStringLiteral("F425 GS Birthday")));
    QCOMPARE(entryDetailsByName.size(), 3);
    QVERIFY(entryDetailsByName.value(
        QStringLiteral("F425 Korean Birthday")
        ).contains(QStringLiteral("Korean Teacher")));
    QVERIFY(entryDetailsByName.value(
        QStringLiteral("F425 Native Birthday")
        ).contains(QStringLiteral("Native English Teacher")));
    QVERIFY(entryDetailsByName.value(
        QStringLiteral("F425 GS Birthday")
        ).contains(QStringLiteral("GS Team")));

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), currentWidget);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QCOMPARE(sidebar->selectedKeys(), sidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QCOMPARE(
        SettingsManager::instance().get(dismissalDateKey),
        dismissalDateBefore
        );
}

void MainWindowCloseFileParityTests::
importClassesActionRequestsJsonAndCancellationIsSilent()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("import-classes-cancel.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedOpenFiles.enqueue(std::nullopt);
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

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
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);
    ClassService* const classService = services->classService();
    QVERIFY(classService);

    const auto classesBefore = classService->classes();
    QVERIFY(classesBefore);
    QVERIFY(classesBefore->isEmpty());

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    QVERIFY(!workspace->hasUnsavedChanges());
    QWidget* const currentWidget = pages->currentWidget();
    QVERIFY(currentWidget);
    const QString currentPageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!currentPageIdentifier.isEmpty());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList sidebarKeys = sidebar->selectedKeys();
    QCOMPARE(sidebarKeys, QStringList{QStringLiteral("my_workspace")});

    QAction* const importClassesAction = window.actions().importClasses;
    QVERIFY(importClassesAction);
    QVERIFY(importClassesAction->isEnabled());
    importClassesAction->trigger();
    QApplication::processEvents();

    QCOMPARE(fileDialogs.openFileRequests.size(), 1);
    QCOMPARE(fileDialogs.openFilesRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileWithOptionsRequests.size(), 0);
    QCOMPARE(fileDialogs.directoryRequests.size(), 0);

    const OpenFileRequest request = fileDialogs.openFileRequests.constFirst();
    QVERIFY(request.purpose == FileDialogPurpose::ClassTransfer);
    QCOMPARE(
        request.initialDirectory,
        QFileInfo(workspacePath).absolutePath()
        );
    QCOMPARE(
        request.nameFilters,
        QStringList{QStringLiteral("JSON Files (*.json)")}
        );

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), currentWidget);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QCOMPARE(sidebar->selectedKeys(), sidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    const auto classesAfter = classService->classes();
    QVERIFY(classesAfter);
    QVERIFY(classesAfter->isEmpty());

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

QTEST_MAIN(MainWindowCloseFileParityTests)

#include "mainwindow_close_file_parity_tests.moc"
