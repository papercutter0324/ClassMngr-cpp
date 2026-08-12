#include "widget_sizing.h"

#include <algorithm>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QMargins>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStyleOptionSpinBox>
#include <QWidget>

namespace WidgetSizing
{
int textWidth(
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

int labelMinimumWidth(
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

int comboChromeWidth(
    const QComboBox* combo,
    int probeWidth
    )
{
    if (!combo)
    {
        return ComboBoxFallbackChromeWidth;
    }

    probeWidth =
        std::max(
            1,
            probeWidth
            );

    QStyleOptionComboBox option;
    option.initFrom(combo);
    option.editable = combo->isEditable();
    option.frame = combo->hasFrame();
    option.currentText = combo->currentText();
    option.rect = QRect(
        0,
        0,
        probeWidth,
        std::max(
            combo->height(),
            combo->sizeHint().height()
            )
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

    if (
        editRect.isValid()
        && editRect.width() > 0
        && editRect.width() < option.rect.width()
        )
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
        + std::max(0, arrowRect.width())
        + ComboBoxTextPadding;
}

int comboMinimumWidthForTexts(
    const QComboBox* combo,
    const QStringList& texts,
    int padding
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

void applyInitialFieldWidth(
    QWidget* widget,
    int width,
    QSizePolicy::Policy horizontalPolicy
    )
{
    if (!widget)
    {
        return;
    }

    widget->setMinimumWidth(width);
    widget->setMaximumWidth(QWIDGETSIZE_MAX);
    widget->setSizePolicy(
        horizontalPolicy,
        widget->sizePolicy().verticalPolicy()
        );
}

int lineEditMinimumWidthForText(
    const QLineEdit* edit,
    int minimumWidth,
    int padding
    )
{
    if (!edit)
    {
        return minimumWidth;
    }

    const QMargins margins =
        edit->textMargins();
    const int displayTextWidth =
        textWidth(
            edit,
            edit->displayText().trimmed()
            );

    return std::max(
        minimumWidth,
        displayTextWidth + margins.left() + margins.right() + padding
        );
}

int comboMinimumWidthForText(
    const QComboBox* combo,
    int minimumWidth,
    int padding
    )
{
    if (!combo)
    {
        return minimumWidth;
    }

    const int probeWidth =
        std::max(
            minimumWidth,
            std::max(
                combo->width(),
                200
                )
            );
    const int currentTextWidth =
        textWidth(
            combo,
            combo->currentText()
            );

    return std::max(
        minimumWidth,
        currentTextWidth
            + comboChromeWidth(combo, probeWidth)
            + padding
        );
}

int dateTimeEditChromeWidth(
    const QDateTimeEdit* edit,
    int probeWidth
    )
{
    if (!edit)
    {
        return DateTimeEditFallbackChromeWidth;
    }

    probeWidth =
        std::max(
            1,
            probeWidth
            );

    QStyleOptionSpinBox option;
    option.initFrom(edit);
    option.frame = edit->hasFrame();
    option.buttonSymbols = edit->buttonSymbols();
    option.stepEnabled =
        QAbstractSpinBox::StepUpEnabled
        | QAbstractSpinBox::StepDownEnabled;
    option.rect = QRect(
        0,
        0,
        probeWidth,
        std::max(
            edit->height(),
            edit->sizeHint().height()
            )
        );

    QStyle* style =
        edit->style()
            ? edit->style()
            : QApplication::style();

    const QRect editRect =
        style->subControlRect(
            QStyle::CC_SpinBox,
            &option,
            QStyle::SC_SpinBoxEditField,
            edit
            );

    if (
        editRect.isValid()
        && editRect.width() > 0
        && editRect.width() < option.rect.width()
        )
    {
        return option.rect.width()
            - editRect.width();
    }

    return DateTimeEditFallbackChromeWidth;
}

int dateTimeEditMinimumWidthForText(
    const QDateTimeEdit* edit,
    int minimumWidth,
    int padding
    )
{
    if (!edit)
    {
        return minimumWidth;
    }

    const int probeWidth =
        std::max(
            minimumWidth,
            std::max(
                edit->width(),
                200
                )
            );

    return std::max(
        minimumWidth,
        textWidth(
            edit,
            edit->text().trimmed()
            )
            + dateTimeEditChromeWidth(edit, probeWidth)
            + padding
        );
}

void updateTextAwareFieldWidth(
    QWidget* widget,
    int minimumWidth,
    bool lockToCalculatedWidth
    )
{
    if (!widget)
    {
        return;
    }

    int calculatedWidth = minimumWidth;

    if (auto* edit = qobject_cast<QLineEdit*>(widget))
    {
        calculatedWidth =
            lineEditMinimumWidthForText(
                edit,
                minimumWidth
                );
    }
    else if (auto* combo = qobject_cast<QComboBox*>(widget))
    {
        calculatedWidth =
            comboMinimumWidthForText(
                combo,
                minimumWidth
                );
    }
    else if (auto* dateTimeEdit = qobject_cast<QDateTimeEdit*>(widget))
    {
        calculatedWidth =
            dateTimeEditMinimumWidthForText(
                dateTimeEdit,
                minimumWidth
                );
    }

    widget->setMinimumWidth(calculatedWidth);
    widget->setMaximumWidth(
        lockToCalculatedWidth
            ? calculatedWidth
            : QWIDGETSIZE_MAX
        );
    widget->updateGeometry();
}

class TextAwareWidthEventFilter final : public QObject
{
public:
    TextAwareWidthEventFilter(
        QWidget* widget,
        int minimumWidth,
        bool lockToCalculatedWidth
        )
        : QObject(widget)
        , m_widget(widget)
        , m_minimumWidth(minimumWidth)
        , m_lockToCalculatedWidth(lockToCalculatedWidth)
    {
    }

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override
    {
        if (
            watched == m_widget
            && event
            )
        {
            switch (event->type())
            {
            case QEvent::ApplicationFontChange:
            case QEvent::FontChange:
            case QEvent::Polish:
            case QEvent::Show:
            case QEvent::StyleChange:
                updateTextAwareFieldWidth(
                    m_widget,
                    m_minimumWidth,
                    m_lockToCalculatedWidth
                    );
                break;

            default:
                break;
            }
        }

        return QObject::eventFilter(
            watched,
            event
            );
    }

private:
    QPointer<QWidget> m_widget;
    int m_minimumWidth = 0;
    bool m_lockToCalculatedWidth = false;
};

void installTextAwareFieldWidth(
    QWidget* widget,
    int minimumWidth,
    QSizePolicy::Policy horizontalPolicy,
    bool lockToCalculatedWidth
    )
{
    if (!widget)
    {
        return;
    }

    applyInitialFieldWidth(
        widget,
        minimumWidth,
        horizontalPolicy
        );
    updateTextAwareFieldWidth(
        widget,
        minimumWidth,
        lockToCalculatedWidth
        );

    auto* filter =
        new TextAwareWidthEventFilter(
            widget,
            minimumWidth,
            lockToCalculatedWidth
            );
    widget->installEventFilter(filter);

    if (auto* edit = qobject_cast<QLineEdit*>(widget))
    {
        QObject::connect(
            edit,
            &QLineEdit::textChanged,
            edit,
            [edit, minimumWidth, lockToCalculatedWidth]()
            {
                updateTextAwareFieldWidth(
                    edit,
                    minimumWidth,
                    lockToCalculatedWidth
                    );
            }
            );
    }
    else if (auto* combo = qobject_cast<QComboBox*>(widget))
    {
        QObject::connect(
            combo,
            &QComboBox::currentTextChanged,
            combo,
            [combo, minimumWidth, lockToCalculatedWidth]()
            {
                updateTextAwareFieldWidth(
                    combo,
                    minimumWidth,
                    lockToCalculatedWidth
                );
            }
            );
    }
    else if (auto* dateTimeEdit = qobject_cast<QDateTimeEdit*>(widget))
    {
        QObject::connect(
            dateTimeEdit,
            &QDateTimeEdit::dateTimeChanged,
            dateTimeEdit,
            [dateTimeEdit, minimumWidth, lockToCalculatedWidth]()
            {
                updateTextAwareFieldWidth(
                    dateTimeEdit,
                    minimumWidth,
                    lockToCalculatedWidth
                    );
            }
            );
    }
}
}
