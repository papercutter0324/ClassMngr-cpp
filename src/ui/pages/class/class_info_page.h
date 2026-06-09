#pragma once

#include "core/enums/schedule_type.h"
#include "models/class_info.h"
#include "ui/pages/basepage.h"
#include "models/classroom.h"

#include <QString>

class ApplicationServices;
class TeacherInfoSection;
class ClassDetailsSection;
class ClassScheduleSection;

class QLabel;
class QPushButton;

class ClassInfoPage : public BasePage
{
    Q_OBJECT

public:
    explicit ClassInfoPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void loadClass(
        const Classroom& classroom
        );

    void refresh() override;
    void saveData() override;

signals:
    void classInfoSaved(int classId);

private:
    void buildUi();

    void markDirty();
    void clearDirty();

    bool showScheduleConflicts(
        const QList<ClassTime>& times,
        ScheduleType type,
        const QString& title
        );

private:
    ApplicationServices* m_services{nullptr};

    Classroom m_classroom;
    QString m_notes;

    bool m_loading{false};
    bool m_dirty{false};

    TeacherInfoSection* m_teacherSection{nullptr};
    ClassDetailsSection* m_detailsSection{nullptr};
    ClassScheduleSection* m_scheduleSection{nullptr};

    QLabel* m_titleLabel{nullptr};
    QLabel* m_subtitleLabel{nullptr};

    QPushButton* m_saveButton{nullptr};
};
