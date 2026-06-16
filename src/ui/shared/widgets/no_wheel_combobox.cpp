#include "no_wheel_combobox.h"

#include <QWheelEvent>

NoWheelComboBox::NoWheelComboBox(
    QWidget* parent
    )
    : QComboBox(parent)
{
}

void NoWheelComboBox::wheelEvent(
    QWheelEvent* event
    )
{
    event->ignore();
}