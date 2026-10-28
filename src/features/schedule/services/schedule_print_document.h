#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace SchedulePrintService
{

// Value-only content shared by legacy and Next schedule output. It owns no
// widgets, models, repositories, or workspace references.
enum class SlotState
{
    Empty,
    Essay,
    Lunch,
    Testing
};

struct Entry final
{
    QString className;
    QString teacherKoreanName;
    QString teacherEnglishName;
    QString teacherPreferredName;
    QString room;
    QString grade;
    QString level;
    QString classColor;
    QString fontColor;
    bool testingClass = false;

    friend bool operator==(const Entry&, const Entry&) = default;
};

struct Cell final
{
    QString day;
    QString timeLabel;
    SlotState slotState = SlotState::Empty;
    QString testingRoom;
    QList<Entry> entries;

    friend bool operator==(const Cell&, const Cell&) = default;
};

struct Row final
{
    QString timeLabel;
    QString timeRangeLabel;
    QList<Cell> cells;

    friend bool operator==(const Row&, const Row&) = default;
};

struct Summary final
{
    int essayBlocks = 0;
    int scheduledBlocks = 0;

    friend bool operator==(const Summary&, const Summary&) = default;
};

struct Document final
{
    QStringList days;
    QList<Row> rows;
    Summary summary;

    friend bool operator==(const Document&, const Document&) = default;
};

} // namespace SchedulePrintService
