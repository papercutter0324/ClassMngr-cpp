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

class ScheduleTableRenderer final
{
public:
    static void initialize(
        QTableWidget* table
        );

    static void render(
        QTableWidget* table,
        const ScheduleViewModel& model,
        const ScheduleTableRenderOptions& options
        );
};
