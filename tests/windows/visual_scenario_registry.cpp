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
            .id = QStringLiteral("editor.class-details.populated"),
            .ledgerId = QStringLiteral("editor.class-details"),
            .artifactPrefix = QStringLiteral("class-details"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Details,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.class-details.dirty"),
            .ledgerId = QStringLiteral("editor.class-details"),
            .artifactPrefix = QStringLiteral("class-details-dirty"),
            .state = QStringLiteral("dirty"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Details,
            .mutation = WindowsQtCaptureMutation::ClassDetailsDirty,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.class-details.validation"),
            .ledgerId = QStringLiteral("editor.class-details"),
            .artifactPrefix = QStringLiteral("class-details-validation"),
            .state = QStringLiteral("validation"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Details,
            .mutation = WindowsQtCaptureMutation::ClassDetailsValidation,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.roster.populated"),
            .ledgerId = QStringLiteral("editor.roster"),
            .artifactPrefix = QStringLiteral("roster"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Roster,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.roster.large"),
            .ledgerId = QStringLiteral("editor.roster"),
            .artifactPrefix = QStringLiteral("roster-large"),
            .state = QStringLiteral("large"),
            .fixtureId = QStringLiteral("roster-large"),
            .fixtureFile = QStringLiteral("roster-large.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Roster,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.roster.dirty"),
            .ledgerId = QStringLiteral("editor.roster"),
            .artifactPrefix = QStringLiteral("roster-dirty"),
            .state = QStringLiteral("dirty"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Roster,
            .mutation = WindowsQtCaptureMutation::RosterDirty,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.class-analytics.populated"),
            .ledgerId = QStringLiteral("editor.class-analytics"),
            .artifactPrefix = QStringLiteral("class-analytics"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Analytics,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.class-analytics.empty"),
            .ledgerId = QStringLiteral("editor.class-analytics"),
            .artifactPrefix = QStringLiteral("class-analytics-empty"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("analytics-empty"),
            .fixtureFile = QStringLiteral("analytics-empty.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Analytics,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.speaking-evaluation.populated"),
            .ledgerId = QStringLiteral("editor.speaking-evaluation"),
            .artifactPrefix = QStringLiteral("speaking-evaluation"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Evaluations,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.speaking-evaluation.large"),
            .ledgerId = QStringLiteral("editor.speaking-evaluation"),
            .artifactPrefix = QStringLiteral("speaking-evaluation-large"),
            .state = QStringLiteral("large"),
            .fixtureId = QStringLiteral("large"),
            .fixtureFile = QStringLiteral("large.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Evaluations,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.speaking-evaluation.dirty"),
            .ledgerId = QStringLiteral("editor.speaking-evaluation"),
            .artifactPrefix = QStringLiteral("speaking-evaluation-dirty"),
            .state = QStringLiteral("dirty"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Evaluations,
            .mutation = WindowsQtCaptureMutation::SpeakingEvaluationDirty,
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("editor.speaking-evaluation.error"),
            .ledgerId = QStringLiteral("editor.speaking-evaluation"),
            .artifactPrefix = QStringLiteral("speaking-evaluation-error"),
            .state = QStringLiteral("error"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::ClassSection,
            .pageType = PageType::Classes,
            .classSection = ClassesSection::Evaluations,
            .mutation = WindowsQtCaptureMutation::SpeakingEvaluationError,
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
