#include "data/database/workspace_store.h"
#include "data/repositories/class_repository.h"
#include "next/application/speaking_evaluation_private_notes_split.h"
#include "next/platform/workspace_store_speaking_evaluation_ports.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/speaking_evaluation_ai_dialogs.h"
#include "next/ui/speaking_evaluation_page.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QListView>
#include <QPalette>
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

private:
    Test::MemoryPreferences m_preferences;
    std::unique_ptr<Resources::ResourceLoader> m_loader;
    std::unique_ptr<Ui::PageHost> m_host;
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
    void batchCheckIndicatorMouseAndKeyboardEventsToggleSelection();
    void batchIncludeIndicatorsRenderCheckedAndUncheckedForAllThemes();
    void koreanLightUndoButtonShowsItsFullLabelAtCaptureSize();
    void workspaceStoreBatchApplySaveReopenPersistsComment();
    void capturesEnglishKoreanLightAndDarkPresentation();
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

QTEST_MAIN(SpeakingEvaluationPageTests)

#include "speaking_evaluation_page_tests.moc"
