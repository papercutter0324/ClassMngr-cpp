#include "visual_scenario_registry.h"

#include "app/menu_builder.h"
#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/build_info.h"
#include "core/fontmanager.h"
#include "core/language_service.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "core/updater/update_service.h"
#include "domain/models/calendar_event.h"
#include "domain/models/class_transfer.h"
#include "domain/models/speaking_evaluation.h"
#include "features/calendar/ui/calendar_event_dialog.h"
#include "features/classes/ui/class_export_dialog.h"
#include "features/classes/ui/class_import_dialog.h"
#include "features/roster/ui/roster_print_dialog.h"
#include "features/roster/ui/roster_model.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_batch_export_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_notes_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "features/sub_prep/ui/sub_prep_print_dialog.h"
#include "features/teacher/ui/teacher_import_dialog.h"
#include "features/teacher/ui/upcoming_birthdays_dialog.h"
#include "ui/shared/dialogs/about_dialog.h"
#include "ui/shared/dialogs/license_dialog.h"
#include "ui/shared/dialogs/memory_usage_dialog.h"
#include "ui/shared/dialogs/record_selection_dialog.h"
#include "ui/shared/dialogs/update_dialog.h"
#include "ui/shared/printing/pdf_print_dialog.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/widgets/sections/class_details_section.h"

#include <QAction>
#include <QAbstractItemModel>
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QPoint>
#include <QPdfDocument>
#include <QRect>
#include <QSaveFile>
#include <QScreen>
#include <QSet>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTableView>
#include <QThreadPool>
#include <QStringList>
#include <QWidget>
#include <QWindow>

#include <cmath>
#include <memory>

#ifndef CLASSMNGR_PHASE0_FIXTURE_DIR
#define CLASSMNGR_PHASE0_FIXTURE_DIR ""
#endif

namespace
{
constexpr int SettleMilliseconds = 250;

QString themeName(Theme theme)
{
    return theme == Theme::Dark
        ? QStringLiteral("dark")
        : QStringLiteral("light");
}

QString appLanguageName(Language language)
{
    return language == Language::Korean
        ? QStringLiteral("ko")
        : QStringLiteral("en");
}

QString inputLanguageName(Language language)
{
    return language == Language::Korean
        ? QStringLiteral("ko-KR")
        : QStringLiteral("en-US");
}

QString architectureName()
{
    const QString architecture =
        QSysInfo::buildCpuArchitecture().toLower();

    return architecture.contains(QStringLiteral("arm"))
        ? QStringLiteral("ARM64")
        : QStringLiteral("x64");
}

QSet<const QWidget*> topLevelWidgets()
{
    QSet<const QWidget*> widgets;
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (widget)
        {
            widgets.insert(widget);
        }
    }
    return widgets;
}

QString topLevelWidgetNames(
    const QSet<const QWidget*>& baseline
    )
{
    QStringList names;
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (widget && !baseline.contains(widget))
        {
            names.append(
                QStringLiteral("%1 (%2)")
                    .arg(widget->objectName(), widget->metaObject()->className())
                );
        }
    }
    return names.join(QStringLiteral(", "));
}

void drainDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

QMenu* findMenu(
    QMenuBar* menuBar,
    const QString& expectedTitle
    )
{
    if (!menuBar)
    {
        return nullptr;
    }

    for (QAction* action : menuBar->actions())
    {
        QMenu* menu = action ? action->menu() : nullptr;
        if (!menu)
        {
            continue;
        }

        QString title = menu->title();
        title.remove(QLatin1Char('&'));
        if (title.trimmed().compare(expectedTitle, Qt::CaseInsensitive) == 0)
        {
            return menu;
        }
    }

    return nullptr;
}

int displayScalePercent(const QScreen* screen)
{
    if (!screen)
    {
        return 0;
    }

    // On Windows, logicalDotsPerInchX() remains the 96-DPI design baseline
    // even when the monitor is scaled. The device-pixel ratio is the Qt value
    // that reflects the effective per-monitor scale used by the native window.
    return qRound(screen->devicePixelRatio() * 100.0);
}

QString sha256ForFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        hash.addData(file.read(1024 * 1024));
    }

    return QString::fromLatin1(hash.result().toHex());
}

QStringList resourcePackIds()
{
    return {
        QStringLiteral("campuses"),
        QStringLiteral("documents"),
        QStringLiteral("files"),
        QStringLiteral("images"),
        QStringLiteral("splash"),
        QStringLiteral("templates")
    };
}

QString workspaceTabName(WorkspaceTab tab)
{
    switch (tab)
    {
    case WorkspaceTab::Details:
        return QStringLiteral("Personal Details");

    case WorkspaceTab::Schedule:
        return QStringLiteral("My Schedule");

    case WorkspaceTab::Calendar:
        return QStringLiteral("Calendar");
    }

    return QStringLiteral("Workspace");
}

int firstClassId(
    ApplicationServices* services,
    QString* errorMessage
    )
{
    if (!services || !services->classService())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral(
                "The scenario requires an available class service."
                );
        }
        return -1;
    }

    const Result<QList<Classroom>> classes = services->classService()->classes();
    if (!classes)
    {
        if (errorMessage)
        {
            *errorMessage = classes.error();
        }
        return -1;
    }

    if (classes->isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral(
                "The deterministic fixture contains no classes."
                );
        }
        return -1;
    }

    return classes->first().id;
}

CalendarEvent captureCalendarEvent(bool existing)
{
    CalendarEvent event;
    event.id = existing ? 17 : -1;
    event.title = existing
        ? QStringLiteral("Faculty Workshop")
        : QStringLiteral("New Semester");
    event.eventType = QStringLiteral("Workshop");
    event.timeStatus = QStringLiteral("Timed");
    event.startDate = QDate(2026, 8, 31);
    event.startTime = QTime(16, 0);
    event.endDate = QDate(2026, 8, 31);
    event.endTime = QTime(17, 30);
    return event;
}

ClassTransferPackage captureClassTransferPackage()
{
    ClassTransferPackage package;
    package.exportedAtUtc = QDateTime(
        QDate(2026, 8, 28),
        QTime(9, 0),
        Qt::UTC
        );

    ClassTransferClass transferClass;
    transferClass.key = QStringLiteral("phase0-class");
    transferClass.name = QStringLiteral("Phase 0 English Class");
    transferClass.info.classGrade = QStringLiteral("E6");
    transferClass.info.classLevel = QStringLiteral("Helios");
    transferClass.info.roomNumber = QStringLiteral("Room 201");
    transferClass.info.readingBook = QStringLiteral("Reading Explorer 3");
    transferClass.info.essayBook = QStringLiteral("6A");
    package.classes.append(transferClass);

    return package;
}

UpcomingBirthdaySchedule captureBirthdaySchedule(bool populated)
{
    UpcomingBirthdaySchedule schedule;
    if (!populated)
    {
        return schedule;
    }

    schedule.today.append({
        .date = QDate(2026, 8, 29),
        .displayName = QStringLiteral("Kim Min-jun"),
        .position = QStringLiteral("Korean Teacher"),
        .group = UpcomingBirthdayGroup::KoreanTeacher
    });
    schedule.thisWeek.append({
        .date = QDate(2026, 8, 31),
        .displayName = QStringLiteral("Alex Smith"),
        .position = QStringLiteral("Native English Teacher"),
        .group = UpcomingBirthdayGroup::NativeEnglishTeacher
    });
    schedule.nextWeek.append({
        .date = QDate(2026, 9, 7),
        .displayName = QStringLiteral("Park Ji-hye"),
        .position = QStringLiteral("GS Team"),
        .group = UpcomingBirthdayGroup::GsTeam
    });
    return schedule;
}

ScheduleImportReviewRequest captureScheduleImportReviewRequest()
{
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.profileName = QStringLiteral("Phase 0 Teacher");
    request.user.name = QStringLiteral("Phase 0 Teacher");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("phase0-teacher");
    candidate.teacherKr = QStringLiteral("Phase 0 Teacher");
    candidate.rooms = {QStringLiteral("201")};
    candidate.classGrade = QStringLiteral("E6");
    candidate.classLevel = QStringLiteral("Orion");
    candidate.times = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
        }
    };
    request.user.classes = {candidate};
    return request;
}

QList<SpeakingEvalBatchReportService::StudentReport>
captureSpeakingReports()
{
    SpeakingEvalBatchReportService::StudentReport first;
    first.displayName = QStringLiteral("Alice Example (예시 학생)");
    first.sourceRow = 0;
    first.report.englishName = QStringLiteral("Alice Example");
    first.report.koreanName = QStringLiteral("예시 학생");
    first.report.classLabel = QStringLiteral("E6 Helios");
    first.report.nativeTeacher = QStringLiteral("Alex Smith");
    first.report.koreanTeacher = QStringLiteral("Kim Min-jun");
    first.report.date = QStringLiteral("August 29, 2026");
    first.report.comments = QStringLiteral(
        "Alice participates actively and communicates her ideas clearly."
        );
    first.report.notes = QStringLiteral("Phase 0 deterministic report fixture");
    first.report.grade = 6;
    first.report.scores = {
        QStringLiteral("A"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("A"),
        QStringLiteral("A"),
        QStringLiteral("A")
    };

    SpeakingEvalBatchReportService::StudentReport second = first;
    second.displayName = QStringLiteral("Bob Example (예시 학생)");
    second.sourceRow = 1;
    second.report.englishName = QStringLiteral("Bob Example");
    second.report.koreanName = QStringLiteral("예시 학생");
    second.report.comments = QStringLiteral(
        "Bob is steadily improving his fluency and classroom confidence."
        );
    second.report.scores = {
        QStringLiteral("B+"),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("A")
    };

    return {first, second};
}

std::unique_ptr<QDialog> createDialogScenario(
    const WindowsQtCaptureScenario& scenario,
    MainWindow* window,
    const QString& temporaryRoot,
    QString* errorMessage
    )
{
    ApplicationServices* services = window ? window->services() : nullptr;

    if (scenario.dialogId == QStringLiteral("initial-setup"))
    {
        return std::make_unique<InitialSetupWizard>(services, window);
    }

    if (scenario.dialogId == QStringLiteral("calendar-event-new"))
    {
        return std::make_unique<CalendarEventDialog>(
            captureCalendarEvent(false),
            false,
            true,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("calendar-event-edit"))
    {
        return std::make_unique<CalendarEventDialog>(
            captureCalendarEvent(true),
            true,
            true,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("calendar-event-validation"))
    {
        return std::make_unique<CalendarEventDialog>(
            captureCalendarEvent(true),
            true,
            true,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("class-import"))
    {
        const ClassTransferPackage package = captureClassTransferPackage();
        ClassImportPreview preview;
        ClassImportClassPreview classPreview;
        classPreview.packageClassIndex = 0;
        preview.classes.append(classPreview);
        return std::make_unique<ClassImportDialog>(
            nullptr,
            nullptr,
            package,
            preview,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("class-export"))
    {
        return std::make_unique<ClassExportDialog>(
            scenario.databaseOpen && services
                ? services->classService()
                : nullptr,
            scenario.databaseOpen && services
                ? services->teacherService()
                : nullptr,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("teacher-import-invalid"))
    {
        auto dialog = std::make_unique<TeacherImportDialog>(window);
        dialog->setFilePath(
            QDir(temporaryRoot).filePath(QStringLiteral("missing-teachers.xlsx"))
            );
        return dialog;
    }

    if (
        scenario.dialogId == QStringLiteral("upcoming-birthdays-populated")
        || scenario.dialogId == QStringLiteral("upcoming-birthdays-empty")
        )
    {
        return std::make_unique<UpcomingBirthdaysDialog>(
            captureBirthdaySchedule(
                scenario.dialogId
                    == QStringLiteral("upcoming-birthdays-populated")
                ),
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("schedule-editor"))
    {
        const int classId = firstClassId(services, errorMessage);
        if (classId < 0)
        {
            return nullptr;
        }
        return std::make_unique<ScheduleEditorDialog>(
            services,
            classId,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("schedule-import-invalid"))
    {
        auto dialog = std::make_unique<ScheduleImportDialog>(services, window);
        dialog->setFilePath(
            QDir(temporaryRoot).filePath(QStringLiteral("missing-schedule.xlsx"))
            );
        return dialog;
    }

    if (scenario.dialogId == QStringLiteral("schedule-import-review"))
    {
        auto dialog = std::make_unique<ScheduleImportReviewDialog>(
            services,
            captureScheduleImportReviewRequest(),
            window
            );
        if (!dialog->prepare())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "The deterministic schedule-import review request could not be prepared."
                    );
            }
            return nullptr;
        }
        return dialog;
    }

    if (scenario.dialogId == QStringLiteral("testing-assignment"))
    {
        if (!services || !services->scheduleService())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "The scenario requires an available schedule service."
                    );
            }
            return nullptr;
        }
        return std::make_unique<TestingAssignmentDialog>(
            services->scheduleService(),
            nullptr,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("schedule-print"))
    {
        return std::make_unique<SchedulePrintDialog>(
            SchedulePrintDialog::Action::SaveAs,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("roster-print"))
    {
        const int classId = firstClassId(services, errorMessage);
        if (classId < 0)
        {
            return nullptr;
        }
        return std::make_unique<RosterPrintDialog>(
            services,
            classId,
            RosterTemplatePrintService::Scope::CurrentClass,
            RosterPrintDialog::Action::SaveAs,
            window,
            true
            );
    }

    if (scenario.dialogId == QStringLiteral("speaking-notes"))
    {
        return std::make_unique<SpeakingEvalNotesDialog>(
            QStringLiteral("Private note for the deterministic fixture."),
            QStringLiteral("A report comment for the deterministic fixture."),
            SpeakingEvalNotesDialog::InitialSection::Notes,
            window,
            QStringLiteral("Alice Example"),
            QStringLiteral("예시 학생")
            );
    }

    if (scenario.dialogId == QStringLiteral("speaking-ai-batch"))
    {
        return std::make_unique<SpeakingEvalAiBatchDialog>(
            captureSpeakingReports(),
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("speaking-batch-export"))
    {
        return std::make_unique<SpeakingEvalBatchExportDialog>(
            captureSpeakingReports(),
            0,
            temporaryRoot,
            SpeakingEvalBatchExportDialog::Mode::SaveAs,
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("speaking-report"))
    {
        return std::make_unique<SpeakingEvalReportDialog>(
            captureSpeakingReports(),
            0,
            window,
            true
            );
    }

    if (scenario.dialogId == QStringLiteral("sub-prep-print"))
    {
        QList<CalendarEvent> events{
            captureCalendarEvent(true)
        };
        return std::make_unique<SubPrepPrintDialog>(
            events,
            QDate(2026, 8, 29),
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("pdf-print"))
    {
        const QString documentPath = QDir::current().filePath(
            QStringLiteral(
                "resources/assets/documents/Book Reports/Book Report Grading.pdf"
                )
            );
        auto* document = new QPdfDocument(window);
        if (document->load(documentPath) != QPdfDocument::Error::None)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Unable to load the deterministic PDF document: %1"
                    ).arg(documentPath);
            }
            delete document;
            return nullptr;
        }

        const PdfPrintDialogSupport::RenderFunction renderFunction =
            [](QPrinter&, const PdfPrintDialogSupport::RenderOptions&)
            {
                return PdfPrintService::Result{
                    PdfPrintService::Status::Canceled,
                    QString()
                };
            };
        return std::make_unique<PdfPrintDialog>(
            window,
            document,
            documentPath,
            renderFunction,
            0,
            QPageLayout::Portrait,
            false,
            QPageSize::A4,
            false
            );
    }

    if (scenario.dialogId == QStringLiteral("record-selection"))
    {
        return std::make_unique<RecordSelectionDialog>(
            QStringLiteral("Select a record"),
            QStringLiteral("Choose the record to inspect."),
            QList<QPair<QString, int>>{
                {QStringLiteral("Alice Example"), 101},
                {QStringLiteral("Bob Example"), 102}
            },
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("about"))
    {
        return std::make_unique<AboutDialog>(window);
    }

    if (scenario.dialogId == QStringLiteral("license"))
    {
        return std::make_unique<LicenseDialog>(
            QStringLiteral("Phase 0 Fixture License"),
            QStringLiteral(
                "This deterministic capture fixture is provided for Phase 0 "
                "visual evidence."
                ),
            window
            );
    }

    if (scenario.dialogId == QStringLiteral("update-error"))
    {
        UpdateConfiguration configuration;
        configuration.releasesApiUrl = QUrl();
        auto* updateService = new UpdateService(configuration, window);
        auto dialog = std::make_unique<UpdateDialog>(
            updateService,
            true,
            window
            );
        updateService->checkForUpdates(UpdateService::CheckPolicy::Force);
        return dialog;
    }

    if (scenario.dialogId == QStringLiteral("preferences"))
    {
        return MenuBuilder::createPreferencesDialog(window);
    }

    if (scenario.dialogId == QStringLiteral("memory-usage"))
    {
        return std::make_unique<MemoryUsageDialog>(
            window,
            nullptr,
            nullptr,
            services,
            nullptr
            );
    }

    if (errorMessage)
    {
        *errorMessage = QStringLiteral(
            "No automated dialog factory exists for '%1'."
            ).arg(scenario.dialogId);
    }
    return nullptr;
}
} // namespace

class WindowsQtVisualCaptureTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void scenarioRegistryIsComplete();
    void capture_data();
    void capture();
    void cleanupTestCase();

private:
    void writeCaptureMetadata(
        const WindowsQtCaptureScenario& scenario,
        const QString& imagePath,
        const QString& imageFileName,
        int actualScalePercent,
        const QSize& actualWindowSize,
        const QRect& windowBounds,
        const QStringList& actions
        );

    QTemporaryDir m_settingsRoot;
    std::unique_ptr<LanguageService> m_languageService;
    QString m_captureRoot;
    QByteArray m_previousSettingsRoot;
    QByteArray m_previousAppData;
    QByteArray m_previousLocalAppData;
    bool m_previousSettingsRootWasSet = false;
    bool m_previousAppDataWasSet = false;
    bool m_previousLocalAppDataWasSet = false;
};

void WindowsQtVisualCaptureTests::initTestCase()
{
    if (QGuiApplication::platformName().compare(
            QStringLiteral("windows"),
            Qt::CaseInsensitive
            ) != 0)
    {
        QSKIP("Phase 0 evidence requires the native Windows Qt platform plugin.");
    }

    if (!QGuiApplication::primaryScreen())
    {
        QSKIP("Phase 0 evidence requires an interactive display.");
    }

    if (!m_settingsRoot.isValid())
    {
        QFAIL("Unable to create an isolated settings directory.");
    }

    m_previousSettingsRootWasSet =
        qEnvironmentVariableIsSet("CLASSMNGR_SETTINGS_ROOT");
    m_previousSettingsRoot = qgetenv("CLASSMNGR_SETTINGS_ROOT");
    m_previousAppDataWasSet = qEnvironmentVariableIsSet("APPDATA");
    m_previousAppData = qgetenv("APPDATA");
    m_previousLocalAppDataWasSet = qEnvironmentVariableIsSet("LOCALAPPDATA");
    m_previousLocalAppData = qgetenv("LOCALAPPDATA");
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
    qputenv("APPDATA", m_settingsRoot.path().toUtf8());
    qputenv("LOCALAPPDATA", m_settingsRoot.path().toUtf8());

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();

    // ResourcePackManager stores discovered-pack metadata below
    // AppDataLocation. Test mode keeps that mutable state out of the user's
    // profile and makes every evidence run disposable.
    QStandardPaths::setTestModeEnabled(true);

    const Status resourceStatus =
        ResourcePackManager::instance().initialize();
    if (!resourceStatus)
    {
        QFAIL(qPrintable(
            QStringLiteral("%1 Storage: %2")
                .arg(
                    resourceStatus.error(),
                    ResourcePackManager::instance().storageDirectory()
                    )
            ));
    }

    m_languageService = std::make_unique<LanguageService>();

    QString captureRoot =
        qEnvironmentVariable("CLASSMNGR_PHASE0_CAPTURE_ROOT");
    if (captureRoot.trimmed().isEmpty())
    {
        const QString runId =
            QDateTime::currentDateTimeUtc().toString(
                QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'")
                )
            + QStringLiteral("-")
            + QString::number(QCoreApplication::applicationPid());
        captureRoot = QDir::current().filePath(
            QStringLiteral("artifacts/phase0/windows-qt-visual/%1")
                .arg(runId)
            );
    }

    m_captureRoot = QFileInfo(captureRoot).absoluteFilePath();
    if (!QDir().mkpath(m_captureRoot))
    {
        QFAIL(qPrintable(
            QStringLiteral("Unable to create capture root: %1")
                .arg(m_captureRoot)
            ));
    }
}

void WindowsQtVisualCaptureTests::scenarioRegistryIsComplete()
{
    const auto& scenarios = windowsQtCaptureScenarios();
    QVERIFY2(!scenarios.isEmpty(), "The Phase 0 scenario registry is empty.");

    QSet<QString> ids;
    QSet<int> registeredPages;
    QSet<int> workspaceTabs;

    for (const WindowsQtCaptureScenario& scenario : scenarios)
    {
        QVERIFY2(!scenario.id.isEmpty(), "A capture scenario has no ID.");
        QVERIFY2(!ids.contains(scenario.id), qPrintable(
            QStringLiteral("Duplicate capture scenario: %1")
                .arg(scenario.id)
            ));
        ids.insert(scenario.id);

        QVERIFY2(!scenario.ledgerId.isEmpty(), qPrintable(
            QStringLiteral("Scenario has no ledger ID: %1")
                .arg(scenario.id)
            ));
        QVERIFY2(!scenario.artifactPrefix.isEmpty(), qPrintable(
            QStringLiteral("Scenario has no artifact prefix: %1")
                .arg(scenario.id)
            ));
        QVERIFY2(!scenario.state.isEmpty(), qPrintable(
            QStringLiteral("Scenario has no state: %1")
                .arg(scenario.id)
            ));
        QVERIFY2(!scenario.fixtureId.isEmpty(), qPrintable(
            QStringLiteral("Scenario has no fixture ID: %1")
                .arg(scenario.id)
            ));
        QVERIFY2(scenario.windowSize.width() >= 800
                     && scenario.windowSize.height() >= 600,
            qPrintable(
                QStringLiteral("Scenario window is below the supported size: %1")
                    .arg(scenario.id)
                ));

        switch (scenario.surface)
        {
        case WindowsQtCaptureSurface::RegisteredPage:
            registeredPages.insert(static_cast<int>(scenario.pageType));
            QVERIFY2(scenario.pageType != PageType::MyWorkspace,
                "My Workspace must be represented through a workspace tab.");
            break;

        case WindowsQtCaptureSurface::WorkspaceTab:
            QVERIFY2(scenario.pageType == PageType::MyWorkspace,
                "Workspace-tab scenarios must target My Workspace.");
            workspaceTabs.insert(static_cast<int>(scenario.workspaceTab));
            break;

        case WindowsQtCaptureSurface::Menu:
            QVERIFY2(!scenario.menuTitle.isEmpty(), qPrintable(
                QStringLiteral("Menu scenario has no menu title: %1")
                    .arg(scenario.id)
                ));
            break;

        case WindowsQtCaptureSurface::ClassSection:
            QVERIFY2(scenario.pageType == PageType::Classes,
                "Class-section scenarios must target Classes.");
            QVERIFY2(!scenario.fixtureFile.isEmpty(), qPrintable(
                QStringLiteral("Class-section scenario has no database fixture: %1")
                    .arg(scenario.id)
                ));
            switch (scenario.mutation)
            {
            case WindowsQtCaptureMutation::None:
                break;

            case WindowsQtCaptureMutation::ClassDetailsDirty:
            case WindowsQtCaptureMutation::ClassDetailsValidation:
                QVERIFY2(scenario.classSection == ClassesSection::Details,
                    "Class-details mutations must target the Details section.");
                break;

            case WindowsQtCaptureMutation::RosterDirty:
                QVERIFY2(scenario.classSection == ClassesSection::Roster,
                    "Roster mutations must target the Roster section.");
                break;

            case WindowsQtCaptureMutation::SpeakingEvaluationDirty:
            case WindowsQtCaptureMutation::SpeakingEvaluationError:
                QVERIFY2(scenario.classSection == ClassesSection::Evaluations,
                    "Speaking-evaluation mutations must target Evaluations.");
                break;
            }
            break;

        case WindowsQtCaptureSurface::Dialog:
            QVERIFY2(!scenario.dialogId.isEmpty(), qPrintable(
                QStringLiteral("Dialog scenario has no dialog ID: %1")
                    .arg(scenario.id)
                ));
            break;
        }
    }

    const QList<PageType> expectedRegisteredPages{
        PageType::MyClasses,
        PageType::Schedule,
        PageType::Classes,
        PageType::TestingClasses,
        PageType::TeacherInfo,
        PageType::NativeEnglishTeachers,
        PageType::GsTeam,
        PageType::CampusDashboard,
        PageType::SubPrep,
        PageType::PdfViewer
    };
    for (PageType page : expectedRegisteredPages)
    {
        QVERIFY2(registeredPages.contains(static_cast<int>(page)),
            qPrintable(
                QStringLiteral("No registered-page scenario for %1")
                    .arg(PageManager::pageTypeIdentifier(page))
                ));
    }

    const QList<WorkspaceTab> expectedWorkspaceTabs{
        WorkspaceTab::Details,
        WorkspaceTab::Schedule,
        WorkspaceTab::Calendar
    };
    for (WorkspaceTab tab : expectedWorkspaceTabs)
    {
        QVERIFY2(workspaceTabs.contains(static_cast<int>(tab)),
            qPrintable(
                QStringLiteral("No workspace-tab scenario for %1")
                    .arg(workspaceTabName(tab))
                ));
    }
}

void WindowsQtVisualCaptureTests::capture_data()
{
    QTest::addColumn<QString>("scenarioId");

    const QString filter =
        qEnvironmentVariable("CLASSMNGR_PHASE0_SCENARIO_FILTER").trimmed();
    const QStringList filters = filter.split(
        QLatin1Char(','),
        Qt::SkipEmptyParts
        );
    int selectedCount = 0;

    for (const WindowsQtCaptureScenario& scenario :
         windowsQtCaptureScenarios())
    {
        bool selected = filters.isEmpty();
        for (const QString& candidate : filters)
        {
            const QString trimmed = candidate.trimmed();
            if (
                trimmed == scenario.id
                || (
                    trimmed.endsWith(QLatin1Char('*'))
                    && scenario.id.startsWith(
                        trimmed.left(trimmed.size() - 1)
                        )
                    )
                )
            {
                selected = true;
                break;
            }
        }

        if (!selected)
        {
            continue;
        }

        const QByteArray rowName = scenario.id.toUtf8();
        QTest::newRow(rowName.constData()) << scenario.id;
        ++selectedCount;
    }

    if (!filters.isEmpty())
    {
        QVERIFY2(selectedCount > 0,
            qPrintable(
                QStringLiteral(
                    "CLASSMNGR_PHASE0_SCENARIO_FILTER selected no scenarios: %1"
                    ).arg(filter)
                ));
    }
}

void WindowsQtVisualCaptureTests::capture()
{
    QFETCH(QString, scenarioId);

    const WindowsQtCaptureScenario* scenario =
        findWindowsQtCaptureScenario(scenarioId);
    QVERIFY2(scenario, qPrintable(
        QStringLiteral("Scenario disappeared from the registry: %1")
            .arg(scenarioId)
        ));

    SettingsManager::instance().clear();
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::Theme),
        static_cast<int>(scenario->theme)
        );
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::Language),
        static_cast<int>(scenario->language)
        );
    SettingsManager::instance().set(
        QString::fromUtf8(OptionKeys::FontSize),
        static_cast<int>(FontSize::Normal)
        );
    SettingsManager::instance().sync();

    QVERIFY2(m_languageService->setLanguage(scenario->language), qPrintable(
        QStringLiteral("Unable to load translation for %1.")
            .arg(appLanguageName(scenario->language))
        ));

    FontManager::setSizeOffset(0);
    FontManager::applyGlobalFont(
        *qApp,
        m_languageService->loadedLocaleName()
        );

    QTemporaryDir databaseRoot;
    QVERIFY2(databaseRoot.isValid(), "Unable to create an isolated database directory.");

    QString databasePath;
    if (!scenario->fixtureFile.isEmpty())
    {
        const QString sourcePath = QDir(
            QStringLiteral(CLASSMNGR_PHASE0_FIXTURE_DIR)
            ).filePath(scenario->fixtureFile);
        QVERIFY2(QFileInfo(sourcePath).isFile(), qPrintable(
            QStringLiteral("Fixture is missing: %1")
                .arg(sourcePath)
            ));

        databasePath = QDir(databaseRoot.path()).filePath(
            QStringLiteral("capture.tps")
            );
        QVERIFY2(QFile::copy(sourcePath, databasePath), qPrintable(
            QStringLiteral("Unable to copy fixture to: %1")
                .arg(databasePath)
            ));
    }

    const QSet<const QWidget*> baselineTopLevels = topLevelWidgets();
    const QStringList baselineConnectionNames =
        QSqlDatabase::connectionNames();
    const QSet<QString> baselineConnections(
        baselineConnectionNames.cbegin(),
        baselineConnectionNames.cend()
        );
    const int baselineThreadCount =
        QThreadPool::globalInstance()->activeThreadCount();

    for (const QString& packId : resourcePackIds())
    {
        QVERIFY2(!ResourcePackManager::instance().isMounted(packId),
            qPrintable(
                QStringLiteral("Resource pack was already mounted before %1: %2")
                    .arg(scenario->id, packId)
                ));
    }

    auto startupThemeService = std::make_unique<ThemeService>();
    startupThemeService->setTheme(scenario->theme);

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = databasePath;
    startupOptions.startupThemeService = std::move(startupThemeService);

    auto window = std::make_unique<MainWindow>(
        [](const QString&) {},
        false,
        m_languageService.get(),
        nullptr,
        std::move(startupOptions)
        );

    window->resize(scenario->windowSize);
    window->show();
    window->raise();
    window->activateWindow();

    bool exposed = QTest::qWaitForWindowExposed(window.get(), 15000);
    if (!exposed)
    {
        // A first native top-level window can miss the initial expose
        // notification while the desktop creates its frame. Re-showing the
        // same production window gives the platform plugin one clean expose
        // cycle without weakening the evidence requirement.
        window->hide();
        QApplication::processEvents(QEventLoop::AllEvents, 100);
        window->show();
        window->raise();
        window->activateWindow();
        exposed = QTest::qWaitForWindowExposed(window.get(), 15000);
    }
    QVERIFY2(
        exposed,
        "MainWindow was not exposed by the native Windows platform plugin."
        );

    QTest::qWait(SettleMilliseconds);
    QApplication::processEvents(QEventLoop::AllEvents, 100);

    PageManager* pages = window->findChild<PageManager*>(
        QStringLiteral("pagesWidget")
        );
    QVERIFY2(pages, "MainWindow did not expose its production PageManager.");

    QStringList actions{
        QStringLiteral("Construct MainWindow production entry point"),
        QStringLiteral("Show window and wait for native exposure")
    };

    QMenu* openMenu = nullptr;
    ClassesPage* classesForScenario = nullptr;
    std::unique_ptr<QDialog> capturedDialog;
    QWidget* captureTarget = window.get();
    switch (scenario->surface)
    {
    case WindowsQtCaptureSurface::RegisteredPage:
        pages->showPage(scenario->pageType);
        actions.append(
            QStringLiteral("Navigate to %1")
                .arg(PageManager::pageTypeIdentifier(scenario->pageType))
            );
        QTRY_COMPARE_WITH_TIMEOUT(
            pages->currentPageIdentifier(),
            PageManager::pageTypeIdentifier(scenario->pageType),
            5000
            );
        break;

    case WindowsQtCaptureSurface::WorkspaceTab:
    {
        pages->showPage(PageType::MyWorkspace);
        auto* workspace = pages->myWorkspacePage();
        QVERIFY(workspace);

        if (scenario->workspaceTab == WorkspaceTab::Calendar)
        {
            QVERIFY(workspace->ensureCalendarPage());
        }

        workspace->openTab(scenario->workspaceTab);
        QVERIFY(workspace->currentTab() == scenario->workspaceTab);
        QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
        actions.append(
            QStringLiteral("Open %1 workspace tab")
                .arg(workspaceTabName(scenario->workspaceTab))
            );
        break;
    }

    case WindowsQtCaptureSurface::ClassSection:
    {
        pages->showPage(PageType::Classes);
        QTRY_COMPARE_WITH_TIMEOUT(
            pages->currentPageIdentifier(),
            PageManager::pageTypeIdentifier(PageType::Classes),
            5000
            );

        ClassesPage* classes = pages->classesPage();
        QVERIFY2(classes, "Classes page was not created by its production entry point.");
        classesForScenario = classes;
        if (scenario->mutation != WindowsQtCaptureMutation::None)
        {
            classes->setSaveMode(SaveMode::Manual);
        }
        QVERIFY2(
            classes->openClass(-1, scenario->classSection),
            qPrintable(
                QStringLiteral("Unable to open a populated class for %1")
                    .arg(scenario->id)
                )
            );
        QVERIFY2(classes->currentClassId() > 0,
            "Class-section scenario did not select a class.");
        QCOMPARE(classes->currentSection(), scenario->classSection);
        QVERIFY2(
            classes->isEditorInstantiated(scenario->classSection),
            qPrintable(
                QStringLiteral("Requested class editor was not instantiated: %1")
                    .arg(scenario->id)
                )
            );
        actions.append(
            QStringLiteral("Open Classes page and select class section %1")
                .arg(static_cast<int>(scenario->classSection))
            );
        break;
    }

    case WindowsQtCaptureSurface::Menu:
        openMenu = findMenu(window->menuBar(), scenario->menuTitle);
        QVERIFY2(openMenu, qPrintable(
            QStringLiteral("Unable to find menu: %1")
                .arg(scenario->menuTitle)
            ));
        openMenu->popup(
            window->menuBar()->mapToGlobal(
                QPoint(window->menuBar()->width() / 2, window->menuBar()->height())
                )
            );
        QTRY_VERIFY_WITH_TIMEOUT(openMenu->isVisible(), 3000);
        QVERIFY(!openMenu->actions().isEmpty());
        actions.append(
            QStringLiteral("Open %1 menu")
                .arg(scenario->menuTitle)
            );
        break;

    case WindowsQtCaptureSurface::Dialog:
        break;
    }

    if (scenario->surface == WindowsQtCaptureSurface::Dialog)
    {
        QString dialogError;
        capturedDialog = createDialogScenario(
            *scenario,
            window.get(),
            databaseRoot.path(),
            &dialogError
            );
        QVERIFY2(
            capturedDialog,
            qPrintable(
                QStringLiteral("Unable to construct dialog for %1: %2")
                    .arg(scenario->id, dialogError)
                )
            );

        capturedDialog->show();
        capturedDialog->raise();
        capturedDialog->activateWindow();
        QVERIFY2(
            QTest::qWaitForWindowExposed(capturedDialog.get(), 15000),
            qPrintable(
                QStringLiteral("Dialog was not exposed: %1")
                    .arg(scenario->id)
                )
            );
        captureTarget = capturedDialog.get();
        actions.append(
            QStringLiteral("Show %1 dialog and wait for native exposure")
                .arg(scenario->dialogId)
            );

        if (scenario->dialogId == QStringLiteral("calendar-event-validation"))
        {
            auto* titleEdit = capturedDialog->findChild<QLineEdit*>(
                QStringLiteral("calendarEventTitleEdit")
                );
            QVERIFY2(titleEdit, "Calendar event title editor was not found.");
            titleEdit->clear();
            capturedDialog->accept();
            QTRY_VERIFY_WITH_TIMEOUT(
                capturedDialog->isVisible(),
                3000
                );
            actions.append(
                QStringLiteral("Submit an empty calendar event and retain validation state")
                );
        }

        if (scenario->dialogId == QStringLiteral("class-export"))
        {
            auto* classList = capturedDialog->findChild<QListWidget*>(
                QStringLiteral("classExportList")
                );
            QVERIFY2(classList, "Class export list was not found.");
            if (classList->count() > 0)
            {
                classList->item(0)->setCheckState(Qt::Checked);
                actions.append(
                    QStringLiteral("Select the first class for export")
                    );
            }
        }

        if (
            scenario->dialogId == QStringLiteral("teacher-import-invalid")
            || scenario->dialogId == QStringLiteral("schedule-import-invalid")
            )
        {
            actions.append(
                QStringLiteral("Wait for deterministic invalid-input validation")
                );
        }
    }

    if (scenario->surface == WindowsQtCaptureSurface::ClassSection)
    {
        QVERIFY(classesForScenario);

        switch (scenario->mutation)
        {
        case WindowsQtCaptureMutation::None:
            if (scenario->classSection == ClassesSection::Details)
            {
                auto* details = window->findChild<ClassDetailsSection*>();
                QVERIFY2(details, "Class-details section was not instantiated.");

                if (scenario->state == QStringLiteral("populated"))
                {
                    QCOMPARE(details->grade(), QStringLiteral("E6"));
                    QCOMPARE(details->level(), QStringLiteral("Helios"));
                    QCOMPARE(details->readingBook(), QStringLiteral("Reading Explorer 3"));
                    QCOMPARE(details->essayBook(), QStringLiteral("6A"));
                    actions.append(
                        QStringLiteral("Assert populated class-details fixture values")
                        );
                }
            }
            else if (scenario->classSection == ClassesSection::Roster)
            {
                auto* table = window->findChild<QTableView*>(
                    QStringLiteral("rosterTable")
                    );
                QVERIFY2(table, "Roster table was not instantiated.");
                auto* model = qobject_cast<RosterModel*>(table->model());
                QVERIFY2(model, "Roster table did not expose its production model.");

                const int englishColumn = model->englishNameColumn();
                QVERIFY(englishColumn >= 0);
                if (scenario->state == QStringLiteral("large"))
                {
                    QCOMPARE(model->rowCount(), 25);
                    QCOMPARE(
                        model->data(
                            model->index(model->rowCount() - 1, englishColumn),
                            Qt::DisplayRole
                            ).toString(),
                        QStringLiteral("Student Y")
                        );
                    actions.append(
                        QStringLiteral(
                            "Assert the fixed 25-row editor surface backed by the 1,000-row fixture"
                            )
                        );
                }
            }
            else if (scenario->classSection == ClassesSection::Analytics)
            {
                const auto hasVisibleEmptyMessage = [&window]()
                {
                    for (QLabel* label : window->findChildren<QLabel*>())
                    {
                        if (
                            label
                            && label->isVisible()
                            && label->text().contains(
                                QStringLiteral("No scored speaking evaluations")
                                )
                            )
                        {
                            return true;
                        }
                    }
                    return false;
                };

                if (scenario->state == QStringLiteral("empty"))
                {
                    QTRY_VERIFY_WITH_TIMEOUT(hasVisibleEmptyMessage(), 5000);
                    actions.append(
                        QStringLiteral("Assert the analytics empty state")
                        );
                }
                else if (scenario->state == QStringLiteral("populated"))
                {
                    QTRY_VERIFY_WITH_TIMEOUT(!hasVisibleEmptyMessage(), 5000);
                    actions.append(
                        QStringLiteral("Assert scored analytics are visible")
                        );
                }
            }
            else if (scenario->classSection == ClassesSection::Evaluations)
            {
                auto* table = window->findChild<QTableView*>(
                    QStringLiteral("classEvaluationsTable")
                    );
                QVERIFY2(table, "Speaking-evaluation table was not instantiated.");
                auto* model = qobject_cast<SpeakingEvalModel*>(table->model());
                QVERIFY2(
                    model,
                    "Speaking-evaluation table did not expose its production model."
                    );

                const int nameColumn =
                    SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);
                const int expectedNames =
                    scenario->state == QStringLiteral("large") ? 25 : 2;
                int populatedNames = 0;
                for (int row = 0; row < model->rowCount(); ++row)
                {
                    if (!model->data(model->index(row, nameColumn)).toString().isEmpty())
                    {
                        ++populatedNames;
                    }
                }
                QVERIFY2(
                    populatedNames >= expectedNames,
                    qPrintable(
                        QStringLiteral(
                            "Expected at least %1 populated evaluation rows, got %2."
                            )
                            .arg(expectedNames)
                            .arg(populatedNames)
                        )
                    );
                actions.append(
                    QStringLiteral("Assert populated speaking-evaluation rows")
                    );
            }
            break;

        case WindowsQtCaptureMutation::ClassDetailsDirty:
        {
            auto* details = window->findChild<ClassDetailsSection*>();
            QVERIFY2(details, "Class-details section was not instantiated.");
            auto* readingBook = details->readingBookEditor();
            QVERIFY(readingBook);
            const int editedIndex = readingBook->findText(
                QStringLiteral("Reading Explorer 4")
                );
            QVERIFY2(
                editedIndex >= 0,
                "The deterministic class-details fixture has no alternate reading book."
                );
            readingBook->setCurrentIndex(editedIndex);
            QTRY_VERIFY_WITH_TIMEOUT(
                classesForScenario->hasUnsavedChanges(),
                5000
                );
            actions.append(
                QStringLiteral("Change the reading book and retain unsaved class-details state")
                );
            break;
        }

        case WindowsQtCaptureMutation::ClassDetailsValidation:
        {
            auto* details = window->findChild<ClassDetailsSection*>();
            QVERIFY2(details, "Class-details section was not instantiated.");
            auto* level = details->levelEditor();
            QVERIFY(level);
            const int emptyIndex = level->findText(QString());
            QVERIFY(emptyIndex >= 0);
            level->setCurrentIndex(emptyIndex);

            auto* validation = window->findChild<QLabel*>(
                QStringLiteral("classLevelValidationMessage")
                );
            QVERIFY2(validation, "Class-details validation label was not instantiated.");
            QTRY_VERIFY_WITH_TIMEOUT(
                !validation->text().trimmed().isEmpty(),
                5000
                );
            actions.append(
                QStringLiteral("Clear the class level and assert validation feedback")
                );
            break;
        }

        case WindowsQtCaptureMutation::RosterDirty:
        {
            auto* table = window->findChild<QTableView*>(
                QStringLiteral("rosterTable")
                );
            QVERIFY2(table, "Roster table was not instantiated.");
            auto* model = qobject_cast<RosterModel*>(table->model());
            QVERIFY2(model, "Roster table did not expose its production model.");
            const int englishColumn = model->englishNameColumn();
            QVERIFY(englishColumn >= 0);
            QVERIFY(model->setData(
                model->index(0, englishColumn),
                QStringLiteral("Fixture Edited Student"),
                Qt::EditRole
                ));
            QTRY_VERIFY_WITH_TIMEOUT(
                classesForScenario->hasUnsavedChanges(),
                5000
                );
            QVERIFY(model->isDirty());
            actions.append(
                QStringLiteral("Edit the first roster name and retain unsaved state")
                );
            break;
        }

        case WindowsQtCaptureMutation::SpeakingEvaluationDirty:
        case WindowsQtCaptureMutation::SpeakingEvaluationError:
        {
            auto* table = window->findChild<QTableView*>(
                QStringLiteral("classEvaluationsTable")
                );
            QVERIFY2(table, "Speaking-evaluation table was not instantiated.");
            auto* model = qobject_cast<SpeakingEvalModel*>(table->model());
            QVERIFY2(
                model,
                "Speaking-evaluation table did not expose its production model."
                );

            const int grammarColumn =
                SpeakingEval::toInt(SpeakingEvalColumn::Grammar);
            const QString value =
                scenario->mutation == WindowsQtCaptureMutation::SpeakingEvaluationError
                    ? QStringLiteral("INVALID")
                    : QStringLiteral("A");
            QVERIFY(model->setData(
                model->index(0, grammarColumn),
                value,
                Qt::EditRole
                ));
            QTRY_VERIFY_WITH_TIMEOUT(
                classesForScenario->hasUnsavedChanges(),
                5000
                );
            QVERIFY(model->isDirty());

            if (scenario->mutation == WindowsQtCaptureMutation::SpeakingEvaluationError)
            {
                QVERIFY(!model->errorsForCell(0, grammarColumn).isEmpty());
                auto* validation = window->findChild<QLabel*>(
                    QStringLiteral("speakingEvalValidationMessage")
                    );
                QVERIFY2(
                    validation,
                    "Speaking-evaluation validation label was not instantiated."
                    );
                QTRY_VERIFY_WITH_TIMEOUT(
                    !validation->text().trimmed().isEmpty(),
                    5000
                    );
                actions.append(
                    QStringLiteral("Enter an invalid score and assert evaluation errors")
                    );
            }
            else
            {
                actions.append(
                    QStringLiteral("Edit the first evaluation score and retain unsaved state")
                    );
            }
            break;
        }
        }
    }

    actions.append(
        QStringLiteral("Allow layout, fonts, and images to settle for %1 ms")
            .arg(SettleMilliseconds)
        );
    QTest::qWait(SettleMilliseconds);
    QApplication::processEvents(QEventLoop::AllEvents, 100);

    QScreen* screen = captureTarget ? captureTarget->screen() : nullptr;
    QVERIFY2(screen, "No screen is associated with the captured window.");

    const int actualScalePercent = displayScalePercent(screen);
    QVERIFY2(
        actualScalePercent == 100
            || actualScalePercent == 150
            || actualScalePercent == 200,
        qPrintable(
            QStringLiteral(
                "Unsupported display scale %1%; run on the fixed 100/150/200% matrix."
                ).arg(actualScalePercent)
            )
        );

    const QByteArray expectedScaleValue =
        qgetenv("CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT");
    if (!expectedScaleValue.isEmpty())
    {
        bool ok = false;
        const int expectedScale = expectedScaleValue.toInt(&ok);
        QVERIFY2(ok, "CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT is not an integer.");
        QCOMPARE(actualScalePercent, expectedScale);
    }

    const WId captureWindowId = captureTarget
        ? captureTarget->winId()
        : 0;
    QVERIFY(captureWindowId != 0);

    const QPixmap pixmap = screen->grabWindow(captureWindowId);
    QVERIFY2(!pixmap.isNull(), "Native Windows capture returned an empty pixmap.");

    const QImage image = pixmap.toImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
    QVERIFY(image.height() > 0);

    const QRgb firstPixel = image.pixel(0, 0);
    bool hasPixelVariation = false;
    for (int y = 0; y < image.height() && !hasPixelVariation; ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (image.pixel(x, y) != firstPixel)
            {
                hasPixelVariation = true;
                break;
            }
        }
    }
    QVERIFY2(hasPixelVariation, "Captured surface is uniformly blank.");

    const QString baseName = QStringLiteral("%1__%2__%3__%4__%5")
        .arg(
            scenario->artifactPrefix,
            scenario->state,
            themeName(scenario->theme),
            QString::number(actualScalePercent),
            appLanguageName(scenario->language)
            );
    const QString imageFileName = baseName + QStringLiteral(".png");
    const QString imagePath = QDir(m_captureRoot).filePath(imageFileName);
    QVERIFY2(!QFileInfo::exists(imagePath), qPrintable(
        QStringLiteral("Refusing to overwrite capture: %1")
            .arg(imagePath)
        ));

    QSaveFile imageFile(imagePath);
    QVERIFY(imageFile.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&imageFile, "PNG"));
    QVERIFY(imageFile.commit());
    actions.append(QStringLiteral("Capture native Windows window pixels"));

    const QSize capturedWindowSize = captureTarget->size();
    const QRect capturedWindowBounds = captureTarget->frameGeometry();

    if (openMenu)
    {
        openMenu->close();
        QTRY_VERIFY_WITH_TIMEOUT(!openMenu->isVisible(), 3000);
    }

    if (capturedDialog)
    {
        capturedDialog->close();
        QTRY_VERIFY_WITH_TIMEOUT(!capturedDialog->isVisible(), 3000);
        capturedDialog.reset();
        drainDeferredDeletes();
    }

    if (
        classesForScenario
        && scenario->mutation != WindowsQtCaptureMutation::None
        )
    {
        classesForScenario->discardChanges();
        QVERIFY2(
            !classesForScenario->hasUnsavedChanges(),
            qPrintable(
                QStringLiteral("Unable to discard synthetic changes after %1")
                    .arg(scenario->id)
                )
            );
        actions.append(QStringLiteral("Discard synthetic changes before teardown"));
    }

    QVERIFY(window->close());
    window.reset();
    drainDeferredDeletes();
    actions.append(QStringLiteral("Close and drain deferred destruction"));

    QTRY_COMPARE_WITH_TIMEOUT(
        QThreadPool::globalInstance()->activeThreadCount(),
        baselineThreadCount,
        5000
        );

    QVERIFY2(
        topLevelWidgets() == baselineTopLevels,
        qPrintable(
            QStringLiteral("Top-level widget leak after %1: %2")
                .arg(scenario->id, topLevelWidgetNames(baselineTopLevels))
            )
        );
    QVERIFY2(
        QApplication::activeModalWidget() == nullptr,
        "A modal widget remained after the capture lifecycle."
        );

    const QStringList remainingConnectionNames =
        QSqlDatabase::connectionNames();
    const QSet<QString> remainingConnections(
        remainingConnectionNames.cbegin(),
        remainingConnectionNames.cend()
        );
    QCOMPARE(remainingConnections, baselineConnections);

    for (const QString& packId : resourcePackIds())
    {
        QVERIFY2(!ResourcePackManager::instance().isMounted(packId),
            qPrintable(
                QStringLiteral("Resource pack remained mounted after %1: %2")
                    .arg(scenario->id, packId)
                ));
    }

    if (!databasePath.isEmpty())
    {
        QVERIFY2(
            !QFileInfo::exists(databasePath) || QFile::remove(databasePath),
            qPrintable(
                QStringLiteral("Database fixture remained locked: %1")
                    .arg(databasePath)
                )
            );
    }

    const QDir databaseDirectory(databaseRoot.path());
    QCOMPARE(
        databaseDirectory.entryList(
            QDir::NoDotAndDotDot | QDir::AllEntries,
            QDir::Name
            ),
        QStringList{}
        );

    writeCaptureMetadata(
        *scenario,
        imagePath,
        imageFileName,
        actualScalePercent,
        capturedWindowSize,
        capturedWindowBounds,
        actions
        );
}

void WindowsQtVisualCaptureTests::writeCaptureMetadata(
    const WindowsQtCaptureScenario& scenario,
    const QString& imagePath,
    const QString& imageFileName,
    int actualScalePercent,
    const QSize& actualWindowSize,
    const QRect& windowBounds,
    const QStringList& actions
    )
{
    const QString metadataPath =
        QDir(m_captureRoot).filePath(
            QFileInfo(imageFileName).completeBaseName()
                + QStringLiteral(".json")
            );
    QVERIFY2(!QFileInfo::exists(metadataPath), qPrintable(
        QStringLiteral("Refusing to overwrite metadata: %1")
            .arg(metadataPath)
        ));

    QJsonObject metadata;
    metadata.insert(
        QStringLiteral("format"),
        QStringLiteral("classmngr-phase0-capture-v1")
        );
    metadata.insert(QStringLiteral("ledgerId"), scenario.ledgerId);
    metadata.insert(
        QStringLiteral("sourceRevision"),
        QString::fromLatin1(BuildInfo::GitRevision)
        );
    metadata.insert(QStringLiteral("fixtureId"), scenario.fixtureId);
    metadata.insert(QStringLiteral("architecture"), architectureName());

    QJsonObject windows;
    windows.insert(QStringLiteral("edition"), QSysInfo::prettyProductName());
    windows.insert(QStringLiteral("build"), QSysInfo::kernelVersion());
    metadata.insert(QStringLiteral("windows"), windows);

    metadata.insert(
        QStringLiteral("displayScalePercent"),
        actualScalePercent
        );
    metadata.insert(QStringLiteral("theme"), themeName(scenario.theme));
    metadata.insert(
        QStringLiteral("appLanguage"),
        appLanguageName(scenario.language)
        );
    metadata.insert(
        QStringLiteral("inputLanguage"),
        inputLanguageName(scenario.language)
        );
    metadata.insert(QStringLiteral("fontSize"), QStringLiteral("normal"));

    QJsonObject window;
    window.insert(QStringLiteral("width"), actualWindowSize.width());
    window.insert(QStringLiteral("height"), actualWindowSize.height());
    window.insert(QStringLiteral("x"), windowBounds.x());
    window.insert(QStringLiteral("y"), windowBounds.y());
    metadata.insert(QStringLiteral("window"), window);

    QJsonArray actionArray;
    for (const QString& action : actions)
    {
        actionArray.append(action);
    }
    metadata.insert(QStringLiteral("actions"), actionArray);

    QJsonObject artifact;
    artifact.insert(QStringLiteral("file"), imageFileName);
    const QString imageHash = sha256ForFile(imagePath);
    QVERIFY2(!imageHash.isEmpty(), "Unable to hash the captured PNG.");
    artifact.insert(QStringLiteral("sha256"), imageHash);
    metadata.insert(QStringLiteral("artifact"), artifact);

    QJsonObject observations;
    observations.insert(
        QStringLiteral("keyboard"),
        QStringLiteral("Not exercised by this automated visual capture.")
        );
    observations.insert(
        QStringLiteral("inputMethod"),
        QStringLiteral("Manual Korean IME evidence remains pending.")
        );
    observations.insert(
        QStringLiteral("accessibility"),
        QStringLiteral(
            "UI Automation, screen-reader, and high-contrast evidence are "
            "deferred from the current roadmap."
            )
        );
    observations.insert(
        QStringLiteral("notes"),
        QStringLiteral(
            "Production-entry-point capture; review the PNG before promoting it to a golden."
            )
        );
    metadata.insert(QStringLiteral("observations"), observations);
    metadata.insert(QStringLiteral("verification"), QStringLiteral("captured"));

    QSaveFile metadataFile(metadataPath);
    QVERIFY(metadataFile.open(QIODevice::WriteOnly));
    QVERIFY(metadataFile.write(
        QJsonDocument(metadata).toJson(QJsonDocument::Indented)
        ) > 0);
    QVERIFY(metadataFile.commit());
}

void WindowsQtVisualCaptureTests::cleanupTestCase()
{
    if (m_languageService)
    {
        m_languageService->setLanguage(Language::SystemDefault);
        m_languageService.reset();
    }

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();

    if (m_previousSettingsRootWasSet)
    {
        qputenv("CLASSMNGR_SETTINGS_ROOT", m_previousSettingsRoot);
    }
    else
    {
        qunsetenv("CLASSMNGR_SETTINGS_ROOT");
    }

    if (m_previousAppDataWasSet)
    {
        qputenv("APPDATA", m_previousAppData);
    }
    else
    {
        qunsetenv("APPDATA");
    }

    if (m_previousLocalAppDataWasSet)
    {
        qputenv("LOCALAPPDATA", m_previousLocalAppData);
    }
    else
    {
        qunsetenv("LOCALAPPDATA");
    }

    QStandardPaths::setTestModeEnabled(false);
}

QTEST_MAIN(WindowsQtVisualCaptureTests)

#include "windows_qt_visual_capture_tests.moc"
