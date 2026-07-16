#pragma once

#include "core/fontmanager.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"

#include <QHeaderView>
#include <QPainter>
#include <QPalette>
#include <QTableView>

class SpeakingEvalHeaderView : public QHeaderView
{
public:
    explicit SpeakingEvalHeaderView(
        Qt::Orientation orientation,
        QWidget* parent = nullptr
        )
        : QHeaderView(orientation, parent)
    {
        setDefaultAlignment(Qt::AlignCenter);
        setSectionResizeMode(QHeaderView::Fixed);
        setHighlightSections(false);
        setSectionsClickable(false);
        setFixedHeight(42);
    }

protected:
    void paintSection(
        QPainter* painter,
        const QRect& rect,
        int logicalIndex
        ) const override
    {
        if (!painter || !rect.isValid())
        {
            return;
        }

        painter->save();

        const auto column =
            SpeakingEval::columnFromInt(
                logicalIndex
                );

        const QColor baseColor =
            SpeakingEval::columnColor(column);

        const QColor headerColor =
            baseColor.darker(115);

        painter->fillRect(
            rect,
            headerColor
            );

        painter->setFont(
            FontManager::getUiFont(
                14,
                QFont::DemiBold
                )
            );

        painter->setPen(
            SpeakingEval::contrastTextColor(
                headerColor
                )
            );

        painter->drawText(
            rect.adjusted(4, 0, -4, 0),
            Qt::AlignCenter,
            model()
                ? model()
                      ->headerData(
                          logicalIndex,
                          Qt::Horizontal,
                          Qt::DisplayRole
                          )
                      .toString()
                : QString()
            );

        if (SpeakingEval::hasThickBorderAfter(column))
        {
            QPen pen(Qt::black);
            pen.setWidth(2);
            pen.setCosmetic(true);

            painter->setPen(pen);
            painter->drawLine(
                rect.right() - 1,
                rect.top(),
                rect.right() - 1,
                rect.bottom()
                );
        }

        painter->restore();
    }

    void paintEvent(
        QPaintEvent* event
        ) override
    {
        QHeaderView::paintEvent(event);

        const int rightEdge =
            contentRightEdge();

        if (rightEdge < 0)
        {
            return;
        }

        QPainter painter(viewport());

        if (rightEdge + 1 < viewport()->width())
        {
            painter.fillRect(
                QRect(
                    rightEdge + 1,
                    0,
                    viewport()->width() - rightEdge - 1,
                    viewport()->height()
                    ),
                trailingBackgroundBrush()
                );
        }

        QPen pen(Qt::black);
        pen.setWidth(2);
        pen.setCosmetic(true);

        painter.setPen(pen);
        painter.drawLine(
            0,
            height() - 1,
            rightEdge,
            height() - 1
            );
    }

private:
    int contentRightEdge() const
    {
        if (count() <= 0)
        {
            return -1;
        }

        const int lastSection =
            count() - 1;

        return sectionViewportPosition(lastSection)
            + sectionSize(lastSection)
            - 1;
    }

    QBrush trailingBackgroundBrush() const
    {
        if (
            const auto* table =
                qobject_cast<const QTableView*>(parentWidget())
            )
        {
            if (table->viewport())
            {
                return table
                    ->viewport()
                    ->palette()
                    .brush(QPalette::Base);
            }
        }

        return palette().brush(QPalette::Base);
    }
};

