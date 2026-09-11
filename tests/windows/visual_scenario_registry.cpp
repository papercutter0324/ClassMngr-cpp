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
            .id = QStringLiteral("dialog.initial-setup.first-run"),
            .ledgerId = QStringLiteral("dialog.initial-setup"),
            .artifactPrefix = QStringLiteral("initial-setup"),
            .state = QStringLiteral("first-run"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("initial-setup")
        },
        {
            .id = QStringLiteral("dialog.calendar-event.new"),
            .ledgerId = QStringLiteral("dialog.calendar-event"),
            .artifactPrefix = QStringLiteral("calendar-event-new"),
            .state = QStringLiteral("new"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("calendar-event-new")
        },
        {
            .id = QStringLiteral("dialog.calendar-event.edit"),
            .ledgerId = QStringLiteral("dialog.calendar-event"),
            .artifactPrefix = QStringLiteral("calendar-event-edit"),
            .state = QStringLiteral("edit"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("calendar-event-edit")
        },
        {
            .id = QStringLiteral("dialog.calendar-event.validation"),
            .ledgerId = QStringLiteral("dialog.calendar-event"),
            .artifactPrefix = QStringLiteral("calendar-event-validation"),
            .state = QStringLiteral("validation"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("calendar-event-validation")
        },
        {
            .id = QStringLiteral("dialog.class-import.valid"),
            .ledgerId = QStringLiteral("dialog.class-import"),
            .artifactPrefix = QStringLiteral("class-import"),
            .state = QStringLiteral("valid"),
            .fixtureId = QStringLiteral("synthetic-package"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("class-import")
        },
        {
            .id = QStringLiteral("dialog.class-export.empty"),
            .ledgerId = QStringLiteral("dialog.class-export"),
            .artifactPrefix = QStringLiteral("class-export-empty"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("class-export"),
        },
        {
            .id = QStringLiteral("dialog.class-export.selection"),
            .ledgerId = QStringLiteral("dialog.class-export"),
            .artifactPrefix = QStringLiteral("class-export-selection"),
            .state = QStringLiteral("selection"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("class-export"),
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("dialog.teacher-import.invalid"),
            .ledgerId = QStringLiteral("dialog.teacher-import"),
            .artifactPrefix = QStringLiteral("teacher-import-invalid"),
            .state = QStringLiteral("invalid"),
            .fixtureId = QStringLiteral("invalid-input"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("teacher-import-invalid")
        },
        {
            .id = QStringLiteral("dialog.upcoming-birthdays.populated"),
            .ledgerId = QStringLiteral("dialog.upcoming-birthdays"),
            .artifactPrefix = QStringLiteral("upcoming-birthdays"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("synthetic-schedule"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("upcoming-birthdays-populated")
        },
        {
            .id = QStringLiteral("dialog.upcoming-birthdays.empty"),
            .ledgerId = QStringLiteral("dialog.upcoming-birthdays"),
            .artifactPrefix = QStringLiteral("upcoming-birthdays-empty"),
            .state = QStringLiteral("empty"),
            .fixtureId = QStringLiteral("synthetic-empty-schedule"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("upcoming-birthdays-empty")
        },
        {
            .id = QStringLiteral("dialog.schedule-editor.edit"),
            .ledgerId = QStringLiteral("dialog.schedule-editor"),
            .artifactPrefix = QStringLiteral("schedule-editor"),
            .state = QStringLiteral("edit"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("schedule-editor"),
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("dialog.schedule-import.invalid"),
            .ledgerId = QStringLiteral("dialog.schedule-import"),
            .artifactPrefix = QStringLiteral("schedule-import-invalid"),
            .state = QStringLiteral("invalid"),
            .fixtureId = QStringLiteral("invalid-input"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("schedule-import-invalid")
        },
        {
            .id = QStringLiteral("dialog.schedule-import-review.proposed"),
            .ledgerId = QStringLiteral("dialog.schedule-import-review"),
            .artifactPrefix = QStringLiteral("schedule-import-review"),
            .state = QStringLiteral("proposed"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("schedule-import-review"),
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("dialog.testing-assignment.new"),
            .ledgerId = QStringLiteral("dialog.testing-assignment"),
            .artifactPrefix = QStringLiteral("testing-assignment"),
            .state = QStringLiteral("new"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("testing-assignment"),
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("dialog.schedule-print.options"),
            .ledgerId = QStringLiteral("dialog.schedule-print"),
            .artifactPrefix = QStringLiteral("schedule-print"),
            .state = QStringLiteral("options"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("schedule-print")
        },
        {
            .id = QStringLiteral("dialog.roster-print.options"),
            .ledgerId = QStringLiteral("dialog.roster-print"),
            .artifactPrefix = QStringLiteral("roster-print"),
            .state = QStringLiteral("options"),
            .fixtureId = QStringLiteral("typical"),
            .fixtureFile = QStringLiteral("typical.tps"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("roster-print"),
            .databaseOpen = true
        },
        {
            .id = QStringLiteral("dialog.speaking-notes.populated"),
            .ledgerId = QStringLiteral("dialog.speaking-notes"),
            .artifactPrefix = QStringLiteral("speaking-notes"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("synthetic-report"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("speaking-notes")
        },
        {
            .id = QStringLiteral("dialog.speaking-ai-batch.ready"),
            .ledgerId = QStringLiteral("dialog.speaking-ai-batch"),
            .artifactPrefix = QStringLiteral("speaking-ai-batch"),
            .state = QStringLiteral("ready"),
            .fixtureId = QStringLiteral("synthetic-report"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("speaking-ai-batch")
        },
        {
            .id = QStringLiteral("dialog.speaking-batch-export.ready"),
            .ledgerId = QStringLiteral("dialog.speaking-batch-export"),
            .artifactPrefix = QStringLiteral("speaking-batch-export"),
            .state = QStringLiteral("ready"),
            .fixtureId = QStringLiteral("synthetic-report"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("speaking-batch-export")
        },
        {
            .id = QStringLiteral("dialog.speaking-report.populated"),
            .ledgerId = QStringLiteral("dialog.speaking-report"),
            .artifactPrefix = QStringLiteral("speaking-report"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("synthetic-report"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("speaking-report")
        },
        {
            .id = QStringLiteral("dialog.sub-prep-print.options"),
            .ledgerId = QStringLiteral("dialog.sub-prep-print"),
            .artifactPrefix = QStringLiteral("sub-prep-print"),
            .state = QStringLiteral("options"),
            .fixtureId = QStringLiteral("synthetic-schedule"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("sub-prep-print")
        },
        {
            .id = QStringLiteral("dialog.pdf-print.document"),
            .ledgerId = QStringLiteral("dialog.pdf-print"),
            .artifactPrefix = QStringLiteral("pdf-print"),
            .state = QStringLiteral("document"),
            .fixtureId = QStringLiteral("document-fixture"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("pdf-print")
        },
        {
            .id = QStringLiteral("dialog.record-selection.populated"),
            .ledgerId = QStringLiteral("dialog.record-selection"),
            .artifactPrefix = QStringLiteral("record-selection"),
            .state = QStringLiteral("populated"),
            .fixtureId = QStringLiteral("synthetic-records"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("record-selection")
        },
        {
            .id = QStringLiteral("dialog.about.default"),
            .ledgerId = QStringLiteral("dialog.about"),
            .artifactPrefix = QStringLiteral("about"),
            .state = QStringLiteral("default"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("about")
        },
        {
            .id = QStringLiteral("dialog.license.default"),
            .ledgerId = QStringLiteral("dialog.license"),
            .artifactPrefix = QStringLiteral("license"),
            .state = QStringLiteral("default"),
            .fixtureId = QStringLiteral("synthetic-license"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("license")
        },
        {
            .id = QStringLiteral("dialog.update.error"),
            .ledgerId = QStringLiteral("dialog.update"),
            .artifactPrefix = QStringLiteral("update-error"),
            .state = QStringLiteral("error"),
            .fixtureId = QStringLiteral("offline-service"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("update-error")
        },
        {
            .id = QStringLiteral("dialog.preferences.all-tabs"),
            .ledgerId = QStringLiteral("dialog.preferences"),
            .artifactPrefix = QStringLiteral("preferences"),
            .state = QStringLiteral("all-tabs"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("preferences")
        },
        {
            .id = QStringLiteral("dialog.memory-usage.open"),
            .ledgerId = QStringLiteral("dialog.memory-usage"),
            .artifactPrefix = QStringLiteral("memory-usage"),
            .state = QStringLiteral("open"),
            .fixtureId = QStringLiteral("no-database"),
            .surface = WindowsQtCaptureSurface::Dialog,
            .dialogId = QStringLiteral("memory-usage")
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
