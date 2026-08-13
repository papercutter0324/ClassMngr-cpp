#include "schedule_widget_delegates.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPolygon>
#include <QStyleOptionViewItem>

namespace ScheduleWidgetDelegates
{
TimeColumnDelegate::TimeColumnDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    setObjectName(TimeColumnDelegateObjectName);
}

void TimeColumnDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    if (!index.data(TimeCellRole).toBool())
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    QStyleOptionViewItem timeOption(option);
    timeOption.state &=
        ~(QStyle::State_MouseOver | QStyle::State_Selected);
    timeOption.state |= QStyle::State_HasFocus;
    QStyledItemDelegate::paint(painter, timeOption, index);
}

void CornerMarkerLabel::paintEvent(QPaintEvent* event)
{
    QLabel::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Highlight));
    constexpr int MarkerSize = 18;
    QPolygon marker;
    marker
        << QPoint(width() - MarkerSize, 0)
        << QPoint(width(), 0)
        << QPoint(width(), MarkerSize);
    painter.drawPolygon(marker);
}
}
