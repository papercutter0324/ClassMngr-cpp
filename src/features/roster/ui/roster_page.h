#pragma once

#include "ui/shared/pages/basepage.h"

#include "domain/models/classroom.h"
#include "domain/models/roster.h"

#include <QVector>

class ApplicationServices;
class QLabel;
class QModelIndex;
class QPoint;
class QPushButton;
class QTimer;
class RosterColumnLayoutController;
class RosterHeaderView;
class RosterItemDelegate;
class RosterModel;
class RosterTableView;

class RosterPage : public BasePage
{
    Q_OBJECT

public:
    explicit RosterPage(
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

    QString unsavedChangesTitle() const override;

    QString unsavedChangesMessage() const override;

    void setSaveMode(
        SaveMode mode
        ) override;
    void retranslateUi() override;

private slots:
    void addColumn();

    void removeColumn();

    void removeStudent();

    void moveStudentRow(
        int sourceRow,
        int destinationRow
        );

    void importScores();

    void printRosters();

    void autosave();

    void updateActions();

    void showRosterContextMenu(
        const QPoint& position
        );

private:
    void buildUi();

    void updateHeaderText();

    bool saveRosterInternal(
        bool showValidationMessages
        );

    bool validateRosterBeforeSave(
        bool showValidationMessages
        );

    void scheduleAutosave();

    void handleNameCellChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight
        );

    void resolveDuplicateName(
        int row,
        int editedColumn
        );

    void selectRosterCell(
        int row,
        int column
        );

    bool removeRosterRow(
        int row
        );

    void transferRosterRow(
        int row,
        int targetClassId
        );

    Roster currentRosterForSave() const;

    Roster rosterWithRowRemoved(
        int row
        ) const;

    QVector<int> normalizedColumnWidths(
        const Roster& roster,
        const QStringList& columns
        ) const;

    QString rosterRowLabel(
        int row
        ) const;

private:
    ApplicationServices* m_services = nullptr;
    Classroom m_classroom;
    bool m_loadingRoster = false;
    bool m_widthsDirty = false;
    bool m_resolvingDuplicateName = false;
    bool m_removingRosterRow = false;
    bool m_movingRosterRow = false;
    SaveMode m_saveMode = SaveMode::Automatic;
    bool m_embedded = false;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;

    RosterTableView* m_table = nullptr;
    RosterHeaderView* m_header = nullptr;
    RosterModel* m_model = nullptr;
    RosterColumnLayoutController* m_layoutController = nullptr;
    RosterItemDelegate* m_delegate = nullptr;

    QPushButton* m_importButton = nullptr;
    QPushButton* m_printButton = nullptr;
    QPushButton* m_removeStudentButton = nullptr;
    QPushButton* m_addColumnButton = nullptr;
    QPushButton* m_removeColumnButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QTimer* m_autosaveTimer = nullptr;
};
