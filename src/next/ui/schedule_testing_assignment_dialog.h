#pragma once

#include "next/application/schedule_testing_class_choices_query.h"

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace ClassMngr::Next::Ui
{

class ScheduleTestingAssignmentDialog final : public QDialog
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

    ScheduleTestingAssignmentDialog(
        const Application::ScheduleTestingClassChoicesSnapshot& choices,
        bool hasExistingAssignment,
        const QString& existingRoom,
        const std::optional<Domain::ClassId>& existingClassId,
        QWidget* parent = nullptr
        );

    [[nodiscard]] Action selectedAction() const noexcept;
    [[nodiscard]] QString room() const;
    [[nodiscard]] std::optional<Domain::ClassId> selectedClassId() const;

private:
    void buildUi(
        const Application::ScheduleTestingClassChoicesSnapshot& choices,
        const std::optional<Domain::ClassId>& existingClassId
        );
    void updateModeUi();
    void accept() override;

    bool m_hasExistingAssignment = false;
    Action m_action = Action::SavePlainTesting;
    QVBoxLayout* m_layout = nullptr;
    QComboBox* m_mode = nullptr;
    QLabel* m_roomLabel = nullptr;
    QLineEdit* m_room = nullptr;
    QLabel* m_classLabel = nullptr;
    QComboBox* m_class = nullptr;
    QWidget* m_classSection = nullptr;
    QPushButton* m_manageClasses = nullptr;
};

} // namespace ClassMngr::Next::Ui
