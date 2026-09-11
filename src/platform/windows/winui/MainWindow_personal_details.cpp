#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populatePersonalDetailsPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    const auto host = page.try_as<
        Microsoft::UI::Xaml::Controls::ContentControl>();
    if (!host && !m_personalNameTextBox)
    {
        return;
    }
    populatePersonalDetailsPage(
        host,
        refresh
        );
}

void MainWindow::populatePersonalDetailsPage(
    Microsoft::UI::Xaml::Controls::ContentControl const& host,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (m_personalNameTextBox && host && m_personalDetailsScroll)
    {
        if (m_personalDetailsHost && m_personalDetailsHost != host)
        {
            // A disabled-cache Home page gets a fresh ContentControl on
            // every visit. Detach the retained view from the old host before
            // attaching it to the new one.
            m_personalDetailsHost.Content(nullptr);
        }

        const auto currentContent = host.Content().try_as<ScrollViewer>();
        if (currentContent != m_personalDetailsScroll)
        {
            host.Content(m_personalDetailsScroll);
        }
        m_personalDetailsHost = host;
    }

    if (!m_personalNameTextBox)
    {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);

        auto root = StackPanel();
        root.Padding(Thickness{32.0, 24.0, 32.0, 32.0});
        root.Spacing(18.0);
        root.MaxWidth(1180.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"My Information");
        title.FontSize(28.0);
        setAutomationName(title, L"My Information");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"Enter your personal information and choose the signature shown on generated documents."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(description, L"Personal details description");
        root.Children().Append(description);

        m_personalStatusText = TextBlock();
        m_personalStatusText.Text(L"Loading personal details...");
        m_personalStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(m_personalStatusText, L"Personal details status");
        root.Children().Append(m_personalStatusText);

        m_personalValidationText = TextBlock();
        m_personalValidationText.TextWrapping(TextWrapping::Wrap);
        m_personalValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(
            m_personalValidationText,
            L"Personal details validation summary"
            );
        root.Children().Append(m_personalValidationText);

        const auto makeTextBox = [this](
            wchar_t const* header,
            wchar_t const* automationName,
            wchar_t const* placeholder
            ) {
            auto box = TextBox();
            box.Header(box_value(hstring(header)));
            box.PlaceholderText(placeholder);
            box.MinWidth(150.0);
            box.HorizontalAlignment(HorizontalAlignment::Stretch);
            box.IsTabStop(true);
            box.TextChanging({this, &MainWindow::PersonalDetailsField_TextChanging});
            setAutomationName(box, automationName);
            return box;
        };

        auto detailsCard = ClassMngrWinUISharedUX::buildCard({
            L"My Information",
            L"These values are stored in the active ClassMngr database.",
            L"Personal details form"
            });
        m_personalNameTextBox = makeTextBox(
            L"My Name",
            L"Personal name",
            L"Enter your name"
            );
        m_personalNameTextBox.TabIndex(0);
        m_personalCampusCombo = ComboBox();
        m_personalCampusCombo.Header(box_value(hstring(L"My Campus")));
        m_personalCampusCombo.PlaceholderText(L"Select your campus");
        m_personalCampusCombo.MinWidth(150.0);
        m_personalCampusCombo.HorizontalAlignment(HorizontalAlignment::Stretch);
        m_personalCampusCombo.IsTabStop(true);
        m_personalCampusCombo.TabIndex(1);
        m_personalCampusCombo.SelectionChanged(
            {this, &MainWindow::PersonalDetailsCampus_SelectionChanged}
            );
        setAutomationName(m_personalCampusCombo, L"Personal campus");
        if (m_campusResourceRecords.empty())
        {
            const auto campusResources = loadPackagedCampusResources();
            if (campusResources)
            {
                m_campusResourceRecords = *campusResources;
            }
        }
        for (const CampusResourceView& campus : m_campusResourceRecords)
        {
            if (campus.campusName.empty())
            {
                continue;
            }
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(campus.campusName)));
            item.Tag(box_value(hstring(campus.campusName)));
            setAutomationName(item, L"Campus " + campus.campusName);
            m_personalCampusCombo.Items().Append(item);
        }
        m_personalZoomLoginIdTextBox = makeTextBox(
            L"Zoom Login ID",
            L"Zoom login ID",
            L"Enter your Zoom login ID"
            );
        m_personalZoomLoginIdTextBox.TabIndex(2);
        m_personalZoomPasswordBox = PasswordBox();
        m_personalZoomPasswordBox.Header(
            box_value(hstring(L"Zoom Password"))
            );
        m_personalZoomPasswordBox.MinWidth(150.0);
        m_personalZoomPasswordBox.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_personalZoomPasswordBox.IsTabStop(true);
        m_personalZoomPasswordBox.TabIndex(3);
        m_personalZoomPasswordBox.PasswordChanged(
            {this, &MainWindow::PersonalDetailsPassword_Changed}
            );
        setAutomationName(m_personalZoomPasswordBox, L"Zoom password");

        m_personalZoomNotAvailableCheck = CheckBox();
        m_personalZoomNotAvailableCheck.Content(
            box_value(hstring(L"N/A"))
            );
        m_personalZoomNotAvailableCheck.IsTabStop(true);
        m_personalZoomNotAvailableCheck.TabIndex(4);
        m_personalZoomNotAvailableCheck.VerticalAlignment(
            VerticalAlignment::Center
            );
        m_personalZoomNotAvailableCheck.Checked(
            {this, &MainWindow::PersonalDetailsZoomAvailability_Changed}
            );
        m_personalZoomNotAvailableCheck.Unchecked(
            {this, &MainWindow::PersonalDetailsZoomAvailability_Changed}
            );
        setAutomationName(
            m_personalZoomNotAvailableCheck,
            L"Zoom not available (N/A)"
            );
        auto informationFields = Grid();
        informationFields.ColumnSpacing(16.0);
        for (int index = 0; index < 4; ++index)
        {
            auto column = ColumnDefinition();
            column.Width(GridLengthHelper::FromValueAndType(
                1.0,
                GridUnitType::Star
                ));
            informationFields.ColumnDefinitions().Append(column);
        }
        auto zoomColumn = ColumnDefinition();
        zoomColumn.Width(GridLengthHelper::FromValueAndType(
            1.0,
            GridUnitType::Auto
            ));
        informationFields.ColumnDefinitions().Append(zoomColumn);
        Grid::SetColumn(m_personalNameTextBox, 0);
        Grid::SetColumn(m_personalCampusCombo, 1);
        Grid::SetColumn(m_personalZoomLoginIdTextBox, 2);
        Grid::SetColumn(m_personalZoomPasswordBox, 3);
        Grid::SetColumn(m_personalZoomNotAvailableCheck, 4);
        informationFields.Children().Append(m_personalNameTextBox);
        informationFields.Children().Append(m_personalCampusCombo);
        informationFields.Children().Append(m_personalZoomLoginIdTextBox);
        informationFields.Children().Append(m_personalZoomPasswordBox);
        informationFields.Children().Append(m_personalZoomNotAvailableCheck);
        detailsCard.content.Children().Append(informationFields);
        root.Children().Append(detailsCard.root);

        auto signatureTitle = TextBlock();
        signatureTitle.Text(L"Signature");
        signatureTitle.FontSize(24.0);
        setAutomationName(signatureTitle, L"Signature");
        root.Children().Append(signatureTitle);

        auto signatureCard = ClassMngrWinUISharedUX::buildCard({
            {},
            {},
            L"Personal signature form"
            });
        auto instructions = TextBlock();
        instructions.Text(L"Choose an image file or type your signature.");
        instructions.TextWrapping(TextWrapping::Wrap);
        setAutomationName(instructions, L"Signature instructions");
        signatureCard.content.Children().Append(instructions);
        auto modeButtons = StackPanel();
        modeButtons.Orientation(Orientation::Horizontal);
        modeButtons.Spacing(8.0);
        m_personalSignatureImageButton = Button();
        m_personalSignatureImageButton.Content(box_value(hstring(L"Image")));
        m_personalSignatureImageButton.IsTabStop(true);
        m_personalSignatureImageButton.TabIndex(5);
        m_personalSignatureImageButton.Click(
            [this](auto const&, auto const&) {
                if (m_personalSignatureModeCombo)
                {
                    m_personalSignatureModeCombo.SelectedIndex(0);
                }
            }
            );
        setAutomationName(
            m_personalSignatureImageButton,
            L"Image signature mode"
            );
        m_personalSignatureTypeButton = Button();
        m_personalSignatureTypeButton.Content(box_value(hstring(L"Type")));
        m_personalSignatureTypeButton.IsTabStop(true);
        m_personalSignatureTypeButton.TabIndex(6);
        m_personalSignatureTypeButton.Click(
            [this](auto const&, auto const&) {
                if (m_personalSignatureModeCombo)
                {
                    m_personalSignatureModeCombo.SelectedIndex(1);
                }
            }
            );
        setAutomationName(
            m_personalSignatureTypeButton,
            L"Typed signature mode"
            );
        modeButtons.Children().Append(m_personalSignatureImageButton);
        modeButtons.Children().Append(m_personalSignatureTypeButton);
        signatureCard.content.Children().Append(modeButtons);

        m_personalSignatureModeCombo = ComboBox();
        m_personalSignatureModeCombo.IsTabStop(false);
        m_personalSignatureModeCombo.Visibility(Visibility::Collapsed);
        auto imageMode = ComboBoxItem();
        imageMode.Content(box_value(hstring(L"Image")));
        setAutomationName(imageMode, L"Image signature mode");
        auto typedMode = ComboBoxItem();
        typedMode.Content(box_value(hstring(L"Type")));
        setAutomationName(typedMode, L"Typed signature mode");
        m_personalSignatureModeCombo.Items().Append(imageMode);
        m_personalSignatureModeCombo.Items().Append(typedMode);
        m_personalSignatureModeCombo.SelectionChanged(
            {this, &MainWindow::PersonalDetailsSignatureMode_SelectionChanged}
            );
        setAutomationName(
            m_personalSignatureModeCombo,
            L"Signature mode"
            );
        signatureCard.content.Children().Append(m_personalSignatureModeCombo);

        m_personalSignaturePreviewBorder = Border();
        m_personalSignaturePreviewBorder.MinHeight(150.0);
        m_personalSignaturePreviewBorder.Padding(
            Thickness{16.0, 12.0, 16.0, 12.0}
            );
        m_personalSignaturePreviewBorder.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_personalSignaturePreviewBorder.CornerRadius(
            CornerRadius{6.0, 6.0, 6.0, 6.0}
            );
        m_personalSignaturePreviewBorder.BorderThickness(
            Thickness{1.0, 1.0, 1.0, 1.0}
            );
        m_personalSignaturePreviewBorder.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 255, 255, 255}
                )
            );
        try
        {
            const auto resources = Application::Current().Resources();
            const auto stroke = resources.Lookup(
                box_value(hstring(L"Phase3ControlStrokeBrush"))
                );
            if (stroke)
            {
                m_personalSignaturePreviewBorder.BorderBrush(
                    stroke.as<Microsoft::UI::Xaml::Media::Brush>()
                    );
            }
        }
        catch (...)
        {
        }
        m_personalSignaturePreviewText = TextBlock();
        m_personalSignaturePreviewText.HorizontalAlignment(
            HorizontalAlignment::Center
            );
        m_personalSignaturePreviewText.VerticalAlignment(
            VerticalAlignment::Center
            );
        m_personalSignaturePreviewText.TextAlignment(TextAlignment::Center);
        m_personalSignaturePreviewText.TextWrapping(TextWrapping::Wrap);
        m_personalSignaturePreviewText.FontSize(38.0);
        m_personalSignaturePreviewText.Foreground(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 24, 24, 24}
                )
            );
        setAutomationName(m_personalSignaturePreviewText, L"Signature preview");
        m_personalSignaturePreviewBorder.Child(m_personalSignaturePreviewText);
        signatureCard.content.Children().Append(m_personalSignaturePreviewBorder);

        m_personalTypedSignatureTextBox = makeTextBox(
            L"Type your signature",
            L"Typed signature text",
            L"Type your name"
            );
        m_personalTypedSignatureTextBox.TabIndex(7);
        m_personalTypedSignatureControls = StackPanel();
        m_personalTypedSignatureControls.Spacing(8.0);
        m_personalTypedSignatureControls.Children().Append(
            m_personalTypedSignatureTextBox
            );
        m_personalSignatureFontCombo = ComboBox();
        m_personalSignatureFontCombo.Header(
            box_value(hstring(L"Signature style"))
            );
        m_personalSignatureFontCombo.MinWidth(280.0);
        m_personalSignatureFontCombo.IsTabStop(true);
        m_personalSignatureFontCombo.TabIndex(8);
        for (auto const& font : {
                 std::pair{0, L"Just Another Hand"},
                 std::pair{1, L"Dancing Script"},
                 std::pair{2, L"Great Vibes"},
                 std::pair{3, L"Caveat"}
             })
        {
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(font.second)));
            item.Tag(box_value(font.first));
            setAutomationName(item, font.second);
            m_personalSignatureFontCombo.Items().Append(item);
        }
        m_personalSignatureFontCombo.SelectionChanged(
            {this, &MainWindow::PersonalDetailsSignatureMode_SelectionChanged}
            );
        setAutomationName(
            m_personalSignatureFontCombo,
            L"Typed signature style"
            );
        m_personalSignatureFontCombo.Visibility(Visibility::Collapsed);
        m_personalTypedSignatureControls.Children().Append(
            m_personalSignatureFontCombo
            );
        auto styleLabel = TextBlock();
        styleLabel.Text(L"Choose a style");
        m_personalTypedSignatureControls.Children().Append(styleLabel);
        auto fontGrid = Grid();
        fontGrid.ColumnSpacing(8.0);
        fontGrid.RowSpacing(8.0);
        for (int index = 0; index < 2; ++index)
        {
            fontGrid.ColumnDefinitions().Append(ColumnDefinition());
            fontGrid.RowDefinitions().Append(RowDefinition());
        }
        m_personalSignatureFontButtons.clear();
        for (int index = 0; index < 4; ++index)
        {
            const auto item = m_personalSignatureFontCombo.Items().GetAt(index)
                .as<ComboBoxItem>();
            auto card = Border();
            card.Padding(Thickness{10.0, 8.0, 10.0, 8.0});
            card.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
            card.CornerRadius(CornerRadius{6.0, 6.0, 6.0, 6.0});
            auto cardContents = StackPanel();
            cardContents.Spacing(6.0);
            auto fontName = TextBlock();
            fontName.Text(hstring(boxedString(item.Content())));
            auto sample = TextBlock();
            sample.Text(L"Your Signature");
            sample.HorizontalAlignment(HorizontalAlignment::Center);
            sample.FontSize(28.0);
            const std::array<wchar_t const*, 4> sampleFonts{
                L"Comic Sans MS", L"Segoe Script", L"Lucida Handwriting", L"Segoe Print"};
            sample.FontFamily(Microsoft::UI::Xaml::Media::FontFamily(
                sampleFonts[static_cast<std::size_t>(index)]));
            auto select = Button();
            select.IsTabStop(true);
            select.TabIndex(9 + index);
            select.Click([this, index](auto const&, auto const&) {
                m_personalSignatureFontCombo.SelectedIndex(index);
            });
            setAutomationName(select, L"Use signature font " + boxedString(item.Content()));
            cardContents.Children().Append(fontName);
            cardContents.Children().Append(sample);
            cardContents.Children().Append(select);
            card.Child(cardContents);
            Grid::SetColumn(card, index % 2);
            Grid::SetRow(card, index / 2);
            fontGrid.Children().Append(card);
            m_personalSignatureFontButtons.push_back(select);
        }
        m_personalTypedSignatureControls.Children().Append(fontGrid);

        m_personalImageStatusText = TextBlock();
        m_personalImageStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_personalImageStatusText,
            L"Signature image status"
            );
        m_personalImageControls = StackPanel();
        m_personalImageControls.Spacing(8.0);
        m_personalImageControls.Children().Append(m_personalImageStatusText);
        auto imageActions = StackPanel();
        imageActions.Orientation(Orientation::Horizontal);
        imageActions.Spacing(8.0);
        auto replaceImageButton = Button();
        replaceImageButton.Content(box_value(hstring(L"Replace Signature...")));
        replaceImageButton.IsEnabled(false);
        replaceImageButton.IsTabStop(false);
        setAutomationName(
            replaceImageButton,
            L"Replace signature image (Phase 7 placeholder)"
            );
        auto removeImageButton = Button();
        removeImageButton.Content(box_value(hstring(L"Remove")));
        removeImageButton.IsEnabled(false);
        removeImageButton.IsTabStop(false);
        setAutomationName(
            removeImageButton,
            L"Remove signature image (Phase 7 placeholder)"
            );
        imageActions.Children().Append(replaceImageButton);
        imageActions.Children().Append(removeImageButton);
        m_personalImageControls.Children().Append(imageActions);
        signatureCard.content.Children().Append(m_personalImageControls);
        signatureCard.content.Children().Append(m_personalTypedSignatureControls);
        root.Children().Append(signatureCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_personalSaveButton = Button();
        m_personalSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_personalSaveButton.IsTabStop(true);
        m_personalSaveButton.TabIndex(13);
        m_personalSaveButton.Click(
            {this, &MainWindow::PersonalDetailsSaveButton_Click}
            );
        setAutomationName(m_personalSaveButton, L"Save personal details");
        m_personalDiscardButton = Button();
        m_personalDiscardButton.Content(box_value(hstring(L"Discard Changes")));
        m_personalDiscardButton.IsTabStop(true);
        m_personalDiscardButton.TabIndex(14);
        m_personalDiscardButton.Click(
            {this, &MainWindow::PersonalDetailsDiscardButton_Click}
            );
        setAutomationName(
            m_personalDiscardButton,
            L"Discard personal detail changes"
            );
        actions.Children().Append(m_personalSaveButton);
        actions.Children().Append(m_personalDiscardButton);
        root.Children().Append(actions);

        scroll.Content(root);
        m_personalDetailsScroll = scroll;
        host.Content(scroll);
        m_personalDetailsHost = host;
        m_personalSignatureModeCombo.SelectedIndex(0);
        m_personalSignatureFontCombo.SelectedIndex(0);
        updatePersonalSignatureControls();
    }

    const auto setEditable = [this](bool enabled) {
        const auto checkedValue = m_personalZoomNotAvailableCheck.IsChecked();
        const bool zoomNotAvailable = checkedValue && checkedValue.Value();
        if (m_personalNameTextBox)
        {
            m_personalNameTextBox.IsEnabled(enabled);
        }
        if (m_personalCampusCombo)
        {
            m_personalCampusCombo.IsEnabled(enabled);
        }
        if (m_personalZoomLoginIdTextBox)
        {
            m_personalZoomLoginIdTextBox.IsEnabled(
                enabled && !zoomNotAvailable
                );
        }
        if (m_personalZoomPasswordBox)
        {
            m_personalZoomPasswordBox.IsEnabled(
                enabled && !zoomNotAvailable
                );
        }
        if (m_personalZoomNotAvailableCheck)
        {
            m_personalZoomNotAvailableCheck.IsEnabled(enabled);
        }
        if (m_personalSignatureModeCombo)
        {
            m_personalSignatureModeCombo.IsEnabled(enabled);
        }
        if (m_personalSignatureImageButton)
        {
            m_personalSignatureImageButton.IsEnabled(enabled);
        }
        if (m_personalSignatureTypeButton)
        {
            m_personalSignatureTypeButton.IsEnabled(enabled);
        }
        if (m_personalTypedSignatureTextBox)
        {
            m_personalTypedSignatureTextBox.IsEnabled(
                enabled && m_personalSignatureModeCombo.SelectedIndex() == 1
                );
        }
        if (m_personalSignatureFontCombo)
        {
            m_personalSignatureFontCombo.IsEnabled(
                enabled && m_personalSignatureModeCombo.SelectedIndex() == 1
                );
        }
        for (const auto& button : m_personalSignatureFontButtons)
        {
            button.IsEnabled(
                enabled && m_personalSignatureModeCombo.SelectedIndex() == 1
                );
        }
        if (m_personalSaveButton)
        {
            m_personalSaveButton.IsEnabled(enabled && m_personalDetailsDirty);
        }
        if (m_personalDiscardButton)
        {
            m_personalDiscardButton.IsEnabled(enabled && m_personalDetailsDirty);
        }
        updatePersonalSignatureControls();
    };

    if (!m_openDatabase)
    {
        m_personalDetailsLoading = true;
        m_personalDetailsLoaded = false;
        m_personalDetailsDirty = false;
        m_personalDetails = {};
        m_personalNameTextBox.Text({});
        m_personalCampusCombo.SelectedIndex(-1);
        m_personalZoomLoginIdTextBox.Text({});
        m_personalZoomPasswordBox.Password({});
        m_personalZoomNotAvailableCheck.IsChecked(false);
        m_personalSignatureModeCombo.SelectedIndex(0);
        m_personalSignatureFontCombo.SelectedIndex(0);
        m_personalTypedSignatureTextBox.Text({});
        m_personalImageStatusText.Text(
            L"No database is open. Open a .tps or .db file to edit personal details."
            );
        m_personalStatusText.Text(L"No database open.");
        m_personalValidationText.Text({});
        m_personalValidationText.Visibility(Visibility::Collapsed);
        m_personalDetailsLoading = false;
        setEditable(false);
        return;
    }

    if (refresh && m_personalDetailsDirty)
    {
        m_personalStatusText.Text(L"Unsaved personal detail changes are retained.");
        return;
    }

    m_personalDetailsLoading = true;
    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    classmngr::engine::PersonalDetailsService service(settings);
    const auto loaded = service.load();
    if (!loaded)
    {
        m_personalDetailsLoaded = false;
        m_personalDetailsDirty = false;
        m_personalStatusText.Text(winrt::hstring(
            L"Personal details could not be loaded: "
            + asWide(loaded.error().message)
            ));
        m_personalValidationText.Text(L"The engine rejected the personal-details read.");
        m_personalValidationText.Visibility(Visibility::Visible);
        m_personalDetailsLoading = false;
        setEditable(false);
        return;
    }

    m_personalDetails = *loaded;
    m_personalNameTextBox.Text(asWide(m_personalDetails.name));
    int campusIndex = -1;
    const std::wstring savedCampus = asWide(m_personalDetails.campus);
    for (uint32_t index = 0; index < m_personalCampusCombo.Items().Size(); ++index)
    {
        const auto item = m_personalCampusCombo.Items().GetAt(index).try_as<
            Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        if (item
            && (equalsIgnoreCase(boxedString(item.Tag()), savedCampus)
                || equalsIgnoreCase(boxedString(item.Content()), savedCampus)))
        {
            campusIndex = static_cast<int>(index);
            break;
        }
    }
    if (campusIndex < 0 && !savedCampus.empty())
    {
        auto item = Microsoft::UI::Xaml::Controls::ComboBoxItem();
        item.Content(box_value(hstring(savedCampus)));
        item.Tag(box_value(hstring(savedCampus)));
        setAutomationName(item, L"Campus " + savedCampus);
        m_personalCampusCombo.Items().Append(item);
        campusIndex = static_cast<int>(m_personalCampusCombo.Items().Size()) - 1;
    }
    m_personalCampusCombo.SelectedIndex(
        campusIndex >= 0
            ? campusIndex
            : (m_personalCampusCombo.Items().Size() > 0 ? 0 : -1)
        );
    m_personalZoomLoginIdTextBox.Text(asWide(m_personalDetails.zoomLoginId));
    m_personalZoomPasswordBox.Password(asWide(m_personalDetails.zoomPassword));
    m_personalZoomNotAvailableCheck.IsChecked(m_personalDetails.zoomNotAvailable);
    m_personalSignatureModeCombo.SelectedIndex(
        m_personalDetails.signatureMode
                == classmngr::engine::SignatureMode::Type
            ? 1
            : 0
        );
    m_personalTypedSignatureTextBox.Text(
        asWide(m_personalDetails.typedSignatureText)
        );
    m_personalSignatureFontCombo.SelectedIndex(
        m_personalDetails.typedSignatureFont >= 0
            && m_personalDetails.typedSignatureFont < 4
            ? m_personalDetails.typedSignatureFont
            : 0
        );
    m_personalDetailsLoaded = true;
    m_personalDetailsDirty = false;
    m_personalStatusText.Text(L"Personal details loaded.");
    m_personalValidationText.Text({});
    m_personalValidationText.Visibility(Visibility::Collapsed);
    m_personalImageStatusText.Text(
        m_personalDetails.signatureImageBase64.empty()
            ? L"Signature image selection will be added in a later slice."
            : L"An existing signature image is retained by the engine; image selection is not available in this slice."
        );
    m_personalDetailsLoading = false;
    setEditable(true);
    updatePersonalSignatureControls();
}

void MainWindow::updatePersonalSignatureControls()
{
    if (!m_personalSignatureModeCombo)
    {
        return;
    }

    const bool typed = m_personalSignatureModeCombo.SelectedIndex() == 1;
    if (m_personalImageControls)
    {
        m_personalImageControls.Visibility(
            typed
                ? Microsoft::UI::Xaml::Visibility::Collapsed
                : Microsoft::UI::Xaml::Visibility::Visible
            );
    }
    if (m_personalTypedSignatureControls)
    {
        m_personalTypedSignatureControls.Visibility(
            typed
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed
            );
    }

    const auto setModeStyle = [](Microsoft::UI::Xaml::Controls::Button const& button,
                                 bool selected) {
        if (!button)
        {
            return;
        }
        try
        {
            const auto resource =
                Microsoft::UI::Xaml::Application::Current().Resources().Lookup(
                    box_value(hstring(
                        selected
                            ? L"Phase3PrimaryButtonStyle"
                            : L"Phase3SecondaryButtonStyle"
                        ))
                    );
            if (resource)
            {
                button.Style(resource.as<Microsoft::UI::Xaml::Style>());
            }
        }
        catch (...)
        {
        }
    };
    setModeStyle(m_personalSignatureImageButton, !typed);
    setModeStyle(m_personalSignatureTypeButton, typed);
    const int selectedFont = m_personalSignatureFontCombo
        ? m_personalSignatureFontCombo.SelectedIndex()
        : -1;
    for (std::size_t index = 0;
         index < m_personalSignatureFontButtons.size();
         ++index)
    {
        const bool selected = static_cast<int>(index) == selectedFont;
        auto const& button = m_personalSignatureFontButtons[index];
        button.Content(box_value(hstring(
            selected ? L"Selected" : L"Use this font"
            )));
        setModeStyle(button, selected);
    }
    updatePersonalSignaturePreview();
}

void MainWindow::updatePersonalSignaturePreview()
{
    if (!m_personalSignaturePreviewText || !m_personalSignatureModeCombo)
    {
        return;
    }

    const bool typed = m_personalSignatureModeCombo.SelectedIndex() == 1;
    if (!typed)
    {
        m_personalSignaturePreviewText.FontFamily(
            Microsoft::UI::Xaml::Media::FontFamily(L"Segoe UI")
            );
        m_personalSignaturePreviewText.FontSize(22.0);
        m_personalSignaturePreviewText.Text(
            m_personalDetails.signatureImageBase64.empty()
                ? L"No signature image added yet."
                : L"Existing signature image retained.\nPreview unavailable in this phase."
            );
        return;
    }

    const std::wstring typedText = asWString(
        m_personalTypedSignatureTextBox.Text()
        );
    m_personalSignaturePreviewText.Text(
        typedText.find_first_not_of(L" \t\r\n") == std::wstring::npos
            ? L"Your Signature"
            : winrt::hstring(typedText)
        );
    m_personalSignaturePreviewText.FontSize(38.0);
    const std::array<wchar_t const*, 4> previewFonts{
        L"Comic Sans MS",
        L"Segoe Script",
        L"Lucida Handwriting",
        L"Segoe Print"
    };
    const int selectedFont = m_personalSignatureFontCombo
        ? m_personalSignatureFontCombo.SelectedIndex()
        : 0;
    const int normalizedFont = selectedFont >= 0 && selectedFont < 4
        ? selectedFont
        : 0;
    m_personalSignaturePreviewText.FontFamily(
        Microsoft::UI::Xaml::Media::FontFamily(
            previewFonts[static_cast<std::size_t>(normalizedFont)]
            )
        );
}

void MainWindow::refreshPersonalDetailsPage()
{
    if (!m_contentFrame
        || (m_currentPageId != homePageId
            && m_currentPageId != personalDetailsPageId))
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populatePersonalDetailsPage(page, true);
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
