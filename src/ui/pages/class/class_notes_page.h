#pragma once

#include "models/classroom.h"
#include "ui/pages/basepage.h"

#include <QString>

class ApplicationServices;
class QLabel;
class QPushButton;
class QTextEdit;

class ClassNotesPage : public BasePage
{
    Q_OBJECT

public:
    explicit ClassNotesPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void loadClass(
        const Classroom& classroom
        );

    void saveData() override;
    void refresh() override;

private slots:
    void markDirty();

private:
    void buildUi();
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

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QTextEdit* m_notesEdit = nullptr;
    QTextEdit* m_timeFillerActivitiesEdit = nullptr;
    QPushButton* m_saveButton = nullptr;
};
