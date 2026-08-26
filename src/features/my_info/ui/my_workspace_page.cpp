#include "my_workspace_page.h"

#include "features/calendar/ui/calendar_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QLabel>
#include <QVBoxLayout>

namespace
{
QLabel* createPlaceholder(QWidget* parent)
{
    auto* placeholder = new QLabel(parent);
    placeholder->setObjectName(QStringLiteral("workspaceTabPlaceholder"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    return placeholder;
}
}

MyWorkspacePage::MyWorkspacePage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    buildUi();
}

void MyWorkspacePage::openTab(WorkspaceTab tab)
{
    if (m_tabs)
    {
        m_tabs->setCurrentIndex(tabIndex(tab));
    }
}

WorkspaceTab MyWorkspacePage::currentTab() const
{
    if (!m_tabs)
    {
        return WorkspaceTab::Schedule;
    }

    switch (m_tabs->currentIndex())
    {
    case tabIndex(WorkspaceTab::Details):
        return WorkspaceTab::Details;

    case tabIndex(WorkspaceTab::Calendar):
        return WorkspaceTab::Calendar;

    case tabIndex(WorkspaceTab::Schedule):
    default:
        return WorkspaceTab::Schedule;
    }
}

PersonalDetailsPage* MyWorkspacePage::personalDetailsPage() const
{
    return m_personalDetailsPage;
}

SchedulePage* MyWorkspacePage::schedulePage() const
{
    return m_schedulePage;
}

CalendarPage* MyWorkspacePage::calendarPage() const
{
    return m_calendarPage;
}

void MyWorkspacePage::saveData()
{
    if (m_personalDetailsPage)
    {
        m_personalDetailsPage->saveData();
    }
}

bool MyWorkspacePage::saveChanges()
{
    return !m_personalDetailsPage
        || !m_personalDetailsPage->hasUnsavedChanges()
        || m_personalDetailsPage->saveChanges();
}

bool MyWorkspacePage::hasUnsavedChanges() const
{
    return m_personalDetailsPage
        && m_personalDetailsPage->hasUnsavedChanges();
}

void MyWorkspacePage::discardChanges()
{
    if (m_personalDetailsPage)
    {
        m_personalDetailsPage->discardChanges();
    }
}

void MyWorkspacePage::setSaveMode(SaveMode mode)
{
    if (m_personalDetailsPage)
    {
        m_personalDetailsPage->setSaveMode(mode);
    }

    if (m_schedulePage)
    {
        m_schedulePage->setSaveMode(mode);
    }
}

void MyWorkspacePage::refresh()
{
    BasePage::refresh();

    if (m_personalDetailsPage)
    {
        m_personalDetailsPage->refresh();
    }

    if (m_schedulePage)
    {
        m_schedulePage->refresh();
    }

    if (m_calendarPage)
    {
        m_calendarPage->refresh();
    }
}

void MyWorkspacePage::clearDatabaseState()
{
    if (m_personalDetailsPage)
    {
        m_personalDetailsPage->clearDatabaseState();
    }

    if (m_schedulePage)
    {
        m_schedulePage->clearDatabaseState();
    }

    if (m_calendarPage)
    {
        m_calendarPage->clearDatabaseState();
    }
}

void MyWorkspacePage::retranslateUi()
{
    if (m_pageHeader)
    {
        m_pageHeader->setTitle(tr("My Workspace"));
        m_pageHeader->setSubtitle(
            tr("Manage your personal details, schedule, and calendar.")
            );
    }

    if (m_tabs)
    {
        m_tabs->setTabText(tabIndex(WorkspaceTab::Details), tr("My Details"));
        m_tabs->setTabText(tabIndex(WorkspaceTab::Schedule), tr("My Schedule"));
        m_tabs->setTabText(tabIndex(WorkspaceTab::Calendar), tr("Calendar"));
    }

    if (m_calendarPlaceholder)
    {
        m_calendarPlaceholder->setText(
            tr("Calendar will load when you open this tab.")
            );
    }

    if (m_personalDetailsPage)
    {
        m_personalDetailsPage->BasePage::retranslateUi();
        m_personalDetailsPage->retranslateUi();
    }

    if (m_schedulePage)
    {
        m_schedulePage->BasePage::retranslateUi();
        m_schedulePage->retranslateUi();
    }

    if (m_calendarPage)
    {
        m_calendarPage->BasePage::retranslateUi();
        m_calendarPage->retranslateUi();
    }
}

void MyWorkspacePage::setDatabaseOpen(bool databaseOpen)
{
    BasePage::setDatabaseOpen(databaseOpen);

    if (m_personalDetailsPage)
    {
        m_personalDetailsPage->setDatabaseOpen(databaseOpen);
    }

    if (m_schedulePage)
    {
        m_schedulePage->setDatabaseOpen(databaseOpen);
    }

    if (m_calendarPage)
    {
        m_calendarPage->setDatabaseOpen(databaseOpen);
    }
}

PageOutputCapabilities MyWorkspacePage::outputCapabilities() const
{
    const BasePage* page = currentChildPage();
    return page ? page->outputCapabilities() : PageOutputCapabilities{};
}

void MyWorkspacePage::printCurrentPage()
{
    if (BasePage* page = currentChildPage())
    {
        page->printCurrentPage();
    }
}

void MyWorkspacePage::saveCurrentPageAs()
{
    if (BasePage* page = currentChildPage())
    {
        page->saveCurrentPageAs();
    }
}

void MyWorkspacePage::buildUi()
{
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);

    auto* content = new QWidget(this);
    auto* workspaceLayout = new QVBoxLayout(content);
    workspaceLayout->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin
        );
    workspaceLayout->setSpacing(UiConstants::Pages::Spacing);

    m_pageHeader = new PageHeader(
        tr("My Workspace"),
        tr("Manage your personal details, schedule, and calendar."),
        content
        );
    workspaceLayout->addWidget(m_pageHeader);
    workspaceLayout->addSpacing(UiConstants::Pages::HeaderContentSpacing);

    m_tabs = new NavigationTabWidget(
        NavigationTabKind::Section,
        QStringLiteral("myWorkspaceTabBar"),
        content
        );
    m_tabs->setObjectName(QStringLiteral("myWorkspaceTabs"));
    m_tabs->setPageSpacing(UiConstants::Pages::Spacing);

    m_personalDetailsPage = new PersonalDetailsPage(m_services, m_tabs);
    m_personalDetailsPage->setPageHeaderVisible(false);
    connectChildPageSignals(m_personalDetailsPage);

    m_schedulePage = new SchedulePage(m_services, m_tabs);
    m_schedulePage->setPageHeaderVisible(false);
    connectChildPageSignals(m_schedulePage);

    m_calendarPlaceholder = createPlaceholder(m_tabs);

    m_tabs->addTab(m_personalDetailsPage, tr("My Details"));
    m_tabs->addTab(m_schedulePage, tr("My Schedule"));
    m_tabs->addTab(m_calendarPlaceholder, tr("Calendar"));

    connect(
        m_tabs,
        &NavigationTabWidget::currentChanged,
        this,
        [this](int index)
        {
            if (index == tabIndex(WorkspaceTab::Calendar))
            {
                ensureCalendarInitialized();
            }
        }
        );
    openTab(WorkspaceTab::Schedule);

    workspaceLayout->addWidget(m_tabs, 1);
    contentLayout()->addWidget(content);
}

void MyWorkspacePage::ensureCalendarInitialized()
{
    if (m_calendarPage || !m_tabs)
    {
        return;
    }

    const int calendarIndex = tabIndex(WorkspaceTab::Calendar);
    m_tabs->removeTab(calendarIndex);

    m_calendarPage = new CalendarPage(m_services, m_tabs);
    m_calendarPage->setPageHeaderVisible(false);
    m_calendarPage->setDatabaseOpen(isDatabaseOpen());
    connectChildPageSignals(m_calendarPage);

    m_tabs->addTab(m_calendarPage, tr("Calendar"));
    m_tabs->setCurrentIndex(calendarIndex);

    if (m_calendarPlaceholder)
    {
        m_calendarPlaceholder->deleteLater();
        m_calendarPlaceholder = nullptr;
    }
}

void MyWorkspacePage::connectChildPageSignals(BasePage* page)
{
    if (!page)
    {
        return;
    }

    connect(page, &BasePage::initialSetupRequested,
            this, &BasePage::initialSetupRequested);
    connect(page, &BasePage::openDatabaseRequested,
            this, &BasePage::openDatabaseRequested);
    connect(page, &BasePage::newDatabaseRequested,
            this, &BasePage::newDatabaseRequested);
    connect(page, &BasePage::outputCapabilitiesChanged,
            this, &BasePage::outputCapabilitiesChanged);
}

BasePage* MyWorkspacePage::currentChildPage() const
{
    if (!m_tabs)
    {
        return nullptr;
    }

    return qobject_cast<BasePage*>(m_tabs->currentWidget());
}
