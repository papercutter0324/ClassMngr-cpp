#pragma once

#include <QObject>

#include "core/application_services.h"
#include "next/platform/legacy_document_content_resource_port.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

class PageManager;

class NavigationController : public QObject
{
    Q_OBJECT

public:
    NavigationController(
        ApplicationServices* services,
        Sidebar* sidebar,
        PageManager* pages,
        ResourcePackManager&,
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

    void handleSubPrep(
        const NavigationData& data
        );

    void handleCampus(
        const NavigationData& data
        );

    void handleDocument(
        const NavigationData& data
        );

    void handleTeacher(
        const NavigationData& data
        );

    void handleRoster(
        const NavigationData& data
        );

    void handleNotes(
        const NavigationData& data
        );

    void handleClass(
        const NavigationData& data
        );

private:
    ApplicationServices* m_services = nullptr;

    ClassMngr::Next::Platform::LegacyDocumentContentResourcePort
        m_legacyDocumentContentResourcePort;

    Sidebar* m_sidebar = nullptr;

    PageManager* m_pages = nullptr;
};
