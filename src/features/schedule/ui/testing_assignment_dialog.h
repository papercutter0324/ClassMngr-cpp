#pragma once

#include "domain/models/testing_block.h"
#include "ui/shared/dialogs/dialog_shell.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace ClassMngr::Next::Application
{
class ScheduleTestingClassChoicesReadPort;
}

class TestingAssignmentDialog : public DialogShell
{
    Q_OBJECT

public:
    enum class Action
    {
        SavePlainTesting,
        AssignTestingClass,
        RemoveAssignment,
        ManageTestingClasses
    };

    TestingAssignmentDialog(
        const ClassMngr::Next::Application::
            ScheduleTestingClassChoicesReadPort& choicesReadPort,
        const TestingAssignment* existingAssignment,
        QWidget* parent = nullptr
        );

    [[nodiscard]] Action selectedAction() const;
    [[nodiscard]] QString room() const;
    [[nodiscard]] int selectedClassId() const;

private:
    void buildUi();
    void loadTestingClasses();
    void updateModeUi();
    void accept() override;

    const ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadPort& m_choicesReadPort;
    bool m_hasExistingAssignment = false;
    TestingAssignment m_existingAssignment;
    Action m_action = Action::SavePlainTesting;

    QComboBox* m_modeCombo = nullptr;
    QLabel* m_roomLabel = nullptr;
    QLineEdit* m_roomEdit = nullptr;
    QLabel* m_classLabel = nullptr;
    QComboBox* m_classCombo = nullptr;
    QWidget* m_classSection = nullptr;
    QPushButton* m_manageClassesButton = nullptr;
};
