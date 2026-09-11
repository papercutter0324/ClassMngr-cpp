#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

winrt::fire_and_forget MainWindow::openSpeakingReportEditor()
{
    auto lifetime = get_strong();
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (m_ownedDialog
        || !m_openDatabase
        || m_classSelectedId <= 0
        || m_classNew
        || !m_speakingEvaluationList
        || !RootGrid().XamlRoot())
    {
        co_return;
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
    const int notesColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Notes
        );
    const int firstScoreColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Grammar
        );

    std::vector<int> reportRows;
    for (int row = 0;
         row < static_cast<int>(m_speakingEvaluationCellBoxes.size());
         ++row)
    {
        const auto& cells = m_speakingEvaluationCellBoxes[
            static_cast<std::size_t>(row)
            ];
        if (cells.size() > static_cast<std::size_t>(koreanColumn)
            && (!cells[static_cast<std::size_t>(englishColumn)].Text().empty()
                || !cells[static_cast<std::size_t>(koreanColumn)].Text().empty()))
        {
            reportRows.push_back(row);
        }
    }
    if (reportRows.empty())
    {
        if (m_speakingEvaluationStatusText)
        {
            m_speakingEvaluationStatusText.Text(
                L"Import or enter a student name before opening the report editor."
                );
        }
        co_return;
    }

    auto content = StackPanel();
    content.Spacing(10.0);
    content.MaxWidth(1050.0);

    auto header = Grid();
    header.ColumnSpacing(10.0);
    auto labelColumn = ColumnDefinition();
    labelColumn.Width(GridLengthHelper::FromValueAndType(
        1.0,
        GridUnitType::Auto
        ));
    auto selectorColumn = ColumnDefinition();
    selectorColumn.Width(GridLengthHelper::FromValueAndType(
        1.0,
        GridUnitType::Star
        ));
    auto navigationColumn = ColumnDefinition();
    navigationColumn.Width(GridLengthHelper::FromValueAndType(
        1.0,
        GridUnitType::Auto
        ));
    header.ColumnDefinitions().Append(labelColumn);
    header.ColumnDefinitions().Append(selectorColumn);
    header.ColumnDefinitions().Append(navigationColumn);

    auto studentLabel = TextBlock();
    studentLabel.Text(L"Student:");
    studentLabel.VerticalAlignment(VerticalAlignment::Center);
    studentLabel.FontSize(18.0);
    Grid::SetColumn(studentLabel, 0);
    header.Children().Append(studentLabel);

    auto studentSelector = ComboBox();
    studentSelector.IsTabStop(true);
    studentSelector.MinWidth(360.0);
    setAutomationName(studentSelector, L"Speaking report editor student");
    for (const int row : reportRows)
    {
        const auto& cells = m_speakingEvaluationCellBoxes[
            static_cast<std::size_t>(row)
            ];
        std::wstring label = cells[static_cast<std::size_t>(englishColumn)].Text().c_str();
        const std::wstring korean = cells[
            static_cast<std::size_t>(koreanColumn)
            ].Text().c_str();
        if (!label.empty() && !korean.empty())
        {
            label += L" (" + korean + L")";
        }
        else if (label.empty())
        {
            label = korean;
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(label)));
        item.Tag(box_value(row));
        studentSelector.Items().Append(item);
    }
    Grid::SetColumn(studentSelector, 1);
    header.Children().Append(studentSelector);

    auto navigation = StackPanel();
    navigation.Orientation(Orientation::Horizontal);
    navigation.Spacing(8.0);
    auto previousButton = Button();
    previousButton.Content(box_value(hstring(L"Previous")));
    auto nextButton = Button();
    nextButton.Content(box_value(hstring(L"Next")));
    setAutomationName(previousButton, L"Previous speaking report");
    setAutomationName(nextButton, L"Next speaking report");
    navigation.Children().Append(previousButton);
    navigation.Children().Append(nextButton);
    Grid::SetColumn(navigation, 2);
    header.Children().Append(navigation);
    content.Children().Append(header);

    auto notesLabel = TextBlock();
    notesLabel.Text(L"Private Notes (not included in the report)");
    notesLabel.FontSize(16.0);
    content.Children().Append(notesLabel);

    auto notesGrid = Grid();
    notesGrid.ColumnSpacing(12.0);
    notesGrid.ColumnDefinitions().Append(ColumnDefinition());
    notesGrid.ColumnDefinitions().Append(ColumnDefinition());
    auto didWellField = TextBox();
    didWellField.Header(box_value(hstring(L"Did Well")));
    didWellField.PlaceholderText(L"\u2022 Add positive observations...");
    didWellField.AcceptsReturn(true);
    didWellField.TextWrapping(TextWrapping::Wrap);
    didWellField.Height(120.0);
    didWellField.IsTabStop(true);
    didWellField.TabIndex(1);
    setAutomationName(didWellField, L"Speaking report Did Well notes");
    Grid::SetColumn(didWellField, 0);
    notesGrid.Children().Append(didWellField);

    auto needsImprovementField = TextBox();
    needsImprovementField.Header(box_value(hstring(L"Needs Improvement")));
    needsImprovementField.PlaceholderText(L"\u2022 Add areas for improvement...");
    needsImprovementField.AcceptsReturn(true);
    needsImprovementField.TextWrapping(TextWrapping::Wrap);
    needsImprovementField.Height(120.0);
    needsImprovementField.IsTabStop(true);
    needsImprovementField.TabIndex(2);
    setAutomationName(
        needsImprovementField,
        L"Speaking report Needs Improvement notes"
        );
    Grid::SetColumn(needsImprovementField, 1);
    notesGrid.Children().Append(needsImprovementField);
    content.Children().Append(notesGrid);

    auto aiActions = StackPanel();
    aiActions.Orientation(Orientation::Horizontal);
    aiActions.Spacing(8.0);
    aiActions.HorizontalAlignment(HorizontalAlignment::Right);
    auto previewAiButton = Button();
    previewAiButton.Content(box_value(hstring(L"Preview AI Prompt")));
    previewAiButton.IsTabStop(true);
    previewAiButton.TabIndex(3);
    setAutomationName(previewAiButton, L"Preview speaking report AI prompt");
    aiActions.Children().Append(previewAiButton);
    auto copyOpenAiButton = Button();
    copyOpenAiButton.Content(box_value(hstring(L"Copy Prompt and Open ChatGPT")));
    copyOpenAiButton.IsTabStop(true);
    copyOpenAiButton.TabIndex(4);
    setAutomationName(copyOpenAiButton, L"Copy and open speaking report AI prompt");
    aiActions.Children().Append(copyOpenAiButton);
    content.Children().Append(aiActions);

    auto promptPreview = TextBox();
    promptPreview.Header(box_value(hstring(L"AI Prompt Preview")));
    promptPreview.AcceptsReturn(true);
    promptPreview.TextWrapping(TextWrapping::Wrap);
    promptPreview.IsReadOnly(true);
    promptPreview.Height(180.0);
    promptPreview.Visibility(Visibility::Collapsed);
    setAutomationName(promptPreview, L"Speaking report AI prompt preview");
    content.Children().Append(promptPreview);

    auto reportScroll = ScrollViewer();
    reportScroll.Height(430.0);
    reportScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    reportScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    auto reportSurface = Border();
    reportSurface.Padding(Thickness{28.0, 22.0, 28.0, 28.0});
    reportSurface.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
        Windows::UI::Color{255, 255, 255, 255}
        ));
    reportSurface.BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
        Windows::UI::Color{255, 92, 99, 108}
        ));
    reportSurface.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
    reportSurface.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
    auto report = StackPanel();
    report.Spacing(12.0);
    auto reportTitle = TextBlock();
    reportTitle.Text(L"Speaking Evaluation");
    reportTitle.FontSize(30.0);
    reportTitle.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
    reportTitle.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
        Windows::UI::Color{255, 184, 20, 25}
        ));
    reportTitle.HorizontalAlignment(HorizontalAlignment::Center);
    report.Children().Append(reportTitle);
    auto reportDetails = TextBlock();
    reportDetails.TextWrapping(TextWrapping::Wrap);
    reportDetails.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
        Windows::UI::Color{255, 0, 0, 0}
        ));
    setAutomationName(reportDetails, L"Speaking report preview details");
    report.Children().Append(reportDetails);

    auto scoreGrid = Grid();
    scoreGrid.ColumnSpacing(8.0);
    const std::array<wchar_t const*, 6> scoreNames{
        L"Grammar", L"Pronunciation", L"Fluency", L"Manner", L"Content",
        L"Overall Effort"
    };
    for (std::size_t index = 0; index < scoreNames.size(); ++index)
    {
        const int row = static_cast<int>(index / 2);
        const int column = static_cast<int>(index % 2);
        if (scoreGrid.RowDefinitions().Size() <= static_cast<uint32_t>(row))
        {
            scoreGrid.RowDefinitions().Append(RowDefinition());
        }
        if (scoreGrid.ColumnDefinitions().Size() <= static_cast<uint32_t>(column))
        {
            scoreGrid.ColumnDefinitions().Append(ColumnDefinition());
        }
        auto scoreField = TextBox();
        scoreField.Header(box_value(hstring(scoreNames[index])));
        scoreField.MaxLength(2);
        scoreField.IsTabStop(true);
        setAutomationName(
            scoreField,
            L"Speaking report " + std::wstring(scoreNames[index])
            );
        scoreField.TabIndex(10 + static_cast<int32_t>(index));
        Grid::SetRow(scoreField, row);
        Grid::SetColumn(scoreField, column);
        scoreGrid.Children().Append(scoreField);
    }
    report.Children().Append(scoreGrid);

    auto commentsField = TextBox();
    commentsField.Header(box_value(hstring(L"Comments")));
    commentsField.AcceptsReturn(true);
    commentsField.TextWrapping(TextWrapping::Wrap);
    commentsField.Height(120.0);
    commentsField.TabIndex(16);
    setAutomationName(commentsField, L"Speaking report comments");
    report.Children().Append(commentsField);
    reportSurface.Child(report);
    reportScroll.Content(reportSurface);
    content.Children().Append(reportScroll);

    auto status = TextBlock();
    status.TextWrapping(TextWrapping::Wrap);
    setAutomationName(status, L"Speaking report editor status");
    content.Children().Append(status);

    std::vector<TextBox> scoreFields;
    scoreFields.reserve(scoreNames.size());
    for (const auto& child : scoreGrid.Children())
    {
        scoreFields.push_back(child.as<TextBox>());
    }
    bool updating = false;
    const auto selectedRow = [&studentSelector]() {
        const auto item = studentSelector.SelectedItem().try_as<ComboBoxItem>();
        return item ? boxedInt(item.Tag()) : -1;
    };
    const auto currentAiPrompt = [&]() {
        const int row = selectedRow();
        if (row < 0 || row >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
        {
            return std::string{};
        }
        const auto& cells = m_speakingEvaluationCellBoxes[
            static_cast<std::size_t>(row)
            ];
        if (cells.size() <= static_cast<std::size_t>(notesColumn))
        {
            return std::string{};
        }

        classmngr::engine::SpeakingEvaluationAiPromptInput input;
        input.grade = classmngr::engine::SpeakingEvaluationReportModel::elementaryGrade(
            m_classInfo.classGrade
            );
        input.englishName = asUtf8(
            cells[static_cast<std::size_t>(englishColumn)].Text()
            );
        input.koreanName = asUtf8(
            cells[static_cast<std::size_t>(koreanColumn)].Text()
            );
        input.didWell = asUtf8(didWellField.Text());
        input.needsImprovement = asUtf8(needsImprovementField.Text());
        input.voice = m_speakingAiVoiceSelector
            && m_speakingAiVoiceSelector.SelectedIndex() == 1
            ? classmngr::engine::SpeakingEvaluationAiVoice::ThirdPerson
            : classmngr::engine::SpeakingEvaluationAiVoice::DirectToStudent;
        return classmngr::engine::SpeakingEvaluationAiPromptService::canBuildPrompt(
            input
            )
            ? classmngr::engine::SpeakingEvaluationAiPromptService::buildCommentPrompt(
                input
                )
            : std::string{};
    };
    const auto updateAiActions = [&]() {
        const bool enabled = !currentAiPrompt().empty();
        previewAiButton.IsEnabled(enabled);
        copyOpenAiButton.IsEnabled(enabled);
    };
    const auto refreshEditor = [&]() {
        const int row = selectedRow();
        if (row < 0 || row >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
        {
            return;
        }
        const auto& cells = m_speakingEvaluationCellBoxes[
            static_cast<std::size_t>(row)
            ];
        updating = true;
        for (std::size_t index = 0; index < scoreFields.size(); ++index)
        {
            scoreFields[index].Text(cells[
                static_cast<std::size_t>(firstScoreColumn)
                    + index
                ].Text());
        }
        commentsField.Text(cells[static_cast<std::size_t>(commentsColumn)].Text());
        const auto notes = splitSpeakingAiPrivateNotes(asUtf8(
            cells[static_cast<std::size_t>(notesColumn)].Text()
            ));
        didWellField.Text(asWide(bulletizeSpeakingAiNotes(notes.didWell)));
        needsImprovementField.Text(
            asWide(bulletizeSpeakingAiNotes(notes.needsImprovement))
            );
        updating = false;
        promptPreview.Text({});
        promptPreview.Visibility(Visibility::Collapsed);
        std::wstring name = cells[static_cast<std::size_t>(englishColumn)].Text().c_str();
        const std::wstring korean = cells[static_cast<std::size_t>(koreanColumn)].Text().c_str();
        if (!name.empty() && !korean.empty())
        {
            name += L" (" + korean + L")";
        }
        else if (name.empty())
        {
            name = korean;
        }
        reportDetails.Text(hstring(
            L"Student: " + name
            + L"\nClass: " + asWide(m_classInfo.classGrade)
            + L" " + asWide(m_classInfo.classLevel)
            + L"\nNative Teacher: " + asWide(m_classInfo.teacherEn)
            + L"\nKorean Teacher: " + asWide(m_classInfo.teacherKr)
            ));
        status.Text(L"Edit report values and private observations; save the evaluation to persist them.");
        updateAiActions();
    };

    studentSelector.SelectionChanged(
        [&refreshEditor](auto const&, auto const&) { refreshEditor(); }
        );
    previousButton.Click(
        [&studentSelector](auto const&, auto const&) {
            const int count = static_cast<int>(studentSelector.Items().Size());
            if (count > 1)
            {
                studentSelector.SelectedIndex(
                    (studentSelector.SelectedIndex() - 1 + count) % count
                    );
            }
        }
        );
    nextButton.Click(
        [&studentSelector](auto const&, auto const&) {
            const int count = static_cast<int>(studentSelector.Items().Size());
            if (count > 1)
            {
                studentSelector.SelectedIndex(
                    (studentSelector.SelectedIndex() + 1) % count
                    );
            }
        }
        );
    for (std::size_t index = 0; index < scoreFields.size(); ++index)
    {
        scoreFields[index].TextChanging(
            [this, &updating, &selectedRow, index, firstScoreColumn](
                TextBox const& sender,
                TextBoxTextChangingEventArgs const&)
            {
                if (updating)
                {
                    return;
                }
                const int row = selectedRow();
                if (row >= 0
                    && row < static_cast<int>(m_speakingEvaluationCellBoxes.size())
                    && m_speakingEvaluationCellBoxes[static_cast<std::size_t>(row)].size()
                        > static_cast<std::size_t>(firstScoreColumn + index))
                {
                    m_speakingEvaluationCellBoxes[static_cast<std::size_t>(row)][
                        static_cast<std::size_t>(firstScoreColumn) + index
                        ].Text(sender.Text());
                    markSpeakingEvaluationDirty();
                }
            }
            );
    }
    const auto connectEditorField = [this, &updating, &selectedRow](
        TextBox& field,
        int column
        ) {
        field.TextChanging(
            [this, &updating, &selectedRow, column](
                TextBox const& sender,
                TextBoxTextChangingEventArgs const&)
            {
                if (updating)
                {
                    return;
                }
                const int row = selectedRow();
                if (row >= 0
                    && row < static_cast<int>(m_speakingEvaluationCellBoxes.size())
                    && m_speakingEvaluationCellBoxes[static_cast<std::size_t>(row)].size()
                        > static_cast<std::size_t>(column))
                {
                    m_speakingEvaluationCellBoxes[static_cast<std::size_t>(row)][
                        static_cast<std::size_t>(column)
                        ].Text(sender.Text());
                    markSpeakingEvaluationDirty();
                }
            }
            );
    };
    connectEditorField(commentsField, commentsColumn);
    const auto savePrivateNotes = [this,
                                   &updating,
                                   &selectedRow,
                                   &didWellField,
                                   &needsImprovementField,
                                   notesColumn,
                                   &updateAiActions](TextBox const&,
                                                     TextBoxTextChangingEventArgs const&) {
        if (updating)
        {
            return;
        }
        const int row = selectedRow();
        if (row < 0 || row >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
        {
            return;
        }
        auto& cells = m_speakingEvaluationCellBoxes[static_cast<std::size_t>(row)];
        if (cells.size() <= static_cast<std::size_t>(notesColumn))
        {
            return;
        }
        cells[static_cast<std::size_t>(notesColumn)].Text(asWide(
            joinSpeakingAiPrivateNotes(
                bulletizeSpeakingAiNotes(asUtf8(didWellField.Text())),
                bulletizeSpeakingAiNotes(asUtf8(needsImprovementField.Text()))
                )
            ));
        markSpeakingEvaluationDirty();
        updateAiActions();
    };
    didWellField.TextChanging(savePrivateNotes);
    needsImprovementField.TextChanging(savePrivateNotes);
    previewAiButton.Click(
        [&currentAiPrompt, &promptPreview, &status](auto const&, auto const&) {
            const std::string prompt = currentAiPrompt();
            if (prompt.empty())
            {
                status.Text(
                    L"AI prompts require an E4-E6 class and at least one observation in both sections."
                    );
                return;
            }
            promptPreview.Text(asWide(prompt));
            promptPreview.Visibility(Visibility::Visible);
            status.Text(L"Review the privacy-preserving AI prompt before copying it.");
        }
        );
    copyOpenAiButton.Click(
        [&currentAiPrompt, &status](auto const&, auto const&) {
            const std::string prompt = currentAiPrompt();
            if (prompt.empty())
            {
                status.Text(
                    L"AI prompts require an E4-E6 class and at least one observation in both sections."
                    );
                return;
            }
            const auto copied = classmngr::windows::winui::WindowsClipboard::writeText(
                prompt
                );
            if (!copied)
            {
                status.Text(hstring(
                    L"The AI prompt could not be copied: "
                        + asWide(copied.error().message)
                    ));
                return;
            }
            const auto opened = classmngr::windows::winui::WindowsUrlLauncher::openUrl(
                "https://chatgpt.com/"
                );
            status.Text(
                opened
                    ? L"AI prompt copied; ChatGPT was opened for review."
                    : hstring(
                        L"AI prompt copied, but ChatGPT could not be opened: "
                            + asWide(opened.error().message)
                        )
                );
        }
        );

    studentSelector.SelectedIndex(0);
    refreshEditor();

    auto dialogScroll = ScrollViewer();
    dialogScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    dialogScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    dialogScroll.Content(content);

    auto dialog = ContentDialog();
    dialog.XamlRoot(RootGrid().XamlRoot());
    dialog.Title(box_value(hstring(L"Speaking Evaluation Reports")));
    dialog.Content(dialogScroll);
    dialog.PrimaryButtonText(L"Print");
    dialog.SecondaryButtonText(L"Save As PDF");
    dialog.CloseButtonText(L"Close");
    dialog.DefaultButton(ContentDialogButton::Primary);
    m_ownedDialog = dialog;

    ContentDialogResult result = ContentDialogResult::None;
    try
    {
        result = co_await dialog.ShowAsync();
    }
    catch (...)
    {
        result = ContentDialogResult::None;
    }
    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
    if (result == ContentDialogResult::Primary
        || result == ContentDialogResult::Secondary)
    {
        openSpeakingBatchReportDialog(result == ContentDialogResult::Secondary);
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
