#pragma once

#include <QFrame>

class ClickableColorPreview : public QFrame
{
    Q_OBJECT

public:
    explicit ClickableColorPreview(
        QWidget* parent = nullptr
        );

signals:
    void clicked();

protected:
    void mousePressEvent(
        QMouseEvent* event
        ) override;
};