#pragma once

#include "features/roster/ui/roster_template_print_service.h"

#include <QDialog>
#include <QList>

class ApplicationServices;
class QButtonGroup;
class QListWidget;

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

    RosterTemplatePrintService::Scope selectedScope() const;
    QList<int> selectedClassIds() const;

private slots:
    void updateClassListEnabled();

private:
    void buildUi();
    void loadClasses();
    void retranslateUi();

    ApplicationServices* m_services = nullptr;
    int m_currentClassId = -1;
    RosterTemplatePrintService::Scope m_defaultScope =
        RosterTemplatePrintService::Scope::AllClasses;

    QButtonGroup* m_scopeGroup = nullptr;
    QListWidget* m_classList = nullptr;
};
