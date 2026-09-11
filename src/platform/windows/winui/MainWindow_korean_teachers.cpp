#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populateKoreanTeachersPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_koreanTeacherKrTextBox)
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
        title.Text(L"Korean Teachers");
        title.FontSize(24.0);
        setAutomationName(title, L"Korean Teachers");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"View and maintain Korean teacher details used by classes and reports."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(description, L"Korean teacher directory description");
        root.Children().Append(description);

        m_koreanTeacherStatusText = TextBlock();
        m_koreanTeacherStatusText.Text(L"Loading Korean teachers...");
        m_koreanTeacherStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(m_koreanTeacherStatusText, L"Korean teacher directory status");
        root.Children().Append(m_koreanTeacherStatusText);

        m_koreanTeacherValidationText = TextBlock();
        m_koreanTeacherValidationText.TextWrapping(TextWrapping::Wrap);
        m_koreanTeacherValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(
            m_koreanTeacherValidationText,
            L"Korean teacher validation summary"
            );
        root.Children().Append(m_koreanTeacherValidationText);

        auto directoryCard = ClassMngrWinUISharedUX::buildCard({
            L"Teacher directory",
            L"Select an existing teacher or create a new directory entry.",
            L"Korean teacher directory selector"
            });
        m_koreanTeacherSelector = ComboBox();
        m_koreanTeacherSelector.Header(box_value(hstring(L"Teacher")));
        m_koreanTeacherSelector.PlaceholderText(L"Select a teacher");
        m_koreanTeacherSelector.MinWidth(420.0);
        m_koreanTeacherSelector.HorizontalAlignment(HorizontalAlignment::Stretch);
        m_koreanTeacherSelector.IsTabStop(true);
        m_koreanTeacherSelector.TabIndex(0);
        m_koreanTeacherSelector.SelectionChanged(
            {this, &MainWindow::KoreanTeacherSelection_SelectionChanged}
            );
        setAutomationName(m_koreanTeacherSelector, L"Korean teacher selector");
        directoryCard.content.Children().Append(m_koreanTeacherSelector);
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
            box.TextChanging({this, &MainWindow::KoreanTeacherField_TextChanging});
            setAutomationName(box, automationName);
            return box;
        };

        auto detailsCard = ClassMngrWinUISharedUX::buildCard({
            L"Teacher details",
            L"Names, contact information, birthday, and preferred display name.",
            L"Korean teacher details form"
            });
        m_koreanTeacherKrTextBox = makeTextBox(
            L"Korean Name",
            L"Korean teacher Korean name",
            L"Enter the Korean name"
            );
        auto koreanInputScope = Input::InputScope();
        koreanInputScope.Names().Append(
            Input::InputScopeName(Input::InputScopeNameValue::Text)
            );
        m_koreanTeacherKrTextBox.InputScope(koreanInputScope);
        m_koreanTeacherKrTextBox.TabIndex(1);
        m_koreanTeacherEnTextBox = makeTextBox(
            L"English Name",
            L"Korean teacher English name",
            L"Enter the English name"
            );
        m_koreanTeacherEnTextBox.TabIndex(2);
        m_koreanTeacherRomanizationTextBox = makeTextBox(
            L"Preferred Spelling",
            L"Korean teacher preferred spelling",
            L"Optional preferred romanization"
            );
        m_koreanTeacherRomanizationTextBox.TabIndex(3);
        m_koreanTeacherPreferredNameCombo = ComboBox();
        m_koreanTeacherPreferredNameCombo.Header(
            box_value(hstring(L"Preferred Name"))
            );
        m_koreanTeacherPreferredNameCombo.MinWidth(320.0);
        m_koreanTeacherPreferredNameCombo.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherPreferredNameCombo.IsTabStop(true);
        m_koreanTeacherPreferredNameCombo.TabIndex(4);
        m_koreanTeacherPreferredNameCombo.SelectionChanged(
            {this, &MainWindow::KoreanTeacherCombo_SelectionChanged}
            );
        setAutomationName(
            m_koreanTeacherPreferredNameCombo,
            L"Korean teacher preferred name"
            );
        m_koreanTeacherRoomTextBox = makeTextBox(
            L"Room Number",
            L"Korean teacher room number",
            L"Optional room number"
            );
        m_koreanTeacherRoomTextBox.TabIndex(5);
        m_koreanTeacherBirthdayTextBox = makeTextBox(
            L"Birthday (MM-dd)",
            L"Korean teacher birthday",
            L"e.g. 03-14"
            );
        m_koreanTeacherBirthdayTextBox.TabIndex(6);
        m_koreanTeacherPhoneTextBox = makeTextBox(
            L"Phone Number",
            L"Korean teacher phone number",
            L"Optional phone number"
            );
        m_koreanTeacherPhoneTextBox.TabIndex(7);
        detailsCard.content.Children().Append(m_koreanTeacherKrTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherEnTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherRomanizationTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherPreferredNameCombo);
        detailsCard.content.Children().Append(m_koreanTeacherRoomTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherBirthdayTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherPhoneTextBox);
        root.Children().Append(detailsCard.root);

        auto connectivityCard = ClassMngrWinUISharedUX::buildCard({
            L"Connectivity and presentation",
            L"Keep the same WiFi, Zoom, and projection choices as the Qt teacher form.",
            L"Korean teacher connectivity form"
            });
        m_koreanTeacherWifiNameTextBox = makeTextBox(
            L"WiFi Name",
            L"Korean teacher WiFi name",
            L"Optional WiFi name"
            );
        m_koreanTeacherWifiNameTextBox.TabIndex(8);
        m_koreanTeacherWifiPasswordBox = PasswordBox();
        m_koreanTeacherWifiPasswordBox.Header(
            box_value(hstring(L"WiFi Password"))
            );
        m_koreanTeacherWifiPasswordBox.MinWidth(320.0);
        m_koreanTeacherWifiPasswordBox.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherWifiPasswordBox.IsTabStop(true);
        m_koreanTeacherWifiPasswordBox.TabIndex(9);
        m_koreanTeacherWifiPasswordBox.PasswordChanged(
            {this, &MainWindow::KoreanTeacherPassword_Changed}
            );
        setAutomationName(
            m_koreanTeacherWifiPasswordBox,
            L"Korean teacher WiFi password"
            );
        m_koreanTeacherInternetTypeCombo = ComboBox();
        m_koreanTeacherInternetTypeCombo.Header(
            box_value(hstring(L"Internet Type"))
            );
        m_koreanTeacherInternetTypeCombo.MinWidth(320.0);
        m_koreanTeacherInternetTypeCombo.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherInternetTypeCombo.IsTabStop(true);
        m_koreanTeacherInternetTypeCombo.TabIndex(10);
        for (wchar_t const* value : {L"WiFi", L"LAN", L"Both", L"N/A"})
        {
            m_koreanTeacherInternetTypeCombo.Items().Append(
                box_value(hstring(value))
                );
        }
        m_koreanTeacherInternetTypeCombo.SelectionChanged(
            {this, &MainWindow::KoreanTeacherCombo_SelectionChanged}
            );
        setAutomationName(
            m_koreanTeacherInternetTypeCombo,
            L"Korean teacher internet type"
            );
        m_koreanTeacherZoomIdTextBox = makeTextBox(
            L"Zoom ID",
            L"Korean teacher Zoom ID",
            L"Optional Zoom ID"
            );
        m_koreanTeacherZoomIdTextBox.TabIndex(11);
        m_koreanTeacherZoomPasswordBox = PasswordBox();
        m_koreanTeacherZoomPasswordBox.Header(
            box_value(hstring(L"Zoom Password"))
            );
        m_koreanTeacherZoomPasswordBox.MinWidth(320.0);
        m_koreanTeacherZoomPasswordBox.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherZoomPasswordBox.IsTabStop(true);
        m_koreanTeacherZoomPasswordBox.TabIndex(12);
        m_koreanTeacherZoomPasswordBox.PasswordChanged(
            {this, &MainWindow::KoreanTeacherPassword_Changed}
            );
        setAutomationName(
            m_koreanTeacherZoomPasswordBox,
            L"Korean teacher Zoom password"
            );
        m_koreanTeacherProjectionTypeCombo = ComboBox();
        m_koreanTeacherProjectionTypeCombo.Header(
            box_value(hstring(L"Projection Type"))
            );
        m_koreanTeacherProjectionTypeCombo.MinWidth(320.0);
        m_koreanTeacherProjectionTypeCombo.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherProjectionTypeCombo.IsTabStop(true);
        m_koreanTeacherProjectionTypeCombo.TabIndex(13);
        for (wchar_t const* value : {L"HDMI", L"Zoom", L"Any", L"N/A"})
        {
            m_koreanTeacherProjectionTypeCombo.Items().Append(
                box_value(hstring(value))
                );
        }
        m_koreanTeacherProjectionTypeCombo.SelectionChanged(
            {this, &MainWindow::KoreanTeacherCombo_SelectionChanged}
            );
        setAutomationName(
            m_koreanTeacherProjectionTypeCombo,
            L"Korean teacher projection type"
            );
        connectivityCard.content.Children().Append(m_koreanTeacherWifiNameTextBox);
        connectivityCard.content.Children().Append(m_koreanTeacherWifiPasswordBox);
        connectivityCard.content.Children().Append(m_koreanTeacherInternetTypeCombo);
        connectivityCard.content.Children().Append(m_koreanTeacherZoomIdTextBox);
        connectivityCard.content.Children().Append(m_koreanTeacherZoomPasswordBox);
        connectivityCard.content.Children().Append(m_koreanTeacherProjectionTypeCombo);
        root.Children().Append(connectivityCard.root);

        auto notesCard = ClassMngrWinUISharedUX::buildCard({
            L"Notes",
            L"Free-form teacher notes are validated and persisted by the engine.",
            L"Korean teacher notes form"
            });
        m_koreanTeacherNotesTextBox = makeTextBox(
            L"Notes",
            L"Korean teacher notes",
            L"Optional notes"
            );
        m_koreanTeacherNotesTextBox.AcceptsReturn(true);
        m_koreanTeacherNotesTextBox.TextWrapping(TextWrapping::Wrap);
        m_koreanTeacherNotesTextBox.Height(150.0);
        m_koreanTeacherNotesTextBox.TabIndex(14);
        notesCard.content.Children().Append(m_koreanTeacherNotesTextBox);
        root.Children().Append(notesCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_koreanTeacherNewButton = Button();
        m_koreanTeacherNewButton.Content(box_value(hstring(L"New Teacher")));
        m_koreanTeacherNewButton.IsTabStop(true);
        m_koreanTeacherNewButton.TabIndex(15);
        m_koreanTeacherNewButton.Click(
            {this, &MainWindow::KoreanTeacherNewButton_Click}
            );
        setAutomationName(m_koreanTeacherNewButton, L"New Korean teacher");
        m_koreanTeacherDeleteButton = Button();
        m_koreanTeacherDeleteButton.Content(box_value(hstring(L"Delete Teacher")));
        m_koreanTeacherDeleteButton.IsTabStop(true);
        m_koreanTeacherDeleteButton.TabIndex(16);
        m_koreanTeacherDeleteButton.Click(
            {this, &MainWindow::KoreanTeacherDeleteButton_Click}
            );
        setAutomationName(m_koreanTeacherDeleteButton, L"Delete Korean teacher");
        m_koreanTeacherSaveButton = Button();
        m_koreanTeacherSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_koreanTeacherSaveButton.IsTabStop(true);
        m_koreanTeacherSaveButton.TabIndex(17);
        m_koreanTeacherSaveButton.Click(
            {this, &MainWindow::KoreanTeacherSaveButton_Click}
            );
        setAutomationName(m_koreanTeacherSaveButton, L"Save Korean teacher");
        m_koreanTeacherDiscardButton = Button();
        m_koreanTeacherDiscardButton.Content(box_value(hstring(L"Discard Changes")));
        m_koreanTeacherDiscardButton.IsTabStop(true);
        m_koreanTeacherDiscardButton.TabIndex(18);
        m_koreanTeacherDiscardButton.Click(
            {this, &MainWindow::KoreanTeacherDiscardButton_Click}
            );
        setAutomationName(
            m_koreanTeacherDiscardButton,
            L"Discard Korean teacher changes"
            );
        actions.Children().Append(m_koreanTeacherNewButton);
        actions.Children().Append(m_koreanTeacherDeleteButton);
        actions.Children().Append(m_koreanTeacherSaveButton);
        actions.Children().Append(m_koreanTeacherDiscardButton);
        root.Children().Append(actions);

        scroll.Content(root);
        page.Content(scroll);
    }

    const auto setEditable = [this](bool enabled) {
        const bool clean = !m_koreanTeacherDirty;
        m_koreanTeacherSelector.IsEnabled(enabled && clean && !m_koreanTeacherNew);
        m_koreanTeacherKrTextBox.IsEnabled(enabled);
        m_koreanTeacherEnTextBox.IsEnabled(enabled);
        m_koreanTeacherRomanizationTextBox.IsEnabled(enabled);
        m_koreanTeacherPreferredNameCombo.IsEnabled(
            enabled && m_koreanTeacherPreferredNameCombo.Items().Size() > 0
            );
        m_koreanTeacherRoomTextBox.IsEnabled(enabled);
        m_koreanTeacherBirthdayTextBox.IsEnabled(enabled);
        m_koreanTeacherPhoneTextBox.IsEnabled(enabled);
        m_koreanTeacherWifiNameTextBox.IsEnabled(enabled);
        m_koreanTeacherWifiPasswordBox.IsEnabled(enabled);
        m_koreanTeacherInternetTypeCombo.IsEnabled(enabled);
        m_koreanTeacherZoomIdTextBox.IsEnabled(enabled);
        m_koreanTeacherZoomPasswordBox.IsEnabled(enabled);
        m_koreanTeacherProjectionTypeCombo.IsEnabled(enabled);
        m_koreanTeacherNotesTextBox.IsEnabled(enabled);
        m_koreanTeacherNewButton.IsEnabled(enabled && clean && !m_koreanTeacherNew);
        m_koreanTeacherDeleteButton.IsEnabled(
            enabled && clean && !m_koreanTeacherNew
                && m_koreanTeacherSelectedId > 0
            );
        m_koreanTeacherSaveButton.IsEnabled(enabled && m_koreanTeacherDirty);
        m_koreanTeacherDiscardButton.IsEnabled(enabled && m_koreanTeacherDirty);
    };

    if (!m_openDatabase)
    {
        m_koreanTeacherLoading = true;
        m_koreanTeachers.clear();
        m_koreanTeacherSelectedIndex = -1;
        m_koreanTeacherSelectedId = -1;
        m_koreanTeacherNew = false;
        m_koreanTeacherDirty = false;
        m_koreanTeacherSelector.Items().Clear();
        m_koreanTeacherSelector.SelectedIndex(-1);
        m_koreanTeacherLoading = false;
        presentKoreanTeacher(-1);
        m_koreanTeacherStatusText.Text(L"No database open.");
        m_koreanTeacherValidationText.Text({});
        m_koreanTeacherValidationText.Visibility(Visibility::Collapsed);
        setEditable(false);
        return;
    }

    if (refresh && m_koreanTeacherDirty)
    {
        m_koreanTeacherStatusText.Text(
            L"Unsaved Korean teacher changes are retained."
            );
        return;
    }

    m_koreanTeacherLoading = true;
    classmngr::engine::TeacherService service(*m_openDatabase);
    const auto loaded = service.list();
    if (!loaded)
    {
        m_koreanTeacherLoading = false;
        m_koreanTeachers.clear();
        m_koreanTeacherSelectedIndex = -1;
        m_koreanTeacherSelectedId = -1;
        m_koreanTeacherNew = false;
        m_koreanTeacherDirty = false;
        m_koreanTeacherSelector.Items().Clear();
        m_koreanTeacherSelector.SelectedIndex(-1);
        presentKoreanTeacher(-1);
        m_koreanTeacherStatusText.Text(winrt::hstring(
            L"Korean teacher directory could not be loaded: "
            + asWide(loaded.error().message)
            ));
        m_koreanTeacherValidationText.Text(
            L"The engine rejected the teacher-directory read."
            );
        m_koreanTeacherValidationText.Visibility(Visibility::Visible);
        setEditable(false);
        return;
    }

    const int previousId = m_koreanTeacherSelectedId;
    m_koreanTeachers = *loaded;
    m_koreanTeacherSelector.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_koreanTeachers.size()); ++index)
    {
        const auto& teacher = m_koreanTeachers[static_cast<std::size_t>(index)];
        std::wstring displayName = asWide(teacher.preferredDisplayName());
        if (!teacher.teacherKr.empty())
        {
            displayName += L" (" + asWide(teacher.teacherKr) + L")";
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(displayName)));
        item.Tag(box_value(teacher.id));
        setAutomationName(item, L"Korean teacher " + displayName);
        m_koreanTeacherSelector.Items().Append(item);
        if (teacher.id == previousId)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !m_koreanTeachers.empty())
    {
        selectedIndex = 0;
    }
    m_koreanTeacherSelectedIndex = selectedIndex;
    m_koreanTeacherSelectedId = selectedIndex >= 0
        ? m_koreanTeachers[static_cast<std::size_t>(selectedIndex)].id
        : -1;
    m_koreanTeacherNew = false;
    m_koreanTeacherDirty = false;
    m_koreanTeacherSelector.SelectedIndex(selectedIndex);
    presentKoreanTeacher(selectedIndex);
    m_koreanTeacherLoading = false;
    m_koreanTeacherStatusText.Text(
        m_koreanTeachers.empty()
            ? L"No Korean teachers found. Choose New Teacher to add one."
            : L"Korean teacher directory loaded."
        );
    m_koreanTeacherValidationText.Text({});
    m_koreanTeacherValidationText.Visibility(Visibility::Collapsed);
    setEditable(true);
}

void MainWindow::refreshKoreanTeachersPage()
{
    if (!m_contentFrame || m_currentPageId != koreanTeachersPageId)
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populateKoreanTeachersPage(page, true);
    }
}

void MainWindow::presentKoreanTeacher(int index)
{
    m_koreanTeacherLoading = true;
    if (index < 0 || index >= static_cast<int>(m_koreanTeachers.size()))
    {
        m_koreanTeacherSelectedIndex = -1;
        m_koreanTeacherSelectedId = -1;
        m_koreanTeacherKrTextBox.Text({});
        m_koreanTeacherEnTextBox.Text({});
        m_koreanTeacherRomanizationTextBox.Text({});
        m_koreanTeacherPreferredNameCombo.Items().Clear();
        m_koreanTeacherPreferredNameCombo.SelectedIndex(-1);
        m_koreanTeacherRoomTextBox.Text({});
        m_koreanTeacherBirthdayTextBox.Text({});
        m_koreanTeacherPhoneTextBox.Text({});
        m_koreanTeacherWifiNameTextBox.Text({});
        m_koreanTeacherWifiPasswordBox.Password({});
        m_koreanTeacherInternetTypeCombo.SelectedIndex(0);
        m_koreanTeacherZoomIdTextBox.Text({});
        m_koreanTeacherZoomPasswordBox.Password({});
        m_koreanTeacherProjectionTypeCombo.SelectedIndex(0);
        m_koreanTeacherNotesTextBox.Text({});
        m_koreanTeacherLoading = false;
        return;
    }

    const auto& teacher = m_koreanTeachers[static_cast<std::size_t>(index)];
    m_koreanTeacherSelectedIndex = index;
    m_koreanTeacherSelectedId = teacher.id;
    m_koreanTeacherKrTextBox.Text(asWide(teacher.teacherKr));
    m_koreanTeacherEnTextBox.Text(asWide(teacher.teacherEn));
    m_koreanTeacherRomanizationTextBox.Text(asWide(teacher.preferredRomanization));
    m_koreanTeacherPreferredNameCombo.Items().Clear();
    int preferredIndex = -1;
    const auto preferredChoices = teacher.preferredNameChoices();
    for (int choiceIndex = 0;
         choiceIndex < static_cast<int>(preferredChoices.size());
         ++choiceIndex)
    {
        const std::wstring choice = asWide(
            preferredChoices[static_cast<std::size_t>(choiceIndex)]
            );
        m_koreanTeacherPreferredNameCombo.Items().Append(
            box_value(hstring(choice))
            );
        if (preferredChoices[static_cast<std::size_t>(choiceIndex)]
            == teacher.preferredName)
        {
            preferredIndex = choiceIndex;
        }
    }
    m_koreanTeacherPreferredNameCombo.SelectedIndex(preferredIndex);
    m_koreanTeacherRoomTextBox.Text(asWide(teacher.roomNumber));
    m_koreanTeacherBirthdayTextBox.Text(asWide(teacher.birthday));
    m_koreanTeacherPhoneTextBox.Text(asWide(teacher.phoneNumber));
    m_koreanTeacherWifiNameTextBox.Text(asWide(teacher.wifiName));
    m_koreanTeacherWifiPasswordBox.Password(asWide(teacher.wifiPassword));

    const auto selectComboValue = [](
        Microsoft::UI::Xaml::Controls::ComboBox combo,
        std::string const& value,
        wchar_t const* fallback
        ) {
        const std::wstring selected = asWide(value);
        for (int optionIndex = 0;
             optionIndex < static_cast<int>(combo.Items().Size());
             ++optionIndex)
        {
            if (boxedString(combo.Items().GetAt(optionIndex)) == selected)
            {
                combo.SelectedIndex(optionIndex);
                return;
            }
        }
        for (int optionIndex = 0;
             optionIndex < static_cast<int>(combo.Items().Size());
             ++optionIndex)
        {
            if (boxedString(combo.Items().GetAt(optionIndex)) == fallback)
            {
                combo.SelectedIndex(optionIndex);
                return;
            }
        }
        combo.SelectedIndex(-1);
    };
    selectComboValue(
        m_koreanTeacherInternetTypeCombo,
        teacher.internetType,
        L"WiFi"
        );
    m_koreanTeacherZoomIdTextBox.Text(asWide(teacher.zoomId));
    m_koreanTeacherZoomPasswordBox.Password(asWide(teacher.zoomPassword));
    selectComboValue(
        m_koreanTeacherProjectionTypeCombo,
        teacher.projectionType,
        L"HDMI"
        );
    m_koreanTeacherNotesTextBox.Text(asWide(teacher.notes));
    m_koreanTeacherLoading = false;
}

void MainWindow::refreshKoreanTeacherPreferredNames()
{
    if (!m_koreanTeacherPreferredNameCombo)
    {
        return;
    }

    classmngr::engine::Teacher teacher;
    teacher.teacherEn = asUtf8(m_koreanTeacherEnTextBox.Text());
    teacher.preferredRomanization = asUtf8(
        m_koreanTeacherRomanizationTextBox.Text()
        );
    const std::wstring current = boxedString(
        m_koreanTeacherPreferredNameCombo.SelectedItem()
        );
    const auto choices = teacher.preferredNameChoices();
    m_koreanTeacherLoading = true;
    m_koreanTeacherPreferredNameCombo.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(choices.size()); ++index)
    {
        const std::wstring choice = asWide(choices[static_cast<std::size_t>(index)]);
        m_koreanTeacherPreferredNameCombo.Items().Append(
            box_value(hstring(choice))
            );
        if (choice == current)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !choices.empty())
    {
        selectedIndex = 0;
    }
    m_koreanTeacherPreferredNameCombo.SelectedIndex(selectedIndex);
    m_koreanTeacherLoading = false;
}

classmngr::engine::Teacher MainWindow::koreanTeacherFromForm() const
{
    classmngr::engine::Teacher teacher;
    teacher.id = m_koreanTeacherSelectedId;
    teacher.teacherKr = asUtf8(m_koreanTeacherKrTextBox.Text());
    teacher.teacherEn = asUtf8(m_koreanTeacherEnTextBox.Text());
    teacher.preferredRomanization = asUtf8(
        m_koreanTeacherRomanizationTextBox.Text()
        );
    teacher.preferredName = asUtf8(
        boxedString(m_koreanTeacherPreferredNameCombo.SelectedItem())
        );
    teacher.roomNumber = asUtf8(m_koreanTeacherRoomTextBox.Text());
    teacher.birthday = asUtf8(m_koreanTeacherBirthdayTextBox.Text());
    teacher.phoneNumber = asUtf8(m_koreanTeacherPhoneTextBox.Text());
    teacher.wifiName = asUtf8(m_koreanTeacherWifiNameTextBox.Text());
    teacher.wifiPassword = asUtf8(m_koreanTeacherWifiPasswordBox.Password());
    teacher.internetType = asUtf8(
        boxedString(m_koreanTeacherInternetTypeCombo.SelectedItem())
        );
    teacher.zoomId = asUtf8(m_koreanTeacherZoomIdTextBox.Text());
    teacher.zoomPassword = asUtf8(m_koreanTeacherZoomPasswordBox.Password());
    teacher.projectionType = asUtf8(
        boxedString(m_koreanTeacherProjectionTypeCombo.SelectedItem())
        );
    teacher.notes = asUtf8(m_koreanTeacherNotesTextBox.Text());
    return teacher;
}

} // namespace winrt::ClassMngrWinUI::implementation
