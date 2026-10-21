#pragma once

#include "ui/shared/pages/basepage.h"

#include "domain/models/classroom.h"
#include "next/application/class_co_teacher_page_read_snapshot.h"
#include "next/domain/operation_result.h"

#include <QHash>
#include <QString>
#include <QtGlobal>

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
class ClassCoTeacherTeacherChoicesReadPort;
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
            readPort = nullptr,
        ClassMngr::Next::Application::ClassCoTeacherTeacherChoicesReadPort*
            teacherChoicesReadPort = nullptr
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
    [[nodiscard]] bool readSelectedClassSnapshot();
    void setAssignmentReadFailure(
        const ClassMngr::Next::Domain::OperationError& error
        );
    void clearAssignmentReadFailure();
    void updateAssignmentReadStatus();
    void selectCachedTeacher();
    [[nodiscard]] qint64 selectedTeacherIdFromSnapshot() const;
    [[nodiscard]] int selectedUiTeacherIdFromSnapshot() const;
    [[nodiscard]] qint64 selectedTeacherIdFromUi() const;
    [[nodiscard]] QString selectedTeacherDisplayNameFromUi() const;
    void updateTitle();
    void markDirty();
    void clearDirty();
    void showAutomaticSaveFailureStatus();
    void clearAutomaticSaveFailureStatus();
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
    ClassMngr::Next::Application::ClassCoTeacherTeacherChoicesReadPort*
        m_teacherChoicesReadPort{nullptr};
    std::optional<
        ClassMngr::Next::Application::ClassCoTeacherPageReadSnapshot
        > m_readSnapshot;
    std::optional<ClassMngr::Next::Domain::OperationError>
        m_assignmentReadError;
    QString m_titleTeacherNameFallback;
    QHash<int, qint64> m_stableTeacherIdsByUiId;
    QHash<qint64, int> m_uiIdsByStableTeacherId;
    Classroom m_classroom;
    bool m_embedded{false};

    AutosaveCoordinator* m_autosave{nullptr};
    SectionCard* m_teacherCard{nullptr};
    TeacherInfoSection* m_teacherSection{nullptr};
    ScrollablePageBody* m_pageBody{nullptr};
    PageHeader* m_pageHeader{nullptr};
    QLabel* m_embeddedHeading{nullptr};
    QLabel* m_assignmentReadStatusLabel{nullptr};
    QLabel* m_automaticSaveFailureStatusLabel{nullptr};
    QPushButton* m_saveButton{nullptr};
    bool m_automaticSaveFailureStatusActive{false};
};
