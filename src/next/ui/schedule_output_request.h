#pragma once

#include "features/schedule/services/schedule_print_document.h"
#include "features/schedule/ui/schedule_print_style.h"
#include "ui/shared/constants/options.h"

#include <QPageLayout>
#include <QFont>
#include <QString>

namespace ClassMngr::Next::Ui
{

enum class ScheduleOutputKind
{
    Print,
    SaveAsPdf
};

struct ScheduleOutputRequest final
{
    SchedulePrintService::Document document;
    ScheduleOutputKind kind = ScheduleOutputKind::Print;
    SchedulePrintStyle style = SchedulePrintStyle::CurrentAppearance;
    Theme currentTheme = Theme::Dark;
    QFont englishFont;
    QFont koreanFont;
    QString profileName;
    bool showEnglishNames = false;
    QPageLayout::Orientation orientation = QPageLayout::Landscape;
    QString documentPath;
};

struct ScheduleOutputResult final
{
    enum class Status
    {
        Sent,
        Canceled,
        Failed
    };

    Status status = Status::Failed;
    QString message;
};

} // namespace ClassMngr::Next::Ui
