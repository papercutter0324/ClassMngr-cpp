#pragma once

#include "core/enums/schedule_type.h"
#include "domain/models/class_info.h"
#include "ui/shared/pages/basepage.h"
#include "domain/models/classroom.h"

#include <QString>

class ApplicationServices;
class TeacherInfoSection;
class ClassDetailsSection;
class ClassScheduleSection;

class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class QWidget;

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
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void setSaveMode(
        SaveMode mode
        ) override;

signals:
    void classInfoSaved(int classId);

private:
    void buildUi();
    void updateScrollContentMinimumWidth();
    void updateTitle(
        const ClassInfo& info
        );

    void markDirty();
    void autosave();
    void clearDirty();
    void updateActions();
    bool saveClassInfoInternal(
        bool showMessages
        );

    bool showScheduleConflicts(
        const QList<ClassTime>& times,
        ScheduleType type,
        const QString& title,
        bool showMessage
        );

private:
    ApplicationServices* m_services{nullptr};

    Classroom m_classroom;

    bool m_loading{false};
    bool m_dirty{false};
    SaveMode m_saveMode{SaveMode::Automatic};

    TeacherInfoSection* m_teacherSection{nullptr};
    ClassDetailsSection* m_detailsSection{nullptr};
    ClassScheduleSection* m_scheduleSection{nullptr};

    QScrollArea* m_scrollArea{nullptr};
    QWidget* m_scrollContent{nullptr};
    QVBoxLayout* m_scrollContentLayout{nullptr};

    QLabel* m_titleLabel{nullptr};
    QLabel* m_subtitleLabel{nullptr};

    QPushButton* m_saveButton{nullptr};
    QTimer* m_autosaveTimer{nullptr};
};
