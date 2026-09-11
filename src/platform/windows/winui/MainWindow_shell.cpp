#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::restoreShellState()
{
    const auto state = loadShellState();
    m_recentDatabasePaths = state.recentDatabasePaths;
    m_restoringState = true;

    bool restored = false;
    if (!state.navigationState.empty())
    {
        try
        {
            m_contentFrame.SetNavigationState(
                winrt::hstring(state.navigationState)
                );
            restored = !m_currentPageId.empty();
        }
        catch (...)
        {
            restored = false;
        }
    }

    if (!restored)
    {
        navigateTo(
            isKnownPageId(state.selectedPage)
                ? std::wstring_view(state.selectedPage)
                : homePageId
            );
    }
    m_restoringState = false;
    updateNavigationState();
}

void MainWindow::refreshRecentDatabaseMenu()
{
    if (!m_recentFilesMenu)
    {
        return;
    }

    m_recentFilesMenu.Items().Clear();
    if (m_recentDatabasePaths.empty())
    {
        auto empty = Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
        empty.Text(L"No recent databases");
        empty.IsEnabled(false);
        setAutomationName(empty, L"No recent databases");
        m_recentFilesMenu.Items().Append(empty);
        return;
    }

    for (std::wstring const& path : m_recentDatabasePaths)
    {
        auto item = Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
        item.Text(winrt::hstring(path));
        item.Tag(winrt::box_value(winrt::hstring(path)));
        setAutomationName(item, L"Open recent database");
        item.Click({this, &MainWindow::RecentDatabaseMenuItem_Click});
        m_recentFilesMenu.Items().Append(item);
    }
}

void MainWindow::addRecentDatabasePath(std::wstring_view path)
{
    const std::wstring normalized = absolutePath(path);
    std::vector<std::wstring> updated;
    updated.reserve(maximumRecentDatabasePaths);
    updated.emplace_back(normalized);
    for (std::wstring const& existing : m_recentDatabasePaths)
    {
        if (!samePath(existing, normalized))
        {
            updated.emplace_back(existing);
        }
        if (updated.size() == maximumRecentDatabasePaths)
        {
            break;
        }
    }
    m_recentDatabasePaths = pruneRecentDatabasePaths(updated);
    refreshRecentDatabaseMenu();
}

void MainWindow::reportDatabaseOpenError(
    std::wstring_view path,
    std::string_view message
    )
{
    if (m_shellDatabaseStatusText)
    {
        const std::wstring status = path.empty()
            ? L"Database open failed."
            : L"Database open failed: " + std::wstring(path);
        m_shellDatabaseStatusText.Text(winrt::hstring(status));
    }
    if (m_statusText)
    {
        m_statusText.Text(L"Database open failed.");
    }
    showDialog(
        L"Open database",
        winrt::to_hstring(std::string(message)),
        {},
        {},
        L"Close",
        {}
        );
}

void MainWindow::reportOutputError(
    std::wstring_view title,
    std::wstring_view path,
    std::string_view message
    )
{
    if (m_statusText)
    {
        m_statusText.Text(winrt::hstring(
            std::wstring(title) + L" failed."
            ));
    }
    showDialog(
        winrt::hstring(title),
        winrt::to_hstring(std::string(message)),
        {},
        {},
        L"Close",
        {}
        );
    if (m_shellDatabaseStatusText && !path.empty())
    {
        m_shellDatabaseStatusText.Text(winrt::hstring(
            std::wstring(title) + L": " + std::wstring(path)
            ));
    }
}

void MainWindow::updateFileCommandState()
{
    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    if (m_saveFileMenu)
    {
        m_saveFileMenu.IsEnabled(hasDatabase);
    }
    if (m_saveAsFileMenu)
    {
        m_saveAsFileMenu.IsEnabled(hasDatabase && !m_currentDatabasePath.empty());
    }
    if (m_exportFileMenu)
    {
        m_exportFileMenu.IsEnabled(hasDatabase && !m_currentDatabasePath.empty());
    }
    if (m_closeFileMenu)
    {
        m_closeFileMenu.IsEnabled(hasDatabase);
    }

    const bool pageCanBeSaved = isCampusPageId(m_currentPageId)
        && (m_campusInformationState == L"no_database"
            || m_campusInformationState == L"empty"
            || m_campusInformationState == L"populated");
    const bool speakingEvaluationSection =
        m_currentPageId == classesPageId
        && m_classSectionIndex == 3;
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    const bool hasSpeakingReports = speakingEvaluationSection
        && std::any_of(
            m_speakingEvaluationCellBoxes.cbegin(),
            m_speakingEvaluationCellBoxes.cend(),
            [englishColumn, koreanColumn](const auto& cells) {
                return cells.size() > static_cast<std::size_t>(koreanColumn)
                    && (!cells[static_cast<std::size_t>(englishColumn)].Text().empty()
                        || !cells[static_cast<std::size_t>(koreanColumn)].Text().empty());
            }
            );
    if (m_printCurrentPageMenu)
    {
        m_printCurrentPageMenu.Text(
            speakingEvaluationSection
                ? hstring(L"Print Speaking Reports...")
                : hstring(L"Print current page")
            );
        m_printCurrentPageMenu.IsEnabled(hasSpeakingReports);
    }
    if (m_saveCurrentPageMenu)
    {
        m_saveCurrentPageMenu.Text(
            speakingEvaluationSection
                ? hstring(L"Save Speaking Reports As...")
                : hstring(L"Save current page as...")
            );
        m_saveCurrentPageMenu.IsEnabled(pageCanBeSaved || hasSpeakingReports);
    }
    if (m_exportCampusResourcesMenu)
    {
        m_exportCampusResourcesMenu.IsEnabled(pageCanBeSaved);
    }
}

void MainWindow::restoreWindowBounds() noexcept
{
    const auto state = loadShellState();
    if (!state.hasWindowBounds || !isUsableWindowBounds(state.windowBounds))
    {
        return;
    }

    RECT bounds = state.windowBounds;
    if (!moveWindowBoundsIntoWorkArea(&bounds))
    {
        return;
    }

    HWND const handle = windowHandle(this);
    if (!handle)
    {
        return;
    }

    const LONG width = bounds.right - bounds.left;
    const LONG height = bounds.bottom - bounds.top;
    SetWindowPos(
        handle,
        nullptr,
        bounds.left,
        bounds.top,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE
        );
}

void MainWindow::saveShellState() noexcept
{
    HKEY const key = openShellStateForWrite();
    if (!key)
    {
        return;
    }

    writeRegistryString(
        key,
        L"SelectedPage",
        selectedPageId()
        );
    for (std::size_t index = 0; index < maximumRecentDatabasePaths; ++index)
    {
        const std::wstring valueName =
            L"RecentDatabase" + std::to_wstring(index);
        if (index < m_recentDatabasePaths.size())
        {
            writeRegistryString(
                key,
                valueName.c_str(),
                m_recentDatabasePaths[index]
                );
        }
        else
        {
            RegDeleteValueW(key, valueName.c_str());
        }
    }
    try
    {
        const auto navigationState = m_contentFrame.GetNavigationState();
        writeRegistryString(
            key,
            L"NavigationState",
            asWString(navigationState)
            );
    }
    catch (...)
    {
    }

    HWND const handle = windowHandle(this);
    RECT bounds{};
    if (handle && GetWindowRect(handle, &bounds))
    {
        writeRegistryDword(key, L"WindowLeft", static_cast<DWORD>(bounds.left));
        writeRegistryDword(key, L"WindowTop", static_cast<DWORD>(bounds.top));
        writeRegistryDword(key, L"WindowRight", static_cast<DWORD>(bounds.right));
        writeRegistryDword(key, L"WindowBottom", static_cast<DWORD>(bounds.bottom));
    }

    RegCloseKey(key);
}

void MainWindow::updateNavigationState()
{
    if (m_navigationView && m_contentFrame)
    {
        m_navigationView.IsBackEnabled(m_contentFrame.CanGoBack());
    }
}

void MainWindow::restoreNavigationSelection()
{
    if (!m_navigationView)
    {
        return;
    }

    Microsoft::UI::Xaml::Controls::NavigationViewItem selected{nullptr};
    if (m_currentPageId == homePageId)
    {
        selected = m_homeNavigationItem;
    }
    else if (m_currentPageId == subPrepPageId)
    {
        selected = m_subPrepNavigationItem;
    }
    else if (m_currentPageId == classesPageId)
    {
        selected = m_classesNavigationItem;
    }
    else if (m_currentPageId == aboutPageId)
    {
        selected = m_aboutNavigationItem;
    }
    else if (m_currentPageId == campusInformationPageId)
    {
        selected = m_campusInformationNavigationItem;
    }
    else if (m_currentPageId == campusDirectionsPageId)
    {
        selected = m_campusDirectionsNavigationItem;
    }
    else if (m_currentPageId == campusAddressPageId)
    {
        selected = m_campusAddressNavigationItem;
    }
    else if (m_currentPageId == campusHousingPageId)
    {
        selected = m_campusHousingNavigationItem;
    }
    else if (m_currentPageId == campusMapPageId)
    {
        selected = m_campusMapNavigationItem;
    }
    else if (m_currentPageId == koreanTeachersPageId)
    {
        selected = m_koreanTeachersNavigationItem;
    }
    else if (m_currentPageId == nativeEnglishTeachersPageId)
    {
        selected = m_nativeEnglishTeachersNavigationItem;
    }
    else if (m_currentPageId == gsTeamPageId)
    {
        selected = m_gsTeamNavigationItem;
    }

    if (selected)
    {
        m_selectionChanging = true;
        m_navigationView.SelectedItem(selected);
        m_selectionChanging = false;
    }
    updateNavigationState();
}

void MainWindow::confirmClassRosterNavigation(
    std::function<void()> continuation
    )
{
    if (!m_classRosterDirty)
    {
        if (continuation)
        {
            continuation();
        }
        return;
    }

    auto weak = get_weak();
    showDialog(
        L"Unsaved roster changes",
        L"This roster has unsaved changes. Save them before leaving?",
        L"Save",
        L"Discard",
        L"Keep editing",
        [weak, continuation = std::move(continuation)](
            ClassMngrWinUIDialogs::DialogOutcome outcome
            ) mutable {
            if (auto self = weak.get())
            {
                if (outcome
                    == ClassMngrWinUIDialogs::DialogOutcome::Primary)
                {
                    self->saveClassRoster();
                    if (!self->m_classRosterDirty && continuation)
                    {
                        continuation();
                    }
                    return;
                }
                if (outcome
                    == ClassMngrWinUIDialogs::DialogOutcome::Secondary)
                {
                    self->discardClassRoster();
                    if (!self->m_classRosterDirty && continuation)
                    {
                        continuation();
                    }
                    return;
                }

                self->restoreNavigationSelection();
                if (self->m_classStatusText)
                {
                    self->m_classStatusText.Text(
                        L"Continuing to edit unsaved roster changes."
                        );
                }
            }
        }
        );
}

void MainWindow::showOwnedDialog()
{
    showDialog(
        L"ClassMngr shell",
        L"This dialog is owned by MainWindow and uses the active XamlRoot.",
        {},
        {},
        L"Close",
        {}
        );
}

void MainWindow::showUnsavedChangesConfirmation()
{
    if (m_classRosterDirty && !m_classDirty && !m_speakingEvaluationDirty)
    {
        confirmClassRosterNavigation({});
        return;
    }

    if (!m_dirtyState.isDirty())
    {
        if (m_statusText)
        {
            m_statusText.Text(L"No unsaved changes.");
        }
        return;
    }

    auto weak = get_weak();
    showDialog(
        L"Unsaved changes",
        L"Choose Save to let the feature save changes, Discard to discard them, or Keep editing to cancel.",
        L"Save",
        L"Discard",
        L"Keep editing",
        [weak](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (auto self = weak.get())
            {
                switch (ClassMngrWinUIDialogs::resolveUnsavedChanges(
                    self->m_dirtyState,
                    outcome
                    ))
                {
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Save:
                    self->m_statusText.Text(
                        L"Save requested; changes remain dirty until the feature completes it."
                        );
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Discard:
                    self->m_dirtyState.markClean();
                    self->m_statusText.Text(L"Unsaved changes discarded.");
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Stay:
                    self->m_statusText.Text(L"Continuing to edit unsaved changes.");
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Proceed:
                    self->m_statusText.Text(L"No unsaved changes.");
                    break;
                }
            }
        }
        );
}

void MainWindow::showDialog(
    winrt::hstring const& title,
    winrt::hstring const& content,
    winrt::hstring const& primaryText,
    winrt::hstring const& secondaryText,
    winrt::hstring const& closeText,
    std::function<void(ClassMngrWinUIDialogs::DialogOutcome)> completion
    )
{
    const auto xamlRoot = RootGrid().XamlRoot();
    if (m_ownedDialog || !m_contentFrame || !xamlRoot)
    {
        return;
    }

    auto dialog = Microsoft::UI::Xaml::Controls::ContentDialog();
    dialog.XamlRoot(xamlRoot);
    dialog.Title(winrt::box_value(title));
    dialog.Content(winrt::box_value(content));
    dialog.PrimaryButtonText(primaryText);
    dialog.SecondaryButtonText(secondaryText);
    dialog.CloseButtonText(closeText);
    dialog.DefaultButton(
        primaryText.empty()
            ? Microsoft::UI::Xaml::Controls::ContentDialogButton::Close
            : Microsoft::UI::Xaml::Controls::ContentDialogButton::Primary
        );
    m_ownedDialog = dialog;
    completeOwnedDialog(dialog, std::move(completion));
}

winrt::fire_and_forget MainWindow::completeOwnedDialog(
    Microsoft::UI::Xaml::Controls::ContentDialog dialog,
    std::function<void(ClassMngrWinUIDialogs::DialogOutcome)> completion
    )
{
    auto lifetime = get_strong();
    ClassMngrWinUIDialogs::DialogOutcome outcome =
        ClassMngrWinUIDialogs::DialogOutcome::Cancel;
    try
    {
        const auto result = co_await dialog.ShowAsync();
        if (result == Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
        {
            outcome = ClassMngrWinUIDialogs::DialogOutcome::Primary;
        }
        else if (result
            == Microsoft::UI::Xaml::Controls::ContentDialogResult::Secondary)
        {
            outcome = ClassMngrWinUIDialogs::DialogOutcome::Secondary;
        }
    }
    catch (...)
    {
        // Hiding during shell teardown is cancellation, not an error dialog.
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
        if (completion)
        {
            completion(outcome);
        }
    }
}

void MainWindow::updateHomePresentation()
{
    if (!m_homeCommand || !m_progressRing || !m_cancelButton || !m_statusText)
    {
        return;
    }

    const bool running = m_homeCommand->IsRunning();
    m_progressRing.IsActive(running);
    m_progressRing.Visibility(
        running
            ? Microsoft::UI::Xaml::Visibility::Visible
            : Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_cancelButton.IsEnabled(running);
    if (running)
    {
        m_statusText.Text(
            m_homeCommand->IsCancellationRequested()
                ? L"Cancelling operation..."
                : L"Operation in progress..."
            );
    }
    else if (m_statusText.Text() == L"Operation in progress..."
        || m_statusText.Text() == L"Cancelling operation...")
    {
        m_statusText.Text(L"Operation complete.");
    }

    if (m_validationSummaryText && m_homeViewModel)
    {
        m_validationSummaryText.Text(
            m_homeViewModel->HasValidationErrors()
                ? m_homeViewModel->ValidationSummary()
                : L"No validation issues."
            );
    }
}

void MainWindow::presentValidationSummary(
    classmngr::engine::ValidationResult const& validation
    )
{
    if (!m_homeViewModel)
    {
        return;
    }

    m_homeViewModel->PresentValidation(validation);
    updateHomePresentation();
}

void MainWindow::closeShell() noexcept
{
    saveShellState();
    try
    {
        if (m_selectionChangedToken.value != 0)
        {
            m_navigationView.SelectionChanged(m_selectionChangedToken);
            m_selectionChangedToken = {};
        }
        if (m_backRequestedToken.value != 0)
        {
            m_navigationView.BackRequested(m_backRequestedToken);
            m_backRequestedToken = {};
        }
        if (m_navigatedToken.value != 0)
        {
            m_contentFrame.Navigated(m_navigatedToken);
            m_navigatedToken = {};
        }
        if (m_activatedToken.value != 0)
        {
            Activated(m_activatedToken);
            m_activatedToken = {};
        }
        if (m_closedToken.value != 0)
        {
            Closed(m_closedToken);
            m_closedToken = {};
        }
        if (m_ownedDialog)
        {
            m_ownedDialog.Hide();
            m_ownedDialog = nullptr;
        }
        if (m_homeCommand && m_homeCommandStateToken.value != 0)
        {
            m_homeCommand.as<Microsoft::UI::Xaml::Input::ICommand>()
                .CanExecuteChanged(m_homeCommandStateToken);
            m_homeCommandStateToken = {};
        }
        if (m_phase5FirstNavigationRenderingToken.value != 0)
        {
            Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
                m_phase5FirstNavigationRenderingToken
                );
            m_phase5FirstNavigationRenderingToken = {};
        }
        m_homeCommand = nullptr;
        m_homeViewModel = nullptr;
        m_recentFilesMenu = nullptr;
        m_shellDatabaseStatusText = nullptr;
        m_contentFrame = nullptr;
        m_navigationView = nullptr;
        m_openDatabase.reset();
    }
    catch (...)
    {
    }
}

std::wstring MainWindow::selectedPageId() const
{
    if (m_navigationView)
    {
        const auto selectedItem = m_navigationView.SelectedItem().try_as<
            Microsoft::UI::Xaml::Controls::NavigationViewItem>();
        if (selectedItem)
        {
            const std::wstring pageId = boxedString(selectedItem.Tag());
            if (isClassesPageId(pageId))
            {
                return std::wstring(classesPageId);
            }
            if (pageId == personalDetailsPageId)
            {
                return std::wstring(homePageId);
            }
            if (isKnownPageId(pageId))
            {
                return pageId;
            }
        }
    }

    if (m_currentPageId == personalDetailsPageId)
    {
        return std::wstring(homePageId);
    }
    return isKnownPageId(m_currentPageId)
        ? m_currentPageId
        : std::wstring(homePageId);
}

bool MainWindow::ensureHomePage()
{
    if (m_currentPageId != homePageId)
    {
        navigateTo(homePageId);
    }
    return m_currentPageId == homePageId
        && static_cast<bool>(m_nameTextBox)
        && static_cast<bool>(m_continueButton)
        && static_cast<bool>(m_statusText);
}

} // namespace winrt::ClassMngrWinUI::implementation
