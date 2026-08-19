#pragma once

#include "class_analytics_ranking_header.h"
#include "core/fontmanager.h"

#include <QFontMetrics>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyle>

// Body-cell delegate for the Analytics "Student Ranking" table.
//
// The app-wide theme stylesheet (dark.qss / light.qss) contains
// QTableView::item / QTableWidget::item rules; as soon as a stylesheet
// covers a view, QStyleSheetStyle takes over item rendering and
// per-item background brushes (QTableWidgetItem::setBackground) are no
// longer painted. The Speaking Evaluations table sidesteps the same
// issue by drawing its cell backgrounds inside its own delegate
// (SpeakingEvalDelegate::paint). This delegate mirrors that behaviour
// so the ranking body is shaded exactly like the eval table: same
// per-column palette, contrast text, elided centered text, thick
// section borders and dotted row separators. The Average column keeps
// its per-grade text color, read from the item's foreground brush.
class ClassAnalyticsRankingDelegate : public QStyledItemDelegate
{
public:
    explicit ClassAnalyticsRankingDelegate(
        QObject* parent = nullptr
        )
        : QStyledItemDelegate(parent)
    {
    }

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        if (!index.isValid())
        {
            return;
        }

        const int column = index.column();

        const QColor background =
            ClassAnalyticsRankingHeader::colorForColumn(
                column
                );

        painter->save();

        painter->fillRect(
            option.rect,
            background
            );

        if (option.state & QStyle::State_Selected)
        {
            painter->fillRect(
                option.rect,
                QColor(0, 120, 215, 60)
                );
        }

        // Same fonts as the eval table body: the Korean font for the
        // Korean name column, the standard 12pt UI font elsewhere.
        const QFont font =
            column == 2
                ? FontManager::getKoreanFont()
                : FontManager::getUiFont(12);
        painter->setFont(font);

        // Prefer the item's own text color (the Average column carries a
        // per-grade color); otherwise use the palette contrast color.
        const QBrush itemForeground =
            index.data(
                Qt::ForegroundRole
                ).value<QBrush>();
        const QColor textColor =
            itemForeground.style() != Qt::NoBrush
                ? itemForeground.color()
                : SpeakingEval::contrastTextColor(background);

        painter->setPen(textColor);

        const QString text =
            index.data(
                Qt::DisplayRole
                ).toString();

        const QFontMetrics metrics(
            painter->font()
            );

        const QString elidedText =
            metrics.elidedText(
                text,
                Qt::ElideRight,
                option.rect.width() - 8
                );

        painter->drawText(
            option.rect.adjusted(4, 0, -4, 0),
            Qt::AlignCenter,
            elidedText
            );

        // Thick section borders in the same places as the eval table:
        // after the index, after the Korean name and after the last
        // criterion column.
        if (
            column == 0
            || column == 2
            || column == 9
            )
        {
            QPen pen(Qt::black);
            pen.setWidth(2);
            pen.setCosmetic(true);

            painter->setPen(pen);
            painter->drawLine(
                option.rect.right() - 1,
                option.rect.top(),
                option.rect.right() - 1,
                option.rect.bottom()
                );
        }

        QPen rowPen(Qt::black);
        rowPen.setWidth(1);
        rowPen.setStyle(Qt::DotLine);
        rowPen.setCosmetic(true);

        painter->setPen(rowPen);
        painter->drawLine(
            option.rect.left(),
            option.rect.bottom() - 1,
            option.rect.right(),
            option.rect.bottom() - 1
            );

        painter->restore();
    }
};