#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populateGsTeamPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_gsTeamNameTextBox)
    {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);

        auto root = StackPanel();
        root.Padding(Thickness{32.0, 24.0, 32.0, 32.0});
        root.Spacing(16.0);
        root.MaxWidth(900.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"GS Team");
        title.FontSize(24.0);
        setAutomationName(title, L"GS Team");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"View and maintain GS and CS team contact information used by classes and reports."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(description, L"GS Team directory description");
        root.Children().Append(description);

        m_gsTeamStatusText = TextBlock();
        m_gsTeamStatusText.Text(L"Loading GS Team...");
        m_gsTeamStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(m_gsTeamStatusText, L"GS Team directory status");
        root.Children().Append(m_gsTeamStatusText);

        m_gsTeamValidationText = TextBlock();
        m_gsTeamValidationText.TextWrapping(TextWrapping::Wrap);
        m_gsTeamValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(m_gsTeamValidationText, L"GS Team validation summary");
        root.Children().Append(m_gsTeamValidationText);

        auto directoryCard = ClassMngrWinUISharedUX::buildCard({
            L"Team directory",
            L"Select an existing team member or create a new directory entry.",
            L"GS Team directory selector"
            });
        m_gsTeamSelector = ComboBox();
        m_gsTeamSelector.Header(box_value(hstring(L"Team member")));
        m_gsTeamSelector.PlaceholderText(L"Select a team member");
        m_gsTeamSelector.MinWidth(420.0);
        m_gsTeamSelector.HorizontalAlignment(HorizontalAlignment::Stretch);
        m_gsTeamSelector.IsTabStop(true);
        m_gsTeamSelector.TabIndex(0);
        m_gsTeamSelector.SelectionChanged({
            this,
            &MainWindow::GsTeamSelection_SelectionChanged
            });
        setAutomationName(m_gsTeamSelector, L"GS Team member selector");
        directoryCard.content.Children().Append(m_gsTeamSelector);
        root.Children().Append(directoryCard.root);

        const auto makeTextBox = [this](
            wchar_t const* header,
            wchar_t const* automationName,
            wchar_t const* placeholder
            ) {
            auto box = TextBox();
            box.Header(box_value(hstring(header)));
            box.PlaceholderText(placeholder);
            box.MinWidth(320.0);
            box.HorizontalAlignment(HorizontalAlignment::Stretch);
            box.IsTabStop(true);
            box.TextChanging({this, &MainWindow::GsTeamField_TextChanging});
            setAutomationName(box, automationName);
            return box;
        };

        auto detailsCard = ClassMngrWinUISharedUX::buildCard({
            L"Team member details",
            L"Names, position, contact information, and birthday.",
            L"GS Team member details form"
            });
        m_gsTeamNameTextBox = makeTextBox(
            L"Name",
            L"GS Team member name",
            L"Enter the English name"
            );
        auto nameInputScope = Input::InputScope();
        nameInputScope.Names().Append(
            Input::InputScopeName(Input::InputScopeNameValue::Text)
            );
        m_gsTeamNameTextBox.InputScope(nameInputScope);
        m_gsTeamNameTextBox.TabIndex(1);

        m_gsTeamKoreanNameTextBox = makeTextBox(
            L"Korean Name",
            L"GS Team member Korean name",
            L"Enter the Korean name"
            );
        auto koreanInputScope = Input::InputScope();
        koreanInputScope.Names().Append(
            Input::InputScopeName(Input::InputScopeNameValue::Text)
            );
        m_gsTeamKoreanNameTextBox.InputScope(koreanInputScope);
        m_gsTeamKoreanNameTextBox.TabIndex(2);

        m_gsTeamPositionTextBox = makeTextBox(
            L"Position",
            L"GS Team member position",
            L"e.g. Branch Manager"
            );
        m_gsTeamPositionTextBox.TabIndex(3);
        m_gsTeamPhoneTextBox = makeTextBox(
            L"Phone Number",
            L"GS Team member phone number",
            L"Optional phone number"
            );
        m_gsTeamPhoneTextBox.TabIndex(4);
        m_gsTeamBirthdayTextBox = makeTextBox(
            L"Birthday (MM-dd)",
            L"GS Team member birthday",
            L"e.g. 03-14"
            );
        m_gsTeamBirthdayTextBox.TabIndex(5);
        detailsCard.content.Children().Append(m_gsTeamNameTextBox);
        detailsCard.content.Children().Append(m_gsTeamKoreanNameTextBox);
        detailsCard.content.Children().Append(m_gsTeamPositionTextBox);
        detailsCard.content.Children().Append(m_gsTeamPhoneTextBox);
        detailsCard.content.Children().Append(m_gsTeamBirthdayTextBox);
        root.Children().Append(detailsCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_gsTeamNewButton = Button();
        m_gsTeamNewButton.Content(box_value(hstring(L"New Member")));
        m_gsTeamNewButton.IsTabStop(true);
        m_gsTeamNewButton.TabIndex(6);
        m_gsTeamNewButton.Click({this, &MainWindow::GsTeamNewButton_Click});
        setAutomationName(m_gsTeamNewButton, L"New GS Team member");
        m_gsTeamDeleteButton = Button();
        m_gsTeamDeleteButton.Content(box_value(hstring(L"Delete Member")));
        m_gsTeamDeleteButton.IsTabStop(true);
        m_gsTeamDeleteButton.TabIndex(7);
        m_gsTeamDeleteButton.Click({this, &MainWindow::GsTeamDeleteButton_Click});
        setAutomationName(m_gsTeamDeleteButton, L"Delete GS Team member");
        m_gsTeamSaveButton = Button();
        m_gsTeamSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_gsTeamSaveButton.IsTabStop(true);
        m_gsTeamSaveButton.TabIndex(8);
        m_gsTeamSaveButton.Click({this, &MainWindow::GsTeamSaveButton_Click});
        setAutomationName(m_gsTeamSaveButton, L"Save GS Team member");
        m_gsTeamDiscardButton = Button();
        m_gsTeamDiscardButton.Content(
            box_value(hstring(L"Discard Changes"))
            );
        m_gsTeamDiscardButton.IsTabStop(true);
        m_gsTeamDiscardButton.TabIndex(9);
        m_gsTeamDiscardButton.Click({
            this,
            &MainWindow::GsTeamDiscardButton_Click
            });
        setAutomationName(
            m_gsTeamDiscardButton,
            L"Discard GS Team member changes"
            );
        actions.Children().Append(m_gsTeamNewButton);
        actions.Children().Append(m_gsTeamDeleteButton);
        actions.Children().Append(m_gsTeamSaveButton);
        actions.Children().Append(m_gsTeamDiscardButton);
        root.Children().Append(actions);

        scroll.Content(root);
        page.Content(scroll);
    }

    const auto setEditable = [this](bool enabled) {
        const bool clean = !m_gsTeamDirty;
        m_gsTeamSelector.IsEnabled(enabled && clean && !m_gsTeamNew);
        m_gsTeamNameTextBox.IsEnabled(enabled);
        m_gsTeamKoreanNameTextBox.IsEnabled(enabled);
        m_gsTeamPositionTextBox.IsEnabled(enabled);
        m_gsTeamPhoneTextBox.IsEnabled(enabled);
        m_gsTeamBirthdayTextBox.IsEnabled(enabled);
        m_gsTeamNewButton.IsEnabled(enabled && clean && !m_gsTeamNew);
        m_gsTeamDeleteButton.IsEnabled(
            enabled && clean && !m_gsTeamNew && m_gsTeamSelectedId > 0
            );
        m_gsTeamSaveButton.IsEnabled(enabled && m_gsTeamDirty);
        m_gsTeamDiscardButton.IsEnabled(enabled && m_gsTeamDirty);
    };

    if (!m_openDatabase)
    {
        m_gsTeamLoading = true;
        m_gsTeamMembers.clear();
        m_gsTeamSelectedIndex = -1;
        m_gsTeamSelectedId = -1;
        m_gsTeamNew = false;
        m_gsTeamDirty = false;
        m_gsTeamSelector.Items().Clear();
        m_gsTeamSelector.SelectedIndex(-1);
        m_gsTeamLoading = false;
        presentGsTeamMember(-1);
        m_gsTeamStatusText.Text(L"No database open.");
        m_gsTeamValidationText.Text({});
        m_gsTeamValidationText.Visibility(Visibility::Collapsed);
        setEditable(false);
        return;
    }

    if (refresh && m_gsTeamDirty)
    {
        m_gsTeamStatusText.Text(L"Unsaved GS Team changes are retained.");
        return;
    }

    m_gsTeamLoading = true;
    classmngr::engine::GsTeamService service(*m_openDatabase);
    const auto loaded = service.list();
    if (!loaded)
    {
        m_gsTeamLoading = false;
        m_gsTeamMembers.clear();
        m_gsTeamSelectedIndex = -1;
        m_gsTeamSelectedId = -1;
        m_gsTeamNew = false;
        m_gsTeamDirty = false;
        m_gsTeamSelector.Items().Clear();
        m_gsTeamSelector.SelectedIndex(-1);
        presentGsTeamMember(-1);
        m_gsTeamStatusText.Text(winrt::hstring(
            L"GS Team directory could not be loaded: "
            + asWide(loaded.error().message)
            ));
        m_gsTeamValidationText.Text(
            L"The engine rejected the team-directory read."
            );
        m_gsTeamValidationText.Visibility(Visibility::Visible);
        setEditable(false);
        return;
    }

    const int previousId = m_gsTeamSelectedId;
    m_gsTeamMembers = *loaded;
    m_gsTeamSelector.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0;
         index < static_cast<int>(m_gsTeamMembers.size());
         ++index)
    {
        const auto& member = m_gsTeamMembers[static_cast<std::size_t>(index)];
        std::wstring displayName = member.name.empty()
            ? asWide(member.koreanName)
            : asWide(member.name);
        if (!member.name.empty() && !member.koreanName.empty())
        {
            displayName += L" (";
            displayName += asWide(member.koreanName);
            displayName += L")";
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(displayName)));
        item.Tag(box_value(member.id));
        setAutomationName(item, L"GS Team member " + displayName);
        m_gsTeamSelector.Items().Append(item);
        if (member.id == previousId)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !m_gsTeamMembers.empty())
    {
        selectedIndex = 0;
    }
    m_gsTeamSelectedIndex = selectedIndex;
    m_gsTeamSelectedId = selectedIndex >= 0
        ? m_gsTeamMembers[static_cast<std::size_t>(selectedIndex)].id
        : -1;
    m_gsTeamNew = false;
    m_gsTeamDirty = false;
    m_gsTeamSelector.SelectedIndex(selectedIndex);
    presentGsTeamMember(selectedIndex);
    m_gsTeamLoading = false;
    m_gsTeamStatusText.Text(
        m_gsTeamMembers.empty()
            ? L"No GS Team members found. Choose New Member to add one."
            : L"GS Team directory loaded."
        );
    m_gsTeamValidationText.Text({});
    m_gsTeamValidationText.Visibility(Visibility::Collapsed);
    setEditable(true);
}

void MainWindow::refreshGsTeamPage()
{
    if (!m_contentFrame || m_currentPageId != gsTeamPageId)
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populateGsTeamPage(page, true);
    }
}

void MainWindow::presentGsTeamMember(int index)
{
    m_gsTeamLoading = true;
    if (index < 0 || index >= static_cast<int>(m_gsTeamMembers.size()))
    {
        m_gsTeamSelectedIndex = -1;
        m_gsTeamSelectedId = -1;
        m_gsTeamNameTextBox.Text({});
        m_gsTeamKoreanNameTextBox.Text({});
        m_gsTeamPositionTextBox.Text({});
        m_gsTeamPhoneTextBox.Text({});
        m_gsTeamBirthdayTextBox.Text({});
        m_gsTeamLoading = false;
        return;
    }

    const auto& member = m_gsTeamMembers[static_cast<std::size_t>(index)];
    m_gsTeamSelectedIndex = index;
    m_gsTeamSelectedId = member.id;
    m_gsTeamNameTextBox.Text(asWide(member.name));
    m_gsTeamKoreanNameTextBox.Text(asWide(member.koreanName));
    m_gsTeamPositionTextBox.Text(asWide(member.position));
    m_gsTeamPhoneTextBox.Text(asWide(member.phoneNumber));
    m_gsTeamBirthdayTextBox.Text(asWide(member.birthday));
    m_gsTeamLoading = false;
}

classmngr::engine::GsTeamMember MainWindow::gsTeamMemberFromForm() const
{
    classmngr::engine::GsTeamMember member;
    member.id = m_gsTeamSelectedId;
    member.name = asUtf8(m_gsTeamNameTextBox.Text());
    member.koreanName = asUtf8(m_gsTeamKoreanNameTextBox.Text());
    member.position = asUtf8(m_gsTeamPositionTextBox.Text());
    member.phoneNumber = asUtf8(m_gsTeamPhoneTextBox.Text());
    member.birthday = asUtf8(m_gsTeamBirthdayTextBox.Text());
    return member;
}

void MainWindow::GsTeamSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_gsTeamLoading || !m_openDatabase)
    {
        return;
    }

    if (m_gsTeamDirty)
    {
        m_gsTeamLoading = true;
        m_gsTeamSelector.SelectedIndex(m_gsTeamSelectedIndex);
        m_gsTeamLoading = false;
        m_gsTeamStatusText.Text(
            L"Save or discard the current GS Team member before selecting another."
            );
        return;
    }

    const auto selected = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (!selected)
    {
        return;
    }
    const int selectedIndex = selected.SelectedIndex();
    const auto item = selected.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int selectedId = item ? boxedInt(item.Tag()) : -1;
    if (selectedIndex < 0 || selectedId <= 0)
    {
        presentGsTeamMember(-1);
        m_gsTeamSelectedIndex = -1;
        m_gsTeamSelectedId = -1;
        return;
    }

    int resolvedIndex = -1;
    for (int index = 0;
         index < static_cast<int>(m_gsTeamMembers.size());
         ++index)
    {
        if (m_gsTeamMembers[static_cast<std::size_t>(index)].id == selectedId)
        {
            resolvedIndex = index;
            break;
        }
    }
    m_gsTeamSelectedIndex = resolvedIndex;
    m_gsTeamSelectedId = selectedId;
    m_gsTeamNew = false;
    presentGsTeamMember(resolvedIndex);
    m_gsTeamStatusText.Text(
        resolvedIndex >= 0
            ? L"GS Team member selected."
            : L"Select a GS Team member or choose New Member."
        );
    m_gsTeamValidationText.Text({});
    m_gsTeamValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::GsTeamField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_gsTeamLoading || !m_openDatabase)
    {
        return;
    }

    m_gsTeamDirty = true;
    m_dirtyState.markDirty();
    m_gsTeamStatusText.Text(L"Unsaved GS Team changes.");
    m_gsTeamNewButton.IsEnabled(false);
    m_gsTeamDeleteButton.IsEnabled(false);
    m_gsTeamSelector.IsEnabled(false);
    m_gsTeamSaveButton.IsEnabled(true);
    m_gsTeamDiscardButton.IsEnabled(true);
}

void MainWindow::GsTeamNewButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_gsTeamDirty)
    {
        if (m_gsTeamStatusText && m_gsTeamDirty)
        {
            m_gsTeamStatusText.Text(
                L"Save or discard the current GS Team member before creating another."
                );
        }
        return;
    }

    m_gsTeamNew = true;
    m_gsTeamSelectedIndex = -1;
    m_gsTeamSelectedId = -1;
    m_gsTeamLoading = true;
    m_gsTeamSelector.SelectedIndex(-1);
    m_gsTeamLoading = false;
    presentGsTeamMember(-1);
    m_gsTeamDirty = true;
    m_dirtyState.markDirty();
    m_gsTeamStatusText.Text(
        L"New GS Team member. Enter at least one name and save."
        );
    m_gsTeamValidationText.Text({});
    m_gsTeamValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_gsTeamSelector.IsEnabled(false);
    m_gsTeamNewButton.IsEnabled(false);
    m_gsTeamDeleteButton.IsEnabled(false);
    m_gsTeamSaveButton.IsEnabled(true);
    m_gsTeamDiscardButton.IsEnabled(true);
}

void MainWindow::GsTeamDeleteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_gsTeamSelectedId <= 0 || m_gsTeamDirty)
    {
        return;
    }

    const int memberId = m_gsTeamSelectedId;
    auto weak = get_weak();
    showDialog(
        L"Delete GS Team member",
        L"Delete the selected GS Team member from the active database?",
        L"Delete",
        {},
        L"Cancel",
        [weak, memberId](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (outcome != ClassMngrWinUIDialogs::DialogOutcome::Primary)
            {
                return;
            }
            if (auto self = weak.get())
            {
                if (!self->m_openDatabase)
                {
                    return;
                }
                classmngr::engine::GsTeamService service(
                    *self->m_openDatabase
                    );
                const auto removed = service.saveDirectory({}, {memberId});
                if (!removed)
                {
                    self->m_gsTeamStatusText.Text(winrt::hstring(
                        L"GS Team member could not be deleted: "
                        + asWide(removed.error().message)
                        ));
                    return;
                }
                self->m_gsTeamSelectedId = -1;
                self->m_gsTeamSelectedIndex = -1;
                self->m_gsTeamDirty = false;
                self->m_gsTeamNew = false;
                self->m_dirtyState.markClean();
                self->refreshGsTeamPage();
                self->m_gsTeamStatusText.Text(L"GS Team member deleted.");
            }
        }
        );
}

void MainWindow::GsTeamSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_gsTeamStatusText.Text(L"No database open.");
        return;
    }

    const auto member = gsTeamMemberFromForm();
    const std::wstring birthday = asWString(m_gsTeamBirthdayTextBox.Text());
    if (!validMonthDay(birthday))
    {
        m_gsTeamStatusText.Text(L"GS Team member could not be saved.");
        m_gsTeamValidationText.Text(
            L"Each GS Team member needs a valid MM-dd birthday."
            );
        m_gsTeamValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_gsTeamDirty = true;
        m_dirtyState.markDirty();
        return;
    }

    std::vector<classmngr::engine::GsTeamMember> draft = m_gsTeamMembers;
    bool replaced = false;
    if (member.id > 0)
    {
        for (auto& existing : draft)
        {
            if (existing.id == member.id)
            {
                existing = member;
                replaced = true;
                break;
            }
        }
    }
    if (!replaced)
    {
        draft.push_back(member);
    }

    classmngr::engine::GsTeamService service(*m_openDatabase);
    const auto saved = service.saveDirectory(draft, {});
    if (!saved)
    {
        m_gsTeamStatusText.Text(winrt::hstring(
            L"GS Team member could not be saved: "
            + asWide(saved.error().message)
            ));
        m_gsTeamValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_gsTeamValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    m_gsTeamSelectedId = member.id;
    m_gsTeamSelectedIndex = -1;
    m_gsTeamNew = false;
    m_gsTeamDirty = false;
    m_dirtyState.markClean();
    refreshGsTeamPage();
    m_gsTeamStatusText.Text(L"GS Team member saved.");
    m_gsTeamValidationText.Text({});
    m_gsTeamValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::GsTeamDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_gsTeamDirty = false;
    m_gsTeamNew = false;
    m_dirtyState.markClean();
    refreshGsTeamPage();
}

} // namespace winrt::ClassMngrWinUI::implementation
