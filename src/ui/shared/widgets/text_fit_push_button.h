#pragma once

#include <algorithm>

#include <QFontMetrics>
#include <QPushButton>

class TextFitPushButton : public QPushButton
{
public:
    using QPushButton::QPushButton;

    QSize sizeHint() const override
    {
        return fitTextWidth(
            QPushButton::sizeHint()
            );
    }

    QSize minimumSizeHint() const override
    {
        QSize hint = fitTextWidth(
            QPushButton::minimumSizeHint()
            );

        hint.setWidth(
            std::max(
                hint.width(),
                sizeHint().width()
                )
            );

        return hint;
    }

private:
    QSize fitTextWidth(
        QSize hint
        ) const
    {
        const int estimatedTextWidth =
            QFontMetrics(font()).horizontalAdvance(text())
            + HorizontalTextPadding;

        hint.setWidth(
            std::max(
                {
                    hint.width(),
                    QPushButton::sizeHint().width(),
                    estimatedTextWidth
                }
                )
            );

        return hint;
    }

    // Includes stylesheet padding, borders, and scaling headroom.
    static constexpr int HorizontalTextPadding = 48;
};
