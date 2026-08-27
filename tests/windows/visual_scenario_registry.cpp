#include "visual_scenario_registry.h"

const QList<WindowsQtCaptureScenario>& windowsQtCaptureScenarios()
{
    static const QList<WindowsQtCaptureScenario> scenarios{
        {
            .id = QStringLiteral("page.personal-details"),
            .ledgerId = QStringLiteral("page.personal-details"),
            .artifactPrefix = QStringLiteral("personal-details"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::WorkspaceTab,
            .pageType = PageType::MyWorkspace,
            .workspaceTab = WorkspaceTab::Details
        },
        {
            .id = QStringLiteral("page.calendar"),
            .ledgerId = QStringLiteral("page.calendar"),
            .artifactPrefix = QStringLiteral("calendar"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::WorkspaceTab,
            .pageType = PageType::MyWorkspace,
            .workspaceTab = WorkspaceTab::Calendar
        },
        {
            .id = QStringLiteral("page.my-schedule"),
            .ledgerId = QStringLiteral("page.my-schedule"),
            .artifactPrefix = QStringLiteral("my-schedule"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::WorkspaceTab,
            .pageType = PageType::MyWorkspace,
            .workspaceTab = WorkspaceTab::Schedule
        },
        {
            .id = QStringLiteral("page.my-classes"),
            .ledgerId = QStringLiteral("page.my-classes"),
            .artifactPrefix = QStringLiteral("my-classes"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::MyClasses
        },
        {
            .id = QStringLiteral("page.schedule"),
            .ledgerId = QStringLiteral("page.schedule"),
            .artifactPrefix = QStringLiteral("schedule"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::Schedule
        },
        {
            .id = QStringLiteral("page.classes"),
            .ledgerId = QStringLiteral("page.classes"),
            .artifactPrefix = QStringLiteral("classes"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::Classes
        },
        {
            .id = QStringLiteral("page.testing-classes"),
            .ledgerId = QStringLiteral("page.testing-classes"),
            .artifactPrefix = QStringLiteral("testing-classes"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::TestingClasses
        },
        {
            .id = QStringLiteral("page.teacher-info"),
            .ledgerId = QStringLiteral("page.teacher-info"),
            .artifactPrefix = QStringLiteral("teacher-info"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::TeacherInfo
        },
        {
            .id = QStringLiteral("page.native-english-teachers"),
            .ledgerId = QStringLiteral("page.native-english-teachers"),
            .artifactPrefix = QStringLiteral("native-english-teachers"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::NativeEnglishTeachers
        },
        {
            .id = QStringLiteral("page.gs-team"),
            .ledgerId = QStringLiteral("page.gs-team"),
            .artifactPrefix = QStringLiteral("gs-team"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::GsTeam
        },
        {
            .id = QStringLiteral("page.campus-dashboard"),
            .ledgerId = QStringLiteral("page.campus-dashboard"),
            .artifactPrefix = QStringLiteral("campus-dashboard"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::CampusDashboard
        },
        {
            .id = QStringLiteral("page.sub-prep"),
            .ledgerId = QStringLiteral("page.sub-prep"),
            .artifactPrefix = QStringLiteral("sub-prep"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::SubPrep
        },
        {
            .id = QStringLiteral("page.pdf-viewer"),
            .ledgerId = QStringLiteral("page.pdf-viewer"),
            .artifactPrefix = QStringLiteral("pdf-viewer"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .pageType = PageType::PdfViewer
        },
        {
            .id = QStringLiteral("page.classes.typical"),
            .ledgerId = QStringLiteral("page.classes"),
            .artifactPrefix = QStringLiteral("classes"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .pageType = PageType::Classes,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("platform.theme-dpi"),
            .ledgerId = QStringLiteral("platform.theme-dpi"),
            .artifactPrefix = QStringLiteral("theme-dpi"),
            .state = QStringLiteral("dark-korean"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::WorkspaceTab,
            .pageType = PageType::MyWorkspace,
            .workspaceTab = WorkspaceTab::Details,
            .theme = Theme::Dark,
            .language = Language::Korean
        },
        {
            .id = QStringLiteral("command.file-recent"),
            .ledgerId = QStringLiteral("command.file-recent"),
            .artifactPrefix = QStringLiteral("file-recent"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Menu,
            .menuTitle = QStringLiteral("File")
        }
    };

    return scenarios;
}

const WindowsQtCaptureScenario* findWindowsQtCaptureScenario(
    const QString& id
    )
{
    const auto& scenarios = windowsQtCaptureScenarios();
    for (const WindowsQtCaptureScenario& scenario : scenarios)
    {
        if (scenario.id == id)
        {
            return &scenario;
        }
    }

    return nullptr;
}
