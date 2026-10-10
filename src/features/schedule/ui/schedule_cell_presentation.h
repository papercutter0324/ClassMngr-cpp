#pragma once
#include "features/schedule/ui/schedule_view_model.h"
#include <QColor>
#include <QFont>

struct ScheduleCellPresentationOptions
{
    bool compactPreview = false;
    bool showKoreanTeacherEnglishNames = false;
    bool dark = false;
};

// Value presentation consumed by the item delegate; owns no QWidget or model.
struct ScheduleCellPresentation
{
    QString text;
    QString html;
    QString accessibleText;
    QColor background = Qt::transparent;
    QColor foreground = Qt::black;
    QColor border;
    QColor markerColor;
    QFont font;
    bool marker = false;
    static ScheduleCellPresentation fromCell(
        const ScheduleCellView& cell,
        const ScheduleCellPresentationOptions& options);
};
