#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/classes_navigator_page.h"
#include "next/ui/page_host.h"
#include "next/ui/selected_class_editor.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableView>
#include <QTextEdit>
#include <QToolButton>
#include <QTranslator>
#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Application::ClassesNavigationEntry navigationEntry(
    const int id,
    const QString& name,
    const QString& grade,
    const QString& level,
    const QString& regularDay = {},
    const QString& regularStart = {},
    const QString& intensiveDay = {},
    const QString& intensiveStart = {},
    const QString& teacherEnglish = {},
    const QString& teacherKorean = {}
    )
{
    Application::ClassesNavigationDetails details{
        .grade = grade.toStdU16String(),
        .level = level.toStdU16String(),
        .teacherEnglishName = teacherEnglish.toStdU16String(),
        .teacherKoreanName = teacherKorean.toStdU16String()
    };
    if (!regularDay.isEmpty())
    {
        details.regularSchedule.push_back({
            .day = regularDay.toStdU16String(),
            .startTime = regularStart.toStdU16String(),
            .endTime = (regularStart + QStringLiteral(" + 50m")).toStdU16String()
        });
    }
    if (!intensiveDay.isEmpty())
    {
        details.intensiveSchedule.push_back({
            .day = intensiveDay.toStdU16String(),
            .startTime = intensiveStart.toStdU16String(),
            .endTime = (intensiveStart + QStringLiteral(" + 50m")).toStdU16String()
        });
    }
    return {
        .classId = classId(id),
        .className = name.toStdU16String(),
        .details = Application::ClassesNavigationDetailsResult::success(
            std::move(details)
            )
    };
}

Application::ClassesListSnapshot classList(
    const std::vector<std::pair<int, QString>>& rows
    )
{
    Application::ClassesListSnapshot result;
    result.classes.reserve(rows.size());
    for (const auto& [id, name] : rows)
    {
        result.classes.push_back({
            .classId = classId(id),
            .className = name.toStdU16String()
        });
    }
    return result;
}

Application::ClassesNavigationSnapshot navigation(
    std::vector<Application::ClassesNavigationEntry> rows
    )
{
    return {.classes = std::move(rows)};
}

class EmptyPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

struct ReadState final
{
    Application::ClassesListSnapshot classes;
    Application::ClassesNavigationSnapshot navigation;
    Application::ClassesNavigatorPreferences preferences{
        .scheduleDisplayMode = Application::ScheduleDisplayMode::Regular,
        .visibilityScope = Application::ClassVisibilityScope::ActiveSchedule,
        .dayFilterResetPolicy =
            Application::ClassDayFilterResetPolicy::OnApplicationClose,
        .classSelectionResetPolicy =
            Application::ClassSelectionResetPolicy::OnApplicationClose
    };
    Application::SaveMode saveMode = Application::SaveMode::Automatic;
    QString identity = QStringLiteral("workspace-a");
    QString path = QStringLiteral("workspace-a.tps");
    bool workspaceOpen = true;
    bool failFirstListRead = false;
    bool failEveryListRead = false;
    bool failFirstNavigationRead = false;
    bool failEveryNavigationRead = false;
    bool failDetailsRead = false;
    bool failNotesRead = false;
    bool failNotesTeacherRead = false;
    bool failAnalyticsRead = false;
    bool analyticsDataAvailable = true;
    bool blankNotesClassMetadata = false;
    bool failNotesSave = false;
    bool failScheduleSave = false;
    bool failVisibilitySave = false;
    bool failDetailsSave = false;
    bool failCoTeacherSave = false;
    bool failTeacherDetailsRead = false;
    int scheduleRowsPerSource = 1;
    bool confirmDelete = true;
    Ui::UnsavedChangesChoice unsavedChoice = Ui::UnsavedChangesChoice::Cancel;
    std::optional<Domain::ClassId> sharedSelection;
    std::unordered_map<std::string, QString> savedGrades;
    std::unordered_map<std::string, QString> savedNotes;
    std::unordered_map<std::string, QString> savedTimeFillerActivities;
    std::unordered_map<std::string, std::optional<Domain::TeacherId>> coTeacherIds;
    std::vector<std::string> blankClassIds;
    std::vector<std::string> teacherDetailReadIds;
    std::vector<QString> notesReadPaths;
    std::vector<QString> notesSavePaths;
    int listReadCount = 0;
    int navigationReadCount = 0;
    int scheduleSaveCount = 0;
    int visibilitySaveCount = 0;
    int detailsReadCount = 0;
    int coTeacherReadCount = 0;
    int teacherChoicesReadCount = 0;
    int teacherDetailsReadCount = 0;
    int detailsSaveCount = 0;
    int notesReadCount = 0;
    int notesSaveCount = 0;
    int analyticsReadCount = 0;
    int analyticsRankingCount = 25;
    int coTeacherSaveCount = 0;
    int createCount = 0;
    int deleteCount = 0;
    int resolveCount = 0;
    int confirmDeleteCount = 0;
    QString lastUnsavedReason;
    std::vector<Domain::ClassId> analyticsClassIds;
    std::vector<Application::ClassAnalyticsPageReadRequest> analyticsRequests;
    std::vector<Application::ClassesNavigationSnapshotQuery> navigationQueries;
};

struct PageFixture final
{
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    ReadState state;

    bool initialize()
    {
        const auto loaded = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
            );
        if (!loaded)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(std::move(*loaded));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        Ui::PageDescriptor classes{
            .id = *Ui::PageId::fromString(QStringLiteral("classes")),
            .label = QStringLiteral("Classes"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::ClassesNavigatorPageActions actions;
                actions.saveMode = state.saveMode;
                actions.workspaceIdentity = [this] { return state.identity; };
                actions.activeWorkspacePath = [this]() -> std::optional<QString> {
                    return state.workspaceOpen
                        ? std::optional<QString>(state.path)
                        : std::nullopt;
                };
                actions.readClasses = [this](const QString&) {
                    ++state.listReadCount;
                    if (state.failEveryListRead
                        || (state.failFirstListRead && state.listReadCount == 1))
                    {
                        return Application::ClassesListReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "class list source failed",
                            .recoverable = true
                        });
                    }
                    return Application::ClassesListReadResult::success(state.classes);
                };
                actions.readNavigation = [this](
                    const QString&,
                    const Application::ClassesNavigationSnapshotQuery& query
                    ) {
                    ++state.navigationReadCount;
                    state.navigationQueries.push_back(query);
                    if (state.failEveryNavigationRead
                        || (state.failFirstNavigationRead
                            && state.navigationReadCount == 1))
                    {
                        return Application::ClassesNavigationSnapshotResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "navigation metadata source failed",
                            .recoverable = true
                        });
                    }
                    return Application::ClassesNavigationSnapshotResult::success(
                        state.navigation
                        );
                };
                actions.readPreferences = [this](const QString&) {
                    return state.preferences;
                };
                actions.saveScheduleMode = [this](
                    const QString&,
                    const Application::ScheduleDisplayMode mode
                    ) {
                    ++state.scheduleSaveCount;
                    if (state.failScheduleSave)
                    {
                        return Application::ScheduleDisplayModePreferencesSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "schedule source save failed",
                            .recoverable = true
                        });
                    }
                    state.preferences.scheduleDisplayMode = mode;
                    return Application::ScheduleDisplayModePreferencesSaveResult::success();
                };
                actions.saveVisibilityScope = [this](
                    const QString&,
                    const Application::ClassVisibilityScope scope
                    ) {
                    ++state.visibilitySaveCount;
                    if (state.failVisibilitySave)
                    {
                        return Application::ClassVisibilityPreferencesSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "visibility save failed",
                            .recoverable = true
                        });
                    }
                    state.preferences.visibilityScope = scope;
                    return Application::ClassVisibilityPreferencesSaveResult::success();
                };
                actions.activeClassSelection = [this] {
                    return state.sharedSelection;
                };
                actions.setActiveClassSelection = [this](
                    const std::optional<Domain::ClassId>& selection
                    ) {
                    state.sharedSelection = selection;
                };
                actions.readClassDetails = [this](
                    const QString&,
                    const Domain::ClassId& id
                    ) {
                    ++state.detailsReadCount;
                    if (state.failDetailsRead)
                    {
                        return Application::ClassDetailsPageReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "class details read failed",
                            .recoverable = true
                        });
                    }
                    const std::string key = id.value();
                    const bool blank = std::find(state.blankClassIds.cbegin(),
                        state.blankClassIds.cend(), key) != state.blankClassIds.cend();
                    Application::ClassDetailsPageFields fields;
                    fields.classGrade = blank ? std::string{} : "E4";
                    fields.classLevel = blank ? std::string{} : "Theseus";
                    fields.readingBook = blank ? std::string{} : "Reading Explorer 1";
                    fields.essayBook = blank ? std::string{} : "4A";
                    fields.classColor = "#FFFFFF";
                    fields.fontColor = "#000000";
                    if (const auto saved = state.savedGrades.find(key);
                        saved != state.savedGrades.end())
                    {
                        fields.classGrade = saved->second.toStdString();
                    }
                    for (int row = 0; row < state.scheduleRowsPerSource; ++row)
                    {
                        fields.regularSchedule.push_back({
                            "Monday", "9:00 AM", "9:50 AM"});
                        fields.intensiveSchedule.push_back({
                            "Tuesday", "10:00 AM", "10:50 AM"});
                    }
                    return Application::ClassDetailsPageReadResult::success({
                        .classId = id,
                        .classFields = Domain::Result<Application::ClassDetailsPageFields>::success(
                            std::move(fields)),
                        .teacherDisplayName = Domain::Result<std::string>::success("Main Teacher"),
                        .studentCount = Domain::Result<Application::ClassDetailsStudentCount>::success(
                            Application::ClassDetailsStudentCount(3))
                    });
                };
                actions.readClassNotes = [this](
                    const QString& path,
                    const Domain::ClassId& id
                    ) {
                    ++state.notesReadCount;
                    state.notesReadPaths.push_back(path);
                    if (state.failNotesRead)
                    {
                        return Application::ClassNotesPageReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "class notes read failed",
                            .recoverable = true
                        });
                    }
                    const std::string key = id.value();
                    const QString notes = state.savedNotes.contains(key)
                        ? state.savedNotes.at(key)
                        : QStringLiteral("  Notes for %1  \n").arg(key);
                    const QString activities =
                        state.savedTimeFillerActivities.contains(key)
                        ? state.savedTimeFillerActivities.at(key)
                        : QStringLiteral("  Activities for %1  \n").arg(key);
                    Application::ClassNotesPageFields fields{
                        .classGrade = state.blankNotesClassMetadata
                            ? std::u16string{} : std::u16string(u"E4"),
                        .classLevel = state.blankNotesClassMetadata
                            ? std::u16string{} : std::u16string(u"Theseus"),
                        .regularSchedule = {{u"Monday", u"9:00 AM"}},
                        .notes = notes.toStdU16String(),
                        .timeFillerActivities = activities.toStdU16String()
                    };
                    Domain::Result<std::u16string> teacherName =
                        state.failNotesTeacherRead
                        ? Domain::Result<std::u16string>::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "teacher display read failed",
                            .recoverable = true
                        })
                        : Domain::Result<std::u16string>::success(u"Preferred Teacher");
                    return Application::ClassNotesPageReadResult::success({
                        .classId = id,
                        .classFields = Domain::Result<
                            Application::ClassNotesPageFields>::success(std::move(fields)),
                        .teacherDisplayName = std::move(teacherName)
                    });
                };
                actions.readClassAnalytics = [this](
                    const QString&,
                    const Application::ClassAnalyticsPageReadRequest& request
                    ) {
                    ++state.analyticsReadCount;
                    state.analyticsClassIds.push_back(request.classId);
                    state.analyticsRequests.push_back(request);
                    if (state.failAnalyticsRead)
                    {
                        return Application::ClassAnalyticsPageReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "analytics read failed",
                            .recoverable = true
                        });
                    }
                    Application::ClassAnalyticsSnapshot snapshot;
                    snapshot.hasData = state.analyticsDataAvailable;
                    snapshot.classAverage3 =
                        Application::ClassAnalyticsAverageThousandths{4'125};
                    snapshot.classAverageLetter = Domain::SpeakingEvaluationGrade::A;
                    snapshot.rosterStudentCount =
                        Application::ClassAnalyticsRosterRowCount{
                            static_cast<std::size_t>(state.analyticsRankingCount)};
                    snapshot.fullyScoredCount =
                        Application::ClassAnalyticsFullyScoredStudentCount{
                            static_cast<std::size_t>(state.analyticsRankingCount)};
                    snapshot.strongestLabels = {u"Grammar"};
                    snapshot.focusLabels = {u"Fluency"};
                    for (int row = 0; row < state.analyticsRankingCount; ++row)
                    {
                        Application::ClassAnalyticsStudentRank rank;
                        rank.englishName = QStringLiteral("Student%1")
                            .arg(row + 1).toStdU16String();
                        rank.koreanName = u"학생";
                        rank.overall3 =
                            Application::ClassAnalyticsAverageThousandths{4'125};
                        rank.overallLetter = Domain::SpeakingEvaluationGrade::A;
                        snapshot.rankings.push_back(std::move(rank));
                    }
                    Application::ClassAnalyticsDashboard dashboard;
                    dashboard.selectedSnapshot = std::move(snapshot);
                    dashboard.classShapeEvaluationName = u"Winter";
                    dashboard.classShapeSnapshot = dashboard.selectedSnapshot;
                    dashboard.yearToDatePoints.push_back({
                        .evaluationName = u"Winter",
                        .classAverage3 =
                            Application::ClassAnalyticsAverageThousandths{4'125},
                        .classAverageLetter = Domain::SpeakingEvaluationGrade::A
                    });
                    return Application::ClassAnalyticsPageReadResult::success({
                        .classId = request.classId,
                        .dashboard = std::move(dashboard),
                        .selectedEvaluationName = request.useDefaultSelection
                            ? std::optional<std::u16string>(u"Winter")
                            : request.selectedEvaluationName
                    });
                };
                actions.readClassCoTeacher = [this](
                    const QString&,
                    const Domain::ClassId& id
                    ) {
                    ++state.coTeacherReadCount;
                    const auto found = state.coTeacherIds.find(id.value());
                    Application::ClassCoTeacherPageFields fields;
                    if (found != state.coTeacherIds.end())
                    {
                        fields.selectedTeacherId = found->second;
                    }
                    fields.classGrade = u"E4";
                    fields.classLevel = u"Theseus";
                    fields.regularSchedule.emplace_back(
                        std::u16string(u"Monday"), std::u16string(u"9:00 AM"));
                    return Application::ClassCoTeacherPageReadResult::success({
                        .classId = id,
                        .classFields = Domain::Result<Application::ClassCoTeacherPageFields>::success(
                            std::move(fields)),
                        .teacherDisplayName = Domain::Result<std::u16string>::success(u"Main Teacher")
                    });
                };
                actions.readClassCoTeacherChoices = [this](const QString&) {
                    ++state.teacherChoicesReadCount;
                    Application::ClassCoTeacherTeacherChoicesSnapshot snapshot;
                    snapshot.teachers.push_back({
                        .teacherId = *Domain::TeacherId::fromString("9223372036854775806"),
                        .teacherKr = u"공동 교사",
                        .teacherEn = u"Wide Teacher",
                        .roomNumber = u"Room 7",
                        .internetType = u"WiFi",
                        .wifiName = u"teacher-wifi",
                        .wifiPassword = u"secret",
                        .projectionType = u"HDMI",
                        .zoomId = u"zoom-wide",
                        .zoomPassword = u"secret"
                    });
                    snapshot.teachers.push_back({
                        .teacherId = *Domain::TeacherId::fromString("9223372036854775805"),
                        .teacherKr = u"보조 교사",
                        .teacherEn = u"Second Teacher",
                        .roomNumber = u"Room 8",
                        .internetType = u"WiFi",
                        .wifiName = u"second-wifi",
                        .wifiPassword = u"secret",
                        .projectionType = u"HDMI",
                        .zoomId = u"zoom-second",
                        .zoomPassword = u"secret"
                    });
                    return Application::ClassCoTeacherTeacherChoicesResult::success(
                        std::move(snapshot));
                };
                actions.readClassCoTeacherTeacherDetails = [this](
                    const QString&,
                    const Domain::TeacherId& teacherId
                    ) {
                    ++state.teacherDetailsReadCount;
                    state.teacherDetailReadIds.push_back(teacherId.value());
                    if (state.failTeacherDetailsRead)
                    {
                        return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "teacher profile read failed",
                            .recoverable = true
                        });
                    }
                    const bool first = teacherId.value() == "9223372036854775806";
                    return Domain::Result<Application::ClassCoTeacherTeacherChoice>::success({
                        .teacherId = teacherId,
                        .roomNumber = first ? u"Room 7" : u"Room 8",
                        .internetType = u"WiFi",
                        .wifiName = first ? u"teacher-wifi" : u"second-wifi",
                        .wifiPassword = u"secret",
                        .projectionType = u"HDMI",
                        .zoomId = first ? u"zoom-wide" : u"zoom-second",
                        .zoomPassword = u"secret"
                    });
                };
                actions.saveClassDetails = [this](
                    const QString&,
                    const Application::ClassDetailsSaveWorkflowRequest& request
                    ) {
                    ++state.detailsSaveCount;
                    if (state.failDetailsSave)
                    {
                        return Application::ClassDetailsSaveWorkflowResult{
                            Application::ClassDetailsSaveWorkflowFailure{
                                .stage = Application::ClassDetailsSaveWorkflowStage::Save,
                                .error = {
                                    .code = Domain::ErrorCode::Technical,
                                    .message = "class details save failed",
                                    .recoverable = true
                                }
                            }
                        };
                    }
                    state.savedGrades[request.input.classId.value()] =
                        QString::fromStdU16String(request.input.classGrade);
                    return Application::ClassDetailsSaveWorkflowResult{
                        Application::ClassDetailsSaveWorkflowSuccess{
                            .normalized = request.input
                        }
                    };
                };
                actions.saveClassNotes = [this](
                    const QString& path,
                    const Application::ClassNotesSaveRequest& request
                    ) {
                    ++state.notesSaveCount;
                    state.notesSavePaths.push_back(path);
                    if (state.failNotesSave)
                    {
                        return Application::ClassNotesSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "class notes save failed",
                            .recoverable = true
                        });
                    }
                    state.savedNotes[request.classId.value()] =
                        QString::fromStdU16String(request.notes);
                    state.savedTimeFillerActivities[request.classId.value()] =
                        QString::fromStdU16String(request.timeFillerActivities);
                    return Application::ClassNotesSaveResult::success();
                };
                actions.saveClassCoTeacher = [this](
                    const QString&,
                    const Application::ClassCoTeacherAssignmentRequest& request
                    ) {
                    ++state.coTeacherSaveCount;
                    if (state.failCoTeacherSave)
                    {
                        return Domain::Result<void>::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "Co-Teacher save failed",
                            .recoverable = true
                        });
                    }
                    state.coTeacherIds[request.classId.value()] = request.teacherId;
                    return Domain::Result<void>::success();
                };
                actions.createClass = [this](const QString&) {
                    ++state.createCount;
                    int next = 1;
                    for (const auto& entry : state.classes.classes)
                    {
                        next = std::max(next, std::stoi(entry.classId.value()) + 1);
                    }
                    const Domain::ClassId id = classId(next);
                    const QString name = QStringLiteral("New Class %1").arg(next);
                    state.classes.classes.push_back({id, name.toStdU16String()});
                    state.navigation.classes.push_back(navigationEntry(
                        next, name, {}, {}));
                    state.blankClassIds.push_back(id.value());
                    return Application::ClassCreateResult::success(id);
                };
                actions.deleteClass = [this](
                    const QString&,
                    const Application::ClassDeleteRequest& request
                    ) {
                    ++state.deleteCount;
                    std::erase_if(state.classes.classes, [&request](const auto& entry) {
                        return entry.classId == request.classId;
                    });
                    std::erase_if(state.navigation.classes, [&request](const auto& entry) {
                        return entry.classId == request.classId;
                    });
                    return Application::ClassDeleteResult::success();
                };
                actions.resolveUnsavedChanges = [this](const QString& reason) {
                    ++state.resolveCount;
                    state.lastUnsavedReason = reason;
                    return state.unsavedChoice;
                };
                actions.confirmDeleteClass = [this](const QString&) {
                    ++state.confirmDeleteCount;
                    return state.confirmDelete;
                };
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::ClassesNavigatorPage>(
                        context,
                        std::move(actions)
                        )
                    );
            }
        };
        Ui::PageDescriptor other{
            .id = *Ui::PageId::fromString(QStringLiteral("other")),
            .label = QStringLiteral("Other"),
            .factory = [](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<EmptyPage>(context)
                    );
            }
        };
        return host->registerPage(std::move(classes))
            && host->registerPage(std::move(other));
    }

    Ui::ClassesNavigatorPage* page() const
    {
        return dynamic_cast<Ui::ClassesNavigatorPage*>(
            host ? host->pageInstance(QStringLiteral("classes")) : nullptr
            );
    }

    bool activate()
    {
        return host && host->activatePage(QStringLiteral("classes"));
    }
};

class KoreanAllTranslator final : public QTranslator
{
public:
    QString translate(
        const char* context,
        const char* sourceText,
        const char* = nullptr,
        const int = -1
        ) const override
    {
        if (QByteArray(context) == QByteArrayLiteral("NextClassesPage")
            && QByteArray(sourceText) == QByteArrayLiteral("All"))
        {
            return QStringLiteral("모두");
        }
        if (QByteArray(context) == QByteArrayLiteral("NextClassesPage")
            && QByteArray(sourceText) == QByteArrayLiteral("Classes"))
        {
            return QStringLiteral("수업");
        }
        return {};
    }
};

int widgetCount(const QWidget* root)
{
    return root ? root->findChildren<QWidget*>().size() + 1 : 0;
}

} // namespace

class ClassesNavigatorPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void loadsLazilyAndKeepsAlwaysGroupedSmallListsPlusAll();
    void appliesSourceVisibilityDayAndWeekendFilters();
    void listFailureClearsProjectionAndRetryRecovers();
    void metadataFailureKeepsNameOnlyListAndRetryEnriches();
    void selectionSurvivesRefreshAndFallsBackWhenClassDisappears();
    void remembersSelectionIndependentlyForEachGradeAndAllGroup();
    void distinguishesNoWorkspaceFromEmptyClasses();
    void suspensionClearsProjectionAndHonorsResetPolicies();
    void ninetySixClassesUseOneCompactModelWithoutClassWidgets();
    void oneSelectedEditorServesNinetySixClassRows();
    void dirtyChoicesGuardClassSwitchAndRetainFailedDrafts();
    void detailsReadFailureHasVisibleRetryPath();
    void detailsAutosaveDebouncesAndRetainsFailedDrafts();
    void detailsManualSaveWaitsForExplicitSave();
    void notesLoadTrimTeacherFailureAndExplicitSave();
    void notesSubtitleUsesClassNameWhenMetadataIsBlank();
    void notesAutosaveManualModeAndFailedDrafts();
    void notesReadRetryDiscardAndTextBounds();
    void notesDraftGuardsClassAndWorkspaceChanges();
    void analyticsLoadsOnlySelectedClassAndUsesOneRankingModel();
    void analyticsReadFailureRetryEmptyStateAndSelectionRetention();
    void coTeacherKeepsWideIdsAndRetriesSaveFailures();
    void coTeacherLoadsOnlySelectedTeacherAndRetriesProfileRead();
    void scheduleEditorStopsAtFourteenRows();
    void addDeleteAndWorkspaceLifecycleClearSelectedEditor();
    void languageRefreshChangesLabelsWithoutChangingCanonicalSelection();
    void productionKoreanCatalogTranslatesNavigatorDisplay();
};

void ClassesNavigatorPageTests::
loadsLazilyAndKeepsAlwaysGroupedSmallListsPlusAll()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {12, QStringLiteral("Level twelve")},
        {7, QStringLiteral("Level seven")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(12, QStringLiteral("Level twelve"), QStringLiteral("E5"),
            QStringLiteral("Artemis"), QStringLiteral("Tuesday"), QStringLiteral("10:00")),
        navigationEntry(7, QStringLiteral("Level seven"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });

    QVERIFY(!fixture.host->hasPageInstance(QStringLiteral("classes")));
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(fixture.state.listReadCount, 1);
    QCOMPARE(fixture.state.navigationReadCount, 1);
    QCOMPARE(fixture.state.navigationQueries.front().classes.size(), std::size_t(2));
    QCOMPARE(page->model()->classCount(), 2);
    QCOMPARE(page->model()->visibleClassCount(), 2);
    QCOMPARE(page->model()->gradeGroupCount(), 2);
    QCOMPARE(page->gradeTabs()->count(), 3);
    QCOMPARE(page->gradeTabs()->tabText(0), QStringLiteral("E4"));
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("E5"));
    QCOMPARE(page->gradeTabs()->tabText(2), QStringLiteral("All"));
    QCOMPARE(page->model()->currentGroup(),
        Ui::ClassesNavigatorModel::allClassesGroupKey());
    QVERIFY(page->selectedClassId().has_value());
    QCOMPARE(page->selectedClassId()->value(), std::string("7"));
    QCOMPARE(page->classList()->model()->rowCount(), 2);
}

void ClassesNavigatorPageTests::
appliesSourceVisibilityDayAndWeekendFilters()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("Regular and weekend")},
        {2, QStringLiteral("Intensive Tuesday")},
        {3, QStringLiteral("No schedule")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("Regular and weekend"),
            QStringLiteral("E4"), QStringLiteral("Theseus"),
            QStringLiteral("Monday"), QStringLiteral("09:00"),
            QStringLiteral("Saturday"), QStringLiteral("10:00")),
        navigationEntry(2, QStringLiteral("Intensive Tuesday"),
            QStringLiteral("E4"), QStringLiteral("Perseus"), {}, {},
            QStringLiteral("Tuesday"), QStringLiteral("11:00")),
        navigationEntry(3, QStringLiteral("No schedule"),
            QStringLiteral("E5"), QStringLiteral("Artemis"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->visibleClassCount(), 1);
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QVERIFY(page->weekendAvailable());
    auto* weekend = page->findChild<QToolButton*>(
        QStringLiteral("classesNavigatorWeekendFilter"));
    QVERIFY(weekend);
    QVERIFY(!weekend->isHidden());

    page->scheduleSourceCombo()->setCurrentIndex(1);
    QCOMPARE(fixture.state.scheduleSaveCount, 1);
    QCOMPARE(page->model()->visibleClassCount(), 2);
    auto* tuesday = page->findChild<QToolButton*>(
        QStringLiteral("classesNavigatorTuesdayFilter"));
    QVERIFY(tuesday);
    tuesday->click();
    QCOMPARE(page->model()->visibleClassCount(), 1);
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));

    weekend->click();
    QCOMPARE(page->model()->visibleClassCount(), 2);
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));

    page->visibilityScopeCombo()->setCurrentIndex(0);
    QCOMPARE(fixture.state.visibilitySaveCount, 1);
    QCOMPARE(page->model()->visibleClassCount(), 2);
    tuesday->click();
    QCOMPARE(page->model()->visibleClassCount(), 1);
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    weekend->click();
    QCOMPARE(page->model()->visibleClassCount(), 3);
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));

    page->scheduleSourceCombo()->setCurrentIndex(0);
    QCOMPARE(page->model()->visibleClassCount(), 3);
    QCOMPARE(fixture.state.scheduleSaveCount, 2);
}

void ClassesNavigatorPageTests::
listFailureClearsProjectionAndRetryRecovers()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.failFirstListRead = true;
    fixture.state.classes = classList({{1, QStringLiteral("One")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("One"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 0);
    QVERIFY(page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"))
        ->text().contains(QStringLiteral("could not be loaded")));
    QVERIFY(!page->findChild<QToolButton*>(QStringLiteral("classesNavigatorRetry"))
        ->isHidden());

    page->findChild<QToolButton*>(QStringLiteral("classesNavigatorRetry"))->click();
    QCOMPARE(fixture.state.listReadCount, 2);
    QCOMPARE(fixture.state.navigationReadCount, 1);
    QCOMPARE(page->model()->classCount(), 1);
    QCOMPARE(page->model()->visibleClassCount(), 1);
}

void ClassesNavigatorPageTests::
metadataFailureKeepsNameOnlyListAndRetryEnriches()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.failFirstNavigationRead = true;
    fixture.state.classes = classList({
        {11, QStringLiteral("Alpha")},
        {22, QStringLiteral("Beta")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(11, QStringLiteral("Alpha"), QStringLiteral("E5"),
            QStringLiteral("Artemis"), QStringLiteral("Tuesday"), QStringLiteral("10:00")),
        navigationEntry(22, QStringLiteral("Beta"), QStringLiteral("E4"),
            QStringLiteral("Perseus"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QVERIFY(!page->navigationMetadataAvailable());
    QCOMPARE(page->model()->classCount(), 2);
    QCOMPARE(page->model()->visibleClassCount(), 2);
    QCOMPARE(page->model()->gradeGroupCount(), 1);
    QCOMPARE(page->model()->data(page->model()->index(0, 0)).toString(),
        QStringLiteral("Alpha"));
    QVERIFY(!page->weekendAvailable());
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("classesNavigatorStatus"));
    QVERIFY(status->text().contains(QStringLiteral("Showing class names")));
    QVERIFY(status->text().contains(QStringLiteral("navigation metadata source failed")));

    page->findChild<QToolButton*>(QStringLiteral("classesNavigatorRetry"))->click();
    QVERIFY(page->navigationMetadataAvailable());
    QCOMPARE(fixture.state.listReadCount, 2);
    QCOMPARE(fixture.state.navigationReadCount, 2);
    QCOMPARE(page->model()->gradeGroupCount(), 2);
}

void ClassesNavigatorPageTests::
selectionSurvivesRefreshAndFallsBackWhenClassDisappears()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("First")},
        {2, QStringLiteral("Second")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00")),
        navigationEntry(2, QStringLiteral("Second"), QStringLiteral("E4"),
            QStringLiteral("Perseus"), QStringLiteral("Tuesday"), QStringLiteral("10:00"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    const QModelIndex second = page->model()->indexForClassId(classId(2));
    QVERIFY(second.isValid());
    page->classList()->setCurrentIndex(second);
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QVERIFY(page->refreshFromStore());
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));

    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });
    QVERIFY(page->refreshFromStore());
    QVERIFY(page->selectedClassId().has_value());
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
}

void ClassesNavigatorPageTests::
remembersSelectionIndependentlyForEachGradeAndAllGroup()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("E4 One")},
        {2, QStringLiteral("E4 Two")},
        {3, QStringLiteral("E5 Three")},
        {4, QStringLiteral("E5 Four")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("E4 One"), QStringLiteral("E4"),
            QStringLiteral("One"), QStringLiteral("Monday"), QStringLiteral("09:00")),
        navigationEntry(2, QStringLiteral("E4 Two"), QStringLiteral("E4"),
            QStringLiteral("Two"), QStringLiteral("Tuesday"), QStringLiteral("10:00")),
        navigationEntry(3, QStringLiteral("E5 Three"), QStringLiteral("E5"),
            QStringLiteral("Three"), QStringLiteral("Wednesday"), QStringLiteral("11:00")),
        navigationEntry(4, QStringLiteral("E5 Four"), QStringLiteral("E5"),
            QStringLiteral("Four"), QStringLiteral("Thursday"), QStringLiteral("12:00"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    page->visibilityScopeCombo()->setCurrentIndex(
        page->visibilityScopeCombo()->findData(
            static_cast<int>(Application::ClassVisibilityScope::AllClasses)));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));

    const auto activateTab = [page](const QString& key) {
        for (int index = 0; index < page->gradeTabs()->count(); ++index)
        {
            if (page->gradeTabs()->tabData(index).toString() == key)
            {
                page->gradeTabs()->setCurrentIndex(index);
                return true;
            }
        }
        return false;
    };
    QVERIFY(activateTab(QStringLiteral("grade:E4")));
    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QVERIFY(activateTab(QStringLiteral("grade:E5")));
    QCOMPARE(page->selectedClassId()->value(), std::string("3"));
    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(4)));
    QCOMPARE(page->selectedClassId()->value(), std::string("4"));

    QVERIFY(activateTab(QStringLiteral("grade:E4")));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QVERIFY(activateTab( Ui::ClassesNavigatorModel::allClassesGroupKey()));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(3)));
    QCOMPARE(page->selectedClassId()->value(), std::string("3"));
    QVERIFY(activateTab(QStringLiteral("grade:E5")));
    QCOMPARE(page->selectedClassId()->value(), std::string("4"));
    QVERIFY(activateTab(Ui::ClassesNavigatorModel::allClassesGroupKey()));
    QCOMPARE(page->selectedClassId()->value(), std::string("3"));
}

void ClassesNavigatorPageTests::
distinguishesNoWorkspaceFromEmptyClasses()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.workspaceOpen = false;
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 0);
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"))
            ->text(),
        QStringLiteral("No Teacher Profile is open."));
    QCOMPARE(fixture.state.listReadCount, 0);

    fixture.state.workspaceOpen = true;
    fixture.state.identity = QStringLiteral("empty-workspace");
    fixture.state.classes = {};
    QVERIFY(page->refreshFromStore());
    QCOMPARE(page->model()->classCount(), 0);
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"))
            ->text(),
        QStringLiteral("No classes available."));
    QCOMPARE(fixture.state.listReadCount, 1);
    QCOMPARE(fixture.state.navigationReadCount, 0);
}

void ClassesNavigatorPageTests::
suspensionClearsProjectionAndHonorsResetPolicies()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("First")},
        {2, QStringLiteral("Second")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00")),
        navigationEntry(2, QStringLiteral("Second"), QStringLiteral("E4"),
            QStringLiteral("Perseus"), QStringLiteral("Tuesday"), QStringLiteral("10:00"))
    });
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(2)));
    auto* const monday = page->findChild<QToolButton*>(
        QStringLiteral("classesNavigatorMondayFilter"));
    monday->click();
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QCOMPARE(page->model()->classCount(), 0);
    QVERIFY(page->selectedClassId().has_value());
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QVERIFY(fixture.host->activatePage(QStringLiteral("classes")));
    QCOMPARE(page->model()->classCount(), 2);
    QVERIFY(page->selectedClassId().has_value());

    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(2)));
    fixture.state.preferences.classSelectionResetPolicy =
        Application::ClassSelectionResetPolicy::OnPageLeave;
    fixture.state.preferences.dayFilterResetPolicy =
        Application::ClassDayFilterResetPolicy::OnPageLeave;
    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QVERIFY(!page->selectedClassId().has_value());
    QVERIFY(!fixture.state.sharedSelection.has_value());
    QVERIFY(fixture.host->activatePage(QStringLiteral("classes")));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QVERIFY(!monday->isChecked());
}

void ClassesNavigatorPageTests::
ninetySixClassesUseOneCompactModelWithoutClassWidgets()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    std::vector<std::pair<int, QString>> classes;
    std::vector<Application::ClassesNavigationEntry> summaries;
    for (int index = 0; index < 96; ++index)
    {
        const int id = 1'000 + index;
        const QString name = QStringLiteral("Navigator Class %1").arg(index + 1);
        classes.emplace_back(id, name);
        summaries.push_back(navigationEntry(
            id,
            name,
            QStringLiteral("E4"),
            (index % 2) == 0 ? QStringLiteral("Theseus") : QStringLiteral("Perseus"),
            (index % 2) == 0 ? QStringLiteral("Monday") : QStringLiteral("Tuesday"),
            QStringLiteral("%1:00").arg(9 + (index % 5))
            ));
    }
    fixture.state.classes = classList(classes);
    fixture.state.navigation = navigation(std::move(summaries));

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 96);
    QCOMPARE(page->model()->visibleClassCount(), 96);
    QCOMPARE(page->model()->gradeGroupCount(), 1);
    QCOMPARE(page->gradeTabs()->count(), 2);
    QCOMPARE(page->classList()->model()->rowCount(), 96);
    QVERIFY(widgetCount(page) < 250);
    QCOMPARE(page->findChildren<QWidget*>(QStringLiteral("selectedClassEditor")).size(), 1);
    int classWidgetCount = 0;
    for (QWidget* widget : page->findChildren<QWidget*>())
    {
        classWidgetCount += widget->property("class_id").isValid() ? 1 : 0;
    }
    QCOMPARE(classWidgetCount, 0);
}

void ClassesNavigatorPageTests::oneSelectedEditorServesNinetySixClassRows()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    std::vector<std::pair<int, QString>> classes;
    std::vector<Application::ClassesNavigationEntry> summaries;
    for (int index = 0; index < 96; ++index)
    {
        const int id = 2'000 + index;
        const QString name = QStringLiteral("Class %1").arg(index + 1);
        classes.emplace_back(id, name);
        summaries.push_back(navigationEntry(
            id, name, QStringLiteral("E4"), QStringLiteral("Theseus"),
            QStringLiteral("Monday"), QStringLiteral("9:00 AM")));
    }
    fixture.state.classes = classList(classes);
    fixture.state.navigation = navigation(std::move(summaries));
    QVERIFY(fixture.activate());

    const Ui::ClassesNavigatorPage* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 96);
    QCOMPARE(page->classList()->model()->rowCount(), 96);
    const auto editors = page->findChildren<QWidget*>(
        QStringLiteral("selectedClassEditor"));
    QCOMPARE(editors.size(), 1);
    QVERIFY(page->selectedClassEditor()->isLoaded());
    QVERIFY(widgetCount(page) < 250);
    for (QWidget* const widget : page->findChildren<QWidget*>())
    {
        QVERIFY(!widget->property("class_id").isValid());
    }
}

void ClassesNavigatorPageTests::dirtyChoicesGuardClassSwitchAndRetainFailedDrafts()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("First")}, {2, QStringLiteral("Second")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM")),
        navigationEntry(2, QStringLiteral("Second"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Tuesday"),
            QStringLiteral("10:00 AM"))
    });
    QVERIFY(fixture.activate());
    Ui::ClassesNavigatorPage* const page = fixture.page();
    QVERIFY(page);
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    QVERIFY(editor);
    QCOMPARE(editor->classId()->value(), std::string("1"));
    QCOMPARE(fixture.state.sharedSelection->value(), std::string("1"));
    QVERIFY(editor->gradeChoice());

    editor->gradeChoice()->setCurrentText(QStringLiteral("E5"));
    QVERIFY(editor->hasUnsavedChanges());
    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Cancel;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(editor->classId()->value(), std::string("1"));
    QCOMPARE(fixture.state.resolveCount, 1);

    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Save;
    fixture.state.failDetailsSave = true;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(editor->gradeChoice()->currentText(), QStringLiteral("E5"));
    QVERIFY(editor->hasUnsavedChanges());
    QVERIFY(page->findChild<QLabel*>(QStringLiteral("classEditorDetailsStatus"))
        ->text().contains(QStringLiteral("class details save failed")));
    QCOMPARE(fixture.state.savedGrades.count("1"), std::size_t(0));

    fixture.state.failDetailsSave = false;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QCOMPARE(fixture.state.sharedSelection->value(), std::string("2"));
    QVERIFY(!editor->hasUnsavedChanges());
    QCOMPARE(fixture.state.savedGrades.at("1"), QStringLiteral("E5"));

    editor->gradeChoice()->setCurrentText(QStringLiteral("E6"));
    QVERIFY(editor->hasUnsavedChanges());
    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Discard;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(1)));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(editor->gradeChoice()->currentText(), QStringLiteral("E5"));
    QVERIFY(!editor->hasUnsavedChanges());
    QVERIFY(fixture.state.savedGrades.count("2") == 0);
}

void ClassesNavigatorPageTests::coTeacherKeepsWideIdsAndRetriesSaveFailures()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("First")}, {2, QStringLiteral("Second")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM")),
        navigationEntry(2, QStringLiteral("Second"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Tuesday"),
            QStringLiteral("10:00 AM"))
    });
    const QString wideId = QStringLiteral("9223372036854775806");
    const QString secondWideId = QStringLiteral("9223372036854775805");
    fixture.state.coTeacherIds["1"] = Domain::TeacherId::fromString(
        wideId.toStdString());
    QVERIFY(fixture.activate());
    Ui::ClassesNavigatorPage* const page = fixture.page();
    QVERIFY(page);
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    QVERIFY(editor);
    editor->sections()->setCurrentIndex(1);
    QCOMPARE(fixture.state.coTeacherReadCount, 1);
    QCOMPARE(fixture.state.teacherChoicesReadCount, 1);

    QComboBox* const locale = editor->findChild<QComboBox*>(
        QStringLiteral("classEditorCoTeacherLocale"));
    QComboBox* const teacher = editor->coTeacherChoice();
    QVERIFY(locale);
    QVERIFY(teacher);
    int teacherIndex = teacher->findData(wideId);
    QVERIFY(teacherIndex >= 0);
    QCOMPARE(teacher->itemData(teacherIndex).toString(), wideId);
    QCOMPARE(teacher->currentData().toString(), wideId);
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral("classEditorCoTeacherName"))
        ->text(), QStringLiteral("Wide Teacher"));
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral("classEditorCoTeacherRoom"))
        ->text(), QStringLiteral("Room 7"));
    QCOMPARE(fixture.state.teacherDetailsReadCount, 1);
    QCOMPARE(fixture.state.teacherDetailReadIds,
        std::vector<std::string>{"9223372036854775806"});
    locale->setCurrentIndex(locale->findData(QStringLiteral("ko")));
    teacherIndex = teacher->findData(wideId);
    QCOMPARE(teacher->itemText(teacherIndex), QStringLiteral("공동 교사"));
    locale->setCurrentIndex(locale->findData(QStringLiteral("en")));
    teacherIndex = teacher->findData(wideId);
    QCOMPARE(teacher->itemText(teacherIndex), QStringLiteral("Wide Teacher"));
    QCOMPARE(fixture.state.teacherDetailsReadCount, 1);
    teacher->setCurrentIndex(teacher->findData(secondWideId));
    QVERIFY(editor->hasUnsavedChanges());
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral("classEditorCoTeacherRoom"))
        ->text(), QStringLiteral("Room 8"));

    auto* const save = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorSaveCoTeacher"));
    auto* const status = editor->findChild<QLabel*>(
        QStringLiteral("classEditorCoTeacherStatus"));
    QVERIFY(save);
    QVERIFY(status);
    fixture.state.failCoTeacherSave = true;
    save->click();
    QVERIFY(editor->hasUnsavedChanges());
    QVERIFY(status->text().contains(QStringLiteral("Co-Teacher save failed")));
    QCOMPARE(fixture.state.coTeacherIds.at("1")->value(),
        std::string("9223372036854775806"));

    fixture.state.failCoTeacherSave = false;
    save->click();
    QVERIFY(!editor->hasUnsavedChanges());
    QVERIFY(fixture.state.coTeacherIds.at("1").has_value());
    QCOMPARE(fixture.state.coTeacherIds.at("1")->value(),
        std::string("9223372036854775805"));

    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QCOMPARE(editor->sections()->currentIndex(), 0);
    editor->sections()->setCurrentIndex(1);
    QCOMPARE(teacher->currentData().toString(), QString{});
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral("classEditorCoTeacherName"))
        ->text(), QStringLiteral("Unassigned"));
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral("classEditorCoTeacherRoom"))
        ->text(), QString{});
    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(1)));
    editor->sections()->setCurrentIndex(1);
    QCOMPARE(teacher->currentData().toString(), secondWideId);

    teacher->setCurrentIndex(teacher->findData(wideId));
    QVERIFY(editor->hasUnsavedChanges());
    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Cancel;
    editor->sections()->setCurrentIndex(0);
    QCOMPARE(editor->sections()->currentIndex(), 1);
    QVERIFY(editor->hasUnsavedChanges());
    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Discard;
    editor->sections()->setCurrentIndex(0);
    QCOMPARE(editor->sections()->currentIndex(), 0);
    QVERIFY(!editor->hasUnsavedChanges());
    editor->sections()->setCurrentIndex(1);
    QCOMPARE(teacher->currentData().toString(), secondWideId);
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral("classEditorCoTeacherRoom"))
        ->text(), QStringLiteral("Room 8"));
}

void ClassesNavigatorPageTests::
coTeacherLoadsOnlySelectedTeacherAndRetriesProfileRead()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    fixture.state.coTeacherIds["1"] = Domain::TeacherId::fromString(
        "9223372036854775806");
    fixture.state.failTeacherDetailsRead = true;
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor = fixture.page()->selectedClassEditor();
    QVERIFY(editor);
    editor->sections()->setCurrentIndex(1);
    auto* const status = editor->findChild<QLabel*>(
        QStringLiteral("classEditorCoTeacherStatus"));
    auto* const retry = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorRetryCoTeacher"));
    QVERIFY(status);
    QVERIFY(retry);
    QVERIFY(status->text().contains(QStringLiteral("teacher profile read failed")));
    QVERIFY(!retry->isHidden());
    QCOMPARE(fixture.state.teacherDetailsReadCount, 1);

    fixture.state.failTeacherDetailsRead = false;
    retry->click();
    QVERIFY(status->isHidden());
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral("classEditorCoTeacherRoom"))
        ->text(), QStringLiteral("Room 7"));
    QCOMPARE(fixture.state.teacherDetailsReadCount, 2);
}

void ClassesNavigatorPageTests::scheduleEditorStopsAtFourteenRows()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    fixture.state.scheduleRowsPerSource = 13;
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor = fixture.page()->selectedClassEditor();
    QVERIFY(editor);
    QCOMPARE(editor->regularSchedule()->rowCount(), 13);
    QCOMPARE(editor->intensiveSchedule()->rowCount(), 13);
    auto* const regularAdd = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorRegularScheduleAdd"));
    auto* const intensiveAdd = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorIntensiveScheduleAdd"));
    QVERIFY(regularAdd);
    QVERIFY(intensiveAdd);
    QVERIFY(regularAdd->isEnabled());
    QVERIFY(intensiveAdd->isEnabled());
    regularAdd->click();
    intensiveAdd->click();
    QCOMPARE(editor->regularSchedule()->rowCount(), 14);
    QCOMPARE(editor->intensiveSchedule()->rowCount(), 14);
    QVERIFY(!regularAdd->isEnabled());
    QVERIFY(!intensiveAdd->isEnabled());
    regularAdd->click();
    intensiveAdd->click();
    QCOMPARE(editor->regularSchedule()->rowCount(), 14);
    QCOMPARE(editor->intensiveSchedule()->rowCount(), 14);
}

void ClassesNavigatorPageTests::detailsReadFailureHasVisibleRetryPath()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    fixture.state.failDetailsRead = true;
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor = fixture.page()->selectedClassEditor();
    QVERIFY(editor);
    QVERIFY(!editor->isLoaded());
    auto* const status = editor->findChild<QLabel*>(
        QStringLiteral("classEditorDetailsStatus"));
    auto* const retry = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorRetryDetails"));
    QVERIFY(status);
    QVERIFY(retry);
    QVERIFY(status->text().contains(QStringLiteral("class details read failed")));
    QVERIFY(!retry->isHidden());

    fixture.state.failDetailsRead = false;
    retry->click();
    QVERIFY(editor->isLoaded());
    QVERIFY(retry->isHidden());
    QCOMPARE(fixture.state.detailsReadCount, 2);
    QCOMPARE(editor->gradeChoice()->currentText(), QStringLiteral("E4"));
}

void ClassesNavigatorPageTests::detailsAutosaveDebouncesAndRetainsFailedDrafts()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor = fixture.page()->selectedClassEditor();
    QVERIFY(editor);

    editor->gradeChoice()->setCurrentText(QStringLiteral("E5"));
    QVERIFY(editor->hasUnsavedChanges());
    QTest::qWait(500);
    QCOMPARE(fixture.state.detailsSaveCount, 0);
    QTRY_COMPARE_WITH_TIMEOUT(fixture.state.detailsSaveCount, 1, 500);
    QVERIFY(!editor->hasUnsavedChanges());
    QCOMPARE(fixture.state.savedGrades.at("1"), QStringLiteral("E5"));

    fixture.state.failDetailsSave = true;
    editor->gradeChoice()->setCurrentText(QStringLiteral("E6"));
    QTRY_COMPARE_WITH_TIMEOUT(fixture.state.detailsSaveCount, 2, 1'500);
    QVERIFY(editor->hasUnsavedChanges());
    QCOMPARE(editor->gradeChoice()->currentText(), QStringLiteral("E6"));
    QCOMPARE(fixture.state.savedGrades.at("1"), QStringLiteral("E5"));
}

void ClassesNavigatorPageTests::detailsManualSaveWaitsForExplicitSave()
{
    PageFixture fixture;
    fixture.state.saveMode = Application::SaveMode::Manual;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor = fixture.page()->selectedClassEditor();
    QVERIFY(editor);

    editor->gradeChoice()->setCurrentText(QStringLiteral("E5"));
    QVERIFY(editor->hasUnsavedChanges());
    QTest::qWait(900);
    QCOMPARE(fixture.state.detailsSaveCount, 0);
    QVERIFY(editor->hasUnsavedChanges());
    QCOMPARE(editor->gradeChoice()->currentText(), QStringLiteral("E5"));

    QPushButton* const save = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorSaveDetails"));
    QVERIFY(save);
    QVERIFY(save->isEnabled());
    save->click();
    QCOMPARE(fixture.state.detailsSaveCount, 1);
    QVERIFY(!editor->hasUnsavedChanges());
    QCOMPARE(fixture.state.savedGrades.at("1"), QStringLiteral("E5"));
}

void ClassesNavigatorPageTests::notesLoadTrimTeacherFailureAndExplicitSave()
{
    PageFixture fixture;
    fixture.state.saveMode = Application::SaveMode::Manual;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    fixture.state.savedNotes["1"] = QStringLiteral("  existing notes \n");
    fixture.state.savedTimeFillerActivities["1"] =
        QStringLiteral("  existing activities \n");
    fixture.state.failNotesTeacherRead = true;
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor = fixture.page()->selectedClassEditor();
    QVERIFY(editor);
    editor->sections()->setCurrentIndex(2);

    auto* const notes = editor->findChild<QTextEdit*>(
        QStringLiteral("classEditorClassNotes"));
    auto* const activities = editor->findChild<QTextEdit*>(
        QStringLiteral("classEditorTimeFillerActivities"));
    auto* const status = editor->findChild<QLabel*>(
        QStringLiteral("classEditorNotesStatus"));
    auto* const save = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorSaveNotes"));
    auto* const subtitle = editor->findChild<QLabel*>(
        QStringLiteral("classEditorNotesSubtitle"));
    QVERIFY(notes && activities && status && save && subtitle);
    QCOMPARE(notes->toPlainText(), QStringLiteral("existing notes"));
    QCOMPARE(activities->toPlainText(), QStringLiteral("existing activities"));
    QVERIFY(notes->isEnabled());
    QVERIFY(status->text().contains(QStringLiteral("could not be loaded")));
    QVERIFY(subtitle->text().contains(QStringLiteral("No Teacher")));
    QVERIFY(subtitle->text().contains(QStringLiteral("Mon (9:00)")));

    notes->setPlainText(QStringLiteral("  edited notes \n"));
    activities->setPlainText(QStringLiteral("  edited activities \n"));
    QVERIFY(editor->hasUnsavedChanges());
    save->click();
    QCOMPARE(fixture.state.notesSaveCount, 1);
    QCOMPARE(fixture.state.savedNotes.at("1"), QStringLiteral("edited notes"));
    QCOMPARE(fixture.state.savedTimeFillerActivities.at("1"),
        QStringLiteral("edited activities"));
    QVERIFY(!editor->hasUnsavedChanges());
    QVERIFY(status->isHidden());
}

void ClassesNavigatorPageTests::notesSubtitleUsesClassNameWhenMetadataIsBlank()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("Fallback Class Name")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("Fallback Class Name"), QStringLiteral(""),
            QStringLiteral(""), QStringLiteral("Monday"), QStringLiteral("9:00 AM"))
    });
    fixture.state.blankNotesClassMetadata = true;
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor = fixture.page()->selectedClassEditor();
    QVERIFY(editor);
    editor->sections()->setCurrentIndex(2);
    const auto* const subtitle = editor->findChild<QLabel*>(
        QStringLiteral("classEditorNotesSubtitle"));
    QVERIFY(subtitle);
    QVERIFY(subtitle->text().startsWith(
        QStringLiteral("Fallback Class Name • Preferred Teacher")));
    QVERIFY(subtitle->text().contains(QStringLiteral("Mon (9:00)")));
}

void ClassesNavigatorPageTests::notesAutosaveManualModeAndFailedDrafts()
{
    PageFixture automatic;
    QVERIFY(automatic.initialize());
    automatic.state.classes = classList({{1, QStringLiteral("First")} });
    automatic.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    QVERIFY(automatic.activate());
    Ui::SelectedClassEditor* const automaticEditor =
        automatic.page()->selectedClassEditor();
    QVERIFY(automaticEditor);
    automaticEditor->sections()->setCurrentIndex(2);
    auto* const automaticNotes = automaticEditor->findChild<QTextEdit*>(
        QStringLiteral("classEditorClassNotes"));
    QVERIFY(automaticNotes);
    automaticNotes->setPlainText(QStringLiteral("first automatic draft"));
    QVERIFY(automaticEditor->hasUnsavedChanges());
    QTest::qWait(500);
    QCOMPARE(automatic.state.notesSaveCount, 0);
    QTRY_COMPARE_WITH_TIMEOUT(automatic.state.notesSaveCount, 1, 500);
    QVERIFY(!automaticEditor->hasUnsavedChanges());
    QCOMPARE(automatic.state.savedNotes.at("1"),
        QStringLiteral("first automatic draft"));

    automatic.state.failNotesSave = true;
    automaticNotes->setPlainText(QStringLiteral("draft after save failure"));
    QTRY_COMPARE_WITH_TIMEOUT(automatic.state.notesSaveCount, 2, 1'500);
    QVERIFY(automaticEditor->hasUnsavedChanges());
    QCOMPARE(automaticNotes->toPlainText(), QStringLiteral("draft after save failure"));
    QCOMPARE(automatic.state.savedNotes.at("1"),
        QStringLiteral("first automatic draft"));
    QVERIFY(automaticEditor->findChild<QLabel*>(
        QStringLiteral("classEditorNotesStatus"))->text().contains(
            QStringLiteral("class notes save failed")));

    PageFixture manual;
    manual.state.saveMode = Application::SaveMode::Manual;
    QVERIFY(manual.initialize());
    manual.state.classes = classList({{1, QStringLiteral("First")} });
    manual.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    QVERIFY(manual.activate());
    Ui::SelectedClassEditor* const manualEditor =
        manual.page()->selectedClassEditor();
    QVERIFY(manualEditor);
    manualEditor->sections()->setCurrentIndex(2);
    auto* const manualNotes = manualEditor->findChild<QTextEdit*>(
        QStringLiteral("classEditorClassNotes"));
    auto* const manualSave = manualEditor->findChild<QPushButton*>(
        QStringLiteral("classEditorSaveNotes"));
    QVERIFY(manualNotes && manualSave);
    manualNotes->setPlainText(QStringLiteral("manual draft"));
    QTest::qWait(900);
    QCOMPARE(manual.state.notesSaveCount, 0);
    QVERIFY(manualEditor->hasUnsavedChanges());
    QVERIFY(manualSave->isEnabled());
    manualSave->click();
    QCOMPARE(manual.state.notesSaveCount, 1);
    QCOMPARE(manual.state.savedNotes.at("1"), QStringLiteral("manual draft"));
    QVERIFY(!manualEditor->hasUnsavedChanges());
}

void ClassesNavigatorPageTests::notesReadRetryDiscardAndTextBounds()
{
    PageFixture fixture;
    fixture.state.saveMode = Application::SaveMode::Manual;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    fixture.state.failNotesRead = true;
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor = fixture.page()->selectedClassEditor();
    QVERIFY(editor);
    editor->sections()->setCurrentIndex(2);
    auto* const status = editor->findChild<QLabel*>(
        QStringLiteral("classEditorNotesStatus"));
    auto* const retry = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorRetryNotes"));
    auto* const notes = editor->findChild<QTextEdit*>(
        QStringLiteral("classEditorClassNotes"));
    auto* const activities = editor->findChild<QTextEdit*>(
        QStringLiteral("classEditorTimeFillerActivities"));
    auto* const save = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorSaveNotes"));
    QVERIFY(status && retry && notes && activities && save);
    QVERIFY(status->text().contains(QStringLiteral("class notes read failed")));
    QVERIFY(!retry->isHidden());
    QVERIFY(!notes->isEnabled());
    fixture.state.failNotesRead = false;
    retry->click();
    QVERIFY(retry->isHidden());
    QVERIFY(notes->isEnabled());
    QCOMPARE(fixture.state.notesReadCount, 2);

    notes->setPlainText(QString(10'000, QLatin1Char('N')));
    save->click();
    QCOMPARE(fixture.state.notesSaveCount, 1);
    QCOMPARE(fixture.state.savedNotes.at("1").size(), 10'000);
    QVERIFY(!editor->hasUnsavedChanges());

    activities->setPlainText(QString(10'001, QLatin1Char('A')));
    save->click();
    QCOMPARE(fixture.state.notesSaveCount, 1);
    QVERIFY(editor->hasUnsavedChanges());
    QVERIFY(status->text().contains(QStringLiteral("10,000 UTF-16")));

    activities->setPlainText(QStringLiteral("persisted activities"));
    notes->setPlainText(QStringLiteral("discard this"));
    QVERIFY(editor->hasUnsavedChanges());
    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Discard;
    editor->sections()->setCurrentIndex(0);
    QCOMPARE(editor->sections()->currentIndex(), 0);
    QVERIFY(!editor->hasUnsavedChanges());
    editor->sections()->setCurrentIndex(2);
    QCOMPARE(notes->toPlainText(), QString(10'000, QLatin1Char('N')));
    QCOMPARE(activities->toPlainText(), QStringLiteral("Activities for 1"));
}

void ClassesNavigatorPageTests::notesDraftGuardsClassAndWorkspaceChanges()
{
    PageFixture fixture;
    fixture.state.saveMode = Application::SaveMode::Manual;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("First")}, {2, QStringLiteral("Second")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM")),
        navigationEntry(2, QStringLiteral("Second"), QStringLiteral("E5"),
            QStringLiteral("Artemis"), QStringLiteral("Tuesday"),
            QStringLiteral("10:00 AM"))
    });
    fixture.state.savedNotes["1"] = QStringLiteral("saved class one");
    fixture.state.savedNotes["2"] = QStringLiteral("saved class two");
    QVERIFY(fixture.activate());
    Ui::ClassesNavigatorPage* const page = fixture.page();
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    QVERIFY(editor);
    editor->sections()->setCurrentIndex(2);
    auto* const notes = editor->findChild<QTextEdit*>(
        QStringLiteral("classEditorClassNotes"));
    QVERIFY(notes);
    QCOMPARE(notes->toPlainText(), QStringLiteral("saved class one"));
    notes->setPlainText(QStringLiteral("unsaved class one"));

    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Cancel;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(notes->toPlainText(), QStringLiteral("unsaved class one"));
    QVERIFY(editor->hasUnsavedChanges());

    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Discard;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    editor->sections()->setCurrentIndex(2);
    QCOMPARE(notes->toPlainText(), QStringLiteral("saved class two"));
    QVERIFY(!editor->hasUnsavedChanges());
    QCOMPARE(fixture.state.notesReadPaths.back(), QStringLiteral("workspace-a.tps"));

    notes->setPlainText(QStringLiteral("unsaved class two"));
    fixture.state.identity = QStringLiteral("replacement-workspace");
    fixture.state.path = QStringLiteral("replacement.tps");
    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Cancel;
    QVERIFY(!page->refreshFromStore());
    QCOMPARE(notes->toPlainText(), QStringLiteral("unsaved class two"));
    QVERIFY(editor->hasUnsavedChanges());

    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Discard;
    QVERIFY(page->refreshFromStore());
    QVERIFY(page->selectedClassId().has_value());
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(editor->classId()->value(), std::string("1"));
    QVERIFY(!editor->hasUnsavedChanges());
    QVERIFY(notes->toPlainText().isEmpty());
    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    editor->sections()->setCurrentIndex(2);
    QVERIFY(fixture.state.notesReadPaths.size() >= 3);
    QCOMPARE(fixture.state.notesReadPaths.back(), QStringLiteral("replacement.tps"));
    QCOMPARE(notes->toPlainText(), QStringLiteral("saved class two"));
}

void ClassesNavigatorPageTests::
analyticsLoadsOnlySelectedClassAndUsesOneRankingModel()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    std::vector<std::pair<int, QString>> classRows;
    std::vector<Application::ClassesNavigationEntry> navigationRows;
    for (int id = 1; id <= 96; ++id)
    {
        const QString name = QStringLiteral("Class %1").arg(id);
        classRows.emplace_back(id, name);
        navigationRows.push_back(navigationEntry(
            id, name, QStringLiteral("E4"), QStringLiteral("Theseus"),
            QStringLiteral("Monday"), QStringLiteral("9:00 AM")));
    }
    fixture.state.classes = classList(classRows);
    fixture.state.navigation = navigation(std::move(navigationRows));
    QVERIFY(fixture.activate());
    Ui::ClassesNavigatorPage* const page = fixture.page();
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    QVERIFY(editor);
    QCOMPARE(page->model()->classCount(), 96);
    QVERIFY(editor->sections());
    QCOMPARE(editor->sections()->count(), 4);
    editor->sections()->setCurrentIndex(3);
    const int reusableEditorWidgetCount = widgetCount(editor);
    QCOMPARE(fixture.state.analyticsReadCount, 1);
    QCOMPARE(fixture.state.analyticsClassIds.size(), std::size_t(1));
    QCOMPARE(fixture.state.analyticsClassIds.front().value(), std::string("1"));
    QVERIFY(fixture.state.analyticsRequests.front().useDefaultSelection);
    QCOMPARE(fixture.state.analyticsRequests.front().classId.value(),
        std::string("1"));

    QTableView* const ranking = editor->analyticsRankingTable();
    QComboBox* const evaluation = editor->analyticsEvaluationChoice();
    QVERIFY(ranking && evaluation);
    QCOMPARE(ranking->model()->rowCount(), 25);
    QCOMPARE(evaluation->currentData().toString(), QStringLiteral("Winter"));
    QCOMPARE(fixture.state.analyticsReadCount, 1);

    evaluation->setCurrentIndex(0);
    QCOMPARE(fixture.state.analyticsReadCount, 2);
    QVERIFY(!fixture.state.analyticsRequests.back().useDefaultSelection);
    QVERIFY(!fixture.state.analyticsRequests.back().selectedEvaluationName);
    QCOMPARE(ranking->model()->rowCount(), 25);

    evaluation->setCurrentIndex(3);
    QCOMPARE(fixture.state.analyticsReadCount, 3);
    QVERIFY(!fixture.state.analyticsRequests.back().useDefaultSelection);
    QCOMPARE(fixture.state.analyticsRequests.back().selectedEvaluationName.value(),
        std::u16string(u"Summer"));
    QCOMPARE(evaluation->currentData().toString(), QStringLiteral("Summer"));

    editor->sections()->setCurrentIndex(2);
    QCOMPARE(ranking->model()->rowCount(), 0);
    editor->sections()->setCurrentIndex(3);
    QCOMPARE(fixture.state.analyticsReadCount, 4);
    QVERIFY(!fixture.state.analyticsRequests.back().useDefaultSelection);
    QCOMPARE(fixture.state.analyticsRequests.back().selectedEvaluationName.value(),
        std::u16string(u"Summer"));
    QCOMPARE(ranking->model()->rowCount(), 25);

    page->classList()->setCurrentIndex(page->model()->indexForClassId(classId(2)));
    QCOMPARE(editor->classId()->value(), std::string("2"));
    QCOMPARE(ranking->model()->rowCount(), 0);
    QCOMPARE(widgetCount(editor), reusableEditorWidgetCount);
    editor->sections()->setCurrentIndex(3);
    QCOMPARE(fixture.state.analyticsReadCount, 5);
    QCOMPARE(fixture.state.analyticsClassIds.back().value(), std::string("2"));
    QVERIFY(fixture.state.analyticsRequests.back().useDefaultSelection);
    QCOMPARE(evaluation->currentData().toString(), QStringLiteral("Winter"));
    QCOMPARE(ranking->model()->rowCount(), 25);
}

void ClassesNavigatorPageTests::
analyticsReadFailureRetryEmptyStateAndSelectionRetention()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("Selected")} });
    fixture.state.navigation = navigation({navigationEntry(
        1, QStringLiteral("Selected"), QStringLiteral("E4"),
        QStringLiteral("Theseus"), QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"))});
    fixture.state.failAnalyticsRead = true;
    QVERIFY(fixture.activate());
    Ui::SelectedClassEditor* const editor =
        fixture.page()->selectedClassEditor();
    QVERIFY(editor);
    editor->sections()->setCurrentIndex(3);
    QLabel* const status = editor->analyticsStatusLabel();
    QPushButton* const retry = editor->findChild<QPushButton*>(
        QStringLiteral("classEditorRetryAnalytics"));
    QVERIFY(status && retry);
    QVERIFY(status->text().contains(QStringLiteral("analytics read failed")));
    QVERIFY(!retry->isHidden());
    QCOMPARE(editor->analyticsRankingTable()->model()->rowCount(), 0);

    fixture.state.failAnalyticsRead = false;
    retry->click();
    QVERIFY(retry->isHidden());
    QCOMPARE(editor->analyticsEvaluationChoice()->currentData().toString(),
        QStringLiteral("Winter"));
    QCOMPARE(editor->analyticsRankingTable()->model()->rowCount(), 25);

    fixture.state.analyticsDataAvailable = false;
    editor->analyticsEvaluationChoice()->setCurrentIndex(2);
    QCOMPARE(editor->analyticsRankingTable()->model()->rowCount(), 0);
    const QLabel* const empty = editor->findChild<QLabel*>(
        QStringLiteral("classEditorAnalyticsEmpty"));
    QVERIFY(empty && !empty->isHidden());
    QCOMPARE(editor->analyticsEvaluationChoice()->currentData().toString(),
        QStringLiteral("Speech Contest"));
}

void ClassesNavigatorPageTests::addDeleteAndWorkspaceLifecycleClearSelectedEditor()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"))
    });
    QVERIFY(fixture.activate());
    Ui::ClassesNavigatorPage* const page = fixture.page();
    QVERIFY(page);
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    QVERIFY(editor && editor->isLoaded());

    page->addClassButton()->click();
    QCOMPARE(fixture.state.createCount, 1);
    QCOMPARE(page->model()->classCount(), 2);
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QCOMPARE(editor->classId()->value(), std::string("2"));
    QVERIFY(editor->isLoaded());
    QCOMPARE(editor->gradeChoice()->currentText(), QString{});
    editor->gradeChoice()->setCurrentText(QStringLiteral("E5"));
    QVERIFY(editor->hasUnsavedChanges());

    fixture.state.confirmDelete = false;
    page->deleteClassButton()->click();
    QCOMPARE(fixture.state.deleteCount, 0);
    QCOMPARE(page->model()->classCount(), 2);
    QVERIFY(editor->hasUnsavedChanges());
    fixture.state.confirmDelete = true;
    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Cancel;
    page->deleteClassButton()->click();
    QCOMPARE(fixture.state.deleteCount, 0);
    QCOMPARE(page->model()->classCount(), 2);
    QVERIFY(editor->hasUnsavedChanges());
    fixture.state.unsavedChoice = Ui::UnsavedChangesChoice::Discard;
    page->deleteClassButton()->click();
    QCOMPARE(fixture.state.deleteCount, 1);
    QCOMPARE(page->model()->classCount(), 1);
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(editor->classId()->value(), std::string("1"));

    const int beforeSuspendReadCount = fixture.state.detailsReadCount;
    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QVERIFY(!editor->classId().has_value());
    QVERIFY(fixture.host->activatePage(QStringLiteral("classes")));
    QVERIFY(editor->isLoaded());
    QVERIFY(fixture.state.detailsReadCount > beforeSuspendReadCount);

    fixture.state.path = QStringLiteral("replacement.tps");
    fixture.state.identity = QStringLiteral("replacement-workspace");
    const int beforeReplacementReadCount = fixture.state.detailsReadCount;
    QVERIFY(page->refreshFromStore());
    QVERIFY(editor->isLoaded());
    QVERIFY(fixture.state.detailsReadCount > beforeReplacementReadCount);

    fixture.state.workspaceOpen = false;
    QVERIFY(!page->refreshFromStore());
    QVERIFY(!page->selectedClassId().has_value());
    QVERIFY(!editor->classId().has_value());
    QVERIFY(!fixture.state.sharedSelection.has_value());
}

void ClassesNavigatorPageTests::
languageRefreshChangesLabelsWithoutChangingCanonicalSelection()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{8, QStringLiteral("Hercules")} });
    fixture.state.navigation = navigation({
        navigationEntry(8, QStringLiteral("Hercules"), QStringLiteral("E4"),
            QStringLiteral("Hercules"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));

    KoreanAllTranslator translator;
    qApp->installTranslator(&translator);
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &languageChange);
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("모두"));
    QCOMPARE(page->model()->data(page->model()->index(0, 0)).toString(),
        QStringLiteral("E4 Hercules • M 9:00"));
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));
    qApp->removeTranslator(&translator);
    QEvent englishChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &englishChange);
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("All"));
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));
}

void ClassesNavigatorPageTests::
productionKoreanCatalogTranslatesNavigatorDisplay()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{8, QStringLiteral("Hercules")} });
    fixture.state.navigation = navigation({
        navigationEntry(8, QStringLiteral("Hercules"), QStringLiteral("E4"),
            QStringLiteral("Hercules"), QStringLiteral("Saturday"),
            QStringLiteral("09:00"))
    });
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));

    QTranslator translator;
    const QString translationPath = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)
        + QStringLiteral("/ClassMngr_ko_KR.qm");
    QVERIFY2(translator.load(translationPath), qPrintable(translationPath));
    QVERIFY(qApp->installTranslator(&translator));
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &languageChange);
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("classesNavigatorTitle"))
                 ->text(),
             QStringLiteral("수업"));
    QCOMPARE(page->scheduleSourceCombo()->itemText(0), QStringLiteral("정규 일정"));
    QCOMPARE(page->visibilityScopeCombo()->itemText(0), QStringLiteral("모든 수업"));
    QCOMPARE(page->findChild<QToolButton*>(QStringLiteral(
        "classesNavigatorMondayFilter"))->text(), QStringLiteral("월"));
    QCOMPARE(page->findChild<QToolButton*>(QStringLiteral(
        "classesNavigatorWeekendFilter"))->text(), QStringLiteral("주말"));
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("전체"));
    QCOMPARE(page->model()->data(page->model()->index(0, 0)).toString(),
        QStringLiteral("E4 Hercules • 토 9:00"));
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));
    Ui::SelectedClassEditor* const editor = page->selectedClassEditor();
    QVERIFY(editor);
    editor->sections()->setCurrentIndex(3);
    QCoreApplication::sendEvent(editor, &languageChange);
    QCOMPARE(editor->sections()->tabText(3), QStringLiteral("분석"));
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral(
        "classEditorAnalyticsHeading"))->text(), QStringLiteral("수업 분석"));
    QCOMPARE(editor->findChild<QLabel*>(QStringLiteral(
        "classEditorAnalyticsEvaluationLabel"))->text(), QStringLiteral("평가"));
    QCOMPARE(editor->analyticsEvaluationChoice()->itemText(1), QStringLiteral("겨울"));
    QCOMPARE(editor->analyticsRankingTable()->model()->headerData(
        0, Qt::Horizontal).toString(), QStringLiteral("순위"));
    QWidget* const analyticsSummary = editor->findChild<QWidget*>(QStringLiteral(
        "classEditorAnalyticsSummary"));
    QVERIFY(analyticsSummary && !analyticsSummary->isHidden());
    QVERIFY(qApp->removeTranslator(&translator));

    QEvent englishChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &englishChange);
    QCoreApplication::sendEvent(editor, &englishChange);
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("All"));
    QCOMPARE(editor->sections()->tabText(3), QStringLiteral("Analytics"));
    QCOMPARE(editor->analyticsEvaluationChoice()->currentData().toString(),
        QStringLiteral("Winter"));
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));
}

QTEST_MAIN(ClassesNavigatorPageTests)
#include "classes_navigator_page_tests.moc"
