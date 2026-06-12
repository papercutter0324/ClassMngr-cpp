#include "clickable_color_preview.h"

#include <QMouseEvent>

ClickableColorPreview::ClickableColorPreview(
    QWidget* parent
    )
    : QFrame(parent)
{
    setCursor(Qt::PointingHandCursor);
}

void ClickableColorPreview::mousePressEvent(
    QMouseEvent* event
    )
{
    emit clicked();

    QFrame::mousePressEvent(event);
}
