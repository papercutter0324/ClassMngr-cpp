#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

Microsoft::UI::Xaml::Controls::StackPanel
MainWindow::buildClassDetailsSection()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto makeRoot = [](StackPanel const& content) {
        content.Padding(Thickness{16.0, 12.0, 16.0, 24.0});
        content.Spacing(16.0);
        content.MaxWidth(2000.0);
        content.HorizontalAlignment(HorizontalAlignment::Stretch);
        return content;
    };

    auto detailsRoot = makeRoot(StackPanel());

    auto detailsDescription = TextBlock();
    detailsDescription.Text(
        L"Edit class information shared by schedules, rosters, and reports."
        );
    detailsDescription.TextWrapping(TextWrapping::Wrap);
    detailsDescription.Visibility(Visibility::Collapsed);
    setAutomationName(
        detailsDescription,
        L"Class details and information description"
        );
    detailsRoot.Children().Append(detailsDescription);

    m_classStatusText = TextBlock();
    m_classStatusText.Text({});
    m_classStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_classStatusText, L"Class information status");
    detailsRoot.Children().Append(m_classStatusText);

    m_classValidationText = TextBlock();
    m_classValidationText.TextWrapping(TextWrapping::Wrap);
    m_classValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    setAutomationName(
        m_classValidationText,
        L"Class information validation summary"
        );
    detailsRoot.Children().Append(m_classValidationText);

    // Class selection is presented by the page-level navigation controls. Keep
    // this selector as the existing state and event bridge for navigation and
    // diagnostic paths without adding a second selector section here.
    m_classSelector = ComboBox();
    m_classSelector.Header(box_value(hstring(L"Class")));
    m_classSelector.PlaceholderText(L"Select a class");
    m_classSelector.MinWidth(420.0);
    m_classSelector.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_classSelector.IsTabStop(true);
    m_classSelector.TabIndex(0);
    m_classSelector.SelectionChanged({
        this,
        &MainWindow::ClassSelection_SelectionChanged
        });
    setAutomationName(m_classSelector, L"Class selector");

    const auto makeClassTextBox = [this](
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
        box.TextChanging({this, &MainWindow::ClassField_TextChanging});
        setAutomationName(box, automationName);
        return box;
    };

    auto detailsCard = ClassMngrWinUISharedUX::buildCard({
        L"Class information",
        L"",
        L"Class information form"
        });
    detailsCard.root.Padding(Thickness{20.0, 20.0, 20.0, 20.0});
    detailsCard.content.Spacing(16.0);

    m_classNameTextBox = makeClassTextBox(
        L"Class name",
        L"Class name",
        L"Enter a class name"
        );
    auto classNameInputScope = Input::InputScope();
    classNameInputScope.Names().Append(
        Input::InputScopeName(Input::InputScopeNameValue::Text)
        );
    m_classNameTextBox.InputScope(classNameInputScope);
    m_classNameTextBox.TabIndex(1);
    detailsCard.content.Children().Append(m_classNameTextBox);

    const auto appendChoice = [](ComboBox combo,
                                 std::wstring_view display,
                                 std::wstring_view value) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(value)));
        combo.Items().Append(item);
    };
    const auto configureClassCombo = [this](
        ComboBox combo,
        wchar_t const* header,
        wchar_t const* automationName,
        int tabIndex
        ) {
        combo.Header(box_value(hstring(header)));
        combo.MinWidth(320.0);
        combo.HorizontalAlignment(HorizontalAlignment::Stretch);
        combo.IsTabStop(true);
        combo.TabIndex(tabIndex);
        combo.SelectionChanged({this, &MainWindow::ClassField_SelectionChanged});
        setAutomationName(combo, automationName);
    };

    m_classGradeCombo = ComboBox();
    configureClassCombo(
        m_classGradeCombo,
        L"Grade",
        L"Class grade",
        2
        );
    m_classGradeCombo.MinWidth(130.0);
    m_classGradeCombo.Width(130.0);
    appendChoice(m_classGradeCombo, L"Not set", L"");
    for (const std::string& value :
         classmngr::engine::ClassInfoConfig::grades())
    {
        const std::wstring wideValue = asWide(value);
        appendChoice(m_classGradeCombo, wideValue, wideValue);
    }

    m_classLevelCombo = ComboBox();
    configureClassCombo(
        m_classLevelCombo,
        L"Level",
        L"Class level",
        3
        );
    m_classLevelCombo.MinWidth(180.0);
    m_classLevelCombo.Width(180.0);

    m_classReadingBookCombo = ComboBox();
    configureClassCombo(
        m_classReadingBookCombo,
        L"Reading Book",
        L"Class reading book",
        4
        );
    m_classReadingBookCombo.MinWidth(300.0);

    m_classEssayBookCombo = ComboBox();
    configureClassCombo(
        m_classEssayBookCombo,
        L"Essay Book",
        L"Class essay book",
        5
        );
    m_classEssayBookCombo.MinWidth(120.0);
    m_classEssayBookCombo.Width(120.0);

    m_classColorTextBox = makeClassTextBox(
        L"Color backing value",
        L"Class color backing value",
        L""
        );
    m_classColorTextBox.IsTabStop(false);
    m_classColorPreview = Border();
    m_classColorPreview.Width(40.0);
    m_classColorPreview.Height(24.0);
    m_classColorPreview.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
    m_classColorPreview.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
    m_classColorPreview.BorderBrush(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 128, 128, 128}
            )
        );
    setAutomationName(m_classColorPreview, L"Class color preview");
    m_classColorChooseButton = Button();
    m_classColorChooseButton.Content(box_value(hstring(L"Choose Color")));
    m_classColorChooseButton.IsTabStop(true);
    m_classColorChooseButton.TabIndex(6);
    setAutomationName(m_classColorChooseButton, L"Choose class color");
    m_classFontColorTextBox = makeClassTextBox(
        L"Font color backing value",
        L"Class font color backing value",
        L""
        );
    m_classFontColorTextBox.IsTabStop(false);

    m_classStudentCountTextBox = TextBox();
    m_classStudentCountTextBox.Header(box_value(hstring(L"# of Students")));
    m_classStudentCountTextBox.MinWidth(64.0);
    m_classStudentCountTextBox.Width(64.0);
    m_classStudentCountTextBox.IsReadOnly(true);
    m_classStudentCountTextBox.IsTabStop(false);
    setAutomationName(m_classStudentCountTextBox, L"Class student count");

    m_classTeacherText = TextBlock();
    m_classTeacherText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_classTeacherText, L"Assigned class teacher");

    auto colorField = StackPanel();
    colorField.Spacing(4.0);
    auto colorLabel = TextBlock();
    colorLabel.Text(L"Color");
    applyResourceStyle(colorLabel, L"Phase3BodyTextBlockStyle");
    auto colorControls = StackPanel();
    colorControls.Orientation(Orientation::Horizontal);
    colorControls.Spacing(5.0);
    colorControls.Children().Append(m_classColorPreview);
    colorControls.Children().Append(m_classColorChooseButton);
    colorField.Children().Append(colorLabel);
    colorField.Children().Append(colorControls);

    const auto openClassColorPicker = [weak = get_weak()](auto const&, auto const&)
        -> winrt::fire_and_forget {
        const auto self = weak.get();
        if (!self || self->m_ownedDialog || !self->m_classColorTextBox
            || !self->RootGrid().XamlRoot())
        {
            co_return;
        }
        auto picker = ClassMngrWinUISharedUX::buildColorPickerDialog(
            self->RootGrid().XamlRoot(),
            L"Select Class Color",
            uiColorFromHex(asUtf8(self->m_classColorTextBox.Text())),
            L"Class color picker"
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
        if (result == ContentDialogResult::Primary && self->m_classColorTextBox)
        {
            const std::string value = uiHexFromColor(picker.picker.Color());
            self->m_classColorTextBox.Text(asWide(value));
            if (self->m_classColorPreview)
            {
                self->m_classColorPreview.Background(
                    Microsoft::UI::Xaml::Media::SolidColorBrush(
                        uiColorFromHex(value)
                        )
                    );
            }
            self->m_classDetailsDirty = true;
            self->markClassDirty();
        }
    };
    m_classColorChooseButton.Click(openClassColorPicker);
    m_classColorPreview.Tapped(openClassColorPicker);

    auto detailsGrid = Grid();
    detailsGrid.ColumnSpacing(16.0);
    detailsGrid.RowSpacing(8.0);
    const std::array<double, 4> detailWidths{
        200.0, 130.0, 180.0, 120.0
    };
    for (const double width : detailWidths)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            width,
            GridUnitType::Pixel
            ));
        detailsGrid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 2; ++row)
    {
        detailsGrid.RowDefinitions().Append(RowDefinition());
    }
    Grid::SetRow(colorField, 0);
    Grid::SetColumn(colorField, 0);
    Grid::SetRow(m_classGradeCombo, 0);
    Grid::SetColumn(m_classGradeCombo, 1);
    Grid::SetRow(m_classLevelCombo, 0);
    Grid::SetColumn(m_classLevelCombo, 2);
    Grid::SetRow(m_classStudentCountTextBox, 0);
    Grid::SetColumn(m_classStudentCountTextBox, 3);
    Grid::SetRow(m_classReadingBookCombo, 1);
    Grid::SetColumn(m_classReadingBookCombo, 0);
    Grid::SetColumnSpan(m_classReadingBookCombo, 2);
    Grid::SetRow(m_classEssayBookCombo, 1);
    Grid::SetColumn(m_classEssayBookCombo, 2);
    detailsGrid.Children().Append(colorField);
    detailsGrid.Children().Append(m_classGradeCombo);
    detailsGrid.Children().Append(m_classLevelCombo);
    detailsGrid.Children().Append(m_classStudentCountTextBox);
    detailsGrid.Children().Append(m_classReadingBookCombo);
    detailsGrid.Children().Append(m_classEssayBookCombo);
    detailsCard.content.Children().Append(detailsGrid);
    detailsRoot.Children().Append(detailsCard.root);

    auto detailsActions = StackPanel();
    detailsActions.Orientation(Orientation::Horizontal);
    detailsActions.Spacing(8.0);
    m_classNewButton = Button();
    m_classNewButton.Content(box_value(hstring(L"New Class")));
    m_classNewButton.IsTabStop(true);
    m_classNewButton.TabIndex(8);
    m_classNewButton.Click({this, &MainWindow::ClassNewButton_Click});
    setAutomationName(m_classNewButton, L"New class");
    m_classDeleteButton = Button();
    m_classDeleteButton.Content(box_value(hstring(L"Delete Class")));
    m_classDeleteButton.IsTabStop(true);
    m_classDeleteButton.TabIndex(9);
    m_classDeleteButton.Click({this, &MainWindow::ClassDeleteButton_Click});
    setAutomationName(m_classDeleteButton, L"Delete class");
    m_classSaveButton = Button();
    m_classSaveButton.Content(box_value(hstring(L"Save Changes")));
    m_classSaveButton.IsTabStop(true);
    m_classSaveButton.TabIndex(10);
    m_classSaveButton.Click({this, &MainWindow::ClassSaveButton_Click});
    setAutomationName(m_classSaveButton, L"Save class information");
    m_classDiscardButton = Button();
    m_classDiscardButton.Content(box_value(hstring(L"Discard Changes")));
    m_classDiscardButton.IsTabStop(true);
    m_classDiscardButton.TabIndex(11);
    m_classDiscardButton.Click({this, &MainWindow::ClassDiscardButton_Click});
    setAutomationName(m_classDiscardButton, L"Discard class information changes");
    // These controls remain instantiated for legacy command and semantic-check
    // paths, but Class Details now keeps actions out of its visual tree.

    auto scheduleCard = ClassMngrWinUISharedUX::buildCard({
        L"Class Times",
        L"",
        L"Class schedule editor"
        });
    scheduleCard.root.Padding(Thickness{20.0, 20.0, 20.0, 20.0});
    scheduleCard.content.Spacing(16.0);

    auto regularScheduleTitle = TextBlock();
    regularScheduleTitle.Text(L"Regular Schedule");
    applyResourceStyle(regularScheduleTitle, L"Phase3BodyTextBlockStyle");
    regularScheduleTitle.FontStyle(
        Windows::UI::Text::FontStyle::Italic
        );
    scheduleCard.content.Children().Append(regularScheduleTitle);
    m_classRegularScheduleGrid = Grid();
    m_classRegularScheduleGrid.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    setAutomationName(
        m_classRegularScheduleGrid,
        L"Regular class schedule"
        );
    scheduleCard.content.Children().Append(m_classRegularScheduleGrid);
    m_classRegularScheduleAddButton = Button();
    m_classRegularScheduleAddButton.Content(
        box_value(hstring(L"+ Add Time"))
        );
    m_classRegularScheduleAddButton.MinWidth(200.0);
    m_classRegularScheduleAddButton.IsTabStop(true);
    m_classRegularScheduleAddButton.Click(
        [this](auto const&, auto const&) {
            addClassScheduleRow(false);
        }
        );
    setAutomationName(
        m_classRegularScheduleAddButton,
        L"Add regular class time"
        );
    scheduleCard.content.Children().Append(m_classRegularScheduleAddButton);

    auto scheduleDivider = Border();
    scheduleDivider.Height(1.0);
    scheduleDivider.Background(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 92, 99, 108}
            )
        );
    scheduleCard.content.Children().Append(scheduleDivider);

    auto intensiveScheduleTitle = TextBlock();
    intensiveScheduleTitle.Text(L"Intensive Schedule");
    applyResourceStyle(intensiveScheduleTitle, L"Phase3BodyTextBlockStyle");
    intensiveScheduleTitle.FontStyle(
        Windows::UI::Text::FontStyle::Italic
        );
    scheduleCard.content.Children().Append(intensiveScheduleTitle);
    m_classIntensiveScheduleGrid = Grid();
    m_classIntensiveScheduleGrid.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    setAutomationName(
        m_classIntensiveScheduleGrid,
        L"Intensive class schedule"
        );
    scheduleCard.content.Children().Append(m_classIntensiveScheduleGrid);
    m_classIntensiveScheduleAddButton = Button();
    m_classIntensiveScheduleAddButton.Content(
        box_value(hstring(L"+ Add Intensive Time"))
        );
    m_classIntensiveScheduleAddButton.MinWidth(200.0);
    m_classIntensiveScheduleAddButton.IsTabStop(true);
    m_classIntensiveScheduleAddButton.Click(
        [this](auto const&, auto const&) {
            addClassScheduleRow(true);
        }
        );
    setAutomationName(
        m_classIntensiveScheduleAddButton,
        L"Add intensive class time"
        );
    scheduleCard.content.Children().Append(m_classIntensiveScheduleAddButton);
    detailsRoot.Children().Append(scheduleCard.root);


    return detailsRoot;
}

Microsoft::UI::Xaml::Controls::StackPanel
MainWindow::buildClassNotesSection()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto makeRoot = [](StackPanel const& content) {
        content.Padding(Thickness{16.0, 12.0, 16.0, 24.0});
        content.Spacing(16.0);
        content.MaxWidth(2000.0);
        content.HorizontalAlignment(HorizontalAlignment::Stretch);
        return content;
    };

    const auto makeClassTextBox = [this](
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
        box.TextChanging({this, &MainWindow::ClassField_TextChanging});
        setAutomationName(box, automationName);
        return box;
    };

    auto notesRoot = makeRoot(StackPanel());

    auto notesDescription = TextBlock();
    notesDescription.Text(
        L"Keep class notes and time-filler activities with the selected class."
        );
    notesDescription.TextWrapping(TextWrapping::Wrap);
    notesDescription.Visibility(Visibility::Collapsed);
    setAutomationName(notesDescription, L"Class notes description");
    notesRoot.Children().Append(notesDescription);

    m_classNotesStatusText = TextBlock();
    m_classNotesStatusText.Text(L"Select a class to edit notes.");
    m_classNotesStatusText.TextWrapping(TextWrapping::Wrap);
    applyResourceStyle(m_classNotesStatusText, L"Phase4CardDescriptionTextBlockStyle");
    m_classNotesStatusText.Visibility(Visibility::Collapsed);
    setAutomationName(m_classNotesStatusText, L"Class notes status");
    notesRoot.Children().Append(m_classNotesStatusText);

    m_classNotesValidationText = TextBlock();
    m_classNotesValidationText.TextWrapping(TextWrapping::Wrap);
    m_classNotesValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_classNotesValidationText,
        L"Class notes validation summary"
        );
    notesRoot.Children().Append(m_classNotesValidationText);

    auto notesCard = ClassMngrWinUISharedUX::buildCard({
        L"Notes",
        L"",
        L"Class notes form"
        });
    notesCard.root.Padding(Thickness{24.0, 24.0, 24.0, 24.0});
    notesCard.root.BorderThickness(Thickness{2.0, 2.0, 2.0, 2.0});
    notesCard.root.BorderBrush(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 0, 120, 212}
            )
        );
    notesCard.content.Spacing(24.0);
    m_classNotesTextBox = makeClassTextBox(
        L"Notes",
        L"Class notes editor",
        L"Enter notes for this class"
        );
    m_classNotesTextBox.AcceptsReturn(true);
    m_classNotesTextBox.TextWrapping(TextWrapping::Wrap);
    m_classNotesTextBox.Height(244.0);
    m_classNotesTextBox.VerticalContentAlignment(VerticalAlignment::Top);
    m_classNotesTextBox.MaxLength(10000);
    m_classNotesTextBox.TabIndex(0);
    m_classTimeFillerActivitiesTextBox = makeClassTextBox(
        L"Time Filler Activities",
        L"Class time filler activities editor",
        L"Enter time-filler activities"
        );
    m_classTimeFillerActivitiesTextBox.AcceptsReturn(true);
    m_classTimeFillerActivitiesTextBox.TextWrapping(TextWrapping::Wrap);
    m_classTimeFillerActivitiesTextBox.Height(244.0);
    m_classTimeFillerActivitiesTextBox.VerticalContentAlignment(
        VerticalAlignment::Top
        );
    m_classTimeFillerActivitiesTextBox.MaxLength(10000);
    m_classTimeFillerActivitiesTextBox.TabIndex(1);
    notesCard.content.Children().Append(m_classNotesTextBox);
    notesCard.content.Children().Append(m_classTimeFillerActivitiesTextBox);
    notesRoot.Children().Append(notesCard.root);

    m_classNotesSaveButton = Button();
    m_classNotesSaveButton.Content(box_value(hstring(L"Save Changes")));
    m_classNotesSaveButton.IsTabStop(true);
    m_classNotesSaveButton.TabIndex(2);
    m_classNotesSaveButton.Click({this, &MainWindow::ClassNotesSaveButton_Click});
    setAutomationName(m_classNotesSaveButton, L"Save class notes");
    m_classNotesDiscardButton = Button();
    m_classNotesDiscardButton.Content(box_value(hstring(L"Discard Changes")));
    m_classNotesDiscardButton.IsTabStop(true);
    m_classNotesDiscardButton.TabIndex(3);
    m_classNotesDiscardButton.Click({this, &MainWindow::ClassNotesDiscardButton_Click});
    setAutomationName(m_classNotesDiscardButton, L"Discard class notes changes");
    m_classNotesSaveButton.Visibility(Visibility::Collapsed);
    m_classNotesDiscardButton.Visibility(Visibility::Collapsed);


    return notesRoot;
}

} // namespace winrt::ClassMngrWinUI::implementation
