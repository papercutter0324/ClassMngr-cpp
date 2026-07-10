#pragma once

#include "features/roster/ui/roster_template_print_service.h"

#include <QDialog>
#include <QList>
#include <QStringList>

class ApplicationServices;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QResizeEvent;

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
    void loadClasses();
    void retranslateUi();

    ApplicationServices* m_services = nullptr;
    int m_currentClassId = -1;
    RosterTemplatePrintService::Scope m_defaultScope =
        RosterTemplatePrintService::Scope::AllClasses;
    QString m_currentClassDisplayName;
    Action m_selectedAction = Action::Print;
    QString m_selectedSavePath;

    QComboBox* m_templateCombo = nullptr;
    QGroupBox* m_extraInfoOptionsGroup = nullptr;
    QButtonGroup* m_extraInfoOrientationGroup = nullptr;
    QListWidget* m_extraColumnList = nullptr;
    QCheckBox* m_livePreviewCheckBox = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_previewStatusLabel = nullptr;
    QButtonGroup* m_scopeGroup = nullptr;
    QListWidget* m_classList = nullptr;
};
