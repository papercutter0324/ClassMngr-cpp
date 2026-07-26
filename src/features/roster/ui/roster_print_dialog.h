#pragma once

#include "features/roster/services/roster_template_print_service.h"

#include <QDialog>
#include <QList>
#include <QStringList>

class ApplicationServices;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QListWidget;
class QResizeEvent;
class QScrollArea;
class QVBoxLayout;
class QWidget;

class RosterPrintDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Action
    {
        Print,
        SaveAs
    };

    explicit RosterPrintDialog(
        ApplicationServices* services,
        int currentClassId,
        RosterTemplatePrintService::Scope defaultScope,
        QWidget* parent = nullptr
        );

    Action selectedAction() const;
    QString selectedSavePath() const;
    RosterTemplatePrintService::Scope selectedScope() const;
    RosterTemplatePrintService::TemplateId selectedTemplateId() const;
    QStringList selectedExtraColumns() const;
    QPageLayout::Orientation selectedPerClassExtraInfoOrientation() const;
    QList<int> selectedClassIds() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void acceptPrint();
    void chooseSavePath();
    void updateClassListVisibility();
    void updateTemplateOptionsVisibility();
    void updateExtraInfoColumns();
    void updateExtraInfoSelectionLimits();
    void updatePreview();

private:
    void buildUi();
    void captureFinalSelection();
    void loadClasses();
    void retranslateUi();
    void resizeForExtraInfoOptions();
    void updateMinimumWidthForCurrentClassName();

    ApplicationServices* m_services = nullptr;
    int m_currentClassId = -1;
    RosterTemplatePrintService::Scope m_defaultScope =
        RosterTemplatePrintService::Scope::AllClasses;
    QString m_currentClassDisplayName;
    Action m_selectedAction = Action::Print;
    QString m_selectedSavePath;
    bool m_hasFinalSelection = false;
    RosterTemplatePrintService::TemplateId m_finalTemplateId =
        RosterTemplatePrintService::TemplateId::ByDay;
    QStringList m_finalExtraColumns;
    QPageLayout::Orientation m_finalPerClassExtraInfoOrientation =
        QPageLayout::Portrait;

    QComboBox* m_templateCombo = nullptr;
    QWidget* m_extraInfoOptionsGroup = nullptr;
    QLabel* m_pageLayoutLabel = nullptr;
    QLabel* m_extraInfoColumnsLabel = nullptr;
    QLabel* m_extraInfoSelectionCountLabel = nullptr;
    QLabel* m_rosterPreviewLabel = nullptr;
    QButtonGroup* m_extraInfoOrientationGroup = nullptr;
    QGridLayout* m_extraColumnGridLayout = nullptr;
    QScrollArea* m_extraColumnScrollArea = nullptr;
    QWidget* m_extraColumnOptionsWidget = nullptr;
    QScrollArea* m_contentScrollArea = nullptr;
    QWidget* m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QList<QCheckBox*> m_extraColumnChecks;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_previewStatusLabel = nullptr;
    QButtonGroup* m_scopeGroup = nullptr;
    QListWidget* m_classList = nullptr;
    int m_classListBaseHeight = 0;
    int m_baseMinimumWidth = 0;
    int m_baseMinimumHeight = 0;
};
