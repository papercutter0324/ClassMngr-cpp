#include "schedule_widget_delegates.h"
#include "core/fontmanager.h"

#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
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

void DayCellDelegate::paint(QPainter* painter,
    const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setClipRect(option.rect);
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool compact = index.data(CompactRole).toBool();
    const bool testingSlot = index.data(TestingSlotRole).toBool();
    const bool dark = option.palette.color(QPalette::Window).lightness() < 128;
    const QColor background = testingSlot ? QColor(dark ? "#4B3D20" : "#FFF0B8") : index.data(BackgroundRole).value<QColor>();
    const QColor border = testingSlot ? QColor(dark ? "#8D7339" : "#D39B25") : index.data(BorderRole).value<QColor>();
    painter->setPen(border.isValid() ? QPen(border, 1) : QPen(Qt::NoPen));
    painter->setBrush(background);
    const QRectF card = QRectF(option.rect).adjusted(0.5, 0.5, -0.5, -0.5);
    painter->drawRoundedRect(card, compact ? 5 : 6, compact ? 5 : 6);
    const QString html = FontManager::adjustRichTextPointSizes(
        index.data(HtmlRole).toString(),
        FontManager::sizeOffset() - index.data(HtmlFontOffsetRole).toInt());
    const QRectF content = card.adjusted(compact ? 3 : 4, compact ? 2.4 : 3.2,
        compact ? -3 : -4, compact ? -2.4 : -3.2);
    if (!html.isEmpty())
    {
        QTextDocument document;
        document.setDocumentMargin(0);
        document.setDefaultFont(index.data(Qt::FontRole).value<QFont>());
        document.setHtml(html);
        document.setTextWidth(content.width());
        painter->translate(content.left(), content.top() + (content.height() - document.size().height()) / 2);
        QAbstractTextDocumentLayout::PaintContext context;
        document.documentLayout()->draw(painter, context);
    }
    else
    {
        painter->setFont(index.data(Qt::FontRole).value<QFont>());
        painter->setPen(testingSlot ? QColor(dark ? "#FFF2C2" : "#4A3500") : index.data(ForegroundRole).value<QColor>());
        painter->drawText(content, Qt::AlignCenter | Qt::TextWordWrap, index.data().toString());
    }
    painter->restore();
    if (index.data(MarkerRole).toBool())
    {
        painter->save();
        painter->setClipRect(option.rect);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(testingSlot ? QColor(dark ? "#FFD166" : "#B66A00") : index.data(MarkerColorRole).value<QColor>());
        const int right = option.rect.right() + 1;
        const int top = option.rect.top();
        painter->drawPolygon(QPolygon{QPoint(right - 18, top), QPoint(right, top), QPoint(right, top + 18)});
        painter->restore();
    }
}
}
