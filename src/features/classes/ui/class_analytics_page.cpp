#include "features/classes/ui/class_analytics_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "app/services/feature_services.h"
#include "features/classes/ui/class_analytics_charts.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/pages/scrollable_page_body.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayoutItem>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{

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

    auto* heading = new QLabel(tr("Speaking analytics"), topBar);
    heading->setFont(FontManager::getUiFont(18));
    heading->setStyleSheet(QStringLiteral("color:#1f2937;"));
    topLayout->addWidget(heading);
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
    statLayout->setSpacing(12);

    auto addStatCard = [this, statRow, statLayout](const QString& title) -> QLabel* {
        auto* card = new SectionCard(title, statRow);
        auto* cl = card->contentLayout();
        cl->setContentsMargins(14, 2, 14, 14);
        auto* value = new QLabel(QStringLiteral("—"), card);
        value->setFont(FontManager::getUiFont(20));
        value->setStyleSheet(QStringLiteral("color:#1f2937;"));
        value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        value->setMinimumHeight(34);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(value);
        statLayout->addWidget(card, 1);
        return value;
    };

    {
        auto* card = new SectionCard(tr("Class average"), statRow);
        auto* cl = card->contentLayout();
        cl->setContentsMargins(14, 2, 14, 14);
        m_avgValue = new QLabel(QStringLiteral("—"), card);
        m_avgValue->setFont(FontManager::getUiFont(26));
        m_avgValue->setStyleSheet(QStringLiteral("color:#1f2937;"));
        m_avgValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(m_avgValue);
        m_avgLetter = new QLabel(QString(), card);
        m_avgLetter->setFont(FontManager::getUiFont(13));
        cl->addWidget(m_avgLetter);
        statLayout->addWidget(card, 1);
    }

    m_assessedValue = addStatCard(tr("Assessed"));
    m_strongestValue = addStatCard(tr("Strongest area"));
    m_focusValue = addStatCard(tr("Focus area"));
    scroll->addWidget(statRow);

    // "Class shape" histogram.
    auto* shapeCard = new SectionCard(tr("Class shape"), this);
    auto* shapeLayout = shapeCard->contentLayout();
    shapeLayout->setContentsMargins(14, 6, 14, 14);
    m_histogram = new GradeHistogram(shapeCard);
    shapeLayout->addWidget(m_histogram);
    scroll->addWidget(shapeCard, 0, Qt::AlignTop);

    // Per-criterion stacked bars.
    auto* criteriaCard = new SectionCard(tr("By criterion"), this);
    auto* criteriaOuter = criteriaCard->contentLayout();
    criteriaOuter->setContentsMargins(14, 6, 14, 14);
    m_criteriaContainer = new QWidget(criteriaCard);
    m_criteriaLayout = new QVBoxLayout(m_criteriaContainer);
    m_criteriaLayout->setContentsMargins(0, 0, 0, 0);
    m_criteriaLayout->setSpacing(10);
    criteriaOuter->addWidget(m_criteriaContainer);
    scroll->addWidget(criteriaCard, 0, Qt::AlignTop);

    // Student ranking table.
    auto* rankCard = new SectionCard(tr("Student ranking"), this);
    auto* rankLayout = rankCard->contentLayout();
    rankLayout->setContentsMargins(14, 6, 14, 14);
    m_rankingTable = new QTableWidget(0, 5, rankCard);
    m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rankingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rankingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rankingTable->verticalHeader()->setVisible(false);
    m_rankingTable->horizontalHeader()->setHighlightSections(false);
    m_rankingTable->horizontalHeader()->setStretchLastSection(true);
    m_rankingTable->setHorizontalHeaderLabels(
        { tr("#"), tr("English"), tr("Korean"), tr("Average"), tr("Grade") }
        );
    m_rankingTable->setColumnWidth(0, 40);
    rankLayout->addWidget(m_rankingTable);
    scroll->addWidget(rankCard, 0, Qt::AlignTop);

    // Empty state.
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color:#8A8F98; padding:24px;"));
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->hide();
    scroll->addWidget(m_emptyLabel);

    scroll->addStretch(1);
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

        // Ranking table.
        m_rankingTable->setRowCount(snap.rankings.size());
        for (int r = 0; r < snap.rankings.size(); ++r)
        {
            const SpeakingAnalytics::StudentRank& rk = snap.rankings.at(r);
            auto* gradeItem = new QTableWidgetItem(rk.overallLetter);
            gradeItem->setForeground(AnalyticsCharts::gradeColor(rk.overallLetter));
            m_rankingTable->setItem(r, 0, new QTableWidgetItem(QString::number(r + 1)));
            m_rankingTable->setItem(r, 1, new QTableWidgetItem(rk.englishName));
            m_rankingTable->setItem(r, 2, new QTableWidgetItem(rk.koreanName));
            m_rankingTable->setItem(
                r, 3,
                new QTableWidgetItem(SpeakingAnalytics::formatAverage(rk.overall3)));
            m_rankingTable->setItem(r, 4, gradeItem);
        }
    }
    else
    {
        m_histogram->setData({});
        m_rankingTable->setRowCount(0);
    }

    m_rebuilding = false;
}