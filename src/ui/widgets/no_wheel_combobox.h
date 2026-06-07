#pragma once

#include <QComboBox>

class NoWheelComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit NoWheelComboBox(
        QWidget* parent = nullptr
        );

protected:
    void wheelEvent(
        QWheelEvent* event
        ) override;
};