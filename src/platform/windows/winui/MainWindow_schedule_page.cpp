#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populateScheduleWorkspace(
    Microsoft::UI::Xaml::Controls::StackPanel const& scheduleRoot
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using winrt::Windows::UI::Color;

    if (m_scheduleTabs)
    {
        scheduleRoot.Children().Append(m_scheduleTabs);
        refreshScheduleWorkspace();
        return;
    }

    auto makeText = [](std::wstring_view text, double fontSize = 0.0) {
        auto value = TextBlock();
        value.Text(winrt::hstring(text));
        value.TextWrapping(TextWrapping::Wrap);
        if (fontSize > 0.0)
        {
            value.FontSize(fontSize);
        }
        return value;
    };
    const auto appendColumn = [](Grid const& grid, double width) {
        auto definition = ColumnDefinition();
        definition.Width(
            GridLengthHelper::FromValueAndType(width, GridUnitType::Pixel)
            );
        grid.ColumnDefinitions().Append(definition);
    };

    auto editorContent = StackPanel();
    editorContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    editorContent.Spacing(12.0);
    editorContent.HorizontalAlignment(HorizontalAlignment::Stretch);

    auto modeBar = Grid();
    modeBar.ColumnSpacing(8.0);
    modeBar.HorizontalAlignment(HorizontalAlignment::Stretch);
    for (int column = 0; column < 6; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            1.0,
            column == 4 ? GridUnitType::Star : GridUnitType::Auto
            ));
        modeBar.ColumnDefinitions().Append(definition);
    }

    const auto makeModeButton = [](std::wstring_view text,
                                   std::wstring_view automationName) {
        auto button = Button();
        button.Content(box_value(hstring(text)));
        button.MinWidth(116.0);
        button.MinHeight(50.0);
        button.Padding(Thickness{16.0, 7.0, 16.0, 7.0});
        button.FontSize(20.0);
        button.IsTabStop(true);
        setAutomationName(button, automationName);
        return button;
    };

    m_scheduleRegularModeButton = makeModeButton(
        L"Regular",
        L"Schedule Regular mode"
        );
    m_scheduleIntensiveModeButton = makeModeButton(
        L"Intensive",
        L"Schedule Intensive mode"
        );
    m_scheduleTestingModeButton = makeModeButton(
        L"Testing",
        L"Schedule Testing mode"
        );
    m_scheduleImportModeButton = makeModeButton(
        L"Import",
        L"Schedule Import"
        );
    m_scheduleImportModeButton.HorizontalAlignment(HorizontalAlignment::Right);
    m_scheduleTestingClassesButton = makeModeButton(
        L"Testing Classes",
        L"Schedule Testing classes"
        );
    m_scheduleTestingClassesButton.MinWidth(132.0);
    m_scheduleTestingClassesButton.Visibility(Visibility::Collapsed);

    Grid::SetColumn(m_scheduleRegularModeButton, 0);
    Grid::SetColumn(m_scheduleIntensiveModeButton, 1);
    Grid::SetColumn(m_scheduleTestingModeButton, 2);
    Grid::SetColumn(m_scheduleTestingClassesButton, 4);
    m_scheduleTestingClassesButton.HorizontalAlignment(
        HorizontalAlignment::Right
        );
    modeBar.Children().Append(m_scheduleRegularModeButton);
    modeBar.Children().Append(m_scheduleIntensiveModeButton);
    modeBar.Children().Append(m_scheduleTestingModeButton);
    modeBar.Children().Append(m_scheduleTestingClassesButton);
    auto modeSpacer = Border();
    modeSpacer.HorizontalAlignment(HorizontalAlignment::Stretch);
    Grid::SetColumn(modeSpacer, 3);
    modeBar.Children().Append(modeSpacer);
    Grid::SetColumn(m_scheduleImportModeButton, 5);
    modeBar.Children().Append(m_scheduleImportModeButton);
    editorContent.Children().Append(modeBar);

    m_scheduleTestingBanner = Border();
    m_scheduleTestingBanner.Padding(Thickness{8.0, 8.0, 8.0, 8.0});
    m_scheduleTestingBanner.MinHeight(36.0);
    m_scheduleTestingBanner.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_scheduleTestingBanner.CornerRadius(CornerRadius{6.0, 6.0, 6.0, 6.0});
    m_scheduleTestingBanner.Background(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            Color{255, 255, 244, 204}
            )
        );
    m_scheduleTestingBanner.BorderBrush(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            Color{255, 213, 165, 46}
            )
        );
    m_scheduleTestingBanner.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
    m_scheduleTestingBanner.Visibility(Visibility::Collapsed);
    auto testingBannerText = TextBlock();
    testingBannerText.Text(
        L"Testing View \u2014 M2 and M3 classes are hidden; M1 classes remain"
        );
    testingBannerText.Foreground(
        Microsoft::UI::Xaml::Media::SolidColorBrush(Color{255, 80, 59, 0})
        );
    testingBannerText.TextWrapping(TextWrapping::Wrap);
    testingBannerText.HorizontalAlignment(HorizontalAlignment::Stretch);
    testingBannerText.TextAlignment(TextAlignment::Center);
    setAutomationName(testingBannerText, L"Schedule testing banner text");
    m_scheduleTestingBanner.Child(testingBannerText);
    setAutomationName(m_scheduleTestingBanner, L"Schedule testing banner");
    m_scheduleBoardRoot = ClassMngrWinUIScheduleBoard::create({
        [this](int classId) {
            openScheduleClassEditor(classId);
        },
        [this](std::wstring day,
               std::wstring timeLabel,
               std::wstring currentState,
               std::wstring defaultState,
               bool slotTogglingEnabled,
               bool testingBlockCreationEnabled) {
            handleScheduleSlotClick(
                std::move(day),
                std::move(timeLabel),
                std::move(currentState),
                std::move(defaultState),
                slotTogglingEnabled,
                testingBlockCreationEnabled
                );
        }
    });
    m_scheduleBoardRoot.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_scheduleBoardRoot.MinWidth(860.0);
    setAutomationName(m_scheduleBoardRoot, L"Weekly class schedule board");
    editorContent.Children().Append(m_scheduleBoardRoot);
    editorContent.Children().Append(m_scheduleTestingBanner);

    m_scheduleRegularModeButton.Click(
        [this](auto const&, auto const&) {
            setScheduleDisplayMode(
                static_cast<int>(
                    classmngr::engine::ScheduleReportDisplayMode::Regular
                    )
                );
        }
        );
    m_scheduleIntensiveModeButton.Click(
        [this](auto const&, auto const&) {
            setScheduleDisplayMode(
                static_cast<int>(
                    classmngr::engine::ScheduleReportDisplayMode::Intensive
                    )
                );
        }
        );
    m_scheduleTestingModeButton.Click(
        [this](auto const&, auto const&) {
            setScheduleDisplayMode(
                static_cast<int>(
                    classmngr::engine::ScheduleReportDisplayMode::Testing
                    )
            );
        }
        );
    m_scheduleTestingClassesButton.Click(
        [this](auto const&, auto const&) {
            refreshTestingWorkspace();
            if (m_scheduleTabs)
            {
                m_scheduleTabs.SelectedIndex(1);
            }
        }
        );
    m_scheduleImportModeButton.Click(
        [this](auto const&, auto const&) {
            openScheduleImportDialog();
        }
        );
    updateScheduleDisplayButtons();

    auto heading = makeText(L"Class schedules", 24.0);
    heading.Visibility(Visibility::Collapsed);
    setAutomationName(heading, L"Class schedules heading");
    editorContent.Children().Append(heading);
    auto description = makeText(
        L"View regular and intensive class times, edit one slot, and let the "
        L"engine reject invalid or overlapping schedules. The table remains "
        L"virtualized by the WinUI ListView for larger class directories."
        );
    description.Visibility(Visibility::Collapsed);
    setAutomationName(description, L"Class schedules description");
    editorContent.Children().Append(description);

    m_scheduleHeaderGrid = Grid();
    m_scheduleHeaderGrid.ColumnSpacing(8.0);
    m_scheduleHeaderGrid.MinWidth(680.0);
    m_scheduleHeaderGrid.Visibility(Visibility::Collapsed);
    setAutomationName(m_scheduleHeaderGrid, L"Class schedule column headers");
    const std::array<double, 5> columnWidths{190.0, 105.0, 125.0, 120.0, 120.0};
    for (const double width : columnWidths)
    {
        appendColumn(m_scheduleHeaderGrid, width);
    }
    const std::array<std::wstring_view, 5> headers{
        L"Class", L"Type", L"Day", L"Start", L"End"
    };
    for (std::size_t column = 0; column < headers.size(); ++column)
    {
        auto header = makeText(headers[column]);
        header.Margin(Thickness{4.0, 4.0, 4.0, 4.0});
        Grid::SetColumn(header, static_cast<int32_t>(column));
        m_scheduleHeaderGrid.Children().Append(header);
    }
    editorContent.Children().Append(m_scheduleHeaderGrid);

    m_scheduleList = ListView();
    m_scheduleList.SelectionMode(ListViewSelectionMode::Single);
    m_scheduleList.IsTabStop(true);
    m_scheduleList.TabIndex(21);
    m_scheduleList.Height(280.0);
    m_scheduleList.Visibility(Visibility::Collapsed);
    m_scheduleList.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_scheduleList.SelectionChanged(
        [this](auto const&, auto const&) {
            if (m_scheduleLoading || !m_scheduleList)
            {
                return;
            }

            const auto item = m_scheduleList.SelectedItem().try_as<
                ListViewItem>();
            if (!item)
            {
                m_scheduleEditingKey.clear();
                return;
            }
            const auto selection = scheduleSelectionFromKey(
                boxedString(item.Tag())
                );
            if (!selection)
            {
                return;
            }

            m_scheduleLoading = true;
            for (int index = 0;
                 index < static_cast<int>(m_scheduleClassSelector.Items().Size());
                 ++index)
            {
                const auto classItem = m_scheduleClassSelector.Items().GetAt(
                    index
                    ).try_as<ComboBoxItem>();
                if (classItem && boxedInt(classItem.Tag()) == selection->classId)
                {
                    m_scheduleClassSelector.SelectedIndex(index);
                    break;
                }
            }
            m_scheduleTypeCombo.SelectedIndex(
                selection->type == classmngr::engine::ScheduleType::Intensive
                    ? 1
                    : 0
                );
            for (int index = 0;
                 index < static_cast<int>(m_scheduleDayCombo.Items().Size());
                 ++index)
            {
                const auto dayItem = m_scheduleDayCombo.Items().GetAt(
                    index
                    ).try_as<ComboBoxItem>();
                if (dayItem && boxedString(dayItem.Tag()) == selection->day)
                {
                    m_scheduleDayCombo.SelectedIndex(index);
                    break;
                }
            }
            m_scheduleStartTextBox.Text(selection->startTime);
            m_scheduleEndTextBox.Text(selection->endTime);
            m_scheduleEditingKey = boxedString(item.Tag());
            m_scheduleLoading = false;
            if (m_scheduleWorkspaceStatusText)
            {
                m_scheduleWorkspaceStatusText.Text(
                    selection->startTime.empty()
                        ? L"Choose a day and time to add a schedule slot."
                        : L"Editing the selected schedule slot."
                    );
            }
        }
        );
    setAutomationName(m_scheduleList, L"Class schedule table");
    editorContent.Children().Append(m_scheduleList);

    auto formCard = ClassMngrWinUISharedUX::buildCard({
        L"Schedule slot editor",
        L"Use the same weekday and time formats as the retained class editor. "
        L"Saving is validated and persisted through the shared engine service.",
        L"Schedule slot editor"
        });
    formCard.root.Visibility(Visibility::Collapsed);
    m_scheduleClassSelector = ComboBox();
    m_scheduleClassSelector.Header(box_value(hstring(L"Class")));
    m_scheduleClassSelector.PlaceholderText(L"Select a class");
    m_scheduleClassSelector.MinWidth(300.0);
    m_scheduleClassSelector.IsTabStop(true);
    m_scheduleClassSelector.TabIndex(22);
    setAutomationName(m_scheduleClassSelector, L"Schedule class selector");
    formCard.content.Children().Append(m_scheduleClassSelector);

    m_scheduleTypeCombo = ComboBox();
    m_scheduleTypeCombo.Header(box_value(hstring(L"Schedule type")));
    m_scheduleTypeCombo.MinWidth(220.0);
    m_scheduleTypeCombo.IsTabStop(true);
    m_scheduleTypeCombo.TabIndex(23);
    for (const auto& choice : {
             std::pair{L"Regular", 0},
             std::pair{L"Intensive", 1}})
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(choice.first)));
        item.Tag(box_value(choice.second));
        setAutomationName(item, choice.first);
        m_scheduleTypeCombo.Items().Append(item);
    }
    m_scheduleTypeCombo.SelectedIndex(0);
    setAutomationName(m_scheduleTypeCombo, L"Schedule type selector");
    formCard.content.Children().Append(m_scheduleTypeCombo);

    m_scheduleDayCombo = ComboBox();
    m_scheduleDayCombo.Header(box_value(hstring(L"Weekday")));
    m_scheduleDayCombo.MinWidth(220.0);
    m_scheduleDayCombo.IsTabStop(true);
    m_scheduleDayCombo.TabIndex(24);
    for (const std::string& day : classmngr::engine::ClassInfoConfig::days())
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(asWide(day))));
        item.Tag(box_value(hstring(asWide(day))));
        setAutomationName(item, asWide(day));
        m_scheduleDayCombo.Items().Append(item);
    }
    m_scheduleDayCombo.SelectedIndex(0);
    setAutomationName(m_scheduleDayCombo, L"Schedule weekday selector");
    formCard.content.Children().Append(m_scheduleDayCombo);

    m_scheduleStartTextBox = TextBox();
    m_scheduleStartTextBox.Header(box_value(hstring(L"Start time")));
    m_scheduleStartTextBox.PlaceholderText(L"e.g. 4:00 PM");
    m_scheduleStartTextBox.MinWidth(220.0);
    m_scheduleStartTextBox.IsTabStop(true);
    m_scheduleStartTextBox.TabIndex(25);
    m_scheduleStartTextBox.TextChanging(
        [this](auto const&, auto const&) {
            if (!m_scheduleLoading && m_scheduleValidationText)
            {
                m_scheduleValidationText.Visibility(Visibility::Collapsed);
            }
        }
        );
    setAutomationName(m_scheduleStartTextBox, L"Schedule start time");
    formCard.content.Children().Append(m_scheduleStartTextBox);

    m_scheduleEndTextBox = TextBox();
    m_scheduleEndTextBox.Header(box_value(hstring(L"End time")));
    m_scheduleEndTextBox.PlaceholderText(L"e.g. 4:55 PM");
    m_scheduleEndTextBox.MinWidth(220.0);
    m_scheduleEndTextBox.IsTabStop(true);
    m_scheduleEndTextBox.TabIndex(26);
    m_scheduleEndTextBox.TextChanging(
        [this](auto const&, auto const&) {
            if (!m_scheduleLoading && m_scheduleValidationText)
            {
                m_scheduleValidationText.Visibility(Visibility::Collapsed);
            }
        }
        );
    setAutomationName(m_scheduleEndTextBox, L"Schedule end time");
    formCard.content.Children().Append(m_scheduleEndTextBox);

    auto actions = StackPanel();
    actions.Orientation(Orientation::Horizontal);
    actions.Spacing(8.0);
    m_scheduleSaveButton = Button();
    m_scheduleSaveButton.Content(box_value(hstring(L"Save schedule slot")));
    m_scheduleSaveButton.IsTabStop(true);
    m_scheduleSaveButton.TabIndex(27);
    m_scheduleSaveButton.Click(
        [this](auto const&, auto const&) { saveScheduleEntry(); }
        );
    setAutomationName(m_scheduleSaveButton, L"Save schedule slot");
    actions.Children().Append(m_scheduleSaveButton);
    m_scheduleClearButton = Button();
    m_scheduleClearButton.Content(box_value(hstring(L"Clear editor")));
    m_scheduleClearButton.IsTabStop(true);
    m_scheduleClearButton.TabIndex(28);
    m_scheduleClearButton.Click(
        [this](auto const&, auto const&) { clearScheduleEntry(); }
        );
    setAutomationName(m_scheduleClearButton, L"Clear schedule editor");
    actions.Children().Append(m_scheduleClearButton);
    formCard.content.Children().Append(actions);

    m_scheduleWorkspaceStatusText = makeText(L"Schedule editor is ready.");
    m_scheduleWorkspaceStatusText.Visibility(Visibility::Collapsed);
    setAutomationName(m_scheduleWorkspaceStatusText, L"Schedule workspace status");
    formCard.content.Children().Append(m_scheduleWorkspaceStatusText);
    m_scheduleValidationText = makeText(L"");
    m_scheduleValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(m_scheduleValidationText, L"Schedule validation");
    formCard.content.Children().Append(m_scheduleValidationText);
    editorContent.Children().Append(formCard.root);

    auto scrollTab = [](StackPanel const& content, bool horizontal = false) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(
            horizontal
                ? ScrollBarVisibility::Auto
                : ScrollBarVisibility::Disabled
            );
        scroll.Content(content);
        return scroll;
    };
    auto scheduleItem = PivotItem();
    scheduleItem.Header(box_value(hstring(L"Schedule")));
    scheduleItem.Content(scrollTab(editorContent, true));
    setAutomationName(scheduleItem, L"Schedule editor tab");

    auto importContent = StackPanel();
    importContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    importContent.Spacing(12.0);
    importContent.HorizontalAlignment(HorizontalAlignment::Stretch);
    auto importHeading = makeText(L"Schedule import", 24.0);
    setAutomationName(importHeading, L"Schedule import heading");
    importContent.Children().Append(importHeading);
    auto importDescription = makeText(
        L"Review one structured user block before applying it. Preview uses "
        L"the shared match and weekday rules; Apply validates the complete "
        L"plan and commits it atomically."
        );
    setAutomationName(importDescription, L"Schedule import description");
    importContent.Children().Append(importDescription);

    auto importCard = ClassMngrWinUISharedUX::buildCard({
        L"Import review",
        L"The form represents the normalized data produced by a workbook "
        L"adapter. It keeps the review/apply boundary visible on Windows.",
        L"Schedule import review"
        });
    const auto makeImportBox = [](std::wstring_view label,
                                  std::wstring_view placeholder,
                                  std::wstring_view automationName) {
        auto box = TextBox();
        box.Header(box_value(hstring(label)));
        box.PlaceholderText(hstring(placeholder));
        box.MinWidth(300.0);
        box.IsTabStop(true);
        setAutomationName(box, automationName);
        return box;
    };
    m_scheduleImportKindCombo = ComboBox();
    m_scheduleImportKindCombo.Header(box_value(hstring(L"Schedule kind")));
    m_scheduleImportKindCombo.MinWidth(240.0);
    m_scheduleImportKindCombo.IsTabStop(true);
    m_scheduleImportKindCombo.TabIndex(30);
    for (const auto& choice : {
             std::pair{L"Normal", 0},
             std::pair{L"Intensive", 1}})
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(choice.first)));
        item.Tag(box_value(choice.second));
        setAutomationName(item, choice.first);
        m_scheduleImportKindCombo.Items().Append(item);
    }
    m_scheduleImportKindCombo.SelectedIndex(0);
    setAutomationName(m_scheduleImportKindCombo, L"Schedule import kind");
    importCard.content.Children().Append(m_scheduleImportKindCombo);

    m_scheduleImportUserTextBox = makeImportBox(
        L"User/profile name",
        L"e.g. Alice",
        L"Schedule import user name"
        );
    m_scheduleImportUserTextBox.Text(L"WinUI User");
    importCard.content.Children().Append(m_scheduleImportUserTextBox);
    m_scheduleImportTeacherTextBox = makeImportBox(
        L"Korean teacher",
        L"Hangul-only teacher key",
        L"Schedule import Korean teacher"
        );
    m_scheduleImportTeacherTextBox.Text(L"\uD64D\uAE38\uB3D9");
    importCard.content.Children().Append(m_scheduleImportTeacherTextBox);
    m_scheduleImportGradeTextBox = makeImportBox(
        L"Class grade",
        L"e.g. E5",
        L"Schedule import class grade"
        );
    m_scheduleImportGradeTextBox.Text(L"E5");
    importCard.content.Children().Append(m_scheduleImportGradeTextBox);
    m_scheduleImportLevelTextBox = makeImportBox(
        L"Class level",
        L"e.g. Zeus",
        L"Schedule import class level"
        );
    m_scheduleImportLevelTextBox.Text(L"Zeus");
    importCard.content.Children().Append(m_scheduleImportLevelTextBox);
    m_scheduleImportRoomTextBox = makeImportBox(
        L"Room",
        L"e.g. 413",
        L"Schedule import room"
        );
    m_scheduleImportRoomTextBox.Text(L"413");
    importCard.content.Children().Append(m_scheduleImportRoomTextBox);
    m_scheduleImportDaysTextBox = makeImportBox(
        L"Meeting days",
        L"Comma-separated, e.g. Monday, Wednesday",
        L"Schedule import meeting days"
        );
    m_scheduleImportDaysTextBox.Text(L"Monday, Wednesday");
    importCard.content.Children().Append(m_scheduleImportDaysTextBox);
    m_scheduleImportStartTextBox = makeImportBox(
        L"Start time",
        L"e.g. 4:00 PM",
        L"Schedule import start time"
        );
    m_scheduleImportStartTextBox.Text(L"4:00 PM");
    importCard.content.Children().Append(m_scheduleImportStartTextBox);
    m_scheduleImportEndTextBox = makeImportBox(
        L"End time",
        L"e.g. 4:55 PM",
        L"Schedule import end time"
        );
    m_scheduleImportEndTextBox.Text(L"4:55 PM");
    importCard.content.Children().Append(m_scheduleImportEndTextBox);

    m_scheduleImportTeacherActionCombo = ComboBox();
    m_scheduleImportTeacherActionCombo.Header(
        box_value(hstring(L"Teacher resolution"))
        );
    m_scheduleImportTeacherActionCombo.MinWidth(280.0);
    m_scheduleImportTeacherActionCombo.IsTabStop(true);
    m_scheduleImportTeacherActionCombo.TabIndex(31);
    for (const auto& choice : {
             std::pair{L"Reuse matching teacher", 0},
             std::pair{L"Create new teacher", 2}})
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(choice.first)));
        item.Tag(box_value(choice.second));
        setAutomationName(item, choice.first);
        m_scheduleImportTeacherActionCombo.Items().Append(item);
    }
    m_scheduleImportTeacherActionCombo.SelectedIndex(0);
    setAutomationName(
        m_scheduleImportTeacherActionCombo,
        L"Schedule import teacher resolution"
        );
    importCard.content.Children().Append(m_scheduleImportTeacherActionCombo);

    m_scheduleImportClassActionCombo = ComboBox();
    m_scheduleImportClassActionCombo.Header(
        box_value(hstring(L"Class resolution"))
        );
    m_scheduleImportClassActionCombo.MinWidth(280.0);
    m_scheduleImportClassActionCombo.IsTabStop(true);
    m_scheduleImportClassActionCombo.TabIndex(32);
    for (const auto& choice : {
             std::pair{L"Update suggested class", 0},
             std::pair{L"Create new class", 1},
             std::pair{L"Skip class", 2}})
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(choice.first)));
        item.Tag(box_value(choice.second));
        setAutomationName(item, choice.first);
        m_scheduleImportClassActionCombo.Items().Append(item);
    }
    m_scheduleImportClassActionCombo.SelectedIndex(1);
    setAutomationName(
        m_scheduleImportClassActionCombo,
        L"Schedule import class resolution"
        );
    importCard.content.Children().Append(m_scheduleImportClassActionCombo);

    auto importActions = StackPanel();
    importActions.Orientation(Orientation::Horizontal);
    importActions.Spacing(8.0);
    m_scheduleImportPreviewButton = Button();
    m_scheduleImportPreviewButton.Content(
        box_value(hstring(L"Preview import"))
        );
    m_scheduleImportPreviewButton.IsTabStop(true);
    m_scheduleImportPreviewButton.TabIndex(33);
    m_scheduleImportPreviewButton.Click(
        [this](auto const&, auto const&) { previewScheduleImport(); }
        );
    setAutomationName(m_scheduleImportPreviewButton, L"Preview schedule import");
    importActions.Children().Append(m_scheduleImportPreviewButton);
    m_scheduleImportApplyButton = Button();
    m_scheduleImportApplyButton.Content(box_value(hstring(L"Apply import")));
    m_scheduleImportApplyButton.IsTabStop(true);
    m_scheduleImportApplyButton.TabIndex(34);
    m_scheduleImportApplyButton.IsEnabled(false);
    m_scheduleImportApplyButton.Click(
        [this](auto const&, auto const&) { applyScheduleImport(); }
        );
    setAutomationName(m_scheduleImportApplyButton, L"Apply schedule import");
    importActions.Children().Append(m_scheduleImportApplyButton);
    importCard.content.Children().Append(importActions);
    m_scheduleImportStatusText = makeText(L"Preview an import block to begin.");
    setAutomationName(m_scheduleImportStatusText, L"Schedule import status");
    importCard.content.Children().Append(m_scheduleImportStatusText);
    m_scheduleImportValidationText = makeText(L"");
    m_scheduleImportValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(m_scheduleImportValidationText, L"Schedule import validation");
    importCard.content.Children().Append(m_scheduleImportValidationText);
    importContent.Children().Append(importCard.root);
    m_scheduleImportDialogRoot = scrollTab(importContent);
    m_scheduleImportDialogRoot.MaxHeight(720.0);
    setAutomationName(m_scheduleImportDialogRoot, L"Schedule import dialog content");

    auto testingContent = StackPanel();
    testingContent.Padding(Thickness{24.0, 20.0, 24.0, 24.0});
    testingContent.Spacing(12.0);
    testingContent.HorizontalAlignment(HorizontalAlignment::Stretch);

    auto testingPage = Grid();
    testingPage.MinHeight(760.0);
    testingPage.HorizontalAlignment(HorizontalAlignment::Stretch);
    testingPage.VerticalAlignment(VerticalAlignment::Stretch);
    testingPage.RowDefinitions().Append(RowDefinition());
    testingPage.RowDefinitions().Append(RowDefinition());
    testingPage.RowDefinitions().GetAt(0).Height(
        GridLengthHelper::FromPixels(48.0)
        );
    testingPage.RowDefinitions().GetAt(1).Height(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
        );

    auto testingHeader = Grid();
    testingHeader.ColumnDefinitions().Append(ColumnDefinition());
    auto headerActionColumn = ColumnDefinition();
    headerActionColumn.Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    testingHeader.ColumnDefinitions().Append(headerActionColumn);

    auto testingHeaderText = StackPanel();
    testingHeaderText.Spacing(2.0);
    auto testingTitle = TextBlock();
    testingTitle.Text(L"Testing Classes");
    applyResourceStyle(testingTitle, L"Phase3PageTitleTextBlockStyle");
    setAutomationName(testingTitle, L"Testing Classes heading");
    testingHeaderText.Children().Append(testingTitle);
    Grid::SetColumn(testingHeaderText, 0);
    testingHeader.Children().Append(testingHeaderText);

    m_testingClassBackButton = Button();
    m_testingClassBackButton.Content(
        box_value(hstring(L"Back to Testing Schedule"))
        );
    applyResourceStyle(
        m_testingClassBackButton,
        L"Phase3SecondaryButtonStyle"
        );
    m_testingClassBackButton.MinWidth(280.0);
    m_testingClassBackButton.Height(40.0);
    m_testingClassBackButton.HorizontalAlignment(HorizontalAlignment::Right);
    m_testingClassBackButton.VerticalAlignment(VerticalAlignment::Top);
    m_testingClassBackButton.Click(
        [this](auto const&, auto const&) {
            if (m_scheduleTabs)
            {
                m_scheduleTabs.SelectedIndex(0);
            }
        }
        );
    setAutomationName(
        m_testingClassBackButton,
        L"Back to Testing Schedule workspace"
        );
    Grid::SetColumn(m_testingClassBackButton, 1);
    testingHeader.Children().Append(m_testingClassBackButton);
    Grid::SetRow(testingHeader, 0);
    testingPage.Children().Append(testingHeader);

    auto testingBody = Grid();
    testingBody.ColumnSpacing(8.0);
    auto navigationColumn = ColumnDefinition();
    navigationColumn.Width(GridLengthHelper::FromPixels(300.0));
    testingBody.ColumnDefinitions().Append(navigationColumn);
    auto editorColumn = ColumnDefinition();
    editorColumn.Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
        );
    testingBody.ColumnDefinitions().Append(editorColumn);

    auto testingNavigation = Grid();
    testingNavigation.RowDefinitions().Append(RowDefinition());
    testingNavigation.RowDefinitions().Append(RowDefinition());
    testingNavigation.RowDefinitions().Append(RowDefinition());
    testingNavigation.RowDefinitions().GetAt(0).Height(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
        );
    testingNavigation.RowDefinitions().GetAt(1).Height(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    testingNavigation.RowDefinitions().GetAt(2).Height(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );

    m_testingClassList = ListView();
    m_testingClassList.SelectionMode(ListViewSelectionMode::Single);
    m_testingClassList.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_testingClassList.HorizontalContentAlignment(
        HorizontalAlignment::Stretch
        );
    m_testingClassList.IsTabStop(true);
    m_testingClassList.TabIndex(0);
    m_testingClassList.Padding(Thickness{0.0, 0.0, 0.0, 0.0});
    setAutomationName(m_testingClassList, L"Testing classes list");
    m_testingClassList.SelectionChanged(
        [this](auto const&, auto const&) {
            if (m_testingClassVisualLoading || !m_testingClassList)
            {
                return;
            }
            const auto item = m_testingClassList.SelectedItem().try_as<
                ListViewItem>();
            if (!item)
            {
                m_testingClassSelectedId = -1;
                updateTestingClassActions();
                return;
            }
            loadTestingClassVisual(boxedInt(item.Tag()));
        }
        );
    auto testingListFrame = Border();
    testingListFrame.Padding(Thickness{0.0, 0.0, 0.0, 0.0});
    applyResourceStyle(testingListFrame, L"Phase4CardBorderStyle");
    testingListFrame.Child(m_testingClassList);
    Grid::SetRow(testingListFrame, 0);
    testingNavigation.Children().Append(testingListFrame);

    m_testingClassAddButton = Button();
    m_testingClassAddButton.Content(box_value(hstring(L"Add Class")));
    applyResourceStyle(
        m_testingClassAddButton,
        L"Phase3SecondaryButtonStyle"
        );
    m_testingClassAddButton.Height(40.0);
    m_testingClassAddButton.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_testingClassAddButton.Margin(Thickness{0.0, 8.0, 0.0, 0.0});
    m_testingClassAddButton.Click(
        [this](auto const&, auto const&) { beginTestingClass(); }
        );
    setAutomationName(m_testingClassAddButton, L"Add testing class");
    Grid::SetRow(m_testingClassAddButton, 1);
    testingNavigation.Children().Append(m_testingClassAddButton);

    m_testingClassDeleteButton = Button();
    m_testingClassDeleteButton.Content(
        box_value(hstring(L"Delete Class"))
        );
    applyResourceStyle(
        m_testingClassDeleteButton,
        L"Phase3SecondaryButtonStyle"
        );
    m_testingClassDeleteButton.Height(40.0);
    m_testingClassDeleteButton.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_testingClassDeleteButton.Margin(Thickness{0.0, 8.0, 0.0, 0.0});
    m_testingClassDeleteButton.Click(
        [this](auto const&, auto const&) { deleteTestingClass(); }
        );
    setAutomationName(m_testingClassDeleteButton, L"Delete testing class");
    Grid::SetRow(m_testingClassDeleteButton, 2);
    testingNavigation.Children().Append(m_testingClassDeleteButton);
    Grid::SetColumn(testingNavigation, 0);
    testingBody.Children().Append(testingNavigation);

    auto detailsPane = Border();
    detailsPane.Padding(Thickness{16.0, 16.0, 16.0, 16.0});
    applyResourceStyle(detailsPane, L"Phase4CardBorderStyle");
    auto detailsGrid = Grid();
    detailsGrid.ColumnSpacing(8.0);
    constexpr double testingFieldHeight = 40.0;
    auto labelColumn = ColumnDefinition();
    labelColumn.Width(GridLengthHelper::FromPixels(120.0));
    detailsGrid.ColumnDefinitions().Append(labelColumn);
    auto fieldColumn = ColumnDefinition();
    fieldColumn.Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
        );
    detailsGrid.ColumnDefinitions().Append(fieldColumn);
    for (int row = 0; row < 6; ++row)
    {
        auto definition = RowDefinition();
        definition.Height(GridLengthHelper::FromPixels(56.0));
        detailsGrid.RowDefinitions().Append(definition);
    }

    const auto makeEditorTextBox = [this](std::wstring_view value,
                                          std::wstring_view automationName) {
        auto box = TextBox();
        box.Text(hstring(value));
        applyResourceStyle(box, L"Phase4FormFieldTextBoxStyle");
        box.MinHeight(testingFieldHeight);
        box.Height(testingFieldHeight);
        box.VerticalAlignment(VerticalAlignment::Center);
        box.VerticalContentAlignment(VerticalAlignment::Center);
        box.HorizontalAlignment(HorizontalAlignment::Stretch);
        box.IsTabStop(true);
        setAutomationName(box, automationName);
        return box;
    };
    const auto makeEditorCombo = [](std::wstring_view automationName) {
        auto combo = ComboBox();
        applyResourceStyle(combo, L"Phase4FilterComboBoxStyle");
        combo.MinHeight(testingFieldHeight);
        combo.Height(testingFieldHeight);
        combo.VerticalAlignment(VerticalAlignment::Center);
        combo.HorizontalAlignment(HorizontalAlignment::Stretch);
        combo.IsTabStop(true);
        setAutomationName(combo, automationName);
        return combo;
    };
    const auto appendComboItem = [](ComboBox const& combo,
                                    std::wstring_view value,
                                    int tag = -1) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(value)));
        if (tag >= 0)
        {
            item.Tag(box_value(tag));
        }
        else
        {
            item.Tag(box_value(hstring(value)));
        }
        combo.Items().Append(item);
    };
    const auto addLabel = [&detailsGrid](std::wstring_view text, int row) {
        auto label = TextBlock();
        label.Text(hstring(text));
        applyResourceStyle(label, L"Phase3BodyTextBlockStyle");
        label.VerticalAlignment(VerticalAlignment::Center);
        label.TextTrimming(TextTrimming::None);
        Grid::SetRow(label, row);
        Grid::SetColumn(label, 0);
        detailsGrid.Children().Append(label);
    };

    addLabel(L"Korean Teacher", 0);
    m_testingClassTeacherCombo = makeEditorCombo(
        L"Testing class Korean teacher"
        );
    appendComboItem(m_testingClassTeacherCombo, L"None", -1);
    Grid::SetRow(m_testingClassTeacherCombo, 0);
    Grid::SetColumn(m_testingClassTeacherCombo, 1);
    detailsGrid.Children().Append(m_testingClassTeacherCombo);

    addLabel(L"Room", 1);
    m_testingClassRoomTextBox = makeEditorTextBox(
        L"",
        L"Testing class room"
        );
    Grid::SetRow(m_testingClassRoomTextBox, 1);
    Grid::SetColumn(m_testingClassRoomTextBox, 1);
    detailsGrid.Children().Append(m_testingClassRoomTextBox);

    addLabel(L"Class Name", 2);
    m_testingClassNameTextBox = makeEditorTextBox(
        L"Testing Class",
        L"Testing class name"
        );
    Grid::SetRow(m_testingClassNameTextBox, 2);
    Grid::SetColumn(m_testingClassNameTextBox, 1);
    detailsGrid.Children().Append(m_testingClassNameTextBox);

    addLabel(L"Grade / Level", 3);
    auto gradeLevelField = Grid();
    gradeLevelField.ColumnSpacing(8.0);
    auto gradeColumn = ColumnDefinition();
    gradeColumn.Width(GridLengthHelper::FromPixels(176.0));
    gradeLevelField.ColumnDefinitions().Append(gradeColumn);
    auto levelColumn = ColumnDefinition();
    levelColumn.Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
        );
    gradeLevelField.ColumnDefinitions().Append(levelColumn);
    m_testingClassGradeCombo = makeEditorCombo(L"Testing class grade");
    for (const std::string& grade : classmngr::engine::testingClassGrades())
    {
        appendComboItem(m_testingClassGradeCombo, asWide(grade));
    }
    m_testingClassGradeCombo.SelectedIndex(0);
    m_testingClassLevelCombo = makeEditorCombo(L"Testing class level");
    for (const std::string& grade : classmngr::engine::testingClassGrades())
    {
        for (const std::string& level :
             classmngr::engine::testingClassLevelsForGrade(grade))
        {
            bool exists = false;
            for (uint32_t index = 0;
                 index < m_testingClassLevelCombo.Items().Size();
                 ++index)
            {
                const auto item = m_testingClassLevelCombo.Items().GetAt(index)
                    .try_as<ComboBoxItem>();
                if (item && boxedString(item.Tag()) == asWide(level))
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
            {
                appendComboItem(m_testingClassLevelCombo, asWide(level));
            }
        }
    }
    m_testingClassLevelCombo.SelectedIndex(0);
    Grid::SetColumn(m_testingClassGradeCombo, 0);
    Grid::SetColumn(m_testingClassLevelCombo, 1);
    gradeLevelField.Children().Append(m_testingClassGradeCombo);
    gradeLevelField.Children().Append(m_testingClassLevelCombo);
    Grid::SetRow(gradeLevelField, 3);
    Grid::SetColumn(gradeLevelField, 1);
    detailsGrid.Children().Append(gradeLevelField);

    addLabel(L"Class Color", 4);
    auto classColorField = StackPanel();
    classColorField.Orientation(Orientation::Horizontal);
    classColorField.Spacing(8.0);
    classColorField.VerticalAlignment(VerticalAlignment::Center);
    m_testingClassColorPreview = Border();
    m_testingClassColorPreview.Width(36.0);
    m_testingClassColorPreview.Height(36.0);
    m_testingClassColorPreview.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
    m_testingClassColorPreview.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
    m_testingClassColorPreview.BorderBrush(detailsPane.BorderBrush());
    setAutomationName(m_testingClassColorPreview, L"Testing class color preview");
    m_testingClassColorButton = Button();
    m_testingClassColorButton.Content(box_value(hstring(L"Choose Color")));
    applyResourceStyle(
        m_testingClassColorButton,
        L"Phase3SecondaryButtonStyle"
        );
    m_testingClassColorButton.Width(185.0);
    m_testingClassColorButton.Height(testingFieldHeight);
    setAutomationName(m_testingClassColorButton, L"Choose testing class color");
    classColorField.Children().Append(m_testingClassColorPreview);
    classColorField.Children().Append(m_testingClassColorButton);
    Grid::SetRow(classColorField, 4);
    Grid::SetColumn(classColorField, 1);
    detailsGrid.Children().Append(classColorField);

    addLabel(L"Font Color", 5);
    auto fontColorField = StackPanel();
    fontColorField.Orientation(Orientation::Horizontal);
    fontColorField.Spacing(8.0);
    fontColorField.VerticalAlignment(VerticalAlignment::Center);
    m_testingClassFontColorPreview = Border();
    m_testingClassFontColorPreview.Width(36.0);
    m_testingClassFontColorPreview.Height(36.0);
    m_testingClassFontColorPreview.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
    m_testingClassFontColorPreview.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
    m_testingClassFontColorPreview.BorderBrush(detailsPane.BorderBrush());
    setAutomationName(
        m_testingClassFontColorPreview,
        L"Testing class font color preview"
        );
    m_testingClassFontColorButton = Button();
    m_testingClassFontColorButton.Content(box_value(hstring(L"Choose Color")));
    applyResourceStyle(
        m_testingClassFontColorButton,
        L"Phase3SecondaryButtonStyle"
        );
    m_testingClassFontColorButton.Width(185.0);
    m_testingClassFontColorButton.Height(testingFieldHeight);
    setAutomationName(
        m_testingClassFontColorButton,
        L"Choose testing class font color"
        );
    fontColorField.Children().Append(m_testingClassFontColorPreview);
    fontColorField.Children().Append(m_testingClassFontColorButton);
    Grid::SetRow(fontColorField, 5);
    Grid::SetColumn(fontColorField, 1);
    detailsGrid.Children().Append(fontColorField);
    detailsPane.Child(detailsGrid);

    auto rosterPane = Border();
    rosterPane.Padding(Thickness{16.0, 16.0, 16.0, 16.0});
    applyResourceStyle(rosterPane, L"Phase4CardBorderStyle");
    auto rosterText = TextBlock();
    rosterText.Text(L"Select a testing class to edit its roster.");
    applyResourceStyle(rosterText, L"Phase4CardDescriptionTextBlockStyle");
    rosterText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(rosterText, L"Testing class roster content");
    rosterPane.Child(rosterText);

    auto notesPane = Border();
    notesPane.Padding(Thickness{16.0, 16.0, 16.0, 16.0});
    applyResourceStyle(notesPane, L"Phase4CardBorderStyle");
    m_testingClassNotesTextBox = TextBox();
    m_testingClassNotesTextBox.PlaceholderText(
        L"Instructions, accommodations, and other notes"
        );
    applyResourceStyle(
        m_testingClassNotesTextBox,
        L"Phase4FormFieldTextBoxStyle"
        );
    m_testingClassNotesTextBox.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_testingClassNotesTextBox.VerticalAlignment(VerticalAlignment::Stretch);
    m_testingClassNotesTextBox.AcceptsReturn(true);
    m_testingClassNotesTextBox.TextWrapping(TextWrapping::Wrap);
    m_testingClassNotesTextBox.VerticalContentAlignment(
        VerticalAlignment::Top
        );
    setAutomationName(m_testingClassNotesTextBox, L"Testing class notes");
    notesPane.Child(m_testingClassNotesTextBox);

    auto testingEditorHost = Grid();
    testingEditorHost.HorizontalAlignment(HorizontalAlignment::Stretch);
    testingEditorHost.VerticalAlignment(VerticalAlignment::Stretch);
    testingEditorHost.Children().Append(detailsPane);
    testingEditorHost.Children().Append(rosterPane);
    testingEditorHost.Children().Append(notesPane);
    const auto testingEditorChildren = testingEditorHost.Children();
    for (uint32_t index = 0; index < testingEditorChildren.Size(); ++index)
    {
        testingEditorChildren.GetAt(index).Visibility(
            index == 0 ? Visibility::Visible : Visibility::Collapsed
            );
    }

    auto testingEditor = Grid();
    testingEditor.HorizontalAlignment(HorizontalAlignment::Stretch);
    testingEditor.VerticalAlignment(VerticalAlignment::Stretch);
    auto testingTabsRow = RowDefinition();
    testingTabsRow.Height(GridLengthHelper::FromPixels(60.0));
    testingEditor.RowDefinitions().Append(testingTabsRow);
    testingEditor.RowDefinitions().Append(RowDefinition());

    auto testingTabs = Pivot();
    testingTabs.IsTabStop(true);
    testingTabs.TabIndex(1);
    testingTabs.HorizontalAlignment(HorizontalAlignment::Stretch);
    applyResourceStyle(testingTabs, L"Phase3TopTabPivotStyle");
    setAutomationName(testingTabs, L"Testing class tabs");
    const auto makeTestingTab = [](std::wstring_view header,
                                   std::wstring_view automationName) {
        auto item = PivotItem();
        item.Header(ClassMngrWinUISharedUX::buildTopTabHeader(
            hstring(header)
            ));
        // Keep the visible editor host separate from the Pivot so switching
        // sections does not recreate the form controls.
        item.Content(Grid());
        setAutomationName(item, automationName);
        return item;
    };
    testingTabs.Items().Append(
        makeTestingTab(L"Details", L"Testing class Details tab")
        );
    testingTabs.Items().Append(
        makeTestingTab(L"Roster", L"Testing class Roster tab")
        );
    testingTabs.Items().Append(
        makeTestingTab(L"Notes", L"Testing class Notes tab")
        );
    testingTabs.SelectedIndex(0);
    testingTabs.SelectionChanged(
        [testingEditorHost](auto const& sender, auto const&) {
            const auto pivot = sender.template try_as<Pivot>();
            if (!pivot)
            {
                return;
            }
            const int32_t selectedIndex = pivot.SelectedIndex();
            const auto children = testingEditorHost.Children();
            if (selectedIndex < 0
                || selectedIndex >= static_cast<int32_t>(children.Size()))
            {
                return;
            }
            for (uint32_t index = 0; index < children.Size(); ++index)
            {
                children.GetAt(index).Visibility(
                    static_cast<int32_t>(index) == selectedIndex
                        ? Visibility::Visible
                        : Visibility::Collapsed
                    );
            }
        }
        );
    Grid::SetRow(testingTabs, 0);
    testingEditor.Children().Append(testingTabs);
    Grid::SetRow(testingEditorHost, 1);
    testingEditor.Children().Append(testingEditorHost);
    setAutomationName(testingEditor, L"Testing class editor");
    Grid::SetColumn(testingEditor, 1);
    testingBody.Children().Append(testingEditor);
    Grid::SetRow(testingBody, 1);
    testingPage.Children().Append(testingBody);
    testingContent.Children().Append(testingPage);

    m_testingClassSaveButton = Button();
    m_testingClassSaveButton.Content(box_value(hstring(L"Save Changes")));
    applyResourceStyle(
        m_testingClassSaveButton,
        L"Phase3PrimaryButtonStyle"
        );
    m_testingClassSaveButton.HorizontalAlignment(HorizontalAlignment::Right);
    m_testingClassSaveButton.Visibility(Visibility::Collapsed);
    m_testingClassSaveButton.Click(
        [this](auto const&, auto const&) { saveTestingClassDetails(); }
        );
    setAutomationName(m_testingClassSaveButton, L"Save testing class changes");
    testingContent.Children().Append(m_testingClassSaveButton);

    const auto openTestingColorPicker = [weak = get_weak()](auto const&, auto const&)
        -> winrt::fire_and_forget {
        const auto self = weak.get();
        if (!self || self->m_ownedDialog || !self->RootGrid().XamlRoot())
        {
            co_return;
        }
        auto picker = ClassMngrWinUISharedUX::buildColorPickerDialog(
            self->RootGrid().XamlRoot(),
            L"Select Testing Class Color",
            uiColorFromHex(self->m_testingClassColor),
            L"Testing class color picker"
            );
        self->m_ownedDialog = picker.dialog;
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await picker.dialog.ShowAsync();
        }
        catch (...)
        {
        }
        if (self->m_ownedDialog == picker.dialog)
        {
            self->m_ownedDialog = nullptr;
        }
        if (result == ContentDialogResult::Primary)
        {
            self->m_testingClassColor = uiHexFromColor(picker.picker.Color());
            self->m_testingClassColorPreview.Background(
                Microsoft::UI::Xaml::Media::SolidColorBrush(
                    uiColorFromHex(self->m_testingClassColor)
                    )
                );
            self->m_testingClassVisualDirty = true;
            self->updateTestingClassActions();
        }
    };
    m_testingClassColorButton.Click(openTestingColorPicker);
    m_testingClassColorPreview.Tapped(openTestingColorPicker);

    const auto openTestingFontColorPicker = [weak = get_weak()](auto const&, auto const&)
        -> winrt::fire_and_forget {
        const auto self = weak.get();
        if (!self || self->m_ownedDialog || !self->RootGrid().XamlRoot())
        {
            co_return;
        }
        auto picker = ClassMngrWinUISharedUX::buildColorPickerDialog(
            self->RootGrid().XamlRoot(),
            L"Select Testing Class Font Color",
            uiColorFromHex(self->m_testingClassFontColor),
            L"Testing class font color picker"
            );
        self->m_ownedDialog = picker.dialog;
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await picker.dialog.ShowAsync();
        }
        catch (...)
        {
        }
        if (self->m_ownedDialog == picker.dialog)
        {
            self->m_ownedDialog = nullptr;
        }
        if (result == ContentDialogResult::Primary)
        {
            self->m_testingClassFontColor = uiHexFromColor(picker.picker.Color());
            self->m_testingClassFontColorPreview.Background(
                Microsoft::UI::Xaml::Media::SolidColorBrush(
                    uiColorFromHex(self->m_testingClassFontColor)
                    )
                );
            self->m_testingClassVisualDirty = true;
            self->updateTestingClassActions();
        }
    };
    m_testingClassFontColorButton.Click(openTestingFontColorPicker);
    m_testingClassFontColorPreview.Tapped(openTestingFontColorPicker);

    const auto markTestingClassVisualDirty = [this](auto const&, auto const&) {
        if (!m_testingClassVisualLoading)
        {
            m_testingClassVisualDirty = true;
            updateTestingClassActions();
        }
    };
    m_testingClassNameTextBox.TextChanging(markTestingClassVisualDirty);
    m_testingClassRoomTextBox.TextChanging(markTestingClassVisualDirty);
    m_testingClassNotesTextBox.TextChanging(markTestingClassVisualDirty);
    m_testingClassGradeCombo.SelectionChanged(markTestingClassVisualDirty);
    m_testingClassLevelCombo.SelectionChanged(markTestingClassVisualDirty);
    m_testingClassTeacherCombo.SelectionChanged(markTestingClassVisualDirty);

    // The assignment editor remains available to the schedule diagnostics and
    // engine smoke checks, but the visible page follows the Qt class editor.
    auto legacyTestingContent = StackPanel();
    legacyTestingContent.Visibility(Visibility::Collapsed);
    m_testingClassSelector = ComboBox();
    m_testingClassSelector.Header(box_value(hstring(L"Existing testing class")));
    m_testingClassSelector.PlaceholderText(L"Select a testing class");
    m_testingClassSelector.MinWidth(320.0);
    m_testingClassSelector.IsTabStop(true);
    m_testingClassSelector.TabIndex(40);
    m_testingClassSelector.SelectionChanged(
        [this](auto const&, auto const&) {
            if (m_testingLoading || !m_testingClassSelector)
            {
                return;
            }
            const auto item = m_testingClassSelector.SelectedItem().try_as<
                ComboBoxItem>();
            const int classId = item ? boxedInt(item.Tag()) : -1;
            for (const auto& testingClass : m_testingClasses)
            {
                if (testingClass.classId != classId)
                {
                    continue;
                }
                m_testingClassNameTextBox.Text(asWide(testingClass.name));
                m_testingClassGradeTextBox.Text(asWide(testingClass.grade));
                m_testingClassLevelTextBox.Text(asWide(testingClass.level));
                m_testingClassRoomTextBox.Text(asWide(testingClass.room));
                break;
            }
        }
        );
    setAutomationName(m_testingClassSelector, L"Testing class selector");
    legacyTestingContent.Children().Append(m_testingClassSelector);
    m_testingClassGradeTextBox = TextBox();
    m_testingClassLevelTextBox = TextBox();
    m_testingCreateButton = Button();
    m_testingCreateButton.Content(box_value(hstring(L"Create testing class")));
    m_testingCreateButton.IsTabStop(true);
    m_testingCreateButton.TabIndex(41);
    m_testingCreateButton.Click(
        [this](auto const&, auto const&) { createTestingClass(); }
        );
    setAutomationName(m_testingCreateButton, L"Create testing class");
    legacyTestingContent.Children().Append(m_testingCreateButton);

    m_testingDayCombo = ComboBox();
    m_testingDayCombo.Header(box_value(hstring(L"Weekday")));
    m_testingDayCombo.MinWidth(220.0);
    m_testingDayCombo.IsTabStop(true);
    m_testingDayCombo.TabIndex(42);
    for (const std::string& day : classmngr::engine::ClassInfoConfig::days())
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(asWide(day))));
        item.Tag(box_value(hstring(asWide(day))));
        setAutomationName(item, asWide(day));
        m_testingDayCombo.Items().Append(item);
    }
    m_testingDayCombo.SelectedIndex(0);
    setAutomationName(m_testingDayCombo, L"Testing assignment weekday");
    legacyTestingContent.Children().Append(m_testingDayCombo);
    m_testingStartTextBox = makeImportBox(
        L"Start time (HH:mm)",
        L"e.g. 09:00",
        L"Testing assignment start time"
        );
    m_testingStartTextBox.Text(L"09:00");
    legacyTestingContent.Children().Append(m_testingStartTextBox);
    m_testingReplaceExistingCheck = CheckBox();
    m_testingReplaceExistingCheck.Content(
        box_value(hstring(L"Replace an existing assignment"))
        );
    m_testingReplaceExistingCheck.IsTabStop(true);
    m_testingReplaceExistingCheck.TabIndex(43);
    setAutomationName(
        m_testingReplaceExistingCheck,
        L"Replace existing testing assignment"
        );
    legacyTestingContent.Children().Append(m_testingReplaceExistingCheck);
    m_testingAssignButton = Button();
    m_testingAssignButton.Content(box_value(hstring(L"Assign selected class")));
    m_testingAssignButton.IsTabStop(true);
    m_testingAssignButton.TabIndex(44);
    m_testingAssignButton.Click(
        [this](auto const&, auto const&) { assignTestingClass(); }
        );
    setAutomationName(m_testingAssignButton, L"Assign testing class");
    legacyTestingContent.Children().Append(m_testingAssignButton);
    m_testingAssignmentList = ListView();
    m_testingAssignmentList.Header(
        box_value(hstring(L"Current testing assignments"))
        );
    m_testingAssignmentList.SelectionMode(ListViewSelectionMode::Single);
    m_testingAssignmentList.IsTabStop(true);
    m_testingAssignmentList.TabIndex(45);
    m_testingAssignmentList.Height(180.0);
    m_testingAssignmentList.SelectionChanged(
        [this](auto const&, auto const&) {
            if (!m_testingLoading && m_testingDeleteAssignmentButton)
            {
                m_testingDeleteAssignmentButton.IsEnabled(
                    m_testingAssignmentList.SelectedIndex() >= 0
                    );
            }
        }
        );
    setAutomationName(m_testingAssignmentList, L"Testing assignment list");
    legacyTestingContent.Children().Append(m_testingAssignmentList);
    m_testingDeleteAssignmentButton = Button();
    m_testingDeleteAssignmentButton.Content(
        box_value(hstring(L"Delete selected assignment"))
        );
    m_testingDeleteAssignmentButton.IsTabStop(true);
    m_testingDeleteAssignmentButton.TabIndex(46);
    m_testingDeleteAssignmentButton.Click(
        [this](auto const&, auto const&) { deleteTestingAssignment(); }
        );
    setAutomationName(
        m_testingDeleteAssignmentButton,
        L"Delete selected testing assignment"
        );
    legacyTestingContent.Children().Append(m_testingDeleteAssignmentButton);
    m_testingStatusText = makeText(L"Testing-class editor is ready.");
    setAutomationName(m_testingStatusText, L"Testing classes status");
    legacyTestingContent.Children().Append(m_testingStatusText);
    m_testingValidationText = makeText(L"");
    m_testingValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(m_testingValidationText, L"Testing classes validation");
    legacyTestingContent.Children().Append(m_testingValidationText);
    testingContent.Children().Append(legacyTestingContent);

    auto testingItem = PivotItem();
    testingItem.Header(box_value(hstring(L"Testing classes")));
    testingItem.Content(scrollTab(testingContent));
    setAutomationName(testingItem, L"Testing classes tab");

    m_scheduleTabs = Pivot();
    // The schedule board owns the visible Regular/Intensive/Testing mode
    // controls. Keep the testing page available to existing command and
    // smoke-test paths, but host schedule import in its modal dialog.
    m_scheduleTabs.IsTabStop(false);
    m_scheduleTabs.TabIndex(0);
    m_scheduleTabs.Items().Append(scheduleItem);
    m_scheduleTabs.Items().Append(testingItem);
    m_scheduleTabs.HeaderTemplate(
        winrt::Microsoft::UI::Xaml::Markup::XamlReader::Load(
            L"<DataTemplate "
            L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/"
            L"presentation\"><Grid Height=\"0\" "
            L"Visibility=\"Collapsed\" /></DataTemplate>"
            )
            .as<winrt::Microsoft::UI::Xaml::DataTemplate>()
        );
    setAutomationName(m_scheduleTabs, L"Schedule workspace tabs");
    scheduleRoot.Children().Append(m_scheduleTabs);
    refreshScheduleWorkspace();
    refreshTestingWorkspace();
}

} // namespace winrt::ClassMngrWinUI::implementation
