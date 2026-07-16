#include "marquee_label.h"

#include <QEnterEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimerEvent>

namespace
{
constexpr int PreferredWidth = 160;
constexpr int ScrollGap = 32;
constexpr int TimerIntervalMs = 35;
}

MarqueeLabel::MarqueeLabel(QWidget* parent)
    : QLabel(parent)
{
    setMouseTracking(true);
}

QSize MarqueeLabel::minimumSizeHint() const
{
    return QSize(0, QLabel::minimumSizeHint().height());
}

QSize MarqueeLabel::sizeHint() const
{
    return QSize(PreferredWidth, QLabel::sizeHint().height());
}

void MarqueeLabel::setMarqueeActive(bool active)
{
    if (m_active == active)
    {
        return;
    }

    m_active = active;

    if (!m_active)
    {
        m_offset = 0;
        m_timer.stop();
        update();
        return;
    }

    updateMarqueeState();
}

void MarqueeLabel::enterEvent(QEnterEvent* event)
{
    QLabel::enterEvent(event);
    setMarqueeActive(true);
}

void MarqueeLabel::leaveEvent(QEvent* event)
{
    QLabel::leaveEvent(event);
    setMarqueeActive(false);
}

void MarqueeLabel::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);
    updateMarqueeState();
}

void MarqueeLabel::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_timer.timerId())
    {
        QLabel::timerEvent(event);
        return;
    }

    const int textWidth = fontMetrics().horizontalAdvance(text());
    m_offset = (m_offset + 1) % qMax(1, textWidth + ScrollGap);
    update();
}

void MarqueeLabel::paintEvent(QPaintEvent* event)
{
    const int textWidth = fontMetrics().horizontalAdvance(text());

    if (!m_active || textWidth <= contentsRect().width())
    {
        QLabel::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setClipRect(contentsRect());

    const QRect rect = contentsRect();
    const int baseline =
        rect.y()
        + (rect.height() + fontMetrics().ascent() - fontMetrics().descent()) / 2;
    const int x = rect.x() - m_offset;

    painter.drawText(x, baseline, text());
    painter.drawText(x + textWidth + ScrollGap, baseline, text());
}

void MarqueeLabel::updateMarqueeState()
{
    const bool shouldRun =
        m_active
        && fontMetrics().horizontalAdvance(text()) > contentsRect().width();

    if (shouldRun && !m_timer.isActive())
    {
        m_timer.start(TimerIntervalMs, this);
    }
    else if (!shouldRun && m_timer.isActive())
    {
        m_timer.stop();
        m_offset = 0;
    }
}
