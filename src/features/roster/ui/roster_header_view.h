#pragma once

#include <QHeaderView>

class RosterColumnLayoutController;

class RosterHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    explicit RosterHeaderView(
        Qt::Orientation orientation,
        QWidget* parent = nullptr
        );

    QSize sizeHint() const override;

    void setLayoutController(
        RosterColumnLayoutController* controller
        );

protected:
    void paintEvent(
        QPaintEvent* event
        ) override;

private:
    void paintGroupRow(
        QPainter& painter
        ) const;

    void paintColumnRow(
        QPainter& painter
        ) const;

    int contentRightEdge() const;

    QBrush trailingBackgroundBrush() const;

private:
    RosterColumnLayoutController* m_controller = nullptr;
};
