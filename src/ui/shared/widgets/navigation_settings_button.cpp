#include "navigation_settings_button.h"

#include "core/fontmanager.h"
#include "ui/shared/widgets/navigation_pill_style.h"

#include <algorithm>

#include <QFocusEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>

namespace
{
constexpr int SettingsButtonWidth = 46;
constexpr qreal ControlRadius = 6.0;
constexpr qreal FocusWidth = 2.0;
}

NavigationSettingsButton::NavigationSettingsButton(QWidget* parent)
    : QToolButton(parent)
{
    setObjectName(QStringLiteral("navigationSettingsButton"));
    setText(QStringLiteral("\u2699"));
    setFont(FontManager::getUiFont(18, QFont::DemiBold));
    setFixedWidth(SettingsButtonWidth);
    setMinimumHeight(NavigationPillStyle::ControlHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
}

QColor NavigationSettingsButton::navigationSettingsHoverColor() const
{
    return m_hoverColor;
}

void NavigationSettingsButton::setNavigationSettingsHoverColor(
    const QColor& color
    )
{
    m_hoverColor = color;
    update();
}

QColor NavigationSettingsButton::navigationSettingsPressedColor() const
{
    return m_pressedColor;
}

void NavigationSettingsButton::setNavigationSettingsPressedColor(
    const QColor& color
    )
{
    m_pressedColor = color;
    update();
}

QColor NavigationSettingsButton::navigationSettingsTextColor() const
{
    return m_textColor;
}

void NavigationSettingsButton::setNavigationSettingsTextColor(
    const QColor& color
    )
{
    m_textColor = color;
    update();
}

QColor NavigationSettingsButton::navigationSettingsFocusColor() const
{
    return m_focusColor;
}

void NavigationSettingsButton::setNavigationSettingsFocusColor(
    const QColor& color
    )
{
    m_focusColor = color;
    update();
}

QSize NavigationSettingsButton::sizeHint() const
{
    const int requiredHeight =
        fontMetrics().height()
        + (2 * NavigationPillStyle::VerticalPadding)
        + 2;

    return QSize(
        SettingsButtonWidth,
        std::max(
            requiredHeight,
            NavigationPillStyle::ControlHeight
            )
        );
}

QSize NavigationSettingsButton::minimumSizeHint() const
{
    return sizeHint();
}

void NavigationSettingsButton::focusInEvent(QFocusEvent* event)
{
    QToolButton::focusInEvent(event);

    m_keyboardFocus =
        event
        && (
            event->reason() == Qt::TabFocusReason
            || event->reason() == Qt::BacktabFocusReason
            || event->reason() == Qt::ShortcutFocusReason
            );
    update();
}

void NavigationSettingsButton::focusOutEvent(QFocusEvent* event)
{
    QToolButton::focusOutEvent(event);
    m_keyboardFocus = false;
    update();
}

void NavigationSettingsButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor fillColor;
    if (isDown())
    {
        fillColor = m_pressedColor.isValid()
            ? m_pressedColor
            : palette().color(QPalette::Midlight);
    }
    else if (underMouse())
    {
        fillColor = m_hoverColor.isValid()
            ? m_hoverColor
            : palette().color(QPalette::Light);
    }

    const QRectF controlRect =
        QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);

    if (fillColor.isValid())
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawRoundedRect(
            controlRect,
            ControlRadius,
            ControlRadius
            );
    }

    if (m_keyboardFocus && hasFocus())
    {
        const QColor focusColor = m_focusColor.isValid()
            ? m_focusColor
            : palette().color(QPalette::Highlight);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(focusColor, FocusWidth));
        painter.drawRoundedRect(
            controlRect,
            ControlRadius,
            ControlRadius
            );
    }

    painter.setFont(font());
    painter.setPen(
        m_textColor.isValid()
            ? m_textColor
            : palette().color(QPalette::ButtonText)
        );

    const QFontMetrics metrics(font());
    const QRect glyphBounds = metrics.tightBoundingRect(text());
    const QPoint baseline(
        rect().center().x() - glyphBounds.center().x(),
        rect().center().y() - glyphBounds.center().y()
        );
    painter.drawText(baseline, text());
}
