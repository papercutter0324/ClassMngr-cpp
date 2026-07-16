#pragma once

#include <QBasicTimer>
#include <QLabel>

class QEnterEvent;
class QEvent;
class QPaintEvent;
class QResizeEvent;
class QTimerEvent;

class MarqueeLabel : public QLabel
{
public:
    explicit MarqueeLabel(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void setMarqueeActive(bool active);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateMarqueeState();

    QBasicTimer m_timer;
    bool m_active = false;
    int m_offset = 0;
};
