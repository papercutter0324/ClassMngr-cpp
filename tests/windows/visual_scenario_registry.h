#pragma once

#include "features/classes/ui/classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/pages/pagemanager.h"

#include <QList>
#include <QString>
#include <QSize>

enum class WindowsQtCaptureSurface
{
    RegisteredPage,
    WorkspaceTab,
    Menu,
    ClassSection
};

struct WindowsQtCaptureScenario
{
    QString id;
    QString ledgerId;
    QString artifactPrefix;
    QString state;
    QString fixtureId;
    QString fixtureFile;
    WindowsQtCaptureSurface surface =
        WindowsQtCaptureSurface::RegisteredPage;
    PageType pageType = PageType::MyWorkspace;
    ClassesSection classSection = ClassesSection::Details;
    WorkspaceTab workspaceTab = WorkspaceTab::Details;
    QString menuTitle;
    Theme theme = Theme::Light;
    Language language = Language::English;
    QSize windowSize = QSize(1270, 1040);
    bool databaseOpen = false;
};

[[nodiscard]] const QList<WindowsQtCaptureScenario>&
windowsQtCaptureScenarios();

[[nodiscard]] const WindowsQtCaptureScenario*
findWindowsQtCaptureScenario(
    const QString& id
    );
