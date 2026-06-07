#pragma once

#include <QObject>

class ApplicationServices;
class Sidebar;
class PageManager;
class ActionRegistry;

class SidebarController : public QObject
{
    Q_OBJECT

public:
    SidebarController(
        ApplicationServices* services,
        Sidebar* sidebar,
        PageManager* pages,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    void refreshClassSidebar();
    void refreshTeacherSidebar();

private slots:
    void addClass();
    void deleteClass();

    void addTeacher();
    void deleteTeacher();

private:
    ApplicationServices* m_services;
    Sidebar* m_sidebar;
    PageManager* m_pages;
};