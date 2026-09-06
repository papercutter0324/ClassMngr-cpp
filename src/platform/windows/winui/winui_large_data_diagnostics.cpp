#include "pch.h"

#include "winui_large_data_diagnostics.h"

#include <psapi.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::windows::winui
{
namespace
{
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

constexpr uint32_t classListRows = 10000, rosterRows = 5000, scheduleRows = 2000;
constexpr uint32_t speakingRows = 10000, speakingColumns = 8, cacheMultiplier = 3;
constexpr uint32_t minimumActiveFrameSamples = 30;
constexpr double p95BudgetMilliseconds = 16.7, maxFrameBudgetMilliseconds = 100.0;
constexpr uint64_t privateBytesDeltaBudget = 67108864, trackedAllocationBudget = 16777216;
constexpr double unavailableFrameMilliseconds = 1000000.0;

struct ProcessMemory { uint64_t privateBytes{}, privateWorkingSetBytes{}; bool privateWorkingSetAvailable{}; };
struct Checkpoint {
    std::string name; uint32_t viewportRows{}, realizedRows{}, realizedCells{}, liveRowViewModels{}, liveCellViewModels{}, activeEditors{}, frameSampleCount{};
    double frameP95Milliseconds{unavailableFrameMilliseconds}, frameMaxMilliseconds{unavailableFrameMilliseconds};
    uint64_t privateBytes{}, privateWorkingSetBytes{}, nativeAllocationBytes{}, nativeAllocationCount{};
    bool privateWorkingSetAvailable{}, functionalPassed{}, performancePassed{}, passed{};
};
struct Workload {
    std::string name; uint32_t sourceRows{}, columns{}; uint64_t baselinePrivateBytes{}, baselinePrivateWorkingSetBytes{};
    bool baselinePrivateWorkingSetAvailable{}, functionalPassed{}, performancePassed{}, passed{};
    std::vector<Checkpoint> checkpoints;
};

ProcessMemory processMemory()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)
            ))
    {
        return {};
    }

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    std::vector<std::byte> buffer(
        sizeof(PSAPI_WORKING_SET_INFORMATION)
        + sizeof(PSAPI_WORKING_SET_BLOCK) * 4096
        );
    for (int attempt = 0; attempt != 8; ++attempt)
    {
        auto* information = reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(
            buffer.data()
            );
        if (QueryWorkingSet(
                GetCurrentProcess(),
                information,
                static_cast<DWORD>(buffer.size())
                ))
        {
            uint64_t privateWorkingSet{};
            for (ULONG_PTR index = 0;
                 index < information->NumberOfEntries;
                 ++index)
            {
                if (information->WorkingSetInfo[index].Shared == 0)
                {
                    privateWorkingSet += systemInfo.dwPageSize;
                }
            }
            return {
                static_cast<uint64_t>(counters.PrivateUsage),
                privateWorkingSet,
                true
            };
        }

        if (GetLastError() != ERROR_BAD_LENGTH)
        {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }

    return {
        static_cast<uint64_t>(counters.PrivateUsage),
        0,
        false
    };
}

std::vector<std::wstring> commandLineArguments(wchar_t const* commandLine)
{
    int count{}; LPWSTR* values = CommandLineToArgvW(commandLine, &count); std::vector<std::wstring> result;
    if (!values) return result;
    for (int i = 0; i < count; ++i) result.emplace_back(values[i]);
    LocalFree(values); return result;
}
bool hasArgument(std::vector<std::wstring> const& values, std::wstring_view name) { return std::find(values.begin(), values.end(), name) != values.end(); }
std::optional<std::wstring> argumentValue(std::vector<std::wstring> const& values, std::wstring_view name)
{
    for (size_t i = 0; i + 1 < values.size(); ++i) if (values[i] == name) return values[i + 1];
    return std::nullopt;
}
bool validRunId(std::wstring const& value)
{
    if (value.empty()) return false;
    std::wstring normalized = value;
    if (normalized.front() != L'{') { normalized.insert(normalized.begin(), L'{'); normalized.push_back(L'}'); }
    GUID guid{}; return SUCCEEDED(CLSIDFromString(normalized.c_str(), &guid));
}
std::string narrow(std::wstring_view value)
{
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}
std::string jsonEscape(std::string_view value)
{
    std::string result;
    for (const unsigned char c : value) {
        switch (c) { case '"': result += "\\\""; break; case '\\': result += "\\\\"; break; case '\n': result += "\\n"; break; case '\r': result += "\\r"; break; case '\t': result += "\\t"; break; default: result += static_cast<char>(c); }
    }
    return result;
}
template<typename T> T findDescendant(DependencyObject const& root)
{
    if (!root) return nullptr;
    if (const auto match = root.try_as<T>()) return match;
    for (int i = 0; i < VisualTreeHelper::GetChildrenCount(root); ++i) if (const auto match = findDescendant<T>(VisualTreeHelper::GetChild(root, i))) return match;
    return nullptr;
}
DependencyObject contentTemplateRoot(DependencyObject const& visual)
{
    if (const auto contentControl = visual.try_as<ContentControl>())
    {
        if (const auto root = contentControl.ContentTemplateRoot())
        {
            return root;
        }
    }
    return visual;
}
void collectTextBlocks(DependencyObject const& root, std::vector<UIElement>& result)
{
    if (!root) return;
    if (const auto text = root.try_as<TextBlock>())
    {
        const auto identity = get_abi(text);
        const bool alreadyPresent = std::any_of(
            result.begin(),
            result.end(),
            [identity](UIElement const& existing) {
                return get_abi(existing) == identity;
            }
            );
        if (!alreadyPresent)
        {
            result.push_back(text);
        }
    }
    const int visualChildren = VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < visualChildren; ++i)
    {
        collectTextBlocks(VisualTreeHelper::GetChild(root, i), result);
    }
    if (const auto panel = root.try_as<Panel>())
    {
        for (const auto& child : panel.Children())
        {
            collectTextBlocks(child, result);
        }
    }
    else if (visualChildren == 0)
    {
        IInspectable content{nullptr};
        if (const auto contentControl = root.try_as<ContentControl>())
        {
            content = contentControl.Content();
        }
        else if (const auto contentPresenter = root.try_as<ContentPresenter>())
        {
            content = contentPresenter.Content();
        }
        if (const auto child = content.try_as<DependencyObject>())
        {
            if (get_abi(child) != get_abi(root))
            {
                collectTextBlocks(child, result);
            }
        }
    }
}
struct ResumeOnDispatcher
{
    Microsoft::UI::Dispatching::DispatcherQueue queue{nullptr};

    bool await_ready() const noexcept
    {
        return queue && queue.HasThreadAccess();
    }

    void await_suspend(std::coroutine_handle<> continuation) const
    {
        if (!queue.TryEnqueue([continuation]() { continuation.resume(); }))
        {
            throw hresult_error(E_FAIL, L"The WinUI dispatcher queue stopped before the diagnostic could resume.");
        }
    }

    void await_resume() const noexcept {}
};
class FrameMeter
{
public:
    ~FrameMeter()
    {
        stop();
    }

    void start()
    {
        stop();
        samples.clear();
        last = {};
        token = CompositionTarget::Rendering(
            [this](IInspectable const&, IInspectable const&) {
                const auto now = std::chrono::steady_clock::now();
                if (last.time_since_epoch().count() != 0)
                {
                    const double milliseconds = std::chrono::duration<
                        double,
                        std::milli
                        >(now - last).count();
                    if (std::isfinite(milliseconds) && milliseconds >= 0.0)
                    {
                        samples.push_back(milliseconds);
                    }
                }
                last = now;
            }
            );
    }

    void stop()
    {
        if (token.value != 0)
        {
            CompositionTarget::Rendering(token);
            token = {};
        }
    }

    uint32_t count() const { return static_cast<uint32_t>(samples.size()); }

    double maximum() const
    {
        return samples.empty()
            ? unavailableFrameMilliseconds
            : *std::max_element(samples.begin(), samples.end());
    }

    double p95() const
    {
        if (samples.empty())
        {
            return unavailableFrameMilliseconds;
        }
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const auto index = std::min(
            sorted.size() - 1,
            static_cast<size_t>(std::ceil(sorted.size() * 0.95)) - 1
            );
        return sorted[index];
    }

private:
    std::vector<double> samples;
    std::chrono::steady_clock::time_point last{};
    event_token token{};
};
DataTemplate rowTemplate() { return Markup::XamlReader::Load(LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"><Border Height="32" Padding="8,0"><TextBlock VerticalAlignment="Center" Text="{Binding}" /></Border></DataTemplate>)").as<DataTemplate>(); }
DataTemplate speakingTemplate() { return Markup::XamlReader::Load(LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><StackPanel Orientation="Horizontal" Height="32"><TextBlock x:Name="cell0" Width="80" Text="0"/><TextBlock x:Name="cell1" Width="80" Text="0"/><TextBlock x:Name="cell2" Width="80" Text="0"/><TextBlock x:Name="cell3" Width="80" Text="0"/><TextBlock x:Name="cell4" Width="80" Text="0"/><TextBlock x:Name="cell5" Width="80" Text="0"/><TextBlock x:Name="cell6" Width="80" Text="0"/><TextBlock x:Name="cell7" Width="80" Text="0"/></StackPanel></DataTemplate>)").as<DataTemplate>(); }
ItemsPanelTemplate itemsPanelTemplate() { return Markup::XamlReader::Load(LR"(<ItemsPanelTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"><ItemsStackPanel CacheLength="0.5" /></ItemsPanelTemplate>)").as<ItemsPanelTemplate>(); }
hstring fixtureValue(uint32_t row, uint32_t value)
{
    return hstring(
        std::wstring(L"row ") + std::to_wstring(row) + L": " + std::to_wstring(value)
        );
}
bool hasEditedFixtureValue(std::wstring const& value)
{
    return value == L"9"
        || (value.size() >= 3 && value.compare(value.size() - 3, 3, L": 9") == 0);
}
} // namespace

struct Phase4LargeDataDiagnostics::Implementation
{
    explicit Implementation(LaunchOptions input) : options(std::move(input)) {}
    LaunchOptions options;
    Window window{nullptr};
    ContentControl host{nullptr};
    Grid surface{nullptr};
    ListView list{nullptr};
    ItemsRepeater repeater{nullptr};
    ScrollViewer scrollViewer{nullptr};
    TextBox editor{nullptr};
    weak_ref<UIElement> editorVisual;
    IObservableVector<IInspectable> source{nullptr};
    std::vector<uint32_t> backingValues;
    uint64_t trackedBytes{};
    uint64_t trackedCount{};
    uint32_t currentRows{};
    uint32_t currentColumns{};
    bool currentSpeaking{};
    std::vector<Workload> workloads;
    std::string errorDetail;
    static constexpr bool performanceRequired =
#if defined(NDEBUG) && defined(_WIN64)
        true;
#else
        false;
#endif

    fire_and_forget run()
    {
        bool completed{};
        try
        {
            prepareWindow();
            co_await runWorkload("class-list", classListRows, 1, true, false);
            co_await runWorkload("roster", rosterRows, 1, true, false);
            co_await runWorkload("schedule", scheduleRows, 1, false, false);
            co_await runWorkload(
                "speaking",
                speakingRows,
                speakingColumns,
                false,
                true
                );
            completed = workloads.size() == 4;
        }
        catch (hresult_error const& error) { errorDetail = narrow(error.message()); }
        catch (std::exception const& error) { errorDetail = error.what(); }
        catch (...) { errorDetail = "unknown native exception"; }
        const bool accepted = completed && std::all_of(
            workloads.begin(),
            workloads.end(),
            [](Workload const& item) { return item.passed; }
            );
        const bool written = writeReport(accepted);
        // The diagnostic process is isolated and exits immediately below.  Do
        // not close the Window first: Window::Close dispatches the normal app
        // shutdown path, which can outlive this coroutine and turn an
        // otherwise durable report into an unhandled shutdown exception.
        ExitProcess(written && accepted ? ERROR_SUCCESS : ERROR_INVALID_DATA);
    }
    void prepareWindow()
    {
        window.Title(L"ClassMngr Phase 4 large-data diagnostics");
        window.AppWindow().Resize(Windows::Graphics::SizeInt32{1200, 800});
        auto root = Grid();
        root.RowDefinitions().Append(RowDefinition());
        root.RowDefinitions().Append(RowDefinition());
        root.RowDefinitions().GetAt(0).Height(
            GridLengthHelper::FromPixels(36)
            );
        root.RowDefinitions().GetAt(1).Height(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star)
            );
        auto title = TextBlock();
        title.Text(L"Phase 4 large-data diagnostic workload (in-memory fixtures only)");
        title.Margin(Thickness{12, 8, 12, 4});
        root.Children().Append(title);
        host = ContentControl();
        host.Margin(Thickness{12, 4, 12, 12});
        Grid::SetRow(host, 1);
        root.Children().Append(host);
        window.Content(root);
        window.Activate();
    }
    IAsyncAction delayOnUi(std::chrono::milliseconds duration)
    {
        co_await resume_after(duration);
        co_await ResumeOnDispatcher{window.DispatcherQueue()};
    }
    std::vector<UIElement> cellsForVisual(UIElement const& visual) const
    {
        const auto templateRoot = contentTemplateRoot(visual);
        if (currentSpeaking)
        {
            std::vector<UIElement> namedCells;
            if (const auto frameworkElement = templateRoot.try_as<FrameworkElement>())
            {
                for (uint32_t column = 0; column < currentColumns; ++column)
                {
                    const auto name = hstring(
                        std::wstring(L"cell") + std::to_wstring(column)
                        );
                    if (const auto cell = frameworkElement.FindName(name).try_as<UIElement>())
                    {
                        namedCells.push_back(cell);
                    }
                }
            }
            if (namedCells.size() == currentColumns)
            {
                return namedCells;
            }
        }

        std::vector<UIElement> cells;
        collectTextBlocks(templateRoot, cells);
        if (currentSpeaking && cells.size() < currentColumns)
        {
            if (const auto frameworkElement = templateRoot.try_as<FrameworkElement>())
            {
                for (uint32_t column = 0; column < currentColumns; ++column)
                {
                    const auto name = hstring(
                        std::wstring(L"cell") + std::to_wstring(column)
                        );
                    if (const auto cell = frameworkElement.FindName(name).try_as<UIElement>())
                    {
                        const auto identity = get_abi(cell);
                        const bool alreadyPresent = std::any_of(
                            cells.begin(),
                            cells.end(),
                            [identity](UIElement const& existing) {
                                return get_abi(existing) == identity;
                            }
                            );
                        if (!alreadyPresent)
                        {
                            cells.push_back(cell);
                        }
                    }
                }
            }
        }
        if (currentSpeaking && cells.size() > currentColumns)
        {
            cells.erase(cells.begin() + currentColumns, cells.end());
        }
        return cells;
    }

    struct CurrentVisuals
    {
        uint32_t rows{};
        uint32_t cells{};
    };

    CurrentVisuals currentVisuals() const
    {
        CurrentVisuals result;
        const auto inspect = [&result, this](UIElement const& visual) {
            ++result.rows;
            result.cells += static_cast<uint32_t>(cellsForVisual(visual).size());
        };
        if (list)
        {
            for (uint32_t index = 0; index < currentRows; ++index)
            {
                if (const auto container = list.ContainerFromIndex(static_cast<int>(index)))
                {
                    if (const auto element = container.try_as<UIElement>())
                    {
                        inspect(element);
                    }
                }
            }
        }
        if (repeater)
        {
            for (uint32_t index = 0; index < currentRows; ++index)
            {
                if (const auto element = repeater.TryGetElement(static_cast<int>(index)))
                {
                    inspect(element);
                }
            }
        }
        return result;
    }

    void buildSource(uint32_t rows, uint32_t columns)
    {
        source = single_threaded_observable_vector<IInspectable>();
        backingValues.assign(static_cast<size_t>(rows) * columns, 0);
        for (uint32_t row = 0; row < rows; ++row)
        {
            for (uint32_t column = 0; column < columns; ++column)
            {
                backingValues[
                    static_cast<size_t>(row) * columns + column
                    ] = (row + column) % 10;
            }
            source.Append(
                box_value(
                    fixtureValue(
                        row,
                        backingValues[static_cast<size_t>(row) * columns]
                        )
                    )
                );
        }
        trackedBytes = static_cast<uint64_t>(backingValues.capacity())
            * sizeof(uint32_t);
        trackedCount = backingValues.empty() ? 0 : 1;
    }
    void buildSurface()
    {
        surface = Grid();
        surface.RowDefinitions().Append(RowDefinition());
        surface.RowDefinitions().Append(RowDefinition());
        surface.RowDefinitions().GetAt(0).Height(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star)
            );
        surface.RowDefinitions().GetAt(1).Height(GridLengthHelper::Auto());
        host.Content(surface);
    }
    void buildList()
    {
        buildSurface();
        list = ListView();
        list.ItemsSource(source);
        list.ItemsPanel(itemsPanelTemplate());
        list.ItemTemplate(rowTemplate());
        list.SelectionMode(ListViewSelectionMode::Single);
        surface.Children().Append(list);
    }
    void buildRepeater(bool speaking)
    {
        buildSurface();
        repeater = ItemsRepeater();
        repeater.ItemsSource(source);
        auto layout = StackLayout();
        layout.Spacing(0);
        repeater.Layout(layout);
        repeater.VerticalCacheLength(0.5);
        repeater.ItemTemplate(
            speaking ? speakingTemplate() : rowTemplate()
            );
        repeater.ElementPrepared(
            [this](
                ItemsRepeater const&,
                ItemsRepeaterElementPreparedEventArgs const& args
                ) {
                if (currentSpeaking)
                {
                    updateSpeakingRow(
                        args.Element(),
                        static_cast<uint32_t>(args.Index())
                        );
                }
            }
            );
        scrollViewer = ScrollViewer();
        scrollViewer.Content(repeater);
        scrollViewer.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        surface.Children().Append(scrollViewer);
    }
    void updateSpeakingRow(UIElement const& visual, uint32_t row)
    {
        if (row >= currentRows)
        {
            return;
        }
        const auto cells = cellsForVisual(visual);
        for (uint32_t column = 0;
             column < currentColumns && column < cells.size();
             ++column)
        {
            if (const auto cell = cells[column].try_as<TextBlock>())
            {
                cell.Text(
                    to_hstring(
                        backingValues[
                            static_cast<size_t>(row) * currentColumns + column
                            ]
                        )
                    );
            }
        }
    }
    void requestScroll(uint32_t row, bool animate)
    {
        if (list)
        {
            if (const auto viewer = findDescendant<ScrollViewer>(list))
            {
                double rowHeight = 32.0;
                if (currentRows != 0 && viewer.ExtentHeight() > 0.0)
                {
                    rowHeight = viewer.ExtentHeight()
                        / static_cast<double>(currentRows);
                }
                viewer.ChangeView(
                    nullptr,
                    static_cast<double>(row) * rowHeight,
                    nullptr,
                    !animate
                    );
            }
            else
            {
                list.ScrollIntoView(source.GetAt(row));
            }
        }
        else if (scrollViewer) scrollViewer.ChangeView(nullptr, static_cast<double>(row) * 32, nullptr, !animate);
    }
    IAsyncAction measureActiveScroll(uint32_t centerRow, FrameMeter& meter)
    {
        meter.start();
        for (uint32_t i = 0; i < 80 && meter.count() < minimumActiveFrameSamples; ++i) { requestScroll(std::min(currentRows - 1, centerRow + (i % 2 ? 0 : 7)), true); co_await delayOnUi(std::chrono::milliseconds(18)); }
        meter.stop();
    }
    IAsyncAction settleUi() { co_await delayOnUi(std::chrono::milliseconds(40)); co_await delayOnUi(std::chrono::milliseconds(40)); }
    IAsyncAction performEditAndVerify(uint32_t row)
    {
        requestScroll(row, false);
        if (list)
        {
            list.ScrollIntoView(
                source.GetAt(row),
                ScrollIntoViewAlignment::Leading
                );
            list.UpdateLayout();
            co_await settleUi();
            list.SelectedIndex(static_cast<int>(row));
        }
        co_await settleUi();

        editor = TextBox();
        editor.Width(120);
        editor.Header(box_value(L"Selected diagnostic editor"));
        editor.TextChanging(
            [this, row](
                TextBox const& sender,
                TextBoxTextChangingEventArgs const&
                ) {
                try
                {
                    const uint32_t value = static_cast<uint32_t>(
                        std::stoul(std::wstring(sender.Text()))
                        );
                    backingValues[
                        static_cast<size_t>(row) * currentColumns
                        ] = value;
                    source.SetAt(row, box_value(fixtureValue(row, value)));
                }
                catch (...)
                {
                    // The probe only enters a known numeric value.
                }
            }
            );
        editorVisual = make_weak(editor.as<UIElement>());
        Grid::SetRow(editor, 1);
        surface.Children().Append(editor);
        editor.Text(L"9");
        co_await settleUi();
        if (list)
        {
            list.SelectedIndex(static_cast<int>(row));
            list.ScrollIntoView(
                source.GetAt(row),
                ScrollIntoViewAlignment::Leading
                );
            list.UpdateLayout();
            co_await settleUi();
        }

        requestScroll(std::min(currentRows - 1, row + 300), false);
        co_await settleUi();
        requestScroll(row, false);
        co_await settleUi();
        if (list)
        {
            const auto targetItem = source.GetAt(row);
            for (uint32_t attempt = 0; attempt != 20; ++attempt)
            {
                list.SelectedIndex(static_cast<int>(row));
                list.ScrollIntoView(targetItem, ScrollIntoViewAlignment::Leading);
                list.UpdateLayout();
                co_await settleUi();
                const auto container = list.ContainerFromItem(targetItem);
                std::vector<UIElement> cells;
                collectTextBlocks(contentTemplateRoot(container), cells);
                if (!cells.empty())
                {
                    break;
                }
            }
        }
    }
    bool editedValueIsBound(uint32_t row)
    {
        const bool backingBound = backingValues[static_cast<size_t>(row) * currentColumns] == 9;
        std::optional<std::wstring> sourceValue;
        if (source)
        {
            try
            {
                sourceValue = std::wstring(
                    unbox_value<hstring>(source.GetAt(row))
                    );
            }
            catch (...)
            {
            }
        }
        if (!backingBound)
        {
            errorDetail = "edit diagnostics: backing value did not become 9";
            return false;
        }
        DependencyObject visual{nullptr};
        DependencyObject indexContainer{nullptr};
        DependencyObject itemContainer{nullptr};
        if (list)
        {
            indexContainer = list.ContainerFromIndex(static_cast<int>(row));
            itemContainer = list.ContainerFromItem(source.GetAt(row));
            visual = itemContainer ? itemContainer : indexContainer;
        }
        else if (repeater)
        {
            visual = repeater.TryGetElement(static_cast<int>(row));
        }
        std::vector<UIElement> cells;
        collectTextBlocks(contentTemplateRoot(visual), cells);
        const bool visualBound = std::any_of(
            cells.begin(),
            cells.end(),
            [](UIElement const& cell) {
                const auto text = cell.try_as<TextBlock>();
                return text && hasEditedFixtureValue(std::wstring(text.Text()));
            }
            );
        const bool sourceBound = sourceValue
            && hasEditedFixtureValue(*sourceValue);
        if (!visualBound || !sourceBound)
        {
            errorDetail = "edited value was not present in the realized target item";
        }
        return visualBound && sourceBound;
    }
    uint32_t viewportRows() const
    {
        const double height = list
            ? list.ActualHeight()
            : (scrollViewer ? scrollViewer.ActualHeight() : 0.0);
        if (!(height > 0.0))
        {
            return 0;
        }
        return std::max(1u, static_cast<uint32_t>(std::ceil(height / 32.0)));
    }
    Checkpoint capture(std::string name, FrameMeter const* meter)
    {
        const auto visuals = currentVisuals();
        const ProcessMemory memory = processMemory();
        Checkpoint result;
        result.name = std::move(name);
        result.viewportRows = viewportRows();
        result.realizedRows = visuals.rows;
        result.realizedCells = visuals.cells;
        result.activeEditors = editorVisual.get() ? 1 : 0;
        if (meter)
        {
            result.frameSampleCount = meter->count();
            result.frameP95Milliseconds = meter->p95();
            result.frameMaxMilliseconds = meter->maximum();
        }
        else
        {
            result.frameSampleCount = 0;
            result.frameP95Milliseconds = 0.0;
            result.frameMaxMilliseconds = 0.0;
        }
        result.privateBytes = memory.privateBytes;
        result.privateWorkingSetBytes = memory.privateWorkingSetBytes;
        result.privateWorkingSetAvailable = memory.privateWorkingSetAvailable;
        result.nativeAllocationBytes = trackedBytes;
        result.nativeAllocationCount = trackedCount;
        return result;
    }
    void evaluate(Checkpoint& point, Workload const& workload, bool editBound) const
    {
        const bool released = point.name == "released";
        const bool bounded = released
            ? point.realizedRows == 0
                && point.realizedCells == 0
                && point.activeEditors == 0
                && point.nativeAllocationBytes == 0
            : point.viewportRows > 0
                && point.realizedRows < point.viewportRows * cacheMultiplier
                && point.realizedCells
                    < point.viewportRows * cacheMultiplier * workload.columns
                && point.activeEditors <= 1;
        const bool privateBounded = point.privateBytes <= workload.baselinePrivateBytes
            || point.privateBytes - workload.baselinePrivateBytes
                <= privateBytesDeltaBudget;
        const bool framesBounded = released
            || (point.frameSampleCount >= minimumActiveFrameSamples
                && point.frameP95Milliseconds <= p95BudgetMilliseconds
                && point.frameMaxMilliseconds <= maxFrameBudgetMilliseconds);
        point.functionalPassed = bounded && editBound;
        point.performancePassed = privateBounded
            && point.privateWorkingSetAvailable
            && point.nativeAllocationBytes <= trackedAllocationBudget
            && framesBounded;
        point.passed = point.functionalPassed
            && (!performanceRequired || point.performancePassed);
    }
    IAsyncAction runWorkload(std::string name, uint32_t rows, uint32_t columns, bool useList, bool speaking)
    {
        co_await settleUi();
        Workload workload;
        workload.name = std::move(name);
        workload.sourceRows = rows;
        workload.columns = columns;
        const ProcessMemory baseline = processMemory();
        workload.baselinePrivateBytes = baseline.privateBytes;
        workload.baselinePrivateWorkingSetBytes = baseline.privateWorkingSetBytes;
        workload.baselinePrivateWorkingSetAvailable = baseline.privateWorkingSetAvailable;
        currentRows = rows;
        currentColumns = columns;
        currentSpeaking = speaking;
        editorVisual = {};
        buildSource(rows, columns);
        if (useList)
        {
            buildList();
        }
        else
        {
            buildRepeater(speaking);
        }
        co_await settleUi();
        FrameMeter initialFrames; co_await measureActiveScroll(0, initialFrames); requestScroll(0, false); co_await settleUi(); auto initial = capture("initial", &initialFrames); evaluate(initial, workload, true); workload.checkpoints.push_back(std::move(initial));
        FrameMeter scrolledFrames; co_await measureActiveScroll(rows / 2, scrolledFrames); co_await settleUi(); auto scrolled = capture("scrolled", &scrolledFrames); evaluate(scrolled, workload, true); workload.checkpoints.push_back(std::move(scrolled));
        co_await performEditAndVerify(rows / 2);
        FrameMeter editedFrames;
        co_await measureActiveScroll(rows / 2, editedFrames);
        requestScroll(rows / 2, false);
        co_await settleUi();
        auto edited = capture("edited", &editedFrames);
        evaluate(edited, workload, editedValueIsBound(rows / 2));
        workload.checkpoints.push_back(std::move(edited));

        if (list)
        {
            list.SelectedIndex(-1);
            list.ItemsSource(nullptr);
        }
        if (repeater)
        {
            repeater.ItemsSource(nullptr);
        }
        if (scrollViewer)
        {
            scrollViewer.Content(nullptr);
        }
        if (surface)
        {
            surface.Children().Clear();
        }
        host.Content(nullptr);
        window.Content(nullptr);
        editorVisual = {};
        editor = nullptr;
        source = nullptr;
        backingValues.clear();
        backingValues.shrink_to_fit();
        trackedBytes = 0;
        trackedCount = 0;
        for (uint32_t wait = 0; wait != 12; ++wait)
        {
            co_await delayOnUi(std::chrono::milliseconds(50));
            const auto visuals = currentVisuals();
            if (visuals.rows == 0 && visuals.cells == 0)
            {
                break;
            }
        }
        auto released = capture("released", nullptr);
        evaluate(released, workload, true);
        workload.checkpoints.push_back(std::move(released));
        host = nullptr;
        surface = nullptr;
        list = nullptr;
        repeater = nullptr;
        scrollViewer = nullptr;
        workload.functionalPassed = std::all_of(workload.checkpoints.begin(), workload.checkpoints.end(), [](Checkpoint const& point) { return point.functionalPassed; }); workload.performancePassed = std::all_of(workload.checkpoints.begin(), workload.checkpoints.end(), [](Checkpoint const& point) { return point.performancePassed; }); workload.passed = workload.functionalPassed && (!performanceRequired || workload.performancePassed); workloads.push_back(std::move(workload));
        if (workloads.size() < 4)
        {
            prepareWindow();
        }
    }
    bool writeReport(bool accepted) const
    {
        std::ofstream output(
            std::filesystem::path(options.outputPath),
            std::ios::binary | std::ios::trunc
            );
        if (!output)
        {
            return false;
        }

        const auto quote = [](std::string_view text) {
            return std::string("\"") + jsonEscape(text) + "\"";
        };
        const bool hasAllWorkloads = workloads.size() == 4;
        const bool functionalPassed = hasAllWorkloads && std::all_of(
            workloads.begin(),
            workloads.end(),
            [](Workload const& item) { return item.functionalPassed; }
            );
        const bool performancePassed = hasAllWorkloads && std::all_of(
            workloads.begin(),
            workloads.end(),
            [](Workload const& item) { return item.performancePassed; }
            );

        output
            << "{\n"
            << "  \"format\": \"classmngr.winui.phase4.large-data.v1\",\n"
            << "  \"runId\": " << quote(narrow(options.runId)) << ",\n"
            << "  \"processId\": " << GetCurrentProcessId() << ",\n"
            << "  \"architecture\": \""
#if defined(_WIN64)
            << "x64"
#else
            << "x86"
#endif
            << "\",\n"
            << "  \"configuration\": \""
#if defined(NDEBUG)
            << "Release"
#else
            << "Debug"
#endif
            << "\",\n"
            << "  \"overallStatus\": \"" << (accepted ? "passed" : "failed") << "\",\n"
            << "  \"functionalPassed\": " << (functionalPassed ? "true" : "false") << ",\n"
            << "  \"performancePassed\": " << (performancePassed ? "true" : "false") << ",\n"
            << "  \"acceptanceLane\": \""
            << (performanceRequired ? "x64-release" : "functional-support")
            << "\",\n"
            << "  \"errorDetail\": " << quote(errorDetail) << ",\n"
            << "  \"metricsScope\": {\n"
            << "    \"allocation\": \"nativeAllocationBytes/nativeAllocationCount track retained std::vector<uint32_t> fixture backing storage only; no row or cell view-model allocations are present or tracked.\",\n"
            << "    \"frame\": \"Initial, scrolled, and edited collect at least 30 CompositionTarget::Rendering intervals during repeated animated ChangeView requests. Settling waits happen after the meter stops; released has no active scroll and reports zero frame samples.\",\n"
            << "    \"memory\": \"privateBytes is PrivateUsage. privateWorkingSetBytes sums QueryWorkingSet non-shared pages and is not total WorkingSetSize.\",\n"
            << "    \"visuals\": \"realized rows/cells are active visual counts from ListView ContainerFromIndex and ItemsRepeater TryGetElement; measurement does not call GetOrCreateElement and does not count an internal recycle-pool element as realized.\",\n"
            << "    \"managed\": \"N/A: this is a native C++ harness with no managed allocation metric.\"\n"
            << "  },\n"
            << "  \"budgets\": {\"realizedRowMultiplier\": 3, \"frameP95Ms\": 16.7, \"frameMaxMs\": 100, \"privateBytesDeltaMax\": 67108864, \"nativeAllocationBytesMax\": 16777216},\n"
            << "  \"workloads\": [\n";
        for (size_t i = 0; i < workloads.size(); ++i)
        {
            const auto& item = workloads[i];
            output
                << "    {\"name\": " << quote(item.name)
                << ", \"sourceRows\": " << item.sourceRows
                << ", \"columns\": " << item.columns
                << ", \"baselinePrivateBytes\": " << item.baselinePrivateBytes
                << ", \"baselinePrivateWorkingSetBytes\": " << item.baselinePrivateWorkingSetBytes
                << ", \"baselinePrivateWorkingSetAvailable\": "
                << (item.baselinePrivateWorkingSetAvailable ? "true" : "false")
                << ", \"functionalPassed\": " << (item.functionalPassed ? "true" : "false")
                << ", \"performancePassed\": " << (item.performancePassed ? "true" : "false")
                << ", \"passed\": " << (item.passed ? "true" : "false")
                << ", \"checkpoints\": [";

            for (size_t j = 0; j < item.checkpoints.size(); ++j)
            {
                const auto& point = item.checkpoints[j];
                output
                    << (j ? ",\n      " : "\n      ")
                    << "{\"name\": " << quote(point.name)
                    << ", \"viewportRows\": " << point.viewportRows
                    << ", \"realizedRows\": " << point.realizedRows
                    << ", \"realizedCells\": " << point.realizedCells
                    << ", \"liveRowViewModels\": " << point.liveRowViewModels
                    << ", \"liveCellViewModels\": " << point.liveCellViewModels
                    << ", \"activeEditors\": " << point.activeEditors
                    << ", \"frameSampleCount\": " << point.frameSampleCount
                    << ", \"frameP95Ms\": " << point.frameP95Milliseconds
                    << ", \"frameMaxMs\": " << point.frameMaxMilliseconds
                    << ", \"privateBytes\": " << point.privateBytes
                    << ", \"privateWorkingSetBytes\": " << point.privateWorkingSetBytes
                    << ", \"privateWorkingSetAvailable\": "
                    << (point.privateWorkingSetAvailable ? "true" : "false")
                    << ", \"nativeAllocationBytes\": " << point.nativeAllocationBytes
                    << ", \"nativeAllocationCount\": " << point.nativeAllocationCount
                    << ", \"functionalPassed\": " << (point.functionalPassed ? "true" : "false")
                    << ", \"performancePassed\": " << (point.performancePassed ? "true" : "false")
                    << ", \"passed\": " << (point.passed ? "true" : "false")
                    << "}";
            }

            output << "\n    ]}" << (i + 1 == workloads.size() ? "\n" : ",\n");
        }

        output << "  ]\n}\n";
        output.flush();
        return output.good();
    }
};

bool Phase4LargeDataDiagnostics::requested(wchar_t const* commandLine) { return hasArgument(commandLineArguments(commandLine), L"--phase4-large-data-test"); }
std::optional<Phase4LargeDataDiagnostics::LaunchOptions> Phase4LargeDataDiagnostics::parse(wchar_t const* commandLine)
{
    const auto values = commandLineArguments(commandLine); if (!hasArgument(values, L"--phase4-large-data-test")) return std::nullopt; const auto output = argumentValue(values, L"--phase4-large-data-output"), runId = argumentValue(values, L"--phase4-large-data-run-id");
    if (!output || !runId || !std::filesystem::path(*output).is_absolute() || !validRunId(*runId)) return std::nullopt; return LaunchOptions{*output, *runId};
}
Phase4LargeDataDiagnostics::Phase4LargeDataDiagnostics(LaunchOptions options) : m_implementation(std::make_unique<Implementation>(std::move(options))) {}
Phase4LargeDataDiagnostics::~Phase4LargeDataDiagnostics() = default;
void Phase4LargeDataDiagnostics::start(Window const& window) { m_implementation->window = window; m_implementation->run(); }
} // namespace classmngr::windows::winui
