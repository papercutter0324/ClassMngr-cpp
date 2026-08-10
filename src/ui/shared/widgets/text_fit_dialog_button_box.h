#pragma once

#include <QDialogButtonBox>
#include <QEvent>
#include <QPushButton>

class TextFitDialogButtonBox : public QDialogButtonBox
{
public:
    using QDialogButtonBox::QDialogButtonBox;

protected:
    bool event(QEvent* event) override
    {
        const bool handled = QDialogButtonBox::event(event);

        switch (event->type())
        {
        case QEvent::Polish:
        case QEvent::Show:
        case QEvent::FontChange:
        case QEvent::StyleChange:
        case QEvent::LanguageChange:
        case QEvent::LayoutRequest:
            updateButtonMinimumWidths();
            break;
        default:
            break;
        }

        return handled;
    }

private:
    void updateButtonMinimumWidths()
    {
        for (QAbstractButton* abstractButton : buttons())
        {
            auto* button = qobject_cast<QPushButton*>(abstractButton);

            if (!button || button->text().isEmpty())
            {
                continue;
            }

            button->setMinimumWidth(
                qMax(button->minimumWidth(), button->sizeHint().width())
                );
        }
    }
};
