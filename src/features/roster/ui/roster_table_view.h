#pragma once

#include <QTableView>

class QEvent;
class QKeyEvent;
class QPainter;
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

    void requestRowMove(
        int sourceRow,
        int destinationRow
        );

    void setDraggedSourceRow(
        int row
        );

    int draggedSourceRow() const;

    void setDraggedDestinationRow(
        int row
        );

    int draggedDestinationRow() const;

    int contentBottomEdge() const;

signals:
    void rowMoveRequested(
        int sourceRow,
        int destinationRow
        );

public slots:
    void copy();

    void cut();

    void paste();

    void clearSelectionValues();

protected:
    bool event(
        QEvent* event
        ) override;

    void keyPressEvent(
        QKeyEvent* event
        ) override;

    void changeEvent(
        QEvent* event
        ) override;

    void paintEvent(
        QPaintEvent* event
        ) override;

private:
    bool isEditableIndex(
        const QModelIndex& index
        ) const;

    QString serializeSelection(
        QModelIndexList* sortedIndexes = nullptr
        ) const;

    void updateVerticalHeaderTrailingBackground();

    void updateRowIndicator(
        int row
        );

    void paintDraggedSourceRow(
        QPainter& painter
        ) const;

    void paintDraggedDestinationRow(
        QPainter& painter
        ) const;

private:
    RosterColumnLayoutController* m_controller = nullptr;
    int m_draggedSourceRow = -1;
    int m_draggedDestinationRow = -1;
};
