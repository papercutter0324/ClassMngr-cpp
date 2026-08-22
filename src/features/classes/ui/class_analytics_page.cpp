#include "features/classes/ui/class_analytics_page.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "features/classes/ui/class_analytics_charts.h"
#include "features/classes/ui/class_analytics_ranking_delegate.h"
#include "features/classes/ui/class_analytics_ranking_header.h"
#include "features/classes/ui/class_analytics_ranking_model.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/pages/scrollable_page_body.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QShowEvent>
#include <QScrollBar>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

constexpr int kSectionSpacing = 20;
constexpr int kSummaryTwoColumnBreakpoint = 520;
constexpr int kSummaryFourColumnBreakpoint = 920;
constexpr int kChartsHorizontalBreakpoint = 900;
constexpr int kRankingColumnPadding = 5;
constexpr int kRankingHeaderPadding = 10;
constexpr int kRankingIndexColumnWidth = 40;
constexpr int kRankingEnglishColumnWidth = 160;
constexpr int kRankingKoreanColumnWidth = 140;
constexpr int kRankingAverageColumnWidth = 90;
constexpr int kRankingCriterionColumnCount = 6;

QLabel* addStatValue(SectionCard* card)
{
    auto* value = new QLabel(QStringLiteral("—"), card);
    value->setFont(FontManager::getUiFont(20, QFont::DemiBold));
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setMinimumHeight(42);
    card->contentLayout()->setContentsMargins(16, 8, 16, 16);
    card->contentLayout()->addWidget(value);
    return value;
}

void clearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }
}

AnalyticsCharts::CriterionInsight criterionInsight(
    const SpeakingAnalytics::CriterionSlice& slice,
    const SpeakingAnalytics::Snapshot& snapshot
)
{
    const bool strongest = snapshot.strongestNames.contains(slice.name);
    const bool focus = snapshot.focusNames.contains(slice.name);
    if (strongest == focus)
        return AnalyticsCharts::CriterionInsight::None;
    return strongest ? AnalyticsCharts::CriterionInsight::Strongest
                     : AnalyticsCharts::CriterionInsight::Focus;
}

int fittedRankingColumnWidth(const QTableView* table, int column)
{
    Q_ASSERT(table);
    Q_ASSERT(table->model());

    const QFont font = column == 2
        ? FontManager::getKoreanFont()
        : FontManager::getUiFont(column == 3 ? 11 : 12);
    const QFontMetrics metrics(font);
    int widest = 0;
    const QAbstractItemModel* model = table->model();
    for (int row = 0; row < model->rowCount(); ++row)
    {
        const QModelIndex index = model->index(row, column);
        const QString text = index.data(Qt::DisplayRole).toString();

        int width = metrics.horizontalAdvance(text);
        if (column == 3
            && !index.data(AnalyticsRankingRoles::Grade).toString().isEmpty())
        {
            width += AnalyticsRankingLayout::GradeBadgeWidth
                + AnalyticsRankingLayout::GradeBadgeTextSpacing;
        }
        widest = qMax(widest, width);
    }

    const int valueWidth = widest + (column == 3
        ? AnalyticsRankingLayout::AverageScoreLeftPadding
            + AnalyticsRankingLayout::AverageScoreRightPadding
        : 2 * kRankingColumnPadding);
    if (column != 1 && column != 2)
        return valueWidth;

    const int headerWidth =
        QFontMetrics(FontManager::getUiFont(12, QFont::DemiBold))
            .horizontalAdvance(
                model->headerData(
                    column,
                    Qt::Horizontal,
                    Qt::DisplayRole
                    ).toString()
                )
        + 2 * kRankingHeaderPadding;
    return qMax(valueWidth, headerWidth);
}

int rankingTableMinimumWidth(const QTableView* table)
{
    Q_ASSERT(table);

    const QHeaderView* header = table->horizontalHeader();
    const int fixedColumnsWidth = header->sectionSize(0)
        + header->sectionSize(1) + header->sectionSize(2)
        + header->sectionSize(3);
    const int tableChromeWidth = 2 * table->frameWidth()
        + table->verticalScrollBar()->sizeHint().width();
    return fixedColumnsWidth
        + kRankingCriterionColumnCount * header->sectionSize(1)
        + tableChromeWidth;
}

void applyAnalyticsCardTitleFont(SectionCard* card)
{
    Q_ASSERT(card);
    card->setTitleFont(FontManager::getUiFont(14, QFont::DemiBold));
}

} // namespace

ClassAnalyticsPage::ClassAnalyticsPage(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_embedded(embedded)
{
    Q_ASSERT(m_services);
    setProperty("role", UiRoles::Analytics);

    if (m_embedded)
    {
        setPageLayoutMargins({});
    }

    buildUi();
}

void ClassAnalyticsPage::buildUi()
{
    const int pageMargin = m_embedded ? 0 : UiConstants::Pages::Margin;
    contentLayout()->setContentsMargins(
        pageMargin, pageMargin, pageMargin, pageMargin);

    auto* body = new ScrollablePageBody(this, QMargins(0, 0, 0, 0), 16);
    body->setWidgetResizable(true);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    body->installEventFilter(this);
    m_dashboardBody = body;
    contentLayout()->addWidget(body);

    auto* content = body->contentLayout();
    content->setContentsMargins(
        m_embedded ? 0 : 8,
        m_embedded ? 0 : 8,
        m_embedded ? 0 : 8,
        m_embedded ? 0 : 12
        );
    content->setSpacing(m_embedded ? 12 : 14);

    auto* topBar = new QWidget(body);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(10);
    m_heading = new QLabel(topBar);
    m_heading->setFont(FontManager::getUiFont(18, QFont::DemiBold));
    topLayout->addWidget(m_heading);
    topLayout->addStretch();
    m_evaluationLabel = new QLabel(topBar);
    m_evaluationLabel->setFont(FontManager::getUiFont(12));
    topLayout->addWidget(m_evaluationLabel);
    m_evaluationCombo = new QComboBox(topBar);
    m_evaluationCombo->setFont(FontManager::getUiFont(12));
    m_evaluationCombo->setMinimumWidth(170);
    topLayout->addWidget(m_evaluationCombo);
    content->addWidget(topBar);

    connect(m_evaluationCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ClassAnalyticsPage::onEvaluationChanged);

    m_summaryContainer = new QWidget(body);
    m_summaryLayout = new QGridLayout(m_summaryContainer);
    m_summaryLayout->setContentsMargins(0, 0, 0, 0);
    m_summaryLayout->setSpacing(kSectionSpacing);
    m_averageCard = new SectionCard(QString(), m_summaryContainer);
    m_assessedCard = new SectionCard(QString(), m_summaryContainer);
    m_strongestCard = new SectionCard(QString(), m_summaryContainer);
    m_focusCard = new SectionCard(QString(), m_summaryContainer);
    for (SectionCard* card : { m_averageCard, m_assessedCard, m_strongestCard,
                               m_focusCard })
    {
        applyAnalyticsCardTitleFont(card);
    }
    m_averageValue = addStatValue(m_averageCard);
    m_assessedValue = addStatValue(m_assessedCard);
    m_strongestValue = addStatValue(m_strongestCard);
    m_focusValue = addStatValue(m_focusCard);
    m_summaryCards = { m_averageCard, m_assessedCard, m_strongestCard, m_focusCard };
    content->addWidget(m_summaryContainer);

    m_chartsContainer = new QWidget(body);
    m_chartsLayout = new QGridLayout(m_chartsContainer);
    m_chartsLayout->setContentsMargins(0, 0, 0, 0);
    m_chartsLayout->setSpacing(kSectionSpacing);

    m_criteriaCard = new SectionCard(QString(), m_chartsContainer);
    applyAnalyticsCardTitleFont(m_criteriaCard);
    auto* criteriaOuter = m_criteriaCard->contentLayout();
    criteriaOuter->setContentsMargins(16, 8, 16, 16);
    auto* legend = new QWidget(m_criteriaCard);
    legend->setObjectName(QStringLiteral("analyticsCriterionLegend"));
    auto* legendLayout = new QHBoxLayout(legend);
    legendLayout->setContentsMargins(0, 0, 0, 0);
    legendLayout->setSpacing(12);
    for (const QString& grade : AnalyticsCharts::gradeOrder())
    {
        auto* label = new QLabel(QStringLiteral("● %1").arg(grade), legend);
        label->setFont(FontManager::getUiFont(12));
        label->setStyleSheet(QStringLiteral("color:%1;")
            .arg(AnalyticsCharts::gradeColor(grade).name()));
        legendLayout->addWidget(label);
    }
    legendLayout->addStretch();
    criteriaOuter->addWidget(legend);
    m_criteriaContainer = new QWidget(m_criteriaCard);
    m_criteriaContainer->setObjectName(QStringLiteral("analyticsCriterionMetrics"));
    m_criteriaLayout = new QGridLayout(m_criteriaContainer);
    m_criteriaLayout->setContentsMargins(12, 8, 12, 8);
    m_criteriaLayout->setVerticalSpacing(4);
    criteriaOuter->addWidget(m_criteriaContainer);

    m_shapeCard = new SectionCard(QString(), m_chartsContainer);
    applyAnalyticsCardTitleFont(m_shapeCard);
    m_shapeCard->setMinimumWidth(270);
    auto* shapeLayout = m_shapeCard->contentLayout();
    shapeLayout->setContentsMargins(16, 8, 16, 16);
    shapeLayout->setSpacing(8);
    m_histogramCaption = new QLabel(m_shapeCard);
    m_histogramCaption->setObjectName(
        QStringLiteral("analyticsClassShapeEvaluationCaption"));
    m_histogramCaption->setFont(FontManager::getUiFont(11, QFont::DemiBold));
    m_histogramCaption->setAlignment(Qt::AlignCenter);
    shapeLayout->addWidget(m_histogramCaption);
    m_histogram = new GradeHistogram(m_shapeCard);
    shapeLayout->addWidget(m_histogram);
    m_yearToDateHeading = new QLabel(m_shapeCard);
    m_yearToDateHeading->setObjectName(
        QStringLiteral("analyticsYearToDateHeading"));
    m_yearToDateHeading->setFont(FontManager::getUiFont(13, QFont::DemiBold));
    m_yearToDateHeading->setAlignment(Qt::AlignCenter);
    shapeLayout->addWidget(m_yearToDateHeading);
    m_yearToDateChart = new YearToDateChart(m_shapeCard);
    shapeLayout->addWidget(m_yearToDateChart);
    content->addWidget(m_chartsContainer);

    m_rankingCard = new SectionCard(QString(), body);
    applyAnalyticsCardTitleFont(m_rankingCard);
    auto* rankLayout = m_rankingCard->contentLayout();
    rankLayout->setContentsMargins(16, 8, 16, 16);
    m_rankingTable = new QTableView(m_rankingCard);
    m_rankingTable->setObjectName(
        QStringLiteral("classAnalyticsRankingTable")
        );
    m_rankingTable->setFont(FontManager::getUiFont(12));
    m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rankingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rankingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rankingTable->setWordWrap(false);
    m_rankingTable->setShowGrid(false);
    m_rankingTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_rankingTable->verticalHeader()->setVisible(false);
    m_rankingTable->verticalHeader()->setDefaultSectionSize(
        ClassAnalyticsRankingDelegate::rowHeight());
    m_rankingTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_rankingTable->setHorizontalHeader(
        new ClassAnalyticsRankingHeader(Qt::Horizontal, m_rankingTable));
    m_rankingTable->setItemDelegate(
        new ClassAnalyticsRankingDelegate(m_rankingTable));
    m_rankingModel = new ClassAnalyticsRankingModel(m_rankingTable);
    m_rankingTable->setModel(m_rankingModel);
    auto* header = m_rankingTable->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, kRankingIndexColumnWidth);
    header->setSectionResizeMode(1, QHeaderView::Fixed);
    header->resizeSection(1, kRankingEnglishColumnWidth);
    header->setSectionResizeMode(2, QHeaderView::Fixed);
    header->resizeSection(2, kRankingKoreanColumnWidth);
    header->setSectionResizeMode(3, QHeaderView::Fixed);
    header->resizeSection(3, kRankingAverageColumnWidth);
    for (int column = 4; column < 10; ++column)
        header->setSectionResizeMode(column, QHeaderView::Stretch);
    m_rankingTable->setMinimumWidth(rankingTableMinimumWidth(m_rankingTable));
    rankLayout->addWidget(m_rankingTable);
    content->addWidget(m_rankingCard);

    m_emptyLabel = new QLabel(body);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setFont(FontManager::getUiFont(12));
    m_emptyLabel->setMinimumHeight(180);
    content->addWidget(m_emptyLabel);
    content->addStretch();

    retranslateUi();
    // The widths are not known while the scroll body is being constructed.
    // layout*() still installs a valid one-column layout now, so the chart
    // cards are visible before the first resize/show event arrives.
    applyResponsiveLayout();
    showEmpty(true);
}

void ClassAnalyticsPage::loadClass(const Classroom& classroom)
{
    m_classId = classroom.id;
    rebuild();
}

void ClassAnalyticsPage::clearDatabaseState()
{
    m_classId = -1;
    clearDisplay();
    showEmpty(true);
}

void ClassAnalyticsPage::refresh()
{
    if (m_classId >= 0)
        rebuild();
}

void ClassAnalyticsPage::retranslateUi()
{
    const QString selected = selectedEvaluationName();
    m_heading->setText(tr("Class Analytics"));
    m_evaluationLabel->setText(tr("Evaluation"));
    m_averageCard->setTitle(tr("Class Average"));
    m_assessedCard->setTitle(tr("Students Fully Scored"));
    m_strongestCard->setTitle(tr("Strongest Area"));
    m_focusCard->setTitle(tr("Focus Area"));
    m_criteriaCard->setTitle(tr("By Criterion"));
    m_shapeCard->setTitle(tr("Class Shape"));
    m_histogramCaption->setText(tr("Evaluation: %1").arg(
        m_classShapeEvaluationName.isEmpty()
            ? QStringLiteral("—")
            : m_classShapeEvaluationName));
    m_yearToDateHeading->setText(tr("Year to Date"));
    m_yearToDateChart->update();
    m_rankingCard->setTitle(tr("Student Ranking"));
    m_emptyLabel->setText(
        tr("No scored speaking evaluations have been recorded for this class yet."));
    setRankingHeaders();
    populateEvaluationSelector(selected);

    if (m_classId >= 0 && !m_rebuilding)
        rebuild();
}

bool ClassAnalyticsPage::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_dashboardBody
        && (event->type() == QEvent::Resize
            || event->type() == QEvent::FontChange))
    {
        applyResponsiveLayout();
    }
    return BasePage::eventFilter(object, event);
}

void ClassAnalyticsPage::showEvent(QShowEvent* event)
{
    BasePage::showEvent(event);
    QTimer::singleShot(0, this, &ClassAnalyticsPage::applyResponsiveLayout);
}

void ClassAnalyticsPage::onEvaluationChanged()
{
    if (!m_rebuilding)
        rebuild();
}

void ClassAnalyticsPage::populateEvaluationSelector(const QString& selectedName)
{
    if (!m_evaluationCombo)
        return;

    const QString retained = selectedName.isEmpty()
        ? selectedEvaluationName()
        : selectedName;
    m_evaluationCombo->blockSignals(true);
    m_evaluationCombo->clear();
    m_evaluationCombo->addItem(tr("All"), QString());
    for (const QString& name : SpeakingAnalytics::evaluationNames())
        m_evaluationCombo->addItem(name, name);

    const int index = m_evaluationCombo->findData(retained);
    m_evaluationCombo->setCurrentIndex(index >= 0 ? index : 0);
    m_evaluationCombo->blockSignals(false);
}

QString ClassAnalyticsPage::selectedEvaluationName() const
{
    return m_evaluationCombo ? m_evaluationCombo->currentData().toString()
                             : QString();
}

void ClassAnalyticsPage::clearDisplay()
{
    m_averageValue->setText(QStringLiteral("—"));
    m_assessedValue->setText(QStringLiteral("—"));
    m_strongestValue->setText(QStringLiteral("—"));
    m_focusValue->setText(QStringLiteral("—"));
    m_strongestLabels.clear();
    m_focusLabels.clear();
    m_classShapeEvaluationName.clear();
    m_histogramCaption->setText(tr("Evaluation: %1").arg(QStringLiteral("—")));
    m_histogram->setData({});
    m_yearToDateChart->setData({});
    clearLayout(m_criteriaLayout);
    m_criterionBars.clear();
    m_rankingModel->setRankings({});
    m_rankingTable->setMinimumHeight(0);
}

void ClassAnalyticsPage::showEmpty(bool empty)
{
    m_summaryContainer->setVisible(!empty);
    m_chartsContainer->setVisible(!empty);
    m_rankingCard->setVisible(!empty);
    m_emptyLabel->setVisible(empty);
}

QString ClassAnalyticsPage::areaText(
    const QList<QString>& labels,
    const QLabel* value
    ) const
{
    if (labels.isEmpty())
        return QStringLiteral("—");

    const int availableWidth = value ? value->contentsRect().width() : 0;
    if (availableWidth <= 0)
        return labels.join(QLatin1Char('\n'));

    const QFontMetrics metrics(value->font());
    QStringList lines;
    for (const QString& label : labels)
    {
        const int gradeStart = label.lastIndexOf(QStringLiteral(" ("));
        const bool hasTrailingGrade = gradeStart > 0 && label.endsWith(')');
        if (hasTrailingGrade
            && metrics.horizontalAdvance(label) > availableWidth)
        {
            lines.append(label.left(gradeStart));
            lines.append(label.mid(gradeStart + 1));
        }
        else
        {
            lines.append(label);
        }
    }
    return lines.join(QLatin1Char('\n'));
}

void ClassAnalyticsPage::refreshAreaValueTexts()
{
    m_strongestValue->setText(areaText(m_strongestLabels, m_strongestValue));
    m_focusValue->setText(areaText(m_focusLabels, m_focusValue));
}

void ClassAnalyticsPage::rebuild()
{
    m_rebuilding = true;
    clearDisplay();

    SpeakingEvaluationDashboard dashboard;
    if (m_services && m_classId >= 0)
    {
        if (const auto* service = m_services->speakingEvaluationService())
        {
            dashboard = service->analyticsDashboard(
                m_classId, selectedEvaluationName())
                .value_or(SpeakingEvaluationDashboard{});
        }
    }

    const bool hasDashboardData = dashboard.selectedSnapshot.hasData
        || !dashboard.yearToDatePoints.isEmpty();
    showEmpty(!hasDashboardData);
    if (hasDashboardData)
        applyDashboard(dashboard);
    m_rebuilding = false;
}

void ClassAnalyticsPage::applyDashboard(
    const SpeakingEvaluationDashboard& dashboard
)
{
    if (dashboard.selectedSnapshot.hasData)
        applySnapshot(dashboard.selectedSnapshot);

    applyClassShape(
        dashboard.classShapeSnapshot,
        dashboard.classShapeEvaluationName);
    m_yearToDateChart->setData(dashboard.yearToDatePoints);
}

void ClassAnalyticsPage::applyClassShape(
    const SpeakingAnalytics::Snapshot& snapshot,
    const QString& evaluationName
)
{
    m_classShapeEvaluationName = evaluationName;
    m_histogramCaption->setText(tr("Evaluation: %1").arg(
        evaluationName.isEmpty() ? QStringLiteral("—") : evaluationName));

    QMap<QString, int> shapeDistribution;
    for (const QString& grade : snapshot.overallLetters)
    {
        if (!grade.isEmpty())
            ++shapeDistribution[grade];
    }
    m_histogram->setData(shapeDistribution);
}

void ClassAnalyticsPage::applySnapshot(const SpeakingAnalytics::Snapshot& snapshot)
{
    m_averageValue->setText(QStringLiteral("%1 · %2")
        .arg(snapshot.classAverageLetter,
             SpeakingAnalytics::formatAverage(snapshot.classAverage3)));

    QString assessed = QString::number(snapshot.fullyScoredCount);
    if (snapshot.rosterStudentCount > 0)
        assessed += QStringLiteral(" / %1").arg(snapshot.rosterStudentCount);
    m_assessedValue->setText(assessed);

    m_strongestCard->setTitle(snapshot.strongestLabels.size() > 1
        ? tr("Strongest Areas") : tr("Strongest Area"));
    m_focusCard->setTitle(snapshot.focusLabels.size() > 1
        ? tr("Focus Areas") : tr("Focus Area"));
    m_strongestLabels = snapshot.strongestLabels;
    m_focusLabels = snapshot.focusLabels;
    refreshAreaValueTexts();

    for (const SpeakingAnalytics::CriterionSlice& slice : snapshot.criteria)
    {
        auto* bar = new CriterionDistributionBar(m_criteriaContainer);
        bar->setLabel(slice.name);
        bar->setAverageText(slice.hasData
            ? QStringLiteral("%1  -  %2")
                  .arg(SpeakingAnalytics::numberToGrade(
                           SpeakingAnalytics::roundAverageToGrade(slice.average3)),
                       SpeakingAnalytics::formatAverage(slice.average3))
            : QStringLiteral("—"));
        bar->setData(slice.distribution);
        bar->setInsight(criterionInsight(slice, snapshot));
        m_criteriaLayout->addWidget(bar, m_criterionBars.size(), 0);
        m_criterionBars.append(bar);
    }
    synchronizeCriterionBarStarts();

    m_rankingModel->setRankings(snapshot.rankings);
    resizeRankingColumnsToContents();

    const int visibleRows = qMin(6, m_rankingModel->rowCount());
    const int height = m_rankingTable->horizontalHeader()->height()
        + visibleRows * ClassAnalyticsRankingDelegate::rowHeight() + 4;
    m_rankingTable->setMinimumHeight(qMax(92, height));
}

void ClassAnalyticsPage::applyResponsiveLayout()
{
    if (!m_dashboardBody)
        return;

    const int width = m_dashboardBody->contentsRect().width();
    const int summaryColumns = width >= kSummaryFourColumnBreakpoint ? 4
        : width >= kSummaryTwoColumnBreakpoint ? 2
                                                  : 1;
    layoutSummaryCards(summaryColumns);
    layoutChartCards(width >= kChartsHorizontalBreakpoint);
    refreshAreaValueTexts();
    synchronizeCriterionBarStarts();
    resizeRankingColumnsToContents();
}

void ClassAnalyticsPage::layoutSummaryCards(int columns)
{
    if (columns == m_summaryColumns)
        return;

    m_summaryColumns = columns;
    for (SectionCard* card : m_summaryCards)
        m_summaryLayout->removeWidget(card);
    for (int column = 0; column < 4; ++column)
        m_summaryLayout->setColumnStretch(column, column < columns ? 1 : 0);
    for (qsizetype index = 0; index < m_summaryCards.size(); ++index)
    {
        m_summaryLayout->addWidget(
            m_summaryCards.at(index), index / columns, index % columns);
    }
}

void ClassAnalyticsPage::layoutChartCards(bool horizontal)
{
    const int horizontalValue = horizontal ? 1 : 0;
    if (horizontalValue == m_chartsHorizontal)
        return;

    m_chartsHorizontal = horizontalValue;
    m_chartsLayout->removeWidget(m_criteriaCard);
    m_chartsLayout->removeWidget(m_shapeCard);
    if (horizontal)
    {
        m_chartsLayout->addWidget(m_criteriaCard, 0, 0);
        m_chartsLayout->addWidget(m_shapeCard, 0, 1);
        m_chartsLayout->setColumnStretch(0, 6);
        m_chartsLayout->setColumnStretch(1, 4);
        m_chartsLayout->setRowStretch(0, 1);
        m_chartsLayout->setRowStretch(1, 0);
    }
    else
    {
        m_chartsLayout->addWidget(m_criteriaCard, 0, 0);
        m_chartsLayout->addWidget(m_shapeCard, 1, 0);
        m_chartsLayout->setColumnStretch(0, 1);
        m_chartsLayout->setColumnStretch(1, 0);
        m_chartsLayout->setRowStretch(0, 0);
        m_chartsLayout->setRowStretch(1, 0);
    }
}

void ClassAnalyticsPage::synchronizeCriterionBarStarts()
{
    qreal sharedBarLeft = 0.0;
    for (const CriterionDistributionBar* bar : m_criterionBars)
        sharedBarLeft = qMax(sharedBarLeft, bar->minimumBarLeft());

    for (CriterionDistributionBar* bar : m_criterionBars)
        bar->setSharedBarLeft(sharedBarLeft);
}

void ClassAnalyticsPage::resizeRankingColumnsToContents()
{
    if (
        !m_rankingTable
        || !m_rankingModel
        || m_rankingModel->rowCount() == 0
        )
        return;

    QHeaderView* header = m_rankingTable->horizontalHeader();
    for (int column = 1; column <= 3; ++column)
        header->resizeSection(
            column, fittedRankingColumnWidth(m_rankingTable, column));
    m_rankingTable->setMinimumWidth(rankingTableMinimumWidth(m_rankingTable));
}

void ClassAnalyticsPage::setRankingHeaders()
{
    m_rankingModel->setHeaderLabels({
        QString(),
        tr("English"),
        tr("Korean"),
        tr("Average"),
        tr("Grammar"),
        tr("Pronunciation"),
        tr("Fluency"),
        tr("Manner"),
        tr("Content"),
        tr("Effort")
    });
}
