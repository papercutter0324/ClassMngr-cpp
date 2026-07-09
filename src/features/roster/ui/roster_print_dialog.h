#pragma once

#include "features/roster/ui/roster_template_print_service.h"

#include <QDialog>
#include <QList>

class ApplicationServices;
class QButtonGroup;
class QCheckBox;
class QComboBox;
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
    QList<int> selectedClassIds() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void acceptPrint();
    void chooseSavePath();
    void updateClassListVisibility();
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
    QCheckBox* m_livePreviewCheckBox = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_previewStatusLabel = nullptr;
    QButtonGroup* m_scopeGroup = nullptr;
    QListWidget* m_classList = nullptr;
};
