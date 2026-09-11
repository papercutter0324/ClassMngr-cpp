#include "pch.h"

#include "winui_schedule_board.h"

#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>

namespace
{
using classmngr::engine::ScheduleReportCell;
using classmngr::engine::ScheduleReportEntry;
using classmngr::engine::ScheduleReportModel;
using classmngr::engine::ScheduleReportRowView;
using classmngr::engine::ScheduleReportService;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using winrt::Windows::UI::Color;
using winrt::Windows::UI::Text::FontStyle;
using winrt::Windows::UI::Text::FontWeights;

constexpr double TimeColumnWidth = 90.0;
constexpr double CompactTimeColumnWidth = 84.0;
constexpr double HeaderHeight = 50.0;
constexpr double CompactHeaderHeight = 36.0;
constexpr double MinimumRowHeight = 62.0;
constexpr double CompactMinimumRowHeight = 40.0;
constexpr double RowBaseHeight = 24.0;
constexpr double CompactRowBaseHeight = 19.0;
constexpr double RowHeightPerEntry = 42.0;
constexpr double CompactRowHeightPerEntry = 32.0;
constexpr double BoardSpacing = 8.0;
constexpr double CardCornerRadius = 7.0;

Color const HeaderColor{255, 45, 52, 64};
Color const HeaderTextColor{255, 255, 255, 255};
Color const TransparentColor{0, 0, 0, 0};
Color const DefaultClassColor{255, 255, 255, 255};
Color const DefaultFontColor{255, 0, 0, 0};
Color const EssayColor{255, 255, 255, 255};
Color const LunchColor{255, 220, 220, 220};
Color const TestingColor{255, 255, 240, 184};
Color const TestingBorderColor{255, 211, 155, 37};
Color const TestingTextColor{255, 74, 53, 0};

struct CallbackState : winrt::implements<
    CallbackState,
    winrt::Windows::Foundation::IInspectable>
{
    explicit CallbackState(
        ClassMngrWinUIScheduleBoard::Callbacks callbacksValue
        )
        : callbacks(std::move(callbacksValue))
    {
    }

    ClassMngrWinUIScheduleBoard::Callbacks callbacks;
};

winrt::Microsoft::UI::Xaml::Media::SolidColorBrush brush(Color color)
{
    return winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(color);
}

int hexDigit(char value) noexcept
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f')
    {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F')
    {
        return value - 'A' + 10;
    }
    return -1;
}

bool byteFromHex(
    std::string_view value,
    std::size_t offset,
    std::uint8_t* result
    ) noexcept
{
    const int high = hexDigit(value[offset]);
    const int low = hexDigit(value[offset + 1]);
    if (high < 0 || low < 0)
    {
        return false;
    }
    *result = static_cast<std::uint8_t>((high << 4) | low);
    return true;
}

Color parseColor(
    std::string_view value,
    Color fallback
    ) noexcept
{
    if (value.size() != 7 && value.size() != 9)
    {
        return fallback;
    }
    if (value.front() != '#')
    {
        return fallback;
    }

    std::uint8_t alpha = 255;
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    const std::size_t offset = 1;
    if (value.size() == 9)
    {
        if (!byteFromHex(value, offset, &alpha))
        {
            return fallback;
        }
    }
    const std::size_t rgbOffset = value.size() == 9 ? 3 : 1;
    if (!byteFromHex(value, rgbOffset, &red)
        || !byteFromHex(value, rgbOffset + 2, &green)
        || !byteFromHex(value, rgbOffset + 4, &blue))
    {
        return fallback;
    }
    return Color{alpha, red, green, blue};
}

winrt::hstring toHString(std::string_view value)
{
    return winrt::to_hstring(std::string(value));
}

std::wstring toWide(std::string_view value)
{
    const winrt::hstring text = toHString(value);
    return std::wstring(text.c_str(), text.size());
}

void setAutomationName(
    winrt::Microsoft::UI::Xaml::DependencyObject const& element,
    std::wstring const& name
    )
{
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        element,
        winrt::hstring(name)
        );
}

void setGridPosition(
    winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
    int row,
    int column
    )
{
    Grid::SetRow(element, row);
    Grid::SetColumn(element, column);
}

Border makeCellSurface(
    Color backgroundColor,
    Color foregroundColor,
    double padding,
    double fontSize,
    bool bold,
    bool italic,
    winrt::hstring const& text,
    std::wstring const& automationName
    )
{
    auto surface = Border();
    surface.Background(brush(backgroundColor));
    surface.CornerRadius(
        CornerRadius{
            CardCornerRadius,
            CardCornerRadius,
            CardCornerRadius,
            CardCornerRadius
        }
        );
    surface.Padding(Thickness{padding, padding, padding, padding});

    auto label = TextBlock();
    label.Text(text);
    label.Foreground(brush(foregroundColor));
    label.FontSize(fontSize);
    if (bold)
    {
        label.FontWeight(FontWeights::Bold());
    }
    if (italic)
    {
        label.FontStyle(FontStyle::Italic);
    }
    label.TextAlignment(TextAlignment::Center);
    label.VerticalAlignment(VerticalAlignment::Center);
    label.HorizontalAlignment(HorizontalAlignment::Stretch);
    label.TextWrapping(TextWrapping::Wrap);
    setAutomationName(label, automationName);
    surface.Child(label);
    return surface;
}

StackPanel makeClassPair(
    ScheduleReportEntry const& entry,
    ClassMngrWinUIScheduleBoard::RenderOptions const& options,
    std::wstring const& automationName
    )
{
    auto pair = StackPanel();
    pair.Spacing(0.0);
    pair.HorizontalAlignment(HorizontalAlignment::Stretch);
    pair.VerticalAlignment(VerticalAlignment::Center);

    auto teacher = TextBlock();
    teacher.Text(toHString(ScheduleReportService::teacherRoomLine(
        entry,
        options.showEnglishNames
        )));
    teacher.Foreground(brush(parseColor(entry.fontColor, DefaultFontColor)));
    teacher.FontSize(options.compactPreview ? 11.0 : 20.0);
    teacher.FontWeight(FontWeights::Bold());
    teacher.TextAlignment(TextAlignment::Center);
    teacher.TextWrapping(TextWrapping::Wrap);
    teacher.HorizontalAlignment(HorizontalAlignment::Stretch);
    teacher.VerticalAlignment(VerticalAlignment::Center);
    setAutomationName(teacher, automationName + L" teacher and room");

    auto classLine = TextBlock();
    classLine.Text(toHString(ScheduleReportService::classLine(
        entry.classGrade,
        entry.classLevel,
        options.compactPreview
        )));
    classLine.Foreground(brush(parseColor(entry.fontColor, DefaultFontColor)));
    classLine.FontSize(options.compactPreview ? 12.0 : 18.0);
    classLine.TextAlignment(TextAlignment::Center);
    classLine.TextWrapping(TextWrapping::Wrap);
    classLine.HorizontalAlignment(HorizontalAlignment::Stretch);
    classLine.VerticalAlignment(VerticalAlignment::Center);
    setAutomationName(classLine, automationName + L" grade and level");

    pair.Children().Append(teacher);
    pair.Children().Append(classLine);
    return pair;
}

Button makeClassButton(
    ScheduleReportEntry const& entry,
    ClassMngrWinUIScheduleBoard::Callbacks const& callbacks,
    ClassMngrWinUIScheduleBoard::RenderOptions const& options
    )
{
    auto button = Button();
    const std::wstring teacherRoom = toWide(
        ScheduleReportService::teacherRoomLine(
            entry,
            options.showEnglishNames
            )
        );
    const std::wstring automationName =
        L"Schedule class " + teacherRoom;
    const Color classColor = parseColor(entry.classColor, DefaultClassColor);
    const Color fontColor = parseColor(entry.fontColor, DefaultFontColor);

    button.Background(brush(classColor));
    button.Foreground(brush(fontColor));
    button.BorderBrush(brush(TransparentColor));
    button.BorderThickness(Thickness{0.0, 0.0, 0.0, 0.0});
    button.CornerRadius(
        CornerRadius{
            CardCornerRadius,
            CardCornerRadius,
            CardCornerRadius,
            CardCornerRadius
        }
        );
    button.Padding(
        options.compactPreview
            ? Thickness{3.0, 2.0, 3.0, 2.0}
            : Thickness{4.0, 3.0, 4.0, 3.0}
        );
    button.HorizontalAlignment(HorizontalAlignment::Stretch);
    button.VerticalAlignment(VerticalAlignment::Stretch);
    button.HorizontalContentAlignment(HorizontalAlignment::Stretch);
    button.VerticalContentAlignment(VerticalAlignment::Center);
    button.IsTabStop(false);
    setAutomationName(button, automationName);
    button.Content(makeClassPair(entry, options, automationName));

    const bool canClick = entry.classId > 0 && callbacks.classClicked;
    button.IsEnabled(canClick && options.enabled);
    if (canClick)
    {
        const std::function<void(int)> classClicked = callbacks.classClicked;
        const int classId = entry.classId;
        button.Click(
            [classClicked, classId](auto const&, auto const&) {
                classClicked(classId);
            }
            );
    }
    return button;
}

Button makeSlotButton(
    ScheduleReportCell const& cell,
    ClassMngrWinUIScheduleBoard::Callbacks const& callbacks,
    ClassMngrWinUIScheduleBoard::RenderOptions const& options
    )
{
    auto button = Button();
    const std::wstring day = toWide(cell.day);
    const std::wstring timeLabel = toWide(cell.timeLabel);
    const std::wstring automationName =
        L"Schedule slot " + day + L" " + timeLabel;

    button.BorderThickness(Thickness{0.0, 0.0, 0.0, 0.0});
    button.BorderBrush(brush(TransparentColor));
    button.Padding(
        options.compactPreview
            ? Thickness{3.0, 2.0, 3.0, 2.0}
            : Thickness{4.0, 3.0, 4.0, 3.0}
        );
    button.HorizontalAlignment(HorizontalAlignment::Stretch);
    button.VerticalAlignment(VerticalAlignment::Stretch);
    button.HorizontalContentAlignment(HorizontalAlignment::Stretch);
    button.VerticalContentAlignment(VerticalAlignment::Center);
    button.IsTabStop(true);
    setAutomationName(button, automationName);

    const auto& state = cell.slotState;
    if (state == ScheduleReportService::essaySlotState())
    {
        button.Background(brush(EssayColor));
        button.Foreground(brush(DefaultFontColor));
        button.CornerRadius(
            CornerRadius{
                CardCornerRadius,
                CardCornerRadius,
                CardCornerRadius,
                CardCornerRadius
            }
            );
        auto label = TextBlock();
        label.Text(L"Essay");
        label.Foreground(brush(DefaultFontColor));
        label.FontSize(options.compactPreview ? 14.0 : 23.0);
        label.FontWeight(FontWeights::Bold());
        label.FontStyle(FontStyle::Italic);
        label.TextAlignment(TextAlignment::Center);
        label.VerticalAlignment(VerticalAlignment::Center);
        label.HorizontalAlignment(HorizontalAlignment::Stretch);
        label.TextWrapping(TextWrapping::Wrap);
        setAutomationName(label, L"Essay");
        button.Content(label);
    }
    else if (state == ScheduleReportService::lunchSlotState())
    {
        button.Background(brush(LunchColor));
        button.Foreground(brush(DefaultFontColor));
        button.CornerRadius(
            CornerRadius{
                CardCornerRadius,
                CardCornerRadius,
                CardCornerRadius,
                CardCornerRadius
            }
            );
        auto label = TextBlock();
        label.Text(L"Lunch");
        label.Foreground(brush(DefaultFontColor));
        label.FontSize(options.compactPreview ? 14.0 : 23.0);
        label.FontWeight(FontWeights::Bold());
        label.FontStyle(FontStyle::Italic);
        label.TextAlignment(TextAlignment::Center);
        label.VerticalAlignment(VerticalAlignment::Center);
        label.HorizontalAlignment(HorizontalAlignment::Stretch);
        label.TextWrapping(TextWrapping::Wrap);
        setAutomationName(label, L"Lunch");
        button.Content(label);
    }
    else if (state == ScheduleReportService::testingSlotState())
    {
        button.Background(brush(TestingColor));
        button.Foreground(brush(TestingTextColor));
        button.BorderBrush(brush(TestingBorderColor));
        button.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        button.CornerRadius(
            CornerRadius{
                CardCornerRadius,
                CardCornerRadius,
                CardCornerRadius,
                CardCornerRadius
            }
            );
        const std::string room = cell.testingRoom;
        const winrt::hstring testingText = room.empty()
            ? winrt::hstring(L"Oral Testing")
            : winrt::hstring(L"Oral Testing\nRm: ") + toHString(room);
        auto label = TextBlock();
        label.Text(testingText);
        label.Foreground(brush(TestingTextColor));
        label.FontSize(options.compactPreview ? 12.0 : 18.0);
        label.FontWeight(FontWeights::Bold());
        label.FontStyle(FontStyle::Italic);
        label.TextAlignment(TextAlignment::Center);
        label.VerticalAlignment(VerticalAlignment::Center);
        label.HorizontalAlignment(HorizontalAlignment::Stretch);
        label.TextWrapping(TextWrapping::Wrap);
        setAutomationName(label, testingText.c_str());
        button.Content(label);
    }
    else
    {
        button.Background(brush(TransparentColor));
        button.Foreground(brush(DefaultFontColor));
    }

    const bool metadataAllowsInteraction =
        cell.slotTogglingEnabled || cell.testingBlockCreationEnabled;
    const bool canClick =
        metadataAllowsInteraction && callbacks.slotClicked;
    // Keep non-interactive Regular-mode cells enabled visually. A disabled
    // WinUI Button applies its disabled visual state over the local
    // background, which makes Essay cells lose their white fill. Hit testing
    // and keyboard focus still stay disabled when the slot is not editable.
    button.IsEnabled(options.enabled);
    button.IsTabStop(canClick && options.enabled);
    button.IsHitTestVisible(canClick && options.enabled);
    if (canClick)
    {
        const std::function<void(
            std::wstring,
            std::wstring,
            std::wstring,
            std::wstring,
            bool,
            bool
            )> slotClicked = callbacks.slotClicked;
        const std::wstring currentState = toWide(cell.slotState);
        const std::wstring defaultState = toWide(cell.defaultSlotState);
        const bool slotTogglingEnabled = cell.slotTogglingEnabled;
        const bool testingBlockCreationEnabled =
            cell.testingBlockCreationEnabled;
        button.Click(
            [slotClicked,
             day,
             timeLabel,
             currentState,
             defaultState,
             slotTogglingEnabled,
             testingBlockCreationEnabled](auto const&, auto const&) {
                slotClicked(
                    day,
                    timeLabel,
                    currentState,
                    defaultState,
                    slotTogglingEnabled,
                    testingBlockCreationEnabled
                    );
            }
            );
    }
    return button;
}

Border makeHeaderCell(
    std::string_view text,
    bool compact,
    std::wstring const& automationName
    )
{
    const double padding = compact ? 4.0 : 6.0;
    auto surface = makeCellSurface(
        HeaderColor,
        HeaderTextColor,
        padding,
        compact ? 12.0 : 18.0,
        true,
        false,
        toHString(text),
        automationName
        );
    surface.HorizontalAlignment(HorizontalAlignment::Stretch);
    surface.VerticalAlignment(VerticalAlignment::Stretch);
    return surface;
}

Border makeTimeCell(
    std::string_view text,
    bool compact,
    std::wstring const& automationName
    )
{
    auto surface = makeCellSurface(
        HeaderColor,
        HeaderTextColor,
        compact ? 3.0 : 4.0,
        compact ? 11.0 : 16.0,
        false,
        false,
        toHString(text),
        automationName
        );
    surface.HorizontalAlignment(HorizontalAlignment::Stretch);
    surface.VerticalAlignment(VerticalAlignment::Stretch);
    return surface;
}

CallbackState* callbackState(
    Grid const& root
    ) noexcept
{
    const auto tag = root.Tag();
    if (!tag)
    {
        return nullptr;
    }
    return winrt::get_self<CallbackState>(tag);
}

void appendEntryCell(
    Grid const& root,
    ScheduleReportCell const& cell,
    int row,
    int column,
    ClassMngrWinUIScheduleBoard::Callbacks const& callbacks,
    ClassMngrWinUIScheduleBoard::RenderOptions const& options
    )
{
    auto content = StackPanel();
    content.Spacing(options.compactPreview ? 3.0 : 4.0);
    content.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.VerticalAlignment(VerticalAlignment::Stretch);
    const std::size_t entryCount = cell.entries.size();
    for (std::size_t entryIndex = 0; entryIndex < entryCount; ++entryIndex)
    {
        const ScheduleReportEntry& entry = cell.entries[entryIndex];
        auto classButton = makeClassButton(entry, callbacks, options);
        if (entryCount > 1)
        {
            setAutomationName(
                classButton,
                L"Schedule class "
                    + toWide(ScheduleReportService::teacherRoomLine(
                        entry,
                        options.showEnglishNames
                        ))
                );
        }
        content.Children().Append(classButton);
    }
    setGridPosition(content, row, column);
    root.Children().Append(content);
}

void appendSlotCell(
    Grid const& root,
    ScheduleReportCell const* cell,
    std::string_view day,
    std::string_view timeLabel,
    int row,
    int column,
    ClassMngrWinUIScheduleBoard::Callbacks const& callbacks,
    ClassMngrWinUIScheduleBoard::RenderOptions const& options
    )
{
    ScheduleReportCell fallback;
    if (cell == nullptr)
    {
        fallback.day = std::string(day);
        fallback.timeLabel = std::string(timeLabel);
        cell = &fallback;
    }
    auto button = makeSlotButton(*cell, callbacks, options);
    setGridPosition(button, row, column);
    root.Children().Append(button);
}

} // namespace

namespace ClassMngrWinUIScheduleBoard
{

winrt::Microsoft::UI::Xaml::Controls::Grid create(Callbacks callbacks)
{
    auto root = Grid();
    root.ColumnSpacing(BoardSpacing);
    root.RowSpacing(BoardSpacing);
    root.Background(brush(TransparentColor));
    setAutomationName(root, L"Schedule board");
    root.Tag(winrt::make<CallbackState>(std::move(callbacks)));
    return root;
}

void render(
    winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
    ScheduleReportModel const& model,
    RenderOptions const& options
    )
{
    if (!root)
    {
        return;
    }

    const auto* state = callbackState(root);
    const Callbacks emptyCallbacks{};
    const Callbacks& callbacks = state ? state->callbacks : emptyCallbacks;

    root.Children().Clear();
    root.RowDefinitions().Clear();
    root.ColumnDefinitions().Clear();
    root.ColumnSpacing(BoardSpacing);
    root.RowSpacing(BoardSpacing);
    root.Background(brush(TransparentColor));

    auto timeColumn = ColumnDefinition();
    timeColumn.Width(GridLengthHelper::FromPixels(
        options.compactPreview
            ? CompactTimeColumnWidth
            : TimeColumnWidth
        ));
    root.ColumnDefinitions().Append(timeColumn);
    for (std::size_t index = 0; index < model.days.size(); ++index)
    {
        auto dayColumn = ColumnDefinition();
        dayColumn.Width(GridLengthHelper::FromValueAndType(
            1.0,
            GridUnitType::Star
            ));
        root.ColumnDefinitions().Append(dayColumn);
    }

    auto headerRow = RowDefinition();
    headerRow.Height(GridLengthHelper::FromPixels(
        options.compactPreview
            ? CompactHeaderHeight
            : HeaderHeight
        ));
    root.RowDefinitions().Append(headerRow);

    auto timeHeader = makeHeaderCell(
        "Time",
        options.compactPreview,
        L"Schedule Time header"
        );
    setGridPosition(timeHeader, 0, 0);
    root.Children().Append(timeHeader);

    for (std::size_t dayIndex = 0; dayIndex < model.days.size(); ++dayIndex)
    {
        auto dayHeader = makeHeaderCell(
            model.days[dayIndex],
            options.compactPreview,
            L"Schedule day " + toWide(model.days[dayIndex]) + L" header"
            );
        setGridPosition(
            dayHeader,
            0,
            static_cast<int>(dayIndex + 1)
            );
        root.Children().Append(dayHeader);
    }

    for (std::size_t rowIndex = 0; rowIndex < model.rows.size(); ++rowIndex)
    {
        const ScheduleReportRowView& modelRow = model.rows[rowIndex];
        auto bodyRow = RowDefinition();
        const int maxEntryCount = std::max(0, modelRow.maxEntryCount);
        const double rowHeight = options.compactPreview
            ? std::max(
                CompactMinimumRowHeight,
                CompactRowBaseHeight
                    + CompactRowHeightPerEntry * maxEntryCount
                )
            : std::max(
                MinimumRowHeight,
                RowBaseHeight + RowHeightPerEntry * maxEntryCount
                );
        bodyRow.Height(GridLengthHelper::FromPixels(rowHeight));
        root.RowDefinitions().Append(bodyRow);

        auto timeCell = makeTimeCell(
            modelRow.timeRangeLabel,
            options.compactPreview,
            L"Schedule time " + toWide(modelRow.timeRangeLabel)
            );
        setGridPosition(
            timeCell,
            static_cast<int>(rowIndex + 1),
            0
            );
        root.Children().Append(timeCell);

        for (std::size_t dayIndex = 0; dayIndex < model.days.size(); ++dayIndex)
        {
            const ScheduleReportCell* cell = nullptr;
            if (dayIndex < modelRow.cells.size())
            {
                cell = &modelRow.cells[dayIndex];
            }
            const int gridRow = static_cast<int>(rowIndex + 1);
            const int gridColumn = static_cast<int>(dayIndex + 1);
            if (cell != nullptr && !cell->entries.empty())
            {
                appendEntryCell(
                    root,
                    *cell,
                    gridRow,
                    gridColumn,
                    callbacks,
                    options
                    );
            }
            else
            {
                appendSlotCell(
                    root,
                    cell,
                    model.days[dayIndex],
                    modelRow.timeLabel,
                    gridRow,
                    gridColumn,
                    callbacks,
                    options
                    );
            }
        }
    }
}

} // namespace ClassMngrWinUIScheduleBoard
