#pragma once

#include "features/roster/ui/roster_template_print_service.h"

#include <QDialog>
#include <QList>

class ApplicationServices;
class QButtonGroup;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class RosterPrintDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RosterPrintDialog(
        ApplicationServices* services,
        int currentClassId,
        RosterTemplatePrintService::Scope defaultScope,
        QWidget* parent = nullptr
        );

    QString templatePath() const;
    RosterTemplatePrintService::Scope selectedScope() const;
    QList<int> selectedClassIds() const;

private slots:
    void browseTemplate();
    void updateClassListEnabled();

private:
    void buildUi();
    void loadClasses();
    void retranslateUi();

    ApplicationServices* m_services = nullptr;
    int m_currentClassId = -1;
    RosterTemplatePrintService::Scope m_defaultScope =
        RosterTemplatePrintService::Scope::AllClasses;

    QLabel* m_templateHintLabel = nullptr;
    QLineEdit* m_templatePathEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QButtonGroup* m_scopeGroup = nullptr;
    QListWidget* m_classList = nullptr;
};
