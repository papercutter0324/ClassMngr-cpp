#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/basepage.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QDialog>
#include <QFileInfo>
#include <QFrame>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <utility>

namespace
{
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

class MainWindowInitialSetupEmptyStateNavigationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void emptyStateSetupButtonOpensWizardAndReturnsToMyWorkspaceSchedule();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowInitialSetupEmptyStateNavigationTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowInitialSetupEmptyStateNavigationTests::
emptyStateSetupButtonOpensWizardAndReturnsToMyWorkspaceSchedule()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString expectedProfilePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("initial-setup.tps"))
        ).absoluteFilePath();
    QVERIFY(!QFileInfo::exists(expectedProfilePath));

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(expectedProfilePath);
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
    QVERIFY(!services->hasOpenDatabase());
    QVERIFY(services->currentDatabasePath().isEmpty());

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));

    CampusDashboardPage* const campusPage = pages->campusDashboard();
    QVERIFY(campusPage);
    QCOMPARE(
        campusPage->currentSectionKey(),
        QStringLiteral("campus_information")
        );

    BasePage* const noDatabasePage = qobject_cast<BasePage*>(
        pages->currentWidget()
        );
    QVERIFY(noDatabasePage);

    QFrame* const noDatabaseBanner =
        noDatabasePage->findChild<QFrame*>(
            QStringLiteral("noDatabaseBanner")
            );
    QVERIFY(noDatabaseBanner);
    QVERIFY(noDatabaseBanner->isVisible());

    QPushButton* const setupButton = noDatabasePage->findChild<QPushButton*>(
        QStringLiteral("noDatabaseSetupButton")
        );
    QVERIFY(setupButton);
    QVERIFY(setupButton->isVisible());
    QVERIFY(setupButton->isEnabled());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList initialSidebarSelection{
        QStringLiteral("campus_info"),
        QStringLiteral("campus_information")
    };
    QCOMPARE(sidebar->selectedKeys(), initialSidebarSelection);

    QSignalSpy pageRequestSpy(
        noDatabasePage,
        &BasePage::initialSetupRequested
        );
    QSignalSpy pageManagerRequestSpy(
        pages,
        &PageManager::initialSetupRequested
        );
    QVERIFY(pageRequestSpy.isValid());
    QVERIFY(pageManagerRequestSpy.isValid());

    bool realWizardWasVisible = false;
    bool realWizardWasAccepted = false;
    int wizardResult = QDialog::Rejected;
    QTimer::singleShot(0, &window, [&]()
    {
        auto* const wizard = qobject_cast<InitialSetupWizard*>(
            QApplication::activeModalWidget()
            );
        if (!wizard)
        {
            return;
        }

        realWizardWasVisible = wizard->isVisible();
        wizard->accept();
        wizardResult = wizard->result();
        realWizardWasAccepted = wizardResult == QDialog::Accepted;
    });

    QTest::mouseClick(
        setupButton,
        Qt::LeftButton,
        Qt::NoModifier
        );
    QApplication::processEvents();

    QCOMPARE(pageRequestSpy.count(), 1);
    QCOMPARE(pageManagerRequestSpy.count(), 1);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    const SaveFileRequest& saveRequest =
        fileDialogs.saveFileRequests.constFirst();
    QVERIFY(saveRequest.parent == &window);
    QCOMPARE(
        static_cast<int>(saveRequest.purpose),
        static_cast<int>(FileDialogPurpose::TeacherProfile)
        );
    QCOMPARE(
        saveRequest.nameFilters,
        QStringList{QStringLiteral("ClassMngr Teacher Profile (*.tps)")}
        );
    QCOMPARE(saveRequest.defaultSuffix, QStringLiteral("tps"));
    QVERIFY(realWizardWasVisible);
    QVERIFY(realWizardWasAccepted);
    QCOMPARE(wizardResult, static_cast<int>(QDialog::Accepted));

    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), expectedProfilePath);
    QVERIFY(QFileInfo::exists(expectedProfilePath));

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspacePage = pages->myWorkspacePage();
    QVERIFY(workspacePage);
    QCOMPARE(workspacePage->currentTab(), WorkspaceTab::Schedule);
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
    QVERIFY(!noDatabaseBanner->isVisible());
}

QTEST_MAIN(MainWindowInitialSetupEmptyStateNavigationTests)

#include "mainwindow_initial_setup_empty_state_navigation_tests.moc"
