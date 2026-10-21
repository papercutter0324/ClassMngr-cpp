#pragma once

#include "ui/shared/pages/basepage.h"
#include "next/domain/gs_team_member_id.h"
#include "next/domain/native_english_teacher_id.h"

#include <QList>

class ApplicationServices;
class QAbstractItemModel;
class QLabel;
class QPushButton;
class QTableView;
class QTimer;
class OnScreenKeyboard;

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
    bool refreshAfterTeacherImport(
        bool importChangedRecords,
        QString* readError = nullptr
        );
    void failNextPostSaveDirectoryReadForTesting();
    void failNextImportRefreshDirectoryReadForTesting();

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

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void directorySaved();

private:
    void buildUi();
    void addRow();
    void deleteSelectedRows();
    void markDirty(bool scheduleAutosave = true);
    bool saveDirectory(bool showErrors);
    void showAutosaveFailureStatus();
    void clearAutosaveFailureStatus();
    void reloadAfterSuccessfulSave();
    bool loadDirectoryImpl(bool importRefresh, QString* readError);
    void reportDirectoryLoadFailure(
        const QString& message,
        bool importRefresh = false
        );
    void updateActions();
    void updateTableMetrics();

    ApplicationServices* m_services = nullptr;
    StaffDirectoryKind m_kind;
    SaveMode m_saveMode = SaveMode::Automatic;
    bool m_loading = false;
    bool m_dirty = false;
    bool m_reloadRequired = false;
    bool m_autosaveFailureStatusActive = false;
    enum class ReloadReason
    {
        None,
        Save,
        Import
    };
    ReloadReason m_reloadReason = ReloadReason::None;
    bool m_failNextPostSaveDirectoryReadForTesting = false;
    bool m_failNextImportRefreshDirectoryReadForTesting = false;
    QList<ClassMngr::Next::Domain::GsTeamMemberId> m_deletedGsTeamMemberIds;
    QList<ClassMngr::Next::Domain::NativeEnglishTeacherId>
        m_deletedNativeEnglishTeacherIds;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_autosaveFailureStatusLabel = nullptr;
    QPushButton* m_koreanKeyboardButton = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;
    QTableView* m_table = nullptr;
    QAbstractItemModel* m_model = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_discardButton = nullptr;
    QTimer* m_autosaveTimer = nullptr;
};
