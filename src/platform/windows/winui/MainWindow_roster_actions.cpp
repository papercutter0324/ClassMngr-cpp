#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

Microsoft::UI::Xaml::Controls::MenuFlyout
MainWindow::createClassRosterContextMenu(int row)
{
    using namespace Microsoft::UI::Xaml::Controls;

    MenuFlyout menu;
    if (row < 0 || row >= static_cast<int>(m_classRoster.rows.size()))
    {
        return menu;
    }

    const bool canRemove = classmngr::engine::isRosterStudentRow(
        m_classRoster,
        m_classRoster.rows[static_cast<std::size_t>(row)]
        );

    auto remove = MenuFlyoutItem();
    remove.Text(L"Remove Student");
    remove.IsEnabled(canRemove);
    setAutomationName(remove, L"Remove roster student");
    remove.Click(
        [this, row](auto const&, auto const&) {
            removeClassRosterRow(row);
        }
        );
    menu.Items().Append(remove);

    auto transfer = MenuFlyoutSubItem();
    transfer.Text(L"Transfer Class");
    transfer.IsEnabled(canRemove);
    setAutomationName(transfer, L"Transfer roster student to another class");
    if (m_classRosterTransferTargets.empty())
    {
        auto empty = MenuFlyoutItem();
        empty.Text(L"No same-grade classes");
        empty.IsEnabled(false);
        setAutomationName(empty, L"No same-grade roster classes");
        transfer.Items().Append(empty);
    }
    else
    {
        for (const ClassRosterTransferTarget& target :
             m_classRosterTransferTargets)
        {
            auto targetItem = MenuFlyoutItem();
            std::wstring label = target.label;
            if (target.full)
            {
                label += L" (full)";
            }
            targetItem.Text(winrt::hstring(label));
            targetItem.IsEnabled(canRemove && !target.full);
            setAutomationName(targetItem, L"Transfer roster student to " + label);
            if (!target.full)
            {
                targetItem.Click(
                    [this, row, targetId = target.classId](auto const&, auto const&) {
                        transferClassRosterRow(row, targetId);
                    }
                    );
            }
            transfer.Items().Append(targetItem);
        }
    }
    menu.Items().Append(transfer);
    return menu;
}

void MainWindow::updateClassRosterActions()
{
    if (!m_classRosterStatusText)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const bool hasClass = hasDatabase && m_classSelectedId > 0 && !m_classNew;
    const bool hasCurrentColumn = m_classRosterCurrentColumn >= 0
        && m_classRosterCurrentColumn
            < static_cast<int>(m_classRoster.columns.size());

    if (m_classRosterList)
    {
        m_classRosterList.IsEnabled(hasClass);
    }
    if (m_classRosterImportScoresButton)
    {
        m_classRosterImportScoresButton.IsEnabled(hasClass);
    }
    if (m_classRosterAddColumnButton)
    {
        m_classRosterAddColumnButton.IsEnabled(hasClass);
    }
    if (m_classRosterRemoveColumnButton)
    {
        m_classRosterRemoveColumnButton.IsEnabled(hasClass && hasCurrentColumn);
    }
}

void MainWindow::markClassRosterDirty()
{
    if (m_classRosterLoading || !m_openDatabase || m_classSelectedId <= 0)
    {
        return;
    }

    m_classRosterDirty = true;
    m_dirtyState.markDirty();
    if (m_classRosterStatusText)
    {
        m_classRosterStatusText.Text(L"Unsaved roster changes.");
    }
    updateClassRosterActions();
    updateClassActions();
}

void MainWindow::clearClassRosterDirty()
{
    m_classRosterDirty = false;
    if (!m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    updateClassRosterActions();
}

void MainWindow::saveClassRoster(bool rebuildGrid)
{
    if (!m_openDatabase)
    {
        m_classRosterStatusText.Text(L"No database open.");
        return;
    }
    if (m_classSelectedId <= 0 || m_classNew)
    {
        m_classRosterStatusText.Text(L"Select and save a class before saving its roster.");
        return;
    }

    classmngr::engine::Roster normalized =
        classmngr::engine::RosterValidator::normalized(classRosterFromForm());
    const auto validation = classmngr::engine::RosterValidator::validate(
        normalized
        );
    if (validation.hasErrors())
    {
        std::wstring summary = L"Engine validation failed:";
        for (const auto& issue : validation.errors())
        {
            summary += L"\n- " + asWide(issue.code);
            if (!issue.field.empty())
            {
                summary += L" (" + asWide(issue.field) + L")";
            }
        }
        m_classRosterStatusText.Text(L"Roster could not be saved.");
        m_classRosterValidationText.Text(winrt::hstring(summary));
        m_classRosterValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classRosterDirty = true;
        m_dirtyState.markDirty();
        updateClassRosterActions();
        return;
    }

    classmngr::engine::RosterService service(*m_openDatabase);
    const auto saved = service.save(m_classSelectedId, normalized);
    if (!saved)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Roster could not be saved: " + asWide(saved.error().message)
            ));
        m_classRosterValidationText.Text(winrt::hstring(
            L"Engine persistence error: " + asWide(saved.error().message)
            ));
        m_classRosterValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classRosterDirty = true;
        m_dirtyState.markDirty();
        updateClassRosterActions();
        return;
    }

    m_classRoster = std::move(normalized);
    if (rebuildGrid)
    {
        m_classRosterLoading = true;
        rebuildClassRosterGrid();
        m_classRosterLoading = false;
    }
    clearClassRosterDirty();
    m_classRosterStatusText.Text(rebuildGrid ? L"Roster saved." : L"");
    m_classRosterValidationText.Text({});
    m_classRosterValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    updateClassActions();
}

void MainWindow::discardClassRoster()
{
    if (!m_openDatabase || m_classSelectedId <= 0)
    {
        return;
    }
    refreshClassRoster();
    m_classRosterStatusText.Text(L"Roster changes discarded.");
}

winrt::fire_and_forget MainWindow::addClassRosterColumn()
{
    auto lifetime = get_strong();
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew
        || m_ownedDialog || !RootGrid().XamlRoot())
    {
        co_return;
    }

    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto form = StackPanel();
    form.Spacing(8.0);
    auto columnName = TextBox();
    columnName.Header(box_value(hstring(L"Column name")));
    columnName.PlaceholderText(L"Enter a column name");
    columnName.IsTabStop(true);
    setAutomationName(columnName, L"New roster column name");
    form.Children().Append(columnName);

    auto validation = TextBlock();
    validation.TextWrapping(TextWrapping::Wrap);
    validation.Visibility(Visibility::Collapsed);
    setAutomationName(validation, L"New roster column validation");
    form.Children().Append(validation);

    auto dialog = ContentDialog();
    dialog.XamlRoot(RootGrid().XamlRoot());
    dialog.Title(box_value(hstring(L"Add Roster Column")));
    dialog.Content(form);
    dialog.PrimaryButtonText(L"Add");
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(ContentDialogButton::Primary);
    m_ownedDialog = dialog;

    for (;;)
    {
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await dialog.ShowAsync();
        }
        catch (...)
        {
            break;
        }
        if (result != ContentDialogResult::Primary)
        {
            break;
        }

        classmngr::engine::Roster candidate = classRosterFromForm();
        candidate.columns.push_back(asUtf8(columnName.Text()));
        candidate.columnWidths.push_back(100);
        candidate = classmngr::engine::RosterValidator::normalized(candidate);
        if (candidate.columns.empty())
        {
            validation.Text(L"Column name cannot be empty.");
            validation.Visibility(Visibility::Visible);
            continue;
        }

        const std::string& normalizedName = candidate.columns.back();
        if (normalizedName.empty())
        {
            validation.Text(L"Column name cannot be empty.");
            validation.Visibility(Visibility::Visible);
            continue;
        }

        const bool duplicate = std::any_of(
            candidate.columns.cbegin(),
            candidate.columns.cend() - 1,
            [&normalizedName](std::string const& existing) {
                return rosterColumnEquals(existing, normalizedName);
            }
            );
        if (duplicate)
        {
            validation.Text(L"A column with that name already exists.");
            validation.Visibility(Visibility::Visible);
            continue;
        }
        if (rosterRequiredColumn(normalizedName))
        {
            validation.Text(L"Required roster columns already exist.");
            validation.Visibility(Visibility::Visible);
            continue;
        }

        padRosterRows(candidate);
        m_classRoster = std::move(candidate);
        m_classRosterCurrentColumn = static_cast<int>(
            m_classRoster.columns.size() - 1
            );
        rebuildClassRosterGrid();
        markClassRosterDirty();
        saveClassRoster();
        if (!m_classRosterDirty)
        {
            m_classRosterStatusText.Text(L"Roster column added.");
        }
        break;
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
}

void MainWindow::removeClassRosterColumn()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }
    const int column = m_classRosterCurrentColumn;
    if (column < 0 || column >= static_cast<int>(m_classRoster.columns.size()))
    {
        m_classRosterStatusText.Text(L"Select a roster column to remove.");
        return;
    }
    if (rosterRequiredColumn(
            m_classRoster.columns[static_cast<std::size_t>(column)]
            ))
    {
        m_classRosterStatusText.Text(
            L"Required roster columns cannot be removed."
            );
        return;
    }

    m_classRoster.columns.erase(
        m_classRoster.columns.begin() + column
        );
    if (column < static_cast<int>(m_classRoster.columnWidths.size()))
    {
        m_classRoster.columnWidths.erase(
            m_classRoster.columnWidths.begin() + column
            );
    }
    for (auto& row : m_classRoster.rows)
    {
        if (column < static_cast<int>(row.size()))
        {
            row.erase(row.begin() + column);
        }
    }
    m_classRosterCurrentColumn = m_classRoster.columns.empty()
        ? -1
        : std::min(
            column,
            static_cast<int>(m_classRoster.columns.size()) - 1
            );
    padRosterRows(m_classRoster);
    rebuildClassRosterGrid();
    markClassRosterDirty();
    saveClassRoster();
    if (!m_classRosterDirty)
    {
        m_classRosterStatusText.Text(L"Roster column removed.");
    }
}

void MainWindow::removeClassRosterRow(int row)
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew
        || !m_classRosterList)
    {
        return;
    }
    if (row < 0 || row >= static_cast<int>(m_classRoster.rows.size()))
    {
        m_classRosterStatusText.Text(L"Select a roster row to remove.");
        return;
    }
    if (!classmngr::engine::isRosterStudentRow(
            m_classRoster,
            m_classRoster.rows[static_cast<std::size_t>(row)]
            ))
    {
        m_classRosterStatusText.Text(L"Selected roster row is already empty.");
        return;
    }

    const std::size_t lastRow = m_classRoster.rows.size() - 1;
    for (std::size_t sourceRow = static_cast<std::size_t>(row) + 1;
         sourceRow <= lastRow;
         ++sourceRow)
    {
        m_classRoster.rows[sourceRow - 1] = m_classRoster.rows[sourceRow];
    }
    m_classRoster.rows[lastRow].assign(m_classRoster.columns.size(), {});
    rebuildClassRosterGrid();
    m_classRosterList.SelectedIndex(row);
    markClassRosterDirty();
    saveClassRoster();
    if (!m_classRosterDirty)
    {
        m_classRosterStatusText.Text(L"Selected roster row removed.");
    }
}

void MainWindow::transferClassRosterRow(
    int row,
    int targetId
    )
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew
        || !m_classRosterList)
    {
        return;
    }
    if (row < 0 || targetId <= 0)
    {
        m_classRosterStatusText.Text(
            L"Select a student and a target class before transferring."
            );
        return;
    }

    classmngr::engine::Roster source = classRosterFromForm();
    if (row >= static_cast<int>(source.rows.size()))
    {
        m_classRosterStatusText.Text(L"The selected roster row is unavailable.");
        return;
    }
    if (!classmngr::engine::isRosterStudentRow(
            source,
            source.rows[static_cast<std::size_t>(row)]
            ))
    {
        m_classRosterStatusText.Text(
            L"Only a populated student row can be transferred."
            );
        return;
    }

    source = classmngr::engine::RosterValidator::normalized(source);
    padRosterRows(source);

    if (!m_classInfo.classGrade.empty())
    {
        classmngr::engine::ClassInfoService infoService(*m_openDatabase);
        const auto targetInfo = infoService.load(targetId);
        if (!targetInfo || targetInfo->classGrade != m_classInfo.classGrade)
        {
            m_classRosterStatusText.Text(
                L"Students can only be transferred between same-grade classes."
                );
            return;
        }
    }

    classmngr::engine::RosterService service(*m_openDatabase);
    const auto loadedTarget = service.load(targetId);
    if (!loadedTarget)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Target roster could not be loaded: "
            + asWide(loadedTarget.error().message)
            ));
        return;
    }
    classmngr::engine::Roster target = *loadedTarget;
    if (target.columns.empty())
    {
        target = defaultRoster();
    }
    target = classmngr::engine::RosterValidator::normalized(target);
    if (target.columnWidths.size() > target.columns.size())
    {
        target.columnWidths.resize(target.columns.size());
    }
    target.columnWidths.resize(target.columns.size(), 100);
    padRosterRows(target);
    if (!rosterHasAvailableRow(target))
    {
        m_classRosterStatusText.Text(
            L"The target roster already has the maximum of 25 rows."
            );
        return;
    }

    const auto& sourceRow = source.rows[static_cast<std::size_t>(row)];
    std::vector<std::string> targetRow(target.columns.size());
    for (std::size_t targetColumn = 0;
         targetColumn < target.columns.size();
         ++targetColumn)
    {
        const auto sourceColumn = std::find(
            source.columns.begin(),
            source.columns.end(),
            target.columns[targetColumn]
            );
        if (sourceColumn != source.columns.end())
        {
            const std::size_t sourceIndex = static_cast<std::size_t>(
                std::distance(source.columns.begin(), sourceColumn)
                );
            if (sourceIndex < sourceRow.size())
            {
                targetRow[targetColumn] = sourceRow[sourceIndex];
            }
        }
    }
    const std::string sourceNamePair = rosterStudentNamePairKey(
        source,
        sourceRow
        );
    if (!sourceNamePair.empty())
    {
        const bool duplicate = std::any_of(
            target.rows.cbegin(),
            target.rows.cend(),
            [&target, &sourceNamePair](std::vector<std::string> const& row) {
                return rosterStudentNamePairKey(target, row) == sourceNamePair;
            }
            );
        if (duplicate)
        {
            m_classRosterStatusText.Text(
                L"The target roster already contains this student."
                );
            return;
        }
    }

    const auto destination = std::find_if(
        target.rows.begin(),
        target.rows.end(),
        [](std::vector<std::string> const& candidate) {
            return !rosterRowHasData(candidate);
        }
        );
    if (destination == target.rows.end())
    {
        m_classRosterStatusText.Text(
            L"The target roster already has the maximum of 25 rows."
            );
        return;
    }
    *destination = std::move(targetRow);
    const std::size_t sourceLastRow = source.rows.size() - 1;
    for (std::size_t sourceShiftRow = static_cast<std::size_t>(row) + 1;
         sourceShiftRow <= sourceLastRow;
         ++sourceShiftRow)
    {
        source.rows[sourceShiftRow - 1] = source.rows[sourceShiftRow];
    }
    source.rows[sourceLastRow].assign(source.columns.size(), {});

    source = classmngr::engine::RosterValidator::normalized(source);
    target = classmngr::engine::RosterValidator::normalized(target);
    padRosterRows(source);
    padRosterRows(target);
    const auto sourceValidation = classmngr::engine::RosterValidator::validate(
        source
        );
    const auto targetValidation = classmngr::engine::RosterValidator::validate(
        target
        );
    if (sourceValidation.hasErrors() || targetValidation.hasErrors())
    {
        m_classRosterStatusText.Text(
            L"Transfer rejected because one or both rosters failed validation."
            );
        m_classRosterValidationText.Text(
            L"Fix roster validation errors before transferring a student."
            );
        m_classRosterValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    const auto saved = service.saveBatch({
        {m_classSelectedId, source},
        {targetId, target}
        });
    if (!saved)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Student transfer could not be saved: "
            + asWide(saved.error().message)
            ));
        return;
    }

    std::wstring targetLabel = L"Class " + std::to_wstring(targetId);
    for (ClassRosterTransferTarget& candidate : m_classRosterTransferTargets)
    {
        if (candidate.classId == targetId)
        {
            targetLabel = candidate.label;
            candidate.full = !rosterHasAvailableRow(target);
            break;
        }
    }

    m_classRosterLoading = true;
    m_classRoster = std::move(source);
    rebuildClassRosterGrid();
    m_classRosterLoading = false;
    clearClassRosterDirty();
    m_classRosterStatusText.Text(
        winrt::hstring(
            L"Student transferred to "
            + targetLabel
            + L"."
            )
        );
    m_classRosterValidationText.Text({});
    m_classRosterValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

} // namespace winrt::ClassMngrWinUI::implementation
