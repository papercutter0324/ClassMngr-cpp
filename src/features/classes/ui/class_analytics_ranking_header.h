#pragma once

#include "core/fontmanager.h"
#include "domain/models/speaking_evaluation.h"

#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QPainter>
#include <QPalette>
#include <QTableWidget>

// Header for the Analytics "Student Ranking" table.
//
// Shading and text are driven by the same palette as the Speaking Evaluations
// table (SpeakingEval::columnColor / contrastTextColor) so a given criterion
// column reads identically across both pages. Section text falls back to an
// abbreviated label when the (window-driven) section width can no longer
// display the full header.
class ClassAnalyticsRankingHeader : public QHeaderView
{
public:
    // Analytics table columns:
    //   0 #      1 English   2 Korean   3 Average   4 Grammar
    //   5 Pronunciation  6 Fluency  7 Manner  8 Content  9 Effort
    //
    // Columns map onto the Speaking Evaluations palette. The Average column
    // has no counterpart in the eval table and is intentionally left white
    // (its value is colored by grade in the body).
    [[nodiscard]] static QColor colorForColumn(int column)
    {
        switch (column)
        {
        case 0:
            return SpeakingEval::columnColor(SpeakingEvalColumn::Index);
        case 1:
        case 2:
        case 3:
            return SpeakingEval::columnColor(SpeakingEvalColumn::EnglishName);
        case 4:
            return SpeakingEval::columnColor(SpeakingEvalColumn::Grammar);
        case 5:
            return SpeakingEval::columnColor(SpeakingEvalColumn::Pronunciation);
        case 6:
            return SpeakingEval::columnColor(SpeakingEvalColumn::Fluency);
        case 7:
            return SpeakingEval::columnColor(SpeakingEvalColumn::Manner);
        case 8:
            return SpeakingEval::columnColor(SpeakingEvalColumn::Content);
        case 9:
            return SpeakingEval::columnColor(SpeakingEvalColumn::OverallEffort);
        }

        return QColor(Qt::white);
    }

    // Abbreviated header used when the full label no longer fits. The
    // fixed-width columns (#, English, Korean) have no fallback and always
    // keep their full text; the flexible columns (Average and the six
    // criteria) shorten once their section width is too small.
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

    explicit ClassAnalyticsRankingHeader(
        Qt::Orientation orientation,
        QWidget* parent = nullptr
        )
        : QHeaderView(orientation, parent)
    {
        setDefaultAlignment(Qt::AlignCenter);
        setHighlightSections(false);
        setSectionsClickable(false);
    }

    [[nodiscard]] static bool prefersAbbreviated(
        const QString& full,
        const QString& abbreviated,
        const QFont& font,
        int sectionWidth
        )
    {
        if (abbreviated.isEmpty() || full.isEmpty())
        {
            return false;
        }

        // Leave 4px of breathing room on each side.
        return QFontMetrics(font).horizontalAdvance(full) > sectionWidth - 8;
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

        const QColor baseColor = colorForColumn(logicalIndex);
        const QColor headerColor = baseColor.darker(115);

        painter->fillRect(
            rect,
            headerColor
            );

        const QFont font = FontManager::getUiFont(14, QFont::DemiBold);
        painter->setFont(font);
        painter->setPen(
            SpeakingEval::contrastTextColor(headerColor)
            );

        QString label =
            model()
                ? model()
                      ->headerData(
                          logicalIndex,
                          Qt::Horizontal,
                          Qt::DisplayRole
                          )
                      .toString()
                : QString();

        const QString shortLabel = abbreviatedLabel(logicalIndex);
        if (prefersAbbreviated(label, shortLabel, font, rect.width()))
        {
            label = shortLabel;
        }

        painter->drawText(
            rect.adjusted(4, 0, -4, 0),
            Qt::AlignCenter,
            label
            );

        painter->restore();
    }
};
