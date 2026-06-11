#include "roster_item_delegate.h"

#include "core/fontmanager.h"
#include "ui/pages/roster/roster_column_layout_controller.h"
#include "ui/pages/roster/roster_constants.h"
#include "ui/pages/roster/roster_model.h"

#include <QApplication>
#include <QLineEdit>
#include <QPainter>

RosterItemDelegate::RosterItemDelegate(
    RosterColumnLayoutController* controller,
    QObject* parent
    )
    : QStyledItemDelegate(parent)
    , m_controller(controller)
{
}

QWidget* RosterItemDelegate::createEditor(
    QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    QWidget* editor =
        QStyledItemDelegate::createEditor(
            parent,
            option,
            index
            );

    if (auto* lineEdit = qobject_cast<QLineEdit*>(editor))
    {
        lineEdit->setAlignment(Qt::AlignCenter);
    }

    return editor;
}

void RosterItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    painter->save();

    QString group =
        RosterUi::studentInformationGroup();

    if (m_controller)
    {
        group =
            m_controller->columnGroup(
                index.column()
                );
    }

    const QColor baseColor =
        RosterUi::groupColor(group);

    const QColor background =
        RosterUi::soften(
            baseColor,
            group == RosterUi::studentInformationGroup()
                ? 0.78
                : 0.84
            );

    if (option.state & QStyle::State_Selected)
    {
        painter->fillRect(
            option.rect,
            option.palette.highlight()
            );
    }
    else
    {
        painter->fillRect(
            option.rect,
            background
            );
    }

    painter->setFont(
        FontManager::getUiFont(11)
        );

    const QString text =
        index.data(Qt::DisplayRole).toString();

    const QFontMetrics metrics(
        painter->font()
        );

    const QString elidedText =
        metrics.elidedText(
            text,
            Qt::ElideRight,
            option.rect.width() - 12
            );

    painter->setPen(
        option.state & QStyle::State_Selected
            ? option.palette.highlightedText().color()
            : option.palette.text().color()
        );

    painter->drawText(
        option.rect.adjusted(6, 0, -6, 0),
        Qt::AlignCenter,
        elidedText
        );

    const auto* rosterModel =
        qobject_cast<const RosterModel*>(
            index.model()
            );

    if (
        rosterModel
        && !rosterModel->errorsForCell(
            index.row(),
            index.column()
            ).isEmpty()
        )
    {
        painter->setPen(
            QPen(QColor(220, 38, 38), 1)
            );

        painter->drawRect(
            option.rect.adjusted(1, 1, -2, -2)
            );
    }

    QPen rowLine(
        QColor(156, 163, 175),
        1,
        Qt::DotLine
        );

    painter->setPen(rowLine);
    painter->drawLine(
        option.rect.left(),
        option.rect.bottom(),
        option.rect.right(),
        option.rect.bottom()
        );

    painter->setPen(
        QColor(209, 213, 219)
        );

    painter->drawLine(
        option.rect.right(),
        option.rect.top(),
        option.rect.right(),
        option.rect.bottom()
        );

    painter->restore();
}
