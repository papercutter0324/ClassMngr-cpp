#pragma once

#include "features/classes/models/class_tab_navigation_model.h"

#include <QDialog>

class QRadioButton;
class QWidget;

struct ClassesNavigationSettingsValues
{
    ClassTabNavigation::VisibilityScope visibilityScope =
        ClassTabNavigation::VisibilityScope::ActiveSchedule;
};

class ClassesNavigationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClassesNavigationSettingsDialog(
        const ClassesNavigationSettingsValues& values,
        QWidget* parent = nullptr
        );

    [[nodiscard]] ClassesNavigationSettingsValues values() const;

private:
    void buildUi();
    QWidget* buildDisplayTab();

    ClassesNavigationSettingsValues m_initialValues;
    QRadioButton* m_allClassesRadio = nullptr;
    QRadioButton* m_activeScheduleRadio = nullptr;
};
