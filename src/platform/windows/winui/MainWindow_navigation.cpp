#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

namespace
{
void selectHomeInformationTab(
    Microsoft::UI::Xaml::Controls::Page const& page
    )
{
    const auto workspace = page.Content().try_as<
        Microsoft::UI::Xaml::Controls::Grid>();
    if (!workspace || workspace.Children().Size() < 2)
    {
        return;
    }

    const auto tabs = workspace.Children().GetAt(0).try_as<
        Microsoft::UI::Xaml::Controls::Pivot>();
    const auto content = workspace.Children().GetAt(1).try_as<
        Microsoft::UI::Xaml::Controls::Grid>();
    if (!tabs || !content || tabs.Items().Size() == 0)
    {
        return;
    }

    tabs.SelectedIndex(0);
    const auto children = content.Children();
    for (uint32_t index = 0; index < children.Size(); ++index)
    {
        children.GetAt(index).Visibility(
            index == 0
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed
            );
    }
}
} // namespace

void MainWindow::ShellInfoMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    showOwnedDialog();
}

void MainWindow::CancelButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_homeCommand)
    {
        m_homeCommand->Cancel();
        updateHomePresentation();
    }
}

void MainWindow::UnsavedChangesButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    showUnsavedChangesConfirmation();
}

void MainWindow::ScheduleApplyButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_scheduleSlotTextBox || !m_scheduleStatusText)
    {
        return;
    }

    m_dirtyState.markDirty();
    m_scheduleStatusText.Text(
        L"Time slot label updated in the prototype (not persisted)."
        );
}

void MainWindow::RosterSource_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    const auto list = sender.try_as<Microsoft::UI::Xaml::Controls::ListViewBase>();
    if (!m_rosterStatusText || !list)
    {
        return;
    }

    const auto selected = list.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::TextBox>();
    if (selected)
    {
        m_rosterStatusText.Text(
            winrt::hstring(L"Selected " + std::wstring(
                selected.Text().c_str(), selected.Text().size()
                ) + L" for transfer.")
            );
    }
}

void MainWindow::RosterTransferButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_rosterSourceList || !m_rosterTransferredList
        || !m_rosterStatusText)
    {
        return;
    }

    const int32_t selectedIndex = m_rosterSourceList.SelectedIndex();
    const auto selected = m_rosterSourceList.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::TextBox>();
    if (selectedIndex < 0 || !selected)
    {
        m_rosterStatusText.Text(L"Select a student before transferring.");
        return;
    }

    auto transferred = Microsoft::UI::Xaml::Controls::TextBox();
    transferred.Text(selected.Text());
    transferred.Header(winrt::box_value(winrt::hstring(L"Student name")));
    transferred.IsTabStop(true);
    setAutomationName(transferred, L"Transferred student name");
    transferred.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    m_rosterSourceList.Items().RemoveAt(
        static_cast<uint32_t>(selectedIndex)
        );
    m_rosterTransferredList.Items().Append(transferred);
    m_rosterTransferredList.SelectedItem(transferred);
    m_dirtyState.markDirty();
    m_rosterStatusText.Text(L"Student transferred in the prototype.");
}

void MainWindow::SpeakingPasteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_speakingPasteTextBox || !m_speakingStatusText)
    {
        return;
    }

    const auto pastedText = m_speakingPasteTextBox.Text();
    const auto rows = parsePastedRange(std::wstring_view(
        pastedText.c_str(),
        pastedText.size()
        ));
    constexpr size_t scoreColumns = 3;
    size_t applied = 0;
    for (size_t row = 0; row < rows.size() && row < 3; ++row)
    {
        for (size_t column = 0;
             column < rows[row].size() && column < scoreColumns;
             ++column)
        {
            const size_t cellIndex = row * scoreColumns + column;
            if (cellIndex < m_speakingScoreCells.size())
            {
                m_speakingScoreCells[cellIndex].Text(
                    winrt::hstring(rows[row][column])
                    );
                ++applied;
            }
        }
    }

    if (applied == 0)
    {
        m_speakingStatusText.Text(L"Paste a tab/newline range to apply scores.");
        return;
    }

    m_dirtyState.markDirty();
    m_speakingStatusText.Text(winrt::hstring(
        L"Applied " + std::to_wstring(applied)
            + L" pasted score cells in the prototype."
        ));
}

void MainWindow::SpeakingAnalyticsButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_speakingStatusText)
    {
        m_speakingStatusText.Text(
            L"Analytics navigation requested for the selected roster."
            );
    }
}

void MainWindow::NavigationView_SelectionChanged(
    Microsoft::UI::Xaml::Controls::NavigationView const& sender,
    Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    if (m_selectionChanging)
    {
        return;
    }

    const auto selectedItem = arguments.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    if (!selectedItem)
    {
        return;
    }

    std::wstring pageId = boxedString(selectedItem.Tag());
    if (pageId == personalDetailsPageId)
    {
        pageId = std::wstring(homePageId);
    }
    if (pageId == homePageId)
    {
        navigateTo(homePageId);
        const auto homePage = m_contentFrame.Content().try_as<
            Microsoft::UI::Xaml::Controls::Page>();
        if (homePage)
        {
            selectHomeInformationTab(homePage);
        }
        return;
    }
    if (selectedItem == m_subPrepNavigationItem)
    {
        navigateTo(subPrepPageId);
        return;
    }
    if (isClassesPageId(pageId))
    {
        navigateTo(classesPageId);
        return;
    }
    if (pageId == L"campus_info")
    {
        pageId = std::wstring(campusInformationPageId);
    }
    navigateTo(pageId);
}

void MainWindow::NavigationView_BackRequested(
    Microsoft::UI::Xaml::Controls::NavigationView const& sender,
    Microsoft::UI::Xaml::Controls::NavigationViewBackRequestedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_classRosterDirty)
    {
        restoreNavigationSelection();
        confirmClassRosterNavigation([this]() {
            if (m_contentFrame.CanGoBack())
            {
                m_contentFrame.GoBack();
            }
            updateNavigationState();
        });
        return;
    }
    if (m_contentFrame.CanGoBack())
    {
        m_contentFrame.GoBack();
    }
    updateNavigationState();
}

void MainWindow::ContentFrame_Navigated(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& arguments
    )
{
    static_cast<void>(sender);

    const auto page = arguments.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (!page)
    {
        return;
    }

    std::wstring pageId = boxedString(arguments.Parameter());
    if (pageId == personalDetailsPageId)
    {
        pageId = std::wstring(homePageId);
    }
    if (isClassesPageId(pageId))
    {
        pageId = std::wstring(classesPageId);
    }
    if (pageId == L"campus_info")
    {
        pageId = std::wstring(campusInformationPageId);
    }
    if (!isKnownPageId(pageId))
    {
        pageId = std::wstring(homePageId);
    }
    populatePage(page, pageId);
    if (pageId == homePageId)
    {
        selectHomeInformationTab(page);
    }
    m_currentPageId = pageId;

    m_selectionChanging = true;
    m_navigationView.SelectedItem(
        pageId == homePageId
            ? m_homeNavigationItem
            : pageId == koreanTeachersPageId
                ? m_koreanTeachersNavigationItem
            : pageId == nativeEnglishTeachersPageId
                ? m_nativeEnglishTeachersNavigationItem
            : pageId == gsTeamPageId
                ? m_gsTeamNavigationItem
            : pageId == subPrepPageId
                ? m_subPrepNavigationItem
            : pageId == classesPageId
                ? m_classesNavigationItem
                : pageId == aboutPageId
                ? m_aboutNavigationItem
                : pageId == campusInformationPageId
                    ? m_campusInformationNavigationItem
                    : pageId == campusDirectionsPageId
                        ? m_campusDirectionsNavigationItem
                        : pageId == campusAddressPageId
                            ? m_campusAddressNavigationItem
                            : pageId == campusHousingPageId
                                ? m_campusHousingNavigationItem
                                : m_campusMapNavigationItem
        );
    m_selectionChanging = false;
    updateNavigationState();
    updateFileCommandState();
    if (!m_restoringState)
    {
        saveShellState();
    }
}

void MainWindow::Window_Activated(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::WindowActivatedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_windowBoundsRestored)
    {
        restoreWindowBounds();
        m_windowBoundsRestored = true;
    }
}

void MainWindow::Window_Closed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::WindowEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    saveShellState();
}

void MainWindow::navigateTo(std::wstring_view pageId)
{
    if (pageId == personalDetailsPageId)
    {
        pageId = homePageId;
    }
    if (isClassesPageId(pageId))
    {
        pageId = classesPageId;
    }
    if (!isKnownPageId(pageId))
    {
        return;
    }

    if (m_currentPageId == pageId && m_contentFrame.Content())
    {
        updateNavigationState();
        return;
    }

    if (m_classRosterDirty)
    {
        const std::wstring requestedPage(pageId);
        restoreNavigationSelection();
        confirmClassRosterNavigation([this, requestedPage]() {
            navigateTo(requestedPage);
        });
        return;
    }

    static_cast<void>(m_contentFrame.Navigate(
        winrt::xaml_typename<Microsoft::UI::Xaml::Controls::Page>(),
        winrt::box_value(winrt::hstring(pageId))
        ));
}

void MainWindow::populatePage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    std::wstring_view pageId
    )
{
    if (page.Content())
    {
        // Campus Information depends on the active database.  A cached page
        // may have been constructed before File > Open completed, so it must
        // be rehydrated whenever navigation returns to it instead of
        // retaining the previous no-database/empty state.
        if (isCampusPageId(pageId))
        {
            populateCampusPage(page, pageId, true);
        }
        else if (pageId == personalDetailsPageId)
        {
            populatePersonalDetailsPage(page, true);
        }
        else if (pageId == koreanTeachersPageId)
        {
            populateKoreanTeachersPage(page, true);
        }
        else if (pageId == nativeEnglishTeachersPageId)
        {
            populateNativeEnglishTeachersPage(page, true);
        }
        else if (pageId == gsTeamPageId)
        {
            populateGsTeamPage(page, true);
        }
        else if (pageId == subPrepPageId)
        {
            refreshSubPrepPage();
        }
        else if (pageId == classesPageId)
        {
            refreshClassesPage();
        }
        else if (pageId == homePageId && !m_engineVersionText)
        {
            populateHomePage(page);
        }
        else if (pageId == homePageId)
        {
            populatePersonalDetailsPage(page, true);
            refreshScheduleWorkspace();
            refreshTestingWorkspace();
            refreshCalendarPage();
        }
        return;
    }

    if (pageId == homePageId)
    {
        populateHomePage(page);
    }
    else if (pageId == personalDetailsPageId)
    {
        populatePersonalDetailsPage(page, false);
    }
    else if (pageId == koreanTeachersPageId)
    {
        populateKoreanTeachersPage(page, false);
    }
    else if (pageId == nativeEnglishTeachersPageId)
    {
        populateNativeEnglishTeachersPage(page, false);
    }
    else if (pageId == gsTeamPageId)
    {
        populateGsTeamPage(page, false);
    }
    else if (pageId == subPrepPageId)
    {
        populateSubPrepPage(page, false);
    }
    else if (isClassesPageId(pageId))
    {
        populateClassesPage(page, pageId);
    }
    else if (isCampusPageId(pageId))
    {
        populateCampusPage(page, pageId, false);
    }
    else
    {
        populateAboutPage(page);
    }
}

void MainWindow::populateHomePage(
    Microsoft::UI::Xaml::Controls::Page const& page
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto root = StackPanel();
    root.Padding(Thickness{32.0, 32.0, 32.0, 32.0});
    root.Spacing(16.0);
    root.MaxWidth(720.0);
    root.HorizontalAlignment(HorizontalAlignment::Center);
    root.VerticalAlignment(VerticalAlignment::Top);

    auto title = TextBlock();
    title.Text(L"ClassMngr Windows");
    title.FontSize(32.0);
    setAutomationName(title, L"ClassMngr Windows");

    m_engineVersionText = TextBlock();
    m_engineVersionText.FontSize(16.0);
    m_engineVersionText.Text(
        winrt::to_hstring(
            std::string("Engine version: ") + m_engineVersion.toString()
            )
        );
    setAutomationName(m_engineVersionText, L"Engine version");

    auto description = TextBlock();
    description.Text(L"The Windows shell is ready for feature pages.");
    description.TextWrapping(TextWrapping::Wrap);

    m_nameTextBox = TextBox();
    m_nameTextBox.Header(winrt::box_value(winrt::hstring(L"ì´ë¦„")));
    m_nameTextBox.PlaceholderText(L"í•œêµ­ì–´ ìž…ë ¥");
    auto inputScope = Input::InputScope();
    inputScope.Names().Append(
        Input::InputScopeName(Input::InputScopeNameValue::Text)
        );
    m_nameTextBox.InputScope(inputScope);
    m_nameTextBox.TabIndex(0);
    m_nameTextBox.IsTabStop(true);
    m_nameTextBox.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    setAutomationName(m_nameTextBox, L"Name input");

    m_continueButton = Button();
    m_continueButton.Content(winrt::box_value(winrt::hstring(L"Continue")));
    m_continueButton.TabIndex(1);
    m_continueButton.IsTabStop(true);
    m_continueButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_continueButton.Click({this, &MainWindow::ContinueButton_Click});
    setAutomationName(m_continueButton, L"Continue");

    m_progressRing = ProgressRing();
    m_progressRing.IsActive(false);
    m_progressRing.Visibility(Visibility::Collapsed);
    m_progressRing.Width(24.0);
    m_progressRing.Height(24.0);
    m_progressRing.HorizontalAlignment(HorizontalAlignment::Left);
    setAutomationName(m_progressRing, L"Operation progress");

    m_cancelButton = Button();
    m_cancelButton.Content(winrt::box_value(winrt::hstring(L"Cancel operation")));
    m_cancelButton.TabIndex(2);
    m_cancelButton.IsTabStop(true);
    m_cancelButton.IsEnabled(false);
    m_cancelButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_cancelButton.Click({this, &MainWindow::CancelButton_Click});
    setAutomationName(m_cancelButton, L"Cancel current operation");

    m_statusText = TextBlock();
    m_statusText.Text(L"Ready");
    m_statusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_statusText, L"Status");

    m_validationSummaryText = TextBlock();
    m_validationSummaryText.Text(L"No validation issues.");
    m_validationSummaryText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_validationSummaryText, L"Validation summary");

    m_unsavedChangesButton = Button();
    m_unsavedChangesButton.Content(
        winrt::box_value(winrt::hstring(L"Review unsaved changes"))
        );
    m_unsavedChangesButton.TabIndex(3);
    m_unsavedChangesButton.IsTabStop(true);
    m_unsavedChangesButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_unsavedChangesButton.Click({this, &MainWindow::UnsavedChangesButton_Click});
    setAutomationName(m_unsavedChangesButton, L"Review unsaved changes");

    auto scheduleCard = ClassMngrWinUISharedUX::buildCard({
        L"Schedule/time-slot editing prototype",
        L"Edit a representative time-slot label in memory. This prototype does not persist data or apply engine rules.",
        L"Schedule editor"
        });
    m_scheduleSlotTextBox = TextBox();
    m_scheduleSlotTextBox.Header(
        winrt::box_value(winrt::hstring(L"Time-slot label"))
        );
    m_scheduleSlotTextBox.Text(L"09:00â€“09:45");
    m_scheduleSlotTextBox.PlaceholderText(L"e.g. 09:00â€“09:45");
    m_scheduleSlotTextBox.IsTabStop(true);
    m_scheduleSlotTextBox.TabIndex(4);
    m_scheduleSlotTextBox.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    setAutomationName(m_scheduleSlotTextBox, L"Schedule time-slot label editor");

    auto scheduleApply = Button();
    scheduleApply.Content(winrt::box_value(winrt::hstring(L"Apply time-slot label")));
    scheduleApply.IsTabStop(true);
    scheduleApply.TabIndex(5);
    scheduleApply.HorizontalAlignment(HorizontalAlignment::Left);
    scheduleApply.Click({this, &MainWindow::ScheduleApplyButton_Click});
    setAutomationName(scheduleApply, L"Schedule apply time-slot label");

    m_scheduleStatusText = TextBlock();
    m_scheduleStatusText.Text(L"Ready to edit a time slot.");
    m_scheduleStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_scheduleStatusText, L"Schedule editor status");
    scheduleCard.content.Children().Append(m_scheduleSlotTextBox);
    scheduleCard.content.Children().Append(scheduleApply);
    scheduleCard.content.Children().Append(m_scheduleStatusText);

    auto rosterCard = ClassMngrWinUISharedUX::buildCard({
        L"Roster selection, transfer, and keyboard editing prototype",
        L"Select a student, edit the name with the keyboard, and simulate a transfer between lists.",
        L"Roster editor"
        });
    auto rosterLists = StackPanel();
    rosterLists.Orientation(Orientation::Horizontal);
    rosterLists.Spacing(8.0);

    m_rosterSourceList = ListView();
    m_rosterSourceList.Header(
        winrt::box_value(winrt::hstring(L"Available students"))
        );
    m_rosterSourceList.SelectionMode(ListViewSelectionMode::Single);
    m_rosterSourceList.IsTabStop(true);
    m_rosterSourceList.Width(220.0);
    m_rosterSourceList.Height(150.0);
    setAutomationName(m_rosterSourceList, L"Roster available students");
    m_rosterSourceList.SelectionChanged({this, &MainWindow::RosterSource_SelectionChanged});
    for (auto const& name : {L"ê¹€ë¯¼ì„œ", L"Alex Kim", L"ë°•ì„œì¤€"})
    {
        auto student = TextBox();
        student.Header(winrt::box_value(winrt::hstring(L"Student name")));
        student.Text(name);
        student.IsTabStop(true);
        student.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
        setAutomationName(student, L"Roster editable student name");
        m_rosterSourceList.Items().Append(student);
    }

    m_rosterTransferredList = ListView();
    m_rosterTransferredList.Header(
        winrt::box_value(winrt::hstring(L"Transferred students"))
        );
    m_rosterTransferredList.SelectionMode(ListViewSelectionMode::Single);
    m_rosterTransferredList.IsTabStop(true);
    m_rosterTransferredList.Width(220.0);
    m_rosterTransferredList.Height(150.0);
    setAutomationName(m_rosterTransferredList, L"Roster transferred students");

    rosterLists.Children().Append(m_rosterSourceList);
    rosterLists.Children().Append(m_rosterTransferredList);
    auto rosterTransfer = Button();
    rosterTransfer.Content(winrt::box_value(winrt::hstring(L"Transfer selected â†’")));
    rosterTransfer.IsTabStop(true);
    rosterTransfer.TabIndex(6);
    rosterTransfer.HorizontalAlignment(HorizontalAlignment::Left);
    rosterTransfer.Click({this, &MainWindow::RosterTransferButton_Click});
    setAutomationName(rosterTransfer, L"Roster transfer selected student");
    m_rosterStatusText = TextBlock();
    m_rosterStatusText.Text(L"Select a roster row to begin.");
    m_rosterStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_rosterStatusText, L"Roster selected student status");
    rosterCard.content.Children().Append(rosterLists);
    rosterCard.content.Children().Append(rosterTransfer);
    rosterCard.content.Children().Append(m_rosterStatusText);

    auto speakingCard = ClassMngrWinUISharedUX::buildCard({
        L"Speaking-evaluation scores and analytics prototype",
        L"Edit score cells, apply a tab/newline range, and request analytics navigation using standard controls.",
        L"Speaking evaluation editor"
        });
    m_speakingScoreCells.clear();
    auto scoreGrid = Grid();
    for (size_t column = 0; column < 4; ++column)
    {
        scoreGrid.ColumnDefinitions().Append(ColumnDefinition());
    }
    for (size_t row = 0; row < 4; ++row)
    {
        scoreGrid.RowDefinitions().Append(RowDefinition());
    }
    setAutomationName(scoreGrid, L"Speaking evaluation score grid");

    auto addScoreHeader = [&scoreGrid](wchar_t const* text, uint32_t row, uint32_t column) {
        auto header = TextBlock();
        header.Text(text);
        header.Margin(Thickness{4.0, 2.0, 4.0, 2.0});
        Grid::SetRow(header, row);
        Grid::SetColumn(header, column);
        scoreGrid.Children().Append(header);
    };
    addScoreHeader(L"Student", 0, 0);
    addScoreHeader(L"Pronunciation", 0, 1);
    addScoreHeader(L"Fluency", 0, 2);
    addScoreHeader(L"Interaction", 0, 3);
    const std::array<wchar_t const*, 3> studentNames{
        L"ê¹€ë¯¼ì„œ", L"Alex Kim", L"ë°•ì„œì¤€"
    };
    const std::array<std::array<wchar_t const*, 3>, 3> initialScores{{
        {{L"8", L"7", L"9"}},
        {{L"9", L"8", L"8"}},
        {{L"7", L"8", L"7"}}
    }};
    for (uint32_t row = 0; row < 3; ++row)
    {
        addScoreHeader(studentNames[row], row + 1, 0);
        for (uint32_t column = 0; column < 3; ++column)
        {
            auto score = TextBox();
            score.Text(initialScores[row][column]);
            score.Width(76.0);
            score.Margin(Thickness{4.0, 2.0, 4.0, 2.0});
            score.IsTabStop(true);
            score.TabIndex(7 + row * 3 + column);
            score.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
            const std::wstring scoreAutomationName =
                L"Speaking score " + std::to_wstring(row + 1) + L" "
                + std::to_wstring(column + 1);
            setAutomationName(score, scoreAutomationName);
            Grid::SetRow(score, row + 1);
            Grid::SetColumn(score, column + 1);
            scoreGrid.Children().Append(score);
            m_speakingScoreCells.emplace_back(score);
        }
    }

    m_speakingPasteTextBox = TextBox();
    m_speakingPasteTextBox.Header(
        winrt::box_value(winrt::hstring(L"Paste scores (tab/newline range)"))
        );
    m_speakingPasteTextBox.PlaceholderText(L"8\t7\t9\n9\t8\t8");
    m_speakingPasteTextBox.AcceptsReturn(true);
    m_speakingPasteTextBox.Height(72.0);
    m_speakingPasteTextBox.IsTabStop(true);
    m_speakingPasteTextBox.TabIndex(20);
    setAutomationName(m_speakingPasteTextBox, L"Speaking pasted score range");
    auto speakingPaste = Button();
    speakingPaste.Content(winrt::box_value(winrt::hstring(L"Apply pasted range")));
    speakingPaste.IsTabStop(true);
    speakingPaste.TabIndex(21);
    speakingPaste.HorizontalAlignment(HorizontalAlignment::Left);
    speakingPaste.Click({this, &MainWindow::SpeakingPasteButton_Click});
    setAutomationName(speakingPaste, L"Apply speaking pasted score range");
    auto speakingAnalytics = Button();
    speakingAnalytics.Content(winrt::box_value(winrt::hstring(L"Open analytics")));
    speakingAnalytics.IsTabStop(true);
    speakingAnalytics.TabIndex(22);
    speakingAnalytics.HorizontalAlignment(HorizontalAlignment::Left);
    speakingAnalytics.Click({this, &MainWindow::SpeakingAnalyticsButton_Click});
    setAutomationName(speakingAnalytics, L"Open speaking analytics");
    m_speakingStatusText = TextBlock();
    m_speakingStatusText.Text(L"Scores ready for editing.");
    m_speakingStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_speakingStatusText, L"Speaking analytics navigation status");
    speakingCard.content.Children().Append(scoreGrid);
    speakingCard.content.Children().Append(m_speakingPasteTextBox);
    speakingCard.content.Children().Append(speakingPaste);
    speakingCard.content.Children().Append(speakingAnalytics);
    speakingCard.content.Children().Append(m_speakingStatusText);

    m_homeViewModel = winrt::make_self<ObservableViewModel>();
    const auto command = winrt::make_self<AsyncCommand>(
        DispatcherQueue(),
        phase3PresentationWork
        );
    const auto commandInterface = command.as<Microsoft::UI::Xaml::Input::ICommand>();
    auto weak = get_weak();
    m_homeCommandStateToken = commandInterface.CanExecuteChanged(
        [weak](
            Windows::Foundation::IInspectable const&,
            Windows::Foundation::IInspectable const&
            ) {
            if (auto self = weak.get())
            {
                self->updateHomePresentation();
            }
        }
        );
    m_homeCommand = command;

    root.Children().Append(title);
    root.Children().Append(m_engineVersionText);
    root.Children().Append(description);
    root.Children().Append(m_nameTextBox);
    root.Children().Append(m_continueButton);
    root.Children().Append(m_progressRing);
    root.Children().Append(m_cancelButton);
    root.Children().Append(m_statusText);
    root.Children().Append(m_validationSummaryText);
    root.Children().Append(m_unsavedChangesButton);

    auto scheduleRoot = StackPanel();
    scheduleRoot.Padding(Thickness{16.0, 12.0, 16.0, 24.0});
    scheduleRoot.Spacing(12.0);
    scheduleRoot.MaxWidth(1260.0);
    scheduleRoot.HorizontalAlignment(HorizontalAlignment::Center);
    // Keep the original phase-4 controls available to the semantic hook, but
    // make the engine-backed workspace below the only user-facing schedule
    // editor.
    scheduleCard.root.Visibility(Visibility::Collapsed);
    scheduleRoot.Children().Append(scheduleCard.root);
    populateScheduleWorkspace(scheduleRoot);

    auto calendarRoot = StackPanel();
    calendarRoot.Padding(Thickness{32.0, 16.0, 32.0, 32.0});
    calendarRoot.Spacing(16.0);
    calendarRoot.MaxWidth(900.0);
    calendarRoot.HorizontalAlignment(HorizontalAlignment::Center);
    populateCalendarWorkspace(calendarRoot);

    const auto scrollTab = [](StackPanel const& content) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.Content(content);
        return scroll;
    };
    const auto makeWorkspaceTab = [](wchar_t const* header,
                                     wchar_t const* automationName) {
        auto item = PivotItem();
        item.Header(winrt::box_value(winrt::hstring(header)));
        // The Pivot is used only for the tab strip.  Keeping the page
        // content outside the Pivot avoids losing the selected item's visual
        // tree when the Home page is cached by Frame navigation.
        item.Content(Grid());
        setAutomationName(item, automationName);
        return item;
    };

    auto personalDetailsHost = ContentControl();
    populatePersonalDetailsPage(personalDetailsHost, false);
    auto workspaceContent = Grid();
    setAutomationName(workspaceContent, L"My Workspace content");
    workspaceContent.Children().Append(personalDetailsHost);
    workspaceContent.Children().Append(scrollTab(scheduleRoot));
    workspaceContent.Children().Append(scrollTab(calendarRoot));
    const auto workspaceChildren = workspaceContent.Children();
    for (uint32_t index = 0; index < workspaceChildren.Size(); ++index)
    {
        workspaceChildren.GetAt(index).Visibility(
            index == 0 ? Visibility::Visible : Visibility::Collapsed
            );
    }

    auto tabs = Pivot();
    tabs.IsTabStop(true);
    tabs.TabIndex(0);
    setAutomationName(tabs, L"My Workspace tabs");
    tabs.Items().Append(makeWorkspaceTab(
        L"My Information",
        L"My Information workspace tab"
        ));
    tabs.Items().Append(makeWorkspaceTab(
        L"Schedule",
        L"Schedule workspace tab"
        ));
    tabs.Items().Append(makeWorkspaceTab(
        L"Calendar",
        L"Calendar workspace tab"
        ));
    tabs.SelectedIndex(0);
    tabs.SelectionChanged([workspaceContent](auto const& sender, auto const&) {
        const auto pivot = sender.template try_as<Pivot>();
        if (!pivot)
        {
            return;
        }

        const int32_t selectedIndex = pivot.SelectedIndex();
        const auto children = workspaceContent.Children();
        if (selectedIndex < 0
            || selectedIndex >= static_cast<int32_t>(children.Size()))
        {
            // Pivot briefly reports no selection while a cached Home page is
            // reattached. Keep the current content instead of collapsing all
            // workspace panels during that transient state.
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
    });
    tabs.Loaded([workspaceContent](auto const& sender, auto const&) {
        const auto pivot = sender.template try_as<Pivot>();
        if (!pivot)
        {
            return;
        }

        // Reattached cached pages can restore a transient Pivot selection
        // after NavigationFrame::Navigated has already selected this tab.
        // Reset the tab and its sibling content once the Pivot is loaded.
        pivot.SelectedIndex(0);
        const auto children = workspaceContent.Children();
        for (uint32_t index = 0; index < children.Size(); ++index)
        {
            children.GetAt(index).Visibility(
                index == 0 ? Visibility::Visible : Visibility::Collapsed
                );
        }
    });

    auto workspace = Grid();
    auto tabsRow = RowDefinition();
    tabsRow.Height(GridLengthHelper::FromPixels(60.0));
    workspace.RowDefinitions().Append(tabsRow);
    workspace.RowDefinitions().Append(RowDefinition());
    Grid::SetRow(tabs, 0);
    Grid::SetRow(workspaceContent, 1);
    workspace.Children().Append(tabs);
    workspace.Children().Append(workspaceContent);
    page.Content(workspace);
}

} // namespace winrt::ClassMngrWinUI::implementation
