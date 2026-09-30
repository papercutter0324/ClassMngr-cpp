#pragma once

#include "ui/shared/pages/basepage.h"

#include "domain/models/classroom.h"
#include "next/application/class_co_teacher_page_read_snapshot.h"

#include <optional>

class ApplicationServices;
class AutosaveCoordinator;
class PageHeader;
class ScrollablePageBody;
class TeacherInfoSection;

namespace ClassMngr::Next::Application
{
class ClassCoTeacherAssignmentPort;
class ClassCoTeacherPageReadPort;
}

class QLabel;
class QPushButton;
class SectionCard;

class ClassCoTeacherPage : public BasePage
{
    Q_OBJECT

public:
    explicit ClassCoTeacherPage(
        ApplicationServices* services,
        bool embedded = false,
        QWidget* parent = nullptr,
        ClassMngr::Next::Application::ClassCoTeacherAssignmentPort*
            assignmentPort = nullptr,
        ClassMngr::Next::Application::ClassCoTeacherPageReadPort*
            readPort = nullptr
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
    void readSelectedClassSnapshot();
    void selectCachedTeacher();
    int selectedTeacherIdFromSnapshot() const;
    void updateTitle();
    void markDirty();
    void clearDirty();
    void updateActions();
    bool saveCoTeacherInternal(
        bool showMessages
        );

private:
    ApplicationServices* m_services{nullptr};
    ClassMngr::Next::Application::ClassCoTeacherAssignmentPort*
        m_assignmentPort{nullptr};
    ClassMngr::Next::Application::ClassCoTeacherPageReadPort*
        m_readPort{nullptr};
    std::optional<
        ClassMngr::Next::Application::ClassCoTeacherPageReadSnapshot
        > m_readSnapshot;
    Classroom m_classroom;
    bool m_embedded{false};

    AutosaveCoordinator* m_autosave{nullptr};
    SectionCard* m_teacherCard{nullptr};
    TeacherInfoSection* m_teacherSection{nullptr};
    ScrollablePageBody* m_pageBody{nullptr};
    PageHeader* m_pageHeader{nullptr};
    QLabel* m_embeddedHeading{nullptr};
    QPushButton* m_saveButton{nullptr};
};
