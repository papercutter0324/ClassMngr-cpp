#pragma once

#include "features/schedule/ui/schedule_print_style.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "ui/shared/constants/options.h"

#include <QPageLayout>
#include <QString>

class QWidget;

namespace SchedulePrintService
{
enum class Status
{
    Sent,
    Canceled,
    Failed
};

struct Request
{
    QWidget* parent = nullptr;
    ScheduleViewModel model;
    SchedulePrintStyle style = SchedulePrintStyle::CurrentAppearance;
    Theme currentTheme = Theme::Dark;
    QString userName;
    QPageLayout::Orientation pageOrientation = QPageLayout::Landscape;
};

struct Result
{
    Status status = Status::Failed;
    QString message;
};

[[nodiscard]] Result printSchedule(
    const Request& request
    );

[[nodiscard]] Result saveSchedulePdf(
    const Request& request,
    const QString& documentPath
    );
}
