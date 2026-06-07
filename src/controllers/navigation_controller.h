#pragma once

#include <QObject>

#include "core/application_services.h"
#include "ui/widgets/sidebar/sidebar.h"
#include "ui/widgets/sidebar/sidebar_types.h"

class PageManager;

class NavigationController : public QObject
{
    Q_OBJECT

public:
    NavigationController(
        ApplicationServices* services,
        Sidebar* sidebar,
        PageManager* pages,
        QObject* parent = nullptr
        );

public slots:
    void handleNavigation(
        const NavigationData& data
        );

private:
    void handleMyInfo(
        const NavigationData& data
        );

    void handleCampus(
        const NavigationData& data
        );

    void handleTeacher(
        const NavigationData& data
        );

    void handleClass(
        const NavigationData& data
        );

private:
    ApplicationServices* m_services = nullptr;

    Sidebar* m_sidebar = nullptr;

    PageManager* m_pages = nullptr;
};