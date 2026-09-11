#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

bool MainWindow::runPhase6SpeakingEvaluationChecks()
{
    m_phase6SpeakingEvaluationFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6SpeakingEvaluationFailureMask = failureMask;
        return false;
    };
    const auto contains = [](winrt::hstring const& value,
                             std::wstring_view text) {
        return std::wstring_view(value.c_str(), value.size()).find(text)
            != std::wstring_view::npos;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_classLoading = false;
    m_classDirty = false;
    m_classDetailsDirty = false;
    m_classNotesDirty = false;
    m_classNew = false;
    m_classRosterLoading = false;
    m_classRosterDirty = false;
    m_speakingEvaluationLoading = false;
    m_speakingEvaluationDirty = false;
    m_speakingEvaluationDirtyCells.clear();
    m_speakingAnalyticsLoading = false;
    m_speakingAnalyticsName = "All";

    navigateTo(classesPageId);
    refreshClassesPage();
    const bool noDatabaseReady =
        m_currentPageId == classesPageId
        && m_speakingEvaluationStatusText
        && m_speakingEvaluationStatusText.Text() == L"No database open."
        && m_speakingEvaluationList
        && !m_speakingEvaluationList.IsEnabled()
        && m_speakingEvaluationSaveButton
        && !m_speakingEvaluationSaveButton.IsEnabled();
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto classId = repository.create("Speaking evaluation class");
    if (!classId)
    {
        return fail(4);
    }

    const auto& grades = classmngr::engine::ClassInfoConfig::grades();
    if (grades.empty())
    {
        return fail(8);
    }
    const auto levels = classmngr::engine::ClassInfoConfig::levelsForGrade(
        grades.front()
        );
    const auto readingBooks = classmngr::engine::ClassInfoConfig::readingBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    const auto essayBooks = classmngr::engine::ClassInfoConfig::essayBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    if (levels.empty() || readingBooks.empty() || essayBooks.empty())
    {
        return fail(8);
    }

    classmngr::engine::ClassInfo info;
    info.classId = *classId;
    info.classGrade = grades.front();
    info.classLevel = levels.front();
    info.readingBook = readingBooks.front();
    info.essayBook = essayBooks.front();
    info.classColor = "#FFFFFF";
    info.fontColor = "#000000";
    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    if (!infoService.save(info))
    {
        return fail(16);
    }

    classmngr::engine::Roster roster;
    for (const std::string_view column : classmngr::engine::RosterBaseColumns)
    {
        roster.columns.emplace_back(column);
    }
    roster.columnWidths = {140, 140, 100, 140, 100, 100};
    roster.rows.push_back({
        "Alice",
        winrt::to_string(winrt::hstring(L"\uC568\uB9AC\uC2A4")),
        "",
        "",
        "",
        ""
        });
    roster.rows.push_back({
        "Bob",
        winrt::to_string(winrt::hstring(L"\uAE40\uBBFC\uC218")),
        "",
        "",
        "",
        ""
        });
    classmngr::engine::RosterService rosterService(*m_openDatabase);
    if (!rosterService.save(*classId, roster))
    {
        return fail(32);
    }

    refreshClassesPage();
    const bool controlsReady =
        m_classSelectedId == *classId
        && m_speakingEvaluationSelector
        && m_speakingEvaluationSelector.Items().Size() == 4
        && m_speakingEvaluationHeaderGrid
        && m_speakingEvaluationHeaderGrid.Children().Size() == 11
        && m_speakingEvaluationList
        && m_speakingEvaluationList.Items().Size() == 25
        && m_speakingEvaluationCellBoxes.size() == 25
        && m_speakingEvaluationCellBoxes.front().size() == 11;
    if (!controlsReady)
    {
        return fail(64);
    }

    importSpeakingEvaluationNames();
    const bool namesImported =
        m_speakingEvaluationDirty
        && m_speakingEvaluationCellBoxes[0][1].Text() == L"Alice"
        && m_speakingEvaluationCellBoxes[0][2].Text()
            == winrt::hstring(L"\uC568\uB9AC\uC2A4")
        && m_speakingEvaluationCellBoxes[1][1].Text() == L"Bob";
    if (!namesImported)
    {
        return fail(128);
    }

    m_speakingEvaluationList.SelectedIndex(0);
    m_speakingEvaluationPasteTextBox.Text(L"A+\tA\tB+\tA\tB+\tA");
    applySpeakingEvaluationPaste();
    const bool pastedScores =
        m_speakingEvaluationCellBoxes[0][3].Text() == L"A+"
        && m_speakingEvaluationCellBoxes[0][8].Text() == L"A"
        && contains(
            m_speakingEvaluationStatusText.Text(),
            L"Applied 6 score cells"
            );
    if (!pastedScores)
    {
        return fail(256);
    }

    saveSpeakingEvaluation();
    classmngr::engine::SpeakingEvaluationPersistenceService evaluationService(
        *m_openDatabase
        );
    const auto winterSaved = evaluationService.load(*classId, "Winter");
    const bool winterPersistenceReady =
        winterSaved
        && !m_speakingEvaluationDirty
        && winterSaved->size() == static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationRowCount
            )
        && !winterSaved->empty()
        && winterSaved->at(0).size() > 8
        && winterSaved->at(0).at(1) == "Alice"
        && winterSaved->at(0).at(3) == "A+"
        && winterSaved->at(0).at(8) == "A";
    if (!winterPersistenceReady)
    {
        return fail(131072);
    }

    refreshSpeakingAnalytics();
    const bool analyticsReady =
        m_speakingAnalyticsStatusText
        && m_speakingAnalyticsStatusText.Text().empty()
        && m_speakingAnalyticsCriteriaPanel
        && m_speakingAnalyticsCriteriaPanel.Children().Size() == 6
        && m_speakingAnalyticsRankingList
        && m_speakingAnalyticsRankingList.Items().Size() == 1
        && contains(
            m_speakingAnalyticsSummaryText.Text(),
            L"Class average"
            )
        && contains(
            m_speakingAnalyticsShapeText.Text(),
            L"Winter"
            );
    if (!analyticsReady)
    {
        return fail(262144);
    }

    m_speakingEvaluationCellBoxes[0][3].Text(L"Z");
    saveSpeakingEvaluation();
    const auto winterAfterInvalid = evaluationService.load(*classId, "Winter");
    const bool invalidScoreRejected =
        m_speakingEvaluationDirty
        && m_speakingEvaluationValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && winterAfterInvalid
        && !winterAfterInvalid->empty()
        && winterAfterInvalid->at(0).size() > 3
        && winterAfterInvalid->at(0).at(3) == "A+";
    if (!invalidScoreRejected)
    {
        return fail(1024);
    }

    m_speakingEvaluationCellBoxes[0][3].Text(L"A+");
    m_speakingEvaluationCellBoxes[0][10].Text(L"private note");
    saveSpeakingEvaluation();
    const auto savedWithNote = evaluationService.load(*classId, "Winter");
    const bool noteSaved =
        savedWithNote
        && !m_speakingEvaluationDirty
        && savedWithNote->at(0).at(10) == "private note";
    if (!noteSaved)
    {
        return fail(2048);
    }

    m_speakingEvaluationSelector.SelectedIndex(2);
    const bool summerLoaded =
        m_speakingEvaluationName == "Summer"
        && !m_speakingEvaluationDirty
        && m_speakingEvaluationCellBoxes[0][1].Text().empty();
    if (!summerLoaded)
    {
        return fail(4096);
    }
    importSpeakingEvaluationNames();
    m_speakingEvaluationList.SelectedIndex(0);
    m_speakingEvaluationPasteTextBox.Text(L"B\tB\tB\tB\tB\tB");
    applySpeakingEvaluationPaste();
    saveSpeakingEvaluation();
    const auto summerSaved = evaluationService.load(*classId, "Summer");
    if (!summerSaved || m_speakingEvaluationDirty
        || summerSaved->at(0).at(3) != "B")
    {
        return fail(8192);
    }

    m_speakingEvaluationSelector.SelectedIndex(0);
    if (m_speakingEvaluationCellBoxes[0][10].Text() != L"private note")
    {
        return fail(16384);
    }
    m_speakingEvaluationCellBoxes[0][10].Text(L"discard me");
    discardSpeakingEvaluation();
    const bool discardReady =
        !m_speakingEvaluationDirty
        && m_speakingEvaluationCellBoxes[0][10].Text() == L"private note";
    if (!discardReady)
    {
        return fail(32768);
    }

    m_speakingEvaluationCellBoxes[0][9].Text({});
    m_speakingEvaluationCellBoxes[1][9].Text({});
    m_speakingEvaluationCellBoxes[0][10].Text(
        L"[Did Well]\nClear pronunciation\n[Needs Improvement]\nUse longer answers"
        );
    m_speakingEvaluationCellBoxes[1][10].Text(
        L"[Did Well]\nStrong vocabulary\n[Needs Improvement]\nAdd supporting details"
        );
    m_speakingEvaluationList.SelectedIndex(0);
    generateSpeakingAiPrompt();
    const bool aiStudentPromptReady =
        m_speakingAiPromptTextBox
        && contains(m_speakingAiPromptTextBox.Text(), L"STD_NAME")
        && contains(m_speakingAiPromptTextBox.Text(), L"Clear pronunciation")
        && m_speakingAiApplyStudentButton
        && !m_speakingAiApplyStudentButton.IsEnabled()
        && m_speakingAiResponseTextBox.Text().empty();
    if (!aiStudentPromptReady)
    {
        return fail(524288);
    }

    m_speakingAiResponseTextBox.Text(L"Great work, STD_NAME!");
    applySpeakingAiStudentComment();
    const bool aiStudentApplied =
        m_speakingEvaluationCellBoxes[0][9].Text()
            == L"Great work, Alice!"
        && !contains(
            m_speakingEvaluationCellBoxes[0][9].Text(),
            L"STD_NAME"
            );
    if (!aiStudentApplied)
    {
        return fail(1048576);
    }

    m_speakingEvaluationCellBoxes[0][9].Text({});
    generateSpeakingAiBatchPrompt();
    const std::string batchPrompt = asUtf8(m_speakingAiPromptTextBox.Text());
    const bool aiBatchPromptReady =
        m_speakingAiBatchRows.size() == 2
        && batchPrompt.find("STUDENT_01") != std::string::npos
        && batchPrompt.find("STUDENT_02") != std::string::npos
        && batchPrompt.find("Alice") == std::string::npos
        && batchPrompt.find("Bob") == std::string::npos;
    if (!aiBatchPromptReady)
    {
        return fail(2097152);
    }

    m_speakingAiResponseTextBox.Text(
        L"<<<STUDENT_01>>>\nAlice spoke clearly and used strong vocabulary.\n"
        L"<<<END_STUDENT_01>>>\n"
        L"<<<STUDENT_02>>>\nBob shared thoughtful ideas and can add more detail.\n"
        L"<<<END_STUDENT_02>>>"
        );
    parseSpeakingAiBatchResponse();
    if (m_speakingAiParsedComments.size() != 2)
    {
        return fail(4194304);
    }
    applySpeakingAiBatchComments();
    const bool aiBatchApplied =
        m_speakingEvaluationCellBoxes[0][9].Text()
            == L"Alice spoke clearly and used strong vocabulary."
        && m_speakingEvaluationCellBoxes[1][9].Text()
            == L"Bob shared thoughtful ideas and can add more detail."
        && m_speakingEvaluationDirty;
    if (!aiBatchApplied)
    {
        return fail(8388608);
    }
    saveSpeakingEvaluation();
    const auto aiSaved = evaluationService.load(*classId, "Winter");
    const bool aiPersistenceReady =
        aiSaved
        && !m_speakingEvaluationDirty
        && aiSaved->at(0).at(9)
            == "Alice spoke clearly and used strong vocabulary."
        && aiSaved->at(1).at(9)
            == "Bob shared thoughtful ideas and can add more detail.";
    if (!aiPersistenceReady)
    {
        return fail(16777216);
    }

    m_speakingBatchRendererSelector.SelectedIndex(0);
    m_speakingBatchTemplateSelector.SelectedIndex(0);
    m_speakingBatchSavePdfCheck.IsChecked(true);
    m_speakingBatchPrintCheck.IsChecked(false);
    m_speakingBatchKeepIndividualPdfsCheck.IsChecked(false);
    m_speakingBatchOutputDirectoryTextBox.Text(
        L"C:\\Temp\\ClassMngr-speaking-reports"
        );
    planSpeakingBatchReports();
    const bool batchArchivePlanReady =
        m_speakingBatchStatusText
        && contains(m_speakingBatchStatusText.Text(), L"Planned 2")
        && contains(m_speakingBatchStatusText.Text(), L"Internal")
        && contains(m_speakingBatchStatusText.Text(), L"one ZIP archive")
        && m_speakingBatchPlanButton.IsEnabled();
    if (!batchArchivePlanReady)
    {
        return fail(33554432);
    }

    m_speakingBatchKeepIndividualPdfsCheck.IsChecked(true);
    planSpeakingBatchReports();
    const bool individualPdfPlanReady =
        contains(
            m_speakingBatchStatusText.Text(),
            L"retain individual PDFs"
            );
    if (!individualPdfPlanReady)
    {
        return fail(67108864);
    }

    m_speakingBatchRendererSelector.SelectedIndex(1);
    m_speakingBatchTemplateSelector.SelectedIndex(1);
    planSpeakingBatchReports();
    const bool powerPointPlanReady =
        contains(m_speakingBatchStatusText.Text(), L"PowerPoint")
        && contains(
            m_speakingBatchStatusText.Text(),
            L"Renderer-neutral plan accepted"
            );
    if (!powerPointPlanReady)
    {
        return fail(134217728);
    }

    m_speakingBatchSavePdfCheck.IsChecked(false);
    m_speakingBatchPrintCheck.IsChecked(false);
    planSpeakingBatchReports();
    const bool outputModeRejected =
        contains(
            m_speakingBatchStatusText.Text(),
            L"output-mode-required"
            )
        && !m_speakingBatchPlanButton.IsEnabled();
    if (!outputModeRejected)
    {
        return fail(268435456);
    }

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    refreshClassesPage();
    const bool clearedReady =
        m_speakingEvaluationStatusText.Text() == L"No database open."
        && !m_speakingEvaluationList.IsEnabled()
        && !m_speakingEvaluationSaveButton.IsEnabled()
        && m_speakingAnalyticsStatusText.Text() == L"No database open."
        && m_speakingAnalyticsRankingList.Items().Size() == 0;
    return clearedReady ? true : fail(65536);
}

uint32_t MainWindow::phase6SpeakingEvaluationFailureMask() const noexcept
{
    return m_phase6SpeakingEvaluationFailureMask;
}

} // namespace winrt::ClassMngrWinUI::implementation
