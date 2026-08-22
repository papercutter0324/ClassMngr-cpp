#pragma once

#include "domain/models/teacher.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QLabel;
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
    QComboBox* teacherSelector() const;

    void retranslateUi();

signals:
    void dataChanged();

private slots:
    void onTeacherIndexChanged(int index);

private:
    void rebuildTeacherCombos();
    const Teacher* teacherById(int teacherId) const;
    void applyTeacher(const Teacher& teacher);
    void clearTeacher();
    void applyFieldWidths();
    void updateFieldWidths();

private:
    QGridLayout* m_grid = nullptr;

    QLabel* m_teacherKrLabel = nullptr;
    QLabel* m_teacherEnLabel = nullptr;
    QLabel* m_roomLabel = nullptr;
    QLabel* m_internetTypeLabel = nullptr;
    QLabel* m_wifiNameLabel = nullptr;
    QLabel* m_wifiPasswordLabel = nullptr;
    QLabel* m_projectionTypeLabel = nullptr;
    QLabel* m_zoomIdLabel = nullptr;
    QLabel* m_zoomPasswordLabel = nullptr;

    QComboBox* m_teacherKrCombo = nullptr;
    QComboBox* m_teacherEnCombo = nullptr;

    QLineEdit* m_roomNumberEdit = nullptr;
    QLineEdit* m_wifiNameEdit = nullptr;
    QLineEdit* m_wifiPasswordEdit = nullptr;
    QLineEdit* m_internetTypeEdit = nullptr;
    QLineEdit* m_zoomIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;
    QLineEdit* m_projectionTypeEdit = nullptr;

    TeacherModel* m_model = nullptr;
    int m_selectedTeacherId = -1;
};
