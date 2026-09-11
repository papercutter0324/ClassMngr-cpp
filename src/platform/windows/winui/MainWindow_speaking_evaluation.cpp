#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::refreshSpeakingEvaluation()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingEvaluationStatusText
        || !m_speakingEvaluationHeaderGrid
        || !m_speakingEvaluationList)
    {
        return;
    }

    const auto emptyRows = []() {
        return classmngr::engine::SpeakingEvaluationRows(
            static_cast<std::size_t>(
                classmngr::engine::SpeakingEvaluationRowCount
                ),
            classmngr::engine::SpeakingEvaluationRow(
                static_cast<std::size_t>(
                    classmngr::engine::SpeakingEvaluationColumnCount
                    )
                )
            );
    };
    const auto clearControls = [this, &emptyRows]() {
        m_speakingEvaluationRows = emptyRows();
        m_speakingEvaluationDirtyCells.clear();
        m_speakingEvaluationDirty = false;
        m_speakingAiStudentRow = -1;
        m_speakingAiBatchRows.clear();
        m_speakingAiParsedComments.clear();
        if (m_speakingAiDidWellTextBox)
        {
            m_speakingAiDidWellTextBox.Text({});
        }
        if (m_speakingAiNeedsImprovementTextBox)
        {
            m_speakingAiNeedsImprovementTextBox.Text({});
        }
        if (m_speakingAiPromptTextBox)
        {
            m_speakingAiPromptTextBox.Text({});
        }
        if (m_speakingAiResponseTextBox)
        {
            m_speakingAiResponseTextBox.Text({});
        }
        if (m_speakingBatchStatusText)
        {
            m_speakingBatchStatusText.Text(
                L"Open or select a saved class before planning batch reports."
                );
        }
        m_speakingEvaluationLoading = true;
        rebuildSpeakingEvaluationGrid();
        m_speakingEvaluationLoading = false;
        m_speakingEvaluationValidationText.Text({});
        m_speakingEvaluationValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Collapsed
            );
    };

    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        clearControls();
        m_speakingEvaluationStatusText.Text(
            !m_openDatabase
                ? L"No database open."
                : L"Save the selected class before editing speaking evaluations."
            );
        if (!m_classDirty && !m_classRosterDirty)
        {
            m_dirtyState.markClean();
        }
        updateSpeakingEvaluationActions();
        return;
    }

    if (m_speakingEvaluationDirty)
    {
        updateSpeakingEvaluationActions();
        return;
    }

    m_speakingEvaluationLoading = true;
    classmngr::engine::SpeakingEvaluationPersistenceService service(
        *m_openDatabase
        );
    const auto loaded = service.load(
        m_classSelectedId,
        m_speakingEvaluationName.empty()
            ? std::string_view{"Winter"}
            : std::string_view(m_speakingEvaluationName)
        );
    if (!loaded)
    {
        m_speakingEvaluationRows = emptyRows();
        m_speakingAiStudentRow = -1;
        m_speakingAiBatchRows.clear();
        m_speakingAiParsedComments.clear();
        if (m_speakingAiDidWellTextBox)
        {
            m_speakingAiDidWellTextBox.Text({});
        }
        if (m_speakingAiNeedsImprovementTextBox)
        {
            m_speakingAiNeedsImprovementTextBox.Text({});
        }
        if (m_speakingAiPromptTextBox)
        {
            m_speakingAiPromptTextBox.Text({});
        }
        if (m_speakingAiResponseTextBox)
        {
            m_speakingAiResponseTextBox.Text({});
        }
        if (m_speakingBatchStatusText)
        {
            m_speakingBatchStatusText.Text(
                L"The batch report plan is unavailable until this evaluation loads."
                );
        }
        rebuildSpeakingEvaluationGrid();
        m_speakingEvaluationLoading = false;
        m_speakingEvaluationDirty = false;
        m_speakingEvaluationDirtyCells.clear();
        m_speakingEvaluationStatusText.Text(winrt::hstring(
            L"Speaking evaluation could not be loaded: "
                + asWide(loaded.error().message)
            ));
        m_speakingEvaluationValidationText.Text(winrt::hstring(
            L"Engine loading error: " + asWide(loaded.error().message)
            ));
        m_speakingEvaluationValidationText.Visibility(Visibility::Visible);
        updateSpeakingEvaluationActions();
        return;
    }

    m_speakingEvaluationRows = loaded->empty()
        ? emptyRows()
        : classmngr::engine::SpeakingEvaluationValidator::normalized(*loaded);
    m_speakingEvaluationRows.resize(
        static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationRowCount
            )
        );
    for (auto& row : m_speakingEvaluationRows)
    {
        row.resize(static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationColumnCount
            ));
    }
    m_speakingEvaluationDirtyCells.clear();
    m_speakingEvaluationDirty = false;
    m_speakingAiStudentRow = -1;
    m_speakingAiBatchRows.clear();
    m_speakingAiParsedComments.clear();
    if (m_speakingEvaluationName.empty())
    {
        m_speakingEvaluationName = "Winter";
    }
    for (int index = 0;
         index < static_cast<int>(m_speakingEvaluationSelector.Items().Size());
         ++index)
    {
        const auto item = m_speakingEvaluationSelector.Items().GetAt(index)
            .try_as<ComboBoxItem>();
        if (item && boxedString(item.Tag()) == asWide(m_speakingEvaluationName))
        {
            m_speakingEvaluationSelector.SelectedIndex(index);
            break;
        }
    }
    rebuildSpeakingEvaluationGrid();
    m_speakingEvaluationLoading = false;
    m_speakingEvaluationStatusText.Text(
        loaded->empty()
            ? L"No saved speaking evaluation; enter scores and save."
            : L""
        );
    m_speakingEvaluationValidationText.Text({});
    m_speakingEvaluationValidationText.Visibility(Visibility::Collapsed);
    if (!m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    updateSpeakingEvaluationActions();
    refreshSpeakingAiSelection();
    if (m_speakingBatchStatusText)
    {
        m_speakingBatchStatusText.Text(
            L"Choose an output mode and plan the named students in this evaluation."
            );
    }
    updateSpeakingBatchReportActions();
}

void MainWindow::rebuildSpeakingEvaluationGrid()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingEvaluationHeaderGrid || !m_speakingEvaluationList)
    {
        return;
    }

    constexpr std::array<double, classmngr::engine::SpeakingEvaluationColumnCount>
        widths{40.0, 180.0, 180.0, 150.0, 150.0, 150.0, 150.0, 150.0,
               150.0, 500.0, 300.0};
    constexpr std::array<wchar_t const*,
                         classmngr::engine::SpeakingEvaluationColumnCount>
        headers{L"#", L"English Name", L"Korean Name", L"Grammar",
                L"Pronunciation", L"Fluency", L"Manner", L"Content",
                L"Overall Effort", L"Comments", L"Notes"};
    const auto columnColor = [](int column, bool header) {
        const auto color = [header](std::uint8_t red,
                                    std::uint8_t green,
                                    std::uint8_t blue) {
            return Windows::UI::Color{
                255,
                static_cast<std::uint8_t>(header ? red * 0.9 : red),
                static_cast<std::uint8_t>(header ? green * 0.9 : green),
                static_cast<std::uint8_t>(header ? blue * 0.9 : blue)
            };
        };
        switch (column)
        {
        case 0:
            return color(217, 217, 217);
        case 3:
            return color(217, 210, 233);
        case 4:
        case 8:
            return color(207, 226, 243);
        case 5:
            return color(244, 204, 204);
        case 6:
            return color(252, 229, 205);
        case 7:
            return color(217, 234, 211);
        case 9:
            return color(238, 238, 238);
        case 10:
            return color(230, 224, 201);
        default:
            return color(255, 255, 255);
        }
    };
    const auto borderColor = Windows::UI::Color{255, 190, 198, 210};

    const bool wasLoading = m_speakingEvaluationLoading;
    m_speakingEvaluationLoading = true;
    m_speakingEvaluationHeaderGrid.ColumnDefinitions().Clear();
    m_speakingEvaluationHeaderGrid.Children().Clear();
    m_speakingEvaluationList.Items().Clear();
    m_speakingEvaluationList.SelectedIndex(-1);
    m_speakingEvaluationCellBoxes.clear();

    double totalWidth = 0.0;
    for (int column = 0;
         column < classmngr::engine::SpeakingEvaluationColumnCount;
         ++column)
    {
        totalWidth += widths[static_cast<std::size_t>(column)];
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            widths[static_cast<std::size_t>(column)],
            GridUnitType::Pixel
            ));
        m_speakingEvaluationHeaderGrid.ColumnDefinitions().Append(definition);

        auto header = Border();
        header.MinHeight(42.0);
        header.Padding(Thickness{6.0, 4.0, 6.0, 4.0});
        header.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
            columnColor(column, true)
            ));
        header.BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
            borderColor
            ));
        header.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        auto label = TextBlock();
        label.Text(headers[static_cast<std::size_t>(column)]);
        label.TextAlignment(TextAlignment::Center);
        label.VerticalAlignment(VerticalAlignment::Center);
        label.TextWrapping(TextWrapping::Wrap);
        label.FontSize(14.0);
        label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        label.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 31, 41, 55}
            ));
        header.Child(label);
        setAutomationName(
            header,
            L"Speaking evaluation header "
                + std::wstring(headers[static_cast<std::size_t>(column)])
            );
        Grid::SetColumn(header, column);
        m_speakingEvaluationHeaderGrid.Children().Append(header);
    }
    m_speakingEvaluationHeaderGrid.MinWidth(totalWidth);

    const std::size_t rowCount = std::min(
        m_speakingEvaluationRows.size(),
        static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationRowCount
            )
        );
    m_speakingEvaluationCellBoxes.reserve(rowCount);
    for (std::size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
    {
        auto rowGrid = Grid();
        rowGrid.ColumnSpacing(4.0);
        rowGrid.MinWidth(totalWidth);
        rowGrid.MinHeight(52.0);
        for (const double width : widths)
        {
            auto definition = ColumnDefinition();
            definition.Width(GridLengthHelper::FromValueAndType(
                width,
                GridUnitType::Pixel
                ));
            rowGrid.ColumnDefinitions().Append(definition);
        }

        std::vector<TextBox> rowBoxes;
        rowBoxes.reserve(
            static_cast<std::size_t>(
                classmngr::engine::SpeakingEvaluationColumnCount
                )
            );
        for (int column = 0;
             column < classmngr::engine::SpeakingEvaluationColumnCount;
             ++column)
        {
            auto cell = TextBox();
            cell.Width(widths[static_cast<std::size_t>(column)]);
            cell.MinHeight(48.0);
            cell.VerticalContentAlignment(VerticalAlignment::Center);
            cell.TextAlignment(TextAlignment::Center);
            cell.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                columnColor(column, false)
                ));
            cell.BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
                borderColor
                ));
            cell.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
            cell.Margin(Thickness{0.0, 2.0, 0.0, 2.0});
            cell.IsTabStop(column != 0);
            cell.TabIndex(
                10 + static_cast<int32_t>(
                    rowIndex * classmngr::engine::SpeakingEvaluationColumnCount
                        + static_cast<std::size_t>(column)
                    )
                );
            if (column == 0)
            {
                cell.Text(std::to_wstring(rowIndex + 1));
                cell.IsReadOnly(true);
            }
            else
            {
                const std::size_t rowSize = m_speakingEvaluationRows[rowIndex].size();
                cell.Text(
                    column < static_cast<int>(rowSize)
                        ? asWide(m_speakingEvaluationRows[rowIndex][
                            static_cast<std::size_t>(column)])
                        : std::wstring{}
                    );
                cell.MaxLength(
                    column == classmngr::engine::toInt(
                        classmngr::engine::SpeakingEvaluationColumn::Notes
                        )
                        ? static_cast<int32_t>(
                            classmngr::engine::SpeakingEvaluationMaximumNotesLength
                            )
                        : column == classmngr::engine::toInt(
                            classmngr::engine::SpeakingEvaluationColumn::Comments
                            )
                            ? classmngr::engine::SpeakingEvaluationCommentMaxLength
                            : 128
                    );
                if (column >= classmngr::engine::toInt(
                        classmngr::engine::SpeakingEvaluationColumn::Comments
                        ))
                {
                    cell.AcceptsReturn(true);
                    cell.TextWrapping(TextWrapping::Wrap);
                    cell.TextAlignment(TextAlignment::Left);
                }
                cell.TextChanging(
                    [this](TextBox const&, TextBoxTextChangingEventArgs const&) {
                        if (!m_speakingEvaluationLoading)
                        {
                            markSpeakingEvaluationDirty();
                        }
                    }
                    );
            }
            setAutomationName(
                cell,
                L"Speaking evaluation row " + std::to_wstring(rowIndex + 1)
                    + L" " + headers[static_cast<std::size_t>(column)]
                );
            Grid::SetColumn(cell, column);
            rowGrid.Children().Append(cell);
            rowBoxes.push_back(cell);
        }
        m_speakingEvaluationList.Items().Append(rowGrid);
        m_speakingEvaluationCellBoxes.push_back(std::move(rowBoxes));
    }
    m_speakingEvaluationLoading = wasLoading;
    updateSpeakingEvaluationActions();
}

void MainWindow::updateSpeakingEvaluationActions()
{
    if (!m_speakingEvaluationStatusText)
    {
        return;
    }

    const bool hasClass = static_cast<bool>(m_openDatabase)
        && m_classSelectedId > 0
        && !m_classNew;
    if (m_speakingEvaluationSelector)
    {
        m_speakingEvaluationSelector.IsEnabled(hasClass && !m_speakingEvaluationDirty);
    }
    if (m_speakingEvaluationList)
    {
        m_speakingEvaluationList.IsEnabled(hasClass);
    }
    if (m_speakingEvaluationPasteTextBox)
    {
        m_speakingEvaluationPasteTextBox.IsEnabled(hasClass);
    }
    if (m_speakingEvaluationImportNamesButton)
    {
        m_speakingEvaluationImportNamesButton.IsEnabled(hasClass);
    }
    const bool hasNamedStudents = std::any_of(
        m_speakingEvaluationCellBoxes.cbegin(),
        m_speakingEvaluationCellBoxes.cend(),
        [](const auto& cells) {
            const int englishColumn = classmngr::engine::toInt(
                classmngr::engine::SpeakingEvaluationColumn::EnglishName
                );
            const int koreanColumn = classmngr::engine::toInt(
                classmngr::engine::SpeakingEvaluationColumn::KoreanName
                );
            return cells.size() > static_cast<std::size_t>(koreanColumn)
                && (!cells[static_cast<std::size_t>(englishColumn)].Text().empty()
                    || !cells[static_cast<std::size_t>(koreanColumn)].Text().empty());
        }
        );
    if (m_speakingEvaluationReportEditorButton)
    {
        m_speakingEvaluationReportEditorButton.IsEnabled(
            hasClass && hasNamedStudents
            );
    }
    if (m_speakingEvaluationGenerateCommentsButton)
    {
        m_speakingEvaluationGenerateCommentsButton.IsEnabled(
            hasClass && hasNamedStudents
            );
    }
    if (m_speakingEvaluationPasteButton)
    {
        m_speakingEvaluationPasteButton.IsEnabled(hasClass);
    }
    if (m_speakingEvaluationSaveButton)
    {
        m_speakingEvaluationSaveButton.IsEnabled(hasClass && m_speakingEvaluationDirty);
    }
    if (m_speakingEvaluationDiscardButton)
    {
        m_speakingEvaluationDiscardButton.IsEnabled(hasClass && m_speakingEvaluationDirty);
    }
    updateSpeakingAiActions();
    updateSpeakingBatchReportActions();
}

void MainWindow::refreshSpeakingAiSelection()
{
    if (!m_speakingEvaluationList
        || !m_speakingAiDidWellTextBox
        || !m_speakingAiNeedsImprovementTextBox
        || !m_speakingAiPromptTextBox
        || !m_speakingAiResponseTextBox
        || !m_speakingAiStatusText)
    {
        return;
    }
    if (m_speakingEvaluationLoading)
    {
        return;
    }

    const int selectedIndex = m_speakingEvaluationList.SelectedIndex();
    if (selectedIndex < 0
        || selectedIndex >= static_cast<int>(m_speakingEvaluationCellBoxes.size())
        || m_speakingEvaluationCellBoxes[static_cast<std::size_t>(selectedIndex)]
            .size() <= static_cast<std::size_t>(
                classmngr::engine::toInt(
                    classmngr::engine::SpeakingEvaluationColumn::Notes
                    )
                ))
    {
        m_speakingAiStudentRow = -1;
        m_speakingAiDidWellTextBox.Text({});
        m_speakingAiNeedsImprovementTextBox.Text({});
        m_speakingAiPromptTextBox.Text({});
        m_speakingAiResponseTextBox.Text({});
        m_speakingAiBatchRows.clear();
        m_speakingAiParsedComments.clear();
        m_speakingAiStatusText.Text(
            L"Select a speaking-evaluation row to prepare an AI comment."
            );
        updateSpeakingAiActions();
        return;
    }

    const auto& cells = m_speakingEvaluationCellBoxes[
        static_cast<std::size_t>(selectedIndex)
        ];
    const auto notes = splitSpeakingAiPrivateNotes(asUtf8(
        cells[static_cast<std::size_t>(
            classmngr::engine::toInt(
                classmngr::engine::SpeakingEvaluationColumn::Notes
                )
            )].Text()
        ));
    m_speakingAiStudentRow = selectedIndex;
    m_speakingAiDidWellTextBox.Text(asWide(notes.didWell));
    m_speakingAiNeedsImprovementTextBox.Text(asWide(notes.needsImprovement));
    m_speakingAiPromptTextBox.Text({});
    m_speakingAiResponseTextBox.Text({});
    m_speakingAiBatchRows.clear();
    m_speakingAiParsedComments.clear();
    m_speakingAiStatusText.Text({});
    updateSpeakingAiActions();
    if (m_speakingBatchStatusText)
    {
        m_speakingBatchStatusText.Text(
            L"Choose an output mode and plan the named students in this evaluation."
            );
    }
    updateSpeakingBatchReportActions();
}

} // namespace winrt::ClassMngrWinUI::implementation
