#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

Microsoft::UI::Xaml::Controls::StackPanel
MainWindow::buildClassAnalyticsSection()
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

    auto analyticsRoot = makeRoot(StackPanel());
    auto analyticsTopBar = Grid();
    analyticsTopBar.ColumnSpacing(16.0);
    auto analyticsTitleColumn = ColumnDefinition();
    analyticsTitleColumn.Width(GridLengthHelper::FromValueAndType(
        1.0,
        GridUnitType::Star
        ));
    analyticsTopBar.ColumnDefinitions().Append(analyticsTitleColumn);
    analyticsTopBar.ColumnDefinitions().Append(ColumnDefinition());

    m_speakingAnalyticsStatusText = TextBlock();
    m_speakingAnalyticsStatusText.Text(
        L"Select a class to view speaking analytics."
        );
    m_speakingAnalyticsStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(
        m_speakingAnalyticsStatusText,
        L"Speaking analytics status"
        );
    m_speakingAnalyticsLoading = true;
    m_speakingAnalyticsName = "All";
    m_speakingAnalyticsSelector = ComboBox();
    m_speakingAnalyticsSelector.MinWidth(220.0);
    m_speakingAnalyticsSelector.IsTabStop(true);
    m_speakingAnalyticsSelector.TabIndex(0);
    setAutomationName(
        m_speakingAnalyticsSelector,
        L"Speaking analytics evaluation selector"
        );
    for (const std::string_view evaluationName : {
             std::string_view{"All"},
             classmngr::engine::SpeakingEvaluationNames[0],
             classmngr::engine::SpeakingEvaluationNames[1],
             classmngr::engine::SpeakingEvaluationNames[2],
             classmngr::engine::SpeakingEvaluationNames[3]
         })
    {
        auto item = ComboBoxItem();
        const std::wstring display = asWide(evaluationName);
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(display)));
        setAutomationName(item, L"Analytics scope " + display);
        m_speakingAnalyticsSelector.Items().Append(item);
    }
    m_speakingAnalyticsSelector.SelectedIndex(0);
    m_speakingAnalyticsSelector.SelectionChanged(
        [this](auto const&, auto const&) {
            if (m_speakingAnalyticsLoading)
            {
                return;
            }
            const auto item = m_speakingAnalyticsSelector.SelectedItem()
                .try_as<ComboBoxItem>();
            if (!item)
            {
                return;
            }
            m_speakingAnalyticsName = asUtf8(
                boxedString(item.Tag())
                );
            refreshSpeakingAnalytics();
        }
        );
    m_speakingAnalyticsLoading = false;
    auto evaluationControls = StackPanel();
    evaluationControls.Orientation(Orientation::Horizontal);
    evaluationControls.Spacing(8.0);
    evaluationControls.VerticalAlignment(VerticalAlignment::Center);
    auto evaluationLabel = TextBlock();
    evaluationLabel.Text(L"Evaluation");
    evaluationLabel.VerticalAlignment(VerticalAlignment::Center);
    applyResourceStyle(evaluationLabel, L"Phase3BodyTextBlockStyle");
    evaluationControls.Children().Append(evaluationLabel);
    evaluationControls.Children().Append(m_speakingAnalyticsSelector);
    Grid::SetColumn(evaluationControls, 1);
    analyticsTopBar.Children().Append(evaluationControls);
    analyticsRoot.Children().Append(analyticsTopBar);
    analyticsRoot.Children().Append(m_speakingAnalyticsStatusText);

    auto analyticsSummaryGrid = Grid();
    analyticsSummaryGrid.ColumnSpacing(12.0);
    const std::array<wchar_t const*, 4> summaryTitles{
        L"Class Average", L"Students Fully Scored", L"Strongest Area", L"Focus Area"
    };
    for (int column = 0; column < static_cast<int>(summaryTitles.size()); ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            1.0,
            GridUnitType::Star
            ));
        analyticsSummaryGrid.ColumnDefinitions().Append(definition);
        auto card = ClassMngrWinUISharedUX::buildCard({
            summaryTitles[static_cast<std::size_t>(column)],
            L"",
            hstring(L"Speaking analytics ")
                + hstring(summaryTitles[static_cast<std::size_t>(column)])
            });
        auto value = TextBlock();
        value.Text(L"â€”");
        value.FontSize(20.0);
        value.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        value.TextWrapping(TextWrapping::Wrap);
        m_speakingAnalyticsSummaryValues[static_cast<std::size_t>(column)] = value;
        card.content.Children().Append(value);
        Grid::SetColumn(card.root, column);
        analyticsSummaryGrid.Children().Append(card.root);
    }
    analyticsRoot.Children().Append(analyticsSummaryGrid);

    m_speakingAnalyticsSummaryText = TextBlock();
    m_speakingAnalyticsSummaryText.TextWrapping(TextWrapping::Wrap);
    m_speakingAnalyticsSummaryText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_speakingAnalyticsSummaryText,
        L"Speaking analytics summary values"
        );
    analyticsRoot.Children().Append(m_speakingAnalyticsSummaryText);

    auto analyticsChartsGrid = Grid();
    analyticsChartsGrid.ColumnSpacing(12.0);
    for (int column = 0; column < 2; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            1.0,
            GridUnitType::Star
            ));
        analyticsChartsGrid.ColumnDefinitions().Append(definition);
    }

    auto analyticsCriteriaCard = ClassMngrWinUISharedUX::buildCard({
        L"By Criterion",
        L"Average score and grade distribution across each criterion.",
        L"Speaking analytics criteria"
        });
    auto analyticsLegend = StackPanel();
    analyticsLegend.Orientation(Orientation::Horizontal);
    analyticsLegend.Spacing(12.0);
    for (const std::wstring_view grade : {L"A+", L"A", L"B+", L"B", L"C"})
    {
        auto item = TextBlock();
        item.Text(hstring(L"â— ") + hstring(grade));
        item.FontSize(12.0);
        const auto gradeColor = grade == L"A+" ? Windows::UI::Color{255, 21, 148, 71}
            : grade == L"A" ? Windows::UI::Color{255, 63, 126, 203}
            : grade == L"B+" ? Windows::UI::Color{255, 215, 163, 22}
            : grade == L"B" ? Windows::UI::Color{255, 239, 90, 19}
            : Windows::UI::Color{255, 189, 24, 33};
        item.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(gradeColor));
        analyticsLegend.Children().Append(item);
    }
    analyticsCriteriaCard.content.Children().Append(analyticsLegend);
    m_speakingAnalyticsCriteriaPanel = StackPanel();
    m_speakingAnalyticsCriteriaPanel.Spacing(10.0);
    setAutomationName(
        m_speakingAnalyticsCriteriaPanel,
        L"Speaking analytics criterion metrics"
        );
    analyticsCriteriaCard.content.Children().Append(
        m_speakingAnalyticsCriteriaPanel
        );
    Grid::SetColumn(analyticsCriteriaCard.root, 0);
    analyticsChartsGrid.Children().Append(analyticsCriteriaCard.root);

    auto analyticsShapeCard = ClassMngrWinUISharedUX::buildCard({
        L"Class Shape",
        L"Grade histogram for the selected evaluation and year-to-date trend.",
        L"Speaking analytics class shape"
        });
    m_speakingAnalyticsShapeText = TextBlock();
    m_speakingAnalyticsShapeText.TextWrapping(TextWrapping::Wrap);
    m_speakingAnalyticsShapeText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_speakingAnalyticsShapeText,
        L"Speaking analytics class shape values"
        );
    analyticsShapeCard.content.Children().Append(m_speakingAnalyticsShapeText);
    m_speakingAnalyticsShapePanel = StackPanel();
    m_speakingAnalyticsShapePanel.Spacing(10.0);
    analyticsShapeCard.content.Children().Append(m_speakingAnalyticsShapePanel);
    Grid::SetColumn(analyticsShapeCard.root, 1);
    analyticsChartsGrid.Children().Append(analyticsShapeCard.root);
    analyticsRoot.Children().Append(analyticsChartsGrid);

    auto analyticsRankingCard = ClassMngrWinUISharedUX::buildCard({
        L"Student Ranking",
        L"Read-only ranking, including every scored speaking criterion.",
        L"Speaking analytics student ranking"
        });
    m_speakingAnalyticsRankingList = ListView();
    m_speakingAnalyticsRankingList.SelectionMode(ListViewSelectionMode::None);
    m_speakingAnalyticsRankingList.IsTabStop(true);
    m_speakingAnalyticsRankingList.TabIndex(1);
    m_speakingAnalyticsRankingList.MinHeight(360.0);
    ScrollViewer::SetHorizontalScrollBarVisibility(
        m_speakingAnalyticsRankingList,
        ScrollBarVisibility::Auto
        );
    setAutomationName(
        m_speakingAnalyticsRankingList,
        L"Speaking analytics student ranking list"
        );
    analyticsRankingCard.content.Children().Append(
        m_speakingAnalyticsRankingList
        );
    analyticsRoot.Children().Append(analyticsRankingCard.root);


    return analyticsRoot;
}

Microsoft::UI::Xaml::Controls::StackPanel
MainWindow::buildClassCoTeacherSection()
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

    auto coTeacherRoot = makeRoot(StackPanel());

    auto coTeacherCard = ClassMngrWinUISharedUX::buildCard({
        L"Korean Teacher",
        L"",
        L"Class co-teacher information"
        });
    coTeacherCard.root.Padding(Thickness{24.0, 24.0, 24.0, 24.0});
    coTeacherCard.content.Spacing(16.0);

    auto coTeacherGrid = Grid();
    coTeacherGrid.ColumnSpacing(20.0);
    coTeacherGrid.RowSpacing(16.0);
    for (int column = 0; column < 3; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            240.0,
            GridUnitType::Pixel
            ));
        coTeacherGrid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 3; ++row)
    {
        coTeacherGrid.RowDefinitions().Append(RowDefinition());
    }

    const auto configureCoTeacherCombo = [this](
        ComboBox combo,
        wchar_t const* header,
        wchar_t const* automationName
        ) {
        combo.Header(box_value(hstring(header)));
        combo.MinWidth(240.0);
        combo.Width(240.0);
        combo.IsTabStop(true);
        combo.SelectionChanged({
            this,
            &MainWindow::ClassCoTeacherSelection_SelectionChanged
            });
        setAutomationName(combo, automationName);
    };
    const auto configureCoTeacherReadOnly = [](
        TextBox box,
        wchar_t const* header,
        wchar_t const* automationName
        ) {
        box.Header(box_value(hstring(header)));
        box.MinWidth(240.0);
        box.Width(240.0);
        box.IsReadOnly(true);
        box.IsTabStop(false);
        setAutomationName(box, automationName);
        return box;
    };

    m_classCoTeacherKrCombo = ComboBox();
    configureCoTeacherCombo(
        m_classCoTeacherKrCombo,
        L"Korean",
        L"Co-teacher Korean name"
        );
    m_classCoTeacherEnCombo = ComboBox();
    configureCoTeacherCombo(
        m_classCoTeacherEnCombo,
        L"English",
        L"Co-teacher English name"
        );
    m_classCoTeacherRoomTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Room",
        L"Co-teacher room"
        );
    m_classCoTeacherInternetTypeTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Internet Type",
        L"Co-teacher internet type"
        );
    m_classCoTeacherWifiNameTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"WiFi Name",
        L"Co-teacher WiFi name"
        );
    m_classCoTeacherWifiPasswordTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"WiFi Password",
        L"Co-teacher WiFi password"
        );
    m_classCoTeacherProjectionTypeTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Projection Type",
        L"Co-teacher projection type"
        );
    m_classCoTeacherZoomIdTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Zoom ID",
        L"Co-teacher Zoom ID"
        );
    m_classCoTeacherZoomPasswordTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Zoom Password",
        L"Co-teacher Zoom password"
        );

    const std::array<FrameworkElement, 9> coTeacherFields{
        m_classCoTeacherKrCombo,
        m_classCoTeacherEnCombo,
        m_classCoTeacherRoomTextBox,
        m_classCoTeacherInternetTypeTextBox,
        m_classCoTeacherWifiNameTextBox,
        m_classCoTeacherWifiPasswordTextBox,
        m_classCoTeacherProjectionTypeTextBox,
        m_classCoTeacherZoomIdTextBox,
        m_classCoTeacherZoomPasswordTextBox
    };
    for (int index = 0; index < static_cast<int>(coTeacherFields.size()); ++index)
    {
        const int row = index / 3;
        const int column = index % 3;
        Grid::SetRow(coTeacherFields[static_cast<std::size_t>(index)], row);
        Grid::SetColumn(
            coTeacherFields[static_cast<std::size_t>(index)],
            column
            );
        coTeacherGrid.Children().Append(
            coTeacherFields[static_cast<std::size_t>(index)]
            );
    }
    coTeacherCard.content.Children().Append(coTeacherGrid);
    coTeacherRoot.Children().Append(coTeacherCard.root);

    m_classCoTeacherSaveButton = Button();
    m_classCoTeacherSaveButton.Content(box_value(hstring(L"Save Changes")));
    m_classCoTeacherSaveButton.IsTabStop(true);
    m_classCoTeacherSaveButton.Click({
        this,
        &MainWindow::ClassCoTeacherSaveButton_Click
        });
    setAutomationName(m_classCoTeacherSaveButton, L"Save co-teacher changes");
    m_classCoTeacherDiscardButton = Button();
    m_classCoTeacherDiscardButton.Content(
        box_value(hstring(L"Discard Changes"))
        );
    m_classCoTeacherDiscardButton.IsTabStop(true);
    m_classCoTeacherDiscardButton.Click({
        this,
        &MainWindow::ClassCoTeacherDiscardButton_Click
        });
    setAutomationName(
        m_classCoTeacherDiscardButton,
        L"Discard co-teacher changes"
        );
    m_classCoTeacherSaveButton.Visibility(Visibility::Collapsed);
    m_classCoTeacherDiscardButton.Visibility(Visibility::Collapsed);


    return coTeacherRoot;
}

} // namespace winrt::ClassMngrWinUI::implementation
