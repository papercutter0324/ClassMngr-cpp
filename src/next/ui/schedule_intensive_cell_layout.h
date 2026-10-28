#pragma once

#include "next/ui/schedule_page_model.h"

#include <QFont>
#include <QFontMetrics>
#include <QRect>
#include <QString>
#include <QStringList>
#include <Qt>

#include <algorithm>
#include <vector>

namespace ClassMngr::Next::Ui
{

struct ScheduleIntensiveTextLine final
{
    QString fullText;
    QString visibleText;
    QFont font;
    QRect bounds;
};

struct ScheduleIntensiveEntryLayout final
{
    ScheduleIntensiveTextLine teacher;
    ScheduleIntensiveTextLine room;
    ScheduleIntensiveTextLine classLine;
};

[[nodiscard]] inline QString scheduleEntryTeacherName(
    const SchedulePageEntry& entry,
    const bool english
    )
{
    const QString preferred = (english
            ? entry.teacherPreferredName
            : entry.teacherKoreanName).trimmed();
    const QString fallback = entry.teacherEnglishName.trimmed();
    return !preferred.isEmpty()
        ? preferred
        : (!fallback.isEmpty() ? fallback : entry.teacherKoreanName.trimmed());
}

[[nodiscard]] inline QString scheduleEntryClassLine(
    const SchedulePageEntry& entry
    )
{
    QStringList parts;
    if (!entry.grade.trimmed().isEmpty())
    {
        parts.append(entry.grade.trimmed());
    }
    if (!entry.level.trimmed().isEmpty())
    {
        parts.append(entry.level.trimmed());
    }
    return parts.join(QStringLiteral(" - "));
}

[[nodiscard]] inline QFont scheduleIntensiveLineFont(
    QFont font,
    const int pixelSize,
    const QFont::Weight weight
    )
{
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

[[nodiscard]] inline ScheduleIntensiveTextLine scheduleIntensiveTextLine(
    const QString& text,
    const QFont& font,
    const QRect& bounds
    )
{
    const int textWidth = std::max(0, bounds.width() - 4);
    return {
        .fullText = text,
        .visibleText = QFontMetrics(font).elidedText(
            text, Qt::ElideRight, textWidth),
        .font = font,
        .bounds = bounds
    };
}

// Every Intensive entry gets teacher, room, and class-line bands. Teacher and
// room values elide on one line; the final band is reserved for grade/level.
// Full source values remain available from the model's accessible/tooltip text.
[[nodiscard]] inline std::vector<ScheduleIntensiveEntryLayout>
layoutScheduleIntensiveEntries(
    const QRect& contentBounds,
    const std::vector<SchedulePageEntry>& entries,
    const bool showEnglishNames,
    const QFont& baseFont
    )
{
    std::vector<ScheduleIntensiveEntryLayout> layouts;
    if (contentBounds.isEmpty() || entries.empty())
    {
        return layouts;
    }

    const bool hasOneEntry = entries.size() == 1;
    const int entryCount = static_cast<int>(entries.size());
    const int teacherLineHeight = hasOneEntry ? 16 : 13;
    const int roomLineHeight = hasOneEntry ? 16 : 13;
    const int classLineHeight = hasOneEntry ? 18 : 12;
    const int blockHeight = teacherLineHeight + roomLineHeight + classLineHeight;
    const int baseEntryHeight = contentBounds.height() / entryCount;
    const int extraEntryPixels = contentBounds.height() % entryCount;
    int entryTop = contentBounds.top();
    layouts.reserve(entries.size());

    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const int entryHeight = baseEntryHeight
            + (static_cast<int>(index) < extraEntryPixels ? 1 : 0);
        const QRect entryBounds(contentBounds.left(), entryTop,
                                contentBounds.width(), entryHeight);
        const int firstBandTop = entryBounds.top()
            + std::max(0, (entryBounds.height() - blockHeight) / 2);
        const QRect teacherBounds(entryBounds.left(), firstBandTop,
                                  entryBounds.width(), teacherLineHeight);
        const QRect roomBounds(entryBounds.left(),
                               teacherBounds.bottom() + 1,
                               entryBounds.width(), roomLineHeight);
        const QRect classBounds(entryBounds.left(), roomBounds.bottom() + 1,
                                entryBounds.width(), classLineHeight);

        const QFont teacherFont = scheduleIntensiveLineFont(
            baseFont, hasOneEntry ? 13 : 10, QFont::DemiBold);
        const QFont roomFont = scheduleIntensiveLineFont(
            baseFont, hasOneEntry ? 13 : 10, QFont::Medium);
        const QFont classFont = scheduleIntensiveLineFont(
            baseFont, hasOneEntry ? 12 : 9, QFont::Normal);

        layouts.push_back({
            .teacher = scheduleIntensiveTextLine(
                scheduleEntryTeacherName(entries[index], showEnglishNames),
                teacherFont, teacherBounds),
            .room = scheduleIntensiveTextLine(
                entries[index].room.trimmed(), roomFont, roomBounds),
            .classLine = scheduleIntensiveTextLine(
                scheduleEntryClassLine(entries[index]), classFont, classBounds)
        });
        entryTop += entryHeight;
    }
    return layouts;
}

} // namespace ClassMngr::Next::Ui
