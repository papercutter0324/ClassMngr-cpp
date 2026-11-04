#pragma once

#include "domain/models/document_output_result.h"
#include "features/schedule/services/schedule_print_document.h"
#include "features/schedule/ui/schedule_print_style.h"
#include "ui/shared/constants/options.h"

#include <QPageLayout>
#include <QFont>
#include <QString>

namespace SchedulePdfRenderer
{
using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;

struct Request final
{
    SchedulePrintService::Document document;
    SchedulePrintStyle style = SchedulePrintStyle::CurrentAppearance;
    Theme currentTheme = Theme::Dark;
    QFont englishFont;
    QFont koreanFont;
    QString userName;
    bool showEnglishNames = false;
    QPageLayout::Orientation pageOrientation = QPageLayout::Landscape;
};

// Renders one schedule page to the requested PDF path without opening a
// printer dialog or depending on Legacy UI and services.
[[nodiscard]] Result renderSchedulePdf(
    const Request& request,
    const QString& documentPath
    );
}
