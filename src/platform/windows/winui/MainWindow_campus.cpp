#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populateAboutPage(
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
    title.Text(L"About ClassMngr");
    title.FontSize(28.0);
    setAutomationName(title, L"About ClassMngr");

    auto description = TextBlock();
    description.Text(
        L"This page is a lazy shell placeholder for the first feature slice."
        );
    description.TextWrapping(TextWrapping::Wrap);

    auto navigationContract = TextBlock();
    navigationContract.Text(
        L"Navigation history and shell state are restored by the main window."
        );
    navigationContract.TextWrapping(TextWrapping::Wrap);

    root.Children().Append(title);
    root.Children().Append(description);
    root.Children().Append(navigationContract);
    page.Content(root);
}

void MainWindow::populateCampusPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    std::wstring_view pageId,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    const auto localize = [this](std::wstring_view source) {
        return m_localizer.getString(L"CampusInformationPage", source);
    };

    if (page.Content() && !refresh)
    {
        return;
    }

    m_campusRecords.clear();
    m_campusResourceRecords.clear();
    m_campusSelector = nullptr;
    m_campusTabs = nullptr;
    m_campusDetailsPanel = nullptr;
    m_campusImage = nullptr;
    m_campusImages.clear();
    ++m_campusImageRequest;
    m_campusInformationState.clear();

    auto pageTitle = [&]() -> std::wstring {
        if (pageId == campusDirectionsPageId)
        {
            return localize(L"Directions");
        }
        if (pageId == campusAddressPageId)
        {
            return localize(L"Address");
        }
        if (pageId == campusHousingPageId)
        {
            return localize(L"Housing");
        }
        if (pageId == campusMapPageId)
        {
            return localize(L"Maps");
        }
        return localize(L"Campus Information");
    };

    auto root = StackPanel();
    // Mirrors the retained Qt content layout: selector above five detail tabs.
    root.Padding(Thickness{12.0, 12.0, 12.0, 0.0});
    root.Spacing(8.0);
    root.VerticalAlignment(VerticalAlignment::Top);

    auto title = TextBlock();
    title.Text(winrt::hstring(pageTitle()));
    title.FontSize(28.0);
    setAutomationName(title, pageTitle());
    root.Children().Append(title);

    const auto appendState = [this, &root, &localize](
                                 winrt::hstring const& titleText,
                                 winrt::hstring const& messageText,
                                 std::wstring_view automationName,
                                 bool errorState) {
        m_campusInformationState = errorState
            ? L"engine_error"
            : titleText == winrt::hstring(localize(L"No database open"))
                ? L"no_database"
                : L"empty";
        const auto state = errorState
            ? ClassMngrWinUISharedUX::buildErrorState({
                  titleText,
                  messageText,
                  winrt::hstring(automationName),
                  {}
                  })
            : ClassMngrWinUISharedUX::buildEmptyState({
                  titleText,
                  messageText,
                  winrt::hstring(automationName),
                  {}
                  });
        root.Children().Append(state.root);
    };

    if (m_phase5CampusScenario == L"error")
    {
        appendState(
            winrt::hstring(localize(L"Campus information unavailable")),
            winrt::hstring(localize(
                L"The engine returned an unexpected campus loading failure."
                )),
            L"Campus information engine error state",
            true
            );
        auto pageScroll = ScrollViewer();
        pageScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        pageScroll.Content(root);
        page.Content(pageScroll);
        return;
    }

    // The phase verifier intentionally uses database scenarios to exercise
    // the engine contract.  The production directory follows Qt and reads
    // the packaged campus JSON catalog, which is independent of the .tps
    // profile database.
    if (!m_phase5CampusScenario.empty())
    {
        if (!m_openDatabase)
        {
            appendState(
                winrt::hstring(localize(L"No database open")),
                winrt::hstring(localize(
                    L"Open or create a database to view campus information."
                    )),
                L"Campus information no database state",
                false
                );
            auto pageScroll = ScrollViewer();
            pageScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
            pageScroll.Content(root);
            page.Content(pageScroll);
            return;
        }

        try
        {
            classmngr::engine::CampusRecordService service(*m_openDatabase);
            const auto result = service.list();
            if (!result)
            {
                std::wstring errorMessage = localize(
                    L"The engine could not load campus records:"
                    );
                errorMessage += L" ";
                const auto errorText = winrt::to_hstring(
                    std::string(result.error().message)
                    );
                errorMessage.append(errorText.c_str(), errorText.size());
                appendState(
                    winrt::hstring(localize(
                        L"Campus information unavailable"
                        )),
                    winrt::hstring(errorMessage),
                    L"Campus information engine error state",
                    true
                    );
            }
            else
            {
                m_campusRecords = *result;
                for (const auto& campus : m_campusRecords)
                {
                    m_campusResourceRecords.emplace_back(
                        campusResourceFromEngine(campus)
                        );
                }
            }
        }
        catch (std::exception const& error)
        {
            std::wstring errorMessage = localize(
                L"The engine could not load campus records:"
                );
            errorMessage += L" ";
            const auto errorText = winrt::to_hstring(std::string(error.what()));
            errorMessage.append(errorText.c_str(), errorText.size());
            appendState(
                winrt::hstring(localize(L"Campus information unavailable")),
                winrt::hstring(errorMessage),
                L"Campus information engine error state",
                true
                );
        }
        catch (...)
        {
            appendState(
                winrt::hstring(localize(L"Campus information unavailable")),
                winrt::hstring(localize(
                    L"The engine returned an unexpected campus loading failure."
                    )),
                L"Campus information engine error state",
                true
                );
        }
    }
    else
    {
        const auto result = loadPackagedCampusResources();
        if (!result)
        {
            std::wstring errorMessage = localize(
                L"The campus resource catalog could not be loaded:"
                );
            errorMessage += L" ";
            const auto errorText = winrt::to_hstring(
                std::string(result.error().message)
                );
            errorMessage.append(errorText.c_str(), errorText.size());
            appendState(
                winrt::hstring(localize(L"Campus information unavailable")),
                winrt::hstring(errorMessage),
                L"Campus resource catalog error state",
                true
                );
        }
        else
        {
            m_campusResourceRecords = *result;
            for (const auto& campus : m_campusResourceRecords)
            {
                m_campusRecords.emplace_back(campusRecordFromResource(campus));
            }
        }
    }

    if (m_campusResourceRecords.empty())
    {
        if (m_campusInformationState.empty())
        {
            m_campusInformationState = L"empty";
            appendState(
                winrt::hstring(localize(L"No campuses found")),
                winrt::hstring(localize(
                    L"This catalog does not contain any campus records."
                    )),
                L"Campus information empty state",
                false
                );
        }
        auto pageScroll = ScrollViewer();
        pageScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        pageScroll.Content(root);
        page.Content(pageScroll);
        return;
    }

    m_campusInformationState = L"populated";
    auto selectorRow = Grid();
    selectorRow.ColumnSpacing(8.0);
    selectorRow.ColumnDefinitions().Append(ColumnDefinition());
    selectorRow.ColumnDefinitions().Append(ColumnDefinition());
    selectorRow.ColumnDefinitions().GetAt(1).Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    auto selectorLabel = TextBlock();
    selectorLabel.Text(winrt::hstring(localize(L"Campuses")));
    selectorLabel.VerticalAlignment(VerticalAlignment::Center);
    setAutomationName(selectorLabel, L"Campus directory label");
    selectorRow.Children().Append(selectorLabel);

    m_campusSelector = ComboBox();
    m_campusSelector.MinWidth(190.0);
    m_campusSelector.IsTabStop(true);
    m_campusSelector.TabIndex(0);
    m_campusSelector.SelectionChanged({this, &MainWindow::CampusSelector_SelectionChanged});
    setAutomationName(m_campusSelector, L"Campus directory selector");
    for (const CampusResourceView& campus : m_campusResourceRecords)
    {
        auto item = TextBlock();
        std::wstring displayName = campus.campusName;
        if (!campus.campusCode.empty())
        {
            displayName += L" (";
            displayName += campus.campusCode;
            displayName += L")";
        }
        item.Text(winrt::hstring(displayName));
        item.TextWrapping(TextWrapping::Wrap);
        setAutomationName(item, L"Campus name " + displayName);
        m_campusSelector.Items().Append(item);
    }
    Grid::SetColumn(m_campusSelector, 1);
    selectorRow.Children().Append(m_campusSelector);
    root.Children().Append(selectorRow);

    m_campusTabs = Pivot();
    m_campusTabs.IsTabStop(true);
    setAutomationName(m_campusTabs, L"Campus detail tabs");
    for (std::wstring_view const header : {
             L"Information", L"Directions", L"Address", L"Housing", L"Maps"})
    {
        auto tab = PivotItem();
        tab.Header(winrt::box_value(winrt::hstring(localize(header))));
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        auto panel = StackPanel();
        panel.Spacing(10.0);
        auto container = Border();
        container.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        container.CornerRadius(CornerRadius{6.0, 6.0, 6.0, 6.0});
        container.Padding(Thickness{12.0, 12.0, 12.0, 12.0});
        container.Child(panel);
        scroll.Content(container);
        tab.Content(scroll);
        if (header == L"Information")
        {
            m_campusDetailsPanel = panel;
            setAutomationName(m_campusDetailsPanel, L"Selected campus details");
        }
        m_campusTabs.Items().Append(tab);
    }
    root.Children().Append(m_campusTabs);
    page.Content(root);

    if (m_selectedCampusIndex < 0
        || static_cast<std::size_t>(m_selectedCampusIndex)
            >= m_campusResourceRecords.size())
    {
        m_selectedCampusIndex = 0;
    }
    m_campusSelector.SelectedIndex(m_selectedCampusIndex);
    presentSelectedCampus(pageId);
    updateFileCommandState();
}

void MainWindow::refreshCampusInformationPage()
{
    if (!isCampusPageId(m_currentPageId) || !m_contentFrame)
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populateCampusPage(page, m_currentPageId, true);
    }
    updateFileCommandState();
}

void MainWindow::CampusSelector_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_campusSelector)
    {
        m_selectedCampusIndex = m_campusSelector.SelectedIndex();
    }
    presentSelectedCampus(m_currentPageId);
}

void MainWindow::presentSelectedCampus(std::wstring_view pageId)
{
    if (!m_campusSelector || !m_campusTabs || !m_campusDetailsPanel)
    {
        return;
    }

    const std::uint64_t requestId = ++m_campusImageRequest;
    m_campusImage = nullptr;
    m_campusImages.clear();
    std::array<Microsoft::UI::Xaml::Controls::StackPanel, 5> tabPanels{};
    for (uint32_t index = 0; index < tabPanels.size(); ++index)
    {
        const auto tab = m_campusTabs.Items().GetAt(index).as<
            Microsoft::UI::Xaml::Controls::PivotItem>();
        const auto scroll = tab.Content().as<
            Microsoft::UI::Xaml::Controls::ScrollViewer>();
        const auto container = scroll.Content().as<
            Microsoft::UI::Xaml::Controls::Border>();
        tabPanels[index] = container.Child().as<
            Microsoft::UI::Xaml::Controls::StackPanel>();
        tabPanels[index].Children().Clear();
    }

    const int32_t selectedIndex = m_campusSelector.SelectedIndex();
    if (selectedIndex < 0
        || static_cast<std::size_t>(selectedIndex)
            >= m_campusResourceRecords.size())
    {
        auto status = Microsoft::UI::Xaml::Controls::TextBlock();
        status.Text(winrt::hstring(m_localizer.getString(
            L"CampusInformationPage",
            L"Select a campus to view its details."
            )));
        status.TextWrapping(Microsoft::UI::Xaml::TextWrapping::Wrap);
        setAutomationName(status, L"Campus details selection prompt");
        m_campusDetailsPanel.Children().Append(status);
        return;
    }

    const CampusResourceView& campus =
        m_campusResourceRecords[static_cast<std::size_t>(selectedIndex)];
    const auto localize = [this](std::wstring_view source) {
        return m_localizer.getString(L"CampusInformationPage", source);
    };
    const auto appendText = [this](
                                Microsoft::UI::Xaml::Controls::Panel const& panel,
                                std::wstring_view text,
                                std::wstring_view automationName,
                                double fontSize = 0.0) {
        auto field = Microsoft::UI::Xaml::Controls::TextBlock();
        field.Text(winrt::hstring(text));
        field.TextWrapping(Microsoft::UI::Xaml::TextWrapping::Wrap);
        if (fontSize > 0.0)
        {
            field.FontSize(fontSize);
        }
        setAutomationName(field, automationName);
        panel.Children().Append(field);
    };
    const auto appendHeading = [&appendText](
                                   Microsoft::UI::Xaml::Controls::Panel const& panel,
                                   std::wstring_view text) {
        appendText(panel, text, text, 18.0);
    };
    const auto appendField = [this, &localize](
                                 Microsoft::UI::Xaml::Controls::Panel const& panel,
                                 std::wstring_view label,
                                 std::wstring_view value,
                                 double minimumHeight = 0.0) {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        auto row = Grid();
        row.ColumnSpacing(10.0);
        row.ColumnDefinitions().Append(ColumnDefinition());
        row.ColumnDefinitions().Append(ColumnDefinition());
        row.ColumnDefinitions().GetAt(1).Width(
            GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star));
        auto caption = TextBlock();
        caption.Text(winrt::hstring(localize(label)));
        caption.TextWrapping(TextWrapping::Wrap);
        caption.VerticalAlignment(VerticalAlignment::Top);
        setAutomationName(caption, label);
        auto field = TextBlock();
        field.Text(winrt::hstring(value));
        field.TextWrapping(TextWrapping::Wrap);
        field.MinWidth(280.0);
        if (minimumHeight > 0.0)
        {
            field.MinHeight(minimumHeight);
        }
        setAutomationName(field, L"Campus field " + std::wstring(label));
        Grid::SetColumn(field, 1);
        row.Children().Append(caption);
        row.Children().Append(field);
        panel.Children().Append(row);
    };
    const auto appendImage = [this, requestId](
                                 Microsoft::UI::Xaml::Controls::Panel const& panel,
                                 std::string const& path) {
        if (path.empty())
        {
            return;
        }
        auto image = Microsoft::UI::Xaml::Controls::Image();
        image.MaxWidth(720.0);
        image.MaxHeight(360.0);
        image.Stretch(Microsoft::UI::Xaml::Media::Stretch::Uniform);
        image.HorizontalAlignment(
            Microsoft::UI::Xaml::HorizontalAlignment::Center
            );
        image.Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        setAutomationName(image, L"Campus image preview");
        if (!m_campusImage)
        {
            m_campusImage = image;
        }
        m_campusImages.emplace_back(image);
        panel.Children().Append(image);
        loadCampusImage(path, requestId, image);
    };
    const auto appendAddress = [&appendField, &appendHeading](
                                  Microsoft::UI::Xaml::Controls::Panel const& parent,
                                  std::wstring_view heading,
                                  CampusAddressView const& address
                              ) {
        auto section = Microsoft::UI::Xaml::Controls::StackPanel();
        section.Spacing(8.0);
        appendHeading(section, heading);
        appendField(section, L"Building Name", address.buildingName);
        appendField(section, L"Province", address.province);
        appendField(section, L"City", address.city);
        appendField(section, L"City District", address.cityDistrict);
        appendField(section, L"District", address.district);
        appendField(section, L"Address Line 1", address.line1);
        appendField(section, L"Address Line 2", address.line2);
        appendField(section, L"Postal Code", address.postalCode);
        appendField(section, L"Address System", address.addressSystem);
        parent.Children().Append(section);
    };

    auto information = tabPanels[0];
    appendHeading(information, campus.campusName);
    if (!campus.mapImagePaths.empty())
    {
        appendImage(information, campus.mapImagePaths.front());
    }
    appendField(information, L"Campus ID", campus.id);
    appendField(information, L"Campus Code", campus.campusCode);
    appendField(information, L"Name", campus.campusName);
    appendField(information, L"Building", campus.buildingName);
    appendField(information, L"Address", campus.address);
    appendField(information, L"Phone", campus.phoneNumber);
    appendField(information, L"Office", campus.officeNumber);
    appendField(information, L"Office Wi-Fi", campus.officeWifi);
    appendField(information, L"Office Wi-Fi password", campus.officeWifiPassword);
    appendField(information, L"Printer", campus.printerName);
    appendField(information, L"Printer steps", campus.printerSteps);
    appendField(information, L"Printer driver URL",
        campus.printerDriverUrlUnavailable ? L"N/A" : campus.printerDriverUrl);
    appendField(information, L"Photocopier code", campus.photocopierCode);

    auto directions = tabPanels[1];
    appendHeading(directions, campus.campusName);
    appendField(directions, L"Building", campus.buildingName);
    appendField(directions, L"Phone", campus.phoneNumber);
    std::wstring transitSteps;
    for (std::size_t index = 0; index < campus.transitSteps.size(); ++index)
    {
        if (index != 0)
        {
            transitSteps += L"\n";
        }
        transitSteps += campus.transitSteps[index];
    }
    appendField(directions, L"Transit Steps", transitSteps);
    appendField(directions, L"Upon Arriving", campus.arrivalInfo);
    appendField(directions, L"Note", campus.directionsNote);

    auto address = tabPanels[2];
    appendHeading(address, campus.campusName);
    appendField(address, L"Campus", campus.campusName);
    appendField(address, L"Phone", campus.phoneNumber);
    appendField(address, L"Complete Address", campus.address, 150.0);
    appendAddress(address, L"English", campus.englishAddress);
    appendAddress(address, L"Korean", campus.koreanAddress);

    auto housing = tabPanels[3];
    housing.Spacing(12.0);
    if (campus.housingLocations.empty())
    {
        appendText(housing, localize(L"No housing information available"),
            L"Campus housing empty state");
    }
    for (std::size_t index = 0; index < campus.housingLocations.size(); ++index)
    {
        const CampusHousingView& location = campus.housingLocations[index];
        auto card = Microsoft::UI::Xaml::Controls::Border();
        card.Margin(Microsoft::UI::Xaml::Thickness{12.0, 12.0, 12.0, 12.0});
        card.Padding(Microsoft::UI::Xaml::Thickness{12.0, 12.0, 12.0, 12.0});
        card.BorderThickness(Microsoft::UI::Xaml::Thickness{1.0, 1.0, 1.0, 1.0});
        auto cardContent = Microsoft::UI::Xaml::Controls::StackPanel();
        cardContent.Spacing(10.0);
        appendHeading(cardContent, L"Housing " + std::to_wstring(index + 1)
            + (location.name.empty() ? L"" : L": " + location.name));
        appendAddress(cardContent, L"English", location.englishAddress);
        appendAddress(cardContent, L"Korean", location.koreanAddress);
        appendField(cardContent, L"Note", location.addressNote);
        for (const std::string& imagePath : location.imagePaths)
        {
            appendImage(cardContent, imagePath);
        }
        card.Child(cardContent);
        housing.Children().Append(card);
    }

    auto maps = tabPanels[4];
    maps.Spacing(16.0);
    if (campus.mapImagePaths.empty())
    {
        appendText(maps, localize(L"No map images available"), L"Campus maps empty state");
    }
    for (const std::string& imagePath : campus.mapImagePaths)
    {
        appendImage(maps, imagePath);
        appendText(maps, asWString(winrt::to_hstring(imagePath)),
            L"Campus map resource path");
    }
    appendField(maps, L"Naver Maps", campus.naverMapUrl);
    appendField(maps, L"Kakao Maps", campus.kakaoMapUrl);

    uint32_t selectedTab = 0;
    if (pageId == campusDirectionsPageId) { selectedTab = 1; }
    else if (pageId == campusAddressPageId) { selectedTab = 2; }
    else if (pageId == campusHousingPageId) { selectedTab = 3; }
    else if (pageId == campusMapPageId) { selectedTab = 4; }
    m_campusTabs.SelectedIndex(selectedTab);
}

winrt::fire_and_forget MainWindow::loadCampusImage(
    std::string logicalPath,
    std::uint64_t requestId,
    Microsoft::UI::Xaml::Controls::Image target
    )
{
    auto lifetime = get_strong();
    const auto dispatcher = DispatcherQueue();
    if (logicalPath.empty() || !dispatcher)
    {
        co_return;
    }

    try
    {
        co_await winrt::resume_background();
        classmngr::windows::winui::WindowsResourceProvider resourceProvider;
        const auto bytes = resourceProvider.readBytes(logicalPath);
        if (!bytes || bytes->empty())
        {
            co_return;
        }

        std::vector<std::uint8_t> payload(bytes->size());
        for (std::size_t index = 0; index < bytes->size(); ++index)
        {
            payload[index] = std::to_integer<std::uint8_t>((*bytes)[index]);
        }

        co_await ResumeOnDispatcherQueue{
            dispatcher,
            Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal
            };
        if (requestId != m_campusImageRequest || !target)
        {
            co_return;
        }

        auto stream = winrt::Windows::Storage::Streams::InMemoryRandomAccessStream();
        auto writer = winrt::Windows::Storage::Streams::DataWriter(stream);
        writer.WriteBytes(winrt::array_view<std::uint8_t const>(
            payload.data(),
            payload.data() + payload.size()
            ));
        co_await writer.StoreAsync();
        co_await writer.FlushAsync();
        writer.DetachStream();
        stream.Seek(0);

        auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
        co_await bitmap.SetSourceAsync(stream);
        if (requestId != m_campusImageRequest || !target)
        {
            co_return;
        }
        target.Source(bitmap);
        target.Visibility(Microsoft::UI::Xaml::Visibility::Visible);
    }
    catch (...)
    {
        // A missing or undecodable optional image must not replace the
        // engine-backed campus details. The source path remains visible.
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
