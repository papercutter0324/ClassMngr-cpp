#pragma once

#include "ui/shared/pages/basepage.h"

#include "domain/models/classroom.h"

class ApplicationServices;
class AutosaveCoordinator;
class PageHeader;
class ScrollablePageBody;
class TeacherInfoSection;

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
    void updateTitle();
    void markDirty();
    void clearDirty();
    void updateActions();
    bool saveCoTeacherInternal(
        bool showMessages
        );

private:
    ApplicationServices* m_services{nullptr};
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
