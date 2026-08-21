#pragma once

#include "domain/models/classroom.h"
#include "ui/shared/pages/basepage.h"

#include <QString>

class ApplicationServices;
class AutosaveCoordinator;
class QLabel;
class PageHeader;
class QPushButton;
class SectionCard;
class QTextEdit;

class ClassNotesPage : public BasePage
{
    Q_OBJECT

public:
    explicit ClassNotesPage(
        ApplicationServices* services,
        bool embedded = false,
        QWidget* parent = nullptr
        );

    void loadClass(
        const Classroom& classroom
        );

    void clearDatabaseState() override;
    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void setSaveMode(
        SaveMode mode
        ) override;
    void refresh() override;
    void retranslateUi() override;

private slots:
    void markDirty();

private:
    void buildUi();
    bool saveClassNotesInternal(
        bool showErrorMessage
        );
    void updateHeaderText();
    void clearDirty();
    void updateActions();

private:
    ApplicationServices* m_services = nullptr;
    Classroom m_classroom;
    QString m_savedNotes;
    QString m_savedTimeFillerActivities;
    QString m_subtitleText;
    bool m_embedded = false;

    AutosaveCoordinator* m_autosave = nullptr;
    PageHeader* m_pageHeader = nullptr;
    QLabel* m_embeddedHeading = nullptr;
    SectionCard* m_notesCard = nullptr;
    QLabel* m_timeFillerActivitiesLabel = nullptr;
    QTextEdit* m_notesEdit = nullptr;
    QTextEdit* m_timeFillerActivitiesEdit = nullptr;
    QPushButton* m_saveButton = nullptr;
};
