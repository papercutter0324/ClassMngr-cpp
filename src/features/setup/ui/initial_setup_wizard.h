#pragma once

#include "domain/models/class_info.h"

#include <QWizard>

class ApplicationServices;
class DataService;

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
    [[nodiscard]] DataService* dataService() const;
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
