#pragma once

#include "class_analytics_ranking_header.h"
#include "core/fontmanager.h"
#include "domain/models/speaking_evaluation.h"
#include "features/classes/ui/class_analytics_charts.h"

#include <QFontMetrics>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyle>

// Read-only body renderer.  It deliberately follows SpeakingEvalDelegate's
// border treatment while using consistently sized grade badges rather than
// source-column fills.
class ClassAnalyticsRankingDelegate : public QStyledItemDelegate
{
public:
    explicit ClassAnalyticsRankingDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        if (!painter || !index.isValid())
            return;

        painter->save();

        const bool dark = option.palette.color(QPalette::Base).lightness() < 128;
        QColor background = option.palette.color(QPalette::Base);
        const bool needsAttention = index.sibling(index.row(), 3)
            .data(AnalyticsRankingRoles::NeedsAttention).toBool();
        if (needsAttention)
        {
            background = dark
                ? QColor(QStringLiteral("#3a302b"))
                : QColor(QStringLiteral("#fff7ed"));
        }
        painter->fillRect(option.rect, background);

        if (option.state & QStyle::State_Selected)
            painter->fillRect(option.rect, QColor(0, 120, 215, 60));

        const int column = index.column();
        const QString display = index.data(Qt::DisplayRole).toString();
        const QString grade = index.data(AnalyticsRankingRoles::Grade).toString();

        if (column >= 3 && !grade.isEmpty())
        {
            drawGradeCell(painter, option.rect, column, grade, display, dark);
        }
        else
        {
            painter->setFont(column == 2 ? FontManager::getKoreanFont()
                                         : FontManager::getUiFont(12));
            painter->setPen(dark ? QColor(QStringLiteral("#f5f5f5"))
                                 : QColor(QStringLiteral("#24303a")));
            const QString text = QFontMetrics(painter->font()).elidedText(
                display, Qt::ElideRight, option.rect.width() - 8);
            painter->drawText(
                option.rect.adjusted(4, 0, -4, 0), Qt::AlignCenter, text);
        }

        if (ClassAnalyticsRankingHeader::hasGroupBorderAfter(column))
        {
            QPen groupBorder(Qt::black);
            groupBorder.setWidth(2);
            groupBorder.setCosmetic(true);
            painter->setPen(groupBorder);
            painter->drawLine(option.rect.right() - 1, option.rect.top(),
                              option.rect.right() - 1, option.rect.bottom());
        }

        QPen rowBorder(Qt::black);
        rowBorder.setWidth(1);
        rowBorder.setStyle(Qt::DotLine);
        rowBorder.setCosmetic(true);
        painter->setPen(rowBorder);
        painter->drawLine(option.rect.left(), option.rect.bottom() - 1,
                          option.rect.right(), option.rect.bottom() - 1);
        painter->restore();
    }

    QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        QSize result = QStyledItemDelegate::sizeHint(option, index);
        result.setHeight(SpeakingEval::RowHeight);
        return result;
    }

private:
    static void drawGradeCell(
        QPainter* painter,
        const QRect& cell,
        int column,
        const QString& grade,
        const QString& display,
        bool dark
        )
    {
        const QSize badgeSize(
            AnalyticsRankingLayout::GradeBadgeWidth,
            AnalyticsRankingLayout::GradeBadgeHeight);
        const QFont badgeFont = FontManager::getUiFont(11, QFont::DemiBold);
        const QFont averageFont = FontManager::getUiFont(11);
        const int textWidth = column == 3 && !display.isEmpty()
            ? QFontMetrics(averageFont).horizontalAdvance(display)
            : 0;
        const int contentWidth = badgeSize.width()
            + (textWidth > 0
                   ? AnalyticsRankingLayout::GradeBadgeTextSpacing + textWidth
                   : 0);
        const int contentLeft = cell.center().x() - contentWidth / 2;
        const QRect badge(
            contentLeft,
            cell.center().y() - badgeSize.height() / 2,
            badgeSize.width(),
            badgeSize.height());
        painter->setPen(Qt::NoPen);
        painter->setBrush(AnalyticsCharts::gradeColor(grade));
        painter->drawRoundedRect(badge, 4.0, 4.0);
        painter->setFont(badgeFont);
        painter->setPen(Qt::white);
        painter->drawText(badge, Qt::AlignCenter, grade);

        if (column == 3 && !display.isEmpty())
        {
            painter->setFont(averageFont);
            painter->setPen(dark ? QColor(QStringLiteral("#f5f5f5"))
                                 : QColor(QStringLiteral("#4e5963")));
            const QRect textRect(
                badge.x() + badge.width()
                    + AnalyticsRankingLayout::GradeBadgeTextSpacing,
                cell.top(),
                textWidth,
                cell.height());
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, display);
        }
    }
};
