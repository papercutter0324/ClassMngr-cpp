#pragma once

#include "../basepage.h"

#include "models/classroom.h"

class ApplicationServices;
class QLabel;
class QModelIndex;
class QPushButton;
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
        QWidget* parent = nullptr
        );

    void loadClass(
        const Classroom& classroom
        );

    void saveData() override;

private slots:
    void addColumn();

    void removeColumn();

    void importScores();

    void updateActions();

private:
    void buildUi();

    void updateHeaderText();

    bool hasUnsavedChanges() const;

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

private:
    ApplicationServices* m_services = nullptr;
    Classroom m_classroom;
    bool m_loadingRoster = false;
    bool m_widthsDirty = false;
    bool m_resolvingDuplicateName = false;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;

    RosterTableView* m_table = nullptr;
    RosterHeaderView* m_header = nullptr;
    RosterModel* m_model = nullptr;
    RosterColumnLayoutController* m_layoutController = nullptr;
    RosterItemDelegate* m_delegate = nullptr;

    QPushButton* m_importButton = nullptr;
    QPushButton* m_addColumnButton = nullptr;
    QPushButton* m_removeColumnButton = nullptr;
    QPushButton* m_saveButton = nullptr;
};
