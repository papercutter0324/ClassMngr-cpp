#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "core/application_services.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QtTest>

class MyWorkspacePageTests : public QObject
{
    Q_OBJECT

private slots:
    void createsNamedTabsWithScheduleSelectedByDefault();
    void opensTabsThroughNamedIdentifiers();
    void preservesChildPagesWhenSwitchingTabs();
    void createsCalendarOnlyWhenItsTabIsOpened();
    void isAvailableAsALazyTopLevelPage();
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

    page.openTab(WorkspaceTab::Calendar);
    QCOMPARE(page.currentTab(), WorkspaceTab::Calendar);

    page.openTab(WorkspaceTab::Schedule);
    QCOMPARE(page.currentTab(), WorkspaceTab::Schedule);
}

void MyWorkspacePageTests::preservesChildPagesWhenSwitchingTabs()
{
    MyWorkspacePage page(nullptr);
    PersonalDetailsPage* const details = page.personalDetailsPage();
    SchedulePage* const schedule = page.schedulePage();

    page.openTab(WorkspaceTab::Details);
    page.openTab(WorkspaceTab::Calendar);
    page.openTab(WorkspaceTab::Schedule);

    QCOMPARE(page.personalDetailsPage(), details);
    QCOMPARE(page.schedulePage(), schedule);
}

void MyWorkspacePageTests::createsCalendarOnlyWhenItsTabIsOpened()
{
    MyWorkspacePage page(nullptr);

    QVERIFY(!page.calendarPage());

    page.openTab(WorkspaceTab::Calendar);

    QVERIFY(page.calendarPage());
    QCOMPARE(page.currentTab(), WorkspaceTab::Calendar);
}

void MyWorkspacePageTests::isAvailableAsALazyTopLevelPage()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    QVERIFY(!pages.isPageInstantiated(PageType::MyWorkspace));

    pages.showPage(PageType::MyWorkspace);

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
