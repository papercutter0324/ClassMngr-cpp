#include "features/classes/ui/class_evaluations_page.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "domain/models/speaking_evaluation.h"
#include "features/classes/services/speaking_analytics.h"
#include "features/speaking_eval/ui/speaking_eval_delegate.h"
#include "features/speaking_eval/ui/speaking_eval_header_view.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>

ClassEvaluationsPage::ClassEvaluationsPage(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_embedded(embedded)
{
    Q_ASSERT(m_services);
    setProperty("role", UiRoles::SpeakingEvals);
    buildUi();
}

void ClassEvaluationsPage::buildUi()
{
    const int pageMargin = m_embedded ? 0 : UiConstants::Pages::Margin;
    contentLayout()->setContentsMargins(
        pageMargin, pageMargin, pageMargin, pageMargin);
    contentLayout()->setSpacing(14);

    auto* topBar = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(
        m_embedded ? 0 : 8,
        m_embedded ? 0 : 8,
        m_embedded ? 0 : 8,
        0);
    topLayout->setSpacing(10);

    m_heading = new QLabel(topBar);
    m_heading->setObjectName(QStringLiteral("classEvaluationsHeading"));
    m_heading->setFont(FontManager::getUiFont(18, QFont::DemiBold));
    topLayout->addWidget(m_heading);
    topLayout->addStretch();

    m_evaluationLabel = new QLabel(topBar);
    m_evaluationLabel->setObjectName(
        QStringLiteral("classEvaluationsEvaluationLabel"));
    m_evaluationLabel->setFont(FontManager::getUiFont(12));
    topLayout->addWidget(m_evaluationLabel);

    m_evaluationCombo = new QComboBox(topBar);
    m_evaluationCombo->setObjectName(
        QStringLiteral("classEvaluationsEvaluationCombo"));
    m_evaluationCombo->setMinimumWidth(170);
    for (const QString& name : SpeakingAnalytics::evaluationNames())
        m_evaluationCombo->addItem(name, name);
    topLayout->addWidget(m_evaluationCombo);
    contentLayout()->addWidget(topBar);

    m_model = new SpeakingEvalModel(this);
    m_table = new QTableView(this);
    m_table->setObjectName(QStringLiteral("classEvaluationsTable"));
    m_table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_table->setModel(m_model);
    m_table->setItemDelegate(new SpeakingEvalDelegate(m_table, true));
    m_table->setHorizontalHeader(
        new SpeakingEvalHeaderView(Qt::Horizontal, m_table));
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(SpeakingEval::RowHeight);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    contentLayout()->addWidget(m_table, 1);

    connect(
        m_evaluationCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &ClassEvaluationsPage::onEvaluationChanged);

    setupTable();
    retranslateUi();
}

void ClassEvaluationsPage::loadClass(const Classroom& classroom)
{
    m_classId = classroom.id;
    rebuild();
}

void ClassEvaluationsPage::clearDatabaseState()
{
    m_classId = -1;
    m_model->loadData(SpeakingEval::emptyRows());
}

void ClassEvaluationsPage::refresh()
{
    if (m_classId >= 0)
        rebuild();
}

void ClassEvaluationsPage::retranslateUi()
{
    m_heading->setText(tr("Speaking Evaluations"));
    m_evaluationLabel->setText(tr("Evaluation"));
}

void ClassEvaluationsPage::onEvaluationChanged()
{
    if (!m_rebuilding)
        rebuild();
}

void ClassEvaluationsPage::rebuild()
{
    m_rebuilding = true;

    SpeakingEvalRows rows;
    if (m_services && m_classId > 0)
    {
        if (const auto* service = m_services->speakingEvaluationService())
            rows = service->evaluation(m_classId, selectedEvaluationName());
    }

    m_model->loadData(rows.isEmpty() ? SpeakingEval::emptyRows() : rows);
    m_rebuilding = false;
}

void ClassEvaluationsPage::setupTable()
{
    for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
    {
        m_table->setColumnWidth(
            column,
            SpeakingEval::columnWidth(SpeakingEval::columnFromInt(column)));
    }

    for (int row = 0; row < SpeakingEval::RowCount; ++row)
        m_table->setRowHeight(row, SpeakingEval::RowHeight);
}

QString ClassEvaluationsPage::selectedEvaluationName() const
{
    return m_evaluationCombo
        ? m_evaluationCombo->currentData().toString()
        : QString();
}
