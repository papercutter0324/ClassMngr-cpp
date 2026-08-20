#pragma once

#include <QStyledItemDelegate>

class SpeakingEvalDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit SpeakingEvalDelegate(
        QObject* parent = nullptr,
        bool readOnly = false
        );

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override;

    void setEditorData(
        QWidget* editor,
        const QModelIndex& index
        ) const override;

    void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index
        ) const override;

    bool editorEvent(
        QEvent* event,
        QAbstractItemModel* model,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) override;

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override;

    QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override;

private:
    bool m_readOnly = false;

    bool showNotesDialog(
        QAbstractItemModel* model,
        const QModelIndex& index
        ) const;
};
