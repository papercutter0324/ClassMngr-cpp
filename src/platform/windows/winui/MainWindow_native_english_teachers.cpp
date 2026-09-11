#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populateNativeEnglishTeachersPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_nativeEnglishTeacherNameTextBox)
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
        title.Text(L"Native English Teachers");
        title.FontSize(24.0);
        setAutomationName(title, L"Native English Teachers");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"View and maintain Native English Teacher contact information used by classes and reports."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            description,
            L"Native English teacher directory description"
            );
        root.Children().Append(description);

        m_nativeEnglishTeacherStatusText = TextBlock();
        m_nativeEnglishTeacherStatusText.Text(
            L"Loading Native English Teachers..."
            );
        m_nativeEnglishTeacherStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_nativeEnglishTeacherStatusText,
            L"Native English teacher directory status"
            );
        root.Children().Append(m_nativeEnglishTeacherStatusText);

        m_nativeEnglishTeacherValidationText = TextBlock();
        m_nativeEnglishTeacherValidationText.TextWrapping(TextWrapping::Wrap);
        m_nativeEnglishTeacherValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(
            m_nativeEnglishTeacherValidationText,
            L"Native English teacher validation summary"
            );
        root.Children().Append(m_nativeEnglishTeacherValidationText);

        auto directoryCard = ClassMngrWinUISharedUX::buildCard({
            L"Teacher directory",
            L"Select an existing teacher or create a new directory entry.",
            L"Native English teacher directory selector"
            });
        m_nativeEnglishTeacherSelector = ComboBox();
        m_nativeEnglishTeacherSelector.Header(box_value(hstring(L"Teacher")));
        m_nativeEnglishTeacherSelector.PlaceholderText(
            L"Select a teacher"
            );
        m_nativeEnglishTeacherSelector.MinWidth(420.0);
        m_nativeEnglishTeacherSelector.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_nativeEnglishTeacherSelector.IsTabStop(true);
        m_nativeEnglishTeacherSelector.TabIndex(0);
        m_nativeEnglishTeacherSelector.SelectionChanged({
            this,
            &MainWindow::NativeEnglishTeacherSelection_SelectionChanged
            });
        setAutomationName(
            m_nativeEnglishTeacherSelector,
            L"Native English teacher selector"
            );
        directoryCard.content.Children().Append(
            m_nativeEnglishTeacherSelector
            );
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
            box.TextChanging({
                this,
                &MainWindow::NativeEnglishTeacherField_TextChanging
                });
            setAutomationName(box, automationName);
            return box;
        };

        auto detailsCard = ClassMngrWinUISharedUX::buildCard({
            L"Teacher details",
            L"Name, position, contact information, birthday, and nationality.",
            L"Native English teacher details form"
            });
        m_nativeEnglishTeacherNameTextBox = makeTextBox(
            L"Name",
            L"Native English teacher name",
            L"Enter the teacher name"
            );
        auto nameInputScope = Input::InputScope();
        nameInputScope.Names().Append(
            Input::InputScopeName(Input::InputScopeNameValue::Text)
            );
        m_nativeEnglishTeacherNameTextBox.InputScope(nameInputScope);
        m_nativeEnglishTeacherNameTextBox.TabIndex(1);

        m_nativeEnglishTeacherPositionCombo = ComboBox();
        m_nativeEnglishTeacherPositionCombo.Header(
            box_value(hstring(L"Position"))
            );
        m_nativeEnglishTeacherPositionCombo.MinWidth(320.0);
        m_nativeEnglishTeacherPositionCombo.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_nativeEnglishTeacherPositionCombo.IsTabStop(true);
        m_nativeEnglishTeacherPositionCombo.TabIndex(2);
        const auto addPosition = [this](
            wchar_t const* display,
            wchar_t const* stored
            ) {
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(display)));
            item.Tag(box_value(hstring(stored)));
            m_nativeEnglishTeacherPositionCombo.Items().Append(item);
        };
        addPosition(L"Coordinator", L"Co-ordinator");
        addPosition(L"Team Leader", L"Team Leader");
        addPosition(L"M3 Song's", L"M3 Song's");
        addPosition(L"M2 Song's", L"M2 Song's");
        addPosition(L"M1 Song's", L"M1 Song's");
        addPosition(L"E6 Song's", L"E6 Song's");
        addPosition(L"E5 Athena", L"E5 Athena");
        addPosition(L"NET", L"NET");
        m_nativeEnglishTeacherPositionCombo.SelectionChanged({
            this,
            &MainWindow::NativeEnglishTeacherPosition_SelectionChanged
            });
        setAutomationName(
            m_nativeEnglishTeacherPositionCombo,
            L"Native English teacher position"
            );

        m_nativeEnglishTeacherPhoneTextBox = makeTextBox(
            L"Phone Number",
            L"Native English teacher phone number",
            L"Optional phone number"
            );
        m_nativeEnglishTeacherPhoneTextBox.TabIndex(3);
        m_nativeEnglishTeacherEmailTextBox = makeTextBox(
            L"Email",
            L"Native English teacher email",
            L"Optional email address"
            );
        m_nativeEnglishTeacherEmailTextBox.TabIndex(4);
        m_nativeEnglishTeacherBirthdayTextBox = makeTextBox(
            L"Birthday (MM-dd)",
            L"Native English teacher birthday",
            L"e.g. 03-14"
            );
        m_nativeEnglishTeacherBirthdayTextBox.TabIndex(5);
        m_nativeEnglishTeacherNationalityTextBox = makeTextBox(
            L"Nationality",
            L"Native English teacher nationality",
            L"Optional nationality"
            );
        m_nativeEnglishTeacherNationalityTextBox.TabIndex(6);

        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherNameTextBox
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherPositionCombo
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherPhoneTextBox
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherEmailTextBox
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherBirthdayTextBox
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherNationalityTextBox
            );
        root.Children().Append(detailsCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_nativeEnglishTeacherNewButton = Button();
        m_nativeEnglishTeacherNewButton.Content(
            box_value(hstring(L"New Teacher"))
            );
        m_nativeEnglishTeacherNewButton.IsTabStop(true);
        m_nativeEnglishTeacherNewButton.TabIndex(7);
        m_nativeEnglishTeacherNewButton.Click({
            this,
            &MainWindow::NativeEnglishTeacherNewButton_Click
            });
        setAutomationName(
            m_nativeEnglishTeacherNewButton,
            L"New Native English teacher"
            );
        m_nativeEnglishTeacherDeleteButton = Button();
        m_nativeEnglishTeacherDeleteButton.Content(
            box_value(hstring(L"Delete Teacher"))
            );
        m_nativeEnglishTeacherDeleteButton.IsTabStop(true);
        m_nativeEnglishTeacherDeleteButton.TabIndex(8);
        m_nativeEnglishTeacherDeleteButton.Click({
            this,
            &MainWindow::NativeEnglishTeacherDeleteButton_Click
            });
        setAutomationName(
            m_nativeEnglishTeacherDeleteButton,
            L"Delete Native English teacher"
            );
        m_nativeEnglishTeacherSaveButton = Button();
        m_nativeEnglishTeacherSaveButton.Content(
            box_value(hstring(L"Save Changes"))
            );
        m_nativeEnglishTeacherSaveButton.IsTabStop(true);
        m_nativeEnglishTeacherSaveButton.TabIndex(9);
        m_nativeEnglishTeacherSaveButton.Click({
            this,
            &MainWindow::NativeEnglishTeacherSaveButton_Click
            });
        setAutomationName(
            m_nativeEnglishTeacherSaveButton,
            L"Save Native English teacher"
            );
        m_nativeEnglishTeacherDiscardButton = Button();
        m_nativeEnglishTeacherDiscardButton.Content(
            box_value(hstring(L"Discard Changes"))
            );
        m_nativeEnglishTeacherDiscardButton.IsTabStop(true);
        m_nativeEnglishTeacherDiscardButton.TabIndex(10);
        m_nativeEnglishTeacherDiscardButton.Click({
            this,
            &MainWindow::NativeEnglishTeacherDiscardButton_Click
            });
        setAutomationName(
            m_nativeEnglishTeacherDiscardButton,
            L"Discard Native English teacher changes"
            );
        actions.Children().Append(m_nativeEnglishTeacherNewButton);
        actions.Children().Append(m_nativeEnglishTeacherDeleteButton);
        actions.Children().Append(m_nativeEnglishTeacherSaveButton);
        actions.Children().Append(m_nativeEnglishTeacherDiscardButton);
        root.Children().Append(actions);

        scroll.Content(root);
        page.Content(scroll);
    }

    const auto setEditable = [this](bool enabled) {
        const bool clean = !m_nativeEnglishTeacherDirty;
        m_nativeEnglishTeacherSelector.IsEnabled(
            enabled && clean && !m_nativeEnglishTeacherNew
            );
        m_nativeEnglishTeacherNameTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherPositionCombo.IsEnabled(enabled);
        m_nativeEnglishTeacherPhoneTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherEmailTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherBirthdayTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherNationalityTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherNewButton.IsEnabled(
            enabled && clean && !m_nativeEnglishTeacherNew
            );
        m_nativeEnglishTeacherDeleteButton.IsEnabled(
            enabled && clean && !m_nativeEnglishTeacherNew
                && m_nativeEnglishTeacherSelectedId > 0
            );
        m_nativeEnglishTeacherSaveButton.IsEnabled(
            enabled && m_nativeEnglishTeacherDirty
            );
        m_nativeEnglishTeacherDiscardButton.IsEnabled(
            enabled && m_nativeEnglishTeacherDirty
            );
    };

    if (!m_openDatabase)
    {
        m_nativeEnglishTeacherLoading = true;
        m_nativeEnglishTeachers.clear();
        m_nativeEnglishTeacherSelectedIndex = -1;
        m_nativeEnglishTeacherSelectedId = -1;
        m_nativeEnglishTeacherNew = false;
        m_nativeEnglishTeacherDirty = false;
        m_nativeEnglishTeacherSelector.Items().Clear();
        m_nativeEnglishTeacherSelector.SelectedIndex(-1);
        m_nativeEnglishTeacherLoading = false;
        presentNativeEnglishTeacher(-1);
        m_nativeEnglishTeacherStatusText.Text(L"No database open.");
        m_nativeEnglishTeacherValidationText.Text({});
        m_nativeEnglishTeacherValidationText.Visibility(Visibility::Collapsed);
        setEditable(false);
        return;
    }

    if (refresh && m_nativeEnglishTeacherDirty)
    {
        m_nativeEnglishTeacherStatusText.Text(
            L"Unsaved Native English Teacher changes are retained."
            );
        return;
    }

    m_nativeEnglishTeacherLoading = true;
    classmngr::engine::NativeEnglishTeacherService service(*m_openDatabase);
    const auto loaded = service.list();
    if (!loaded)
    {
        m_nativeEnglishTeacherLoading = false;
        m_nativeEnglishTeachers.clear();
        m_nativeEnglishTeacherSelectedIndex = -1;
        m_nativeEnglishTeacherSelectedId = -1;
        m_nativeEnglishTeacherNew = false;
        m_nativeEnglishTeacherDirty = false;
        m_nativeEnglishTeacherSelector.Items().Clear();
        m_nativeEnglishTeacherSelector.SelectedIndex(-1);
        presentNativeEnglishTeacher(-1);
        m_nativeEnglishTeacherStatusText.Text(winrt::hstring(
            L"Native English Teacher directory could not be loaded: "
            + asWide(loaded.error().message)
            ));
        m_nativeEnglishTeacherValidationText.Text(
            L"The engine rejected the teacher-directory read."
            );
        m_nativeEnglishTeacherValidationText.Visibility(Visibility::Visible);
        setEditable(false);
        return;
    }

    const int previousId = m_nativeEnglishTeacherSelectedId;
    m_nativeEnglishTeachers = *loaded;
    m_nativeEnglishTeacherSelector.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0;
         index < static_cast<int>(m_nativeEnglishTeachers.size());
         ++index)
    {
        const auto& teacher = m_nativeEnglishTeachers[
            static_cast<std::size_t>(index)
            ];
        std::wstring displayName = asWide(teacher.name);
        if (!teacher.position.empty())
        {
            displayName += L" (";
            displayName += asWide(teacher.position);
            displayName += L")";
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(displayName)));
        item.Tag(box_value(teacher.id));
        setAutomationName(item, L"Native English teacher " + displayName);
        m_nativeEnglishTeacherSelector.Items().Append(item);
        if (teacher.id == previousId)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !m_nativeEnglishTeachers.empty())
    {
        selectedIndex = 0;
    }
    m_nativeEnglishTeacherSelectedIndex = selectedIndex;
    m_nativeEnglishTeacherSelectedId = selectedIndex >= 0
        ? m_nativeEnglishTeachers[static_cast<std::size_t>(selectedIndex)].id
        : -1;
    m_nativeEnglishTeacherNew = false;
    m_nativeEnglishTeacherDirty = false;
    m_nativeEnglishTeacherSelector.SelectedIndex(selectedIndex);
    presentNativeEnglishTeacher(selectedIndex);
    m_nativeEnglishTeacherLoading = false;
    m_nativeEnglishTeacherStatusText.Text(
        m_nativeEnglishTeachers.empty()
            ? L"No Native English Teachers found. Choose New Teacher to add one."
            : L"Native English Teacher directory loaded."
        );
    m_nativeEnglishTeacherValidationText.Text({});
    m_nativeEnglishTeacherValidationText.Visibility(Visibility::Collapsed);
    setEditable(true);
}

void MainWindow::refreshNativeEnglishTeachersPage()
{
    if (!m_contentFrame || m_currentPageId != nativeEnglishTeachersPageId)
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populateNativeEnglishTeachersPage(page, true);
    }
}

void MainWindow::presentNativeEnglishTeacher(int index)
{
    m_nativeEnglishTeacherLoading = true;
    if (index < 0
        || index >= static_cast<int>(m_nativeEnglishTeachers.size()))
    {
        m_nativeEnglishTeacherSelectedIndex = -1;
        m_nativeEnglishTeacherSelectedId = -1;
        m_nativeEnglishTeacherNameTextBox.Text({});
        m_nativeEnglishTeacherPositionCombo.SelectedIndex(-1);
        m_nativeEnglishTeacherPhoneTextBox.Text({});
        m_nativeEnglishTeacherEmailTextBox.Text({});
        m_nativeEnglishTeacherBirthdayTextBox.Text({});
        m_nativeEnglishTeacherNationalityTextBox.Text({});
        m_nativeEnglishTeacherLoading = false;
        return;
    }

    const auto& teacher = m_nativeEnglishTeachers[
        static_cast<std::size_t>(index)
        ];
    m_nativeEnglishTeacherSelectedIndex = index;
    m_nativeEnglishTeacherSelectedId = teacher.id;
    m_nativeEnglishTeacherNameTextBox.Text(asWide(teacher.name));
    m_nativeEnglishTeacherPositionCombo.SelectedIndex(-1);
    for (int optionIndex = 0;
         optionIndex < static_cast<int>(
             m_nativeEnglishTeacherPositionCombo.Items().Size());
         ++optionIndex)
    {
        const auto item = m_nativeEnglishTeacherPositionCombo.Items().GetAt(
            optionIndex
            ).try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        if (item && boxedString(item.Tag()) == asWide(teacher.position))
        {
            m_nativeEnglishTeacherPositionCombo.SelectedIndex(optionIndex);
            break;
        }
    }
    m_nativeEnglishTeacherPhoneTextBox.Text(asWide(teacher.phoneNumber));
    m_nativeEnglishTeacherEmailTextBox.Text(asWide(teacher.email));
    m_nativeEnglishTeacherBirthdayTextBox.Text(asWide(teacher.birthday));
    m_nativeEnglishTeacherNationalityTextBox.Text(asWide(teacher.nationality));
    m_nativeEnglishTeacherLoading = false;
}

classmngr::engine::NativeEnglishTeacher
MainWindow::nativeEnglishTeacherFromForm() const
{
    classmngr::engine::NativeEnglishTeacher teacher;
    teacher.id = m_nativeEnglishTeacherSelectedId;
    teacher.name = asUtf8(m_nativeEnglishTeacherNameTextBox.Text());
    const auto selectedPosition =
        m_nativeEnglishTeacherPositionCombo.SelectedItem().try_as<
            Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    teacher.position = selectedPosition
        ? asUtf8(boxedString(selectedPosition.Tag()))
        : asUtf8(boxedString(
            m_nativeEnglishTeacherPositionCombo.SelectedItem()
            ));
    teacher.phoneNumber = asUtf8(m_nativeEnglishTeacherPhoneTextBox.Text());
    teacher.email = asUtf8(m_nativeEnglishTeacherEmailTextBox.Text());
    teacher.birthday = asUtf8(
        m_nativeEnglishTeacherBirthdayTextBox.Text()
        );
    teacher.nationality = asUtf8(
        m_nativeEnglishTeacherNationalityTextBox.Text()
        );
    return teacher;
}

void MainWindow::NativeEnglishTeacherSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_nativeEnglishTeacherLoading || !m_openDatabase)
    {
        return;
    }

    if (m_nativeEnglishTeacherDirty)
    {
        m_nativeEnglishTeacherLoading = true;
        m_nativeEnglishTeacherSelector.SelectedIndex(
            m_nativeEnglishTeacherSelectedIndex
            );
        m_nativeEnglishTeacherLoading = false;
        m_nativeEnglishTeacherStatusText.Text(
            L"Save or discard the current Native English Teacher before selecting another."
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
        presentNativeEnglishTeacher(-1);
        m_nativeEnglishTeacherSelectedIndex = -1;
        m_nativeEnglishTeacherSelectedId = -1;
        return;
    }

    int resolvedIndex = -1;
    for (int index = 0;
         index < static_cast<int>(m_nativeEnglishTeachers.size());
         ++index)
    {
        if (m_nativeEnglishTeachers[static_cast<std::size_t>(index)].id
            == selectedId)
        {
            resolvedIndex = index;
            break;
        }
    }
    m_nativeEnglishTeacherSelectedIndex = resolvedIndex;
    m_nativeEnglishTeacherSelectedId = selectedId;
    m_nativeEnglishTeacherNew = false;
    presentNativeEnglishTeacher(resolvedIndex);
    m_nativeEnglishTeacherStatusText.Text(
        resolvedIndex >= 0
            ? L"Native English Teacher selected."
            : L"Select a Native English Teacher or choose New Teacher."
        );
    m_nativeEnglishTeacherValidationText.Text({});
    m_nativeEnglishTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::NativeEnglishTeacherField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_nativeEnglishTeacherLoading || !m_openDatabase)
    {
        return;
    }

    m_nativeEnglishTeacherDirty = true;
    m_dirtyState.markDirty();
    m_nativeEnglishTeacherStatusText.Text(
        L"Unsaved Native English Teacher changes."
        );
    m_nativeEnglishTeacherNewButton.IsEnabled(false);
    m_nativeEnglishTeacherDeleteButton.IsEnabled(false);
    m_nativeEnglishTeacherSelector.IsEnabled(false);
    m_nativeEnglishTeacherSaveButton.IsEnabled(true);
    m_nativeEnglishTeacherDiscardButton.IsEnabled(true);
}

void MainWindow::NativeEnglishTeacherPosition_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_nativeEnglishTeacherLoading || !m_openDatabase)
    {
        return;
    }

    m_nativeEnglishTeacherDirty = true;
    m_dirtyState.markDirty();
    m_nativeEnglishTeacherStatusText.Text(
        L"Unsaved Native English Teacher changes."
        );
    m_nativeEnglishTeacherNewButton.IsEnabled(false);
    m_nativeEnglishTeacherDeleteButton.IsEnabled(false);
    m_nativeEnglishTeacherSelector.IsEnabled(false);
    m_nativeEnglishTeacherSaveButton.IsEnabled(true);
    m_nativeEnglishTeacherDiscardButton.IsEnabled(true);
}

void MainWindow::NativeEnglishTeacherNewButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_nativeEnglishTeacherDirty)
    {
        if (m_nativeEnglishTeacherStatusText
            && m_nativeEnglishTeacherDirty)
        {
            m_nativeEnglishTeacherStatusText.Text(
                L"Save or discard the current Native English Teacher before creating another."
                );
        }
        return;
    }

    m_nativeEnglishTeacherNew = true;
    m_nativeEnglishTeacherSelectedIndex = -1;
    m_nativeEnglishTeacherSelectedId = -1;
    m_nativeEnglishTeacherLoading = true;
    m_nativeEnglishTeacherSelector.SelectedIndex(-1);
    m_nativeEnglishTeacherLoading = false;
    presentNativeEnglishTeacher(-1);
    m_nativeEnglishTeacherDirty = true;
    m_dirtyState.markDirty();
    m_nativeEnglishTeacherStatusText.Text(
        L"New Native English Teacher. Enter the name and save."
        );
    m_nativeEnglishTeacherValidationText.Text({});
    m_nativeEnglishTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_nativeEnglishTeacherSelector.IsEnabled(false);
    m_nativeEnglishTeacherNewButton.IsEnabled(false);
    m_nativeEnglishTeacherDeleteButton.IsEnabled(false);
    m_nativeEnglishTeacherSaveButton.IsEnabled(true);
    m_nativeEnglishTeacherDiscardButton.IsEnabled(true);
}

void MainWindow::NativeEnglishTeacherDeleteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_nativeEnglishTeacherSelectedId <= 0
        || m_nativeEnglishTeacherDirty)
    {
        return;
    }

    const int teacherId = m_nativeEnglishTeacherSelectedId;
    auto weak = get_weak();
    showDialog(
        L"Delete Native English Teacher",
        L"Delete the selected Native English Teacher from the active database?",
        L"Delete",
        {},
        L"Cancel",
        [weak, teacherId](ClassMngrWinUIDialogs::DialogOutcome outcome) {
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
                classmngr::engine::NativeEnglishTeacherService service(
                    *self->m_openDatabase
                    );
                const auto removed = service.saveDirectory(
                    {},
                    {teacherId}
                    );
                if (!removed)
                {
                    self->m_nativeEnglishTeacherStatusText.Text(winrt::hstring(
                        L"Native English Teacher could not be deleted: "
                        + asWide(removed.error().message)
                        ));
                    return;
                }
                self->m_nativeEnglishTeacherSelectedId = -1;
                self->m_nativeEnglishTeacherSelectedIndex = -1;
                self->m_nativeEnglishTeacherDirty = false;
                self->m_nativeEnglishTeacherNew = false;
                self->m_dirtyState.markClean();
                self->refreshNativeEnglishTeachersPage();
                self->m_nativeEnglishTeacherStatusText.Text(
                    L"Native English Teacher deleted."
                    );
            }
        }
        );
}

void MainWindow::NativeEnglishTeacherSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_nativeEnglishTeacherStatusText.Text(L"No database open.");
        return;
    }

    const auto teacher = nativeEnglishTeacherFromForm();
    const std::wstring birthday = asWString(
        m_nativeEnglishTeacherBirthdayTextBox.Text()
        );
    if (!validMonthDay(birthday))
    {
        m_nativeEnglishTeacherStatusText.Text(
            L"Native English Teacher could not be saved."
            );
        m_nativeEnglishTeacherValidationText.Text(
            L"Each Native English Teacher needs a valid MM-dd birthday."
            );
        m_nativeEnglishTeacherValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_nativeEnglishTeacherDirty = true;
        m_dirtyState.markDirty();
        return;
    }

    std::vector<classmngr::engine::NativeEnglishTeacher> draft =
        m_nativeEnglishTeachers;
    bool replaced = false;
    if (teacher.id > 0)
    {
        for (auto& existing : draft)
        {
            if (existing.id == teacher.id)
            {
                existing = teacher;
                replaced = true;
                break;
            }
        }
    }
    if (!replaced)
    {
        draft.push_back(teacher);
    }

    classmngr::engine::NativeEnglishTeacherService service(*m_openDatabase);
    const auto saved = service.saveDirectory(draft, {});
    if (!saved)
    {
        m_nativeEnglishTeacherStatusText.Text(winrt::hstring(
            L"Native English Teacher could not be saved: "
            + asWide(saved.error().message)
            ));
        m_nativeEnglishTeacherValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_nativeEnglishTeacherValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    m_nativeEnglishTeacherSelectedId = teacher.id;
    m_nativeEnglishTeacherSelectedIndex = -1;
    m_nativeEnglishTeacherNew = false;
    m_nativeEnglishTeacherDirty = false;
    m_dirtyState.markClean();
    refreshNativeEnglishTeachersPage();
    m_nativeEnglishTeacherStatusText.Text(
        L"Native English Teacher saved."
        );
    m_nativeEnglishTeacherValidationText.Text({});
    m_nativeEnglishTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::NativeEnglishTeacherDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_nativeEnglishTeacherDirty = false;
    m_nativeEnglishTeacherNew = false;
    m_dirtyState.markClean();
    refreshNativeEnglishTeachersPage();
}

} // namespace winrt::ClassMngrWinUI::implementation
