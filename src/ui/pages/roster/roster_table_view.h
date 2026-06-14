#pragma once

#include <QTableView>

class QEvent;
class RosterColumnLayoutController;

class RosterTableView : public QTableView
{
    Q_OBJECT

public:
    explicit RosterTableView(
        QWidget* parent = nullptr
        );

    void setLayoutController(
        RosterColumnLayoutController* controller
        );

    int contentBottomEdge() const;

public slots:
    void copy();

    void cut();

    void paste();

protected:
    void changeEvent(
        QEvent* event
        ) override;

    void paintEvent(
        QPaintEvent* event
        ) override;

private:
    void setupShortcuts();

    bool isEditableIndex(
        const QModelIndex& index
        ) const;

    QString serializeSelection(
        QModelIndexList* sortedIndexes = nullptr
        ) const;

    void updateVerticalHeaderTrailingBackground();

private:
    RosterColumnLayoutController* m_controller = nullptr;
};
