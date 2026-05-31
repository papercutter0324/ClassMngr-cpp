#pragma once

#include "../basepage.h"
#include "models/teacher.h"

class QScrollArea;
class QLabel;
class QLineEdit;
class QTextEdit;
class QPushButton;

class ApplicationServices;

class TeacherInfoPage : public BasePage
{
    Q_OBJECT

public:

    explicit TeacherInfoPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void loadTeacher(const Teacher& teacher);

    void saveData() override;
    void refresh() override;

    QVariantMap getTeacherData() const;

private slots:
    void saveTeacher();

private:
    void buildUi();
    QLabel* createFieldLabel(const QString& text);

private:

    // =====================================================
    // Services
    // =====================================================

    ApplicationServices* m_services = nullptr;




    Teacher m_teacher;

    QScrollArea* m_scroll;

    QLabel* m_titleLabel;
    QLabel* m_subtitleLabel;

    QLineEdit* m_teacherKrEdit;
    QLineEdit* m_teacherEnEdit;
    QLineEdit* m_roomNumberEdit;

    QLineEdit* m_wifiNameEdit;
    QLineEdit* m_wifiPasswordEdit;

    QLineEdit* m_zoomIdEdit;
    QLineEdit* m_zoomPasswordEdit;

    QTextEdit* m_notesEdit;

    QPushButton* m_saveButton;
};