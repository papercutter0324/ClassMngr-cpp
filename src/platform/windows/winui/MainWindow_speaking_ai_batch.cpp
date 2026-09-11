#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::parseSpeakingAiBatchResponse()
{
    if (!m_speakingAiStatusText || !m_speakingAiResponseTextBox)
    {
        return;
    }
    if (m_speakingAiBatchRows.empty())
    {
        m_speakingAiStatusText.Text(
            L"Generate a batch prompt before parsing a batch response."
            );
        updateSpeakingAiActions();
        return;
    }

    const std::string response = asUtf8(m_speakingAiResponseTextBox.Text());
    if (response.empty())
    {
        m_speakingAiStatusText.Text(
            L"Paste the marked batch response before parsing it."
            );
        updateSpeakingAiActions();
        return;
    }

    std::vector<std::string> expectedIds;
    expectedIds.reserve(m_speakingAiBatchRows.size());
    for (const int row : m_speakingAiBatchRows)
    {
        if (row >= 0)
        {
            expectedIds.push_back(speakingAiStudentId(
                static_cast<std::size_t>(row)
                ));
        }
    }
    const auto parsed =
        classmngr::engine::SpeakingEvaluationAiPromptService::parseBatchResponse(
            response,
            expectedIds
            );
    m_speakingAiParsedComments = parsed.comments;

    std::wstring status = L"Parsed "
        + std::to_wstring(parsed.comments.size())
        + L" of " + std::to_wstring(expectedIds.size())
        + L" batch comments.";
    if (!parsed.duplicateIds.empty())
    {
        status += L" Duplicate blocks: "
            + std::to_wstring(parsed.duplicateIds.size()) + L".";
    }
    if (!parsed.malformedIds.empty())
    {
        status += L" Malformed blocks: "
            + std::to_wstring(parsed.malformedIds.size()) + L".";
    }
    if (!parsed.unknownIds.empty())
    {
        status += L" Unknown IDs ignored: "
            + std::to_wstring(parsed.unknownIds.size()) + L".";
    }
    m_speakingAiStatusText.Text(hstring(status));
    if (m_speakingAiParseSummary)
    {
        m_speakingAiParseSummary.Text(hstring(status));
    }
    rebuildSpeakingAiBatchReview();
    updateSpeakingAiActions();
}

void MainWindow::rebuildSpeakingAiBatchReview()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingAiBatchReviewList)
    {
        return;
    }
    m_speakingAiBatchReviewList.Items().Clear();
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
    const auto makeRow = [&appendColumn, &addText](bool header) {
        auto grid = Grid();
        grid.ColumnSpacing(8.0);
        appendColumn(grid, 58.0, GridUnitType::Pixel);
        appendColumn(grid, 150.0, GridUnitType::Pixel);
        appendColumn(grid, 130.0, GridUnitType::Pixel);
        appendColumn(grid, 88.0, GridUnitType::Pixel);
        appendColumn(grid, 1.0, GridUnitType::Star);
        if (header)
        {
            addText(grid, L"Apply", 0, true);
            addText(grid, L"Student", 1, true);
            addText(grid, L"Status", 2, true);
            addText(grid, L"Characters", 3, true);
            addText(grid, L"Comment", 4, true);
        }
        return grid;
    };
    auto header = ListViewItem();
    header.Content(makeRow(true));
    header.IsTabStop(false);
    m_speakingAiBatchReviewList.Items().Append(header);

    for (const auto& parsed : m_speakingAiParsedComments)
    {
        const auto rowIt = std::find_if(
            m_speakingAiBatchRows.begin(), m_speakingAiBatchRows.end(),
            [&parsed](int row) {
                return speakingAiStudentId(static_cast<std::size_t>(row)) == parsed.id;
            });
        if (rowIt == m_speakingAiBatchRows.end())
        {
            continue;
        }
        const int rowIndex = *rowIt;
        if (rowIndex < 0
            || rowIndex >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
        {
            continue;
        }
        const int englishColumn = classmngr::engine::toInt(
            classmngr::engine::SpeakingEvaluationColumn::EnglishName
            );
        const int koreanColumn = classmngr::engine::toInt(
            classmngr::engine::SpeakingEvaluationColumn::KoreanName
            );
        const auto& cells = m_speakingEvaluationCellBoxes[
            static_cast<std::size_t>(rowIndex)
            ];
        const std::wstring english = cells[static_cast<std::size_t>(englishColumn)].Text().c_str();
        const std::wstring korean = cells[static_cast<std::size_t>(koreanColumn)].Text().c_str();
        const std::wstring comment = asWide(parsed.comment);
        const bool valid = !comment.empty()
            && comment.size() <= static_cast<std::size_t>(
                classmngr::engine::SpeakingEvaluationCommentMaxLength);
        const std::wstring status = !valid
            ? (comment.empty() ? L"Comment is empty" : L"Over 450 characters")
            : (comment.size() < 100 ? L"Short comment" : L"Ready");
        auto grid = makeRow(false);
        auto check = CheckBox();
        check.Tag(box_value(hstring(asWide(parsed.id))));
        check.IsChecked(valid);
        check.IsEnabled(valid);
        Grid::SetColumn(check, 0);
        grid.Children().Append(check);
        addText(grid, english.empty() ? korean : english, 1);
        auto statusText = TextBlock();
        statusText.Text(hstring(status));
        statusText.TextWrapping(TextWrapping::Wrap);
        Grid::SetColumn(statusText, 2);
        grid.Children().Append(statusText);
        auto characterCountText = TextBlock();
        characterCountText.Text(std::to_wstring(comment.size()));
        characterCountText.TextWrapping(TextWrapping::Wrap);
        Grid::SetColumn(characterCountText, 3);
        grid.Children().Append(characterCountText);
        auto edit = TextBox();
        edit.Text(hstring(comment));
        edit.Tag(box_value(hstring(asWide(parsed.id))));
        edit.AcceptsReturn(true);
        edit.TextWrapping(TextWrapping::Wrap);
        edit.TextChanging([this, check, statusText, characterCountText](
            auto const& sender,
            auto const&
            ) {
            const auto editor = sender.template try_as<TextBox>();
            if (!editor)
            {
                return;
            }
            const std::string id = asUtf8(unbox_value<hstring>(editor.Tag()));
            const auto comment = std::find_if(
                m_speakingAiParsedComments.begin(), m_speakingAiParsedComments.end(),
                [&id](const auto& item) { return item.id == id; });
            if (comment != m_speakingAiParsedComments.end())
            {
                comment->comment = asUtf8(editor.Text());
            }
            const std::wstring editedComment = editor.Text().c_str();
            const bool valid = !editedComment.empty()
                && editedComment.size() <= static_cast<std::size_t>(
                    classmngr::engine::SpeakingEvaluationCommentMaxLength
                    );
            statusText.Text(
                !valid
                    ? (editedComment.empty()
                        ? L"Comment is empty"
                        : L"Over 450 characters")
                    : (editedComment.size() < 100 ? L"Short comment" : L"Ready")
                );
            characterCountText.Text(std::to_wstring(editedComment.size()));
            check.IsEnabled(valid);
            if (!valid)
            {
                check.IsChecked(false);
            }
            updateSpeakingAiActions();
        });
        Grid::SetColumn(edit, 4);
        grid.Children().Append(edit);
        auto item = ListViewItem();
        item.Content(grid);
        item.IsTabStop(false);
        m_speakingAiBatchReviewList.Items().Append(item);
    }
}

void MainWindow::applySpeakingAiStudentComment()
{
    if (!m_speakingAiStatusText || !m_speakingAiResponseTextBox)
    {
        return;
    }
    if (m_speakingAiStudentRow < 0
        || m_speakingAiStudentRow >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
    {
        m_speakingAiStatusText.Text(
            L"Generate a student prompt and keep that row selected before applying a comment."
            );
        updateSpeakingAiActions();
        return;
    }

    std::wstring comment = asWString(m_speakingAiResponseTextBox.Text());
    if (comment.empty())
    {
        m_speakingAiStatusText.Text(
            L"Paste a completed student comment before applying it."
            );
        updateSpeakingAiActions();
        return;
    }
    const auto& cells = m_speakingEvaluationCellBoxes[
        static_cast<std::size_t>(m_speakingAiStudentRow)
        ];
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    const int commentsColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Comments
        );
    if (cells.size() <= static_cast<std::size_t>(commentsColumn))
    {
        return;
    }
    const std::wstring englishName = cells[static_cast<std::size_t>(
        englishColumn
        )].Text().c_str();
    const std::wstring koreanName = cells[static_cast<std::size_t>(
        koreanColumn
        )].Text().c_str();
    const std::wstring preferredName = englishName.empty()
        ? koreanName
        : englishName;
    replaceSpeakingAiPlaceholder(comment, preferredName);
    if (comment.size() > static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationCommentMaxLength
            ))
    {
        m_speakingAiStatusText.Text(
            L"The student comment is longer than the 450-character limit."
            );
        updateSpeakingAiActions();
        return;
    }
    m_speakingEvaluationCellBoxes[
        static_cast<std::size_t>(m_speakingAiStudentRow)
        ][static_cast<std::size_t>(commentsColumn)].Text(hstring(comment));
    m_speakingAiStatusText.Text(
        preferredName.empty()
            ? L"Student comment applied without a name placeholder. Save the evaluation to persist it."
            : L"Student comment applied. Save the evaluation to persist it."
        );
    updateSpeakingAiActions();
}

void MainWindow::applySpeakingAiBatchComments()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingAiStatusText)
    {
        return;
    }
    if (m_speakingAiBatchRows.empty() || m_speakingAiParsedComments.empty())
    {
        m_speakingAiStatusText.Text(
            L"Generate and parse a batch response before applying comments."
            );
        updateSpeakingAiActions();
        return;
    }

    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    const int commentsColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Comments
        );
    std::vector<std::string> selectedCommentIds;
    const bool hasReviewSelection = m_speakingAiBatchReviewList
        && m_speakingAiBatchReviewList.Items().Size() > 1;
    if (hasReviewSelection)
    {
        for (uint32_t index = 1;
             index < m_speakingAiBatchReviewList.Items().Size();
             ++index)
        {
            const auto item = m_speakingAiBatchReviewList.Items().GetAt(index)
                .try_as<ListViewItem>();
            const auto grid = item ? item.Content().try_as<Grid>() : nullptr;
            const auto check = grid && grid.Children().Size() > 0
                ? grid.Children().GetAt(0).try_as<CheckBox>()
                : nullptr;
            if (check)
            {
                const auto checked = check.IsChecked();
                if (checked && checked.Value())
                {
                    selectedCommentIds.push_back(asUtf8(
                        unbox_value<hstring>(check.Tag())
                        ));
                }
            }
        }
    }
    std::size_t applied = 0;
    std::size_t skipped = 0;
    for (const auto& parsed : m_speakingAiParsedComments)
    {
        if (hasReviewSelection
            && std::find(selectedCommentIds.begin(), selectedCommentIds.end(),
                         parsed.id) == selectedCommentIds.end())
        {
            continue;
        }
        const auto rowIt = std::find_if(
            m_speakingAiBatchRows.begin(),
            m_speakingAiBatchRows.end(),
            [&parsed](int row) {
                return speakingAiStudentId(static_cast<std::size_t>(row))
                    == parsed.id;
            }
            );
        if (rowIt == m_speakingAiBatchRows.end())
        {
            ++skipped;
            continue;
        }
        const int row = *rowIt;
        if (row < 0
            || row >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
        {
            ++skipped;
            continue;
        }
        const auto& cells = m_speakingEvaluationCellBoxes[
            static_cast<std::size_t>(row)
            ];
        if (cells.size() <= static_cast<std::size_t>(commentsColumn))
        {
            ++skipped;
            continue;
        }
        std::wstring comment = asWide(parsed.comment);
        const std::wstring englishName = cells[static_cast<std::size_t>(
            englishColumn
            )].Text().c_str();
        const std::wstring koreanName = cells[static_cast<std::size_t>(
            koreanColumn
            )].Text().c_str();
        replaceSpeakingAiPlaceholder(
            comment,
            englishName.empty() ? koreanName : englishName
            );
        if (comment.empty()
            || comment.size() > static_cast<std::size_t>(
                classmngr::engine::SpeakingEvaluationCommentMaxLength
                ))
        {
            ++skipped;
            continue;
        }
        cells[static_cast<std::size_t>(commentsColumn)].Text(hstring(comment));
        ++applied;
    }

    if (applied > 0)
    {
        markSpeakingEvaluationDirty();
    }

    m_speakingAiStatusText.Text(winrt::hstring(
        L"Applied " + std::to_wstring(applied)
            + L" parsed batch comments"
            + (skipped == 0
                ? L". Save the evaluation to persist them."
                : L"; " + std::to_wstring(skipped)
                    + L" were skipped as invalid. Save the rest to persist them.")
        ));
    updateSpeakingAiActions();
}

void MainWindow::updateSpeakingAiActions()
{
    if (!m_speakingAiStatusText)
    {
        return;
    }
    const bool hasClass = static_cast<bool>(m_openDatabase)
        && m_classSelectedId > 0
        && !m_classNew;
    const bool hasPrompt = m_speakingAiPromptTextBox
        && !m_speakingAiPromptTextBox.Text().empty();
    const bool hasResponse = m_speakingAiResponseTextBox
        && !m_speakingAiResponseTextBox.Text().empty();
    if (m_speakingAiVoiceSelector)
    {
        m_speakingAiVoiceSelector.IsEnabled(hasClass);
    }
    if (m_speakingAiDidWellTextBox)
    {
        m_speakingAiDidWellTextBox.IsEnabled(hasClass);
    }
    if (m_speakingAiNeedsImprovementTextBox)
    {
        m_speakingAiNeedsImprovementTextBox.IsEnabled(hasClass);
    }
    if (m_speakingAiGenerateButton)
    {
        m_speakingAiGenerateButton.IsEnabled(hasClass);
    }
    if (m_speakingAiGenerateBatchButton)
    {
        m_speakingAiGenerateBatchButton.IsEnabled(hasClass);
    }
    if (m_speakingAiPromptTextBox)
    {
        m_speakingAiPromptTextBox.IsEnabled(hasClass);
    }
    if (m_speakingAiCopyButton)
    {
        m_speakingAiCopyButton.IsEnabled(hasClass && hasPrompt);
    }
    if (m_speakingAiCopyOpenButton)
    {
        m_speakingAiCopyOpenButton.IsEnabled(hasClass && hasPrompt);
    }
    if (m_speakingAiResponseTextBox)
    {
        m_speakingAiResponseTextBox.IsEnabled(hasClass);
    }
    if (m_speakingAiApplyStudentButton)
    {
        m_speakingAiApplyStudentButton.IsEnabled(
            hasClass && m_speakingAiStudentRow >= 0 && hasResponse
            );
    }
    if (m_speakingAiParseBatchButton)
    {
        m_speakingAiParseBatchButton.IsEnabled(
            hasClass && !m_speakingAiBatchRows.empty() && hasResponse
            );
    }
    if (m_speakingAiApplyBatchButton)
    {
        m_speakingAiApplyBatchButton.IsEnabled(
            hasClass && !m_speakingAiParsedComments.empty()
            );
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
