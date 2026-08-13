#pragma once

#include "domain/models/class_info.h"

#include <QWizard>

class ApplicationServices;
class ClassService;
class SettingsService;
class TeacherService;

class InitialSetupWizard final : public QWizard
{
    Q_OBJECT

public:
    enum PageId
    {
        ResourcesPage = 0,
        TeacherImportPage,
        ScheduleImportPage,
        PersonalDetailsPage,
        TeacherEntryPage,
        ClassDetailsPage,
        ClassTimesPage,
        CompletionPage
    };

    explicit InitialSetupWizard(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    [[nodiscard]] ApplicationServices* services() const;
    [[nodiscard]] SettingsService* settingsService() const;
    [[nodiscard]] TeacherService* teacherService() const;
    [[nodiscard]] ClassService* classService() const;
    [[nodiscard]] bool wantsTeacherImport() const;
    [[nodiscard]] bool wantsScheduleImport() const;

    void setClassDraft(const ClassInfo& info);
    [[nodiscard]] ClassInfo classDraft() const;

    void setCreatedClassId(int classId);
    [[nodiscard]] int createdClassId() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refreshWizardButtonMinimumWidths();

    ApplicationServices* m_services = nullptr;
    ClassInfo m_classDraft;
    int m_createdClassId = -1;
};
