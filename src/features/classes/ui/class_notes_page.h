#pragma once

#include "domain/models/classroom.h"
#include "ui/shared/pages/basepage.h"

#include <QString>

class ApplicationServices;
class QLabel;
class QPushButton;
class SectionCard;
class QTextEdit;
class QTimer;

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
    void autosave();

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
    bool m_loading = false;
    bool m_dirty = false;
    SaveMode m_saveMode = SaveMode::Automatic;
    bool m_embedded = false;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    SectionCard* m_notesCard = nullptr;
    QLabel* m_timeFillerActivitiesLabel = nullptr;
    QTextEdit* m_notesEdit = nullptr;
    QTextEdit* m_timeFillerActivitiesEdit = nullptr;
    QPushButton* m_saveButton = nullptr;
    QTimer* m_autosaveTimer = nullptr;
};
