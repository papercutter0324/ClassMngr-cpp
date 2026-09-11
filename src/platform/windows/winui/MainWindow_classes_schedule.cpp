#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::rebuildClassScheduleRows(
    bool intensive,
    std::vector<classmngr::engine::ClassTime> const& times
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto grid = intensive
        ? m_classIntensiveScheduleGrid
        : m_classRegularScheduleGrid;
    if (!grid)
    {
        return;
    }

    auto& dayCombos = intensive
        ? m_classIntensiveDayCombos
        : m_classRegularDayCombos;
    auto& startHourCombos = intensive
        ? m_classIntensiveStartHourCombos
        : m_classRegularStartHourCombos;
    auto& startMinuteCombos = intensive
        ? m_classIntensiveStartMinuteCombos
        : m_classRegularStartMinuteCombos;
    auto& startPeriodCombos = intensive
        ? m_classIntensiveStartPeriodCombos
        : m_classRegularStartPeriodCombos;
    auto& endCombos = intensive
        ? m_classIntensiveEndCombos
        : m_classRegularEndCombos;
    dayCombos.clear();
    startHourCombos.clear();
    startMinuteCombos.clear();
    startPeriodCombos.clear();
    endCombos.clear();
    grid.ColumnDefinitions().Clear();
    grid.RowDefinitions().Clear();
    grid.Children().Clear();

    constexpr double dayWidth = 160.0;
    constexpr double startHourWidth = 70.0;
    constexpr double startMinuteWidth = 80.0;
    constexpr double startPeriodWidth = 70.0;
    constexpr double startWidth = startHourWidth + startMinuteWidth
        + startPeriodWidth + 16.0;
    constexpr double endWidth = 120.0;
    constexpr double removeWidth = 90.0;
    const std::array<double, 4> widths{
        dayWidth, startWidth, endWidth, removeWidth
    };
    grid.ColumnSpacing(16.0);
    grid.RowSpacing(4.0);
    for (const double width : widths)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            width,
            GridUnitType::Pixel
            ));
        grid.ColumnDefinitions().Append(definition);
    }
    const std::array<wchar_t const*, 4> headers{
        L"Days", L"Start Time", L"End Time", L""
    };
    grid.RowDefinitions().Append(RowDefinition());
    for (int column = 0; column < 4; ++column)
    {
        auto header = TextBlock();
        header.Text(headers[static_cast<std::size_t>(column)]);
        header.Margin(Thickness{4.0, 0.0, 4.0, 4.0});
        applyResourceStyle(header, L"Phase3BodyTextBlockStyle");
        Grid::SetColumn(header, column);
        grid.Children().Append(header);
    }

    const auto appendChoice = [](ComboBox const& combo,
                                 std::string_view value) {
        auto item = ComboBoxItem();
        const hstring text{asWide(value)};
        item.Content(box_value(text));
        item.Tag(box_value(text));
        combo.Items().Append(item);
    };
    const auto selectChoice = [](ComboBox const& combo,
                                 std::string_view value) {
        for (int choice = 0; choice < static_cast<int>(combo.Items().Size());
             ++choice)
        {
            const auto item = combo.Items().GetAt(choice).try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == asWide(value))
            {
                combo.SelectedIndex(choice);
                return true;
            }
        }
        return false;
    };
    const auto formatTime = [](int totalMinutes) {
        totalMinutes %= 24 * 60;
        const int hour24 = totalMinutes / 60;
        const int minute = totalMinutes % 60;
        const int hour = hour24 % 12 == 0 ? 12 : hour24 % 12;
        const char* period = hour24 < 12 ? "AM" : "PM";
        return std::to_string(hour) + ":"
            + (minute < 10 ? "0" : "") + std::to_string(minute)
            + " " + period;
    };
    const auto parseStart = [](std::string_view value,
                               std::string& hour,
                               std::string& minute,
                               std::string& period) {
        const auto space = value.find(' ');
        const auto colon = value.find(':');
        if (space == std::string_view::npos || colon == std::string_view::npos
            || colon > space || space + 1 >= value.size())
        {
            return false;
        }
        hour = std::string(value.substr(0, colon));
        minute = ":" + std::string(value.substr(colon + 1, space - colon - 1));
        period = std::string(value.substr(space + 1));
        return true;
    };
    const bool wasLoading = m_classLoading;
    m_classLoading = true;
    for (std::size_t index = 0; index < times.size(); ++index)
    {
        grid.RowDefinitions().Append(RowDefinition());

        auto day = ComboBox();
        day.Width(widths[0]);
        day.MinWidth(widths[0]);
        day.IsTabStop(true);
        for (const std::string& weekday : classmngr::engine::ClassInfoConfig::days())
        {
            appendChoice(day, weekday);
        }
        if (!selectChoice(day, times[index].day))
        {
            day.SelectedIndex(0);
        }
        day.SelectionChanged({this, &MainWindow::ClassField_SelectionChanged});
        setAutomationName(
            day,
            std::wstring(L"Class schedule day ") + std::to_wstring(index + 1)
            );

        auto start = StackPanel();
        start.Orientation(Orientation::Horizontal);
        start.Spacing(8.0);
        auto hour = ComboBox();
        hour.Width(startHourWidth);
        hour.IsTabStop(true);
        for (const std::string& value : intensive
                 ? classmngr::engine::ClassInfoConfig::intensiveHours()
                 : classmngr::engine::ClassInfoConfig::regularHours())
        {
            appendChoice(hour, value);
        }
        auto minute = ComboBox();
        minute.Width(startMinuteWidth);
        minute.IsTabStop(true);
        for (const std::string& value : classmngr::engine::ClassInfoConfig::startMinutes())
        {
            appendChoice(minute, value);
        }
        auto period = ComboBox();
        period.Width(startPeriodWidth);
        period.IsTabStop(true);
        appendChoice(period, "PM");
        appendChoice(period, "AM");
        std::string parsedHour;
        std::string parsedMinute;
        std::string parsedPeriod;
        const bool hasStart = parseStart(
            times[index].startTime, parsedHour, parsedMinute, parsedPeriod
            );
        if (!hasStart || !selectChoice(hour, parsedHour))
        {
            selectChoice(hour, "4");
        }
        if (!hasStart || !selectChoice(minute, parsedMinute))
        {
            selectChoice(minute, ":00");
        }
        if (!hasStart || !selectChoice(period, parsedPeriod))
        {
            selectChoice(period, "PM");
        }
        setAutomationName(hour, std::wstring(L"Class schedule start hour ")
            + std::to_wstring(index + 1));
        setAutomationName(minute, std::wstring(L"Class schedule start minute ")
            + std::to_wstring(index + 1));
        setAutomationName(period, std::wstring(L"Class schedule start period ")
            + std::to_wstring(index + 1));
        start.Children().Append(hour);
        start.Children().Append(minute);
        start.Children().Append(period);

        auto end = ComboBox();
        end.Width(endWidth);
        end.IsTabStop(true);
        setAutomationName(end, std::wstring(L"Class schedule end time ")
            + std::to_wstring(index + 1));
        const auto updateEndOptions = [hour, minute, period, end, formatTime,
                                       selectChoice]() {
            const int selectedHour = std::stoi(
                asUtf8(selectedComboValue(hour))
                );
            const int selectedMinute = std::stoi(asUtf8(
                selectedComboValue(minute).substr(1)
                ));
            const bool am = selectedComboValue(period) == L"AM";
            const int hour24 = am
                ? (selectedHour == 12 ? 0 : selectedHour)
                : (selectedHour == 12 ? 12 : selectedHour + 12);
            const std::string current = asUtf8(selectedComboValue(end));
            end.Items().Clear();
            for (const int duration : {55, 85, 115, 175, 235})
            {
                const int endMinutes = hour24 * 60 + selectedMinute + duration;
                if (endMinutes <= 21 * 60 + 55)
                {
                    auto item = ComboBoxItem();
                    const hstring text{asWide(formatTime(endMinutes))};
                    item.Content(box_value(text));
                    item.Tag(box_value(text));
                    end.Items().Append(item);
                }
            }
            if (!selectChoice(end, current) && end.Items().Size() > 0)
            {
                end.SelectedIndex(0);
            }
        };
        updateEndOptions();
        if (!times[index].endTime.empty())
        {
            selectChoice(end, times[index].endTime);
        }
        hour.SelectionChanged([this, updateEndOptions](auto const&, auto const&) {
            updateEndOptions();
            m_classDetailsDirty = true;
            markClassDirty();
        });
        minute.SelectionChanged([this, updateEndOptions](auto const&, auto const&) {
            updateEndOptions();
            m_classDetailsDirty = true;
            markClassDirty();
        });
        period.SelectionChanged([this, updateEndOptions](auto const&, auto const&) {
            updateEndOptions();
            m_classDetailsDirty = true;
            markClassDirty();
        });
        end.SelectionChanged({this, &MainWindow::ClassField_SelectionChanged});

        auto remove = Button();
        remove.Content(box_value(hstring(L"Remove")));
        remove.Width(removeWidth);
        remove.IsTabStop(true);
        remove.Click(
            [this, intensive, index](auto const&, auto const&) {
                removeClassScheduleRow(
                    intensive,
                    static_cast<int>(index)
                    );
            }
            );
        setAutomationName(
            remove,
            std::wstring(L"Remove class schedule row ")
                + std::to_wstring(index + 1)
            );

        Grid::SetRow(day, static_cast<int>(index + 1));
        Grid::SetColumn(day, 0);
        Grid::SetRow(start, static_cast<int>(index + 1));
        Grid::SetColumn(start, 1);
        Grid::SetRow(end, static_cast<int>(index + 1));
        Grid::SetColumn(end, 2);
        Grid::SetRow(remove, static_cast<int>(index + 1));
        Grid::SetColumn(remove, 3);
        grid.Children().Append(day);
        grid.Children().Append(start);
        grid.Children().Append(end);
        grid.Children().Append(remove);
        dayCombos.push_back(day);
        startHourCombos.push_back(hour);
        startMinuteCombos.push_back(minute);
        startPeriodCombos.push_back(period);
        endCombos.push_back(end);
    }
    grid.MinWidth(std::accumulate(widths.begin(), widths.end(), 0.0));
    m_classLoading = wasLoading;
}

void MainWindow::addClassScheduleRow(bool intensive)
{
    auto times = classScheduleFromForm(intensive);
    times.push_back({});
    rebuildClassScheduleRows(intensive, times);
    m_classDetailsDirty = true;
    markClassDirty();
}

void MainWindow::removeClassScheduleRow(bool intensive, int index)
{
    auto times = classScheduleFromForm(intensive);
    if (index < 0 || index >= static_cast<int>(times.size()))
    {
        return;
    }
    times.erase(times.begin() + index);
    rebuildClassScheduleRows(intensive, times);
    m_classDetailsDirty = true;
    markClassDirty();
}

} // namespace winrt::ClassMngrWinUI::implementation
