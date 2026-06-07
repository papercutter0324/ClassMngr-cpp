#pragma once

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

private slots:
    void onTeacherIndexChanged(int index);

private:
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
};