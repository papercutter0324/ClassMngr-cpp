#pragma once

#include <QPushButton>

class TextFitPushButton : public QPushButton
{
public:
    using QPushButton::QPushButton;

    QSize sizeHint() const override
    {
        return QPushButton::sizeHint();
    }

    QSize minimumSizeHint() const override
    {
        return QPushButton::minimumSizeHint()
            .expandedTo(sizeHint())
            .expandedTo(minimumSize());
    }
};
