#pragma once

#include <QString>

namespace ClassesPageSubtitleText
{

[[nodiscard]] inline QString fromDisplayNameOrFallback(
    const QString& formattedDisplayName,
    const QString& classroomName,
    const QString& localizedClassIdFallback
    )
{
    const QString trimmedDisplayName = formattedDisplayName.trimmed();
    if (!trimmedDisplayName.isEmpty())
    {
        return trimmedDisplayName;
    }

    const QString trimmedClassroomName = classroomName.trimmed();
    return trimmedClassroomName.isEmpty()
        ? localizedClassIdFallback
        : trimmedClassroomName;
}

} // namespace ClassesPageSubtitleText
