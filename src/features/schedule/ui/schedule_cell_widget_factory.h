#pragma once

#include "features/schedule/ui/schedule_view_model.h"

class QWidget;

struct ScheduleCellWidgetOptions
{
    QWidget* parent = nullptr;
    bool compactPreview = false;
    bool showKoreanTeacherEnglishNames = false;
};

class ScheduleCellWidgetFactory final
{
public:
    [[nodiscard]] static QWidget* create(
        const ScheduleCellView& cell,
        const ScheduleCellWidgetOptions& options
        );
};
