#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "core/application_services.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QFrame>
#include <QtTest>

class MyWorkspacePageTests : public QObject
{
    Q_OBJECT

private slots:
    void createsNamedTabsWithScheduleSelectedByDefault();
    void opensTabsThroughNamedIdentifiers();
    void preservesChildPagesWhenSwitchingTabs();
    void defersCalendarConstructionWhileOtherTabsAreOpen();
    void initializesCalendarWithoutChangingTheActiveWorkspaceTab();
    void showsOnlyTheWorkspaceBannerWhenNoDatabaseIsOpen();
    void isAvailableAsTheDefaultTopLevelPage();
    void receivesTheTopLevelDatabaseState();
};

void MyWorkspacePageTests::createsNamedTabsWithScheduleSelectedByDefault()
{
    MyWorkspacePage page(nullptr);

    auto* tabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("myWorkspaceTabs")
        );
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("My Details"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("My Schedule"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("Calendar"));
    QCOMPARE(page.currentTab(), WorkspaceTab::Schedule);
    QCOMPARE(tabs->currentIndex(), 1);
    QVERIFY(page.personalDetailsPage());
    QVERIFY(page.schedulePage());
}

void MyWorkspacePageTests::opensTabsThroughNamedIdentifiers()
{
    MyWorkspacePage page(nullptr);

    page.openTab(WorkspaceTab::Details);
    QCOMPARE(page.currentTab(), WorkspaceTab::Details);

    page.openTab(WorkspaceTab::Schedule);
    QCOMPARE(page.currentTab(), WorkspaceTab::Schedule);
}

void MyWorkspacePageTests::preservesChildPagesWhenSwitchingTabs()
{
    MyWorkspacePage page(nullptr);
    PersonalDetailsPage* const details = page.personalDetailsPage();
    SchedulePage* const schedule = page.schedulePage();

    page.openTab(WorkspaceTab::Details);
    page.openTab(WorkspaceTab::Schedule);

    QCOMPARE(page.personalDetailsPage(), details);
    QCOMPARE(page.schedulePage(), schedule);
}

void MyWorkspacePageTests::defersCalendarConstructionWhileOtherTabsAreOpen()
{
    MyWorkspacePage page(nullptr);

    QVERIFY(!page.calendarPage());

    page.openTab(WorkspaceTab::Details);
    page.openTab(WorkspaceTab::Schedule);

    QVERIFY(!page.calendarPage());
}

void MyWorkspacePageTests::initializesCalendarWithoutChangingTheActiveWorkspaceTab()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    MyWorkspacePage* const workspace = pages.myWorkspacePage();
    QVERIFY(workspace);

    for (const WorkspaceTab tab : {
             WorkspaceTab::Details,
             WorkspaceTab::Schedule,
             WorkspaceTab::Calendar
         })
    {
        workspace->openTab(tab);

        QVERIFY(pages.ensureCalendarPage());
        QCOMPARE(workspace->currentTab(), tab);
    }
}

void MyWorkspacePageTests::showsOnlyTheWorkspaceBannerWhenNoDatabaseIsOpen()
{
    MyWorkspacePage page(nullptr);
    page.resize(800, 600);
    page.setDatabaseOpen(false);
    page.show();

    auto* workspaceBanner = page.findChild<QFrame*>(
        QStringLiteral("noDatabaseBanner"),
        Qt::FindDirectChildrenOnly
        );
    QVERIFY(workspaceBanner);
    QVERIFY(workspaceBanner->isVisible());

    page.openTab(WorkspaceTab::Details);
    QCoreApplication::processEvents();

    auto* detailsBanner = page.personalDetailsPage()->findChild<QFrame*>(
        QStringLiteral("noDatabaseBanner"),
        Qt::FindDirectChildrenOnly
        );
    QVERIFY(detailsBanner);
    QVERIFY(!detailsBanner->isVisible());

    page.openTab(WorkspaceTab::Schedule);
    QCoreApplication::processEvents();

    auto* scheduleBanner = page.schedulePage()->findChild<QFrame*>(
        QStringLiteral("noDatabaseBanner"),
        Qt::FindDirectChildrenOnly
        );
    QVERIFY(scheduleBanner);
    QVERIFY(!scheduleBanner->isVisible());

    page.openTab(WorkspaceTab::Calendar);
    QCoreApplication::processEvents();

    QVERIFY(page.calendarPage());
    auto* calendarBanner = page.calendarPage()->findChild<QFrame*>(
        QStringLiteral("noDatabaseBanner"),
        Qt::FindDirectChildrenOnly
        );
    QVERIFY(calendarBanner);
    QVERIFY(!calendarBanner->isVisible());
}

void MyWorkspacePageTests::isAvailableAsTheDefaultTopLevelPage()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    QVERIFY(pages.isPageInstantiated(PageType::MyWorkspace));

    QVERIFY(pages.isCurrentPage(PageType::MyWorkspace));
    QVERIFY(pages.myWorkspacePage());
    QCOMPARE(
        pages.myWorkspacePage()->currentTab(),
        WorkspaceTab::Schedule
        );
}

void MyWorkspacePageTests::receivesTheTopLevelDatabaseState()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);
    pages.setDatabaseOpen(true);

    pages.showPage(PageType::MyWorkspace);

    QVERIFY(pages.outputCapabilities().printEnabled);
    QVERIFY(pages.outputCapabilities().saveAsEnabled);
}

QTEST_MAIN(MyWorkspacePageTests)

#include "my_workspace_page_tests.moc"
