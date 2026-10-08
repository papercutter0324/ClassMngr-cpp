#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/repositories/class_info_repository.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/classes/ui/class_export_dialog.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "domain/models/class_info.h"
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
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSqlQuery>
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

int persistTeacher(
    ApplicationServices& services,
    Teacher& teacher
    )
{
    const auto saved = services.databaseSession()
        ->teacherRepository()
        ->createTeacher(teacher);
    if (!saved)
    {
        return -1;
    }

    teacher.id = *saved;
    return *saved;
}

int classTableRowCount(DatabaseSession* session)
{
    if (!session)
    {
        return -1;
    }

    QSqlQuery query(session->database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM classes"))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

QJsonObject classInfoSnapshot(const ClassInfo& info)
{
    const auto timesSnapshot = [](const QList<ClassTime>& times)
        {
            QJsonArray values;
            for (const ClassTime& time : times)
            {
                values.append(QJsonObject{
                    {QStringLiteral("day"), time.day},
                    {QStringLiteral("startTime"), time.startTime},
                    {QStringLiteral("endTime"), time.endTime}
                });
            }
            return values;
        };

    return {
        {QStringLiteral("classId"), info.classId},
        {QStringLiteral("teacherId"), info.teacherId},
        {QStringLiteral("teacherKr"), info.teacherKr},
        {QStringLiteral("teacherEn"), info.teacherEn},
        {QStringLiteral("teacherPreferredName"), info.teacherPreferredName},
        {QStringLiteral("roomNumber"), info.roomNumber},
        {QStringLiteral("wifiName"), info.wifiName},
        {QStringLiteral("wifiPassword"), info.wifiPassword},
        {QStringLiteral("internetType"), info.internetType},
        {QStringLiteral("zoomId"), info.zoomId},
        {QStringLiteral("zoomPassword"), info.zoomPassword},
        {QStringLiteral("projectionType"), info.projectionType},
        {QStringLiteral("classGrade"), info.classGrade},
        {QStringLiteral("classLevel"), info.classLevel},
        {QStringLiteral("readingBook"), info.readingBook},
        {QStringLiteral("essayBook"), info.essayBook},
        {QStringLiteral("classColor"), info.classColor},
        {QStringLiteral("fontColor"), info.fontColor},
        {QStringLiteral("classTimes"), timesSnapshot(info.classTimes)},
        {QStringLiteral("intensiveTimes"), timesSnapshot(info.intensiveTimes)},
        {QStringLiteral("notes"), info.notes},
        {QStringLiteral("timeFillerActivities"), info.timeFillerActivities}
    };
}

struct ClassExportDialogObservation final
{
    bool dialogObserved = false;
    bool listFound = false;
    bool seededClassFound = false;
    bool seededClassChecked = false;
    bool exportButtonFound = false;
    bool exportButtonEnabled = false;
    bool exportButtonClicked = false;
    bool timedOut = false;
    bool fallbackRejectedModal = false;
    QList<int> listedClassIds;
    QList<int> selectedClassIds;
};

void triggerExportClassesWithDialogObserver(
    QWidget* timerContext,
    QAction* exportClassesAction,
    const int seededClassId,
    ClassExportDialogObservation& observation
    )
{
    QTimer poll;
    poll.setInterval(10);
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(&poll, &QTimer::timeout, timerContext, [&]
    {
        auto* dialog = qobject_cast<ClassExportDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                dialog = qobject_cast<ClassExportDialog*>(widget);
                if (dialog)
                {
                    break;
                }
            }
        }
        if (!dialog)
        {
            return;
        }

        observation.dialogObserved = true;
        auto* const classList = dialog->findChild<QListWidget*>(
            QStringLiteral("classExportList")
            );
        observation.listFound = classList != nullptr;

        QListWidgetItem* seededItem = nullptr;
        if (classList)
        {
            for (int index = 0; index < classList->count(); ++index)
            {
                QListWidgetItem* const item = classList->item(index);
                const int classId = item->data(Qt::UserRole).toInt();
                observation.listedClassIds.append(classId);
                if (classId == seededClassId)
                {
                    seededItem = item;
                }
            }
        }
        observation.seededClassFound = seededItem != nullptr;
        if (seededItem)
        {
            seededItem->setCheckState(Qt::Checked);
            observation.seededClassChecked =
                seededItem->checkState() == Qt::Checked;
        }

        observation.selectedClassIds = dialog->selectedClassIds();
        auto* const exportButton = dialog->findChild<QPushButton*>(
            QStringLiteral("exportClassesButton")
            );
        observation.exportButtonFound = exportButton != nullptr;
        observation.exportButtonEnabled =
            exportButton && exportButton->isEnabled();

        if (observation.seededClassFound
            && observation.seededClassChecked
            && observation.exportButtonEnabled
            && observation.selectedClassIds == QList<int>{seededClassId})
        {
            observation.exportButtonClicked = true;
            poll.stop();
            exportButton->click();
            return;
        }

        poll.stop();
        dialog->reject();
    });
    QObject::connect(&timeout, &QTimer::timeout, timerContext, [&]
    {
        observation.timedOut = true;
        poll.stop();

        QDialog* dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                auto* candidate = qobject_cast<QDialog*>(widget);
                if (candidate && candidate->isModal() && candidate->isVisible())
                {
                    dialog = candidate;
                    break;
                }
            }
        }
        if (dialog)
        {
            observation.fallbackRejectedModal = true;
            dialog->reject();
        }
    });

    poll.start();
    timeout.start(5000);
    exportClassesAction->trigger();
    poll.stop();
    timeout.stop();
}
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
    void exportClassesActionReachesJsonPickerAndCancellationIsSilent();
    void exportClassesActionWritesSelectedClassPackage();
    void newTeacherActionShowsRequiredNameWarningWithoutNavigation();

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

void MainWindowCloseFileParityTests::
exportClassesActionReachesJsonPickerAndCancellationIsSilent()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("export-classes-cancel.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김수출");
    teacher.teacherEn = QStringLiteral("F432 Export Teacher");
    teacher.preferredRomanization = QStringLiteral("Export Teacher Roman");
    teacher.preferredName = QStringLiteral("Export Teacher");
    teacher.roomNumber = QStringLiteral("Room 32");
    teacher.birthday = QStringLiteral("07-19");
    teacher.phoneNumber = QStringLiteral("010-4321-8765");
    teacher.wifiName = QStringLiteral("Export Teacher Wi-Fi");
    teacher.wifiPassword = QStringLiteral("Export Teacher Wi-Fi password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("export.teacher.zoom");
    teacher.zoomPassword = QStringLiteral("Export Teacher Zoom password");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.notes = QStringLiteral("F432 assigned export fixture");
    const int teacherId = persistTeacher(seedServices, teacher);
    QVERIFY(teacherId > 0);

    const auto createdClass = seedServices.classService()->create(
        QStringLiteral("F432 Export target class")
        );
    QVERIFY(createdClass);
    const int seededClassId = *createdClass;

    ClassInfo seededClassInfo;
    seededClassInfo.classId = seededClassId;
    seededClassInfo.teacherId = teacherId;
    seededClassInfo.classGrade = QStringLiteral("E4");
    seededClassInfo.classLevel = QStringLiteral("Lyra");
    seededClassInfo.classTimes = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("5:00 PM")
        }
    };
    QVERIFY(seedServices.databaseSession()
        ->classInfoRepository()->saveClassInfo(seededClassInfo));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

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
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    ClassService* const classService = services->classService();
    QVERIFY(classService);
    const auto classesBefore = classService->classes();
    QVERIFY(classesBefore);
    QCOMPARE(classesBefore->size(), 1);
    QCOMPARE(classesBefore->constFirst().id, seededClassId);
    QCOMPARE(
        classesBefore->constFirst().name,
        QStringLiteral("F432 Export target class")
        );
    const auto classInfoBefore = classService->classInfo(seededClassId);
    QVERIFY(classInfoBefore);
    QCOMPARE(classInfoBefore->teacherId, teacherId);
    const int classTableRowsBefore = classTableRowCount(activeSession);
    QVERIFY(classTableRowsBefore > 0);

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

    QAction* const exportClassesAction = window.actions().exportClasses;
    QVERIFY(exportClassesAction);
    QVERIFY(exportClassesAction->isEnabled());

    ClassExportDialogObservation dialogObservation;
    triggerExportClassesWithDialogObserver(
        &window,
        exportClassesAction,
        seededClassId,
        dialogObservation
        );

    QVERIFY(dialogObservation.dialogObserved);
    QVERIFY(dialogObservation.listFound);
    QVERIFY(dialogObservation.seededClassFound);
    QVERIFY(dialogObservation.seededClassChecked);
    QVERIFY(dialogObservation.exportButtonFound);
    QVERIFY(dialogObservation.exportButtonEnabled);
    QVERIFY(dialogObservation.exportButtonClicked);
    QVERIFY(!dialogObservation.timedOut);
    QVERIFY(!dialogObservation.fallbackRejectedModal);
    QCOMPARE(
        dialogObservation.selectedClassIds,
        (QList<int>{seededClassId})
        );

    QCOMPARE(fileDialogs.scriptedSaveFiles.size(), 0);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(fileDialogs.openFileRequests.size(), 0);
    QCOMPARE(fileDialogs.openFilesRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileWithOptionsRequests.size(), 0);
    QCOMPARE(fileDialogs.directoryRequests.size(), 0);
    const SaveFileRequest request = fileDialogs.saveFileRequests.constFirst();
    QCOMPARE(request.parent, static_cast<QWidget*>(sidebar));
    QCOMPARE(request.title, QStringLiteral("Export Classes"));
    QVERIFY(request.purpose == FileDialogPurpose::ClassTransfer);
    QCOMPARE(
        request.initialDirectory,
        QFileInfo(workspacePath).absolutePath()
        );
    QCOMPARE(request.suggestedFileName, QStringLiteral("Classes.json"));
    QCOMPARE(
        request.nameFilters,
        QStringList{QStringLiteral("JSON Files (*.json)")}
        );
    QCOMPARE(request.defaultSuffix, QStringLiteral("json"));
    QVERIFY(fileDialogs.scriptedOpenFiles.isEmpty());
    QVERIFY(fileDialogs.scriptedOpenFileLists.isEmpty());
    QVERIFY(fileDialogs.scriptedSaveFileSelections.isEmpty());
    QVERIFY(fileDialogs.scriptedDirectories.isEmpty());

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), currentWidget);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QCOMPARE(sidebar->selectedKeys(), sidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(exportClassesAction->isEnabled());

    const auto classesAfter = classService->classes();
    QVERIFY(classesAfter);
    QCOMPARE(classesAfter->size(), classesBefore->size());
    QCOMPARE(classesAfter->constFirst().id, classesBefore->constFirst().id);
    QCOMPARE(classesAfter->constFirst().name, classesBefore->constFirst().name);
    const auto classInfoAfter = classService->classInfo(seededClassId);
    QVERIFY(classInfoAfter);
    QCOMPARE(classInfoAfter->teacherId, classInfoBefore->teacherId);
    QCOMPARE(classInfoAfter->classGrade, classInfoBefore->classGrade);
    QCOMPARE(classInfoAfter->classLevel, classInfoBefore->classLevel);
    QCOMPARE(classInfoAfter->classTimes.size(), classInfoBefore->classTimes.size());
    for (int index = 0; index < classInfoBefore->classTimes.size(); ++index)
    {
        QCOMPARE(
            classInfoAfter->classTimes.at(index).day,
            classInfoBefore->classTimes.at(index).day
            );
        QCOMPARE(
            classInfoAfter->classTimes.at(index).startTime,
            classInfoBefore->classTimes.at(index).startTime
            );
        QCOMPARE(
            classInfoAfter->classTimes.at(index).endTime,
            classInfoBefore->classTimes.at(index).endTime
            );
    }
    QCOMPARE(classTableRowCount(activeSession), classTableRowsBefore);

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(prompts.scriptedChoices.isEmpty());
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(prompts.scriptedActionIds.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void MainWindowCloseFileParityTests::
newTeacherActionShowsRequiredNameWarningWithoutNavigation()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("new-teacher-validation.tps"))
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
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    TeacherService* const teacherService = services->teacherService();
    QVERIFY(teacherService);
    const auto teachersBefore = teacherService->teachers();
    QVERIFY(teachersBefore);
    QVERIFY(teachersBefore->isEmpty());

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QVERIFY(!pages->isPageInstantiated(PageType::TeacherInfo));
    QVERIFY(pages->teacherPage() == nullptr);
    QWidget* const currentWidget = pages->currentWidget();
    QVERIFY(currentWidget);
    const QString currentPageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!currentPageIdentifier.isEmpty());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList sidebarKeys = sidebar->selectedKeys();
    QCOMPARE(sidebarKeys, QStringList{QStringLiteral("my_workspace")});

    QAction* const newTeacherAction = window.actions().newTeacher;
    QVERIFY(newTeacherAction);
    QVERIFY(newTeacherAction->isEnabled());
    newTeacherAction->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Add Teacher"));
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QVERIFY(warning.details.contains(QStringLiteral("teacher.name.required")));
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(prompts.scriptedChoices.isEmpty());
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(prompts.scriptedActionIds.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    const auto teachersAfter = teacherService->teachers();
    QVERIFY(teachersAfter);
    QVERIFY(teachersAfter->isEmpty());

    QVERIFY(!pages->isCurrentPage(PageType::TeacherInfo));
    QVERIFY(!pages->isPageInstantiated(PageType::TeacherInfo));
    QVERIFY(pages->teacherPage() == nullptr);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), currentWidget);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QCOMPARE(sidebar->selectedKeys(), sidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(newTeacherAction->isEnabled());
}

void MainWindowCloseFileParityTests::
exportClassesActionWritesSelectedClassPackage()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("export-classes-success.tps"))
        ).absoluteFilePath();
    const QString exportPath = workspaceRoot.filePath(
        QStringLiteral("selected-classes.json")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김수출성공");
    teacher.teacherEn = QStringLiteral("F440 Export Teacher");
    teacher.preferredRomanization = QStringLiteral("Export Teacher Roman");
    teacher.preferredName = QStringLiteral("Export Teacher");
    teacher.roomNumber = QStringLiteral("Room 44");
    teacher.birthday = QStringLiteral("08-14");
    teacher.phoneNumber = QStringLiteral("010-4400-8844");
    teacher.wifiName = QStringLiteral("F440 Teacher Wi-Fi");
    teacher.wifiPassword = QStringLiteral("F440 Teacher Wi-Fi password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("f440.export.teacher");
    teacher.zoomPassword = QStringLiteral("F440 Zoom password");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.notes = QStringLiteral("F440 assigned export teacher");
    const int teacherId = persistTeacher(seedServices, teacher);
    QVERIFY(teacherId > 0);

    const QString targetClassName = QStringLiteral("F440 Selected Target");
    const QString unselectedClassName = QStringLiteral("F440 Unselected Class");
    const auto targetClass = seedServices.classService()->create(
        targetClassName
        );
    const auto unselectedClass = seedServices.classService()->create(
        unselectedClassName
        );
    QVERIFY(targetClass);
    QVERIFY(unselectedClass);
    const int targetClassId = targetClass.value();
    const int unselectedClassId = unselectedClass.value();
    QVERIFY(targetClassId > 0);
    QVERIFY(unselectedClassId > 0);

    ClassInfo targetInfo;
    targetInfo.classId = targetClassId;
    targetInfo.teacherId = teacherId;
    targetInfo.classGrade = QStringLiteral("E4");
    targetInfo.classLevel = QStringLiteral("Lyra");
    targetInfo.readingBook = QStringLiteral("F440 Target Reader");
    targetInfo.essayBook = QStringLiteral("F440 Target Essay");
    targetInfo.classColor = QStringLiteral("#DDEEFF");
    targetInfo.fontColor = QStringLiteral("#112233");
    targetInfo.classTimes = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:30 PM"),
            QStringLiteral("5:20 PM")
        }
    };
    targetInfo.intensiveTimes = {
        {
            QStringLiteral("Thursday"),
            QStringLiteral("2:00 PM"),
            QStringLiteral("3:00 PM")
        }
    };
    targetInfo.notes = QStringLiteral("F440 selected target notes");
    targetInfo.timeFillerActivities = QStringLiteral("F440 target filler");
    QVERIFY(seedServices.databaseSession()
        ->classInfoRepository()->saveClassInfo(targetInfo));

    ClassInfo unselectedInfo;
    unselectedInfo.classId = unselectedClassId;
    unselectedInfo.classGrade = QStringLiteral("E3");
    unselectedInfo.classLevel = QStringLiteral("Pegasus");
    unselectedInfo.readingBook = QStringLiteral("F440 Unselected Reader");
    unselectedInfo.essayBook = QStringLiteral("F440 Unselected Essay");
    unselectedInfo.notes = QStringLiteral("F440 unselected class notes");
    unselectedInfo.timeFillerActivities = QStringLiteral("F440 unselected filler");
    unselectedInfo.classTimes = {
        {
            QStringLiteral("Friday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:50 PM")
        }
    };
    QVERIFY(seedServices.databaseSession()
        ->classInfoRepository()->saveClassInfo(unselectedInfo));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(exportPath);
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

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
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    ClassService* const classService = services->classService();
    QVERIFY(classService);
    const auto classesBefore = classService->classes();
    QVERIFY(classesBefore);
    QCOMPARE(classesBefore->size(), 2);
    QHash<int, QString> classNamesBefore;
    for (const Classroom& classroom : *classesBefore)
    {
        classNamesBefore.insert(classroom.id, classroom.name);
    }
    QCOMPARE(classNamesBefore.size(), 2);
    QCOMPARE(classNamesBefore.value(targetClassId), targetClassName);
    QCOMPARE(classNamesBefore.value(unselectedClassId), unselectedClassName);

    const auto targetInfoBefore = classService->classInfo(targetClassId);
    const auto unselectedInfoBefore = classService->classInfo(unselectedClassId);
    QVERIFY(targetInfoBefore);
    QVERIFY(unselectedInfoBefore);
    const QJsonObject targetInfoSnapshot = classInfoSnapshot(*targetInfoBefore);
    const QJsonObject unselectedInfoSnapshot =
        classInfoSnapshot(*unselectedInfoBefore);
    QCOMPARE(targetInfoBefore->teacherId, teacherId);
    QCOMPARE(unselectedInfoBefore->teacherId, -1);
    const int classTableRowsBefore = classTableRowCount(activeSession);
    QVERIFY(classTableRowsBefore > 0);

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

    QAction* const exportClassesAction = window.actions().exportClasses;
    QVERIFY(exportClassesAction);
    QVERIFY(exportClassesAction->isEnabled());

    ClassExportDialogObservation dialogObservation;
    triggerExportClassesWithDialogObserver(
        &window,
        exportClassesAction,
        targetClassId,
        dialogObservation
        );

    QVERIFY(dialogObservation.dialogObserved);
    QVERIFY(dialogObservation.listFound);
    QVERIFY(dialogObservation.listedClassIds.contains(targetClassId));
    QVERIFY(dialogObservation.listedClassIds.contains(unselectedClassId));
    QCOMPARE(dialogObservation.listedClassIds.size(), 2);
    QVERIFY(dialogObservation.seededClassFound);
    QVERIFY(dialogObservation.seededClassChecked);
    QVERIFY(dialogObservation.exportButtonFound);
    QVERIFY(dialogObservation.exportButtonEnabled);
    QVERIFY(dialogObservation.exportButtonClicked);
    QVERIFY(!dialogObservation.timedOut);
    QVERIFY(!dialogObservation.fallbackRejectedModal);
    QCOMPARE(dialogObservation.selectedClassIds, QList<int>{targetClassId});

    QVERIFY(fileDialogs.scriptedSaveFiles.isEmpty());
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(fileDialogs.openFileRequests.size(), 0);
    QCOMPARE(fileDialogs.openFilesRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileWithOptionsRequests.size(), 0);
    QCOMPARE(fileDialogs.directoryRequests.size(), 0);
    const SaveFileRequest saveRequest = fileDialogs.saveFileRequests.constFirst();
    QCOMPARE(saveRequest.parent, static_cast<QWidget*>(sidebar));
    QCOMPARE(saveRequest.title, QStringLiteral("Export Classes"));
    QCOMPARE(saveRequest.purpose, FileDialogPurpose::ClassTransfer);
    QCOMPARE(
        saveRequest.initialDirectory,
        QFileInfo(workspacePath).absolutePath()
        );
    QCOMPARE(saveRequest.suggestedFileName, QStringLiteral("Classes.json"));
    QCOMPARE(
        saveRequest.nameFilters,
        QStringList{QStringLiteral("JSON Files (*.json)")}
        );
    QCOMPARE(saveRequest.defaultSuffix, QStringLiteral("json"));
    QVERIFY(fileDialogs.scriptedOpenFiles.isEmpty());
    QVERIFY(fileDialogs.scriptedOpenFileLists.isEmpty());
    QVERIFY(fileDialogs.scriptedSaveFileSelections.isEmpty());
    QVERIFY(fileDialogs.scriptedDirectories.isEmpty());

    QVERIFY(QFileInfo::exists(exportPath));
    QFile exportFile(exportPath);
    QVERIFY2(
        exportFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(exportFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument exportDocument = QJsonDocument::fromJson(
        exportFile.readAll(),
        &parseError
        );
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(exportDocument.isObject());
    const QJsonObject package = exportDocument.object();
    QCOMPARE(package.value(QStringLiteral("format")).toString(),
             QStringLiteral("ClassMngr Classes"));
    QCOMPARE(package.value(QStringLiteral("version")).toInt(-1), 1);

    const QString exportedAtText = package.value(
        QStringLiteral("exported_at_utc")
        ).toString();
    const QDateTime exportedAt = QDateTime::fromString(
        exportedAtText,
        Qt::ISODateWithMs
        );
    QVERIFY(exportedAt.isValid());
    QCOMPARE(exportedAt.timeSpec(), Qt::UTC);

    const QJsonArray exportedClasses = package.value(
        QStringLiteral("classes")
        ).toArray();
    QCOMPARE(exportedClasses.size(), 1);
    const QJsonObject exportedClass = exportedClasses.at(0).toObject();
    QCOMPARE(
        exportedClass.value(QStringLiteral("name")).toString(),
        targetClassName
        );
    QVERIFY(
        exportedClass.value(QStringLiteral("name")).toString()
            != unselectedClassName
        );
    const QJsonObject exportedInfo = exportedClass.value(
        QStringLiteral("info")
        ).toObject();
    QCOMPARE(exportedInfo.value(QStringLiteral("class_grade")).toString(),
             QStringLiteral("E4"));
    QCOMPARE(exportedInfo.value(QStringLiteral("class_level")).toString(),
             QStringLiteral("Lyra"));
    QCOMPARE(exportedInfo.value(QStringLiteral("reading_book")).toString(),
             QStringLiteral("F440 Target Reader"));
    QCOMPARE(exportedInfo.value(QStringLiteral("essay_book")).toString(),
             QStringLiteral("F440 Target Essay"));
    QCOMPARE(exportedInfo.value(QStringLiteral("notes")).toString(),
             QStringLiteral("F440 selected target notes"));
    QCOMPARE(
        exportedInfo.value(QStringLiteral("time_filler_activities")).toString(),
        QStringLiteral("F440 target filler")
        );
    const QJsonArray exportedRegularTimes = exportedInfo.value(
        QStringLiteral("regular_times")
        ).toArray();
    QCOMPARE(exportedRegularTimes.size(), 1);
    const QJsonObject exportedRegularTime =
        exportedRegularTimes.at(0).toObject();
    QCOMPARE(exportedRegularTime.value(QStringLiteral("day")).toString(),
             QStringLiteral("Tuesday"));
    QCOMPARE(exportedRegularTime.value(QStringLiteral("start_time")).toString(),
             QStringLiteral("4:30 PM"));
    QCOMPARE(exportedRegularTime.value(QStringLiteral("end_time")).toString(),
             QStringLiteral("5:20 PM"));
    const QJsonArray exportedIntensiveTimes = exportedInfo.value(
        QStringLiteral("intensive_times")
        ).toArray();
    QCOMPARE(exportedIntensiveTimes.size(), 1);
    QCOMPARE(
        exportedIntensiveTimes.at(0).toObject()
            .value(QStringLiteral("day")).toString(),
        QStringLiteral("Thursday")
        );

    const QJsonArray exportedTeachers = package.value(
        QStringLiteral("teachers")
        ).toArray();
    QCOMPARE(exportedTeachers.size(), 1);
    const QJsonObject exportedTeacher = exportedTeachers.at(0).toObject();
    const QString exportedTeacherKey = exportedTeacher.value(
        QStringLiteral("key")
        ).toString();
    const QString classTeacherReference = exportedClass.value(
        QStringLiteral("teacher_ref")
        ).toString();
    QVERIFY(!exportedTeacherKey.isEmpty());
    QCOMPARE(classTeacherReference, exportedTeacherKey);
    QCOMPARE(exportedTeacher.value(QStringLiteral("teacher_kr")).toString(),
             teacher.teacherKr);
    QCOMPARE(exportedTeacher.value(QStringLiteral("teacher_en")).toString(),
             teacher.teacherEn);
    QCOMPARE(exportedTeacher.value(QStringLiteral("preferred_name")).toString(),
             teacher.preferredName);

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& information = prompts.messages.constFirst();
    QCOMPARE(information.parent, static_cast<QWidget*>(sidebar));
    QCOMPARE(information.title, QStringLiteral("Export Classes"));
    QCOMPARE(information.severity, PromptSeverity::Information);
    QCOMPARE(
        information.message,
        QStringLiteral("Exported 1 class(es) to:\n%1")
            .arg(QDir::toNativeSeparators(exportPath))
        );
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(prompts.scriptedChoices.isEmpty());
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(prompts.scriptedActionIds.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), currentWidget);
    QCOMPARE(pages->currentPageIdentifier(), currentPageIdentifier);
    QCOMPARE(sidebar->selectedKeys(), sidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QVERIFY(activeSession->isOpen());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(exportClassesAction->isEnabled());

    const auto classesAfter = classService->classes();
    QVERIFY(classesAfter);
    QCOMPARE(classesAfter->size(), classesBefore->size());
    QHash<int, QString> classNamesAfter;
    for (const Classroom& classroom : *classesAfter)
    {
        classNamesAfter.insert(classroom.id, classroom.name);
    }
    QCOMPARE(classNamesAfter, classNamesBefore);
    QCOMPARE(classTableRowCount(activeSession), classTableRowsBefore);

    const auto targetInfoAfter = classService->classInfo(targetClassId);
    const auto unselectedInfoAfter = classService->classInfo(unselectedClassId);
    QVERIFY(targetInfoAfter);
    QVERIFY(unselectedInfoAfter);
    QCOMPARE(classInfoSnapshot(*targetInfoAfter), targetInfoSnapshot);
    QCOMPARE(classInfoSnapshot(*unselectedInfoAfter), unselectedInfoSnapshot);
}

QTEST_MAIN(MainWindowCloseFileParityTests)

#include "mainwindow_close_file_parity_tests.moc"
