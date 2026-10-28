#include "next/ui/schedule_page_model.h"

#include "domain/rules/schedule_value_parser.h"
#include "next/application/schedule_display_mode_source_policy.h"
#include "next/application/schedule_testing_grade_suppression_policy.h"
#include "next/domain/course.h"
#include "next/ui/schedule_page_slot_policy_bridge.h"

#include <QCoreApplication>
#include <QTime>

#include <algorithm>
#include <charconv>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

inline constexpr int kRegularDefaultStartHour = 16;
inline constexpr int kFinalHour = 21;
inline constexpr int kIntensiveStartHour = 9;
inline constexpr std::size_t kMaximumRows = 4'096;
inline constexpr std::size_t kMaximumCells = 32'768;
inline constexpr std::size_t kMaximumCellsPerRow = 96;
inline constexpr std::size_t kMaximumEntries = 32'768;
inline constexpr std::size_t kMaximumDisplayedRows =
    static_cast<std::size_t>(kFinalHour + 1);
inline constexpr std::size_t kMaximumDisplayedDays = 7;

// Each visible cell is rejected above 96 entries, so the current 22 x 7 grid
// can contain at most 14,784 entries. Keep the aggregate guard defensive and
// force a review if future grid bounds ever make that guard reachable.
static_assert(kMaximumDisplayedRows * kMaximumDisplayedDays
        * kMaximumCellsPerRow < kMaximumEntries,
    "Reassess the aggregate schedule entry cap when the visible grid expands.");

using EntryKey = std::pair<int, int>;

[[nodiscard]] QString trSchedule(const char* source)
{
    return QCoreApplication::translate("ScheduleWidget", source);
}

[[nodiscard]] std::optional<int> weekdayIndex(const QString& value)
{
    const auto parsed = ScheduleValueParser::parseWeekday(value);
    if (!parsed || parsed->text != value.trimmed())
    {
        return std::nullopt;
    }
    return static_cast<int>(parsed->value);
}

[[nodiscard]] QString canonicalDay(const int index)
{
    switch (index)
    {
    case 0: return QStringLiteral("Monday");
    case 1: return QStringLiteral("Tuesday");
    case 2: return QStringLiteral("Wednesday");
    case 3: return QStringLiteral("Thursday");
    case 4: return QStringLiteral("Friday");
    case 5: return QStringLiteral("Saturday");
    case 6: return QStringLiteral("Sunday");
    }
    return {};
}

[[nodiscard]] std::optional<int> minuteOfDay(const QString& raw)
{
    const auto parsed = ScheduleValueParser::parseDisplayTime(raw);
    if (!parsed)
    {
        return std::nullopt;
    }
    return parsed->hour() * 60 + parsed->minute();
}

[[nodiscard]] QString timeLabel(const int minute)
{
    const QTime time(minute / 60, minute % 60);
    return time.isValid() ? time.toString(QStringLiteral("HH:mm")) : QString{};
}

[[nodiscard]] QString displayTime(const int minute, const bool use24Hour)
{
    const QTime time(minute / 60, minute % 60);
    return !time.isValid()
        ? QString{}
        : time.toString(use24Hour
              ? QStringLiteral("HH:mm")
              : QStringLiteral("h:mm AP"));
}

[[nodiscard]] QString timeRangeLabel(
    const int startMinute,
    const int durationMinutes,
    const bool use24Hour
    )
{
    const int normalizedStart = startMinute % (24 * 60);
    const int normalizedEnd = (startMinute + durationMinutes) % (24 * 60);
    const QString start = displayTime(normalizedStart, use24Hour);
    const QString end = displayTime(normalizedEnd, use24Hour);
    if (use24Hour)
    {
        return QStringLiteral("%1 - %2").arg(start, end);
    }

    const QString startMeridiem = start.endsWith(QStringLiteral("AM"))
        ? QStringLiteral("AM") : QStringLiteral("PM");
    const QString endMeridiem = end.endsWith(QStringLiteral("AM"))
        ? QStringLiteral("AM") : QStringLiteral("PM");
    QString startClean = start;
    startClean.remove(QStringLiteral(" AM"));
    startClean.remove(QStringLiteral(" PM"));
    QString endClean = end;
    endClean.remove(QStringLiteral(" AM"));
    endClean.remove(QStringLiteral(" PM"));
    if (startMeridiem == endMeridiem)
    {
        return QStringLiteral("%1 -\n%2 %3")
            .arg(startClean, endClean, endMeridiem);
    }
    return QStringLiteral("%1\n- %2").arg(start, end);
}

[[nodiscard]] QString teacherName(
    const SchedulePageEntry& entry,
    const bool showEnglish
    )
{
    const QString preferred = (showEnglish
            ? entry.teacherPreferredName
            : entry.teacherKoreanName).trimmed();
    if (!preferred.isEmpty())
    {
        return preferred;
    }
    const QString fallback = entry.teacherEnglishName.trimmed();
    return fallback.isEmpty() ? entry.teacherKoreanName.trimmed() : fallback;
}

[[nodiscard]] QString stateSetting(const std::u16string& raw)
{
    return QString::fromStdU16String(raw).trimmed().toLower();
}

[[nodiscard]] std::optional<Application::ScheduleSlotState> slotState(
    const std::u16string& raw
    )
{
    const QString value = stateSetting(raw);
    if (value == QStringLiteral("empty"))
    {
        return Application::ScheduleSlotState::Empty;
    }
    if (value == QStringLiteral("essay"))
    {
        return Application::ScheduleSlotState::Essay;
    }
    if (value == QStringLiteral("lunch"))
    {
        return Application::ScheduleSlotState::Lunch;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Application::ScheduleSlotState> pageState(
    const SchedulePageDefaultSlotState state
    )
{
    return state == SchedulePageDefaultSlotState::Empty
        ? Application::ScheduleSlotState::Empty
        : Application::ScheduleSlotState::Essay;
}

[[nodiscard]] bool isWeekend(const int dayIndex) noexcept
{
    return dayIndex == 5 || dayIndex == 6;
}

[[nodiscard]] SchedulePageEntry entryFromSource(
    const Application::ScheduleBuilderSourceClass& source
    )
{
    return {
        .classId = source.classId,
        .teacherKoreanName = QString::fromStdU16String(source.teacherKoreanName),
        .teacherEnglishName = QString::fromStdU16String(source.teacherEnglishName),
        .teacherPreferredName = QString::fromStdU16String(source.teacherPreferredName),
        .room = QString::fromStdU16String(source.roomNumber),
        .grade = QString::fromStdU16String(source.grade),
        .level = QString::fromStdU16String(source.level),
        .classColor = source.classColor.empty()
            ? QStringLiteral("#FFFFFF") : QString::fromStdU16String(source.classColor),
        .fontColor = source.fontColor.empty()
            ? QStringLiteral("#000000") : QString::fromStdU16String(source.fontColor)
    };
}

[[nodiscard]] SchedulePageEntry entryFromTestingClass(
    const Application::ScheduleTestingAssignmentReadRow& source
    )
{
    if (!source.specialClass)
    {
        return SchedulePageEntry{.classId = source.classId};
    }
    return {
        .classId = source.classId,
        .className = QString::fromStdU16String(source.specialClass->name),
        .teacherKoreanName = QString::fromStdU16String(
            source.specialClass->teacherKoreanName),
        .teacherEnglishName = QString::fromStdU16String(
            source.specialClass->teacherEnglishName),
        .teacherPreferredName = QString::fromStdU16String(
            source.specialClass->teacherPreferredName),
        .room = QString::fromStdU16String(source.specialClass->room),
        .grade = QString::fromStdU16String(source.specialClass->grade),
        .level = QString::fromStdU16String(source.specialClass->level),
        .classColor = source.specialClass->classColor.empty()
            ? QStringLiteral("#FFFFFF")
            : QString::fromStdU16String(source.specialClass->classColor),
        .fontColor = source.specialClass->fontColor.empty()
            ? QStringLiteral("#000000")
            : QString::fromStdU16String(source.specialClass->fontColor),
        .testingClass = true
    };
}

[[nodiscard]] Domain::OperationError invalidProjection(std::string message)
{
    return {
        .code = Domain::ErrorCode::Validation,
        .message = std::move(message),
        .recoverable = false
    };
}

} // namespace

SchedulePageModel::SchedulePageModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int SchedulePageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int SchedulePageModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_days.size() + 1;
}

QVariant SchedulePageModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.column() < 0
        || index.row() >= rowCount() || index.column() >= columnCount())
    {
        return {};
    }
    if (index.column() == 0)
    {
        if (role == Qt::DisplayRole)
        {
            return m_rows[static_cast<std::size_t>(index.row())].timeRangeLabel;
        }
        if (role == Qt::TextAlignmentRole)
        {
            return static_cast<int>(Qt::AlignCenter);
        }
        return {};
    }

    const SchedulePageCell* const cell = cellAt(index.row(), index.column());
    if (!cell)
    {
        return {};
    }
    if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole
        || role == Qt::ToolTipRole)
    {
        return accessibleCellText(*cell);
    }
    if (role == Qt::TextAlignmentRole)
    {
        return static_cast<int>(Qt::AlignCenter);
    }
    if (role == Qt::UserRole)
    {
        return cell->entries.empty()
            ? QVariant{}
            : QVariant(QString::fromUtf8(
                  cell->entries.front().classId.value().data(),
                  static_cast<qsizetype>(
                      cell->entries.front().classId.value().size())));
    }
    return {};
}

QVariant SchedulePageModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    if (section == 0)
    {
        return trSchedule("Time");
    }
    if (section < 1 || section > m_days.size())
    {
        return {};
    }
    const QString& day = m_days.at(section - 1);
    return trSchedule(day.toUtf8().constData());
}

Domain::Result<void> SchedulePageModel::setSnapshot(
    Application::SchedulePageReadSnapshot snapshot
    )
{
    m_snapshot = std::move(snapshot);
    m_projectionError.clear();
    beginResetModel();
    rebuild();
    endResetModel();
    if (!m_projectionError.isEmpty()
        || m_rows.size() > kMaximumRows
        || m_rows.size() * static_cast<std::size_t>(m_days.size()) > kMaximumCells
        || m_entryCount > kMaximumEntries)
    {
        const QString message = m_projectionError.isEmpty()
            ? QStringLiteral(
                  "The schedule projection exceeds its supported row, cell, or entry limit.")
            : m_projectionError;
        clear();
        return Domain::Result<void>::failure(invalidProjection(
            message.toUtf8().toStdString()));
    }
    return Domain::Result<void>::success();
}

void SchedulePageModel::clear()
{
    beginResetModel();
    m_snapshot = {};
    m_days.clear();
    m_rows.clear();
    m_entryCount = 0;
    m_projectionError.clear();
    endResetModel();
}

const SchedulePageCell* SchedulePageModel::cellAt(
    const int row,
    const int tableColumn
    ) const noexcept
{
    const int cellIndex = tableColumn - 1;
    if (row < 0 || row >= static_cast<int>(m_rows.size())
        || cellIndex < 0
        || cellIndex >= static_cast<int>(m_rows[static_cast<std::size_t>(row)].cells.size()))
    {
        return nullptr;
    }
    return &m_rows[static_cast<std::size_t>(row)]
                .cells[static_cast<std::size_t>(cellIndex)];
}

const SchedulePageRow* SchedulePageModel::rowAt(const int row) const noexcept
{
    return row < 0 || row >= static_cast<int>(m_rows.size())
        ? nullptr
        : &m_rows[static_cast<std::size_t>(row)];
}

Application::ScheduleDisplayMode SchedulePageModel::displayMode() const noexcept
{
    return m_snapshot.displayMode;
}

const Application::ScheduleDisplayPreferences&
SchedulePageModel::preferences() const noexcept
{
    return m_snapshot.displayPreferences;
}

SchedulePageRuntimeMetrics SchedulePageModel::runtimeMetrics() const noexcept
{
    return {
        .rowCount = static_cast<int>(m_rows.size()),
        .cellCount = static_cast<int>(m_rows.size()
            * static_cast<std::size_t>(m_days.size())),
        .entryCount = static_cast<int>(m_entryCount),
        .sourceClassCount = static_cast<int>(m_snapshot.source.classes.size())
    };
}

void SchedulePageModel::rebuild()
{
    m_rows.clear();
    m_entryCount = 0;
    m_days = {
        QStringLiteral("Monday"), QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"), QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };
    if (m_snapshot.displayPreferences.showWeekends)
    {
        m_days.append(QStringLiteral("Saturday"));
        m_days.append(QStringLiteral("Sunday"));
    }

    const bool intensive = Application::scheduleDisplayModeUsesIntensiveSource(
        m_snapshot.displayMode);
    const bool testingMode = m_snapshot.displayMode
        == Application::ScheduleDisplayMode::Testing;
    const int dayCount = m_days.size();
    std::map<EntryKey, std::vector<SchedulePageEntry>> entriesByKey;
    bool hasEarliestHour = false;
    int earliestHour = 0;
    int scheduleOffset = 0;
    bool uses55Endings = false;

    for (const Application::ScheduleBuilderSourceClass& source :
         m_snapshot.source.classes)
    {
        const auto& scheduleRows = intensive
            ? source.intensiveSchedule
            : source.regularSchedule;
        const SchedulePageEntry entry = entryFromSource(source);
        for (const Application::ScheduleBuilderSourceScheduleRow& time : scheduleRows)
        {
            const QString rawDay = QString::fromStdU16String(time.day.value()).trimmed();
            const QString day = rawDay.isEmpty()
                ? QStringLiteral("Monday") : rawDay;
            const auto dayNumber = weekdayIndex(day);
            if (!dayNumber || *dayNumber >= dayCount
                || canonicalDay(*dayNumber) != day)
            {
                continue;
            }

            const auto start = ScheduleValueParser::parseDisplayTime(
                QString::fromStdU16String(time.startTime.value()));
            if (!start)
            {
                continue;
            }
            const auto end = ScheduleValueParser::parseDisplayTime(
                QString::fromStdU16String(time.endTime.value()));
            if (end && end->minute() == 55)
            {
                uses55Endings = true;
            }

            int adjustedHour = start->hour();
            if (start->minute() == 55)
            {
                ++adjustedHour;
            }
            if (!hasEarliestHour || adjustedHour < earliestHour)
            {
                earliestHour = adjustedHour;
                hasEarliestHour = true;
            }
            if (start->minute() == 55)
            {
                scheduleOffset = 55;
            }
            else if (start->minute() == 5 && scheduleOffset != 55)
            {
                scheduleOffset = 5;
            }
            entriesByKey[{*dayNumber, start->hour() * 60 + start->minute()}]
                .push_back(entry);
        }
    }

    if (intensive)
    {
        scheduleOffset = 0;
        uses55Endings = false;
    }
    const int firstHour = intensive
        ? kIntensiveStartHour
        : (hasEarliestHour && earliestHour < kRegularDefaultStartHour
            ? earliestHour : kRegularDefaultStartHour);
    const int durationMinutes = uses55Endings ? 55 : 50;
    for (int hour = firstHour; hour <= kFinalHour; ++hour)
    {
        const int displayHour = scheduleOffset == 55 ? hour - 1 : hour;
        const int startMinute = displayHour * 60 + scheduleOffset;
        SchedulePageRow row;
        row.startMinute = startMinute;
        row.timeRangeLabel = timeRangeLabel(
            startMinute, durationMinutes,
            m_snapshot.displayPreferences.use24HourTime);
        row.cells.reserve(static_cast<std::size_t>(dayCount));
        for (int day = 0; day < dayCount; ++day)
        {
            const int canonicalStart = startMinute;
            const SchedulePageSlotPolicy policy = schedulePageSlotPolicy(
                isWeekend(day), hour, intensive);
            const Application::ScheduleSlotState defaultState =
                pageState(policy.defaultState).value();
            SchedulePageCell cell{
                .day = m_days.at(day),
                .startMinute = canonicalStart,
                .state = defaultState,
                .defaultState = defaultState,
                .slotTogglingEnabled = policy.togglingEnabled
            };

            const auto storedState = std::find_if(
                m_snapshot.slotStates.rows.cbegin(),
                m_snapshot.slotStates.rows.cend(),
                [&](const Application::ScheduleSlotStateReadRow& stored)
                {
                    const QString storedDay = QString::fromStdU16String(
                        stored.identity.day());
                    const auto parsedDay = weekdayIndex(storedDay);
                    const auto storedStart = minuteOfDay(QString::fromStdU16String(
                        stored.identity.startTime()));
                    return parsedDay && *parsedDay == day
                        && storedStart && *storedStart == canonicalStart;
                });
            if (cell.slotTogglingEnabled
                && storedState != m_snapshot.slotStates.rows.cend())
            {
                const auto parsedState = slotState(storedState->state);
                if (parsedState)
                {
                    cell.state = *parsedState;
                }
            }

            const auto foundEntries = entriesByKey.find({day, canonicalStart});
            if (foundEntries != entriesByKey.end())
            {
                cell.entries = foundEntries->second;
            }

            if (testingMode)
            {
                const auto assignment = std::find_if(
                    m_snapshot.testingAssignments.rows.cbegin(),
                    m_snapshot.testingAssignments.rows.cend(),
                    [&](const Application::ScheduleTestingAssignmentReadRow& row)
                    {
                        const QString assignmentDay = QString::fromStdU16String(
                            row.assignmentKey.day());
                        const auto parsedDay = weekdayIndex(assignmentDay);
                        const auto assignmentStart = minuteOfDay(
                            QString::fromStdU16String(row.assignmentKey.startTime()));
                        return parsedDay && *parsedDay == day
                            && assignmentStart && *assignmentStart == canonicalStart;
                    });
                if (assignment != m_snapshot.testingAssignments.rows.cend()
                    && (assignment->classId.value() == "-1"
                        || assignment->specialClass))
                {
                    cell.entries.clear();
                    if (assignment->classId.value() != "-1"
                        && assignment->specialClass)
                    {
                        cell.entries.push_back(entryFromTestingClass(*assignment));
                    }
                    else
                    {
                        cell.plainTestingAssignment = true;
                        cell.testingRoom = QString::fromStdU16String(
                            assignment->room);
                    }
                }
                else if (!cell.entries.empty())
                {
                    const auto affected = std::remove_if(
                        cell.entries.begin(), cell.entries.end(),
                        [&](const SchedulePageEntry& entry)
                        {
                            const QString grade = entry.grade.trimmed().toUpper();
                            const Domain::CourseGradeBand band =
                                Domain::Course::gradeBandForName(
                                    grade.toStdString());
                            return Application::scheduleTestingSuppressesGradeBand(
                                band,
                                m_snapshot.displayPreferences.testingAffectsM1);
                        });
                    const bool removed = affected != cell.entries.end();
                    cell.entries.erase(affected, cell.entries.end());
                    if (removed && cell.entries.empty())
                    {
                        cell.state = Application::ScheduleSlotState::Essay;
                    }
                }
            }

            m_entryCount += cell.entries.size();
            if (cell.entries.size() > kMaximumCellsPerRow
                || m_entryCount > kMaximumEntries)
            {
                m_projectionError = cell.entries.size() > kMaximumCellsPerRow
                    ? QStringLiteral(
                          "A schedule cell exceeds the supported 96 class-entry limit.")
                    : QStringLiteral(
                          "The schedule exceeds the supported 32,768 visible entry limit.");
                m_rows.clear();
                return;
            }
            row.cells.push_back(std::move(cell));
        }
        m_rows.push_back(std::move(row));
    }

    if (intensive && !m_snapshot.displayPreferences.showAllIntensiveHours)
    {
        int firstVisibleRow = -1;
        int lastVisibleRow = -1;
        for (int rowIndex = 0; rowIndex < static_cast<int>(m_rows.size()); ++rowIndex)
        {
            const SchedulePageRow& row = m_rows[static_cast<std::size_t>(rowIndex)];
            const bool visible = std::any_of(
                row.cells.cbegin(), row.cells.cend(),
                [](const SchedulePageCell& cell)
                {
                    return !cell.entries.empty()
                        || cell.state != Application::ScheduleSlotState::Empty;
                });
            if (visible)
            {
                if (firstVisibleRow < 0)
                {
                    firstVisibleRow = rowIndex;
                }
                lastVisibleRow = rowIndex;
            }
        }
        if (firstVisibleRow < 0)
        {
            m_rows.clear();
            m_entryCount = 0;
        }
        else if (firstVisibleRow > 0 || lastVisibleRow + 1 < static_cast<int>(m_rows.size()))
        {
            std::vector<SchedulePageRow> visibleRows(
                m_rows.begin() + firstVisibleRow,
                m_rows.begin() + lastVisibleRow + 1);
            m_rows = std::move(visibleRows);
        }
    }
}

QString SchedulePageModel::accessibleCellText(const SchedulePageCell& cell) const
{
    if (!cell.entries.empty())
    {
        QStringList parts;
        for (const SchedulePageEntry& entry : cell.entries)
        {
            if (entry.testingClass && !entry.className.trimmed().isEmpty())
            {
                parts.append(entry.className.trimmed());
            }
            const QString teacher = teacherName(
                entry, m_snapshot.displayPreferences.showEnglishNames);
            QString teacherRoom = teacher;
            if (!entry.room.trimmed().isEmpty())
            {
                teacherRoom += QStringLiteral(" ") + entry.room.trimmed();
            }
            if (!teacherRoom.trimmed().isEmpty())
            {
                parts.append(teacherRoom.simplified());
            }
            QString classLine = entry.grade.trimmed();
            if (!entry.level.trimmed().isEmpty())
            {
                classLine = classLine.isEmpty()
                    ? entry.level.trimmed()
                    : classLine + QStringLiteral(" - ") + entry.level.trimmed();
            }
            if (!classLine.isEmpty())
            {
                parts.append(classLine);
            }
        }
        return parts.join(QStringLiteral("\n"));
    }
    if (cell.plainTestingAssignment)
    {
        return cell.testingRoom.trimmed().isEmpty()
            ? trSchedule("Oral Testing")
            : trSchedule("Oral Testing") + QStringLiteral("\nRm: ")
                + cell.testingRoom.trimmed();
    }
    switch (cell.state)
    {
    case Application::ScheduleSlotState::Empty:
        return {};
    case Application::ScheduleSlotState::Essay:
        return trSchedule("Essay");
    case Application::ScheduleSlotState::Lunch:
        return trSchedule("Lunch");
    }
    return {};
}

} // namespace ClassMngr::Next::Ui
