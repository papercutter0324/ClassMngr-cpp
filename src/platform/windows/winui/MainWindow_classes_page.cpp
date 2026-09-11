#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populateClassesPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    std::wstring_view pageId
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    // Class child routes share one presentation page so the prototype
    // controls keep a single owner while the sidebar exposes the complete
    // class hierarchy.
    static_cast<void>(pageId);
    auto detailsRoot = buildClassDetailsSection();
    auto notesRoot = buildClassNotesSection();
    auto rosterRoot = buildClassRosterSection();
    auto speakingRoot = buildClassSpeakingSection();
    auto analyticsRoot = buildClassAnalyticsSection();
    auto coTeacherRoot = buildClassCoTeacherSection();

    const auto navigationCard = ClassMngrWinUISharedUX::buildCard({
        L"",
        L"",
        L"Classes navigation"
        });
    m_classNavigationCard = navigationCard.root;
    m_classNavigationRoot = StackPanel();
    m_classNavigationRoot.Spacing(8.0);
    m_classNavigationRoot.HorizontalAlignment(HorizontalAlignment::Stretch);
    setAutomationName(m_classNavigationRoot, L"Classes navigation controls");

    auto navigationFilters = Grid();
    navigationFilters.ColumnSpacing(12.0);
    navigationFilters.ColumnDefinitions().Append(ColumnDefinition());
    navigationFilters.ColumnDefinitions().Append(ColumnDefinition());

    m_classNavigationGradeTabs = StackPanel();
    m_classNavigationGradeTabs.Orientation(Orientation::Horizontal);
    m_classNavigationGradeTabs.Spacing(6.0);
    m_classNavigationGradeTabs.HorizontalAlignment(HorizontalAlignment::Left);
    setAutomationName(
        m_classNavigationGradeTabs,
        L"Class grade filters"
        );
    Grid::SetColumn(m_classNavigationGradeTabs, 0);
    navigationFilters.Children().Append(m_classNavigationGradeTabs);

    auto dayFilterScroll = ScrollViewer();
    dayFilterScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
    dayFilterScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);
    dayFilterScroll.HorizontalAlignment(HorizontalAlignment::Right);
    m_classNavigationDayTabs = StackPanel();
    m_classNavigationDayTabs.Orientation(Orientation::Horizontal);
    m_classNavigationDayTabs.Spacing(6.0);
    m_classNavigationDayTabs.HorizontalAlignment(HorizontalAlignment::Right);
    setAutomationName(
        m_classNavigationDayTabs,
        L"Class day filters"
        );
    dayFilterScroll.Content(m_classNavigationDayTabs);
    Grid::SetColumn(dayFilterScroll, 1);
    navigationFilters.Children().Append(dayFilterScroll);
    m_classNavigationRoot.Children().Append(navigationFilters);

    auto classTabScroll = ScrollViewer();
    classTabScroll.Height(64.0);
    classTabScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
    classTabScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);
    m_classNavigationClassTabs = StackPanel();
    m_classNavigationClassTabs.Orientation(Orientation::Horizontal);
    m_classNavigationClassTabs.Spacing(6.0);
    m_classNavigationClassTabs.HorizontalAlignment(HorizontalAlignment::Left);
    setAutomationName(
        m_classNavigationClassTabs,
        L"Class selection tabs"
        );
    classTabScroll.Content(m_classNavigationClassTabs);
    m_classNavigationRoot.Children().Append(classTabScroll);
    navigationCard.content.Children().Append(m_classNavigationRoot);

    const auto scrollTab = [](StackPanel const& content,
                              bool allowHorizontalScroll = false) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(
            allowHorizontalScroll
                ? ScrollBarVisibility::Auto
                : ScrollBarVisibility::Disabled
            );
        scroll.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        scroll.Content(content);
        return scroll;
    };

    m_classSectionScrollViews = {
        scrollTab(detailsRoot),
        scrollTab(rosterRoot, true),
        scrollTab(analyticsRoot),
        scrollTab(speakingRoot, true),
        scrollTab(coTeacherRoot),
        scrollTab(notesRoot)
    };

    m_classSectionSelectorBar = SelectorBar();
    m_classSectionSelectorBar.IsTabStop(true);
    m_classSectionSelectorBar.TabIndex(0);
    setAutomationName(
        m_classSectionSelectorBar,
        L"Classes section selector"
        );
    const std::array<std::pair<wchar_t const*, wchar_t const*>, 6>
        sectionDefinitions{
            {
                {L"Details", L"Class Details section"},
                {L"Roster", L"Class Roster section"},
                {L"Analytics", L"Class Analytics section"},
                {L"Evaluations", L"Class Evaluations section"},
                {L"Co-Teacher", L"Class Co-Teacher section"},
                {L"Notes", L"Class Notes section"}
            }
        };
    for (int index = 0;
         index < static_cast<int>(sectionDefinitions.size());
         ++index)
    {
        auto item = SelectorBarItem();
        item.Text(sectionDefinitions[static_cast<std::size_t>(index)].first);
        item.Tag(box_value(index));
        setAutomationName(
            item,
            sectionDefinitions[static_cast<std::size_t>(index)].second
            );
        m_classSectionSelectorItems[static_cast<std::size_t>(index)] = item;
        m_classSectionSelectorBar.Items().Append(item);
    }
    m_classSectionSelectorBar.SelectionChanged(
        [this](SelectorBar const& sender, auto const&) {
            if (m_classSectionSelectionChanging)
            {
                return;
            }
            const auto selected = sender.SelectedItem();
            if (selected)
            {
                const int requested = boxedInt(selected.Tag());
                if (m_classRosterDirty
                    && !m_classDirty
                    && !m_speakingEvaluationDirty
                    && requested != m_classSectionIndex)
                {
                    selectClassSection(m_classSectionIndex);
                    confirmClassRosterNavigation([this, requested]() {
                        selectClassSection(requested);
                    });
                    return;
                }
                selectClassSection(requested);
            }
        }
        );

    m_classSectionTitle = TextBlock();
    m_classSectionTitle.FontSize(24.0);
    m_classSectionTitle.Margin(Thickness{16.0, 0.0, 16.0, 0.0});
    m_classSectionTitle.HorizontalAlignment(HorizontalAlignment::Left);
    m_classSectionTitle.VerticalAlignment(VerticalAlignment::Center);
    applyResourceStyle(
        m_classSectionTitle,
        L"Phase3PageTitleTextBlockStyle"
        );
    setAutomationName(m_classSectionTitle, L"Active Classes section title");

    m_classSectionContentHost = ContentControl();
    m_classSectionContentHost.HorizontalContentAlignment(
        HorizontalAlignment::Stretch
        );
    m_classSectionContentHost.VerticalContentAlignment(
        VerticalAlignment::Stretch
        );
    setAutomationName(
        m_classSectionContentHost,
        L"Active Classes section content"
        );

    m_classSectionActionsHost = ContentControl();
    m_classSectionActionsHost.HorizontalContentAlignment(
        HorizontalAlignment::Stretch
        );
    m_classSectionActionsHost.VerticalContentAlignment(
        VerticalAlignment::Bottom
        );
    m_classSectionActionsHost.Margin(Thickness{16.0, 0.0, 16.0, 12.0});
    m_classSectionActionsHost.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_classSectionActionsHost,
        L"Active Classes section actions"
        );

    selectClassSection(m_classSectionIndex);

    m_classPageRoot = Grid();
    m_classPageRoot.RowSpacing(12.0);
    m_classPageRoot.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_classPageRoot.VerticalAlignment(VerticalAlignment::Stretch);
    for (int rowIndex = 0; rowIndex < 5; ++rowIndex)
    {
        auto row = RowDefinition();
        row.Height(
            GridLengthHelper::FromValueAndType(
                1.0,
                rowIndex == 3 ? GridUnitType::Star : GridUnitType::Auto
                )
            );
        m_classPageRoot.RowDefinitions().Append(row);
    }
    Grid::SetRow(m_classSectionSelectorBar, 0);
    Grid::SetRow(m_classSectionTitle, 2);
    Grid::SetRow(m_classSectionContentHost, 3);
    Grid::SetRow(m_classSectionActionsHost, 4);
    m_classPageRoot.Children().Append(m_classSectionSelectorBar);
    m_classPageRoot.Children().Append(m_classSectionTitle);
    m_classPageRoot.Children().Append(m_classSectionContentHost);
    m_classPageRoot.Children().Append(m_classSectionActionsHost);
    // applyClassNavigationLayout() assigns the navigation card, title, and
    // content host to their preference-dependent rows after the fixed section
    // selector/action chrome is in place.
    m_classPageRoot.Children().Append(m_classNavigationCard);
    page.Content(m_classPageRoot);
    applyClassNavigationLayout();
    refreshClassesPage();
}

} // namespace winrt::ClassMngrWinUI::implementation
