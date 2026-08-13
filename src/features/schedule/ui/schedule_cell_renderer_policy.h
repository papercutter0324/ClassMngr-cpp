#pragma once

#include "features/schedule/ui/schedule_view_model.h"

#include <QString>

class ScheduleCellRendererPolicy final
{
public:
    [[nodiscard]] static QString escaped(const QString& text);
    [[nodiscard]] static QString classStyle(
        const QString& classColor,
        const QString& fontColor,
        qreal verticalPadding,
        int horizontalPadding,
        int borderRadius
        );
    [[nodiscard]] static QString englishLine(const ScheduleEntry& entry);
};
