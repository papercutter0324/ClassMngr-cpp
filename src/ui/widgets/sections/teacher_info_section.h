#pragma once

#include "models/teacher.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QLineEdit;
class TeacherModel;

class TeacherInfoSection : public QWidget
{
    Q_OBJECT

public:
    explicit TeacherInfoSection(QWidget* parent = nullptr);

    void setTeacherModel(TeacherModel* model);
    void setTeachers(const QList<Teacher>& teachers);
    void selectTeacher(int teacherId);

    int teacherId() const;

signals:
    void dataChanged();

private slots:
    void onTeacherIndexChanged(int index);

private:
    void rebuildTeacherCombos();
    void applyTeacher(int index);
    void clearTeacher();

private:
    QComboBox* m_teacherKrCombo = nullptr;
    QComboBox* m_teacherEnCombo = nullptr;

    QLineEdit* m_roomNumberEdit = nullptr;
    QLineEdit* m_wifiNameEdit = nullptr;
    QLineEdit* m_wifiPasswordEdit = nullptr;
    QLineEdit* m_zoomIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;

    TeacherModel* m_model = nullptr;
    int m_selectedTeacherId = -1;
};
