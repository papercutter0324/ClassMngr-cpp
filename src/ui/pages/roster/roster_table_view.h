#pragma once

#include <QTableView>

class QEvent;
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

    int contentBottomEdge() const;

protected:
    void changeEvent(
        QEvent* event
        ) override;

    void paintEvent(
        QPaintEvent* event
        ) override;

private:
    void updateVerticalHeaderTrailingBackground();

private:
    RosterColumnLayoutController* m_controller = nullptr;
};
