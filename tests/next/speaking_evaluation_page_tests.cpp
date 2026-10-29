#include "data/database/workspace_store.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/speaking_evaluation_private_notes_split.h"
#include "next/platform/workspace_store_speaking_evaluation_ports.h"
#include "next/platform/workspace_store_speaking_evaluation_report_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/speaking_evaluation_ai_dialogs.h"
#include "next/ui/speaking_evaluation_page.h"
#include "features/speaking_eval/ui/speaking_eval_report_widget.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QListView>
#include <QLineEdit>
#include <QPalette>
#include <QPainter>
#include <QPdfDocument>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QVBoxLayout>
#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

QString evaluationKey(const Domain::ClassId& classId, const QString& evaluation)
{
    return QString::fromStdString(classId.value()) + QLatin1Char('|') + evaluation;
}

Application::ClassesListSnapshot classes96()
{
    Application::ClassesListSnapshot snapshot;
    snapshot.classes.reserve(96);
    for (int id = 1; id <= 96; ++id)
    {
        const std::string decimalId = std::to_string(id);
        std::u16string name = u"Speaking Class ";
        for (const char digit : decimalId)
        {
            name.push_back(static_cast<char16_t>(digit));
        }
        snapshot.classes.push_back({
            .classId = classId(id),
            .className = std::move(name)
        });
    }
    return snapshot;
}

std::vector<std::vector<std::u16string>> emptyEvaluation()
{
    return std::vector<std::vector<std::u16string>>(
        Application::SpeakingEvaluationRowCount,
        std::vector<std::u16string>(Application::SpeakingEvaluationColumnCount));
}

Application::ClassesListReadResult readClassesFromStore(const QString& path)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(path);
    if (!opened)
    {
        const QByteArray diagnostic = opened.error().diagnostic.toUtf8();
        return Application::ClassesListReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::string(diagnostic.constData(),
                static_cast<std::size_t>(diagnostic.size())),
            .recoverable = true
        });
    }
    ClassRepository* const repository = store.classRepository();
    if (!repository)
    {
        return Application::ClassesListReadResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "Class repository unavailable.",
            .recoverable = true
        });
    }
    const auto classes = repository->getBoundedClassesForNavigation(
        static_cast<int>(Application::kClassesNavigationMaximumClasses),
        static_cast<int>(Application::kClassesNavigationMaximumTextLength));
    if (!classes)
    {
        const QByteArray diagnostic = classes.error().diagnostic.toUtf8();
        return Application::ClassesListReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::string(diagnostic.constData(),
                static_cast<std::size_t>(diagnostic.size())),
            .recoverable = true
        });
    }
    Application::ClassesListSnapshot snapshot;
    snapshot.classes.reserve(static_cast<std::size_t>(classes->size()));
    for (const Classroom& classroom : *classes)
    {
        const auto id = Domain::ClassId::fromString(std::to_string(classroom.id));
        if (!id)
        {
            return Application::ClassesListReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "The fixture contains an invalid class identifier.",
                .recoverable = false
            });
        }
        snapshot.classes.push_back({
            .classId = *id,
            .className = classroom.name.toStdU16String()
        });
    }
    return Application::ClassesListReadResult::success(std::move(snapshot));
}

struct FakeEvaluationState final
{
    Application::ClassesListSnapshot classes = classes96();
    std::map<QString, std::vector<std::vector<std::u16string>>> evaluations;
    Application::SpeakingEvaluationRosterNamesReadSnapshot roster{classId(1)};
    std::optional<Domain::ClassId> selectedClass = classId(1);
    QString path = QStringLiteral("fake-profile.tps");
    QString identity = QStringLiteral("fake-workspace");
    Ui::SpeakingEvaluationUnsavedChoice unsavedChoice =
        Ui::SpeakingEvaluationUnsavedChoice::Cancel;
    bool failClassesRead = false;
    bool failEvaluationRead = false;
    bool failSave = false;
    bool failRosterRead = false;
    bool failClassGradeRead = false;
    bool openExternalUrlResult = false;
    QString classGrade = QStringLiteral("E4");
    int readCalls = 0;
    int saveCalls = 0;
    int classGradeReadCalls = 0;
    int externalUrlOpenCalls = 0;
    std::optional<Application::SpeakingEvaluationSaveRequest> lastSaveRequest;
};

class FakeEvaluationReadPort final : public Application::SpeakingEvaluationReadPort
{
public:
    explicit FakeEvaluationReadPort(FakeEvaluationState& state) : m_state(state) {}

    [[nodiscard]] Application::SpeakingEvaluationReadResult readEvaluation(
        const Application::SpeakingEvaluationReadQuery& query
        ) const override
    {
        ++m_state.readCalls;
        if (m_state.failEvaluationRead)
        {
            return Application::SpeakingEvaluationReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "simulated read failure",
                .recoverable = true
            });
        }
        const QString key = evaluationKey(
            query.classId, QString::fromStdU16String(query.evaluationName));
        const auto found = m_state.evaluations.find(key);
        return Application::SpeakingEvaluationReadResult::success({
            .classId = query.classId,
            .evaluationName = query.evaluationName,
            .rows = found == m_state.evaluations.end()
                ? std::vector<std::vector<std::u16string>>{}
                : found->second
        });
    }

private:
    FakeEvaluationState& m_state;
};

class FakeEvaluationSavePort final : public Application::SpeakingEvaluationSavePort
{
public:
    explicit FakeEvaluationSavePort(FakeEvaluationState& state) : m_state(state) {}

    [[nodiscard]] Domain::Result<void> saveEvaluation(
        const Application::SpeakingEvaluationSaveRequest& request
        ) const override
    {
        ++m_state.saveCalls;
        m_state.lastSaveRequest = request;
        if (m_state.failSave)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "simulated save failure",
                .recoverable = true
            });
        }
        const QString key = evaluationKey(
            request.classId, QString::fromStdU16String(request.evaluationName));
        auto [entry, inserted] = m_state.evaluations.try_emplace(key);
        if (inserted || entry->second.empty())
        {
            entry->second = emptyEvaluation();
        }
        for (const auto& change : request.evaluation.changedCells)
        {
            const int row = change.row.rawValue();
            const int column = change.column.rawValue();
            if (row >= 0 && row < Application::SpeakingEvaluationRowCount
                && column >= 0
                && column < Application::SpeakingEvaluationColumnCount)
            {
                entry->second[static_cast<std::size_t>(row)]
                    [static_cast<std::size_t>(column)] =
                    request.evaluation.rows[static_cast<std::size_t>(row)]
                        [static_cast<std::size_t>(column)];
            }
        }
        return Domain::Result<void>::success();
    }

private:
    FakeEvaluationState& m_state;
};

class FakeRosterNamesPort final
    : public Application::SpeakingEvaluationRosterNamesReadPort
{
public:
    explicit FakeRosterNamesPort(FakeEvaluationState& state) : m_state(state) {}

    [[nodiscard]] Application::SpeakingEvaluationRosterNamesReadResult
    readSpeakingEvaluationRosterNames(
        const Application::SpeakingEvaluationRosterNamesReadRequest& request
        ) const override
    {
        if (m_state.failRosterRead)
        {
            return Application::SpeakingEvaluationRosterNamesReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "simulated roster read failure",
                .recoverable = true
            });
        }
        auto snapshot = m_state.roster;
        snapshot.classId = request.classId;
        return Application::SpeakingEvaluationRosterNamesReadResult::success(
            std::move(snapshot));
    }

private:
    FakeEvaluationState& m_state;
};

class FakeClassGradeReadPort final
    : public Application::SelectedClassGradeReadPort
{
public:
    explicit FakeClassGradeReadPort(FakeEvaluationState& state) : m_state(state) {}

    [[nodiscard]] Application::SelectedClassGradeReadResult
    readSelectedClassGrade(const Domain::ClassId& classId) const override
    {
        ++m_state.classGradeReadCalls;
        if (m_state.failClassGradeRead)
        {
            return Application::SelectedClassGradeReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "simulated grade read failure",
                .recoverable = true
            });
        }
        return Application::SelectedClassGradeReadResult::success({
            .classId = classId,
            .classGrade = Application::SelectedClassGradeValue(
                m_state.classGrade.toStdString())
        });
    }

private:
    FakeEvaluationState& m_state;
};

class OtherPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

class PageHarness final
{
public:
    PageHarness()
    {
        m_reportActions.assets.latinFamily = QStringLiteral("Arial");
        m_reportActions.assets.koreanFamily = QStringLiteral("Arial");
    }

    bool initialize(
        FakeEvaluationState& state,
        const bool realWorkspace = false,
        const Application::SaveMode saveMode = Application::SaveMode::Manual)
    {
        const auto catalog = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
        if (!catalog)
        {
            return false;
        }
        m_loader = std::make_unique<Resources::ResourceLoader>(std::move(*catalog));
        m_host = std::make_unique<Ui::PageHost>(*m_loader, m_preferences.model);
        Ui::PageDescriptor descriptor{
            .id = *Ui::PageId::fromString(QStringLiteral("speaking_evaluation")),
            .label = QStringLiteral("Speaking Evaluation"),
            .factory = [this, &state, realWorkspace, saveMode](
                const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::SpeakingEvaluationPageActions actions;
                actions.saveMode = saveMode;
                actions.workspaceIdentity = [&state] { return state.identity; };
                actions.activeWorkspacePath = [&state]() -> std::optional<QString> {
                    return state.path;
                };
                actions.preferredClassSelection = [&state] {
                    return state.selectedClass;
                };
                actions.setActiveClassSelection = [&state](
                    const std::optional<Domain::ClassId>& selected) {
                    state.selectedClass = selected;
                };
                actions.readClasses = [&state, realWorkspace](const QString& path) {
                    if (state.failClassesRead)
                    {
                        return Application::ClassesListReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "simulated class list failure",
                            .recoverable = true
                        });
                    }
                    return realWorkspace ? readClassesFromStore(path)
                        : Application::ClassesListReadResult::success(state.classes);
                };
                actions.evaluationReadPortFactory = [&state, realWorkspace](
                    const QString& path) -> std::unique_ptr<
                        Application::SpeakingEvaluationReadPort> {
                    if (realWorkspace)
                    {
                        return std::make_unique<
                            Platform::WorkspaceStoreSpeakingEvaluationReadPort>(path);
                    }
                    return std::make_unique<FakeEvaluationReadPort>(state);
                };
                actions.evaluationSavePortFactory = [&state, realWorkspace](
                    const QString& path) -> std::unique_ptr<
                        Application::SpeakingEvaluationSavePort> {
                    if (realWorkspace)
                    {
                        return std::make_unique<
                            Platform::WorkspaceStoreSpeakingEvaluationSavePort>(path);
                    }
                    return std::make_unique<FakeEvaluationSavePort>(state);
                };
                actions.rosterNamesReadPortFactory = [&state, realWorkspace](
                    const QString& path) -> std::unique_ptr<
                        Application::SpeakingEvaluationRosterNamesReadPort> {
                    if (realWorkspace)
                    {
                        return std::make_unique<
                            Platform::WorkspaceStoreSpeakingEvaluationRosterNamesReadPort>(
                                path);
                    }
                    return std::make_unique<FakeRosterNamesPort>(state);
                };
                actions.classGradeReadPortFactory = [&state, realWorkspace](
                    const QString& path) -> std::unique_ptr<
                        Application::SelectedClassGradeReadPort> {
                    if (realWorkspace)
                    {
                        return std::make_unique<
                            Platform::WorkspaceStoreSpeakingEvaluationClassGradeReadPort>(
                                path);
                    }
                    return std::make_unique<FakeClassGradeReadPort>(state);
                };
                actions.reportContextReadPortFactory = [](const QString& path) {
                    return std::make_unique<
                        Platform::WorkspaceStoreSpeakingEvaluationReportPort>(path);
                };
                actions.reports = m_reportActions;
                actions.preferredAiCommentProvider = [] {
                    return Application::AiCommentProvider::ChatGPT;
                };
                actions.preferredAiCommentVoice = [] {
                    return Application::AiCommentVoice::DirectToStudent;
                };
                actions.customAiCommentWebsiteUrl = [] { return QString{}; };
                actions.openExternalUrl = [&state](const QUrl&) {
                    ++state.externalUrlOpenCalls;
                    return state.openExternalUrlResult;
                };
                actions.resolveUnsavedChanges = [&state](const QString&) {
                    return state.unsavedChoice;
                };
                actions.confirmQuestionableKoreanNameLengths = [] { return true; };
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::SpeakingEvaluationPage>(
                        context, std::move(actions)));
            }
        };
        if (!m_host->registerPage(std::move(descriptor)))
        {
            return false;
        }
        Ui::PageDescriptor other{
            .id = *Ui::PageId::fromString(QStringLiteral("other")),
            .label = QStringLiteral("Other"),
            .factory = [](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<OtherPage>(context));
            }
        };
        if (!m_host->registerPage(std::move(other)))
        {
            return false;
        }
        return static_cast<bool>(m_host->activatePage(
            QStringLiteral("speaking_evaluation")));
    }

    [[nodiscard]] Ui::SpeakingEvaluationPage* page() const
    {
        return m_host
            ? dynamic_cast<Ui::SpeakingEvaluationPage*>(m_host->pageInstance(
                  QStringLiteral("speaking_evaluation")))
            : nullptr;
    }

    [[nodiscard]] Ui::PageHost& host() const { return *m_host; }
    [[nodiscard]] Ui::SpeakingEvaluationReportActions& reportActions()
    {
        return m_reportActions;
    }
    [[nodiscard]] Resources::ResourceLoader& resourceLoader() const
    {
        return *m_loader;
    }

private:
    Test::MemoryPreferences m_preferences;
    std::unique_ptr<Resources::ResourceLoader> m_loader;
    std::unique_ptr<Ui::PageHost> m_host;
    Ui::SpeakingEvaluationReportActions m_reportActions;
};

bool copyLargeFixture(QTemporaryDir& temporary, QString* destination)
{
    if (!temporary.isValid())
    {
        return false;
    }
    const QString path = temporary.filePath(QStringLiteral("speaking-evaluation.tps"));
    if (!QFile::copy(QStringLiteral(CLASSMNGR_NEXT_SPEAKING_EVALUATION_LARGE_FIXTURE),
            path))
    {
        return false;
    }
    *destination = path;
    return true;
}

bool createReportContextWorkspace(const QString& path, QByteArray* signature)
{
    WorkspaceStore store;
    if (!store.open(path)) return false;
    const auto createdClass = store.classRepository()->createClass(
        QStringLiteral("Speaking Report Class"));
    if (!createdClass) return false;
    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Teacher North");
    teacher.teacherKr = QString::fromUtf8("교사 북");
    teacher.wifiName = QStringLiteral("PRIVATE_WIFI");
    teacher.wifiPassword = QStringLiteral("PRIVATE_WIFI_PASSWORD");
    teacher.zoomId = QStringLiteral("PRIVATE_ZOOM");
    teacher.zoomPassword = QStringLiteral("PRIVATE_ZOOM_PASSWORD");
    const auto teacherId = store.teacherRepository()->createTeacher(teacher);
    if (!teacherId) return false;
    ClassInfo info;
    info.classId = *createdClass;
    info.teacherId = *teacherId;
    info.classGrade = QStringLiteral("E5");
    info.classLevel = QStringLiteral("Athena");
    info.classTimes = {{QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")}};
    if (!store.classInfoRepository()->saveClassInfo(info)) return false;

    *signature = QByteArrayLiteral("report signature bytes");
    return bool(store.settingsRepository()->saveSetting(
        QStringLiteral("myInfo/signatureImage"),
        QString::fromLatin1(signature->toBase64())));
}

std::vector<std::vector<std::u16string>> evaluationWithAiStudents(
    const bool includeSecondStudent = true,
    const QString& secondComment = {}
    )
{
    auto rows = emptyEvaluation();
    rows[0][1] = u"Ada Park";
    rows[0][2] = u"\uAE40\uBBFC\uC9C0";
    rows[0][10] = u"[Did Well]\nClear ideas\n[Needs Improvement]\nPractice past tense";
    if (includeSecondStudent)
    {
        rows[2][1] = u"Bora Lee";
        rows[2][2] = u"\uC774\uC608\uC740";
        rows[2][9] = secondComment.toStdU16String();
        rows[2][10] = u"[Did Well]\nStrong details\n[Needs Improvement]\nAdd a clear ending";
    }
    return rows;
}

QPalette themePalette(const QString& theme)
{
    QPalette palette;
    const bool dark = theme == QStringLiteral("dark");
    palette.setColor(QPalette::Window, dark ? QColor(31, 35, 43) : QColor(247, 248, 250));
    palette.setColor(QPalette::WindowText, dark ? QColor(232, 235, 240) : QColor(31, 41, 55));
    palette.setColor(QPalette::Base, dark ? QColor(24, 28, 35) : QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, dark ? QColor(35, 40, 49) : QColor(243, 245, 248));
    palette.setColor(QPalette::Text, dark ? QColor(232, 235, 240) : QColor(31, 41, 55));
    palette.setColor(QPalette::Button, dark ? QColor(43, 49, 60) : QColor(239, 242, 246));
    palette.setColor(QPalette::ButtonText, dark ? QColor(232, 235, 240) : QColor(31, 41, 55));
    palette.setColor(QPalette::Highlight, dark ? QColor(83, 116, 204) : QColor(50, 96, 180));
    palette.setColor(QPalette::HighlightedText, QColor(Qt::white));
    return palette;
}

QString readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool buttonLabelFits(const QPushButton& button)
{
    QStyleOptionButton option;
    option.initFrom(&button);
    option.text = button.text();
    const QRect labelRect = button.style()->subElementRect(
        QStyle::SE_PushButtonContents, &option, &button);
    return labelRect.width()
        >= button.fontMetrics().horizontalAdvance(button.text());
}

} // namespace

class SpeakingEvaluationPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void matrixIsFixedSizeTracksExactDeltaValidationAndUndo();
    void saveReopenPersistsUnicodeAndOnlyChangedCells();
    void largeNavigationUsesOneMatrixAndReleasesItOnSuspension();
    void selectionSaveDiscardAndFailureRetryPreserveDrafts();
    void saveOnSwitchPersistsBeforeLoadingAndFailureKeepsDraft();
    void automaticModeDebouncesAndSavesOnce();
    void rosterImportNotesAndValidationUseTypedRules();
    void aiPromptRetriesGradeReadAndPreservesConfiguredWebsiteFlow();
    void aiBatchIsBoundedOrderedAndCancelDoesNotEdit();
    void aiBatchResponseRetryMapsCommentsToSourceRows();
    void aiBatchRejectsOneHundredStudentsWithVisibleLimit();
    void reportContextUsesPathScopedWorkspaceStoreAndOmitsCredentials();
    void currentReportSaveAsPdfAndPrintActionsUseOutputEngine();
    void reportsUseUnsavedProjectionCurrentSelectionAndBatchOutputs();
    void batchCheckIndicatorMouseAndKeyboardEventsToggleSelection();
    void batchIncludeIndicatorsRenderCheckedAndUncheckedForAllThemes();
    void koreanLightUndoButtonShowsItsFullLabelAtCaptureSize();
    void workspaceStoreBatchApplySaveReopenPersistsComment();
    void capturesEnglishKoreanLightAndDarkPresentation();
    void capturesSpeakingEvaluationReportsForEnglishKoreanLightAndDark();
};

void SpeakingEvaluationPageTests::
matrixIsFixedSizeTracksExactDeltaValidationAndUndo()
{
    FakeEvaluationState state;
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    auto* const page = harness.page();
    QVERIFY(page);
    QCOMPARE(page->matrixModel()->rowCount(), Application::SpeakingEvaluationRowCount);
    QCOMPARE(page->matrixModel()->columnCount(), Application::SpeakingEvaluationColumnCount);
    QCOMPARE(page->residentMatrixCount(), std::size_t(1));

    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 3),
        QStringLiteral("not-a-score")));
    QVERIFY(!page->matrixModel()->errorsForCell(0, 3).isEmpty());
    QCOMPARE(page->matrixModel()->snapshot().changedCells.size(), std::size_t(1));
    const Application::SpeakingEvaluationCellChange expectedChange{
        Application::SpeakingEvaluationCellChangeRowIndex{0},
        Application::SpeakingEvaluationCellChangeColumnIndex{3}
    };
    QCOMPARE(page->matrixModel()->snapshot().changedCells.front(), expectedChange);

    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 3),
        QStringLiteral("4")));
    QCOMPARE(page->matrixModel()->cellText(0, 3), QStringLiteral("A"));
    QVERIFY(page->matrixModel()->errorsForCell(0, 3).isEmpty());
    QCOMPARE(page->matrixModel()->snapshot().changedCells.size(), std::size_t(1));

    page->findChild<QPushButton*>(QStringLiteral("speakingEvaluationUndo"))->click();
    QVERIFY(page->matrixModel()->isDirty());
    QCOMPARE(page->matrixModel()->cellText(0, 3), QStringLiteral("not-a-score"));
}

void SpeakingEvaluationPageTests::saveReopenPersistsUnicodeAndOnlyChangedCells()
{
    QTemporaryDir temporary;
    QString path;
    QVERIFY(copyLargeFixture(temporary, &path));
    const auto typedId = classId(1);
    auto loadedBefore = Platform::WorkspaceStoreSpeakingEvaluationReadPort(path)
        .readEvaluation({.classId = typedId, .evaluationName = u"Winter"});
    QVERIFY(loadedBefore);
    QCOMPARE(loadedBefore.value().rows.size(), std::size_t(0));

    Application::SpeakingEvaluationSnapshot initial;
    initial.rows = emptyEvaluation();
    initial.rows[0][1] = u"Lena Park";
    initial.rows[0][2] = u"\uAE40\uBBFC\uC9C0";
    initial.rows[0][9] = u"First comment";
    initial.rows[1][1] = u"Nora Lee";
    initial.rows[1][2] = u"\uC774\uC608\uC740";
    initial.rows[1][9] = u"Keep this cell";
    initial.changedCells = {
        {Application::SpeakingEvaluationCellChangeRowIndex{0},
         Application::SpeakingEvaluationCellChangeColumnIndex{1}},
        {Application::SpeakingEvaluationCellChangeRowIndex{0},
         Application::SpeakingEvaluationCellChangeColumnIndex{2}},
        {Application::SpeakingEvaluationCellChangeRowIndex{0},
         Application::SpeakingEvaluationCellChangeColumnIndex{9}},
        {Application::SpeakingEvaluationCellChangeRowIndex{1},
         Application::SpeakingEvaluationCellChangeColumnIndex{1}},
        {Application::SpeakingEvaluationCellChangeRowIndex{1},
         Application::SpeakingEvaluationCellChangeColumnIndex{2}},
        {Application::SpeakingEvaluationCellChangeRowIndex{1},
         Application::SpeakingEvaluationCellChangeColumnIndex{9}}
    };
    Application::SpeakingEvaluationSaveRequest initialRequest{
        .classId = typedId,
        .evaluationName = u"Winter",
        .evaluation = std::move(initial),
        .allowQuestionableKoreanNameLengths = false
    };
    Platform::WorkspaceStoreSpeakingEvaluationSavePort initialSave(path);
    QVERIFY(Application::SpeakingEvaluationSaveUseCase::execute(
        initialRequest, initialSave));

    auto reopened = Platform::WorkspaceStoreSpeakingEvaluationReadPort(path)
        .readEvaluation({.classId = typedId, .evaluationName = u"Winter"});
    QVERIFY(reopened);
    Application::SpeakingEvaluationSaveRequest deltaRequest{
        .classId = typedId,
        .evaluationName = u"Winter",
        .evaluation = {
            .rows = reopened.value().rows,
            .changedCells = {
                {Application::SpeakingEvaluationCellChangeRowIndex{0},
                 Application::SpeakingEvaluationCellChangeColumnIndex{9}}
            }
        },
        .allowQuestionableKoreanNameLengths = false
    };
    deltaRequest.evaluation.rows[0][9] = u"Updated comment";
    QCOMPARE(deltaRequest.evaluation.changedCells.size(), std::size_t(1));
    Platform::WorkspaceStoreSpeakingEvaluationSavePort deltaSave(path);
    QVERIFY(Application::SpeakingEvaluationSaveUseCase::execute(
        deltaRequest, deltaSave));

    const auto persisted = Platform::WorkspaceStoreSpeakingEvaluationReadPort(path)
        .readEvaluation({.classId = typedId, .evaluationName = u"Winter"});
    QVERIFY(persisted);
    QCOMPARE(persisted.value().rows[0][1], std::u16string(u"Lena Park"));
    QCOMPARE(persisted.value().rows[0][2], std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(persisted.value().rows[0][9], std::u16string(u"Updated comment"));
    QCOMPARE(persisted.value().rows[1][9], std::u16string(u"Keep this cell"));
    QCOMPARE(persisted.value().rows.size(), std::size_t(25));
}

void SpeakingEvaluationPageTests::largeNavigationUsesOneMatrixAndReleasesItOnSuspension()
{
    QTemporaryDir temporary;
    QString path;
    QVERIFY(copyLargeFixture(temporary, &path));
    FakeEvaluationState state;
    state.path = path;
    state.identity = QStringLiteral("large-workspace");
    PageHarness harness;
    QVERIFY(harness.initialize(state, true));
    auto* const page = harness.page();
    QVERIFY(page);
    QCOMPARE(page->classModel()->summaryCount(), std::size_t(96));
    QCOMPARE(page->classList()->model()->rowCount(), 96);
    QCOMPARE(page->residentMatrixCount(), std::size_t(1));
    QCOMPARE(page->matrixModel()->rowCount(), 25);
    QCOMPARE(page->matrixModel()->columnCount(), 11);
    QVERIFY(page->selectedClassId());
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QVERIFY(harness.host().activatePage(QStringLiteral("other")));
    QCOMPARE(page->residentMatrixCount(), std::size_t(0));
    QCOMPARE(page->matrixModel()->rowCount(), 0);
    QVERIFY(harness.host().activatePage(QStringLiteral("speaking_evaluation")));
    QCOMPARE(page->residentMatrixCount(), std::size_t(1));
    QCOMPARE(page->classModel()->summaryCount(), std::size_t(96));
    QCOMPARE(page->selectedEvaluationName(), QStringLiteral("Winter"));
}

void SpeakingEvaluationPageTests::selectionSaveDiscardAndFailureRetryPreserveDrafts()
{
    FakeEvaluationState state;
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    auto* const page = harness.page();
    QVERIFY(page);
    QCOMPARE(page->selectedEvaluationName(), QStringLiteral("Winter"));
    QCOMPARE(page->evaluationChoice()->count(), 4);
    QCOMPARE(state.selectedClass->value(), std::string("1"));

    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 1),
        QStringLiteral("Alex Lee")));
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(state.saveCalls, 0);
    page->evaluationChoice()->setCurrentIndex(1);
    QCOMPARE(page->selectedEvaluationName(), QStringLiteral("Winter"));
    QCOMPARE(page->evaluationChoice()->currentIndex(), 0);
    QVERIFY(page->hasUnsavedChanges());

    page->classList()->setCurrentIndex(page->classModel()->index(1, 0));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(page->classList()->currentIndex().row(), 0);
    QVERIFY(page->hasUnsavedChanges());

    state.unsavedChoice = Ui::SpeakingEvaluationUnsavedChoice::Discard;
    page->classList()->setCurrentIndex(page->classModel()->index(1, 0));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QCOMPARE(state.selectedClass->value(), std::string("2"));
    QCOMPARE(page->matrixModel()->cellText(0, 1), QString{});
    page->classList()->setCurrentIndex(page->classModel()->index(0, 0));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(state.selectedClass->value(), std::string("1"));
    page->evaluationChoice()->setCurrentIndex(1);
    QCOMPARE(page->selectedEvaluationName(), QStringLiteral("Speech Contest"));
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(page->matrixModel()->cellText(0, 1), QString{});

    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 1),
        QStringLiteral("Alex Lee")));
    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 2),
        QStringLiteral("\uAE40\uBBFC\uC9C0")));
    state.failSave = true;
    QVERIFY(!page->saveChanges());
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(!page->retryButton()->isHidden());
    QVERIFY(state.lastSaveRequest.has_value());
    QCOMPARE(state.lastSaveRequest->evaluation.changedCells.size(), std::size_t(2));
    QCOMPARE(state.lastSaveRequest->evaluation.changedCells[0],
        (Application::SpeakingEvaluationCellChange{
            Application::SpeakingEvaluationCellChangeRowIndex{0},
            Application::SpeakingEvaluationCellChangeColumnIndex{1}}));
    QCOMPARE(state.lastSaveRequest->evaluation.changedCells[1],
        (Application::SpeakingEvaluationCellChange{
            Application::SpeakingEvaluationCellChangeRowIndex{0},
            Application::SpeakingEvaluationCellChangeColumnIndex{2}}));

    state.failSave = false;
    page->retryButton()->click();
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(state.saveCalls >= 2);
    QVERIFY(state.evaluations.contains(evaluationKey(
        classId(1), QStringLiteral("Speech Contest"))));

    state.failEvaluationRead = true;
    page->evaluationChoice()->setCurrentIndex(2);
    QVERIFY(!page->retryButton()->isHidden());
    QCOMPARE(page->residentMatrixCount(), std::size_t(0));
    const int failedReadCalls = state.readCalls;
    state.failEvaluationRead = false;
    page->retryButton()->click();
    QCOMPARE(state.readCalls, failedReadCalls + 1);
    QCOMPARE(page->residentMatrixCount(), std::size_t(1));
    QCOMPARE(page->selectedEvaluationName(), QStringLiteral("Summer"));
}

void SpeakingEvaluationPageTests::saveOnSwitchPersistsBeforeLoadingAndFailureKeepsDraft()
{
    FakeEvaluationState state;
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    auto* const page = harness.page();
    QVERIFY(page);
    state.unsavedChoice = Ui::SpeakingEvaluationUnsavedChoice::Save;

    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 1),
        QStringLiteral("Eval Save")));
    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 2),
        QString::fromUtf16(u"\uAE40\uBBFC\uC9C0")));
    page->evaluationChoice()->setCurrentIndex(1);
    QCOMPARE(page->selectedEvaluationName(), QStringLiteral("Speech Contest"));
    QVERIFY(!page->hasUnsavedChanges());
    const auto savedWinter = state.evaluations.find(
        evaluationKey(classId(1), QStringLiteral("Winter")));
    QVERIFY(savedWinter != state.evaluations.end());
    QCOMPARE(savedWinter->second[0][1], std::u16string(u"Eval Save"));
    QCOMPARE(savedWinter->second[0][2], std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(page->matrixModel()->cellText(0, 1), QString{});

    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 1),
        QStringLiteral("Class Save")));
    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 2),
        QString::fromUtf16(u"\uC774\uC608\uC740")));
    state.failSave = true;
    page->classList()->setCurrentIndex(page->classModel()->index(1, 0));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(state.selectedClass->value(), std::string("1"));
    QCOMPARE(page->classList()->currentIndex().row(), 0);
    QCOMPARE(page->matrixModel()->cellText(0, 1), QStringLiteral("Class Save"));
    QCOMPARE(page->matrixModel()->cellText(0, 2), QString::fromUtf16(u"\uC774\uC608\uC740"));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(!page->retryButton()->isHidden());
    QVERIFY(state.lastSaveRequest.has_value());
    QCOMPARE(state.lastSaveRequest->evaluationName, std::u16string(u"Speech Contest"));
    QCOMPARE(state.lastSaveRequest->evaluation.changedCells.size(), std::size_t(2));

    state.failSave = false;
    page->retryButton()->click();
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QCOMPARE(state.selectedClass->value(), std::string("1"));
    QCOMPARE(page->matrixModel()->cellText(0, 1), QStringLiteral("Class Save"));
    const auto savedSpeechContest = state.evaluations.find(
        evaluationKey(classId(1), QStringLiteral("Speech Contest")));
    QVERIFY(savedSpeechContest != state.evaluations.end());
    QCOMPARE(savedSpeechContest->second[0][1], std::u16string(u"Class Save"));

    page->classList()->setCurrentIndex(page->classModel()->index(1, 0));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QCOMPARE(state.selectedClass->value(), std::string("2"));
    QCOMPARE(page->matrixModel()->cellText(0, 1), QString{});
    QVERIFY(!page->hasUnsavedChanges());
}

void SpeakingEvaluationPageTests::automaticModeDebouncesAndSavesOnce()
{
    FakeEvaluationState state;
    PageHarness harness;
    QVERIFY(harness.initialize(state, false, Application::SaveMode::Automatic));
    auto* const page = harness.page();
    QVERIFY(page);
    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 1),
        QStringLiteral("Alex Lee")));
    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 2),
        QString::fromUtf16(u"\uAE40\uBBFC\uC9C0")));
    QCOMPARE(state.saveCalls, 0);
    QTRY_COMPARE(state.saveCalls, 1);
    QVERIFY(!page->hasUnsavedChanges());
    const auto saved = state.evaluations.find(
        evaluationKey(classId(1), QStringLiteral("Winter")));
    QVERIFY(saved != state.evaluations.end());
    QCOMPARE(saved->second[0][1], std::u16string(u"Alex Lee"));
    QCOMPARE(saved->second[0][2], std::u16string(u"\uAE40\uBBFC\uC9C0"));
}

void SpeakingEvaluationPageTests::rosterImportNotesAndValidationUseTypedRules()
{
    FakeEvaluationState state;
    state.roster.hasEnglishColumn = true;
    state.roster.hasKoreanColumn = true;
    state.roster.rows = {
        {u"Ada Park", u"\uAE40\uBBFC\uC9C0"},
        {u"Ada Park", u"\uAE40\uBBFC\uC9C0"},
        {u"Bora Lee", u"\uC774\uC608\uC740"}
    };
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    auto* const page = harness.page();
    QVERIFY(page);

    auto* importButton = page->findChild<QPushButton*>(
        QStringLiteral("speakingEvaluationImportNames"));
    QVERIFY(importButton);
    importButton->click();
    QCOMPARE(page->matrixModel()->cellText(0, 1), QStringLiteral("Ada Park"));
    QCOMPARE(page->matrixModel()->cellText(0, 2), QString::fromUtf16(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(page->matrixModel()->cellText(1, 1), QStringLiteral("Bora Lee"));
    QCOMPARE(page->matrixModel()->snapshot().changedCells.size(), std::size_t(4));
    page->findChild<QPushButton*>(QStringLiteral("speakingEvaluationUndo"))->click();
    QCOMPARE(page->matrixModel()->cellText(0, 1), QString{});
    QVERIFY(!page->hasUnsavedChanges());
    state.roster.hasKoreanColumn = false;
    importButton->click();
    QCOMPARE(page->statusLabel()->text(),
        QStringLiteral("Roster must contain 'English' and 'Korean' columns."));
    state.roster.hasKoreanColumn = true;

    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 3),
        QStringLiteral("invalid")));
    QVERIFY(!page->matrixModel()->errorsForCell(0, 3).isEmpty());
    QVERIFY(page->matrixModel()->setData(page->matrixModel()->index(0, 3),
        QStringLiteral("5")));
    QCOMPARE(page->matrixModel()->cellText(0, 3), QStringLiteral("A+"));
    QVERIFY(page->matrixModel()->errorsForCell(0, 3).isEmpty());

    page->tableView()->setCurrentIndex(page->matrixModel()->index(0, 10));
    QTimer::singleShot(0, [] {
        auto* dialog = qobject_cast<QDialog*>(qApp->activeModalWidget());
        if (!dialog)
        {
            return;
        }
        auto* didWell = dialog->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvaluationDidWellNotes"));
        auto* needsWork = dialog->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvaluationNeedsImprovementNotes"));
        if (!didWell || !needsWork)
        {
            dialog->reject();
            return;
        }
        didWell->setPlainText(QStringLiteral("Clear explanations"));
        needsWork->setPlainText(QStringLiteral("Practice past tense"));
        for (QPushButton* button : dialog->findChildren<QPushButton*>())
        {
            if (button->text() == QStringLiteral("Save Changes")
                || button->text() == QStringLiteral("변경 사항 저장"))
            {
                button->click();
                return;
            }
        }
        dialog->reject();
    });
    page->findChild<QPushButton*>(QStringLiteral("speakingEvaluationEditNotes"))->click();
    const auto split = Application::splitSpeakingEvaluationPrivateNotes(
        page->matrixModel()->cellText(0, 10).toStdU16String());
    QCOMPARE(split.didWell, std::u16string(u"Clear explanations"));
    QCOMPARE(split.needsImprovement, std::u16string(u"Practice past tense"));
}

void SpeakingEvaluationPageTests::
aiPromptRetriesGradeReadAndPreservesConfiguredWebsiteFlow()
{
    FakeEvaluationState state;
    state.evaluations[evaluationKey(classId(1), QStringLiteral("Winter"))] =
        evaluationWithAiStudents(false);
    state.failClassGradeRead = true;
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    auto* const page = harness.page();
    QVERIFY(page);
    page->tableView()->setCurrentIndex(page->matrixModel()->index(0, 9));

    page->aiCommentButton()->click();
    QCOMPARE(page->statusLabel()->text(), QStringLiteral("simulated grade read failure"));
    QVERIFY(!page->retryButton()->isHidden());
    QCOMPARE(state.classGradeReadCalls, 1);

    state.failClassGradeRead = false;
    page->retryButton()->click();
    QVERIFY(page->statusLabel()->text().isEmpty());
    QCOMPARE(state.classGradeReadCalls, 2);

    QString prompt;
    QString copyOpenLabel;
    QString clipboard;
    bool promptDialogSeen = false;
    bool websiteFailureClosed = false;
    QTimer::singleShot(0, [&] {
        auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationAiPromptDialog*>(
            qApp->activeModalWidget());
        if (!dialog)
        {
            return;
        }
        promptDialogSeen = true;
        prompt = dialog->promptEditor()->toPlainText();
        copyOpenLabel = dialog->copyAndOpenButton()->text();
        QTimer::singleShot(0, [&] {
            auto* const warning = dynamic_cast<QMessageBox*>(
                qApp->activeModalWidget());
            if (warning && warning->button(QMessageBox::Ok))
            {
                websiteFailureClosed = true;
                warning->button(QMessageBox::Ok)->click();
            }
        });
        dialog->copyAndOpenButton()->click();
        clipboard = QApplication::clipboard()->text();
        dialog->reject();
    });
    page->aiCommentButton()->click();

    QVERIFY(promptDialogSeen);
    QVERIFY(websiteFailureClosed);
    QVERIFY(prompt.contains(QStringLiteral("4th-grade elementary ESL")));
    QVERIFY(prompt.contains(QStringLiteral("STD_NAME")));
    QVERIFY(!prompt.contains(QStringLiteral("Ada Park")));
    QCOMPARE(copyOpenLabel, QStringLiteral("Copy Prompt and Open ChatGPT"));
    QCOMPARE(clipboard, prompt);
    QCOMPARE(state.externalUrlOpenCalls, 1);
    QVERIFY(!page->hasUnsavedChanges());
}

void SpeakingEvaluationPageTests::
aiBatchIsBoundedOrderedAndCancelDoesNotEdit()
{
    FakeEvaluationState state;
    auto rows = emptyEvaluation();
    for (int row = 0; row < Application::SpeakingEvaluationRowCount; ++row)
    {
        rows[static_cast<std::size_t>(row)][1] = u"Student ";
        rows[static_cast<std::size_t>(row)][1].push_back(
            static_cast<char16_t>(u'1' + row / 10));
        rows[static_cast<std::size_t>(row)][1].push_back(
            static_cast<char16_t>(u'0' + row % 10));
        rows[static_cast<std::size_t>(row)][10] =
            u"[Did Well]\nClear ideas\n[Needs Improvement]\nAdd more detail";
    }
    state.evaluations[evaluationKey(classId(1), QStringLiteral("Winter"))] =
        std::move(rows);
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    auto* const page = harness.page();
    QVERIFY(page);

    bool dialogSeen = false;
    bool promptOrdered = false;
    bool checkboxToggleWorks = false;
    int selectedCount = -1;
    int batchSize = -1;
    QTimer::singleShot(0, [&] {
        auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationAiBatchDialog*>(
            qApp->activeModalWidget());
        if (!dialog)
        {
            return;
        }
        dialogSeen = true;
        auto* const selection = dialog->selectionModel();
        batchSize = selection->rowCount();
        auto* const selectionView = dialog->findChild<QTableView*>(
            QStringLiteral("speakingEvaluationAiBatchSelectionTable"));
        if (selectionView)
        {
            QCoreApplication::processEvents();
            const QModelIndex firstInclude = selection->index(
                0, Ui::SpeakingEvaluationAiBatchSelectionModel::IncludeColumn);
            const QRect firstIncludeRect = selectionView->visualRect(firstInclude);
            const QPoint checkboxPoint = firstIncludeRect.center();
            QTest::mouseClick(selectionView->viewport(), Qt::LeftButton,
                Qt::NoModifier, checkboxPoint);
            const bool deselected = !selection->isSelected(0);
            QTest::mouseClick(selectionView->viewport(), Qt::LeftButton,
                Qt::NoModifier, checkboxPoint);
            checkboxToggleWorks = deselected && selection->isSelected(0);
        }
        for (int row = 0; row < selection->rowCount(); ++row)
        {
            if (!selection->isSelected(row))
            {
                return;
            }
            if (selection->data(selection->index(row, 0),
                    Ui::SpeakingEvaluationAiBatchSelectionModel::SourceRowRole)
                .toInt() != row)
            {
                return;
            }
        }
        selectedCount = static_cast<int>(selection->selectedRows().size());
        dialog->createPromptButton()->click();
        const QString prompt = dialog->promptEditor()->toPlainText();
        const qsizetype first = prompt.indexOf(QStringLiteral("Student ID: STUDENT_01"));
        const qsizetype last = prompt.indexOf(QStringLiteral("Student ID: STUDENT_25"));
        promptOrdered = !prompt.isEmpty() && first >= 0 && last > first;
        dialog->reject();
    });
    page->aiBatchCommentsButton()->click();

    QVERIFY(dialogSeen);
    QVERIFY(checkboxToggleWorks);
    QCOMPARE(batchSize, static_cast<int>(
        Application::kSpeakingEvaluationAiBatchMaximumStudents));
    QCOMPARE(selectedCount, static_cast<int>(
        Application::kSpeakingEvaluationAiBatchMaximumStudents));
    QVERIFY(promptOrdered);
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(page->matrixModel()->snapshot().changedCells.size(), std::size_t(0));
}

void SpeakingEvaluationPageTests::
aiBatchResponseRetryMapsCommentsToSourceRows()
{
    FakeEvaluationState state;
    state.evaluations[evaluationKey(classId(1), QStringLiteral("Winter"))] =
        evaluationWithAiStudents(true, QStringLiteral("Existing comment"));
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    auto* const page = harness.page();
    QVERIFY(page);

    bool dialogSeen = false;
    bool malformedWasRejected = false;
    bool overwriteConfirmed = false;
    int acceptedSourceRow = -1;
    QString acceptedComment;
    const QString response = QStringLiteral(
        "<<<STUDENT_02>>>\n"
        "STD_NAME shared clear ideas during the lesson and used strong words with confidence. "
        "Keep practicing past tense verbs so each answer sounds complete. "
        "Your careful effort helps you improve and makes your speaking fun to hear.\n"
        "<<<END_STUDENT_02>>>");

    QTimer::singleShot(0, [&] {
        auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationAiBatchDialog*>(
            qApp->activeModalWidget());
        if (!dialog || dialog->selectionModel()->rowCount() != 2)
        {
            return;
        }
        dialogSeen = true;
        auto* const selection = dialog->selectionModel();
        static_cast<void>(selection->setData(
            selection->index(0, Ui::SpeakingEvaluationAiBatchSelectionModel::IncludeColumn),
            Qt::Unchecked,
            Qt::CheckStateRole));
        static_cast<void>(selection->setData(
            selection->index(1, Ui::SpeakingEvaluationAiBatchSelectionModel::IncludeColumn),
            Qt::Checked,
            Qt::CheckStateRole));
        dialog->createPromptButton()->click();
        if (!dialog->promptEditor()->toPlainText().contains(
                QStringLiteral("Student ID: STUDENT_02")))
        {
            dialog->reject();
            return;
        }

        dialog->responseEditor()->setPlainText(
            QStringLiteral("<<<STUDENT_02>>>\nunfinished response"));
        dialog->parseButton()->click();
        malformedWasRejected = dialog->reviewModel()->rowCount() == 1
            && dialog->reviewModel()->data(
                dialog->reviewModel()->index(0,
                    Ui::SpeakingEvaluationAiBatchReviewModel::StatusColumn))
                .toString() == QStringLiteral("Malformed response block")
            && !dialog->applyButton()->isEnabled();

        dialog->responseEditor()->setPlainText(response);
        dialog->parseButton()->click();
        auto* const review = dialog->reviewModel();
        const QString reviewedComment = review->data(
            review->index(0, Ui::SpeakingEvaluationAiBatchReviewModel::CommentColumn),
            Qt::EditRole).toString();
        if (review->rowCount() != 1
            || !reviewedComment.contains(QStringLiteral("Bora Lee"))
            || reviewedComment.contains(QStringLiteral("STD_NAME")))
        {
            dialog->reject();
            return;
        }
        static_cast<void>(review->setData(
            review->index(0, Ui::SpeakingEvaluationAiBatchReviewModel::ApplyColumn),
            Qt::Checked,
            Qt::CheckStateRole));
        QTimer::singleShot(0, [&] {
            auto* const question = dynamic_cast<QMessageBox*>(
                qApp->activeModalWidget());
            if (question && question->button(QMessageBox::Yes))
            {
                overwriteConfirmed = true;
                question->button(QMessageBox::Yes)->click();
            }
        });
        dialog->applyButton()->click();
        if (!dialog->acceptedComments().empty())
        {
            acceptedSourceRow = dialog->acceptedComments().front().sourceRow;
            acceptedComment = dialog->acceptedComments().front().newComment;
        }
    });
    page->aiBatchCommentsButton()->click();

    QVERIFY(dialogSeen);
    QVERIFY(malformedWasRejected);
    QVERIFY(overwriteConfirmed);
    QCOMPARE(acceptedSourceRow, 2);
    QVERIFY(acceptedComment.contains(QStringLiteral("Bora Lee")));
    QCOMPARE(page->matrixModel()->cellText(2, 9), acceptedComment);
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(page->matrixModel()->snapshot().changedCells.size(), std::size_t(1));
    page->findChild<QPushButton*>(QStringLiteral("speakingEvaluationUndo"))->click();
    QCOMPARE(page->matrixModel()->cellText(2, 9), QStringLiteral("Existing comment"));
    QVERIFY(!page->hasUnsavedChanges());
}

void SpeakingEvaluationPageTests::
aiBatchRejectsOneHundredStudentsWithVisibleLimit()
{
    std::vector<Ui::SpeakingEvaluationAiBatchStudent> students;
    students.reserve(100);
    for (int index = 0; index < 100; ++index)
    {
        const QString id = QStringLiteral("STUDENT_%1")
            .arg(index + 1, 2, 10, QLatin1Char('0'));
        students.push_back({
            .sourceRow = index % Application::SpeakingEvaluationRowCount,
            .id = id,
            .englishName = QStringLiteral("Student %1").arg(index + 1),
            .grade = Domain::CourseGradeBand::E4,
            .eligibilityReason =
                Application::SpeakingEvaluationAiBatchEligibilityReason::Eligible
        });
    }
    Ui::SpeakingEvaluationAiBatchDialog dialog(
        std::move(students),
        Application::AiCommentVoice::DirectToStudent,
        QStringLiteral("ChatGPT"),
        QUrl(QStringLiteral("https://chatgpt.com/")));
    QCOMPARE(dialog.selectionModel()->rowCount(), 0);
    QVERIFY(!dialog.createPromptButton()->isEnabled());
    QVERIFY(dialog.findChild<QLabel*>(
        QStringLiteral("speakingEvaluationAiBatchStatus"))->text()
        .contains(QStringLiteral("25")));
    QVERIFY(dialog.promptEditor()->toPlainText().isEmpty());
}

void SpeakingEvaluationPageTests::
reportContextUsesPathScopedWorkspaceStoreAndOmitsCredentials()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = temporary.filePath(QStringLiteral("reports.tps"));
    QByteArray expectedSignature;
    QVERIFY(createReportContextWorkspace(path, &expectedSignature));

    const auto context =
        Platform::WorkspaceStoreSpeakingEvaluationReportPort(path)
            .readReportContext(classId(1));
    QVERIFY(context);
    QCOMPARE(context.value().classValues.grade, std::u16string(u"E5"));
    QCOMPARE(context.value().classValues.level, std::u16string(u"Athena"));
    QCOMPARE(context.value().classValues.nativeTeacher,
        std::u16string(u"Teacher North"));
    QCOMPARE(context.value().classValues.koreanTeacher,
        std::u16string(u"교사 북"));
    QCOMPARE(context.value().regularTimes.size(), std::size_t(1));
    QCOMPARE(context.value().regularTimes.front().day, std::u16string(u"Monday"));
    QCOMPARE(context.value().regularTimes.front().startTime,
        std::u16string(u"4:00 PM"));
    QCOMPARE(context.value().regularTimes.front().endTime,
        std::u16string(u"4:50 PM"));
    QCOMPARE(context.value().signatureImage,
        std::string(expectedSignature.constData(),
            static_cast<std::size_t>(expectedSignature.size())));

    const QString otherPath = temporary.filePath(QStringLiteral("other.tps"));
    WorkspaceStore emptyStore;
    QVERIFY(emptyStore.open(otherPath));
    const auto otherContext =
        Platform::WorkspaceStoreSpeakingEvaluationReportPort(otherPath)
            .readReportContext(classId(1));
    QVERIFY(!otherContext);
    QCOMPARE(otherContext.error().code, Domain::ErrorCode::NotFound);
}

void SpeakingEvaluationPageTests::
currentReportSaveAsPdfAndPrintActionsUseOutputEngine()
{
    QTemporaryDir workspaceDirectory;
    QVERIFY(workspaceDirectory.isValid());
    const QString workspacePath =
        workspaceDirectory.filePath(QStringLiteral("single-report-actions.tps"));
    QByteArray expectedSignature;
    QVERIFY(createReportContextWorkspace(workspacePath, &expectedSignature));

    FakeEvaluationState state;
    state.path = workspacePath;
    auto rows = emptyEvaluation();
    rows[0][1] = u"Ada Park";
    rows[0][2] = u"김민지";
    rows[0][3] = u"A";
    rows[0][9] = u"A clear, synthetic comment.";
    state.evaluations[evaluationKey(classId(1), QStringLiteral("Winter"))] = rows;

    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());
    const QString pdfPath = outputDirectory.filePath(
        QStringLiteral("selected-speaking-report.pdf"));
    QStringList progressNames;
    QStringList stagingDirectories;
    QStringList noticeTitles;
    QStringList noticeMessages;
    QList<bool> noticeFailures;
    QString suggestedFileName;
    int pathChooserCalls = 0;
    int printCalls = 0;
    bool stagedPdfWasAvailableToPrinter = false;

    PageHarness harness;
    auto& reportActions = harness.reportActions();
    reportActions.engineOverrides.progressCallback = [&progressNames](
        int, int, const QString& name) {
        if (!name.isEmpty()) progressNames.append(name);
        return true;
    };
    reportActions.engineOverrides.stagingDirectoryCreated =
        [&stagingDirectories](const QString& path) {
            stagingDirectories.append(path);
        };
    reportActions.engineOverrides.printCallback = [&] {
        ++printCalls;
        if (!stagingDirectories.isEmpty())
        {
            const QStringList stagedPdfs = QDir(stagingDirectories.constLast())
                .entryList({QStringLiteral("*.pdf")}, QDir::Files);
            if (stagedPdfs.size() == 1)
            {
                QPdfDocument stagedDocument;
                stagedPdfWasAvailableToPrinter = stagedDocument.load(
                    QDir(stagingDirectories.constLast()).filePath(
                        stagedPdfs.constFirst())) == QPdfDocument::Error::None
                    && stagedDocument.pageCount() == 1;
            }
        }
        return DocumentOutputResult{
            DocumentOutputStatus::Completed,
            QStringLiteral("Synthetic print job accepted."),
            {},
            {}
        };
    };
    reportActions.choosePdfPath = [&] (QWidget*, const QString& suggested) {
        ++pathChooserCalls;
        suggestedFileName = suggested;
        return pdfPath;
    };
    reportActions.showNotice = [&] (QWidget*, bool failed,
        const QString& title, const QString& message) {
        noticeFailures.append(failed);
        noticeTitles.append(title);
        noticeMessages.append(message);
    };

    QVERIFY(harness.initialize(state));
    Ui::SpeakingEvaluationPage* const page = harness.page();
    QVERIFY(page);
    page->tableView()->setCurrentIndex(page->matrixModel()->index(0, 1));

    const auto activatePreviewAction = [page](const QString& objectName) {
        bool dialogSeen = false;
        QTimer::singleShot(0, [objectName, &dialogSeen] {
            auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationReportsDialog*>(
                qApp->activeModalWidget());
            if (!dialog) return;
            auto* const action = dialog->findChild<QPushButton*>(objectName);
            if (!action)
            {
                dialog->reject();
                return;
            }
            dialogSeen = true;
            action->click();
            dialog->reject();
        });
        page->reportsButton()->click();
        return dialogSeen;
    };

    QVERIFY(activatePreviewAction(QStringLiteral("nextReportSavePdf")));
    QCOMPARE(pathChooserCalls, 1);
    QVERIFY(!suggestedFileName.isEmpty());
    QCOMPARE(progressNames,
        QStringList{QStringLiteral("Ada Park (김민지)")});
    QVERIFY(QFileInfo::exists(pdfPath));
    QPdfDocument savedDocument;
    QCOMPARE(savedDocument.load(pdfPath), QPdfDocument::Error::None);
    QCOMPARE(savedDocument.pageCount(), 1);
    QCOMPARE(noticeTitles.constLast(), QStringLiteral("Reports Ready"));
    QCOMPARE(noticeFailures.constLast(), false);
    QVERIFY(noticeMessages.constLast().contains(pdfPath));
    QVERIFY(!stagingDirectories.isEmpty());
    QVERIFY(!QFileInfo::exists(stagingDirectories.constLast()));

    QVERIFY(activatePreviewAction(QStringLiteral("nextReportPrint")));
    QCOMPARE(pathChooserCalls, 1);
    QCOMPARE(printCalls, 1);
    QVERIFY(stagedPdfWasAvailableToPrinter);
    QCOMPARE(progressNames,
        (QStringList{QStringLiteral("Ada Park (김민지)"),
            QStringLiteral("Ada Park (김민지)")}));
    QCOMPARE(noticeTitles.constLast(), QStringLiteral("Reports Ready"));
    QCOMPARE(noticeFailures.constLast(), false);
    QCOMPARE(QDir(outputDirectory.path()).entryList(
        QDir::Files | QDir::NoDotAndDotDot),
        QStringList{QStringLiteral("selected-speaking-report.pdf")});
    QVERIFY(!QFileInfo::exists(stagingDirectories.constLast()));
    QCOMPARE(speakingEvalReportAssetResidency().sessions, 0);
    QCOMPARE(speakingEvalReportAssetResidency().templates, 0);
    QCOMPARE(speakingEvalReportAssetResidency().decodedImageBytes, qsizetype(0));
}

void SpeakingEvaluationPageTests::
reportsUseUnsavedProjectionCurrentSelectionAndBatchOutputs()
{
    QTemporaryDir workspaceDirectory;
    QVERIFY(workspaceDirectory.isValid());
    const QString workspacePath =
        workspaceDirectory.filePath(QStringLiteral("reports.tps"));
    QByteArray expectedSignature;
    QVERIFY(createReportContextWorkspace(workspacePath, &expectedSignature));

    FakeEvaluationState state;
    state.path = workspacePath;
    state.identity = QStringLiteral("report-output-workspace");
    auto rows = emptyEvaluation();
    rows[0][1] = u"Ada Park";
    rows[0][2] = u"김민지";
    rows[0][3] = u"A";
    rows[0][4] = u"B";
    rows[0][5] = u"A";
    rows[0][6] = u"B";
    rows[0][7] = u"A";
    rows[0][8] = u"B";
    rows[0][9] = u"Ada's existing comment";
    rows[0][10] = u"[Did Well]\nClear ideas\n[Needs Improvement]\nAdd more detail";
    rows[2][1] = u"Bora Lee";
    rows[2][2] = u"이예은";
    rows[2][3] = u"B";
    rows[2][4] = u"A";
    rows[2][5] = u"B";
    rows[2][6] = u"A";
    rows[2][7] = u"B";
    rows[2][8] = u"A";
    rows[2][9] = u"Bora's unsaved comment";
    rows[2][10] = u"[Did Well]\nGood details\n[Needs Improvement]\nAdd transitions";
    state.evaluations[evaluationKey(classId(1), QStringLiteral("Winter"))] = rows;

    QTemporaryDir outputRoot;
    QVERIFY(outputRoot.isValid());
    QStringList renderedNames;
    QStringList stagingDirectories;
    int overwritePromptCount = 0;
    PageHarness harness;
    auto& reportActions = harness.reportActions();
    reportActions.engineOverrides.internalRenderer = [&renderedNames](
        const SpeakingEvalReportData& report, const QString& path,
        QString* error)
    {
        renderedNames.append(report.englishName);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)
            || file.write("%PDF-next-report") != 16)
        {
            if (error) *error = QStringLiteral("test report write failed");
            return false;
        }
        return true;
    };
    reportActions.engineOverrides.stagingDirectoryCreated =
        [&stagingDirectories](const QString& path) {
            stagingDirectories.append(path);
        };
    reportActions.confirmOverwrite = [&overwritePromptCount](QWidget*, int count) {
        overwritePromptCount = count;
        return false;
    };
    reportActions.showNotice = [](QWidget*, bool, const QString&, const QString&) {};

    QVERIFY(harness.initialize(state));
    Ui::SpeakingEvaluationPage* const page = harness.page();
    QVERIFY(page);
    QVERIFY(page->reportsButton()->isEnabled());
    page->tableView()->setCurrentIndex(page->matrixModel()->index(2, 1));

    bool previewSeen = false;
    int previewReportCount = 0;
    int selectedReportIndex = -1;
    QString previewComment;
    QTimer::singleShot(0, [&] {
        auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationReportsDialog*>(
            qApp->activeModalWidget());
        if (!dialog) return;
        previewSeen = true;
        previewReportCount = dialog->reports().size();
        selectedReportIndex = dialog->studentSelector()->currentIndex();
        const auto& reports = dialog->reports();
        if (reports.size() != 2
            || reports.at(0).report.classLabel != QStringLiteral("E5 Athena")
            || reports.at(0).report.nativeTeacher != QStringLiteral("Teacher North")
            || reports.at(0).report.koreanTeacher != QStringLiteral("교사 북")
            || reports.at(0).report.signatureImage != expectedSignature
            || reports.at(2 - 1).sourceRow != 2
            || reports.at(1).report.comments != QStringLiteral("Bora's unsaved comment"))
        {
            dialog->reject();
            return;
        }
        auto* const commentEditor = dialog->reportWidget()->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalReportComments"));
        if (!commentEditor)
        {
            dialog->reject();
            return;
        }
        previewComment = QStringLiteral("Edited in the report preview");
        commentEditor->setPlainText(previewComment);
        dialog->reject();
    });
    page->reportsButton()->click();
    QVERIFY(previewSeen);
    QCOMPARE(previewReportCount, 2);
    QCOMPARE(selectedReportIndex, 1);
    QCOMPARE(page->matrixModel()->cellText(2, 9), previewComment);
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(speakingEvalReportAssetResidency().sessions, 0);
    QCOMPARE(speakingEvalReportAssetResidency().templates, 0);
    QCOMPARE(speakingEvalReportAssetResidency().decodedImageBytes, qsizetype(0));

    const QString currentDirectory = outputRoot.filePath(QStringLiteral("current"));
    bool currentBatchSeen = false;
    DocumentOutputResult currentResult;
    QTimer::singleShot(0, [&] {
        auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationReportBatchDialog*>(
            qApp->activeModalWidget());
        if (!dialog) return;
        currentBatchSeen = true;
        dialog->scopeSelector()->setCurrentIndex(0);
        dialog->outputDirectoryEdit()->setText(currentDirectory);
        dialog->exportButton()->click();
        currentResult = dialog->lastResult().output;
    });
    page->saveReportsButton()->click();
    QVERIFY(currentBatchSeen);
    QCOMPARE(currentResult.status, DocumentOutputStatus::Completed);
    QCOMPARE(currentResult.savedPdfPaths.size(), 1);
    QVERIFY(currentResult.savedArchivePath.isEmpty());
    QVERIFY(QFileInfo::exists(currentResult.savedPdfPaths.constFirst()));
    QCOMPARE(renderedNames, QStringList{QStringLiteral("Bora Lee")});
    QVERIFY(!QFileInfo::exists(stagingDirectories.constLast()));

    const QString allDirectory = outputRoot.filePath(QStringLiteral("all"));
    bool allBatchSeen = false;
    DocumentOutputResult allResult;
    QTimer::singleShot(0, [&] {
        auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationReportBatchDialog*>(
            qApp->activeModalWidget());
        if (!dialog) return;
        allBatchSeen = true;
        dialog->scopeSelector()->setCurrentIndex(1);
        dialog->keepIndividualCheck()->setChecked(true);
        dialog->outputDirectoryEdit()->setText(allDirectory);
        dialog->exportButton()->click();
        allResult = dialog->lastResult().output;
    });
    page->saveReportsButton()->click();
    QVERIFY(allBatchSeen);
    QCOMPARE(allResult.status, DocumentOutputStatus::Completed);
    QCOMPARE(allResult.savedPdfPaths.size(), 2);
    QVERIFY(!allResult.savedArchivePath.isEmpty());
    QVERIFY(QFileInfo::exists(allResult.savedArchivePath));
    for (const QString& path : allResult.savedPdfPaths)
        QVERIFY(QFileInfo::exists(path));
    QCOMPARE(renderedNames,
        (QStringList{QStringLiteral("Bora Lee"), QStringLiteral("Ada Park"),
            QStringLiteral("Bora Lee")}));
    QVERIFY(!QFileInfo::exists(stagingDirectories.constLast()));

    const int rendererCallsBeforeOverwrite = renderedNames.size();
    bool overwriteBatchSeen = false;
    QTimer::singleShot(0, [&] {
        auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationReportBatchDialog*>(
            qApp->activeModalWidget());
        if (!dialog) return;
        overwriteBatchSeen = true;
        dialog->scopeSelector()->setCurrentIndex(0);
        dialog->outputDirectoryEdit()->setText(currentDirectory);
        dialog->exportButton()->click();
        dialog->reject();
    });
    page->saveReportsButton()->click();
    QVERIFY(overwriteBatchSeen);
    QCOMPARE(overwritePromptCount, 1);
    QCOMPARE(renderedNames.size(), rendererCallsBeforeOverwrite);
    QVERIFY(!QFileInfo::exists(stagingDirectories.constLast()));
}

void SpeakingEvaluationPageTests::
batchCheckIndicatorMouseAndKeyboardEventsToggleSelection()
{
    std::vector<Ui::SpeakingEvaluationAiBatchStudent> students{
        {
            .sourceRow = 0,
            .id = QStringLiteral("STUDENT_01"),
            .englishName = QStringLiteral("Jin Park"),
            .koreanName = QStringLiteral("김민지"),
            .didWell = QStringLiteral("Clear ideas"),
            .needsImprovement = QStringLiteral("Add more detail"),
            .grade = Domain::CourseGradeBand::E4,
            .eligibilityReason = Application::
                SpeakingEvaluationAiBatchEligibilityReason::Eligible
        },
        {
            .sourceRow = 1,
            .id = QStringLiteral("STUDENT_02"),
            .englishName = QStringLiteral("Noah Sample"),
            .oldComment = QStringLiteral("Existing synthetic comment"),
            .grade = Domain::CourseGradeBand::E4,
            .eligibilityReason = Application::
                SpeakingEvaluationAiBatchEligibilityReason::Eligible
        }
    };
    Ui::SpeakingEvaluationAiBatchDialog dialog(
        std::move(students),
        Application::AiCommentVoice::DirectToStudent,
        QStringLiteral("ChatGPT"),
        QUrl(QStringLiteral("https://chatgpt.com/")));
    auto* const view = dialog.findChild<QTableView*>(
        QStringLiteral("speakingEvaluationAiBatchSelectionTable"));
    QVERIFY(view);
    dialog.show();
    QCoreApplication::processEvents();

    auto* const model = dialog.selectionModel();
    const QModelIndex firstInclude = model->index(
        0, Ui::SpeakingEvaluationAiBatchSelectionModel::IncludeColumn);
    const QRect visibleIndicatorCell = view->visualRect(firstInclude);
    QVERIFY(visibleIndicatorCell.isValid());
    QVERIFY(model->isSelected(0));

    const QPoint visibleIndicatorCenter = visibleIndicatorCell.center();
    QTest::mouseClick(view->viewport(), Qt::LeftButton,
        Qt::NoModifier, visibleIndicatorCenter);
    QVERIFY(!model->isSelected(0));
    QTest::mouseClick(view->viewport(), Qt::LeftButton,
        Qt::NoModifier, visibleIndicatorCenter);
    QVERIFY(model->isSelected(0));

    view->setCurrentIndex(firstInclude);
    view->setFocus();
    QTest::keyClick(view, Qt::Key_Space);
    QVERIFY(!model->isSelected(0));
    QTest::keyClick(view, Qt::Key_Space);
    QVERIFY(model->isSelected(0));

    dialog.createPromptButton()->click();
    dialog.responseEditor()->setPlainText(QStringLiteral(
        "<<<STUDENT_01>>>\n"
        "STD_NAME explains ideas and uses details. "
        "STD_NAME should add transitions. "
        "STD_NAME can practice more.\n"
        "<<<END_STUDENT_01>>>"));
    dialog.parseButton()->click();
    QCOMPARE(dialog.reviewModel()->rowCount(), 1);
    const QModelIndex reviewApplyIndex = dialog.reviewModel()->index(
        0, Ui::SpeakingEvaluationAiBatchReviewModel::ApplyColumn);
    const QModelIndex reviewStatusIndex = dialog.reviewModel()->index(
        0, Ui::SpeakingEvaluationAiBatchReviewModel::StatusColumn);
    QVERIFY2(dialog.reviewModel()->data(
        reviewApplyIndex,
        Ui::SpeakingEvaluationAiBatchReviewModel::ValidRole).toBool(),
        qPrintable(dialog.reviewModel()->data(reviewStatusIndex).toString()));
    auto* const tabs = dialog.findChild<QTabWidget*>(
        QStringLiteral("speakingEvaluationAiBatchTabs"));
    QVERIFY(tabs);
    tabs->setCurrentIndex(1);
    QCoreApplication::processEvents();
    auto* const reviewView = dialog.findChild<QTableView*>(
        QStringLiteral("speakingEvaluationAiBatchReviewTable"));
    QVERIFY(reviewView);
    auto* const reviewModel = dialog.reviewModel();
    const QModelIndex applyIndex = reviewModel->index(
        0, Ui::SpeakingEvaluationAiBatchReviewModel::ApplyColumn);
    QCOMPARE(reviewModel->data(applyIndex, Qt::CheckStateRole).toInt(),
        static_cast<int>(Qt::Unchecked));
    const QRect applyCell = reviewView->visualRect(applyIndex);
    QVERIFY(applyCell.isValid());
    const QPoint applyIndicatorCenter = applyCell.center();
    QTest::mouseClick(reviewView->viewport(), Qt::LeftButton,
        Qt::NoModifier, applyIndicatorCenter);
    QCOMPARE(reviewModel->data(applyIndex, Qt::CheckStateRole).toInt(),
        static_cast<int>(Qt::Checked));
    QTest::mouseClick(reviewView->viewport(), Qt::LeftButton,
        Qt::NoModifier, applyIndicatorCenter);
    QCOMPARE(reviewModel->data(applyIndex, Qt::CheckStateRole).toInt(),
        static_cast<int>(Qt::Unchecked));
    reviewView->setCurrentIndex(applyIndex);
    reviewView->setFocus();
    QTest::keyClick(reviewView, Qt::Key_Space);
    QCOMPARE(reviewModel->data(applyIndex, Qt::CheckStateRole).toInt(),
        static_cast<int>(Qt::Checked));
    QTest::keyClick(reviewView, Qt::Key_Space);
    QCOMPARE(reviewModel->data(applyIndex, Qt::CheckStateRole).toInt(),
        static_cast<int>(Qt::Unchecked));
}

void SpeakingEvaluationPageTests::
batchIncludeIndicatorsRenderCheckedAndUncheckedForAllThemes()
{
    struct RestoreAppearance final
    {
        QPalette palette;
        QString styleSheet;
        QLocale locale;
        QTranslator* translator = nullptr;
        bool translatorInstalled = false;

        ~RestoreAppearance()
        {
            if (translatorInstalled && translator)
            {
                qApp->removeTranslator(translator);
            }
            QLocale::setDefault(locale);
            qApp->setPalette(palette);
            qApp->setStyleSheet(styleSheet);
        }
    } restore{
        qApp->palette(), qApp->styleSheet(), QLocale(), nullptr, false};

    QTranslator translator;
    QVERIFY(translator.load(QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)).filePath(
            QStringLiteral("ClassMngr_ko_KR.qm"))));
    const QString styleDirectory = QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)).filePath(QStringLiteral("assets/styles"));

    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        if (locale == QStringLiteral("ko_KR"))
        {
            restore.translator = &translator;
            restore.translatorInstalled = qApp->installTranslator(&translator);
            QVERIFY(restore.translatorInstalled);
        }
        QLocale::setDefault(QLocale(locale));
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            qApp->setPalette(themePalette(theme));
            qApp->setStyleSheet(readFile(QDir(styleDirectory).filePath(
                theme + QStringLiteral(".qss"))));
            QCoreApplication::processEvents();

            std::vector<Ui::SpeakingEvaluationAiBatchStudent> students{
                {
                    .sourceRow = 0,
                    .id = QStringLiteral("STUDENT_01"),
                    .englishName = QStringLiteral("Mina Example"),
                    .koreanName = QStringLiteral("민아 예시"),
                    .grade = Domain::CourseGradeBand::E4,
                    .eligibilityReason = Application::
                        SpeakingEvaluationAiBatchEligibilityReason::Eligible
                },
                {
                    .sourceRow = 1,
                    .id = QStringLiteral("STUDENT_02"),
                    .englishName = QStringLiteral("Noah Sample"),
                    .koreanName = QStringLiteral("노아 예시"),
                    .oldComment = QStringLiteral("Existing synthetic comment"),
                    .grade = Domain::CourseGradeBand::E4,
                    .eligibilityReason = Application::
                        SpeakingEvaluationAiBatchEligibilityReason::Eligible
                }
            };
            Ui::SpeakingEvaluationAiBatchDialog dialog(
                std::move(students),
                Application::AiCommentVoice::DirectToStudent,
                QStringLiteral("ChatGPT"),
                QUrl(QStringLiteral("https://chatgpt.com/")));
            dialog.resize(1080, 760);
            dialog.show();
            QCoreApplication::processEvents();

            auto* const view = dialog.findChild<QTableView*>(
                QStringLiteral("speakingEvaluationAiBatchSelectionTable"));
            QVERIFY(view);
            const auto* const model = dialog.selectionModel();
            QVERIFY(model->isSelected(0));
            QVERIFY(!model->isSelected(1));
            const QRect checkedCell = view->visualRect(model->index(
                0, Ui::SpeakingEvaluationAiBatchSelectionModel::IncludeColumn));
            const QRect uncheckedCell = view->visualRect(model->index(
                1, Ui::SpeakingEvaluationAiBatchSelectionModel::IncludeColumn));
            QVERIFY(checkedCell.isValid());
            QVERIFY(uncheckedCell.isValid());

            const QImage viewport = view->viewport()->grab().toImage();
            const QColor checkedCenter = viewport.pixelColor(checkedCell.center());
            const QColor uncheckedCenter = viewport.pixelColor(uncheckedCell.center());
            QVERIFY2(checkedCenter != uncheckedCenter,
                qPrintable(QStringLiteral("Checked indicator is not distinct in %1/%2")
                    .arg(locale, theme)));

            const QRect checkBox(
                checkedCell.center().x() - 10,
                checkedCell.center().y() - 10,
                20,
                20);
            const QColor accent = view->palette().color(QPalette::Highlight);
            int accentPixelsOutsideVisibleIndicator = 0;
            for (int y = checkedCell.top(); y <= checkedCell.bottom(); ++y)
            {
                for (int x = checkedCell.left(); x <= checkedCell.right(); ++x)
                {
                    if (!checkBox.contains(x, y)
                        && viewport.pixelColor(x, y) == accent)
                    {
                        ++accentPixelsOutsideVisibleIndicator;
                    }
                }
            }
            QVERIFY2(accentPixelsOutsideVisibleIndicator == 0,
                qPrintable(QStringLiteral(
                    "An extra checked indicator is rendered outside the centered indicator in %1/%2")
                    .arg(locale, theme)));

            int brightCheckPixels = 0;
            for (int y = checkBox.top(); y <= checkBox.bottom(); ++y)
            {
                for (int x = checkBox.left(); x <= checkBox.right(); ++x)
                {
                    const QColor pixel = viewport.pixelColor(x, y);
                    if (pixel.red() > 220 && pixel.green() > 220
                        && pixel.blue() > 220)
                    {
                        ++brightCheckPixels;
                    }
                }
            }
            QVERIFY2(brightCheckPixels > 0,
                qPrintable(QStringLiteral("Checked indicator has no visible tick in %1/%2")
                    .arg(locale, theme)));
        }
    }
}

void SpeakingEvaluationPageTests::
koreanLightUndoButtonShowsItsFullLabelAtCaptureSize()
{
    struct RestoreAppearance final
    {
        QPalette palette;
        QString styleSheet;
        QLocale locale;
        QTranslator* translator = nullptr;
        bool translatorInstalled = false;

        ~RestoreAppearance()
        {
            if (translatorInstalled && translator)
            {
                qApp->removeTranslator(translator);
            }
            QLocale::setDefault(locale);
            qApp->setPalette(palette);
            qApp->setStyleSheet(styleSheet);
        }
    } restore{
        qApp->palette(), qApp->styleSheet(), QLocale(), nullptr, false};

    FakeEvaluationState state;
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    Ui::SpeakingEvaluationPage* const page = harness.page();
    QVERIFY(page);

    QTranslator translator;
    QVERIFY(translator.load(QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)).filePath(
            QStringLiteral("ClassMngr_ko_KR.qm"))));
    restore.translator = &translator;
    restore.translatorInstalled = qApp->installTranslator(&translator);
    QVERIFY(restore.translatorInstalled);
    QLocale::setDefault(QLocale(QStringLiteral("ko_KR")));
    qApp->setPalette(themePalette(QStringLiteral("light")));
    const QString styleDirectory = QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)).filePath(QStringLiteral("assets/styles"));
    qApp->setStyleSheet(readFile(QDir(styleDirectory).filePath(
        QStringLiteral("light.qss"))));

    Ui::PageHost& host = harness.host();
    host.resize(1440, 880);
    host.show();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();

    auto* const undo = page->findChild<QPushButton*>(
        QStringLiteral("speakingEvaluationUndo"));
    QVERIFY(undo);
    QCOMPARE(undo->text(), QString::fromUtf8("\xEC\x8B\xA4\xED\x96\x89 \xEC\xB7\xA8\xEC\x86\x8C"));
    QVERIFY2(buttonLabelFits(*undo), qPrintable(QStringLiteral(
        "Undo label is clipped: button width %1, label width %2")
        .arg(undo->width())
        .arg(undo->fontMetrics().horizontalAdvance(undo->text()))));
}

void SpeakingEvaluationPageTests::
workspaceStoreBatchApplySaveReopenPersistsComment()
{
    QTemporaryDir temporary;
    QString path;
    QVERIFY(copyLargeFixture(temporary, &path));

    FakeEvaluationState state;
    state.path = path;
    state.identity = QStringLiteral("c39-batch-persistence-workspace");
    const QString expectedComment = QStringLiteral(
        "Synthetic Student communicates ideas clearly and supports answers with useful details. "
        "Synthetic Student should add transitions so each point connects smoothly. "
        "Synthetic Student can keep practicing these strategies to make presentations even stronger.");
    const QString response = QStringLiteral(
        "<<<STUDENT_01>>>\n"
        "STD_NAME communicates ideas clearly and supports answers with useful details. "
        "STD_NAME should add transitions so each point connects smoothly. "
        "STD_NAME can keep practicing these strategies to make presentations even stronger.\n"
        "<<<END_STUDENT_01>>>");

    {
        PageHarness harness;
        QVERIFY(harness.initialize(state, true));
        Ui::SpeakingEvaluationPage* const page = harness.page();
        QVERIFY(page);
        QVERIFY(page->selectedClassId().has_value());
        QVERIFY(*page->selectedClassId() == classId(1));
        QCOMPARE(page->selectedEvaluationName(), QStringLiteral("Winter"));

        auto* const model = page->matrixModel();
        const auto setCellIfChanged = [model](
            const int row, const int column, const QString& value) {
            return model->cellText(row, column) == value
                || model->setData(model->index(row, column), value);
        };
        for (int row = 0; row < Application::SpeakingEvaluationRowCount; ++row)
        {
            const QString englishName = row == 0
                ? QStringLiteral("Synthetic Student") : QString{};
            const QString koreanName = row == 0
                ? QStringLiteral("민아예시") : QString{};
            const QString notes = row == 0
                ? QStringLiteral("[Did Well]\nClear ideas\n"
                                 "[Needs Improvement]\nAdd more detail")
                : QString{};
            QVERIFY(setCellIfChanged(row, 1, englishName));
            QVERIFY(setCellIfChanged(row, 2, koreanName));
            QVERIFY(setCellIfChanged(row, 9, QString{}));
            QVERIFY(setCellIfChanged(row, 10, notes));
        }

        bool dialogSeen = false;
        bool applied = false;
        QTimer::singleShot(0, [&] {
            auto* const dialog = dynamic_cast<Ui::SpeakingEvaluationAiBatchDialog*>(
                qApp->activeModalWidget());
            if (!dialog)
            {
                return;
            }
            dialogSeen = true;
            if (dialog->selectionModel()->rowCount() != 1
                || !dialog->selectionModel()->isSelected(0))
            {
                dialog->reject();
                return;
            }
            dialog->createPromptButton()->click();
            if (!dialog->promptEditor()->toPlainText().contains(
                    QStringLiteral("Student ID: STUDENT_01")))
            {
                dialog->reject();
                return;
            }
            dialog->responseEditor()->setPlainText(response);
            dialog->parseButton()->click();
            auto* const review = dialog->reviewModel();
            if (review->rowCount() != 1
                || !review->data(review->index(0, 0),
                    Ui::SpeakingEvaluationAiBatchReviewModel::ValidRole).toBool())
            {
                dialog->reject();
                return;
            }
            static_cast<void>(review->setData(
                review->index(0,
                    Ui::SpeakingEvaluationAiBatchReviewModel::ApplyColumn),
                Qt::Checked,
                Qt::CheckStateRole));
            dialog->applyButton()->click();
            applied = !dialog->acceptedComments().empty()
                && dialog->acceptedComments().front().sourceRow == 0;
        });
        page->aiBatchCommentsButton()->click();

        QVERIFY(dialogSeen);
        QVERIFY(applied);
        QCOMPARE(model->cellText(0, 9), expectedComment);
        QVERIFY(page->hasUnsavedChanges());
        auto* const save = page->findChild<QPushButton*>(
            QStringLiteral("speakingEvaluationSave"));
        QVERIFY(save);
        QVERIFY(save->isEnabled());
        save->click();
        QCoreApplication::processEvents();
        QVERIFY(!page->hasUnsavedChanges());
    }

    {
        PageHarness reopenedHarness;
        QVERIFY(reopenedHarness.initialize(state, true));
        Ui::SpeakingEvaluationPage* const reopened = reopenedHarness.page();
        QVERIFY(reopened);
        QVERIFY(reopened->selectedClassId().has_value());
        QVERIFY(*reopened->selectedClassId() == classId(1));
        QCOMPARE(reopened->selectedEvaluationName(), QStringLiteral("Winter"));
        QCOMPARE(reopened->matrixModel()->cellText(0, 1),
            QStringLiteral("Synthetic Student"));
        QCOMPARE(reopened->matrixModel()->cellText(0, 2),
            QStringLiteral("민아예시"));
        QCOMPARE(reopened->matrixModel()->cellText(0, 9), expectedComment);
        QVERIFY(!reopened->hasUnsavedChanges());
    }
}

void SpeakingEvaluationPageTests::capturesEnglishKoreanLightAndDarkPresentation()
{
    QTemporaryDir temporary;
    QString path;
    QVERIFY(copyLargeFixture(temporary, &path));
    FakeEvaluationState state;
    state.path = path;
    state.identity = QStringLiteral("capture-workspace");
    PageHarness harness;
    QVERIFY(harness.initialize(state, true));
    Ui::SpeakingEvaluationPage* const page = harness.page();
    QVERIFY(page);
    QVERIFY(page->matrixModel()->setData(
        page->matrixModel()->index(0, 1), QStringLiteral("Jin Park")));
    QVERIFY(page->matrixModel()->setData(
        page->matrixModel()->index(0, 2), QStringLiteral("김민지")));
    QVERIFY(page->matrixModel()->setData(
        page->matrixModel()->index(0, 10),
        QStringLiteral("[Did Well]\nClear ideas\n"
                       "[Needs Improvement]\nPractice past tense")));
    page->tableView()->setCurrentIndex(page->matrixModel()->index(0, 9));
    auto* const host = &harness.host();
    host->resize(1440, 880);
    host->show();
    QCoreApplication::processEvents();

    const QString outputDirectory = QStringLiteral(
        CLASSMNGR_NEXT_SPEAKING_EVALUATION_CAPTURE_DIRECTORY);
    QVERIFY(QDir().mkpath(outputDirectory));
    const QString translationDirectory = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    const QString styleDirectory = QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)).filePath(QStringLiteral("assets/styles"));
    QJsonArray outputs;
    QTranslator translator;
    bool translatorInstalled = false;
    const auto capture = [&outputs, &outputDirectory](
        QWidget& widget,
        const QString& kind,
        const QString& locale,
        const QString& theme,
        const QSize& expectedSize
        ) {
        const QString fileName = kind + QLatin1Char('_') + locale
            + QLatin1Char('-') + theme + QStringLiteral(".png");
        const QString imagePath = QDir(outputDirectory).filePath(fileName);
        const QPixmap image = widget.grab();
        if (image.size() != expectedSize || !image.save(imagePath))
        {
            return false;
        }
        outputs.append(QJsonObject{
            {QStringLiteral("kind"), kind},
            {QStringLiteral("locale"), locale},
            {QStringLiteral("theme"), theme},
            {QStringLiteral("path"), imagePath},
            {QStringLiteral("width"), image.width()},
            {QStringLiteral("height"), image.height()}
        });
        return true;
    };
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        if (locale == QStringLiteral("ko_KR"))
        {
            QVERIFY(translator.load(QDir(translationDirectory).filePath(
                QStringLiteral("ClassMngr_ko_KR.qm"))));
            translatorInstalled = qApp->installTranslator(&translator);
            QVERIFY(translatorInstalled);
        }
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            QLocale::setDefault(QLocale(locale));
            qApp->setPalette(themePalette(theme));
            qApp->setStyleSheet(readFile(QDir(styleDirectory).filePath(
                theme + QStringLiteral(".qss"))));
            QCoreApplication::processEvents();
            for (const QString& objectName : {
                     QStringLiteral("speakingEvaluationImportNames"),
                     QStringLiteral("speakingEvaluationEditNotes"),
                     QStringLiteral("speakingEvaluationAiComment"),
                     QStringLiteral("speakingEvaluationAiBatchComments"),
                     QStringLiteral("speakingEvaluationReports"),
                     QStringLiteral("speakingEvaluationSaveReports"),
                     QStringLiteral("speakingEvaluationPrintReports"),
                     QStringLiteral("speakingEvaluationUndo"),
                     QStringLiteral("speakingEvaluationSave")})
            {
                auto* const button = page->findChild<QPushButton*>(objectName);
                QVERIFY(button);
                QVERIFY2(buttonLabelFits(*button),
                    qPrintable(QStringLiteral("Action label does not fit: %1")
                        .arg(button->text())));
            }
            QVERIFY(capture(*host, QStringLiteral("page"), locale, theme,
                QSize(1440, 880)));

            Ui::SpeakingEvaluationAiPromptDialog promptDialog(
                QStringLiteral("Write a speaking comment for STD_NAME.\n\n"
                               "Did well:\nClear ideas\n\n"
                               "Needs improvement:\nAdd more detail."),
                QStringLiteral("ChatGPT"),
                QUrl(QStringLiteral("https://chatgpt.com/")));
            promptDialog.show();
            QCoreApplication::processEvents();
            QVERIFY(capture(promptDialog, QStringLiteral("ai-prompt"),
                locale, theme, QSize(840, 600)));
            promptDialog.hide();

            std::vector<Ui::SpeakingEvaluationAiBatchStudent> batchStudents{
                {
                    .sourceRow = 0,
                    .id = QStringLiteral("STUDENT_01"),
                    .englishName = QStringLiteral("Jin Park"),
                    .koreanName = QStringLiteral("김민지"),
                    .didWell = QStringLiteral("Clear ideas"),
                    .needsImprovement = QStringLiteral("Add more detail"),
                    .grade = Domain::CourseGradeBand::E4,
                    .eligibilityReason = Application::
                        SpeakingEvaluationAiBatchEligibilityReason::Eligible
                },
                {
                    .sourceRow = 1,
                    .id = QStringLiteral("STUDENT_02"),
                    .englishName = QStringLiteral("Mina Lee"),
                    .koreanName = QStringLiteral("이미나"),
                    .oldComment = QStringLiteral("Existing synthetic comment"),
                    .didWell = QStringLiteral("Strong details"),
                    .needsImprovement = QStringLiteral("Use transitions"),
                    .grade = Domain::CourseGradeBand::E4,
                    .eligibilityReason = Application::
                        SpeakingEvaluationAiBatchEligibilityReason::Eligible
                }
            };
            Ui::SpeakingEvaluationAiBatchDialog batchDialog(
                std::move(batchStudents),
                Application::AiCommentVoice::DirectToStudent,
                QStringLiteral("ChatGPT"),
                QUrl(QStringLiteral("https://chatgpt.com/")));
            batchDialog.resize(1080, 760);
            batchDialog.show();
            QCoreApplication::processEvents();
            batchDialog.createPromptButton()->click();
            QCoreApplication::processEvents();
            QVERIFY(!batchDialog.promptEditor()->toPlainText().isEmpty());
            QVERIFY(capture(batchDialog, QStringLiteral("ai-batch"),
                locale, theme, QSize(1080, 760)));

            batchDialog.responseEditor()->setPlainText(QStringLiteral(
                "<<<STUDENT_01>>>\n"
                "STD_NAME explains ideas and uses details. "
                "STD_NAME should add transitions. "
                "STD_NAME can practice more.\n"
                "<<<END_STUDENT_01>>>"));
            batchDialog.parseButton()->click();
            auto* const review = batchDialog.reviewModel();
            QCOMPARE(review->rowCount(), 1);
            QVERIFY(review->data(review->index(0, 0),
                Ui::SpeakingEvaluationAiBatchReviewModel::ValidRole).toBool());
            static_cast<void>(review->setData(
                review->index(0,
                    Ui::SpeakingEvaluationAiBatchReviewModel::ApplyColumn),
                Qt::Checked,
                Qt::CheckStateRole));
            auto* const tabs = batchDialog.findChild<QTabWidget*>(
                QStringLiteral("speakingEvaluationAiBatchTabs"));
            QVERIFY(tabs);
            tabs->setCurrentIndex(1);
            QCoreApplication::processEvents();
            QVERIFY(capture(batchDialog, QStringLiteral("ai-batch-review"),
                locale, theme, QSize(1080, 760)));
            batchDialog.hide();
        }
        if (translatorInstalled)
        {
            qApp->removeTranslator(&translator);
            translatorInstalled = false;
        }
    }
    QFile manifest(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(manifest.write(QJsonDocument(QJsonObject{
        {QStringLiteral("captures"), outputs},
        {QStringLiteral("claim"), QStringLiteral(
            "C39 presentation evidence; no pixel-parity claim.")}
    }).toJson(QJsonDocument::Indented)) > 0);
    manifest.close();
    QVERIFY(QFileInfo::exists(QDir(outputDirectory).filePath(
        QStringLiteral("ai-batch_ko_KR-dark.png"))));
}

void SpeakingEvaluationPageTests::
capturesSpeakingEvaluationReportsForEnglishKoreanLightAndDark()
{
    const QString captureDirectory = QDir(QStringLiteral(
        CLASSMNGR_NEXT_SPEAKING_EVALUATION_CAPTURE_DIRECTORY)).filePath(
            QStringLiteral("reports-c40"));
    QDir existingCaptures(captureDirectory);
    if (existingCaptures.exists())
        QVERIFY(existingCaptures.removeRecursively());
    QVERIFY(QDir().mkpath(captureDirectory));

    FakeEvaluationState state;
    PageHarness harness;
    QVERIFY(harness.initialize(state));
    const auto resolvedAssets = Output::resolveSpeakingEvaluationReportAssets(
        harness.resourceLoader(), SpeakingEvalReportTemplate::Standard);
    QVERIFY(resolvedAssets);
    SpeakingEvalReportAssetEnvironment assets = *resolvedAssets;

    QList<int> fontIds;
    const auto loadFont = [&harness, &fontIds](const QString& logicalId) {
        const auto id = Resources::ResourceId::fromString(logicalId);
        if (!id) return QString{};
        const auto request = Resources::ResourceRequest::create(*id,
            Resources::ResourceScopeKind::Operation,
            QStringLiteral("speaking-evaluation-report-capture"));
        if (!request) return QString{};
        const auto file = harness.resourceLoader().resolveFile(*request);
        if (!file) return QString{};
        const int fontId = QFontDatabase::addApplicationFont(file->get());
        if (fontId < 0) return QString{};
        fontIds.append(fontId);
        return QFontDatabase::applicationFontFamilies(fontId).value(0);
    };
    assets.latinFamily = loadFont(QStringLiteral("resource:assets/fonts/Inter.ttc"));
    assets.koreanFamily = loadFont(
        QStringLiteral("resource:assets/fonts/PretendardVariable.ttf"));
    QVERIFY(!assets.latinFamily.isEmpty());
    QVERIFY(!assets.koreanFamily.isEmpty());

    QImage signature(360, 90, QImage::Format_ARGB32_Premultiplied);
    signature.fill(Qt::transparent);
    {
        QPainter painter(&signature);
        painter.setPen(QColor(QStringLiteral("#23445A")));
        painter.drawText(signature.rect(), Qt::AlignCenter,
            QStringLiteral("Synthetic Signature"));
    }
    QByteArray signatureBytes;
    QBuffer signatureBuffer(&signatureBytes);
    QVERIFY(signatureBuffer.open(QIODevice::WriteOnly));
    QVERIFY(signature.save(&signatureBuffer, "PNG"));

    QList<SpeakingEvalBatchReportService::StudentReport> reports;
    for (int index = 0; index < 2; ++index)
    {
        SpeakingEvalReportData report;
        report.reportTemplate = SpeakingEvalReportTemplate::Standard;
        report.englishName = index == 0
            ? QStringLiteral("Jin Park") : QStringLiteral("Mina Lee");
        report.koreanName = index == 0
            ? QStringLiteral("김민지") : QStringLiteral("이미나");
        report.classLabel = QStringLiteral("E5 Athena");
        report.nativeTeacher = QStringLiteral("Teacher North");
        report.koreanTeacher = QStringLiteral("교사 북");
        report.date = QStringLiteral("Jun. 2026");
        report.comments = index == 0
            ? QStringLiteral("Jin explains ideas clearly and supports each answer with useful details.")
            : QStringLiteral("Mina organizes her thoughts well and speaks with confidence.");
        report.scores = {
            QStringLiteral("A+"), QStringLiteral("A"), QStringLiteral("B+"),
            QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("A+")
        };
        report.signatureImage = signatureBytes;
        reports.append({
            report.englishName + QStringLiteral(" (") + report.koreanName
                + QStringLiteral(")"),
            report,
            index * 2
        });
    }

    const QLocale originalLocale;
    const QPalette originalPalette = qApp->palette();
    const QString originalStyleSheet = qApp->styleSheet();
    QTranslator translator;
    bool translatorInstalled = false;
    const QString translationDirectory = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    const QString styleDirectory = QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)).filePath(QStringLiteral("assets/styles"));
    QJsonArray captures;
    int expectedCaptureCount = 8;
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    expectedCaptureCount += 2;
#endif
    const auto capture = [&captures, &captureDirectory](QWidget& widget,
        const QString& kind, const QString& locale, const QString& theme,
        const QSize& expectedSize)
    {
        const QString fileName = kind + QLatin1Char('_') + locale
            + QLatin1Char('-') + theme + QStringLiteral(".png");
        const QString path = QDir(captureDirectory).filePath(fileName);
        const QPixmap image = widget.grab();
        if (image.size() != expectedSize || !image.save(path)) return false;
        captures.append(QJsonObject{
            {QStringLiteral("kind"), kind},
            {QStringLiteral("locale"), locale},
            {QStringLiteral("theme"), theme},
            {QStringLiteral("path"), path},
            {QStringLiteral("width"), image.width()},
            {QStringLiteral("height"), image.height()}
        });
        return true;
    };

    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        if (locale == QStringLiteral("ko_KR"))
        {
            QVERIFY(translator.load(QDir(translationDirectory).filePath(
                QStringLiteral("ClassMngr_ko_KR.qm"))));
            translatorInstalled = qApp->installTranslator(&translator);
            QVERIFY(translatorInstalled);
        }
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            QLocale::setDefault(QLocale(locale));
            qApp->setPalette(themePalette(theme));
            qApp->setStyleSheet(readFile(QDir(styleDirectory).filePath(
                theme + QStringLiteral(".qss"))));
            QCoreApplication::processEvents();

            Ui::SpeakingEvaluationReportActions actions;
            actions.assets = assets;
            {
                Ui::SpeakingEvaluationReportsDialog preview(reports, 1, actions);
                preview.resize(940, 900);
                preview.show();
                QCoreApplication::processEvents();
                if (locale == QStringLiteral("ko_KR"))
                {
                    const auto* const closeButton =
                        preview.findChild<QPushButton*>(
                            QStringLiteral("nextReportClose"));
                    const auto* const saveButton =
                        preview.findChild<QPushButton*>(
                            QStringLiteral("nextReportSavePdf"));
                    QVERIFY(closeButton);
                    QVERIFY(saveButton);
                    QCOMPARE(closeButton->text(), QStringLiteral("닫기"));
                    QCOMPARE(saveButton->text(), QStringLiteral("PDF로 저장"));
                }
                QVERIFY(capture(preview, QStringLiteral("report-preview"),
                    locale, theme, QSize(940, 900)));
                preview.hide();
            }
            QCOMPARE(speakingEvalReportAssetResidency().sessions, 0);
            QCOMPARE(speakingEvalReportAssetResidency().templates, 0);
            QCOMPARE(speakingEvalReportAssetResidency().decodedImageBytes, qsizetype(0));

            {
                Ui::SpeakingEvaluationReportBatchDialog batch(reports, 1,
                    captureDirectory, Ui::SpeakingEvaluationReportBatchDialog::Mode::SaveAs,
                    actions);
                batch.resize(760, 500);
                batch.show();
                QCoreApplication::processEvents();
                QVERIFY(capture(batch, QStringLiteral("report-batch"),
                    locale, theme, QSize(760, 500)));
                if (locale == QStringLiteral("ko_KR"))
                {
                    const auto hasNote = [&batch](const QString& text) {
                        for (const QLabel* label : batch.findChildren<QLabel*>())
                            if (label->text() == text) return true;
                        return false;
                    };
                    const auto* const cancelButton =
                        batch.findChild<QPushButton*>(
                            QStringLiteral("nextReportBatchCancel"));
                    const auto* const previewButton =
                        batch.findChild<QPushButton*>(
                            QStringLiteral("nextReportBatchPreview"));
                    const auto* const exportButton =
                        batch.findChild<QPushButton*>(
                            QStringLiteral("nextReportBatchExport"));
                    const auto* const chooseButton =
                        batch.findChild<QPushButton*>(
                            QStringLiteral("nextReportChooseDirectory"));
                    const auto* const keepPdfs =
                        batch.findChild<QCheckBox*>(
                            QStringLiteral("nextReportKeepPdfs"));
                    const auto* const openOutput =
                        batch.findChild<QCheckBox*>(
                            QStringLiteral("nextReportOpenOutput"));
                    QVERIFY(cancelButton);
                    QVERIFY(previewButton);
                    QVERIFY(exportButton);
                    QVERIFY(chooseButton);
                    QVERIFY(keepPdfs);
                    QVERIFY(openOutput);
                    QCOMPARE(batch.windowTitle(),
                        QStringLiteral("말하기 평가 보고서를 다른 이름으로 저장"));
                    QCOMPARE(cancelButton->text(), QStringLiteral("취소"));
                    QCOMPARE(previewButton->text(), QStringLiteral("보고서 미리 보기"));
                    QCOMPARE(exportButton->text(), QStringLiteral("다른 이름으로 저장..."));
                    QCOMPARE(chooseButton->text(), QStringLiteral("선택…"));
                    QCOMPARE(keepPdfs->text(), QStringLiteral("압축 후 개별 PDF 파일 유지"));
                    QCOMPARE(openOutput->text(), QStringLiteral("저장 후 출력 폴더 열기"));
                    QCOMPARE(batch.scopeSelector()->itemText(0), QStringLiteral("현재 학생"));
                    QCOMPARE(batch.scopeSelector()->itemText(1), QStringLiteral("모든 학생(2)"));
                    QCOMPARE(batch.rendererSelector()->itemText(0),
                        QStringLiteral("내부 템플릿 (기본값)"));
                    QVERIFY(hasNote(QStringLiteral("학생:")));
                    QVERIFY(hasNote(QStringLiteral("렌더러:")));
                    QVERIFY(hasNote(QStringLiteral("출력 폴더:")));
                    QVERIFY(hasNote(QStringLiteral("말하기 평가 보고서를 파일로 저장합니다.")));
                    QVERIFY(hasNote(QStringLiteral(
                        "보고서를 만들 때 권장하는 방법입니다.")));
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
                    QVERIFY(batch.rendererSelector()->count() > 1);
                    QCOMPARE(batch.rendererSelector()->itemText(1),
                        QStringLiteral("PowerPoint (대체)"));
                    batch.rendererSelector()->setCurrentIndex(1);
                    QCoreApplication::processEvents();
#ifdef Q_OS_MACOS
                    QVERIFY(hasNote(QStringLiteral(
                        "내부 방식이 작동하지 않을 때만 사용하세요. 완료하는 데 시간이 더 걸릴 수 있으며, 여러 파일 및 폴더 권한 요청이 표시될 수 있습니다. PowerPoint에서 작업 중인 내용이 있다면 저장하세요. 진행 상황이 손실될 위험이 있습니다.")));
#else
                    QVERIFY(hasNote(QStringLiteral(
                        "내부 방식이 작동하지 않을 때만 사용하세요. PowerPoint에서 작업 중인 내용이 있다면 저장하세요. 진행 상황이 손실될 위험이 있습니다.")));
#endif
                    const QRect exportButtonGeometry(
                        exportButton->mapTo(&batch, QPoint()), exportButton->size());
                    QVERIFY(batch.rect().contains(exportButtonGeometry));
                    QVERIFY(capture(batch, QStringLiteral("report-batch-powerpoint"),
                        locale, theme, batch.size()));
#else
                    QCOMPARE(batch.rendererSelector()->count(), 1);
#endif
                }
                batch.hide();
            }
        }
        if (translatorInstalled)
        {
            qApp->removeTranslator(&translator);
            translatorInstalled = false;
        }
    }

    const QString pdfPath = QDir(captureDirectory).filePath(
        QStringLiteral("synthetic-speaking-evaluation-report.pdf"));
    SpeakingEvalBatchReportService::Request request;
    request.reports = {reports.constFirst()};
    request.savePdf = true;
    request.outputFilePath = pdfPath;
    {
        SpeakingEvalReportAssetSession assetSession(assets);
        QVERIFY(assetSession.isValid());
        const auto result =
            SpeakingEvalBatchReportService::exportResolvedReports(request);
        QCOMPARE(result.status, SpeakingEvalBatchReportService::Status::Completed);
        QCOMPARE(result.savedPdfPaths, QStringList{pdfPath});
    }
    QPdfDocument pdf;
    QCOMPARE(pdf.load(pdfPath), QPdfDocument::Error::None);
    QCOMPARE(pdf.pageCount(), 1);

    if (translatorInstalled) qApp->removeTranslator(&translator);
    QLocale::setDefault(originalLocale);
    qApp->setPalette(originalPalette);
    qApp->setStyleSheet(originalStyleSheet);
    for (const int fontId : fontIds)
        QFontDatabase::removeApplicationFont(fontId);

    QJsonObject manifest{
        {QStringLiteral("captures"), captures},
        {QStringLiteral("pdf"), QJsonObject{
            {QStringLiteral("path"), pdfPath},
            {QStringLiteral("pages"), pdf.pageCount()},
            {QStringLiteral("syntheticInput"), true}
        }},
        {QStringLiteral("claim"), QStringLiteral(
            "Synthetic C40 report UI and output evidence; no Legacy/Next pixel-parity claim.")}
    };
    QFile manifestFile(QDir(captureDirectory).filePath(
        QStringLiteral("manifest.json")));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(manifestFile.write(QJsonDocument(manifest).toJson(
        QJsonDocument::Indented)) > 0);
    manifestFile.close();
    QCOMPARE(captures.size(), expectedCaptureCount);
    QVERIFY(QFileInfo::exists(pdfPath));
}

QTEST_MAIN(SpeakingEvaluationPageTests)

#include "speaking_evaluation_page_tests.moc"
