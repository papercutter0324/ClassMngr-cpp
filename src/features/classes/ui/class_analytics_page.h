#pragma once

#include "domain/models/classroom.h"
#include "features/classes/services/speaking_analytics.h"
#include "ui/shared/pages/basepage.h"

#include <QList>
#include <QString>

class ApplicationServices;
class CriterionDistributionBar;
class GradeHistogram;
class PageHeader;
class SectionCard;
class QComboBox;
class QLabel;
class QTableWidget;
class QVBoxLayout;

// Read-only "Analytics" editor for a Class: speaking-evaluation statistics,
// grade distributions, per-criterion breakdowns and student rankings.
//
// Computed entirely from existing data via SpeakingEvaluationService::
// analytics(); it never writes to the database.
class ClassAnalyticsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ClassAnalyticsPage(
        ApplicationServices* services,
        bool embedded = false,
        QWidget* parent = nullptr
        );

    void loadClass(
        const Classroom& classroom
        );

    void clearDatabaseState() override;
    void refresh() override;
    void retranslateUi() override;

    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onEvaluationChanged();
    void syncThemeStyles();

private:
    void buildUi();
    void rebuild();
    void showEmpty(bool empty);
    void applyChartsRowLayout();

    // Stat-card sizing: every card is as wide as the widest title label
    // ("Students Assessed").  Recomputed when the font changes so the
    // cards stay proportional at any font size.  statValueWidth() reports
    // the width available for value text; applyAreaValue() fills the
    // strongest/focus cards (pluralizing the title and wrapping one
    // metric per line).
    void layoutStatCards();
    int statValueWidth() const;
    void applyAreaValue(
        SectionCard* card,
        const QString& singularTitle,
        const QString& pluralTitle,
        const QList<QString>& names,
        const QList<QString>& labels,
        QLabel* value
        );

    ApplicationServices* m_services = nullptr;
    bool m_embedded = false;
    int m_classId = -1;

    PageHeader* m_header = nullptr;
    QLabel* m_heading = nullptr;
    QComboBox* m_evaluationCombo = nullptr;

    // Stat-card value labels (populated by rebuild()).
    QLabel* m_avgValue = nullptr;
    QLabel* m_avgLetter = nullptr;
    QLabel* m_assessedValue = nullptr;
    QLabel* m_strongestValue = nullptr;
    QLabel* m_focusValue = nullptr;

    // Stat-card container and the card/title pieces it depends on.  The
    // card widths are derived from the widest title (e.g. "Students
    // Assessed") and recomputed on font changes (see layoutStatCards()).
    QWidget* m_statRow = nullptr;
    QLabel* m_statTitleLabel = nullptr;
    SectionCard* m_avgCard = nullptr;
    SectionCard* m_assessedCard = nullptr;
    SectionCard* m_strongestCard = nullptr;
    SectionCard* m_focusCard = nullptr;

    QWidget* m_chartsRow = nullptr;
    SectionCard* m_shapeCard = nullptr;
    SectionCard* m_criteriaCard = nullptr;
    bool m_chartsRowHorizontal = false;

    GradeHistogram* m_histogram = nullptr;
    QWidget* m_criteriaContainer = nullptr;
    QVBoxLayout* m_criteriaLayout = nullptr;
    QList<CriterionDistributionBar*> m_criterionBars;

    QTableWidget* m_rankingTable = nullptr;
    QLabel* m_emptyLabel = nullptr;

    bool m_rebuilding = false;
};