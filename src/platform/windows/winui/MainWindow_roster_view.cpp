#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::refreshClassRoster()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classRosterStatusText || !m_classRosterHeaderGrid
        || !m_classRosterList)
    {
        return;
    }

    const auto clearRosterControls = [this]() {
        m_classRoster = {};
        m_classRosterTransferTargets.clear();
        m_classRosterCellBoxes.clear();
        m_classRosterCurrentColumn = -1;
        m_classRosterHeaderGrid.ColumnDefinitions().Clear();
        m_classRosterHeaderGrid.Children().Clear();
        m_classRosterList.Items().Clear();
        m_classRosterList.SelectedIndex(-1);
        if (m_classStudentCountTextBox)
        {
            m_classStudentCountTextBox.Text(L"0");
        }
    };

    m_classRosterLoading = true;
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        clearRosterControls();
        m_classRosterDirty = false;
        m_classRosterLoading = false;
        m_classRosterStatusText.Text(
            !m_openDatabase
                ? L"No database open."
                : L"Save the selected class before editing its roster."
            );
        m_classRosterValidationText.Text({});
        m_classRosterValidationText.Visibility(Visibility::Collapsed);
        if (!m_classDirty)
        {
            m_dirtyState.markClean();
        }
        updateClassRosterActions();
        return;
    }

    classmngr::engine::RosterService service(*m_openDatabase);
    const auto loaded = service.load(m_classSelectedId);
    if (!loaded)
    {
        clearRosterControls();
        m_classRosterDirty = false;
        m_classRosterLoading = false;
        m_classRosterStatusText.Text(winrt::hstring(
            L"Roster could not be loaded: " + asWide(loaded.error().message)
            ));
        m_classRosterValidationText.Text(winrt::hstring(
            L"Engine loading error: " + asWide(loaded.error().message)
            ));
        m_classRosterValidationText.Visibility(Visibility::Visible);
        if (!m_classDirty)
        {
            m_dirtyState.markClean();
        }
        updateClassRosterActions();
        return;
    }

    m_classRoster = classmngr::engine::RosterValidator::normalized(*loaded);
    if (m_classRoster.columns.empty())
    {
        m_classRoster = defaultRoster();
    }
    if (m_classRoster.columnWidths.size() > m_classRoster.columns.size())
    {
        m_classRoster.columnWidths.resize(m_classRoster.columns.size());
    }
    m_classRoster.columnWidths.resize(
        m_classRoster.columns.size(),
        100
        );
    padRosterRows(m_classRoster);
    m_classRosterCurrentColumn = -1;
    if (m_classStudentCountTextBox)
    {
        m_classStudentCountTextBox.Text(
            std::to_wstring(
                classmngr::engine::rosterStudentCount(m_classRoster)
                )
            );
    }

    m_classRosterTransferTargets.clear();
    const std::string currentGrade = m_classInfo.classGrade;
    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    classmngr::engine::TeacherService teacherService(*m_openDatabase);
    for (const auto& classroom : m_classes)
    {
        if (classroom.id <= 0 || classroom.id == m_classSelectedId)
        {
            continue;
        }

        const auto targetInfo = infoService.load(classroom.id);
        const bool sameGrade = !currentGrade.empty()
            && targetInfo
            && targetInfo->classGrade == currentGrade;
        if (!sameGrade)
        {
            continue;
        }

        std::wstring display;
        if (targetInfo)
        {
            classmngr::engine::Teacher teacher;
            if (targetInfo->teacherId > 0)
            {
                const auto targetTeacher = teacherService.get(
                    targetInfo->teacherId
                    );
                if (targetTeacher)
                {
                    teacher = *targetTeacher;
                }
            }
            display = asWide(
                classmngr::engine::ClassNamingService::classDisplayName(
                    *targetInfo,
                    teacher
                    )
                );
        }
        if (display.empty())
        {
            display = asWide(classroom.name);
        }
        if (display.empty())
        {
            display = L"Class " + std::to_wstring(classroom.id);
        }

        ClassRosterTransferTarget target;
        target.classId = classroom.id;
        target.label = std::move(display);
        const auto targetRoster = service.load(classroom.id);
        target.full = targetRoster && !rosterHasAvailableRow(*targetRoster);
        m_classRosterTransferTargets.push_back(std::move(target));
    }
    std::sort(
        m_classRosterTransferTargets.begin(),
        m_classRosterTransferTargets.end(),
        [](ClassRosterTransferTarget const& left,
           ClassRosterTransferTarget const& right) {
            return left.label != right.label
                ? left.label < right.label
                : left.classId < right.classId;
        }
        );

    rebuildClassRosterGrid();
    m_classRosterLoading = false;
    m_classRosterDirty = false;
    if (!m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    m_classRosterStatusText.Text({});
    m_classRosterValidationText.Text({});
    m_classRosterValidationText.Visibility(Visibility::Collapsed);

    updateClassRosterActions();
}

void MainWindow::rebuildClassRosterGrid()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classRosterHeaderGrid || !m_classRosterList)
    {
        return;
    }

    padRosterRows(m_classRoster);
    if (m_classRosterCurrentColumn >= static_cast<int>(m_classRoster.columns.size()))
    {
        m_classRosterCurrentColumn = -1;
    }

    const bool wasLoading = m_classRosterLoading;
    m_classRosterLoading = true;
    m_classRosterCellBoxes.clear();
    m_classRosterHeaderGrid.ColumnDefinitions().Clear();
    m_classRosterHeaderGrid.Children().Clear();
    m_classRosterList.Items().Clear();
    m_classRosterList.SelectedIndex(-1);

    std::vector<double> widths;
    widths.reserve(m_classRoster.columns.size());
    const auto columnGroup = [](std::string_view column) {
        if (column == "English" || column == "Korean")
        {
            return 0;
        }
        if (column == "Winter" || column == "Speech Contest"
            || column == "Summer" || column == "Fall" || column == "Autumn")
        {
            return 1;
        }
        return 2;
    };
    const auto groupColor = [](int group, bool header) {
        if (group == 0)
        {
            return Windows::UI::Color{
                255,
                static_cast<std::uint8_t>(header ? 100 : 232),
                static_cast<std::uint8_t>(header ? 160 : 242),
                static_cast<std::uint8_t>(header ? 255 : 255)
            };
        }
        if (group == 1)
        {
            return Windows::UI::Color{
                255,
                static_cast<std::uint8_t>(header ? 120 : 232),
                static_cast<std::uint8_t>(header ? 200 : 246),
                static_cast<std::uint8_t>(header ? 120 : 232)
            };
        }
        return Windows::UI::Color{
            255,
            static_cast<std::uint8_t>(header ? 200 : 245),
            static_cast<std::uint8_t>(header ? 200 : 245),
            static_cast<std::uint8_t>(header ? 200 : 245)
        };
    };
    double totalWidth = 0.0;
    for (std::size_t column = 0; column < m_classRoster.columns.size(); ++column)
    {
        const int configuredWidth = column < m_classRoster.columnWidths.size()
            ? m_classRoster.columnWidths[column]
            : 100;
        const double width = std::clamp(
            static_cast<double>(configuredWidth),
            88.0,
            280.0
            );
        widths.push_back(width);
        totalWidth += width;

        auto definition = ColumnDefinition();
        definition.Width(
            GridLengthHelper::FromValueAndType(width, GridUnitType::Pixel)
            );
        m_classRosterHeaderGrid.ColumnDefinitions().Append(definition);

        const int group = columnGroup(m_classRoster.columns[column]);
        const bool beginsGroup = column == 0
            || group != columnGroup(m_classRoster.columns[column - 1]);
        auto header = Border();
        header.MinHeight(58.0);
        header.Padding(Thickness{8.0, 6.0, 8.0, 6.0});
        header.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(groupColor(group, true))
            );
        header.CornerRadius(CornerRadius{3.0, 3.0, 3.0, 3.0});
        auto headerContent = StackPanel();
        headerContent.Spacing(2.0);
        auto groupLabel = TextBlock();
        groupLabel.Text(
            beginsGroup
                ? group == 0
                    ? L"Student Names"
                    : group == 1 ? L"Evaluations" : L"Student Information"
                : L""
            );
        groupLabel.FontSize(11.0);
        groupLabel.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 31, 41, 55}
            ));
        auto columnLabel = TextBlock();
        columnLabel.Text(asWide(m_classRoster.columns[column]));
        columnLabel.TextWrapping(TextWrapping::Wrap);
        columnLabel.FontSize(14.0);
        columnLabel.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        columnLabel.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 31, 41, 55}
            ));
        headerContent.Children().Append(groupLabel);
        headerContent.Children().Append(columnLabel);
        header.Child(headerContent);
        header.Tapped(
            [this, column](auto const&, auto const&) {
                m_classRosterCurrentColumn = static_cast<int>(column);
                updateClassRosterActions();
            }
            );
        Grid::SetColumn(header, static_cast<int32_t>(column));
        m_classRosterHeaderGrid.Children().Append(header);
    }
    m_classRosterHeaderGrid.MinWidth(totalWidth);

    for (std::size_t rowIndex = 0; rowIndex < m_classRoster.rows.size(); ++rowIndex)
    {
        auto rowGrid = Grid();
        rowGrid.ColumnSpacing(2.0);
        rowGrid.MinWidth(totalWidth);
        rowGrid.MinHeight(46.0);
        rowGrid.ContextFlyout(createClassRosterContextMenu(
            static_cast<int>(rowIndex)
            ));
        rowGrid.RightTapped(
            [this, rowIndex](auto const&, auto const&) {
                if (m_classRosterList)
                {
                    m_classRosterList.SelectedIndex(
                        static_cast<int32_t>(rowIndex)
                        );
                }
            }
            );
        for (const double width : widths)
        {
            auto definition = ColumnDefinition();
            definition.Width(
                GridLengthHelper::FromValueAndType(width, GridUnitType::Pixel)
                );
            rowGrid.ColumnDefinitions().Append(definition);
        }

        std::vector<TextBox> rowBoxes;
        rowBoxes.reserve(m_classRoster.columns.size());
        for (std::size_t column = 0; column < m_classRoster.columns.size(); ++column)
        {
            auto cell = TextBox();
            cell.Text(
                column < m_classRoster.rows[rowIndex].size()
                    ? asWide(m_classRoster.rows[rowIndex][column])
                    : std::wstring{}
                );
            cell.MinWidth(widths[column]);
            cell.MinHeight(42.0);
            cell.VerticalContentAlignment(VerticalAlignment::Center);
            cell.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                groupColor(columnGroup(m_classRoster.columns[column]), false)
                ));
            cell.BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 190, 198, 210}
                ));
            cell.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
            cell.MaxLength(
                static_cast<int32_t>(
                    classmngr::engine::RosterValidator::MaximumCellLength
                    )
                );
            cell.Margin(Thickness{0.0, 2.0, 0.0, 2.0});
            cell.IsTabStop(true);
            cell.GotFocus(
                [this, column](auto const&, auto const&) {
                    m_classRosterCurrentColumn = static_cast<int>(column);
                    updateClassRosterActions();
                }
                );
            cell.TextChanging(
                [this, rowIndex, column](
                    TextBox const& sender,
                    TextBoxTextChangingEventArgs const&)
                {
                    if (!m_classRosterLoading)
                    {
                        if (rowIndex < m_classRoster.rows.size()
                            && column < m_classRoster.rows[rowIndex].size())
                        {
                            m_classRoster.rows[rowIndex][column] = asUtf8(
                                sender.Text()
                                );
                        }
                        m_classRosterCurrentColumn = static_cast<int>(column);
                        markClassRosterDirty();
                        saveClassRoster(false);
                    }
                }
                );
            setAutomationName(
                cell,
                L"Roster row " + std::to_wstring(rowIndex + 1) + L" "
                    + asWide(m_classRoster.columns[column])
                );
            cell.ContextFlyout(createClassRosterContextMenu(
                static_cast<int>(rowIndex)
                ));
            cell.RightTapped(
                [this, rowIndex](auto const&, auto const&) {
                    if (m_classRosterList)
                    {
                        m_classRosterList.SelectedIndex(
                            static_cast<int32_t>(rowIndex)
                            );
                    }
                }
                );
            Grid::SetColumn(cell, static_cast<int32_t>(column));
            rowGrid.Children().Append(cell);
            rowBoxes.push_back(cell);
        }
        m_classRosterList.Items().Append(rowGrid);
        m_classRosterCellBoxes.push_back(std::move(rowBoxes));
    }

    m_classRosterLoading = wasLoading;
    updateClassRosterActions();
}

} // namespace winrt::ClassMngrWinUI::implementation
