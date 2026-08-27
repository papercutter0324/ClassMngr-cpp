#pragma once

#include "features/schedule/ui/schedule_view_model.h"

class QTableWidget;
class QWidget;

struct ScheduleTableRenderOptions
{
    QWidget* cellParent = nullptr;
    int maximumVisibleRows = 0;
    bool compactPreview = false;
    bool showKoreanTeacherEnglishNames = false;
};

struct ScheduleTableRenderMetrics
{
    bool fullRender = false;
    int tableItemsCreated = 0;
    int cellWidgetsCreated = 0;
    int cellWidgetsRemoved = 0;
    int cellWidgetsQueuedForDeletion = 0;
};

class ScheduleTableRenderer final
{
public:
    static void initialize(
        QTableWidget* table
        );

    static void invalidate(
        QTableWidget* table
        );

    [[nodiscard]] static ScheduleTableRenderMetrics render(
        QTableWidget* table,
        const ScheduleViewModel& model,
        const ScheduleTableRenderOptions& options
        );
};
