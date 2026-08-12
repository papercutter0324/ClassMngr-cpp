#pragma once

#include "core/enums/schedule_type.h"
#include "domain/models/class_info.h"
#include "ui/shared/pages/basepage.h"
#include "domain/models/classroom.h"

#include <QString>

class ApplicationServices;
class AutosaveCoordinator;
class PageHeader;
class ScrollablePageBody;
class TeacherInfoSection;
class ClassDetailsSection;
class ClassScheduleSection;

class QPushButton;
class SectionCard;
class QVBoxLayout;
class QWidget;

class ClassDetailsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ClassDetailsPage(
        ApplicationServices* services,
        bool embedded = false,
        QWidget* parent = nullptr
        );

    void loadClass(
        const Classroom& classroom
        );

    void clearDatabaseState() override;
    void refresh() override;
    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void setSaveMode(
        SaveMode mode
        ) override;
    void retranslateUi() override;

signals:
    void classInfoSaved(int classId);

private:
    void buildUi();
    void updateScrollContentMinimumWidth();
    void updateTitle(
        const ClassInfo& info
        );

    void markDirty();
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

    bool m_embedded{false};
    AutosaveCoordinator* m_autosave{nullptr};

    SectionCard* m_teacherCard{nullptr};
    SectionCard* m_detailsCard{nullptr};
    SectionCard* m_scheduleCard{nullptr};

    TeacherInfoSection* m_teacherSection{nullptr};
    ClassDetailsSection* m_detailsSection{nullptr};
    ClassScheduleSection* m_scheduleSection{nullptr};

    ScrollablePageBody* m_pageBody{nullptr};
    QWidget* m_scrollContent{nullptr};
    QVBoxLayout* m_scrollContentLayout{nullptr};

    PageHeader* m_pageHeader{nullptr};

    QPushButton* m_saveButton{nullptr};
};
