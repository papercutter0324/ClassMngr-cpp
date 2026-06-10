#pragma once

#include <algorithm>

#include <QApplication>
#include <QComboBox>
#include <QFontMetrics>
#include <QLabel>
#include <QMargins>
#include <QRect>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStringList>
#include <QWidget>

namespace WidgetSizing
{
inline int textWidth(
    const QWidget* widget,
    const QString& text
    )
{
    const QFont font =
        widget
            ? widget->font()
            : QApplication::font();

    return QFontMetrics(font).horizontalAdvance(text);
}

inline int labelMinimumWidth(
    const QLabel* label
    )
{
    if (!label)
    {
        return 0;
    }

    const QMargins margins =
        label->contentsMargins();

    return textWidth(label, label->text())
        + margins.left()
        + margins.right();
}

inline int comboChromeWidth(
    const QComboBox* combo
    )
{
    if (!combo)
    {
        return 0;
    }

    QStyleOptionComboBox option;
    option.initFrom(combo);
    option.editable = combo->isEditable();
    option.frame = combo->hasFrame();
    option.currentText = combo->currentText();
    option.rect = QRect(
        0,
        0,
        200,
        combo->sizeHint().height()
        );

    QStyle* style =
        combo->style()
            ? combo->style()
            : QApplication::style();

    const QRect editRect =
        style->subControlRect(
            QStyle::CC_ComboBox,
            &option,
            QStyle::SC_ComboBoxEditField,
            combo
            );

    if (editRect.isValid())
    {
        return option.rect.width()
            - editRect.width();
    }

    const int frameWidth =
        style->pixelMetric(
            QStyle::PM_ComboBoxFrameWidth,
            &option,
            combo
            );

    const QRect arrowRect =
        style->subControlRect(
            QStyle::CC_ComboBox,
            &option,
            QStyle::SC_ComboBoxArrow,
            combo
            );

    return frameWidth * 2
        + std::max(0, arrowRect.width());
}

inline int comboMinimumWidthForTexts(
    const QComboBox* combo,
    const QStringList& texts,
    int padding = 10
    )
{
    int maxTextWidth = 0;

    for (const QString& text : texts)
    {
        maxTextWidth =
            std::max(
                maxTextWidth,
                textWidth(combo, text)
                );
    }

    if (combo)
    {
        maxTextWidth =
            std::max(
                maxTextWidth,
                textWidth(combo, combo->currentText())
                );
    }

    return maxTextWidth
        + comboChromeWidth(combo)
        + padding;
}
}
