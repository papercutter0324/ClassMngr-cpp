#pragma once

class QTableWidget;

struct ScheduleTableGeometry
{
    int height = 0;
    bool hasHiddenRows = false;
};

class ScheduleWidgetGeometry final
{
public:
    [[nodiscard]] static ScheduleTableGeometry tableGeometry(
        const QTableWidget* table,
        int maximumVisibleRows
        );
};
