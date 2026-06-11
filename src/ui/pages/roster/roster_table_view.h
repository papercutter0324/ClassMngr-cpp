#pragma once

#include <QTableView>

class RosterColumnLayoutController;

class RosterTableView : public QTableView
{
    Q_OBJECT

public:
    explicit RosterTableView(
        QWidget* parent = nullptr
        );

    void setLayoutController(
        RosterColumnLayoutController* controller
        );

protected:
    void paintEvent(
        QPaintEvent* event
        ) override;

private:
    RosterColumnLayoutController* m_controller = nullptr;
};
