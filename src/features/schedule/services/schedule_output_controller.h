#pragma once

#include "features/schedule/ui/schedule_view_model.h"
#include "ui/shared/constants/options.h"

class ApplicationServices;
class QWidget;

class ScheduleOutputController final
{
public:
    enum class Action
    {
        Print,
        SaveAs
    };

    static void execute(
        Action action,
        QWidget* parent,
        ApplicationServices* services,
        const ScheduleViewModel& model,
        Theme currentTheme,
        bool showEnglishNames
        );
};
