#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::planSpeakingBatchReports()
{
    if (!m_speakingBatchStatusText
        || !m_speakingBatchRendererSelector
        || !m_speakingBatchTemplateSelector
        || !m_speakingBatchSavePdfCheck
        || !m_speakingBatchPrintCheck
        || !m_speakingBatchKeepIndividualPdfsCheck
        || !m_speakingBatchOutputDirectoryTextBox)
    {
        return;
    }

    const bool hasClass = static_cast<bool>(m_openDatabase)
        && m_classSelectedId > 0
        && !m_classNew;
    if (!hasClass)
    {
        m_speakingBatchStatusText.Text(
            L"Open or select a saved class before planning batch reports."
            );
        updateSpeakingBatchReportActions();
        return;
    }

    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    std::size_t reportCount = 0;
    for (const auto& cells : m_speakingEvaluationCellBoxes)
    {
        if (cells.size() <= static_cast<std::size_t>(koreanColumn))
        {
            continue;
        }
        if (!cells[static_cast<std::size_t>(englishColumn)].Text().empty()
            || !cells[static_cast<std::size_t>(koreanColumn)].Text().empty())
        {
            ++reportCount;
        }
    }

    if (reportCount == 0)
    {
        m_speakingBatchStatusText.Text(
            L"Import or enter at least one student name before planning reports."
            );
        updateSpeakingBatchReportActions();
        return;
    }

    const auto checked = [](Microsoft::UI::Xaml::Controls::CheckBox const& box) {
        const auto value = box.IsChecked();
        return value && value.Value();
    };
    classmngr::engine::SpeakingEvaluationBatchReportRequest request;
    request.reportCount = reportCount;
    request.renderer = m_speakingBatchRendererSelector.SelectedIndex() == 1
        ? classmngr::engine::SpeakingEvaluationReportRenderer::PowerPoint
        : classmngr::engine::SpeakingEvaluationReportRenderer::Internal;
    request.savePdf = checked(m_speakingBatchSavePdfCheck);
    request.printReports = checked(m_speakingBatchPrintCheck);
    request.keepIndividualPdfFiles =
        checked(m_speakingBatchKeepIndividualPdfsCheck);
    request.hasOutputDirectory =
        !asUtf8(m_speakingBatchOutputDirectoryTextBox.Text()).empty();
    request.hasExactOutputFilePath = false;
    const auto reportTemplate = m_speakingBatchTemplateSelector.SelectedIndex() == 1
        ? classmngr::engine::SpeakingEvaluationReportTemplate::Advanced
        : classmngr::engine::SpeakingEvaluationReportTemplate::Standard;
    request.reportTemplates.assign(reportCount, reportTemplate);

    const auto planned =
        classmngr::engine::SpeakingEvaluationBatchReportPolicy::plan(request);
    if (!planned)
    {
        m_speakingBatchStatusText.Text(winrt::hstring(
            L"Batch report plan rejected: "
                + asWide(planned.error().message)
            ));
        updateSpeakingBatchReportActions();
        return;
    }

    const std::wstring renderer = request.renderer
        == classmngr::engine::SpeakingEvaluationReportRenderer::PowerPoint
        ? L"PowerPoint"
        : L"Internal";
    std::wstring status = L"Planned "
        + std::to_wstring(reportCount)
        + L" speaking report(s) using the "
        + renderer
        + L" renderer. ";
    if (planned->createsBatchArchive)
    {
        status += L"The output phase will create one ZIP archive";
        if (planned->savesIndividualPdfFiles)
        {
            status += L" and retain individual PDFs";
        }
        status += L".";
    }
    else if (planned->savesIndividualPdfFiles)
    {
        status += L"The output phase will save individual PDF files.";
    }
    else
    {
        status += L"No PDF files are required.";
    }
    if (request.printReports)
    {
        status += L" Printing is also requested.";
    }
    status += L" Renderer-neutral plan accepted; output execution remains in Phase 7.";
    m_speakingBatchStatusText.Text(winrt::hstring(status));
    updateSpeakingBatchReportActions();
}

void MainWindow::updateSpeakingBatchReportActions()
{
    if (!m_speakingBatchStatusText
        || !m_speakingBatchRendererSelector
        || !m_speakingBatchTemplateSelector
        || !m_speakingBatchSavePdfCheck
        || !m_speakingBatchPrintCheck
        || !m_speakingBatchKeepIndividualPdfsCheck
        || !m_speakingBatchOutputDirectoryTextBox
        || !m_speakingBatchPlanButton)
    {
        return;
    }

    const bool hasClass = static_cast<bool>(m_openDatabase)
        && m_classSelectedId > 0
        && !m_classNew;
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    std::size_t reportCount = 0;
    for (const auto& cells : m_speakingEvaluationCellBoxes)
    {
        if (cells.size() <= static_cast<std::size_t>(koreanColumn))
        {
            continue;
        }
        if (!cells[static_cast<std::size_t>(englishColumn)].Text().empty()
            || !cells[static_cast<std::size_t>(koreanColumn)].Text().empty())
        {
            ++reportCount;
        }
    }

    const auto checked = [](Microsoft::UI::Xaml::Controls::CheckBox const& box) {
        const auto value = box.IsChecked();
        return value && value.Value();
    };
    const bool savePdf = checked(m_speakingBatchSavePdfCheck);
    const bool printReports = checked(m_speakingBatchPrintCheck);
    m_speakingBatchRendererSelector.IsEnabled(hasClass);
    m_speakingBatchTemplateSelector.IsEnabled(hasClass);
    m_speakingBatchSavePdfCheck.IsEnabled(hasClass);
    m_speakingBatchPrintCheck.IsEnabled(hasClass);
    m_speakingBatchKeepIndividualPdfsCheck.IsEnabled(
        hasClass && savePdf && reportCount > 1
        );
    m_speakingBatchOutputDirectoryTextBox.IsEnabled(hasClass && savePdf);
    m_speakingBatchPlanButton.IsEnabled(
        hasClass && reportCount > 0 && (savePdf || printReports)
        );
    if (m_speakingBatchChooseOutputButton)
    {
        m_speakingBatchChooseOutputButton.IsEnabled(hasClass && savePdf);
    }
}

void MainWindow::markSpeakingEvaluationDirty()
{
    if (m_speakingEvaluationLoading || !m_openDatabase
        || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }

    m_speakingEvaluationDirty = true;
    m_dirtyState.markDirty();
    if (m_speakingEvaluationStatusText)
    {
        m_speakingEvaluationStatusText.Text(
            L"Unsaved speaking evaluation changes."
            );
    }
    updateSpeakingEvaluationActions();
    updateClassActions();
}

void MainWindow::clearSpeakingEvaluationDirty()
{
    m_speakingEvaluationDirty = false;
    m_speakingEvaluationDirtyCells.clear();
    if (!m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    updateSpeakingEvaluationActions();
    updateClassActions();
}

classmngr::engine::SpeakingEvaluationRows
MainWindow::speakingEvaluationFromForm() const
{
    classmngr::engine::SpeakingEvaluationRows rows =
        m_speakingEvaluationRows;
    rows.resize(static_cast<std::size_t>(
        classmngr::engine::SpeakingEvaluationRowCount
        ));
    for (auto& row : rows)
    {
        row.resize(static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationColumnCount
            ));
    }
    for (std::size_t rowIndex = 0;
         rowIndex < rows.size() && rowIndex < m_speakingEvaluationCellBoxes.size();
         ++rowIndex)
    {
        const auto& rowBoxes = m_speakingEvaluationCellBoxes[rowIndex];
        for (std::size_t column = 1;
             column < rows[rowIndex].size() && column < rowBoxes.size();
             ++column)
        {
            rows[rowIndex][column] = asUtf8(rowBoxes[column].Text());
        }
    }
    return rows;
}

void MainWindow::saveSpeakingEvaluation()
{
    if (!m_openDatabase)
    {
        m_speakingEvaluationStatusText.Text(L"No database open.");
        return;
    }
    if (m_classSelectedId <= 0 || m_classNew)
    {
        m_speakingEvaluationStatusText.Text(
            L"Select and save a class before saving its speaking evaluation."
            );
        return;
    }

    classmngr::engine::SpeakingEvaluationRows normalized =
        classmngr::engine::SpeakingEvaluationValidator::normalized(
            speakingEvaluationFromForm()
            );
    const auto validation = classmngr::engine::SpeakingEvaluationValidator::validate(
        m_classSelectedId,
        m_speakingEvaluationName,
        normalized
        );
    if (validation.hasErrors())
    {
        std::wstring summary = L"Engine validation failed:";
        for (const auto& issue : validation.errors())
        {
            summary += L"\n- " + asWide(issue.code);
            if (issue.row >= 0)
            {
                summary += L" (row " + std::to_wstring(issue.row + 1);
                if (issue.column >= 0)
                {
                    summary += L", column " + std::to_wstring(issue.column + 1);
                }
                summary += L")";
            }
        }
        m_speakingEvaluationStatusText.Text(
            L"Speaking evaluation could not be saved."
            );
        m_speakingEvaluationValidationText.Text(winrt::hstring(summary));
        m_speakingEvaluationValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_speakingEvaluationDirty = true;
        m_dirtyState.markDirty();
        updateSpeakingEvaluationActions();
        return;
    }

    m_speakingEvaluationDirtyCells.clear();
    for (std::size_t row = 0; row < normalized.size(); ++row)
    {
        for (std::size_t column = 0; column < normalized[row].size(); ++column)
        {
            const std::string oldValue = row < m_speakingEvaluationRows.size()
                && column < m_speakingEvaluationRows[row].size()
                ? m_speakingEvaluationRows[row][column]
                : std::string{};
            if (oldValue != normalized[row][column])
            {
                m_speakingEvaluationDirtyCells.push_back({
                    static_cast<int>(row),
                    static_cast<int>(column)
                });
            }
        }
    }

    classmngr::engine::SpeakingEvaluationPersistenceService service(
        *m_openDatabase
        );
    const auto saved = service.save(
        m_classSelectedId,
        m_speakingEvaluationName,
        normalized,
        m_speakingEvaluationDirtyCells
        );
    if (!saved)
    {
        m_speakingEvaluationStatusText.Text(winrt::hstring(
            L"Speaking evaluation could not be saved: "
                + asWide(saved.error().message)
            ));
        m_speakingEvaluationValidationText.Text(winrt::hstring(
            L"Engine persistence error: " + asWide(saved.error().message)
            ));
        m_speakingEvaluationValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_speakingEvaluationDirty = true;
        m_dirtyState.markDirty();
        updateSpeakingEvaluationActions();
        return;
    }

    m_speakingEvaluationRows = std::move(normalized);
    m_speakingEvaluationLoading = true;
    rebuildSpeakingEvaluationGrid();
    m_speakingEvaluationLoading = false;
    clearSpeakingEvaluationDirty();
    m_speakingEvaluationStatusText.Text(L"Speaking evaluation saved.");
    m_speakingEvaluationValidationText.Text({});
    m_speakingEvaluationValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    refreshSpeakingAnalytics();
}

void MainWindow::discardSpeakingEvaluation()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }

    m_speakingEvaluationDirty = false;
    refreshSpeakingEvaluation();
    m_speakingEvaluationStatusText.Text(L"Speaking evaluation changes discarded.");
}

void MainWindow::importSpeakingEvaluationNames()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }

    classmngr::engine::RosterService service(*m_openDatabase);
    const auto loaded = service.load(m_classSelectedId);
    if (!loaded)
    {
        m_speakingEvaluationStatusText.Text(winrt::hstring(
            L"Roster names could not be imported: "
                + asWide(loaded.error().message)
            ));
        return;
    }

    const auto columnIndex = [](const std::vector<std::string>& columns,
                                std::string_view expected) {
        for (std::size_t index = 0; index < columns.size(); ++index)
        {
            if (columns[index].size() != expected.size())
            {
                continue;
            }
            bool matches = true;
            for (std::size_t character = 0; character < expected.size(); ++character)
            {
                const unsigned char actual = static_cast<unsigned char>(
                    columns[index][character]
                    );
                const unsigned char wanted = static_cast<unsigned char>(
                    expected[character]
                    );
                if (std::tolower(actual) != std::tolower(wanted))
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
            {
                return static_cast<int>(index);
            }
        }
        return -1;
    };
    const int englishColumn = columnIndex(loaded->columns, "English");
    const int koreanColumn = columnIndex(loaded->columns, "Korean");
    if (englishColumn < 0 || koreanColumn < 0)
    {
        m_speakingEvaluationStatusText.Text(
            L"Roster must contain English and Korean columns before names can be imported."
            );
        return;
    }

    classmngr::engine::SpeakingEvaluationRows importedRows =
        classmngr::engine::SpeakingEvaluationValidator::normalized(
            speakingEvaluationFromForm()
            );
    bool changed = false;
    const std::size_t rowCount = std::min(
        loaded->rows.size(),
        static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationRowCount
            )
        );
    for (std::size_t row = 0; row < rowCount; ++row)
    {
        const auto& source = loaded->rows[row];
        const std::string english = englishColumn < static_cast<int>(source.size())
            ? source[static_cast<std::size_t>(englishColumn)]
            : std::string{};
        const std::string korean = koreanColumn < static_cast<int>(source.size())
            ? source[static_cast<std::size_t>(koreanColumn)]
            : std::string{};
        if (importedRows[row][1] != english
            || importedRows[row][2] != korean)
        {
            changed = true;
        }
        importedRows[row][1] = english;
        importedRows[row][2] = korean;
    }
    if (!changed)
    {
        m_speakingEvaluationStatusText.Text(
            L"Roster names are already up to date."
            );
        return;
    }

    m_speakingEvaluationLoading = true;
    for (std::size_t row = 0;
         row < rowCount && row < m_speakingEvaluationCellBoxes.size();
         ++row)
    {
        auto& rowBoxes = m_speakingEvaluationCellBoxes[row];
        if (rowBoxes.size() > 2)
        {
            rowBoxes[1].Text(asWide(importedRows[row][1]));
            rowBoxes[2].Text(asWide(importedRows[row][2]));
        }
    }
    m_speakingEvaluationLoading = false;
    markSpeakingEvaluationDirty();
    m_speakingEvaluationStatusText.Text(
        L"Roster names imported into the speaking evaluation. Save to persist them."
        );
}

void MainWindow::applySpeakingEvaluationPaste()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew
        || !m_speakingEvaluationPasteTextBox)
    {
        return;
    }

    const auto rows = parsePastedRange(std::wstring_view(
        m_speakingEvaluationPasteTextBox.Text().c_str(),
        m_speakingEvaluationPasteTextBox.Text().size()
        ));
    if (rows.empty())
    {
        m_speakingEvaluationStatusText.Text(
            L"Paste a tab/newline range of score values first."
            );
        return;
    }

    const int selectedRow = m_speakingEvaluationList
        ? m_speakingEvaluationList.SelectedIndex()
        : -1;
    const std::size_t startRow = selectedRow >= 0
        ? static_cast<std::size_t>(selectedRow)
        : 0;
    constexpr std::size_t firstScoreColumn = static_cast<std::size_t>(
        classmngr::engine::toInt(
            classmngr::engine::SpeakingEvaluationColumn::Grammar
            )
        );
    constexpr std::size_t lastScoreColumn = static_cast<std::size_t>(
        classmngr::engine::toInt(
            classmngr::engine::SpeakingEvaluationColumn::OverallEffort
            )
        );
    std::size_t applied = 0;
    m_speakingEvaluationLoading = true;
    for (std::size_t row = 0;
         row < rows.size() && startRow + row < m_speakingEvaluationCellBoxes.size();
         ++row)
    {
        for (std::size_t column = 0;
             column < rows[row].size()
             && firstScoreColumn + column <= lastScoreColumn;
             ++column)
        {
            const std::size_t targetColumn = firstScoreColumn + column;
            m_speakingEvaluationCellBoxes[startRow + row][targetColumn].Text(
                rows[row][column]
                );
            ++applied;
        }
    }
    m_speakingEvaluationLoading = false;
    if (applied == 0)
    {
        m_speakingEvaluationStatusText.Text(
            L"The pasted range did not contain any score cells."
            );
        return;
    }
    markSpeakingEvaluationDirty();
    m_speakingEvaluationStatusText.Text(winrt::hstring(
        L"Applied " + std::to_wstring(applied)
            + L" score cells starting at row " + std::to_wstring(startRow + 1)
            + L". Save to persist them."
    ));
}

winrt::fire_and_forget MainWindow::openSpeakingAiDialog()
{
    auto lifetime = get_strong();
    if (!m_speakingAiDialogRoot
        || m_ownedDialog
        || !RootGrid().XamlRoot())
    {
        co_return;
    }

    refreshSpeakingAiSelection();
    m_speakingAiBatchRows.clear();
    m_speakingAiParsedComments.clear();
    m_speakingAiPromptTextBox.Text({});
    m_speakingAiResponseTextBox.Text({});
    if (m_speakingAiParseSummary)
    {
        m_speakingAiParseSummary.Text({});
    }
    refreshSpeakingAiBatchSelection();
    rebuildSpeakingAiBatchReview();
    updateSpeakingAiActions();

    auto dialog = Microsoft::UI::Xaml::Controls::ContentDialog();
    dialog.XamlRoot(RootGrid().XamlRoot());
    dialog.Title(box_value(hstring(L"Generate Class Comments")));
    dialog.Content(m_speakingAiDialogRoot);
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(
        Microsoft::UI::Xaml::Controls::ContentDialogButton::Close
        );
    m_ownedDialog = dialog;

    try
    {
        static_cast<void>(co_await dialog.ShowAsync());
    }
    catch (...)
    {
        // Dialog cancellation during navigation or shell teardown is normal.
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
}

winrt::fire_and_forget MainWindow::openSpeakingBatchReportDialog(bool saveMode)
{
    auto lifetime = get_strong();
    if (!m_speakingBatchDialogRoot
        || m_ownedDialog
        || !RootGrid().XamlRoot())
    {
        co_return;
    }

    if (m_speakingBatchSavePdfCheck)
    {
        m_speakingBatchSavePdfCheck.IsChecked(saveMode);
    }
    if (m_speakingBatchPrintCheck)
    {
        m_speakingBatchPrintCheck.IsChecked(!saveMode);
    }
    updateSpeakingBatchReportActions();

    auto dialog = Microsoft::UI::Xaml::Controls::ContentDialog();
    dialog.XamlRoot(RootGrid().XamlRoot());
    dialog.Title(box_value(hstring(
        saveMode
            ? L"Save Speaking Reports As"
            : L"Print Speaking Reports"
        )));
    dialog.Content(m_speakingBatchDialogRoot);
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(
        Microsoft::UI::Xaml::Controls::ContentDialogButton::Close
        );
    m_ownedDialog = dialog;

    try
    {
        static_cast<void>(co_await dialog.ShowAsync());
    }
    catch (...)
    {
        // Dialog cancellation during navigation or shell teardown is normal.
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
}

winrt::fire_and_forget MainWindow::chooseSpeakingBatchOutputDirectory()
{
    auto lifetime = get_strong();
    if (m_filePickerActive
        || !m_speakingBatchOutputDirectoryTextBox)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FolderPicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            m_speakingBatchStatusText.Text(
                L"The output-folder picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.SuggestedStartLocation(
                winrt::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary
                );
            picker.FileTypeFilter().Append(L"*");
            const auto folder = co_await picker.PickSingleFolderAsync();
            if (folder)
            {
                m_speakingBatchOutputDirectoryTextBox.Text(folder.Path());
                updateSpeakingBatchReportActions();
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        m_speakingBatchStatusText.Text(winrt::hstring(
            L"The output-folder picker failed: "
                + asWide(winrt::to_string(error.message()))
            ));
    }
    catch (...)
    {
        m_speakingBatchStatusText.Text(
            L"The output-folder picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

} // namespace winrt::ClassMngrWinUI::implementation
