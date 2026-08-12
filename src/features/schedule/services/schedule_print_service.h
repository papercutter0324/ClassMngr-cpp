#pragma once

#include "domain/models/document_output_result.h"
#include "features/schedule/ui/schedule_print_style.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "ui/shared/constants/options.h"

#include <QPageLayout>
#include <QString>

class QWidget;

namespace SchedulePrintService
{
using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;

struct Request
{
    QWidget* parent = nullptr;
    ScheduleViewModel model;
    SchedulePrintStyle style = SchedulePrintStyle::CurrentAppearance;
    Theme currentTheme = Theme::Dark;
    QString userName;
    bool showEnglishNames = false;
    QPageLayout::Orientation pageOrientation = QPageLayout::Landscape;
};

[[nodiscard]] Result printSchedule(
    const Request& request
    );

[[nodiscard]] Result saveSchedulePdf(
    const Request& request,
    const QString& documentPath
    );
}
