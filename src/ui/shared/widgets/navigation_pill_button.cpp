#include "navigation_pill_button.h"

#include "ui/shared/widgets/navigation_pill_style.h"

#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QStyle>

NavigationPillButton::NavigationPillButton(
    QWidget* parent
    )
    : QPushButton(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(
        this,
        &QPushButton::toggled,
        this,
        qOverload<>(&NavigationPillButton::update)
        );
}

QColor NavigationPillButton::navigationTabFillColor() const
{
    return m_navigationTabFillColor;
}

void NavigationPillButton::setNavigationTabFillColor(
    const QColor& color
    )
{
    m_navigationTabFillColor = color;
    update();
}

QColor NavigationPillButton::navigationTabBorderColor() const
{
    return m_navigationTabBorderColor;
}

void NavigationPillButton::setNavigationTabBorderColor(
    const QColor& color
    )
{
    m_navigationTabBorderColor = color;
    update();
}

QColor NavigationPillButton::navigationTabHoverColor() const
{
    return m_navigationTabHoverColor;
}

void NavigationPillButton::setNavigationTabHoverColor(
    const QColor& color
    )
{
    m_navigationTabHoverColor = color;
    update();
}

QColor NavigationPillButton::navigationTabSelectedColor() const
{
    return m_navigationTabSelectedColor;
}

void NavigationPillButton::setNavigationTabSelectedColor(
    const QColor& color
    )
{
    m_navigationTabSelectedColor = color;
    update();
}

QColor NavigationPillButton::navigationTabTextColor() const
{
    return m_navigationTabTextColor;
}

void NavigationPillButton::setNavigationTabTextColor(
    const QColor& color
    )
{
    m_navigationTabTextColor = color;
    update();
}

QSize NavigationPillButton::sizeHint() const
{
    const int iconExtent =
        style()->pixelMetric(
            QStyle::PM_SmallIconSize,
            nullptr,
            this
            );
    const QSize effectiveIconSize =
        icon().isNull()
            ? QSize()
            : icon().actualSize(QSize(iconExtent, iconExtent));

    return NavigationPillStyle::sizeHint(
        fontMetrics(),
        icon(),
        text(),
        effectiveIconSize
        );
}

QSize NavigationPillButton::minimumSizeHint() const
{
    return sizeHint();
}

void NavigationPillButton::changeEvent(
    QEvent* event
    )
{
    QPushButton::changeEvent(event);
    updateGeometry();
    update();
}

void NavigationPillButton::enterEvent(
    QEnterEvent* event
    )
{
    QPushButton::enterEvent(event);
    update();
}

void NavigationPillButton::leaveEvent(
    QEvent* event
    )
{
    QPushButton::leaveEvent(event);
    update();
}

void NavigationPillButton::paintEvent(
    QPaintEvent* event
    )
{
    Q_UNUSED(event);

    QPainter painter(this);
    NavigationPillStyle::paint(
        &painter,
        rect(),
        text(),
        icon(),
        font(),
        Qt::ElideRight,
        style(),
        this,
        palette(),
        {
            isEnabled(),
            isCheckable() && isChecked(),
            underMouse(),
            isActiveWindow()
        },
        {
            m_navigationTabFillColor,
            m_navigationTabBorderColor,
            m_navigationTabHoverColor,
            m_navigationTabSelectedColor,
            m_navigationTabTextColor
        }
        );
}
