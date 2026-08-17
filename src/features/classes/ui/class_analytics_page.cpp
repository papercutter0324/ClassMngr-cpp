#include "features/classes/ui/class_analytics_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/theme_service.h"
#include "app/services/feature_services.h"
#include "features/classes/ui/class_analytics_charts.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/pages/scrollable_page_body.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayoutItem>
#include <QPalette>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{

// Page width at which "Class Shape" and "By Criterion" sit side by side
// (Class Shape on the right); below it they stack vertically.
constexpr int kChartsRowBreakpoint = 840;

// Spacer between the sections, same as between the stat cards.
constexpr int kSectionSpacing = 20;

QList<QString> evaluationNames()
{
    return { QStringLiteral("All"),
             QStringLiteral("Winter"),
             QStringLiteral("Speech Contest"),
             QStringLiteral("Summer"),
             QStringLiteral("Fall") };
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

    buildUi();
}

void ClassAnalyticsPage::buildUi()
{
    contentLayout()->setContentsMargins(
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin
        );

    auto* body = new ScrollablePageBody(
        this,
        QMargins(0, 0, 0, 0),
        16
        );
    body->setWidgetResizable(true);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QVBoxLayout* scroll = body->contentLayout();
    scroll->setContentsMargins(
        m_embedded ? 0 : 8,
        m_embedded ? 0 : 8,
        m_embedded ? 0 : 8,
        8
        );
    scroll->setSpacing(14);

    contentLayout()->addWidget(body);

    // Top bar: heading + evaluation selector.
    auto* topBar = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(12);

    m_heading = new QLabel(tr("Class Analytics"), topBar);
    m_heading->setFont(FontManager::getUiFont(18));
    topLayout->addWidget(m_heading);
    topLayout->addStretch(1);

    auto* selectorLabel = new QLabel(tr("Evaluation"), topBar);
    selectorLabel->setFont(FontManager::getUiFont(12));
    topLayout->addWidget(selectorLabel);

    m_evaluationCombo = new QComboBox(topBar);
    m_evaluationCombo->addItems(evaluationNames());
    m_evaluationCombo->setMinimumWidth(170);
    topLayout->addWidget(m_evaluationCombo);

    scroll->addWidget(topBar);

    connect(
        m_evaluationCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &ClassAnalyticsPage::onEvaluationChanged
        );
    // Stat cards.
    auto* statRow = new QWidget(this);
    auto* statLayout = new QHBoxLayout(statRow);
    statLayout->setContentsMargins(0, 0, 0, 0);
    statLayout->setSpacing(kSectionSpacing);

    auto addStatCard = [this, statRow, statLayout](const QString& title) -> QLabel* {
        auto* card = new SectionCard(title, statRow);
        auto* cl = card->contentLayout();
        cl->setContentsMargins(14, 2, 14, 14);
        auto* value = new QLabel(QStringLiteral("—"), card);
        value->setFont(FontManager::getUiFont(20));
        value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        value->setMinimumHeight(34);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(value);
        statLayout->addWidget(card, 1);
        return value;
    };

    {
        auto* card = new SectionCard(tr("Class Average"), statRow);
        auto* cl = card->contentLayout();
        cl->setContentsMargins(14, 2, 14, 14);
        m_avgValue = new QLabel(QStringLiteral("—"), card);
        m_avgValue->setFont(FontManager::getUiFont(26));
        m_avgValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(m_avgValue);
        m_avgLetter = new QLabel(QString(), card);
        m_avgLetter->setFont(FontManager::getUiFont(13));
        cl->addWidget(m_avgLetter);
        statLayout->addWidget(card, 1);
    }

    m_assessedValue = addStatCard(tr("Students Assessed"));
    m_strongestValue = addStatCard(tr("Strongest Area"));
    m_focusValue = addStatCard(tr("Focus Area"));
    scroll->addWidget(statRow);

    // "Class Shape" histogram + "By Criterion" bars in one row.  When the
    // page is at least kChartsRowBreakpoint px wide the cards sit side by
    // side (Class Shape on the right) with the same 20px spacer as the stat
    // cards; below that they stack vertically.  applyChartsRowLayout()
    // re-lays the row out on every body resize event.
    auto* chartsRow = new QWidget(this);
    chartsRow->setObjectName("analyticsChartsRow");
    m_chartsRow = chartsRow;
    body->installEventFilter(this);

    m_shapeCard = new SectionCard(tr("Class Shape"), chartsRow);
    m_shapeCard->setMinimumWidth(300);
    auto* shapeLayout = m_shapeCard->contentLayout();
    shapeLayout->setContentsMargins(14, 6, 14, 14);
    m_histogram = new GradeHistogram(m_shapeCard);
    shapeLayout->addWidget(m_histogram);

    m_criteriaCard = new SectionCard(tr("By Criterion"), chartsRow);
    m_criteriaCard->setMinimumWidth(400);
    auto* criteriaOuter = m_criteriaCard->contentLayout();
    criteriaOuter->setContentsMargins(14, 6, 14, 14);
    m_criteriaContainer = new QWidget(m_criteriaCard);
    m_criteriaLayout = new QVBoxLayout(m_criteriaContainer);
    m_criteriaLayout->setContentsMargins(0, 0, 0, 0);
    m_criteriaLayout->setSpacing(10);
    criteriaOuter->addWidget(m_criteriaContainer);

    scroll->addWidget(chartsRow, 0, Qt::AlignTop);
    applyChartsRowLayout();

    // Student ranking table.
    auto* rankCard = new SectionCard(tr("Student Ranking"), this);
    auto* rankLayout = rankCard->contentLayout();
    rankLayout->setContentsMargins(14, 6, 14, 14);
    m_rankingTable = new QTableWidget(0, 10, rankCard);
    m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rankingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rankingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rankingTable->verticalHeader()->setVisible(false);
    m_rankingTable->horizontalHeader()->setHighlightSections(false);
    m_rankingTable->horizontalHeader()->setStretchLastSection(true);
    m_rankingTable->setHorizontalHeaderLabels(
        { tr("#"),
          tr("English"),
          tr("Korean"),
          tr("Average"),
          tr("Grammar"),
          tr("Pronunciation"),
          tr("Fluency"),
          tr("Manner"),
          tr("Content"),
          tr("Effort") }
        );
    m_rankingTable->setColumnWidth(0, 40);
    rankLayout->addWidget(m_rankingTable);
    scroll->addWidget(rankCard, 0, Qt::AlignTop);

    // Empty state.
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->hide();
    scroll->addWidget(m_emptyLabel);

    scroll->addStretch(1);

    // Apply theme-aware colors to the labels above (their hardcoded light
    // theme colors were removed) and keep them in sync on theme switches.
    syncThemeStyles();

    if (auto* themeService =
            m_services ? m_services->themeService() : nullptr)
    {
        connect(
            themeService,
            QOverload<Theme>::of(&ThemeService::themeChanged),
            this,
            &ClassAnalyticsPage::syncThemeStyles
            );
    }
}


void ClassAnalyticsPage::loadClass(const Classroom& classroom)
{
    m_classId = classroom.id;
    rebuild();
}

void ClassAnalyticsPage::clearDatabaseState()
{
    m_classId = -1;

    m_avgValue->setText(QStringLiteral("—"));
    m_avgLetter->clear();
    m_avgLetter->setStyleSheet(QString());
    m_assessedValue->setText(QStringLiteral("—"));
    m_strongestValue->setText(QStringLiteral("—"));
    m_focusValue->setText(QStringLiteral("—"));

    m_histogram->setData({});
    for (CriterionDistributionBar* bar : m_criterionBars)
        bar->setData({});
    m_rankingTable->setRowCount(0);
    m_rankingTable->setMinimumHeight(0);

    showEmpty(true);
}

void ClassAnalyticsPage::refresh()
{
    if (m_classId >= 0)
        rebuild();
}

void ClassAnalyticsPage::onEvaluationChanged()
{
    if (m_rebuilding)
        return;
    rebuild();
}

void ClassAnalyticsPage::retranslateUi()
{
    m_emptyLabel->setText(
        tr("No speaking evaluations have been recorded for this class yet.")
        );

    if (m_evaluationCombo)
    {
        const int idx = m_evaluationCombo->currentIndex();
        m_evaluationCombo->blockSignals(true);
        m_evaluationCombo->clear();
        m_evaluationCombo->addItems(evaluationNames());
        m_evaluationCombo->setCurrentIndex(idx < 0 ? 0 : idx);
        m_evaluationCombo->blockSignals(false);
    }

    if (m_classId >= 0)
        rebuild();
}

void ClassAnalyticsPage::showEmpty(bool empty)
{
    // The parent of each content widget is its SectionCard.
    m_histogram->parentWidget()->setVisible(!empty);
    m_criteriaContainer->parentWidget()->setVisible(!empty);
    m_rankingTable->parentWidget()->setVisible(!empty);
    m_emptyLabel->setVisible(empty);
}

void ClassAnalyticsPage::syncThemeStyles()
{
    // Same dark-theme check the schedule widgets use. The light values below
    // are the original hardcoded colors, so the light theme keeps its look;
    // the dark values follow dark.qss (#f5f5f5 primary, #a8b0bd secondary).
    const bool dark =
        palette()
            .color(QPalette::Window)
            .lightness() < 128;

    const QString primaryText =
        dark ? QStringLiteral("#f5f5f5") : QStringLiteral("#1f2937");
    const QString mutedText =
        dark ? QStringLiteral("#a8b0bd") : QStringLiteral("#8A8F98");

    if (m_heading)
    {
        m_heading->setStyleSheet(
            QStringLiteral("color:") + primaryText + QStringLiteral(";")
            );
    }

    const QList<QLabel*> statValues =
        { m_avgValue, m_assessedValue, m_strongestValue, m_focusValue };
    for (QLabel* value : statValues)
    {
        if (value)
        {
            value->setStyleSheet(
                QStringLiteral("color:") + primaryText + QStringLiteral(";")
                );
        }
    }

    if (m_emptyLabel)
    {
        m_emptyLabel->setStyleSheet(
            QStringLiteral("color:") + mutedText
            + QStringLiteral("; padding:24px;")
            );
    }
}


// Re-lays out the "Class Shape" / "By Criterion" row when the page width
// crosses the 840px breakpoint: side by side (Class Shape on the right) at
// or above it, stacked vertically below it.
void ClassAnalyticsPage::applyChartsRowLayout()
{
    if (!m_chartsRow)
        return;

    const bool horizontal = m_chartsRow->width() >= kChartsRowBreakpoint;
    if (horizontal == m_chartsRowHorizontal)
        return;
    m_chartsRowHorizontal = horizontal;

    if (m_chartsRow->layout())
        m_chartsRow->layout()->setEnabled(false);
    delete m_chartsRow->layout();

    if (horizontal)
    {
        auto* row = new QHBoxLayout(m_chartsRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(kSectionSpacing);
        // By Criterion targets ~70% of the row width; Class Shape keeps the
        // remaining ~30%, floored at its 300px minimum width.
        row->addWidget(m_criteriaCard, 7);
        row->addWidget(m_shapeCard, 3);
    }
    else
    {
        auto* row = new QVBoxLayout(m_chartsRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(kSectionSpacing);
        row->addWidget(m_shapeCard);
        row->addWidget(m_criteriaCard);
    }
}

bool ClassAnalyticsPage::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Resize)
        applyChartsRowLayout();
    return BasePage::eventFilter(obj, event);
}


void ClassAnalyticsPage::rebuild()
{
    m_rebuilding = true;

    const QString evalName =
        m_evaluationCombo ? m_evaluationCombo->currentText() : QStringLiteral("All");

    SpeakingAnalytics::Snapshot snap;
    if (m_services && m_classId >= 0)
    {
        const auto* svc = m_services->speakingEvaluationService();
        if (svc)
            snap = svc->analytics(m_classId, evalName);
    }

    const bool hasData = snap.hasData;
    showEmpty(!hasData);

    if (hasData)
    {
        // Stat cards.
        m_avgValue->setText(
            snap.classAverage3 > 0.0 ? SpeakingAnalytics::formatAverage(snap.classAverage3)
                                     : QStringLiteral("—")
            );
        if (!snap.classAverageLetter.isEmpty())
        {
            m_avgLetter->setText(snap.classAverageLetter);
            m_avgLetter->setStyleSheet(
                QStringLiteral("color:")
                + AnalyticsCharts::gradeColor(snap.classAverageLetter).name()
                + QStringLiteral("; font-weight:bold;")
                );
        }
        else
        {
            m_avgLetter->clear();
            m_avgLetter->setStyleSheet(QString());
        }

        QString assessed = QString::number(snap.fullyScoredCount);
        if (snap.rosterStudentCount > 0)
            assessed += QStringLiteral(" / ") + QString::number(snap.rosterStudentCount);
        m_assessedValue->setText(assessed);

        m_strongestValue->setText(
            snap.strongestNames.isEmpty() ? tr("—")
                                          : snap.strongestNames.join(QStringLiteral(", "))
            );
        m_focusValue->setText(
            snap.focusNames.isEmpty() ? tr("—")
                                     : snap.focusNames.join(QStringLiteral(", "))
            );

        // Class shape histogram: count each student's overall letter.
        QMap<QString, int> dist;
        for (const QString& letter : snap.overallLetters)
        {
            if (!letter.isEmpty())
                dist[letter] += 1;
        }
        m_histogram->setData(dist);

        // Per-criterion bars.
        while (QLayoutItem* item = m_criteriaLayout->takeAt(0))
        {
            if (QWidget* w = item->widget())
                w->deleteLater();
            delete item;
        }
        m_criterionBars.clear();
        for (const SpeakingAnalytics::CriterionSlice& slice : snap.criteria)
        {
            auto* bar = new CriterionDistributionBar(m_criteriaContainer);
            bar->setLabel(slice.name);
            bar->setAverageText(
                slice.hasData ? SpeakingAnalytics::formatAverage(slice.average3)
                              : QStringLiteral("—")
                );
            bar->setData(slice.distribution);
            m_criteriaLayout->addWidget(bar);
            m_criterionBars.append(bar);
        }

        // Ranking table: average shown as "Letter (numeric)", then one
        // centered column per criterion.
        m_rankingTable->setRowCount(snap.rankings.size());
        // Section keeps three times the table's natural height.
        m_rankingTable->setMinimumHeight(
            3 * m_rankingTable->sizeHint().height());
        for (int r = 0; r < snap.rankings.size(); ++r)
        {
            const SpeakingAnalytics::StudentRank& rk = snap.rankings.at(r);

            auto setCentered = [this, r](int column, const QString& text) {
                auto* item = new QTableWidgetItem(text);
                item->setTextAlignment(Qt::AlignCenter);
                m_rankingTable->setItem(r, column, item);
            };

            const QString average =
                (rk.overallLetter.isEmpty()
                     ? SpeakingAnalytics::formatAverage(rk.overall3)
                     : rk.overallLetter
                         + QStringLiteral(" (")
                         + SpeakingAnalytics::formatAverage(rk.overall3)
                         + QStringLiteral(")"));
            auto* avgItem = new QTableWidgetItem(average);
            avgItem->setTextAlignment(Qt::AlignCenter);
            if (!rk.overallLetter.isEmpty())
                avgItem->setForeground(
                    AnalyticsCharts::gradeColor(rk.overallLetter));
            m_rankingTable->setItem(r, 3, avgItem);

            setCentered(0, QString::number(r + 1));
            setCentered(1, rk.englishName);
            setCentered(2, rk.koreanName);
            for (int c = 0; c < 6; ++c)
            {
                const QString letter =
                    rk.criterionLetters.size() > c
                        ? rk.criterionLetters.at(c).trimmed()
                        : QString();
                setCentered(4 + c, letter);
            }
        }
    }
    else
    {
        m_histogram->setData({});
        m_rankingTable->setRowCount(0);
        m_rankingTable->setMinimumHeight(0);
    }

    m_rebuilding = false;
}