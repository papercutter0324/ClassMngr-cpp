#pragma once

#include "ui/shared/pages/basepage.h"

class ApplicationServices;
class BasePage;
class CalendarPage;
class NavigationTabWidget;
class PageHeader;
class PersonalDetailsPage;
class SchedulePage;
class QLabel;
class QWidget;

enum class WorkspaceTab
{
    Details,
    Schedule,
    Calendar
};

class MyWorkspacePage : public BasePage
{
    Q_OBJECT

public:
    explicit MyWorkspacePage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void openTab(WorkspaceTab tab);
    [[nodiscard]] WorkspaceTab currentTab() const;
    [[nodiscard]] PersonalDetailsPage* personalDetailsPage() const;
    [[nodiscard]] SchedulePage* schedulePage() const;
    [[nodiscard]] CalendarPage* calendarPage() const;

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void setSaveMode(SaveMode mode) override;
    void refresh() override;
    void clearDatabaseState() override;
    void retranslateUi() override;
    void setDatabaseOpen(bool databaseOpen) override;
    [[nodiscard]] PageOutputCapabilities outputCapabilities() const override;
    void printCurrentPage() override;
    void saveCurrentPageAs() override;

private:
    void buildUi();
    void ensureCalendarInitialized();
    void connectChildPageSignals(BasePage* page);
    [[nodiscard]] BasePage* currentChildPage() const;
    [[nodiscard]] static constexpr int tabIndex(WorkspaceTab tab)
    {
        return static_cast<int>(tab);
    }

    ApplicationServices* m_services = nullptr;
    PageHeader* m_pageHeader = nullptr;
    NavigationTabWidget* m_tabs = nullptr;
    PersonalDetailsPage* m_personalDetailsPage = nullptr;
    SchedulePage* m_schedulePage = nullptr;
    CalendarPage* m_calendarPage = nullptr;
    QLabel* m_calendarPlaceholder = nullptr;
};
