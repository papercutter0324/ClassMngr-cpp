#pragma once

#include "ui/shared/pages/basepage.h"

#include <QList>

class ApplicationServices;
class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

enum class StaffDirectoryKind
{
    NativeEnglishTeachers,
    GsTeam
};

class StaffDirectoryPage final : public BasePage
{
    Q_OBJECT

public:
    StaffDirectoryPage(
        ApplicationServices* services,
        StaffDirectoryKind kind,
        QWidget* parent = nullptr
        );

    bool loadDirectory();

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    QString unsavedChangesTitle() const override;
    QString unsavedChangesMessage() const override;
    void setSaveMode(SaveMode mode) override;
    void refresh() override;
    void clearDatabaseState() override;
    void retranslateUi() override;

signals:
    void directorySaved();

private:
    void buildUi();
    void addRow();
    void deleteSelectedRows();
    void markDirty(bool scheduleAutosave = true);
    bool saveDirectory(bool showErrors);
    bool validateBirthday(const QString& value) const;
    void updateActions();

    ApplicationServices* m_services = nullptr;
    StaffDirectoryKind m_kind;
    SaveMode m_saveMode = SaveMode::Automatic;
    bool m_loading = false;
    bool m_dirty = false;
    QList<int> m_deletedIds;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_discardButton = nullptr;
    QTimer* m_autosaveTimer = nullptr;
};
