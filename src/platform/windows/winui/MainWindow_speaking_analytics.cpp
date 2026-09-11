#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

#include <winrt/Microsoft.UI.Xaml.Shapes.h>

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::refreshSpeakingAnalytics()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingAnalyticsStatusText
        || !m_speakingAnalyticsCriteriaPanel
        || !m_speakingAnalyticsShapeText
        || !m_speakingAnalyticsShapePanel
        || !m_speakingAnalyticsRankingList)
    {
        return;
    }

    m_speakingAnalyticsCriteriaPanel.Children().Clear();
    m_speakingAnalyticsRankingList.Items().Clear();
    m_speakingAnalyticsSummaryText.Text({});
    m_speakingAnalyticsShapeText.Text({});
    m_speakingAnalyticsShapePanel.Children().Clear();
    m_speakingAnalyticsYearToDateChart = nullptr;
    for (auto const& value : m_speakingAnalyticsSummaryValues)
    {
        if (value)
        {
            value.Text(L"-");
        }
    }
    const auto showAnalyticsPlaceholder = [this](std::wstring_view message) {
        auto placeholder = TextBlock();
        placeholder.Text(hstring(message));
        placeholder.TextWrapping(TextWrapping::Wrap);
        m_speakingAnalyticsShapePanel.Children().Append(placeholder);
    };

    if (!m_openDatabase)
    {
        m_speakingAnalyticsStatusText.Text(L"No database open.");
        m_speakingAnalyticsSummaryText.Text(
            L"Open a database to calculate speaking analytics."
            );
        showAnalyticsPlaceholder(L"Open a database to view class shape and trend data.");
        return;
    }
    if (m_classSelectedId <= 0 || m_classNew)
    {
        m_speakingAnalyticsStatusText.Text(
            L"Save the selected class before viewing speaking analytics."
            );
        m_speakingAnalyticsSummaryText.Text(
            L"No class is available for analytics."
            );
        showAnalyticsPlaceholder(L"Save the selected class to view analytics.");
        return;
    }

    classmngr::engine::RosterService rosterService(*m_openDatabase);
    const auto roster = rosterService.load(m_classSelectedId);
    if (!roster)
    {
        m_speakingAnalyticsStatusText.Text(winrt::hstring(
            L"Speaking analytics could not load the roster: "
                + asWide(roster.error().message)
            ));
        m_speakingAnalyticsSummaryText.Text(
            L"The analytics roster is unavailable."
            );
        showAnalyticsPlaceholder(L"The analytics roster is unavailable.");
        return;
    }

    classmngr::engine::SpeakingEvaluationPersistenceService evaluationService(
        *m_openDatabase
        );
    classmngr::engine::SpeakingAnalyticsDashboardInput input;
    input.selection = m_speakingAnalyticsName.empty()
        ? "All"
        : m_speakingAnalyticsName;
    input.roster = *roster;
    input.evaluations.reserve(
        classmngr::engine::SpeakingEvaluationNames.size()
        );
    for (const std::string_view evaluationName :
         classmngr::engine::SpeakingEvaluationNames)
    {
        const auto loaded = evaluationService.load(
            m_classSelectedId,
            evaluationName
            );
        if (!loaded)
        {
            m_speakingAnalyticsStatusText.Text(winrt::hstring(
                L"Speaking analytics could not load an evaluation: "
                    + asWide(loaded.error().message)
                ));
            m_speakingAnalyticsSummaryText.Text(
                L"The analytics evaluations are unavailable."
                );
            showAnalyticsPlaceholder(L"The analytics evaluations are unavailable.");
            return;
        }

        input.evaluations.push_back({
            std::string(evaluationName),
            loaded->empty()
                ? classmngr::engine::SpeakingAnalyticsRows{}
                : classmngr::engine::SpeakingEvaluationValidator::normalized(
                    *loaded
                    )
            });
    }

    rebuildSpeakingAnalytics(
        classmngr::engine::SpeakingAnalyticsService::buildDashboard(input)
        );
}

void MainWindow::rebuildSpeakingAnalytics(
    classmngr::engine::SpeakingAnalyticsDashboard const& dashboard
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingAnalyticsStatusText
        || !m_speakingAnalyticsSummaryText
        || !m_speakingAnalyticsCriteriaPanel
        || !m_speakingAnalyticsShapeText
        || !m_speakingAnalyticsShapePanel
        || !m_speakingAnalyticsRankingList)
    {
        return;
    }

    m_speakingAnalyticsCriteriaPanel.Children().Clear();
    m_speakingAnalyticsShapePanel.Children().Clear();
    m_speakingAnalyticsRankingList.Items().Clear();

    const auto join = [](std::vector<std::string> const& values,
                         std::wstring_view separator) {
        std::wstring result;
        for (const std::string& value : values)
        {
            if (!result.empty())
            {
                result += separator;
            }
            result += asWide(value);
        }
        return result;
    };
    const auto displayOrDash = [](std::wstring value) {
        return value.empty() ? std::wstring(L"\u2014") : value;
    };

    const auto gradeColor = [](std::wstring_view grade) {
        if (grade == L"A+") return Windows::UI::Color{255, 21, 148, 71};
        if (grade == L"A") return Windows::UI::Color{255, 63, 126, 203};
        if (grade == L"B+") return Windows::UI::Color{255, 215, 163, 22};
        if (grade == L"B") return Windows::UI::Color{255, 239, 90, 19};
        return Windows::UI::Color{255, 189, 24, 33};
    };
    const auto gradeBadge = [&gradeColor](std::wstring_view grade) {
        auto badge = Border();
        badge.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(gradeColor(grade)));
        badge.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
        badge.Padding(Thickness{6.0, 2.0, 6.0, 2.0});
        auto label = TextBlock();
        label.Text(hstring(grade));
        label.FontSize(12.0);
        label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        label.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 255, 255, 255}));
        badge.Child(label);
        return badge;
    };
    const auto setSummaryValue = [this](std::size_t index,
                                        std::wstring_view value) {
        if (index < m_speakingAnalyticsSummaryValues.size()
            && m_speakingAnalyticsSummaryValues[index])
        {
            m_speakingAnalyticsSummaryValues[index].Text(hstring(value));
        }
    };

    const auto& snapshot = dashboard.selectedSnapshot.hasData
        ? dashboard.selectedSnapshot
        : dashboard.classShapeSnapshot;
    const bool hasData = dashboard.selectedSnapshot.hasData
        || dashboard.classShapeSnapshot.hasData
        || !dashboard.yearToDatePoints.empty();
    if (!hasData)
    {
        m_speakingAnalyticsStatusText.Text(
            L"No scored speaking evaluations have been recorded for this class."
            );
        m_speakingAnalyticsSummaryText.Text(
            L"Enter and save scores in Speaking Evaluations to populate analytics."
            );
        m_speakingAnalyticsShapeText.Text(
            L"Class shape: No fully scored evaluation is available."
            );
        setSummaryValue(0, L"-");
        setSummaryValue(1, L"0 students");
        setSummaryValue(2, L"-");
        setSummaryValue(3, L"-");
        auto emptyShape = TextBlock();
        emptyShape.Text(L"No fully scored evaluation is available yet.");
        emptyShape.TextWrapping(TextWrapping::Wrap);
        m_speakingAnalyticsShapePanel.Children().Append(emptyShape);
        return;
    }

    m_speakingAnalyticsStatusText.Text({});

    std::wstring summary = L"Class average: ";
    if (dashboard.selectedSnapshot.hasData)
    {
        const std::wstring average = asWide(
            classmngr::engine::SpeakingAnalyticsService::formatAverage(
                snapshot.classAverage3
                ));
        setSummaryValue(
            0,
            asWide(snapshot.classAverageLetter) + L" \u00B7 " + average
            );
        std::wstring assessed = std::to_wstring(snapshot.fullyScoredCount);
        if (snapshot.rosterStudentCount > 0)
        {
            assessed += L" / " + std::to_wstring(snapshot.rosterStudentCount);
        }
        setSummaryValue(1, assessed + L" students");
        setSummaryValue(2, displayOrDash(join(snapshot.strongestLabels, L", ")));
        setSummaryValue(3, displayOrDash(join(snapshot.focusLabels, L", ")));
        summary += asWide(snapshot.classAverageLetter)
            + L" \u00B7 " + asWide(
                classmngr::engine::SpeakingAnalyticsService::formatAverage(
                    snapshot.classAverage3
                    )
                );
        summary += L"\nStudents fully scored: "
            + std::to_wstring(snapshot.fullyScoredCount);
        if (snapshot.rosterStudentCount > 0)
        {
            summary += L" / " + std::to_wstring(snapshot.rosterStudentCount);
        }
        summary += L"\nStrongest areas: "
            + displayOrDash(join(snapshot.strongestLabels, L", "));
        summary += L"\nFocus areas: "
            + displayOrDash(join(snapshot.focusLabels, L", "));
    }
    else
    {
        summary += L"\u2014\nNo aggregate score is available for the selected scope.";
    }
    if (!dashboard.selectedSnapshot.hasData)
    {
        setSummaryValue(0, L"-");
        setSummaryValue(1, L"No scores");
        setSummaryValue(2, L"-");
        setSummaryValue(3, L"-");
    }
    m_speakingAnalyticsSummaryText.Text(hstring(summary));

    for (const auto& criterion : snapshot.criteria)
    {
        auto criterionRoot = StackPanel();
        criterionRoot.Spacing(4.0);
        auto value = TextBlock();
        std::wstring text = asWide(criterion.name) + L": ";
        if (!criterion.hasData)
        {
            text += L"No scores";
        }
        else
        {
            text += asWide(
                classmngr::engine::SpeakingAnalyticsService::numberToGrade(
                    classmngr::engine::SpeakingAnalyticsService::roundAverageToGrade(
                        criterion.average3
                        )
                    )
                );
            text += L" \u00B7 average " + asWide(
                classmngr::engine::SpeakingAnalyticsService::formatAverage(
                    criterion.average3
                    )
                );
            text += L" \u00B7 distribution: ";
            bool hasDistribution = false;
            for (const std::string_view grade :
                 classmngr::engine::SpeakingEvaluationScoreValues)
            {
                const auto found = criterion.distribution.find(
                    std::string(grade)
                    );
                if (found == criterion.distribution.end())
                {
                    continue;
                }
                if (hasDistribution)
                {
                    text += L", ";
                }
                text += asWide(grade) + L" " + std::to_wstring(found->second);
                hasDistribution = true;
            }
        }
        value.Text(hstring(text));
        value.TextWrapping(TextWrapping::Wrap);
        value.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        criterionRoot.Children().Append(value);
        if (criterion.hasData)
        {
            int largestCount = 1;
            for (const auto& [grade, count] : criterion.distribution)
            {
                static_cast<void>(grade);
                largestCount = std::max(largestCount, count);
            }
            auto distribution = StackPanel();
            distribution.Orientation(Orientation::Horizontal);
            distribution.Spacing(4.0);
            for (const std::string_view grade :
                 classmngr::engine::SpeakingEvaluationScoreValues)
            {
                const auto found = criterion.distribution.find(std::string(grade));
                if (found == criterion.distribution.end())
                {
                    continue;
                }
                auto segment = Border();
                segment.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                    gradeColor(asWide(grade))));
                segment.CornerRadius(CornerRadius{3.0, 3.0, 3.0, 3.0});
                segment.Padding(Thickness{6.0, 3.0, 6.0, 3.0});
                segment.Width(std::max(
                    34.0,
                    190.0 * static_cast<double>(found->second)
                        / static_cast<double>(largestCount)));
                auto segmentLabel = TextBlock();
                segmentLabel.Text(hstring(asWide(grade) + L" "
                    + std::to_wstring(found->second)));
                segmentLabel.FontSize(11.0);
                segmentLabel.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
                    Windows::UI::Color{255, 255, 255, 255}));
                segment.Child(segmentLabel);
                distribution.Children().Append(segment);
            }
            criterionRoot.Children().Append(distribution);
        }
        setAutomationName(
            criterionRoot,
            L"Speaking analytics " + asWide(criterion.name)
            );
        m_speakingAnalyticsCriteriaPanel.Children().Append(criterionRoot);
    }

    std::map<std::string, int> shapeDistribution;
    for (const std::string& letter : snapshot.overallLetters)
    {
        if (!letter.empty())
        {
            ++shapeDistribution[letter];
        }
    }
    std::wstring shape = L"Class-shape evaluation: ";
    shape += dashboard.classShapeEvaluationName.empty()
        ? L"\u2014"
        : asWide(dashboard.classShapeEvaluationName);
    shape += L"\nOverall grades: ";
    bool hasShape = false;
    for (const std::string_view grade :
         classmngr::engine::SpeakingEvaluationScoreValues)
    {
        const auto found = shapeDistribution.find(std::string(grade));
        if (found == shapeDistribution.end())
        {
            continue;
        }
        if (hasShape)
        {
            shape += L", ";
        }
        shape += asWide(grade) + L" " + std::to_wstring(found->second);
        hasShape = true;
    }
    if (!hasShape)
    {
        shape += L"\u2014";
    }
    shape += L"\nYear to date: ";
    if (dashboard.yearToDatePoints.empty())
    {
        shape += L"No fully scored evaluations";
    }
    else
    {
        bool first = true;
        for (const auto& point : dashboard.yearToDatePoints)
        {
            if (!first)
            {
                shape += L", ";
            }
            shape += asWide(point.evaluationName) + L" "
                + asWide(point.classAverageLetter) + L" ("
                + asWide(
                    classmngr::engine::SpeakingAnalyticsService::formatAverage(
                        point.classAverage3
                        )
                    )
                + L")";
            first = false;
        }
    }
    m_speakingAnalyticsShapeText.Text(hstring(shape));

    auto evaluationCaption = TextBlock();
    evaluationCaption.Text(hstring(L"Evaluation: "
        + (dashboard.classShapeEvaluationName.empty()
            ? std::wstring(L"-")
            : asWide(dashboard.classShapeEvaluationName))));
    evaluationCaption.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
    m_speakingAnalyticsShapePanel.Children().Append(evaluationCaption);

    auto histogram = Grid();
    histogram.Height(150.0);
    histogram.ColumnSpacing(8.0);
    int histogramMaximum = 1;
    for (const auto& [grade, count] : shapeDistribution)
    {
        static_cast<void>(grade);
        histogramMaximum = std::max(histogramMaximum, count);
    }
    for (int column = 0;
         column < static_cast<int>(classmngr::engine::SpeakingEvaluationScoreValues.size());
         ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star));
        histogram.ColumnDefinitions().Append(definition);
        const std::string_view grade = classmngr::engine::SpeakingEvaluationScoreValues[
            static_cast<std::size_t>(column)];
        const auto found = shapeDistribution.find(std::string(grade));
        const int count = found == shapeDistribution.end() ? 0 : found->second;
        auto barColumn = StackPanel();
        barColumn.VerticalAlignment(VerticalAlignment::Bottom);
        barColumn.HorizontalAlignment(HorizontalAlignment::Stretch);
        barColumn.Spacing(3.0);
        auto countLabel = TextBlock();
        countLabel.Text(std::to_wstring(count));
        countLabel.HorizontalAlignment(HorizontalAlignment::Center);
        barColumn.Children().Append(countLabel);
        auto bar = Border();
        bar.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(gradeColor(asWide(grade))));
        bar.CornerRadius(CornerRadius{4.0, 4.0, 0.0, 0.0});
        bar.Height(count == 0 ? 5.0 : 92.0 * static_cast<double>(count)
            / static_cast<double>(histogramMaximum));
        barColumn.Children().Append(bar);
        auto gradeLabel = TextBlock();
        gradeLabel.Text(hstring(asWide(grade)));
        gradeLabel.HorizontalAlignment(HorizontalAlignment::Center);
        gradeLabel.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        barColumn.Children().Append(gradeLabel);
        Grid::SetColumn(barColumn, column);
        histogram.Children().Append(barColumn);
    }
    m_speakingAnalyticsShapePanel.Children().Append(histogram);

    auto trendHeading = TextBlock();
    trendHeading.Text(L"Year to Date");
    trendHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
    m_speakingAnalyticsShapePanel.Children().Append(trendHeading);
    auto trend = StackPanel();
    trend.Spacing(8.0);
    if (dashboard.yearToDatePoints.empty())
    {
        auto emptyTrend = TextBlock();
        emptyTrend.Text(L"No fully scored evaluations");
        trend.Children().Append(emptyTrend);
    }
    else
    {
        constexpr double plotCanvasWidth = 368.0;
        constexpr double plotCanvasHeight = 156.0;
        constexpr double plotLeft = 24.0;
        constexpr double plotTop = 8.0;
        constexpr double plotWidth = 336.0;
        constexpr double plotHeight = 112.0;
        const auto chartBrush = Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 63, 126, 203}
            );
        const auto guideBrush = Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 224, 228, 234}
            );
        auto chart = Microsoft::UI::Xaml::Controls::Canvas();
        chart.Width(plotCanvasWidth);
        chart.Height(plotCanvasHeight);
        chart.HorizontalAlignment(HorizontalAlignment::Center);
        chart.VerticalAlignment(VerticalAlignment::Top);
        chart.IsHitTestVisible(false);
        for (int tick = 1; tick <= 5; ++tick)
        {
            const double y = plotTop + plotHeight
                * (5.0 - static_cast<double>(tick)) / 4.0;
            auto guide = Microsoft::UI::Xaml::Shapes::Line();
            guide.X1(plotLeft);
            guide.Y1(y);
            guide.X2(plotLeft + plotWidth);
            guide.Y2(y);
            guide.Stroke(guideBrush);
            guide.StrokeThickness(1.0);
            chart.Children().Append(guide);

            auto label = TextBlock();
            label.Text(std::to_wstring(tick));
            label.FontSize(10.0);
            label.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 96, 102, 112}
                ));
            Canvas::SetLeft(label, 0.0);
            Canvas::SetTop(label, y - 8.0);
            chart.Children().Append(label);
        }

        const std::size_t pointCount = dashboard.yearToDatePoints.size();
        double previousX = 0.0;
        double previousY = 0.0;
        for (std::size_t index = 0; index < pointCount; ++index)
        {
            const auto& point = dashboard.yearToDatePoints[index];
            const double x = pointCount == 1
                ? plotLeft + plotWidth / 2.0
                : plotLeft + plotWidth * static_cast<double>(index)
                    / static_cast<double>(pointCount - 1);
            const double average = std::clamp(point.classAverage3, 1.0, 5.0);
            const double y = plotTop + plotHeight
                * (5.0 - average) / 4.0;
            if (index > 0)
            {
                auto segment = Microsoft::UI::Xaml::Shapes::Line();
                segment.X1(previousX);
                segment.Y1(previousY);
                segment.X2(x);
                segment.Y2(y);
                segment.Stroke(chartBrush);
                segment.StrokeThickness(3.0);
                chart.Children().Append(segment);
            }

            auto marker = Microsoft::UI::Xaml::Shapes::Ellipse();
            marker.Width(10.0);
            marker.Height(10.0);
            marker.Fill(chartBrush);
            Canvas::SetLeft(marker, x - 5.0);
            Canvas::SetTop(marker, y - 5.0);
            chart.Children().Append(marker);
            previousX = x;
            previousY = y;
        }
        setAutomationName(chart, L"Speaking analytics year-to-date line chart");
        m_speakingAnalyticsYearToDateChart = chart;
        trend.Children().Append(chart);

        auto labels = Grid();
        labels.Width(plotCanvasWidth);
        labels.HorizontalAlignment(HorizontalAlignment::Center);
        labels.ColumnSpacing(4.0);
        for (std::size_t index = 0; index < pointCount; ++index)
        {
            auto definition = ColumnDefinition();
            definition.Width(GridLengthHelper::FromValueAndType(
                1.0,
                GridUnitType::Star
                ));
            labels.ColumnDefinitions().Append(definition);
            const auto& point = dashboard.yearToDatePoints[index];
            auto value = TextBlock();
            value.Text(hstring(
                asWide(point.evaluationName) + L"\n"
                + asWide(
                    classmngr::engine::SpeakingAnalyticsService::formatAverage(
                        point.classAverage3
                        )
                    )
                + L" \u00B7 " + asWide(point.classAverageLetter)
                ));
            value.FontSize(11.0);
            value.TextAlignment(TextAlignment::Center);
            value.TextWrapping(TextWrapping::Wrap);
            Grid::SetColumn(value, static_cast<int>(index));
            labels.Children().Append(value);
        }
        setAutomationName(labels, L"Speaking analytics year-to-date labels");
        trend.Children().Append(labels);
    }
    m_speakingAnalyticsShapePanel.Children().Append(trend);

    auto rankingHeader = Grid();
    rankingHeader.ColumnSpacing(8.0);
    rankingHeader.MinWidth(900.0);
    const std::array<double, 5> rankingWidths{44.0, 160.0, 140.0, 100.0, 420.0};
    const std::array<wchar_t const*, 5> rankingHeaders{
        L"#", L"English Name", L"Korean Name", L"Average", L"Criteria"
    };
    for (int column = 0; column < static_cast<int>(rankingWidths.size()); ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            rankingWidths[static_cast<std::size_t>(column)], GridUnitType::Pixel));
        rankingHeader.ColumnDefinitions().Append(definition);
        auto label = TextBlock();
        label.Text(rankingHeaders[static_cast<std::size_t>(column)]);
        label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        label.Margin(Thickness{4.0, 2.0, 4.0, 6.0});
        Grid::SetColumn(label, column);
        rankingHeader.Children().Append(label);
    }
    m_speakingAnalyticsRankingList.Header(rankingHeader);

    for (std::size_t index = 0;
         index < snapshot.rankings.size();
         ++index)
    {
        const auto& rank = snapshot.rankings[index];
        auto row = Grid();
        row.ColumnSpacing(8.0);
        row.MinWidth(900.0);
        const std::array<double, 5> widths{
            44.0, 160.0, 140.0, 100.0, 420.0
        };
        for (const double width : widths)
        {
            auto definition = ColumnDefinition();
            definition.Width(GridLengthHelper::FromValueAndType(
                width,
                GridUnitType::Pixel
                ));
            row.ColumnDefinitions().Append(definition);
        }
        const std::array<std::wstring, 5> values{
            std::to_wstring(index + 1),
            asWide(rank.englishName),
            asWide(rank.koreanName),
            asWide(
                classmngr::engine::SpeakingAnalyticsService::formatAverage(
                    rank.overall3
                    )
                ) + L" (" + asWide(rank.overallLetter) + L")",
            join(rank.criterionLetters, L" \u00B7 ")
        };
        for (int column = 0; column < static_cast<int>(values.size()); ++column)
        {
            if (column == 4)
            {
                auto criteria = StackPanel();
                criteria.Orientation(Orientation::Horizontal);
                criteria.Spacing(6.0);
                const std::size_t count = std::min(
                    snapshot.criteria.size(), rank.criterionLetters.size());
                for (std::size_t criterionIndex = 0;
                     criterionIndex < count;
                     ++criterionIndex)
                {
                    auto criterion = StackPanel();
                    criterion.Spacing(2.0);
                    auto criterionName = TextBlock();
                    criterionName.Text(hstring(asWide(
                        snapshot.criteria[criterionIndex].name)));
                    criterionName.FontSize(10.0);
                    criterionName.MaxWidth(64.0);
                    criterionName.TextTrimming(TextTrimming::CharacterEllipsis);
                    criterion.Children().Append(criterionName);
                    criterion.Children().Append(gradeBadge(asWide(
                        rank.criterionLetters[criterionIndex])));
                    criteria.Children().Append(criterion);
                }
                Grid::SetColumn(criteria, column);
                row.Children().Append(criteria);
                continue;
            }
            if (column == 3)
            {
                auto average = StackPanel();
                average.Orientation(Orientation::Horizontal);
                average.Spacing(6.0);
                auto score = TextBlock();
                score.Text(hstring(asWide(
                    classmngr::engine::SpeakingAnalyticsService::formatAverage(
                        rank.overall3))));
                score.VerticalAlignment(VerticalAlignment::Center);
                average.Children().Append(score);
                average.Children().Append(gradeBadge(asWide(rank.overallLetter)));
                Grid::SetColumn(average, column);
                row.Children().Append(average);
                continue;
            }
            auto cell = TextBlock();
            cell.Text(hstring(values[static_cast<std::size_t>(column)]));
            cell.TextWrapping(TextWrapping::Wrap);
            cell.Margin(Thickness{4.0, 4.0, 4.0, 4.0});
            Grid::SetColumn(cell, column);
            row.Children().Append(cell);
        }
        setAutomationName(
            row,
            L"Speaking analytics ranking row " + std::to_wstring(index + 1)
            );
        m_speakingAnalyticsRankingList.Items().Append(row);
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
