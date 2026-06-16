#pragma once

#include <QStyledItemDelegate>

class RosterColumnLayoutController;

class RosterItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit RosterItemDelegate(
        RosterColumnLayoutController* controller,
        QObject* parent = nullptr
        );

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override;

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override;

private:
    RosterColumnLayoutController* m_controller = nullptr;
};
