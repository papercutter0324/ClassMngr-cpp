#pragma once

#include <QList>
#include <QString>
#include <QTableView>

class QMouseEvent;
class QPaintEvent;
class QEvent;
class QKeyEvent;
class QUndoStack;

struct SpeakingEvalCellEdit
{
    int row = -1;
    int column = -1;
    QString oldValue;
    QString newValue;
};

class SpeakingEvalTableView : public QTableView
{
    Q_OBJECT

public:
    explicit SpeakingEvalTableView(
        QWidget* parent = nullptr
        );

    void setUndoStack(
        QUndoStack* stack
        );

    void applyChanges(
        const QList<SpeakingEvalCellEdit>& changes,
        const QString& description
        );

public slots:
    void copy();

    void cut();

    void paste();

    void clearSelectionValues();

    void fillDown();

    void undo();

    void redo();

protected:
    bool event(
        QEvent* event
        ) override;

    bool edit(
        const QModelIndex& index,
        QAbstractItemView::EditTrigger trigger,
        QEvent* event
        ) override;

    void keyPressEvent(
        QKeyEvent* event
        ) override;

    void mousePressEvent(
        QMouseEvent* event
        ) override;

    void mouseMoveEvent(
        QMouseEvent* event
        ) override;

    void mouseReleaseEvent(
        QMouseEvent* event
        ) override;

    void paintEvent(
        QPaintEvent* event
        ) override;

private:
    void setupShortcuts();

    bool allowsImmediateTyping(
        const QModelIndex& index
        ) const;

    QList<SpeakingEvalCellEdit> changesForIndexes(
        const QModelIndexList& indexes,
        const QString& newValue
        ) const;

    bool addChangeIfValid(
        const QModelIndex& index,
        const QString& newValue,
        QList<SpeakingEvalCellEdit>& changes
        ) const;

    QString serializeSelection(
        QModelIndexList* sortedIndexes = nullptr
        ) const;

    bool isOnFillHandle(
        const QPoint& position,
        const QModelIndex& index
        ) const;

    void applyDragFill(
        const QModelIndex& start,
        const QModelIndex& end
        );

private:
    QUndoStack* m_undoStack = nullptr;
    bool m_draggingFill = false;
    QModelIndex m_fillStartIndex;
};
