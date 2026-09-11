#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populateSubPrepPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    static_cast<void>(refresh);
    page.NavigationCacheMode(
        Microsoft::UI::Xaml::Navigation::NavigationCacheMode::Disabled
        );

    if (m_subPrepScroll && page)
    {
        if (m_subPrepPageHost && m_subPrepPageHost != page)
        {
            // The disabled-cache page is recreated on each visit. Detach the
            // retained view from its previous host before reattaching it.
            m_subPrepPageHost.Content(nullptr);
        }

        const auto currentContent = page.Content().try_as<ScrollViewer>();
        if (currentContent != m_subPrepScroll)
        {
            page.Content(m_subPrepScroll);
        }
        m_subPrepPageHost = page;
    }

    if (!m_subPrepScroll)
    {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        m_subPrepScroll = scroll;

        auto root = StackPanel();
        root.Padding(Thickness{32.0, 24.0, 32.0, 32.0});
        root.Spacing(16.0);
        root.MaxWidth(980.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"Sub Prep");
        title.FontSize(24.0);
        setAutomationName(title, L"Sub Prep");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"Prepare substitute materials, schedules, and class notes for a substitute teacher."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(description, L"Sub Prep description");
        root.Children().Append(description);

        m_subPrepTabs = Pivot();
        m_subPrepTabs.IsTabStop(true);
        m_subPrepTabs.TabIndex(0);
        applyResourceStyle(m_subPrepTabs, L"Phase3TopTabPivotStyle");
        setAutomationName(m_subPrepTabs, L"Sub Prep sections");
        root.Children().Append(m_subPrepTabs);

        m_subPrepStatusText = TextBlock();
        m_subPrepStatusText.Text(L"Loading substitute-preparation information...");
        m_subPrepStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(m_subPrepStatusText, L"Sub Prep status");
        root.Children().Append(m_subPrepStatusText);

        m_subPrepValidationText = TextBlock();
        m_subPrepValidationText.TextWrapping(TextWrapping::Wrap);
        m_subPrepValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(
            m_subPrepValidationText,
            L"Sub Prep validation summary"
            );
        root.Children().Append(m_subPrepValidationText);

        const auto appendReadOnlyValue = [](
            ClassMngrWinUISharedUX::Card& card,
            wchar_t const* label,
            wchar_t const* automationName
            ) {
            auto heading = TextBlock();
            heading.Text(label);
            heading.TextWrapping(TextWrapping::Wrap);
            setAutomationName(heading, automationName);
            card.content.Children().Append(heading);

            auto value = TextBlock();
            value.Text(L"N/A");
            value.TextWrapping(TextWrapping::Wrap);
            value.Margin(Thickness{0.0, 0.0, 0.0, 6.0});
            setAutomationName(value, std::wstring(automationName) + L" value");
            card.content.Children().Append(value);
            return value;
        };

        auto importantRoot = StackPanel();
        importantRoot.Spacing(12.0);

        auto campusCard = ClassMngrWinUISharedUX::buildCard({
            L"Campus Information",
            L"Read-only campus and office details from the packaged campus directory.",
            L"Sub Prep campus information"
            });
        m_subPrepCampusText = appendReadOnlyValue(
            campusCard,
            L"Campus",
            L"Sub Prep campus"
            );
        m_subPrepOfficeText = appendReadOnlyValue(
            campusCard,
            L"Office",
            L"Sub Prep office"
            );
        m_subPrepWifiText = appendReadOnlyValue(
            campusCard,
            L"Office Wi-Fi",
            L"Sub Prep office Wi-Fi"
            );
        m_subPrepWifiPasswordText = appendReadOnlyValue(
            campusCard,
            L"Office Wi-Fi password",
            L"Sub Prep office Wi-Fi password"
            );
        m_subPrepPhotocopierText = appendReadOnlyValue(
            campusCard,
            L"Photocopier code",
            L"Sub Prep photocopier code"
            );
        importantRoot.Children().Append(campusCard.root);

        auto zoomCard = ClassMngrWinUISharedUX::buildCard({
            L"Personal Zoom Information",
            L"Read-only Zoom information from My Details.",
            L"Sub Prep Zoom information"
            });
        m_subPrepZoomLoginText = appendReadOnlyValue(
            zoomCard,
            L"Zoom login ID",
            L"Sub Prep Zoom login ID"
            );
        m_subPrepZoomPasswordText = appendReadOnlyValue(
            zoomCard,
            L"Zoom password",
            L"Sub Prep Zoom password"
            );
        importantRoot.Children().Append(zoomCard.root);

        auto notesCard = ClassMngrWinUISharedUX::buildCard({
            L"Class Materials and Lesson Notes",
            L"These notes are saved with the active database and included in the renderer-neutral document model.",
            L"Sub Prep editable notes"
            });
        const auto makeNotesBox = [this](
            wchar_t const* header,
            wchar_t const* automationName
            ) {
            auto box = TextBox();
            box.Header(box_value(hstring(header)));
            box.MinHeight(92.0);
            box.MaxLength(10000);
            box.AcceptsReturn(true);
            box.TextWrapping(TextWrapping::Wrap);
            box.VerticalContentAlignment(VerticalAlignment::Top);
            box.HorizontalAlignment(HorizontalAlignment::Stretch);
            box.IsTabStop(true);
            box.TextChanging(
                [this](
                    TextBox const& sender,
                    TextBoxTextChangingEventArgs const& arguments
                    ) {
                    static_cast<void>(sender);
                    static_cast<void>(arguments);
                    markSubPrepDirty();
                }
                );
            setAutomationName(box, automationName);
            return box;
        };
        m_subPrepClassMaterialsTextBox = makeNotesBox(
            L"Class materials",
            L"Sub Prep class materials"
            );
        m_subPrepGradingTextBox = makeNotesBox(
            L"Book report grading",
            L"Sub Prep book report grading"
            );
        m_subPrepSpecialInstructionsTextBox = makeNotesBox(
            L"Book report special instructions",
            L"Sub Prep book report special instructions"
            );
        m_subPrepNotesTextBox = makeNotesBox(
            L"Detailed class and lesson notes",
            L"Sub Prep detailed class and lesson notes"
            );
        notesCard.content.Children().Append(m_subPrepClassMaterialsTextBox);
        notesCard.content.Children().Append(m_subPrepGradingTextBox);
        notesCard.content.Children().Append(m_subPrepSpecialInstructionsTextBox);
        notesCard.content.Children().Append(m_subPrepNotesTextBox);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_subPrepSaveButton = Button();
        m_subPrepSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_subPrepSaveButton.IsTabStop(true);
        m_subPrepSaveButton.TabIndex(5);
        m_subPrepSaveButton.Click(
            [this](
                Windows::Foundation::IInspectable const& sender,
                RoutedEventArgs const& arguments
                ) {
                static_cast<void>(sender);
                static_cast<void>(arguments);
                saveSubPrepPage();
            }
            );
        setAutomationName(m_subPrepSaveButton, L"Save Sub Prep changes");

        m_subPrepDiscardButton = Button();
        m_subPrepDiscardButton.Content(
            box_value(hstring(L"Discard Changes"))
            );
        m_subPrepDiscardButton.IsTabStop(true);
        m_subPrepDiscardButton.TabIndex(6);
        m_subPrepDiscardButton.Click(
            [this](
                Windows::Foundation::IInspectable const& sender,
                RoutedEventArgs const& arguments
                ) {
                static_cast<void>(sender);
                static_cast<void>(arguments);
                discardSubPrepPage();
            }
            );
        setAutomationName(
            m_subPrepDiscardButton,
            L"Discard Sub Prep changes"
            );
        actions.Children().Append(m_subPrepSaveButton);
        actions.Children().Append(m_subPrepDiscardButton);
        notesCard.content.Children().Append(actions);
        importantRoot.Children().Append(notesCard.root);

        auto scheduleRoot = StackPanel();
        scheduleRoot.Spacing(12.0);
        m_subPrepScheduleSummaryText = TextBlock();
        m_subPrepScheduleSummaryText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_subPrepScheduleSummaryText,
            L"Sub Prep schedule summary"
            );
        scheduleRoot.Children().Append(m_subPrepScheduleSummaryText);

        m_subPrepScheduleList = ListView();
        m_subPrepScheduleList.Height(360.0);
        m_subPrepScheduleList.IsTabStop(true);
        m_subPrepScheduleList.SelectionMode(ListViewSelectionMode::None);
        setAutomationName(m_subPrepScheduleList, L"Sub Prep schedule list");
        scheduleRoot.Children().Append(m_subPrepScheduleList);

        m_subPrepDocumentSummaryText = TextBlock();
        m_subPrepDocumentSummaryText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
        m_subPrepDocumentSummaryText,
            L"Sub Prep document model summary"
            );
        scheduleRoot.Children().Append(m_subPrepDocumentSummaryText);

        auto packageCard = ClassMngrWinUISharedUX::buildCard({
            L"Bundled Sub Prep Package",
            L"Choose dates and classes to preview the deterministic document paths that the Phase 7 output adapters will produce.",
            L"Sub Prep bundled package planner"
            });
        m_subPrepPackageUserNameTextBox = TextBox();
        m_subPrepPackageUserNameTextBox.Header(
            box_value(hstring(L"Substitute teacher name"))
            );
        m_subPrepPackageUserNameTextBox.PlaceholderText(
            L"Name used in the package folder"
            );
        m_subPrepPackageUserNameTextBox.MinWidth(320.0);
        m_subPrepPackageUserNameTextBox.IsTabStop(true);
        setAutomationName(
            m_subPrepPackageUserNameTextBox,
            L"Sub Prep package substitute teacher name"
            );

        m_subPrepPackageDatesTextBox = TextBox();
        m_subPrepPackageDatesTextBox.Header(
            box_value(hstring(L"Selected dates"))
            );
        m_subPrepPackageDatesTextBox.PlaceholderText(
            L"yyyy-MM-dd, yyyy-MM-dd"
            );
        m_subPrepPackageDatesTextBox.MinWidth(320.0);
        m_subPrepPackageDatesTextBox.IsTabStop(true);
        setAutomationName(
            m_subPrepPackageDatesTextBox,
            L"Sub Prep package selected dates"
            );

        m_subPrepPackageRosterTemplateCombo = ComboBox();
        m_subPrepPackageRosterTemplateCombo.Header(
            box_value(hstring(L"Roster document"))
            );
        m_subPrepPackageRosterTemplateCombo.IsTabStop(true);
        m_subPrepPackageRosterTemplateCombo.MinWidth(320.0);
        for (const wchar_t* label : {
                 L"By day",
                 L"Daily",
                 L"One roster per class with extra information"
             })
        {
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(label)));
            m_subPrepPackageRosterTemplateCombo.Items().Append(item);
        }
        m_subPrepPackageRosterTemplateCombo.SelectedIndex(0);
        setAutomationName(
            m_subPrepPackageRosterTemplateCombo,
            L"Sub Prep package roster template"
            );

        auto classesLabel = TextBlock();
        classesLabel.Text(L"Classes included in the package");
        classesLabel.TextWrapping(TextWrapping::Wrap);
        setAutomationName(classesLabel, L"Sub Prep package classes label");

        m_subPrepPackageClassesList = ListView();
        m_subPrepPackageClassesList.Height(200.0);
        m_subPrepPackageClassesList.IsTabStop(true);
        m_subPrepPackageClassesList.SelectionMode(
            ListViewSelectionMode::None
            );
        setAutomationName(
            m_subPrepPackageClassesList,
            L"Sub Prep package class selection"
            );

        m_subPrepPackagePlanButton = Button();
        m_subPrepPackagePlanButton.Content(
            box_value(hstring(L"Plan Bundled Package"))
            );
        m_subPrepPackagePlanButton.IsTabStop(true);
        m_subPrepPackagePlanButton.Click(
            [this](
                Windows::Foundation::IInspectable const& sender,
                RoutedEventArgs const& arguments
                ) {
                static_cast<void>(sender);
                static_cast<void>(arguments);
                planSubPrepPackage();
            }
            );
        setAutomationName(
            m_subPrepPackagePlanButton,
            L"Plan Sub Prep bundled package"
            );

        m_subPrepPackageStatusText = TextBlock();
        m_subPrepPackageStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_subPrepPackageStatusText,
            L"Sub Prep bundled package status"
            );

        auto packagePathsLabel = TextBlock();
        packagePathsLabel.Text(L"Planned relative document paths");
        packagePathsLabel.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            packagePathsLabel,
            L"Sub Prep package paths label"
            );

        m_subPrepPackagePathsList = ListView();
        m_subPrepPackagePathsList.Height(140.0);
        m_subPrepPackagePathsList.IsTabStop(true);
        m_subPrepPackagePathsList.SelectionMode(ListViewSelectionMode::None);
        setAutomationName(
            m_subPrepPackagePathsList,
            L"Sub Prep bundled package paths"
            );

        packageCard.content.Children().Append(m_subPrepPackageUserNameTextBox);
        packageCard.content.Children().Append(m_subPrepPackageDatesTextBox);
        packageCard.content.Children().Append(m_subPrepPackageRosterTemplateCombo);
        packageCard.content.Children().Append(classesLabel);
        packageCard.content.Children().Append(m_subPrepPackageClassesList);
        packageCard.content.Children().Append(m_subPrepPackagePlanButton);
        packageCard.content.Children().Append(m_subPrepPackageStatusText);
        packageCard.content.Children().Append(packagePathsLabel);
        packageCard.content.Children().Append(m_subPrepPackagePathsList);
        scheduleRoot.Children().Append(packageCard.root);

        auto classInformationRoot = StackPanel();
        classInformationRoot.Spacing(12.0);
        auto classInformationIntro = TextBlock();
        classInformationIntro.Text(
            L"Classes are grouped by teacher using the same stable ordering as the Qt workflow."
            );
        classInformationIntro.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            classInformationIntro,
            L"Sub Prep class information description"
            );
        classInformationRoot.Children().Append(classInformationIntro);

        m_subPrepClassInformationList = ListView();
        m_subPrepClassInformationList.Height(480.0);
        m_subPrepClassInformationList.IsTabStop(true);
        m_subPrepClassInformationList.SelectionMode(
            ListViewSelectionMode::None
            );
        setAutomationName(
            m_subPrepClassInformationList,
            L"Sub Prep class information list"
            );
        classInformationRoot.Children().Append(m_subPrepClassInformationList);

        auto importantTab = PivotItem();
        importantTab.Header(ClassMngrWinUISharedUX::buildTopTabHeader(
            hstring(L"Important Information")
            ));
        importantTab.Content(importantRoot);
        setAutomationName(importantTab, L"Sub Prep Important Information tab");
        m_subPrepTabs.Items().Append(importantTab);

        auto scheduleTab = PivotItem();
        scheduleTab.Header(ClassMngrWinUISharedUX::buildTopTabHeader(
            hstring(L"Schedule")
            ));
        scheduleTab.Content(scheduleRoot);
        setAutomationName(scheduleTab, L"Sub Prep Schedule tab");
        m_subPrepTabs.Items().Append(scheduleTab);

        auto classInformationTab = PivotItem();
        classInformationTab.Header(ClassMngrWinUISharedUX::buildTopTabHeader(
            hstring(L"Class Information")
            ));
        classInformationTab.Content(classInformationRoot);
        setAutomationName(
            classInformationTab,
            L"Sub Prep Class Information tab"
            );
        m_subPrepTabs.Items().Append(classInformationTab);

        scroll.Content(root);
        page.Content(m_subPrepScroll);
        m_subPrepPageHost = page;
    }

    refreshSubPrepPage();
}

void MainWindow::refreshSubPrepPage()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_subPrepStatusText || !m_subPrepClassMaterialsTextBox)
    {
        return;
    }

    const bool preserveDraft = m_subPrepDirty;
    m_subPrepLoading = true;
    const auto clearPresentation = [this]() {
        m_subPrepDocument = {};
        m_subPrepClasses.clear();
        m_subPrepSourceClasses.clear();
        m_subPrepClassInformation.clear();
        m_subPrepCampusText.Text(L"Campus: N/A");
        m_subPrepOfficeText.Text(L"Office: N/A");
        m_subPrepWifiText.Text(L"Office Wi-Fi: N/A");
        m_subPrepWifiPasswordText.Text(L"Office Wi-Fi password: N/A");
        m_subPrepPhotocopierText.Text(L"Photocopier code: N/A");
        m_subPrepZoomLoginText.Text(L"Zoom login ID: N/A");
        m_subPrepZoomPasswordText.Text(L"Zoom password: N/A");
        m_subPrepClassMaterialsTextBox.Text({});
        m_subPrepGradingTextBox.Text({});
        m_subPrepSpecialInstructionsTextBox.Text({});
        m_subPrepNotesTextBox.Text({});
        m_subPrepScheduleSummaryText.Text({});
        m_subPrepDocumentSummaryText.Text({});
        m_subPrepScheduleList.Items().Clear();
        m_subPrepClassInformationList.Items().Clear();
        m_subPrepPackagePlan = {};
        m_subPrepPackageUserNameTextBox.Text({});
        m_subPrepPackageDatesTextBox.Text({});
        m_subPrepPackageClassesList.Items().Clear();
        m_subPrepPackageClassChecks.clear();
        m_subPrepPackageStatusText.Text({});
        m_subPrepPackagePathsList.Items().Clear();
    };
    const auto showFailure = [this](
        std::wstring status,
        std::wstring validation
        ) {
        m_subPrepStatusText.Text(winrt::hstring(status));
        m_subPrepValidationText.Text(winrt::hstring(validation));
        m_subPrepValidationText.Visibility(Visibility::Visible);
        m_subPrepLoading = false;
        updateSubPrepActions();
    };

    if (!m_openDatabase)
    {
        clearPresentation();
        m_subPrepDirty = false;
        m_subPrepStatusText.Text(L"No database open.");
        m_subPrepValidationText.Text(
            L"Open a .tps or .db file to view substitute-preparation information."
            );
        m_subPrepValidationText.Visibility(Visibility::Collapsed);
        m_subPrepLoading = false;
        updateSubPrepActions();
        return;
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    classmngr::engine::PersonalDetailsService personalService(settings);
    const auto personal = personalService.load();
    if (!personal)
    {
        showFailure(
            L"Sub Prep could not load personal details.",
            L"Personal details error: " + asWide(personal.error().message)
            );
        return;
    }

    const auto classMaterials = subPrepTextSetting(
        settings,
        "subPrep/classMaterials",
        {}
        );
    const auto gradingInstructions = subPrepTextSetting(
        settings,
        "subPrep/bookReportGrading",
        "Scoring: 0 / 20 / 40 / 60 / 80 / 100\nComments: Please leave a comment about what the student did well and what they need to work on."
        );
    const auto specialInstructions = subPrepTextSetting(
        settings,
        "subPrep/bookReportSpecialInstructions",
        "N/A"
        );
    const auto subNotes = subPrepTextSetting(
        settings,
        "subPrep/subComments",
        {}
        );
    if (!classMaterials || !gradingInstructions || !specialInstructions || !subNotes)
    {
        const auto* error = !classMaterials
            ? &classMaterials.error()
            : !gradingInstructions
                ? &gradingInstructions.error()
                : !specialInstructions
                    ? &specialInstructions.error()
                    : &subNotes.error();
        showFailure(
            L"Sub Prep settings could not be loaded.",
            L"Sub Prep settings error: " + asWide(error->message)
            );
        return;
    }
    if (!preserveDraft)
    {
        m_subPrepClassMaterialsTextBox.Text(asWide(*classMaterials));
        m_subPrepGradingTextBox.Text(asWide(*gradingInstructions));
        m_subPrepSpecialInstructionsTextBox.Text(asWide(*specialInstructions));
        m_subPrepNotesTextBox.Text(asWide(*subNotes));
    }

    const auto displayValue = [](std::wstring value) {
        return value.empty() ? std::wstring(L"N/A") : std::move(value);
    };
    const auto setLabeledText = [&displayValue](
        TextBlock const& target,
        wchar_t const* label,
        std::wstring value
        ) {
        std::wstring text(label);
        text += L": ";
        text += displayValue(std::move(value));
        target.Text(winrt::hstring(text));
    };

    CampusResourceView selectedCampus;
    std::wstring resourceWarning;
    const auto campusResources = loadPackagedCampusResources();
    if (!campusResources)
    {
        resourceWarning = L"Campus directory warning: ";
        resourceWarning += asWide(campusResources.error().message);
    }
    else
    {
        const auto normalized = [](std::wstring value) {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](wchar_t character) { return std::towlower(character); }
                );
            return value;
        };
        const std::wstring wantedCampus = normalized(asWide(personal->campus));
        for (const CampusResourceView& candidate : *campusResources)
        {
            if ((!wantedCampus.empty()
                 && (normalized(candidate.id) == wantedCampus
                     || normalized(candidate.campusName) == wantedCampus))
                || (wantedCampus.empty() && selectedCampus.campusName.empty()))
            {
                selectedCampus = candidate;
                break;
            }
        }
        if (selectedCampus.campusName.empty() && !campusResources->empty())
        {
            selectedCampus = campusResources->front();
        }
        if (selectedCampus.campusName.empty() && !wantedCampus.empty())
        {
            resourceWarning = L"No packaged campus directory entry matched the selected campus.";
        }
    }
    setLabeledText(
        m_subPrepCampusText,
        L"Campus",
        selectedCampus.campusName.empty()
            ? asWide(personal->campus)
            : selectedCampus.campusName
        );
    setLabeledText(
        m_subPrepOfficeText,
        L"Office",
        selectedCampus.officeNumber
        );
    setLabeledText(
        m_subPrepWifiText,
        L"Office Wi-Fi",
        selectedCampus.officeWifi
        );
    setLabeledText(
        m_subPrepWifiPasswordText,
        L"Office Wi-Fi password",
        selectedCampus.officeWifiPassword
        );
    setLabeledText(
        m_subPrepPhotocopierText,
        L"Photocopier code",
        selectedCampus.photocopierCode
        );
    setLabeledText(
        m_subPrepZoomLoginText,
        L"Zoom login ID",
        asWide(personal->zoomLoginId)
        );
    setLabeledText(
        m_subPrepZoomPasswordText,
        L"Zoom password",
        asWide(personal->zoomPassword)
        );

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto classrooms = repository.list();
    if (!classrooms)
    {
        clearPresentation();
        showFailure(
            L"Sub Prep classes could not be loaded.",
            L"Class list error: " + asWide(classrooms.error().message)
            );
        return;
    }

    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    classmngr::engine::RosterService rosterService(*m_openDatabase);
    classmngr::engine::TeacherService teacherService(*m_openDatabase);
    std::vector<classmngr::engine::ClassInfo> classInfos;
    classInfos.reserve(classrooms->size());
    m_subPrepClasses = *classrooms;
    m_subPrepSourceClasses.clear();
    m_subPrepSourceClasses.reserve(classrooms->size());
    for (const classmngr::engine::Classroom& classroom : *classrooms)
    {
        const auto info = infoService.load(classroom.id);
        if (!info)
        {
            clearPresentation();
            showFailure(
                L"Sub Prep class information could not be loaded.",
                L"Class information error: " + asWide(info.error().message)
                );
            return;
        }
        const auto studentCount = rosterService.studentCount(classroom.id);
        if (!studentCount)
        {
            clearPresentation();
            showFailure(
                L"Sub Prep rosters could not be loaded.",
                L"Roster error: " + asWide(studentCount.error().message)
                );
            return;
        }

        classmngr::engine::SubPrepSourceClass source;
        source.classroom = classroom;
        source.info = *info;
        source.studentCount = *studentCount;
        if (info->teacherId > 0)
        {
            const auto teacher = teacherService.get(info->teacherId);
            if (!teacher)
            {
                clearPresentation();
                showFailure(
                    L"Sub Prep teacher information could not be loaded.",
                    L"Teacher error: " + asWide(teacher.error().message)
                    );
                return;
            }
            source.teacher = *teacher;
        }
        classInfos.push_back(*info);
        m_subPrepSourceClasses.push_back(std::move(source));
    }

    m_subPrepPackagePlan = {};
    m_subPrepPackageClassesList.Items().Clear();
    m_subPrepPackageClassChecks.clear();
    if (m_subPrepPackageUserNameTextBox.Text().empty())
    {
        m_subPrepPackageUserNameTextBox.Text(asWide(personal->name));
    }
    if (m_subPrepPackageDatesTextBox.Text().empty())
    {
        m_subPrepPackageDatesTextBox.Text(
            calendarDateText(calendarToday())
            );
    }
    for (const auto& source : m_subPrepSourceClasses)
    {
        std::wstring className = asWide(source.classroom.name);
        if (className.empty())
        {
            className = L"Class " + std::to_wstring(source.classroom.id);
        }
        auto check = CheckBox();
        check.Content(box_value(hstring(className)));
        check.Tag(box_value(source.classroom.id));
        check.IsChecked(true);
        check.IsTabStop(true);
        setAutomationName(
            check,
            L"Include " + className + L" in Sub Prep package"
            );
        m_subPrepPackageClassesList.Items().Append(check);
        m_subPrepPackageClassChecks.push_back(check);
    }
    m_subPrepPackageStatusText.Text(
        L"Select a date and classes, then plan the bundled package."
        );
    m_subPrepPackagePathsList.Items().Clear();

    const auto visibleDays = classmngr::engine::ScheduleReportService::visibleDays(
        false
        );
    const auto scheduleBuild = classmngr::engine::ScheduleBuilderService::build(
        classInfos,
        false,
        visibleDays
        );
    classmngr::engine::ScheduleReportRequest scheduleRequest;
    scheduleRequest.days = visibleDays;
    scheduleRequest.displayMode =
        classmngr::engine::ScheduleReportDisplayMode::Regular;
    const auto schedule = classmngr::engine::ScheduleReportService::build(
        scheduleBuild,
        scheduleRequest
        );

    classmngr::engine::SubPrepBuildOptions classInformationOptions;
    classInformationOptions.visibleDays = visibleDays;
    classInformationOptions.visibleClassIds.reserve(
        m_subPrepSourceClasses.size()
        );
    for (const auto& source : m_subPrepSourceClasses)
    {
        classInformationOptions.visibleClassIds.push_back(source.classroom.id);
    }
    m_subPrepClassInformation =
        classmngr::engine::SubPrepClassInformationService::build(
            m_subPrepSourceClasses,
            classInformationOptions
            );

    classmngr::engine::SubPrepDocumentRequest documentRequest;
    documentRequest.campus.officeNumber = asUtf8(
        std::wstring_view(selectedCampus.officeNumber)
        );
    documentRequest.campus.officeWifi = asUtf8(
        std::wstring_view(selectedCampus.officeWifi)
        );
    documentRequest.campus.officeWifiPassword = asUtf8(
        std::wstring_view(selectedCampus.officeWifiPassword)
        );
    documentRequest.campus.photocopierCode = asUtf8(
        std::wstring_view(selectedCampus.photocopierCode)
        );
    documentRequest.zoom.loginId = personal->zoomLoginId;
    documentRequest.zoom.password = personal->zoomPassword;
    documentRequest.classMaterials = asUtf8(asWString(
        m_subPrepClassMaterialsTextBox.Text()
        ));
    documentRequest.gradingInstructions = asUtf8(asWString(
        m_subPrepGradingTextBox.Text()
        ));
    documentRequest.specialInstructions = asUtf8(asWString(
        m_subPrepSpecialInstructionsTextBox.Text()
        ));
    documentRequest.schedule = schedule;
    documentRequest.classInformation = m_subPrepClassInformation;
    documentRequest.subNotes = asUtf8(asWString(m_subPrepNotesTextBox.Text()));
    m_subPrepDocument = classmngr::engine::SubPrepDocumentService::build(
        documentRequest
        );

    const auto appendListText = [](ListView const& list,
                                   std::wstring text,
                                   std::wstring automationName) {
        auto row = TextBlock();
        row.Text(winrt::hstring(text));
        row.TextWrapping(TextWrapping::Wrap);
        auto item = ListViewItem();
        item.Content(row);
        item.IsTabStop(false);
        setAutomationName(item, automationName);
        list.Items().Append(item);
    };
    m_subPrepScheduleList.Items().Clear();
    std::size_t scheduleRows = 0;
    for (const auto& source : m_subPrepSourceClasses)
    {
        std::wstring className = asWide(source.classroom.name);
        if (className.empty())
        {
            className = L"Class " + std::to_wstring(source.classroom.id);
        }
        std::wstring teacherName = asWide(
            source.teacher.preferredDisplayName()
            );
        if (teacherName.empty())
        {
            teacherName = L"N/A";
        }
        for (const classmngr::engine::ClassTime& time : source.info.classTimes)
        {
            if (std::find(visibleDays.begin(), visibleDays.end(), time.day)
                == visibleDays.end())
            {
                continue;
            }
            std::wstring row = className;
            row += L" â€” ";
            row += asWide(time.day);
            row += L" ";
            row += asWide(time.startTime);
            row += L" - ";
            row += asWide(time.endTime);
            row += L" â€” ";
            row += teacherName;
            row += L" â€” Room ";
            row += displayValue(asWide(source.info.roomNumber));
            appendListText(
                m_subPrepScheduleList,
                std::move(row),
                L"Sub Prep schedule row"
                );
            ++scheduleRows;
        }
    }
    if (scheduleRows == 0)
    {
        appendListText(
            m_subPrepScheduleList,
            L"No regular class times are scheduled for Monday through Friday.",
            L"Sub Prep empty schedule"
            );
    }
    m_subPrepScheduleSummaryText.Text(winrt::hstring(
        L"Schedule model ready: "
            + std::to_wstring(m_subPrepDocument.schedule.summary.scheduledBlocks)
            + L" scheduled blocks across "
            + std::to_wstring(visibleDays.size())
            + L" weekdays."
        ));
    m_subPrepDocumentSummaryText.Text(
        L"Renderer-neutral document model ready for the Phase 7 output adapters."
        );

    m_subPrepClassInformationList.Items().Clear();
    if (m_subPrepClassInformation.empty())
    {
        appendListText(
            m_subPrepClassInformationList,
            L"No teacher-grouped class information is available.",
            L"Sub Prep empty class information"
            );
    }
    else
    {
        for (const auto& group : m_subPrepClassInformation)
        {
            std::wstring row = asWide(group.displayName);
            row += L" â€” ";
            row += asWide(group.classListText);
            for (const auto& details : group.classes)
            {
                row += L"\n  ";
                row += asWide(details.classLabel);
                row += L" | ";
                row += asWide(details.timeText);
                row += L" | ";
                row += std::to_wstring(details.studentCount);
                row += L" students";
                if (!details.info.notes.empty())
                {
                    row += L"\n  Notes: ";
                    row += asWide(details.info.notes);
                }
            }
            appendListText(
                m_subPrepClassInformationList,
                std::move(row),
                L"Sub Prep teacher class information"
                );
        }
    }

    m_subPrepStatusText.Text(
        preserveDraft
            ? L"Sub Prep information loaded; unsaved text remains in the editor."
            : L"Sub Prep information loaded."
        );
    if (resourceWarning.empty())
    {
        m_subPrepValidationText.Text({});
        m_subPrepValidationText.Visibility(Visibility::Collapsed);
    }
    else
    {
        m_subPrepValidationText.Text(winrt::hstring(resourceWarning));
        m_subPrepValidationText.Visibility(Visibility::Visible);
    }
    m_subPrepLoading = false;
    updateSubPrepActions();
}

} // namespace winrt::ClassMngrWinUI::implementation
