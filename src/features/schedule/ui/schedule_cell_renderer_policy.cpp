#include "schedule_cell_renderer_policy.h"

#include "classmngr/engine/schedule_report.h"

QString ScheduleCellRendererPolicy::escaped(const QString& text)
{
    return text.toHtmlEscaped();
}

QString ScheduleCellRendererPolicy::classStyle(
    const QString& classColor,
    const QString& fontColor,
    qreal verticalPadding,
    int horizontalPadding,
    int borderRadius
    )
{
    return QStringLiteral(
        "QLabel {background:%1;color:%2;padding:%3px %4px;"
        "border-radius:%5px;}"
        )
        .arg(classColor.isEmpty() ? QStringLiteral("#FFFFFF") : classColor)
        .arg(fontColor.isEmpty() ? QStringLiteral("#000000") : fontColor)
        .arg(verticalPadding)
        .arg(horizontalPadding)
        .arg(borderRadius);
}

QString ScheduleCellRendererPolicy::englishLine(const ScheduleEntry& entry)
{
    return QString::fromUtf8(
        classmngr::engine::ScheduleReportService::classLine(
            entry.classGrade.toStdString(),
            entry.classLevel.toStdString()
            )
        );
}
