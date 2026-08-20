#pragma once

#include "core/fontmanager.h"

#include <QHeaderView>
#include <QPainter>
#include <QPalette>
#include <QTableView>

// Roles shared by the analytics page and its item delegate.  Keeping display
// text separate from grade/attention metadata makes the table read correctly
// without encoding UI decisions into the analytics service.
namespace AnalyticsRankingRoles
{
inline constexpr int Grade = Qt::UserRole + 1;
inline constexpr int NeedsAttention = Qt::UserRole + 2;
} // namespace AnalyticsRankingRoles

namespace AnalyticsRankingLayout
{
inline constexpr int GradeBadgeWidth = 38;
inline constexpr int GradeBadgeHeight = 26;
inline constexpr int GradeBadgeTextSpacing = 6;
inline constexpr int CellVerticalPadding = 10;
inline constexpr int AverageScoreLeftPadding = 5;
inline constexpr int AverageScoreRightPadding = 15;
} // namespace AnalyticsRankingLayout

// Header group boundaries separate the row number, student names and average,
// and the final rubric column.
class ClassAnalyticsRankingHeader : public QHeaderView
{
public:
    explicit ClassAnalyticsRankingHeader(
        Qt::Orientation orientation,
        QWidget* parent = nullptr
        )
        : QHeaderView(orientation, parent)
    {
        setDefaultAlignment(Qt::AlignCenter);
        setHighlightSections(false);
        setSectionsClickable(false);
        setFixedHeight(42);
    }

    [[nodiscard]] static bool hasGroupBorderAfter(int column)
    {
        return column == 0 || column == 3 || column == 9;
    }

    [[nodiscard]] static QString abbreviatedLabel(int column)
    {
        switch (column)
        {
        case 3:
            return QStringLiteral("Avg.");
        case 4:
            return QStringLiteral("Gram.");
        case 5:
            return QStringLiteral("Pron.");
        case 6:
            return QStringLiteral("Flu.");
        case 7:
            return QStringLiteral("Mann.");
        case 8:
            return QStringLiteral("Cont.");
        case 9:
            return QStringLiteral("Eff.");
        default:
            return {};
        }
    }

protected:
    void paintSection(
        QPainter* painter,
        const QRect& rect,
        int logicalIndex
        ) const override
    {
        if (!painter || !rect.isValid())
            return;

        painter->save();
        painter->fillRect(rect, palette().brush(QPalette::Base));
        painter->setFont(FontManager::getUiFont(12, QFont::DemiBold));
        painter->setPen(palette().color(QPalette::Text));

        QString label = model()
            ? model()->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole)
                  .toString()
            : QString();
        const QString abbreviated = abbreviatedLabel(logicalIndex);
        if (!abbreviated.isEmpty()
            && QFontMetrics(painter->font()).horizontalAdvance(label)
                > rect.width() - 8)
        {
            label = abbreviated;
        }
        painter->drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter, label);

        if (hasGroupBorderAfter(logicalIndex))
        {
            QPen groupBorder(Qt::black);
            groupBorder.setWidth(2);
            groupBorder.setCosmetic(true);
            painter->setPen(groupBorder);
            painter->drawLine(
                rect.right() - 1, rect.top(), rect.right() - 1, rect.bottom());
        }
        painter->restore();
    }

    void paintEvent(QPaintEvent* event) override
    {
        QHeaderView::paintEvent(event);

        if (count() == 0)
            return;

        const int rightEdge = sectionViewportPosition(count() - 1)
            + sectionSize(count() - 1) - 1;
        QPainter painter(viewport());
        if (rightEdge + 1 < viewport()->width())
        {
            painter.fillRect(
                QRect(rightEdge + 1, 0, viewport()->width() - rightEdge - 1,
                      viewport()->height()),
                palette().brush(QPalette::Base));
        }

        QPen bottomBorder(Qt::black);
        bottomBorder.setWidth(2);
        bottomBorder.setCosmetic(true);
        painter.setPen(bottomBorder);
        painter.drawLine(0, height() - 1, qMax(0, rightEdge), height() - 1);
    }
};
