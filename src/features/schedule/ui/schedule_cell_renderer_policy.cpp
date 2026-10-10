#include "schedule_cell_renderer_policy.h"

QString ScheduleCellRendererPolicy::escaped(const QString& text)
{
    return text.toHtmlEscaped();
}

QString ScheduleCellRendererPolicy::englishLine(const ScheduleEntry& entry)
{
    QStringList parts;
    if (!entry.classGrade.trimmed().isEmpty())
    {
        parts.append(entry.classGrade.trimmed());
    }
    if (!entry.classLevel.trimmed().isEmpty())
    {
        parts.append(entry.classLevel.trimmed());
    }
    return parts.join(QStringLiteral(" - "));
}
