#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::generateSpeakingAiPrompt()
{
    if (!m_speakingAiStatusText
        || !m_speakingAiPromptTextBox
        || !m_speakingAiResponseTextBox
        || !m_speakingEvaluationList
        || !m_speakingAiDidWellTextBox
        || !m_speakingAiNeedsImprovementTextBox)
    {
        return;
    }

    const int selectedIndex = m_speakingEvaluationList.SelectedIndex();
    if (selectedIndex < 0
        || selectedIndex >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
    {
        m_speakingAiStatusText.Text(
            L"Select a named speaking-evaluation row before generating a prompt."
            );
        updateSpeakingAiActions();
        return;
    }

    const auto& cells = m_speakingEvaluationCellBoxes[
        static_cast<std::size_t>(selectedIndex)
        ];
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    if (cells.size() <= static_cast<std::size_t>(koreanColumn))
    {
        return;
    }
    const std::wstring englishName = cells[static_cast<std::size_t>(
        englishColumn
        )].Text().c_str();
    const std::wstring koreanName = cells[static_cast<std::size_t>(
        koreanColumn
        )].Text().c_str();
    if (englishName.empty() && koreanName.empty())
    {
        m_speakingAiStatusText.Text(
            L"The selected speaking-evaluation row does not have a student name."
            );
        updateSpeakingAiActions();
        return;
    }

    classmngr::engine::SpeakingEvaluationAiPromptInput input;
    input.grade = classmngr::engine::SpeakingEvaluationReportModel::elementaryGrade(
        m_classInfo.classGrade
        );
    input.englishName = asUtf8(englishName);
    input.koreanName = asUtf8(koreanName);
    input.didWell = asUtf8(m_speakingAiDidWellTextBox.Text());
    input.needsImprovement = asUtf8(m_speakingAiNeedsImprovementTextBox.Text());
    input.voice = m_speakingAiVoiceSelector
        && m_speakingAiVoiceSelector.SelectedIndex() == 1
        ? classmngr::engine::SpeakingEvaluationAiVoice::ThirdPerson
        : classmngr::engine::SpeakingEvaluationAiVoice::DirectToStudent;

    if (!classmngr::engine::SpeakingEvaluationAiPromptService::canBuildPrompt(input))
    {
        m_speakingAiPromptTextBox.Text({});
        m_speakingAiResponseTextBox.Text({});
        m_speakingAiStatusText.Text(
            L"AI comments require an E4-E6 class and at least one observation in both sections."
            );
        updateSpeakingAiActions();
        return;
    }

    const std::string prompt =
        classmngr::engine::SpeakingEvaluationAiPromptService::buildCommentPrompt(
            input
            );
    m_speakingAiStudentRow = selectedIndex;
    m_speakingAiBatchRows.clear();
    m_speakingAiParsedComments.clear();
    m_speakingAiPromptTextBox.Text(asWide(prompt));
    m_speakingAiResponseTextBox.Text({});
    m_speakingAiStatusText.Text(
        L"Student prompt generated with the STD_NAME privacy placeholder."
        );
    updateSpeakingAiActions();
}

void MainWindow::copySpeakingAiPrompt(bool openProvider)
{
    if (!m_speakingAiPromptTextBox || !m_speakingAiStatusText)
    {
        return;
    }
    const std::string prompt = asUtf8(m_speakingAiPromptTextBox.Text());
    if (prompt.empty())
    {
        m_speakingAiStatusText.Text(
            L"Generate an AI prompt before copying it."
            );
        updateSpeakingAiActions();
        return;
    }

    const auto copied = classmngr::windows::winui::WindowsClipboard::writeText(
        prompt
        );
    if (!copied)
    {
        m_speakingAiStatusText.Text(winrt::hstring(
            L"The AI prompt could not be copied: "
                + asWide(copied.error().message)
            ));
        return;
    }

    if (!openProvider)
    {
        m_speakingAiStatusText.Text(L"AI prompt copied to the clipboard.");
        return;
    }

    const auto opened = classmngr::windows::winui::WindowsUrlLauncher::openUrl(
        "https://chatgpt.com/"
        );
    m_speakingAiStatusText.Text(
        opened
            ? L"AI prompt copied; ChatGPT was opened for review."
            : winrt::hstring(
                L"AI prompt copied, but ChatGPT could not be opened: "
                    + asWide(opened.error().message)
                )
        );
}

void MainWindow::refreshSpeakingAiBatchSelection()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingAiBatchSelectionList)
    {
        return;
    }

    m_speakingAiBatchSelectionList.Items().Clear();
    const auto appendColumn = [](Grid const& grid, double value, GridUnitType type) {
        auto column = ColumnDefinition();
        column.Width(GridLengthHelper::FromValueAndType(value, type));
        grid.ColumnDefinitions().Append(column);
    };
    const auto addText = [](Grid const& grid, std::wstring_view text, int column,
                            bool header = false) {
        auto block = TextBlock();
        block.Text(hstring(text));
        block.TextWrapping(TextWrapping::Wrap);
        if (header)
        {
            block.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        }
        Grid::SetColumn(block, column);
        grid.Children().Append(block);
    };
    const auto addRow = [this, &appendColumn, &addText](
                            std::wstring_view include,
                            std::wstring_view student,
                            std::wstring_view status,
                            int row,
                            bool enabled,
                            bool checked) {
        auto grid = Grid();
        grid.ColumnSpacing(8.0);
        appendColumn(grid, 84.0, GridUnitType::Pixel);
        appendColumn(grid, 180.0, GridUnitType::Pixel);
        appendColumn(grid, 1.0, GridUnitType::Star);
        if (row < 0)
        {
            addText(grid, include, 0, true);
        }
        else
        {
            auto check = CheckBox();
            check.Content(box_value(hstring(include)));
            check.Tag(box_value(row));
            check.IsChecked(checked);
            check.IsEnabled(enabled);
            check.Click([this](auto const&, auto const&) {
                m_speakingAiBatchRows.clear();
                m_speakingAiParsedComments.clear();
                if (m_speakingAiPromptTextBox)
                {
                    m_speakingAiPromptTextBox.Text({});
                }
                if (m_speakingAiResponseTextBox)
                {
                    m_speakingAiResponseTextBox.Text({});
                }
                if (m_speakingAiParseSummary)
                {
                    m_speakingAiParseSummary.Text({});
                }
                if (m_speakingAiStatusText)
                {
                    m_speakingAiStatusText.Text({});
                }
                rebuildSpeakingAiBatchReview();
                updateSpeakingAiActions();
            });
            Grid::SetColumn(check, 0);
            grid.Children().Append(check);
        }
        addText(grid, student, 1, row < 0);
        addText(grid, status, 2, row < 0);
        auto item = ListViewItem();
        item.Content(grid);
        item.IsTabStop(false);
        m_speakingAiBatchSelectionList.Items().Append(item);
    };
    addRow(L"Include", L"Student", L"Status", -1, false, false);

    const int grade = classmngr::engine::SpeakingEvaluationReportModel::elementaryGrade(
        m_classInfo.classGrade
        );
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    const int commentsColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Comments
        );
    const int notesColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Notes
        );
    for (std::size_t row = 0; row < m_speakingEvaluationCellBoxes.size(); ++row)
    {
        const auto& cells = m_speakingEvaluationCellBoxes[row];
        if (cells.size() <= static_cast<std::size_t>(notesColumn))
        {
            continue;
        }
        const std::wstring english = cells[static_cast<std::size_t>(englishColumn)].Text().c_str();
        const std::wstring korean = cells[static_cast<std::size_t>(koreanColumn)].Text().c_str();
        const std::wstring comment = cells[static_cast<std::size_t>(commentsColumn)].Text().c_str();
        const auto notes = splitSpeakingAiPrivateNotes(asUtf8(
            cells[static_cast<std::size_t>(notesColumn)].Text()
            ));
        std::wstring reason;
        if (english.empty() && korean.empty())
        {
            reason = L"Student name is missing.";
        }
        else if (grade < 4 || grade > 6)
        {
            reason = L"AI comments are available for grades E4 through E6.";
        }
        else if (classmngr::engine::SpeakingEvaluationAiPromptService::observationItems(
                     notes.didWell).empty())
        {
            reason = L"Add at least one Did Well note.";
        }
        else if (classmngr::engine::SpeakingEvaluationAiPromptService::observationItems(
                     notes.needsImprovement).empty())
        {
            reason = L"Add at least one Needs Improvement note.";
        }
        const bool eligible = reason.empty();
        if (eligible)
        {
            reason = comment.empty()
                ? L"Ready"
                : L"Existing comment â€” select to regenerate";
        }
        const std::wstring student = english.empty() ? korean : english;
        addRow(L"", student, reason, static_cast<int>(row), eligible,
               eligible && comment.empty());
    }
}

void MainWindow::generateSpeakingAiBatchPrompt()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingAiPromptTextBox
        || !m_speakingAiResponseTextBox
        || !m_speakingAiStatusText
        || !m_speakingEvaluationCellBoxes.size())
    {
        return;
    }

    classmngr::engine::SpeakingEvaluationAiBatchPromptInput input;
    input.voice = m_speakingAiVoiceSelector
        && m_speakingAiVoiceSelector.SelectedIndex() == 1
        ? classmngr::engine::SpeakingEvaluationAiVoice::ThirdPerson
        : classmngr::engine::SpeakingEvaluationAiVoice::DirectToStudent;
    const int grade = classmngr::engine::SpeakingEvaluationReportModel::elementaryGrade(
        m_classInfo.classGrade
        );
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    const int notesColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Notes
        );

    for (const auto& cells : m_speakingEvaluationCellBoxes)
    {
        if (cells.size() <= static_cast<std::size_t>(koreanColumn))
        {
            continue;
        }
        const std::string englishName = asUtf8(
            cells[static_cast<std::size_t>(englishColumn)].Text()
            );
        const std::string koreanName = asUtf8(
            cells[static_cast<std::size_t>(koreanColumn)].Text()
            );
        if (!englishName.empty())
        {
            input.additionalNamesToRedact.push_back(englishName);
        }
        if (!koreanName.empty())
        {
            input.additionalNamesToRedact.push_back(koreanName);
        }
    }

    std::vector<int> selectedRows;
    const bool hasBatchSelection = m_speakingAiBatchSelectionList
        && m_speakingAiBatchSelectionList.Items().Size() > 1;
    if (hasBatchSelection)
    {
        for (uint32_t index = 1;
             index < m_speakingAiBatchSelectionList.Items().Size();
             ++index)
        {
            const auto item = m_speakingAiBatchSelectionList.Items().GetAt(index)
                .try_as<ListViewItem>();
            const auto row = item ? item.Content().try_as<Grid>() : nullptr;
            const auto check = row && row.Children().Size() > 0
                ? row.Children().GetAt(0).try_as<CheckBox>()
                : nullptr;
            if (check)
            {
                const auto checked = check.IsChecked();
                if (checked && checked.Value())
                {
                    selectedRows.push_back(unbox_value<int>(check.Tag()));
                }
            }
        }
    }

    m_speakingAiBatchRows.clear();
    m_speakingAiParsedComments.clear();
    if (m_speakingAiParseSummary)
    {
        m_speakingAiParseSummary.Text({});
    }
    rebuildSpeakingAiBatchReview();
    for (std::size_t rowIndex = 0;
         rowIndex < m_speakingEvaluationCellBoxes.size();
         ++rowIndex)
    {
        const auto& cells = m_speakingEvaluationCellBoxes[rowIndex];
        if (cells.size() <= static_cast<std::size_t>(notesColumn))
        {
            continue;
        }
        const std::string englishName = asUtf8(
            cells[static_cast<std::size_t>(englishColumn)].Text()
            );
        const std::string koreanName = asUtf8(
            cells[static_cast<std::size_t>(koreanColumn)].Text()
            );
        const auto notes = splitSpeakingAiPrivateNotes(asUtf8(
            cells[static_cast<std::size_t>(notesColumn)].Text()
            ));
        if ((englishName.empty() && koreanName.empty())
            || grade < 4
            || grade > 6
            || classmngr::engine::SpeakingEvaluationAiPromptService::observationItems(
                notes.didWell
                ).empty()
            || classmngr::engine::SpeakingEvaluationAiPromptService::observationItems(
                notes.needsImprovement
                ).empty())
        {
            continue;
        }
        if (hasBatchSelection
            && std::find(selectedRows.begin(), selectedRows.end(),
                         static_cast<int>(rowIndex)) == selectedRows.end())
        {
            continue;
        }

        input.students.push_back({
            speakingAiStudentId(rowIndex),
            grade,
            englishName,
            koreanName,
            notes.didWell,
            notes.needsImprovement
            });
        m_speakingAiBatchRows.push_back(static_cast<int>(rowIndex));
    }

    if (input.students.empty())
    {
        m_speakingAiPromptTextBox.Text({});
        m_speakingAiResponseTextBox.Text({});
        m_speakingAiParsedComments.clear();
        m_speakingAiStatusText.Text(
            L"No selected eligible students. Each batch row needs a name, an E4-E6 class, and observations in both note sections."
            );
        updateSpeakingAiActions();
        return;
    }

    const std::string prompt =
        classmngr::engine::SpeakingEvaluationAiPromptService::buildBatchCommentPrompt(
            input
            );
    if (prompt.empty())
    {
        m_speakingAiBatchRows.clear();
        m_speakingAiStatusText.Text(
            L"The engine could not build the batch AI prompt from the selected rows."
            );
        updateSpeakingAiActions();
        return;
    }

    m_speakingAiStudentRow = -1;
    m_speakingAiParsedComments.clear();
    m_speakingAiPromptTextBox.Text(asWide(prompt));
    m_speakingAiResponseTextBox.Text({});
    m_speakingAiStatusText.Text(winrt::hstring(
        L"Batch prompt generated for "
            + std::to_wstring(input.students.size())
            + L" students; names are redacted in the prompt."
        ));
    updateSpeakingAiActions();
}

} // namespace winrt::ClassMngrWinUI::implementation
