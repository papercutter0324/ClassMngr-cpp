#include "schedule_import_review_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/colorutils.h"
#include "data/data_service.h"
#include "domain/models/classroom.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/classes/config/class_info_config.h"
#include "features/schedule/ui/schedule_import_dialog_shared.h"
#include "features/schedule/ui/schedule_import_resolution_view.h"
#include "features/schedule/services/schedule_import_review_model.h"
#include "features/schedule/services/schedule_import_review_summary.h"
#include "features/schedule/ui/schedule_time_formatter.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/no_wheel_combobox.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScreen>
#include <QSet>
#include <QSizePolicy>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <utility>

namespace
{
using namespace ScheduleImportResolutionView;
constexpr int ReviewDialogHeight = 820;
constexpr int MaximumReviewDialogWidth = 1180;
constexpr int MaximumPreviewVisibleRows = 6;
constexpr int InitialPreviewWidth = 540;
constexpr int PreviewHeadingSpacer = 16;
constexpr int PreferredResolutionPaneWidth = 380;
constexpr int RegularPreviewFirstHour = 16;
constexpr int RegularPreviewLastHour = 21;

int timeMinutes(
    const QString& value
    )
{
    QTime time;
    const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm")
    };
    for (const QString& format : formats)
    {
        time =
            QTime::fromString(
                value.trimmed(),
                format
                );
        if (time.isValid())
        {
            break;
        }
    }
    return time.isValid()
        ? time.hour() * 60 + time.minute()
        : -1;
}

int weekdayIndex(
    const QString& day
    );

QString weekdayLabel(
    const QString& day
    );

int weekdayIndex(
    const QString& day
    )
{
    const int index =
        ClassInfoConfig::Days.indexOf(day.trimmed());
    return index >= 0
        ? index
        : std::numeric_limits<int>::max();
}

QString weekdayLabel(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QObject::tr("Mon.");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QObject::tr("Tues.");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QObject::tr("Wed.");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QObject::tr("Thurs.");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QObject::tr("Fri.");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QObject::tr("Sat.");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QObject::tr("Sun.");
    }
    return day.trimmed();
}

QString compactTimeDisplay(
    const QString& value
    )
{
    const QTime time =
        QTime::fromString(
            value,
            QStringLiteral("h:mm AP")
            );
    if (!time.isValid())
    {
        return value;
    }

    const QString minutePart =
        time.minute() == 0
            ? QString()
            : QStringLiteral(":%1")
                  .arg(time.minute(), 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1%2%3")
        .arg(
            time.hour() == 0 || time.hour() == 12
                ? 12
                : time.hour() % 12
            )
        .arg(minutePart)
        .arg(time.hour() < 12 ? QStringLiteral("am") : QStringLiteral("pm"));
}

QString reconciliationTimeDisplay(
    const QString& value
    )
{
    const QTime time =
        QTime::fromString(
            value,
            QStringLiteral("h:mm AP")
            );
    if (!time.isValid())
    {
        return value.toHtmlEscaped();
    }

    return QStringLiteral("%1:%2%3")
        .arg(
            time.hour() == 0 || time.hour() == 12
                ? 12
                : time.hour() % 12
            )
        .arg(
            time.minute(),
            2,
            10,
            QLatin1Char('0')
            )
        .arg(
            time.hour() < 12
                ? QStringLiteral("am")
                : QStringLiteral("pm")
            );
}

QString reconciliationMeetingText(
    const ClassTime& time
    )
{
    return QStringLiteral("%1 %2 - %3")
        .arg(
            weekdayLabel(time.day).toHtmlEscaped(),
            reconciliationTimeDisplay(time.startTime),
            reconciliationTimeDisplay(time.endTime)
            );
}

QList<ClassTime> orderedMeetings(
    QList<ClassTime> times
    )
{
    std::stable_sort(
        times.begin(),
        times.end(),
        [](const ClassTime& left, const ClassTime& right)
        {
            const int leftDay = weekdayIndex(left.day);
            const int rightDay = weekdayIndex(right.day);
            if (leftDay != rightDay)
            {
                return leftDay < rightDay;
            }
            return timeMinutes(left.startTime)
                < timeMinutes(right.startTime);
        }
        );
    return times;
}

QString meetingDifferenceText(
    const QList<ClassTime>& existingTimes,
    const QList<ClassTime>& importedTimes
    )
{
    const QList<ClassTime> orderedExisting =
        orderedMeetings(existingTimes);
    const QList<ClassTime> orderedImported =
        orderedMeetings(importedTimes);
    QStringList lines;
    int existingIndex = 0;
    int importedIndex = 0;
    while (
        existingIndex < orderedExisting.size()
        || importedIndex < orderedImported.size()
        )
    {
        const bool hasExisting =
            existingIndex < orderedExisting.size();
        const bool hasImported =
            importedIndex < orderedImported.size();
        const int existingDay =
            hasExisting
                ? weekdayIndex(orderedExisting[existingIndex].day)
                : std::numeric_limits<int>::max();
        const int importedDay =
            hasImported
                ? weekdayIndex(orderedImported[importedIndex].day)
                : std::numeric_limits<int>::max();
        const bool sameDay =
            hasExisting
            && hasImported
            && orderedExisting[existingIndex].day.compare(
                orderedImported[importedIndex].day,
                Qt::CaseInsensitive
                ) == 0;
        const bool existingDayComesFirst =
            hasExisting
            && hasImported
            && !sameDay
            && (
                existingDay < importedDay
                || (
                    existingDay == importedDay
                    && orderedExisting[existingIndex].day.compare(
                        orderedImported[importedIndex].day,
                        Qt::CaseInsensitive
                        ) < 0
                    )
                );
        const bool useExisting =
            hasExisting
            && (
                !hasImported
                || existingDayComesFirst
                );
        const bool useImported =
            hasImported
            && (
                !hasExisting
                || (
                    !sameDay
                    && !existingDayComesFirst
                    )
                );
        const QString existing =
            hasExisting && !useImported
                ? reconciliationMeetingText(orderedExisting[existingIndex])
                : QStringLiteral("—");
        const QString imported =
            hasImported && !useExisting
                ? reconciliationMeetingText(orderedImported[importedIndex])
                : QStringLiteral("—");
        lines.append(
            QStringLiteral("%1 → %2")
                .arg(existing, imported)
            );

        if (hasExisting && !useImported)
        {
            ++existingIndex;
        }
        if (hasImported && !useExisting)
        {
            ++importedIndex;
        }
    }

    return lines.join(QStringLiteral("<br>"));
}

QString compactMeetingText(
    const QList<ClassTime>& times
    )
{
    QList<ClassTime> orderedTimes = times;
    std::sort(
        orderedTimes.begin(),
        orderedTimes.end(),
        [](const ClassTime& left, const ClassTime& right)
        {
            const int leftDay = weekdayIndex(left.day);
            const int rightDay = weekdayIndex(right.day);
            if (leftDay != rightDay)
            {
                return leftDay < rightDay;
            }
            return timeMinutes(left.startTime)
                < timeMinutes(right.startTime);
        }
        );

    QStringList text;
    for (const ClassTime& time : orderedTimes)
    {
        text.append(
            QStringLiteral("%1 %2")
                .arg(
                    weekdayLabel(time.day),
                    compactTimeDisplay(time.startTime)
                    )
            );
    }
    return text.join(QStringLiteral(" / "));
}

QStringList meetingKeys(
    const QList<ClassTime>& times
    )
{
    QStringList keys;
    for (const ClassTime& time : times)
    {
        keys.append(
            QStringLiteral("%1\x1f%2\x1f%3")
                .arg(
                    time.day,
                    time.startTime,
                    time.endTime
                    )
            );
    }
    keys.sort(Qt::CaseInsensitive);
    return keys;
}

bool timesOverlap(
    const ClassTime& left,
    const ClassTime& right
    )
{
    if (left.day != right.day)
    {
        return false;
    }
    const int leftStart = timeMinutes(left.startTime);
    const int leftEnd = timeMinutes(left.endTime);
    const int rightStart = timeMinutes(right.startTime);
    const int rightEnd = timeMinutes(right.endTime);
    return leftStart >= 0
        && rightStart >= 0
        && leftEnd > leftStart
        && rightEnd > rightStart
        && leftStart < rightEnd
        && rightStart < leftEnd;
}

QString importedClassConflictLabel(
    const ScheduleImportClassCandidate& candidate
    )
{
    const QString course =
        QStringLiteral("%1 %2")
            .arg(
                candidate.classGrade,
                candidate.classLevel
                )
            .simplified();
    const QString meetings =
        compactMeetingText(candidate.times);
    return QStringLiteral("%1 — %2 (%3)")
        .arg(
            course,
            candidate.teacherKr.trimmed(),
            meetings.isEmpty()
                ? QObject::tr("time unavailable")
                : meetings
            );
}

QStringList projectedScheduleConflicts(
    const ScheduleImportUserBlock& user
    )
{
    struct Occurrence
    {
        QString label;
        ClassTime time;
    };
    QList<Occurrence> occurrences;
    QStringList conflicts;

    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        const QString label =
            QStringLiteral("%1 %2")
                .arg(
                    candidate.classGrade,
                    candidate.classLevel
                    );
        for (const ClassTime& time : candidate.times)
        {
            for (const Occurrence& existing : occurrences)
            {
                if (timesOverlap(time, existing.time))
                {
                    conflicts.append(
                        QObject::tr(
                            "%1 overlaps %2 on %3 (%4 - %5 and %6 - %7)."
                            )
                            .arg(
                                label,
                                existing.label,
                                scheduleImportWeekdayDisplayName(time.day),
                                reconciliationTimeDisplay(
                                    existing.time.startTime
                                    ),
                                reconciliationTimeDisplay(
                                    existing.time.endTime
                                    ),
                                reconciliationTimeDisplay(
                                    time.startTime
                                    ),
                                reconciliationTimeDisplay(
                                    time.endTime
                                    )
                                )
                        );
                }
            }
            occurrences.append({label, time});
        }
    }

    conflicts.removeDuplicates();
    return conflicts;
}

QString classLabel(
    DataService* dataService,
    int classId,
    ScheduleImportKind kind
    )
{
    const Classroom classroom =
        dataService->getClassById(classId);
    const ClassInfo info =
        dataService->loadClassInfo(classId);
    const QString course =
        QStringLiteral("%1 %2")
            .arg(
                info.classGrade,
                info.classLevel
                )
            .simplified();

    const QString label =
        !course.isEmpty()
            ? course
            : !classroom.name.trimmed().isEmpty()
                ? classroom.name.trimmed()
                : QObject::tr("Class %1").arg(classId);
    const Teacher teacher =
        dataService->getTeacher(info.teacherId);
    const bool importingIntensive =
        kind == ScheduleImportKind::Intensive;
    const QList<ClassTime>& preferredTimes =
        importingIntensive
            ? info.intensiveTimes
            : info.classTimes;
    const QList<ClassTime>& fallbackTimes =
        importingIntensive
            ? info.classTimes
            : info.intensiveTimes;
    const bool usesPreferredTimes =
        !preferredTimes.isEmpty();
    const QList<ClassTime>& times =
        usesPreferredTimes
            ? preferredTimes
            : fallbackTimes;
    const QString schedule =
        compactMeetingText(times);
    QStringList detailParts;
    if (!teacher.teacherKr.trimmed().isEmpty())
    {
        detailParts.append(teacher.teacherKr.trimmed());
    }
    if (!schedule.isEmpty())
    {
        detailParts.append(schedule);
    }
    const QString details =
        detailParts.join(QLatin1Char(' '));
    const QString scheduleTag =
        schedule.isEmpty()
            ? QString()
            : usesPreferredTimes
                ? importingIntensive
                    ? QStringLiteral(" ") + QObject::tr("[Int]")
                    : QStringLiteral(" ") + QObject::tr("[Reg]")
                : importingIntensive
                    ? QStringLiteral(" ") + QObject::tr("[Reg]")
                    : QStringLiteral(" ") + QObject::tr("[Int]");
    return details.isEmpty()
        ? label
        : QStringLiteral("%1 (%2)%3")
              .arg(label, details, scheduleTag);
}

QString classDifferences(
    DataService* dataService,
    const ScheduleImportClassCandidate& candidate,
    int targetClassId,
    ScheduleImportKind kind,
    const QString& classColor,
    const QColor& changesColor,
    const QColor& changesHeadingColor
    )
{
    if (!dataService || targetClassId <= 0)
    {
        return QObject::tr(
                   "A new class will be created with color %1."
                   )
            .arg(classColor)
            .toHtmlEscaped();
    }

    const auto differenceItem =
        [](const QString& label,
           const QString& existing,
           const QString& imported)
        {
            return QStringLiteral(
                "<li><b>%1:</b> %2 → %3</li>"
                )
                .arg(
                    label.toHtmlEscaped(),
                    existing.toHtmlEscaped(),
                    imported.toHtmlEscaped()
                    );
        };
    const auto meetingDifferenceItem =
        [](const QString& label,
           const QString& differences)
        {
            return QStringLiteral(
                "<li><b>%1:</b><br>%2</li>"
                )
                .arg(
                    label.toHtmlEscaped(),
                    differences
                    );
        };

    const ClassInfo existing =
        dataService->loadClassInfo(targetClassId);
    const Teacher existingTeacher =
        dataService->getTeacher(existing.teacherId);
    const QList<ClassTime> existingTimes =
        kind == ScheduleImportKind::Intensive
            ? existing.intensiveTimes
            : existing.classTimes;
    QStringList differences;

    if (existing.classGrade != candidate.classGrade)
    {
        differences.append(
            differenceItem(
                QObject::tr("Grade"),
                existing.classGrade,
                candidate.classGrade
                )
            );
    }
    if (existing.classLevel != candidate.classLevel)
    {
        differences.append(
            differenceItem(
                QObject::tr("Level"),
                existing.classLevel,
                candidate.classLevel
                )
            );
    }
    if (
        TeacherImportNameUtils::hangulOnly(
            existingTeacher.teacherKr
            ) != candidate.teacherKey
        )
    {
        differences.append(
            differenceItem(
                QObject::tr("Teacher"),
                existingTeacher.teacherKr,
                candidate.teacherKr
                )
            );
    }
    if (meetingKeys(existingTimes) != meetingKeys(candidate.times))
    {
        differences.append(
            meetingDifferenceItem(
                QObject::tr("Days"),
                meetingDifferenceText(
                    existingTimes,
                    candidate.times
                    )
                )
            );
    }
    if (
        existing.classColor.compare(
            classColor,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        differences.append(
            differenceItem(
                QObject::tr("Color"),
                existing.classColor,
                classColor
                )
            );
    }

    if (differences.isEmpty())
    {
        return QObject::tr(
                   "No grade, level, teacher, day, or color differences."
                   )
            .toHtmlEscaped();
    }

    return QStringLiteral(
        "<span style=\"color:%1\"><b style=\"color:%2\">%3</b>"
        "<ul style=\"margin-top:2px; margin-bottom:0px;\">%4</ul>"
        "</span>"
        )
        .arg(
            changesColor.name(QColor::HexRgb),
            changesHeadingColor.name(QColor::HexRgb),
            QObject::tr("Changes:").toHtmlEscaped(),
            differences.join(QString())
            );
}

QString teacherLabel(
    const Teacher& teacher
    )
{
    return QObject::tr("%1 — Room %2")
        .arg(
            teacher.teacherKr.trimmed(),
            teacher.roomNumber.trimmed().isEmpty()
                ? QObject::tr("not set")
                : teacher.roomNumber.trimmed()
            );
}

int configuredValueOrder(
    const QStringList& configuredValues,
    const QString& value
    )
{
    for (int index = 0; index < configuredValues.size(); ++index)
    {
        if (
            configuredValues[index].compare(
                value.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            return index;
        }
    }
    return std::numeric_limits<int>::max();
}

bool importedClassLess(
    const ScheduleImportClassCandidate& left,
    const ScheduleImportClassCandidate& right
    )
{
    const int leftGrade =
        configuredValueOrder(
            ClassInfoConfig::Grades,
            left.classGrade
            );
    const int rightGrade =
        configuredValueOrder(
            ClassInfoConfig::Grades,
            right.classGrade
            );
    if (leftGrade != rightGrade)
    {
        return leftGrade < rightGrade;
    }
    if (
        leftGrade == std::numeric_limits<int>::max()
        && left.classGrade.compare(
            right.classGrade,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        return left.classGrade.compare(
            right.classGrade,
            Qt::CaseInsensitive
            ) < 0;
    }

    const QString configuredGrade =
        leftGrade == std::numeric_limits<int>::max()
            ? left.classGrade.trimmed()
            : ClassInfoConfig::Grades[leftGrade];
    const QStringList levels =
        ClassInfoConfig::levelsForGrade(
            configuredGrade
            );
    const int leftLevel =
        configuredValueOrder(levels, left.classLevel);
    const int rightLevel =
        configuredValueOrder(levels, right.classLevel);
    if (leftLevel != rightLevel)
    {
        return leftLevel < rightLevel;
    }
    if (
        leftLevel == std::numeric_limits<int>::max()
        && left.classLevel.compare(
            right.classLevel,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        return left.classLevel.compare(
            right.classLevel,
            Qt::CaseInsensitive
            ) < 0;
    }
    return false;
}

ScheduleViewModel previewModel(
    const ScheduleImportUserBlock& user,
    bool useIntensive,
    const ScheduleDisplayState& displayState
    )
{
    const QStringList days =
        visibleScheduleDays(
            displayState.showWeekends
            );

    QSet<int> starts;
    QHash<QString, QList<ScheduleEntry>> entries;
    QHash<QString, QString> slotStates;
    bool uses55Endings = false;

    if (useIntensive)
    {
        for (const IntensiveSlotState& state :
             user.intensiveSlotStates)
        {
            const int start =
                timeMinutes(state.startTime);
            if (start < 0)
            {
                continue;
            }
            starts.insert(start);
            slotStates.insert(
                state.day
                    + QLatin1Char('\x1f')
                    + QString::number(start),
                state.state
                );
        }
    }

    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        ScheduleEntry entry;
        entry.teacherKr = candidate.teacherKr;
        entry.roomNumber =
            candidate.rooms.isEmpty()
                ? QString()
                : candidate.rooms.first();
        entry.classGrade = candidate.classGrade;
        entry.classLevel = candidate.classLevel;
        entry.classColor =
            candidate.importedColors.isEmpty()
                ? QStringLiteral("#FFFFFF")
                : candidate.importedColors.first();
        entry.fontColor =
            ColorUtils::getContrastingFontColor(
                QColor(entry.classColor)
                );

        for (const ClassTime& time : candidate.times)
        {
            const int start =
                timeMinutes(time.startTime);
            if (start < 0)
            {
                continue;
            }
            starts.insert(start);
            const int end =
                timeMinutes(time.endTime);
            if (end >= 0 && end % 60 == 55)
            {
                uses55Endings = true;
            }
            entries[
                time.day
                + QLatin1Char('\x1f')
                + QString::number(start)
                ].append(entry);
        }
    }

    if (!useIntensive)
    {
        for (int hour = RegularPreviewFirstHour;
             hour <= RegularPreviewLastHour;
             ++hour)
        {
            starts.insert(hour * 60);
        }
    }

    QList<int> sortedStarts =
        starts.values();
    std::sort(
        sortedStarts.begin(),
        sortedStarts.end()
        );

    ScheduleViewModel model;
    model.days = days;
    model.uses55Endings =
        !useIntensive
        && uses55Endings;

    for (int start : sortedStarts)
    {
        ScheduleRowView row;
        const QTime startTime(
            start / 60,
            start % 60
            );
        row.timeLabel =
            startTime.toString(
                QStringLiteral("HH:mm")
                );
        row.timeRangeLabel =
            ScheduleTimeFormatter::rangeLabel(
                row.timeLabel,
                model.uses55Endings,
                displayState.use24HourTime
                );

        for (const QString& day : days)
        {
            ScheduleCellView cell;
            cell.day = day;
            cell.timeLabel = row.timeLabel;
            cell.entries =
                entries.value(
                    day
                    + QLatin1Char('\x1f')
                    + QString::number(start)
                    );
            cell.defaultSlotState =
                useIntensive
                    ? scheduleEmptySlotState()
                    : scheduleEssaySlotState();
            cell.slotState =
                useIntensive
                    ? slotStates.value(
                        day
                            + QLatin1Char('\x1f')
                            + QString::number(start),
                        scheduleEmptySlotState()
                        )
                    : cell.entries.isEmpty()
                        ? scheduleEssaySlotState()
                        : scheduleEmptySlotState();
            row.maxEntryCount =
                std::max(
                    row.maxEntryCount,
                    static_cast<int>(
                        cell.entries.size()
                        )
                    );
            if (!cell.entries.isEmpty())
            {
                ++model.summary.scheduledBlocks;
            }
            else if (
                cell.slotState
                    == scheduleEssaySlotState()
                )
            {
                ++model.summary.essayBlocks;
                ++model.summary.scheduledBlocks;
            }
            row.cells.append(cell);
        }

        model.rows.append(row);
    }

    if (!useIntensive)
    {
        return model;
    }

    int firstVisibleRow = -1;
    int lastVisibleRow = -1;
    for (int rowIndex = 0;
         rowIndex < model.rows.size();
         ++rowIndex)
    {
        const ScheduleRowView& row = model.rows[rowIndex];
        const bool hasVisibleContent =
            std::any_of(
                row.cells.cbegin(),
                row.cells.cend(),
                [](const ScheduleCellView& cell)
                {
                    return !cell.entries.isEmpty()
                        || cell.slotState != scheduleEmptySlotState();
                }
                );
        if (!hasVisibleContent)
        {
            continue;
        }

        if (firstVisibleRow < 0)
        {
            firstVisibleRow = rowIndex;
        }
        lastVisibleRow = rowIndex;
    }

    model.rows = firstVisibleRow >= 0
        ? model.rows.mid(
            firstVisibleRow,
            lastVisibleRow - firstVisibleRow + 1
            )
        : QList<ScheduleRowView>();

    return model;
}
}

ScheduleImportReviewDialog::ScheduleImportReviewDialog(
    ApplicationServices* services,
    ScheduleImportReviewRequest request,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("scheduleImportReview"), parent)
    , m_services(services)
    , m_request(std::move(request))
{
    setWindowTitle(tr("Review & Reconcile"));
    setModal(true);
    buildUi();
}

void ScheduleImportReviewDialog::buildUi()
{
    auto* outerLayout = contentLayout();
    m_reviewPage =
        new QWidget(this);
    m_reviewPage->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Ignored
        );
    auto* layout =
        new QVBoxLayout(m_reviewPage);
    auto* heading =
        new QLabel(
            tr("Review & Reconcile"),
            m_reviewPage
            );
    heading->setObjectName(
        QStringLiteral("pageTitle")
        );
    heading->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );
    layout->addWidget(heading);

    auto* subtitle =
        new QLabel(
            tr("Review imported classes and resolve any conflicts before continuing."),
            m_reviewPage
            );
    subtitle->setObjectName(
        QStringLiteral("pageSubtitle")
        );
    subtitle->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );
    layout->addWidget(subtitle);

    m_intensiveModeSection =
        new QGroupBox(
            tr("Existing intensive schedule"),
            m_reviewPage
            );
    m_intensiveModeSection->setObjectName(
        QStringLiteral("scheduleImportIntensiveModeSection")
        );
    auto* intensiveModeLayout =
        new QVBoxLayout(m_intensiveModeSection);
    auto* intensiveModePrompt =
        new QLabel(
            tr("How should this import handle the existing intensive schedule?"),
            m_intensiveModeSection
            );
    intensiveModePrompt->setWordWrap(true);
    m_updateIntensiveRadio =
        new QRadioButton(
            tr("Update existing intensive schedule"),
            m_intensiveModeSection
            );
    m_updateIntensiveRadio->setObjectName(
        QStringLiteral("scheduleImportUpdateIntensiveRadio")
        );
    m_replaceIntensiveRadio =
        new QRadioButton(
            tr("Create a brand-new intensive schedule"),
            m_intensiveModeSection
            );
    m_replaceIntensiveRadio->setObjectName(
        QStringLiteral("scheduleImportReplaceIntensiveRadio")
        );
    m_updateIntensiveRadio->setChecked(true);
    intensiveModeLayout->addWidget(intensiveModePrompt);
    intensiveModeLayout->addWidget(m_updateIntensiveRadio);
    intensiveModeLayout->addWidget(m_replaceIntensiveRadio);
    m_intensiveModeSection->setVisible(false);
    layout->addWidget(m_intensiveModeSection);

    m_reviewSplitter =
        new QSplitter(
            Qt::Horizontal,
            m_reviewPage
            );
    m_reviewSplitter->setObjectName(
        QStringLiteral("scheduleImportReviewSplitter")
        );
    m_reviewSplitter->setChildrenCollapsible(false);

    m_previewPane =
        new QWidget(m_reviewSplitter);
    m_previewPane->setObjectName(
        QStringLiteral("scheduleImportPreviewPane")
        );
    auto* previewLayout =
        new QVBoxLayout(m_previewPane);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);
    m_previewHeading =
        new QLabel(
            tr("Schedule Preview"),
            m_previewPane
            );
    m_previewHeading->setObjectName(
        QStringLiteral("scheduleImportPreviewHeading")
        );
    m_previewHeading->setAlignment(
        Qt::AlignHCenter | Qt::AlignTop
        );
    previewLayout->addWidget(
        m_previewHeading,
        0,
        Qt::AlignTop
        );
    previewLayout->addSpacing(PreviewHeadingSpacer);
    m_previewWidget =
        new ScheduleWidget(
            m_services,
            m_previewPane,
            ScheduleMode::ReadOnly
            );
    m_previewWidget->setObjectName(
        QStringLiteral("scheduleImportPreview")
        );
    m_previewWidget->setCompactPreview(true);
    m_previewWidget->setMaximumVisibleRows(
        MaximumPreviewVisibleRows
        );
    m_previewWidget->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );
    previewLayout->addWidget(
        m_previewWidget,
        1
        );

    m_resolutionTabs =
        new QTabWidget(m_reviewSplitter);
    m_resolutionTabs->setObjectName(
        QStringLiteral("scheduleImportResolutionTabs")
        );

    m_teacherScrollArea =
        new QScrollArea(m_resolutionTabs);
    m_teacherScrollArea->setObjectName(
        QStringLiteral("scheduleImportTeacherScrollArea")
        );
    m_teacherScrollArea->setWidgetResizable(true);
    m_teacherScrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
        );
    m_teacherContent =
        new QWidget(m_teacherScrollArea);
    m_teacherContent->setObjectName(
        QStringLiteral("scheduleImportTeacherGroup")
        );
    m_teacherLayout =
        new QVBoxLayout(m_teacherContent);
    m_teacherScrollArea->setWidget(m_teacherContent);
    m_classScrollArea =
        new QScrollArea(m_resolutionTabs);
    m_classScrollArea->setObjectName(
        QStringLiteral("scheduleImportClassScrollArea")
        );
    m_classScrollArea->setWidgetResizable(true);
    m_classScrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
        );
    m_classContent =
        new QWidget(m_classScrollArea);
    m_classContent->setObjectName(
        QStringLiteral("scheduleImportClassesGroup")
        );
    m_classLayout =
        new QVBoxLayout(m_classContent);
    m_classScrollArea->setWidget(m_classContent);
    m_resolutionTabs->addTab(
        m_classScrollArea,
        tr("Classes")
        );
    m_resolutionTabs->addTab(
        m_teacherScrollArea,
        tr("Korean Teachers")
        );
    m_resolutionTabs->setCurrentWidget(m_classScrollArea);

    m_reviewSplitter->addWidget(m_previewPane);
    m_reviewSplitter->addWidget(m_resolutionTabs);
    m_reviewSplitter->setStretchFactor(0, 0);
    m_reviewSplitter->setStretchFactor(1, 1);
    layout->addWidget(m_reviewSplitter, 1);

    m_reviewStatus =
        new QLabel(m_reviewPage);
    m_reviewStatus->setObjectName(
        QStringLiteral("scheduleImportReviewStatus")
        );
    m_reviewStatus->setWordWrap(true);
    layout->addWidget(m_reviewStatus);

    m_reviewSummary =
        new QLabel(m_reviewPage);
    m_reviewSummary->setObjectName(
        QStringLiteral("scheduleImportReviewSummary")
        );
    m_reviewSummary->setWordWrap(true);
    layout->addWidget(m_reviewSummary);
    outerLayout->addWidget(m_reviewPage, 1);

    m_buttons = addButtonBox(QDialogButtonBox::Cancel);
    m_backButton =
        m_buttons->addButton(
            tr("Back"),
            QDialogButtonBox::ActionRole
            );
    m_importButton =
        m_buttons->addButton(
            tr("Import"),
            QDialogButtonBox::ActionRole
            );
    m_backButton->setObjectName(
        QStringLiteral("scheduleImportBackButton")
        );
    m_importButton->setObjectName(
        QStringLiteral("scheduleImportAcceptButton")
        );
    m_importButton->setEnabled(false);
    m_importButton->setDefault(true);
    m_importButton->setAutoDefault(true);
    connect(
        m_backButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );
    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &ScheduleImportReviewDialog::applyImport
        );
    connect(
        m_updateIntensiveRadio,
        &QRadioButton::toggled,
        this,
        &ScheduleImportReviewDialog::updateReviewState
        );
    connect(
        m_replaceIntensiveRadio,
        &QRadioButton::toggled,
        this,
        &ScheduleImportReviewDialog::updateReviewState
        );
}

bool ScheduleImportReviewDialog::prepare()
{
    if (m_prepared)
    {
        return true;
    }

    DataService* dataService =
        openScheduleImportDataService(m_services);
    if (!dataService)
    {
        return false;
    }

    const auto preview =
        dataService->previewScheduleImport(
            m_request.user,
            m_request.kind
            );
    if (!preview)
    {
        DialogServices::showWarning(
            this,
            tr("Import Schedule"),
            preview.error()
            );
        return false;
    }

    m_preview = *preview;
    m_intensiveModeSection->setVisible(
        m_request.kind == ScheduleImportKind::Intensive
        && m_preview.inventory.hasIntensiveHours
        );
    m_updateIntensiveRadio->setChecked(true);
    m_previewWidget->setPreviewModel(
        previewModel(
            m_preview.user,
            m_request.kind == ScheduleImportKind::Intensive,
            m_previewWidget->displayState()
            )
        );
    rebuildResolutionControls();
    updateReviewState();
    m_prepared = true;
    resizeForReviewStage();
    QTimer::singleShot(
        0,
        this,
        &ScheduleImportReviewDialog::resizeForReviewStage
        );
    return true;
}

void ScheduleImportReviewDialog::rebuildResolutionControls()
{
    clearLayout(m_teacherLayout);
    clearLayout(m_classLayout);

    m_teacherControls.clear();
    m_classControls.clear();
    m_warningLabel = nullptr;
    m_warningAcknowledgement = nullptr;

    if (m_warningScrollArea)
    {
        const int warningTabIndex =
            m_resolutionTabs->indexOf(m_warningScrollArea);
        if (warningTabIndex >= 0)
        {
            m_resolutionTabs->removeTab(warningTabIndex);
        }
        delete m_warningScrollArea;
        m_warningScrollArea = nullptr;
    }

    DataService* dataService =
        openScheduleImportDataService(m_services);
    if (!dataService)
    {
        return;
    }

    if (!m_preview.user.diagnostics.isEmpty())
    {
        m_warningScrollArea =
            new QScrollArea(m_resolutionTabs);
        m_warningScrollArea->setObjectName(
            QStringLiteral("scheduleImportWarningScrollArea")
            );
        m_warningScrollArea->setWidgetResizable(true);
        m_warningScrollArea->setHorizontalScrollBarPolicy(
            Qt::ScrollBarAlwaysOff
            );
        auto* warnings =
            new QWidget(m_warningScrollArea);
        auto* warningsLayout =
            new QVBoxLayout(warnings);
        QStringList lines;
        for (const ScheduleImportDiagnostic& diagnostic :
             m_preview.user.diagnostics)
        {
            lines.append(
                tr("%1: %2")
                    .arg(
                        diagnostic.cellReference,
                        diagnostic.value.trimmed()
                        )
                );
        }
        m_warningLabel =
            new QLabel(
                lines.join(QLatin1Char('\n')),
                warnings
                );
        m_warningLabel->setWordWrap(true);
        m_warningAcknowledgement =
            new QCheckBox(
                tr("I reviewed these cells and want to skip them."),
                warnings
                );
        m_warningAcknowledgement->setObjectName(
            QStringLiteral("scheduleImportWarningAcknowledgement")
            );
        warningsLayout->addWidget(m_warningLabel);
        warningsLayout->addWidget(
            m_warningAcknowledgement
            );
        warningsLayout->addStretch();
        m_warningScrollArea->setWidget(warnings);
        m_resolutionTabs->insertTab(
            2,
            m_warningScrollArea,
            tr("Unrecognized cells")
            );
        m_resolutionTabs->setCurrentWidget(m_classScrollArea);
        connect(
            m_warningAcknowledgement,
            &QCheckBox::toggled,
            this,
            &ScheduleImportReviewDialog::updateReviewState
            );
    }


    for (int row = 0;
         row < m_preview.teachers.size();
         ++row)
    {
        const ScheduleImportTeacherPreview& preview =
            m_preview.teachers[row];
        TeacherControl control;
        control.teacherKey = preview.teacherKey;
        control.action =
            new NoWheelComboBox(m_teacherContent);
        control.room =
            new NoWheelComboBox(m_teacherContent);
        configureCompactActionCombo(control.action);
        configureCompactActionCombo(control.room);
        control.action->setObjectName(
            QStringLiteral("scheduleImportTeacherAction_%1")
                .arg(row)
            );
        control.room->setObjectName(
            QStringLiteral("scheduleImportTeacherRoom_%1")
                .arg(row)
            );
        if (preview.importedRooms.size() > 1)
        {
            control.room->addItem(
                tr("Choose a room..."),
                QString()
                );
        }
        for (const QString& room : preview.importedRooms)
        {
            control.room->addItem(room, room);
        }

        if (preview.matchingTeacherIds.size() == 1)
        {
            const int teacherId =
                preview.matchingTeacherIds.first();
            const Teacher existing =
                dataService->getTeacher(teacherId);
            const bool exactRoom =
                preview.importedRooms.size() == 1
                && existing.roomNumber.trimmed()
                    == preview.importedRooms.first().trimmed();

            if (!exactRoom)
            {
                addResolutionItem(
                    control.action,
                    tr("Choose a resolution..."),
                    -1
                    );
            }
            addResolutionItem(
                control.action,
                tr("Keep existing: %1")
                    .arg(teacherLabel(existing)),
                static_cast<int>(
                    ScheduleImportTeacherAction::Reuse
                    ),
                teacherId
                );
            if (!exactRoom)
            {
                addResolutionItem(
                    control.action,
                    tr("Update room globally (%1 affected classes)")
                        .arg(preview.affectedClassCount),
                    static_cast<int>(
                        ScheduleImportTeacherAction::UpdateRoom
                        ),
                    teacherId
                    );
                addResolutionItem(
                    control.action,
                    tr("Skip affected classes"),
                    static_cast<int>(
                        ScheduleImportTeacherAction::Skip
                        )
                    );
            }
        }
        else
        {
            if (!preview.matchingTeacherIds.isEmpty())
            {
                addResolutionItem(
                    control.action,
                    tr("Choose a resolution..."),
                    -1
                    );
            }
            for (int teacherId : preview.matchingTeacherIds)
            {
                const Teacher existing =
                    dataService->getTeacher(teacherId);
                addResolutionItem(
                    control.action,
                    tr("Use existing: %1")
                        .arg(teacherLabel(existing)),
                    static_cast<int>(
                        ScheduleImportTeacherAction::Reuse
                        ),
                    teacherId
                    );
                addResolutionItem(
                    control.action,
                    tr("Update existing room: %1")
                        .arg(teacherLabel(existing)),
                    static_cast<int>(
                        ScheduleImportTeacherAction::UpdateRoom
                        ),
                    teacherId
                    );
            }
            addResolutionItem(
                control.action,
                tr("Create a new Korean teacher"),
                static_cast<int>(
                    ScheduleImportTeacherAction::Create
                    )
                );
            addResolutionItem(
                control.action,
                tr("Skip affected classes"),
                static_cast<int>(
                    ScheduleImportTeacherAction::Skip
                    )
                );
        }

        auto* teacherCard =
            createReconciliationCard(
                m_teacherContent,
                QStringLiteral("scheduleImportTeacherCard_%1")
                    .arg(row)
                );
        auto* teacherCardLayout =
            new QVBoxLayout(teacherCard);
        auto* spreadsheetTeacher =
            new QLabel(
                QStringLiteral("%1 (%2)")
                    .arg(
                        preview.teacherKr,
                        preview.importedRooms.join(
                            QStringLiteral(", ")
                            )
                        ),
                teacherCard
                );
        spreadsheetTeacher->setObjectName(
            QStringLiteral("scheduleImportTeacherSource_%1")
                .arg(row)
            );
        QFont teacherFont = spreadsheetTeacher->font();
        teacherFont.setBold(true);
        spreadsheetTeacher->setFont(teacherFont);
        spreadsheetTeacher->setWordWrap(true);
        spreadsheetTeacher->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );
        teacherCardLayout->addWidget(
            spreadsheetTeacher
            );
        addLabeledControlRow(
            teacherCardLayout,
            tr("Import Action"),
            control.action,
            teacherCard
            );
        addLabeledControlRow(
            teacherCardLayout,
            tr("Imported Room"),
            control.room,
            teacherCard
            );
        connect(
            control.action,
            &QComboBox::currentIndexChanged,
            this,
            &ScheduleImportReviewDialog::updateReviewState
            );
        connect(
            control.room,
            &QComboBox::currentIndexChanged,
            this,
            &ScheduleImportReviewDialog::updateReviewState
            );
        m_teacherLayout->addWidget(teacherCard);
        m_teacherControls.append(control);
    }
    m_teacherLayout->addStretch();

    const QList<Classroom> allClasses =
        dataService->getClasses();
    QList<ScheduleImportClassPreview> orderedClasses =
        m_preview.classes;
    std::stable_sort(
        orderedClasses.begin(),
        orderedClasses.end(),
        [this](
            const ScheduleImportClassPreview& left,
            const ScheduleImportClassPreview& right
            )
        {
            return importedClassLess(
                m_preview.user.classes[left.candidateIndex],
                m_preview.user.classes[right.candidateIndex]
                );
        }
        );

    for (const ScheduleImportClassPreview& preview :
         orderedClasses)
    {
        const ScheduleImportClassCandidate& candidate =
            m_preview.user.classes[
                preview.candidateIndex
                ];
        ClassControl control;
        control.candidateIndex =
            preview.candidateIndex;
        control.teacherKey =
            candidate.teacherKey;
        control.action =
            new NoWheelComboBox(m_classContent);
        configureCompactActionCombo(control.action);
        control.colorButton =
            new QPushButton(m_classContent);
        control.color =
            candidate.importedColors.isEmpty()
                ? QStringLiteral("#FFFFFF")
                : candidate.importedColors.first().toUpper();
        control.details =
            new QLabel(m_classContent);
        control.action->setObjectName(
            QStringLiteral("scheduleImportClassAction_%1")
                .arg(preview.candidateIndex)
            );
        control.details->setObjectName(
            QStringLiteral("scheduleImportClassDifferences_%1")
                .arg(preview.candidateIndex)
            );
        control.colorButton->setObjectName(
            QStringLiteral("scheduleImportClassColor_%1")
                .arg(preview.candidateIndex)
            );
        control.details->setWordWrap(true);
        control.details->setTextFormat(Qt::RichText);
        updateClassColorButton(&control);

        if (
            !preview.exactMatch
            && !preview.matchingClassIds.isEmpty()
            )
        {
            addResolutionItem(
                control.action,
                tr("Choose Update, Create, or Skip..."),
                -1
                );
        }

        QSet<int> addedTargets;
        for (int classId : preview.matchingClassIds)
        {
            addResolutionItem(
                control.action,
                (classId == preview.suggestedClassId
                    ? tr("Update suggested: %1")
                    : tr("Update existing: %1"))
                    .arg(
                        classLabel(
                            dataService,
                            classId,
                            m_request.kind
                            )
                        ),
                static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    ),
                classId
                );
            addedTargets.insert(classId);
        }
        for (const Classroom& classroom : allClasses)
        {
            if (addedTargets.contains(classroom.id))
            {
                continue;
            }
            const ClassInfo info =
                dataService->loadClassInfo(classroom.id);
            if (
                !scheduleImportClassOptionIsEligible(
                    candidate,
                    info,
                    m_request.kind
                    )
                )
            {
                continue;
            }
            addResolutionItem(
                control.action,
                tr("Update existing: %1")
                    .arg(
                        classLabel(
                            dataService,
                            classroom.id,
                            m_request.kind
                            )
                        ),
                static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    ),
                classroom.id
                );
        }
        addResolutionItem(
            control.action,
            tr("Create new class"),
            static_cast<int>(
                ScheduleImportClassAction::CreateNew
                )
            );
        addResolutionItem(
            control.action,
            tr("Skip imported class"),
            static_cast<int>(
                ScheduleImportClassAction::Skip
                ),
            preview.suggestedClassId
            );

        if (preview.suggestedClassId > 0)
        {
            control.action->setCurrentIndex(
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::UpdateExisting
                        ),
                    preview.suggestedClassId
                    )
                );
        }
        else if (preview.matchingClassIds.isEmpty())
        {
            control.action->setCurrentIndex(
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::CreateNew
                        )
                    )
                );
        }

        const QString importedMeetings =
            compactMeetingText(candidate.times);
        auto* classCard =
            createReconciliationCard(
                m_classContent,
                QStringLiteral("scheduleImportClassCard_%1")
                    .arg(preview.candidateIndex)
                );
        auto* classCardLayout =
            new QVBoxLayout(classCard);
        auto* importedClassLabel =
            new QLabel(
                QStringLiteral("%1 %2 — %3\n(%4)")
                    .arg(
                        candidate.classGrade,
                        candidate.classLevel,
                        candidate.teacherKr,
                        importedMeetings.isEmpty()
                            ? tr("time unavailable")
                            : importedMeetings
                        ),
                classCard
                );
        importedClassLabel->setObjectName(
            QStringLiteral("scheduleImportClassCandidate_%1")
                .arg(preview.candidateIndex)
            );
        QFont classFont = importedClassLabel->font();
        classFont.setBold(true);
        importedClassLabel->setFont(classFont);
        importedClassLabel->setWordWrap(true);
        importedClassLabel->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );
        auto* classHeaderLayout = new QHBoxLayout();
        classHeaderLayout->setContentsMargins(0, 0, 0, 0);
        classHeaderLayout->setSpacing(8);
        auto* colorLayout = new QVBoxLayout();
        colorLayout->setContentsMargins(0, 0, 0, 0);
        colorLayout->setSpacing(0);
        auto* colorLabel =
            new QLabel(
                tr("Color"),
                classCard
                );
        colorLabel->setObjectName(
            QStringLiteral("scheduleImportClassColorLabel_%1")
                .arg(preview.candidateIndex)
            );
        colorLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
        colorLayout->addWidget(
            colorLabel,
            0,
            Qt::AlignRight | Qt::AlignTop
            );
        colorLayout->addWidget(
            control.colorButton,
            0,
            Qt::AlignRight | Qt::AlignTop
            );
        classHeaderLayout->addWidget(
            importedClassLabel,
            1,
            Qt::AlignLeft | Qt::AlignBottom
            );
        classHeaderLayout->addLayout(colorLayout);
        classCardLayout->addLayout(classHeaderLayout);
        auto* matchStatus =
            new QLabel(
                preview.matchExplanation,
                classCard
                );
        matchStatus->setObjectName(
            QStringLiteral("scheduleImportClassMatch_%1")
                .arg(preview.candidateIndex)
            );
        matchStatus->setWordWrap(true);
        matchStatus->setProperty(
            "matchConfidence",
            static_cast<int>(preview.matchConfidence)
            );
        classCardLayout->addWidget(matchStatus);
        addLabeledControlRow(
            classCardLayout,
            tr("Import Action"),
            control.action,
            classCard
            );
        classCardLayout->addWidget(
            control.details
            );
        connect(
            control.action,
            &QComboBox::currentIndexChanged,
            this,
            &ScheduleImportReviewDialog::updateReviewState
            );
        connect(
            control.colorButton,
            &QPushButton::clicked,
            this,
            [this, candidateIndex = preview.candidateIndex]()
            {
                chooseClassColor(candidateIndex);
            }
            );
        m_classLayout->addWidget(classCard);
        m_classControls.append(control);
    }
    m_classLayout->addStretch();
}

void ScheduleImportReviewDialog::chooseClassColor(
    int candidateIndex
    )
{
    for (ClassControl& control : m_classControls)
    {
        if (control.candidateIndex != candidateIndex)
        {
            continue;
        }

        const QColor selected =
            ColorUtils::getColor(
                QColor(control.color),
                this,
                tr("Select Imported Class Color"),
                openScheduleImportDataService(m_services)
                );
        if (!selected.isValid())
        {
            return;
        }

        control.color =
            selected.name(QColor::HexRgb)
                .toUpper();
        updateClassColorButton(&control);
        updateReviewState();
        return;
    }
}

void ScheduleImportReviewDialog::updateClassColorButton(
    ClassControl* control
    )
{
    if (!control || !control->colorButton)
    {
        return;
    }

    QColor color(control->color);
    if (!color.isValid())
    {
        color = QColor(QStringLiteral("#FFFFFF"));
        control->color = QStringLiteral("#FFFFFF");
    }
    const QString description =
        QStringLiteral("%1 (%2)")
            .arg(
                tr("Select Imported Class Color"),
                control->color
                );
    control->colorButton->setText(QString());
    control->colorButton->setToolTip(description);
    control->colorButton->setAccessibleName(description);
    control->colorButton->setFixedSize(
        UiConstants::ClassInfo::Details::ColorPreviewWidth,
        UiConstants::ClassInfo::Details::ColorPreviewHeight
        );
    control->colorButton->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );
    control->colorButton->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "background-color:%1;"
            "border:1px solid #777;"
            "border-radius:4px;"
            "padding:0;"
            "}"
            )
            .arg(control->color)
        );
}

void ScheduleImportReviewDialog::updateScheduleConflictWarning(
    const QStringList& conflicts
    )
{
    QStringList uniqueConflicts = conflicts;
    uniqueConflicts.removeDuplicates();
    const QString signature =
        uniqueConflicts.join(QLatin1Char('\x1f'));
    m_activeScheduleConflictSignature = signature;

    if (signature.isEmpty())
    {
        m_lastWarnedScheduleConflictSignature.clear();
        m_pendingScheduleConflictSignature.clear();
        m_pendingScheduleConflictMessage.clear();
        return;
    }

    if (signature == m_lastWarnedScheduleConflictSignature)
    {
        return;
    }

    m_pendingScheduleConflictSignature = signature;
    m_pendingScheduleConflictMessage =
        tr("Review these schedule conflicts before importing:\n\n%1\n\n"
           "Choose a different existing class, create a new class, or skip an imported class.")
            .arg(uniqueConflicts.join(QLatin1Char('\n')));

    if (m_scheduleConflictWarningQueued)
    {
        return;
    }

    m_scheduleConflictWarningQueued = true;
    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            m_scheduleConflictWarningQueued = false;
            if (
                m_pendingScheduleConflictSignature.isEmpty()
                || m_pendingScheduleConflictSignature
                    != m_activeScheduleConflictSignature
                || m_pendingScheduleConflictSignature
                    == m_lastWarnedScheduleConflictSignature
                )
            {
                return;
            }

            m_lastWarnedScheduleConflictSignature =
                m_pendingScheduleConflictSignature;
            DialogServices::prompts().showMessageAsync(
                PromptRequest{
                    .parent = this,
                    .objectName = QStringLiteral("scheduleImportConflictWarning"),
                    .title = tr("Schedule Import Conflict"),
                    .message = m_pendingScheduleConflictMessage,
                    .severity = PromptSeverity::Warning
                }
                );
        }
        );
}

void ScheduleImportReviewDialog::updateReviewState()
{
    bool valid = true;
    QString message;
    const bool preservesAbsentIntensiveClasses =
        m_request.kind == ScheduleImportKind::Intensive
        && m_updateIntensiveRadio
        && m_updateIntensiveRadio->isChecked();
    QSet<QString> skippedTeachers;
    QHash<QString, int> teacherActions;
    QHash<QString, int> teacherTargets;
    QHash<QString, QString> teacherRooms;

    for (const TeacherControl& control : m_teacherControls)
    {
        const int action =
            control.action->currentData(
                ActionRole
                ).toInt();
        const QString room =
            control.room->currentData().toString();
        teacherActions.insert(control.teacherKey, action);
        teacherTargets.insert(
            control.teacherKey,
            control.action->currentData(TargetRole).toInt()
            );
        teacherRooms.insert(control.teacherKey, room);

        if (action < 0)
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose a resolution for every Korean teacher.");
            }
            continue;
        }
        if (
            (
                action == static_cast<int>(
                    ScheduleImportTeacherAction::Create
                    )
                || action == static_cast<int>(
                    ScheduleImportTeacherAction::UpdateRoom
                    )
                || (
                    action == static_cast<int>(
                        ScheduleImportTeacherAction::Reuse
                        )
                    && control.room->findData(QString()) >= 0
                    )
                )
            && room.isEmpty()
            )
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose one imported room for this Korean teacher resolution.");
            }
            continue;
        }
        if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::Skip
                )
            )
        {
            skippedTeachers.insert(
                control.teacherKey
                );
        }
        else if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::Create
                )
            )
        {
        }
        else if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::UpdateRoom
                )
            )
        {
        }
    }

    DataService* dataService =
        openScheduleImportDataService(m_services);
    QSet<int> targets;
    QMap<int, QList<int>> candidateIndexesByTarget;
    QHash<int, int> classActions;
    QHash<int, int> classTargets;
    QStringList scheduleConflicts;

    for (const ClassControl& control : m_classControls)
    {
        if (skippedTeachers.contains(control.teacherKey))
        {
            const int skipIndex =
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::Skip
                        )
                    );
            if (skipIndex >= 0)
            {
                control.action->setCurrentIndex(skipIndex);
            }
        }

        const int action =
            control.action->currentData(
                ActionRole
                ).toInt();
        const int target =
            control.action->currentData(
                TargetRole
                ).toInt();
        classActions.insert(control.candidateIndex, action);
        classTargets.insert(control.candidateIndex, target);

        if (action < 0)
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose an action for every class.");
            }
            if (control.details)
            {
                control.details->setText(
                    tr("Choose how this imported row should be reconciled.")
                    );
            }
            continue;
        }

        if (
            action == static_cast<int>(
                ScheduleImportClassAction::UpdateExisting
                )
            )
        {
            if (target <= 0)
            {
                valid = false;
                message =
                    tr("Choose an existing class to update.");
            }
            if (control.details)
            {
                control.details->setText(
                    classDifferences(
                        openScheduleImportDataService(m_services),
                        m_preview.user.classes[
                            control.candidateIndex
                        ],
                        target,
                        m_request.kind,
                        control.color,
                        control.details->palette().color(
                            QPalette::Link
                            ),
                        control.details->palette().color(
                            QPalette::Text
                            )
                        )
                    );
            }
        }
        else if (
            action == static_cast<int>(
                ScheduleImportClassAction::CreateNew
                )
            )
        {
            if (control.details)
            {
                control.details->setText(
                    classDifferences(
                        openScheduleImportDataService(m_services),
                        m_preview.user.classes[
                            control.candidateIndex
                        ],
                        -1,
                        m_request.kind,
                        control.color,
                        control.details->palette().color(
                            QPalette::Link
                            ),
                        control.details->palette().color(
                            QPalette::Text
                            )
                        )
                    );
            }
        }
        else
        {
            if (control.details)
            {
                control.details->setText(
                    target > 0
                        ? tr("The imported row will be skipped and its unique existing match will keep its current schedule.")
                        : tr("The imported row will be skipped.")
                );
            }
        }

        if (control.colorButton)
        {
            control.colorButton->setEnabled(
                action != static_cast<int>(
                    ScheduleImportClassAction::Skip
                    )
                );
        }

        if (
            action != static_cast<int>(
                ScheduleImportClassAction::Skip
                )
            )
        {
            const ScheduleImportClassCandidate& candidate =
                m_preview.user.classes[
                    control.candidateIndex
                    ];
            if (!candidate.meetingPatternError.isEmpty())
            {
                valid = false;
                if (message.isEmpty())
                {
                    message =
                        tr("%1 %2 has an invalid meeting pattern. Update the spreadsheet or skip this class.")
                            .arg(
                                candidate.classGrade,
                                candidate.classLevel
                                );
                }
                if (control.details)
                {
                    control.details->setText(
                        control.details->text()
                        + QStringLiteral(
                            "<br><span style=\"color:#b91c1c\"><b>%1</b> %2</span>"
                            )
                            .arg(
                                tr("Meeting pattern:").toHtmlEscaped(),
                                candidate.meetingPatternError
                                    .toHtmlEscaped()
                                )
                        );
                }
            }
            if (
                candidate.importedColors.size() > 1
                && control.details
                )
            {
                control.details->setText(
                    control.details->text()
                    + QStringLiteral(
                        "<br><span style=\"color:%1\">%2</span>"
                        )
                        .arg(
                            control.details->palette()
                                .color(QPalette::Link)
                                .name(QColor::HexRgb),
                            tr("The spreadsheet uses multiple colors for this class; confirm the selected color.")
                                .toHtmlEscaped()
                            )
                    );
            }
        }

        if (target > 0)
        {
            targets.insert(target);
            candidateIndexesByTarget[target].append(
                control.candidateIndex
                );
        }
    }

    for (
        auto iterator = candidateIndexesByTarget.cbegin();
        iterator != candidateIndexesByTarget.cend();
        ++iterator
        )
    {
        if (iterator.value().size() < 2)
        {
            continue;
        }

        valid = false;
        if (message.isEmpty())
        {
            message =
                tr("Multiple imported classes cannot use the same existing class.");
        }

        QStringList importedClasses;
        for (int candidateIndex : iterator.value())
        {
            if (
                candidateIndex < 0
                || candidateIndex >= m_preview.user.classes.size()
                )
            {
                continue;
            }
            importedClasses.append(
                importedClassConflictLabel(
                    m_preview.user.classes[candidateIndex]
                    )
                );
        }
        const QString targetLabel =
            dataService
                ? classLabel(
                    dataService,
                    iterator.key(),
                    m_request.kind
                    )
                : tr("Class %1").arg(iterator.key());
        scheduleConflicts.append(
            tr("Multiple imported classes are assigned to %1: %2.")
                .arg(
                    targetLabel,
                    importedClasses.join(QStringLiteral(", "))
                    )
            );
    }

    ScheduleImportUserBlock projected;
    projected.name = m_preview.user.name;
    projected.intensiveSlotStates =
        m_preview.user.intensiveSlotStates;

    if (dataService)
    {
        for (const ClassControl& control : m_classControls)
        {
            const int action =
                classActions.value(
                    control.candidateIndex,
                    -1
                    );
            const int target =
                classTargets.value(
                    control.candidateIndex,
                    -1
                    );

            if (
                action == static_cast<int>(
                    ScheduleImportClassAction::Skip
                    )
                )
            {
                if (target <= 0)
                {
                    continue;
                }

                const ClassInfo info =
                    dataService->loadClassInfo(target);
                ScheduleImportClassCandidate preserved;
                preserved.teacherKr =
                    dataService->getTeacher(
                        info.teacherId
                        ).teacherKr;
                preserved.rooms = {
                    dataService->getTeacher(
                        info.teacherId
                        ).roomNumber
                };
                preserved.classGrade = info.classGrade;
                preserved.classLevel = info.classLevel;
                preserved.importedColors = {
                    info.classColor.isEmpty()
                        ? QStringLiteral("#FFFFFF")
                        : info.classColor
                };
                preserved.times =
                    m_request.kind
                            == ScheduleImportKind::Intensive
                        ? info.intensiveTimes
                        : info.classTimes;
                projected.classes.append(preserved);
                continue;
            }

            if (action < 0)
            {
                continue;
            }

            ScheduleImportClassCandidate candidate =
                m_preview.user.classes[
                    control.candidateIndex
                    ];
            candidate.importedColors = {
                control.color
            };
            const int teacherAction =
                teacherActions.value(
                    candidate.teacherKey,
                    -1
                    );
            QString room =
                teacherRooms.value(candidate.teacherKey);
            if (
                teacherAction == static_cast<int>(
                    ScheduleImportTeacherAction::Reuse
                    )
                )
            {
                room =
                    dataService->getTeacher(
                        teacherTargets.value(
                            candidate.teacherKey,
                            -1
                            )
                        ).roomNumber;
            }
            candidate.rooms =
                room.trimmed().isEmpty()
                    ? QStringList{}
                    : QStringList{room.trimmed()};
            projected.classes.append(candidate);
        }

        if (preservesAbsentIntensiveClasses)
        {
            for (const Classroom& classroom : dataService->getClasses())
            {
                if (targets.contains(classroom.id))
                {
                    continue;
                }

                const ClassInfo info =
                    dataService->loadClassInfo(classroom.id);
                if (info.intensiveTimes.isEmpty())
                {
                    continue;
                }

                ScheduleImportClassCandidate preserved;
                const Teacher teacher =
                    dataService->getTeacher(info.teacherId);
                preserved.teacherKr = teacher.teacherKr;
                preserved.rooms = {teacher.roomNumber};
                preserved.classGrade = info.classGrade;
                preserved.classLevel = info.classLevel;
                preserved.importedColors = {
                    info.classColor.isEmpty()
                        ? QStringLiteral("#FFFFFF")
                        : info.classColor
                };
                preserved.times = info.intensiveTimes;
                projected.classes.append(preserved);
            }
        }
    }
    else
    {
        projected = m_preview.user;
    }

    m_previewWidget->setPreviewModel(
        previewModel(
            projected,
            m_request.kind == ScheduleImportKind::Intensive,
            m_previewWidget->displayState()
            )
        );
    updatePreviewVisibleRows();
    const QStringList projectedConflicts =
        projectedScheduleConflicts(projected);
    if (!projectedConflicts.isEmpty())
    {
        valid = false;
        if (message.isEmpty())
        {
            message =
                tr("The proposed schedule has a conflict: %1")
                    .arg(projectedConflicts.first());
        }
        for (const QString& conflict : projectedConflicts)
        {
            scheduleConflicts.append(
                tr("The proposed schedule has a conflict: %1")
                    .arg(conflict)
                );
        }
    }

    updateScheduleConflictWarning(scheduleConflicts);

    if (
        m_warningAcknowledgement
        && !m_warningAcknowledgement->isChecked()
        )
    {
        valid = false;
        if (message.isEmpty())
        {
            message =
                tr("Acknowledge the unrecognized cells before importing.");
            }
    }

    int cleared = 0;
    if (dataService && !preservesAbsentIntensiveClasses)
    {
        for (const Classroom& classroom : dataService->getClasses())
        {
            const ClassInfo info =
                dataService->loadClassInfo(classroom.id);
            const bool hasSelectedTimes =
                m_request.kind == ScheduleImportKind::Intensive
                    ? !info.intensiveTimes.isEmpty()
                    : !info.classTimes.isEmpty();
            if (
                hasSelectedTimes
                && !targets.contains(classroom.id)
                )
            {
                ++cleared;
            }
        }
    }

    m_reviewStatus->setText(
        valid
            ? tr("All required resolutions are complete.")
            : message
        );
    QString profileNameChange;
    if (m_request.updateProfileName)
    {
        profileNameChange =
            tr(" My Information name will be updated to “%1”.")
                .arg(m_preview.user.name);
    }
    else if (m_request.profileName.trimmed().isEmpty())
    {
        profileNameChange =
            tr(" My Information name will be set to “%1”.")
                .arg(m_preview.user.name);
    }

    const ScheduleImportReviewSummary summary =
        ScheduleImportReviewSummaryBuilder::build(
            importPlan(),
            cleared
            );
    m_reviewSummary->setText(
        tr("Proposed import: %1 teacher(s) created, %2 room update(s), %3 teacher group(s) skipped; "
           "%4 class(es) created, %5 updated, %6 skipped; %7 existing schedule(s) cleared; "
           "%8 occupied cell(s) acknowledged and ignored.%9%10")
            .arg(summary.teachersCreated)
            .arg(summary.teacherRoomsUpdated)
            .arg(summary.teachersSkipped)
            .arg(summary.classesCreated)
            .arg(summary.classesUpdated)
            .arg(summary.classesSkipped)
            .arg(summary.schedulesCleared)
            .arg(summary.ignoredCells)
            .arg(profileNameChange)
            .arg(
                m_request.kind != ScheduleImportKind::Intensive
                    ? QString()
                    : preservesAbsentIntensiveClasses
                        ? tr(" Existing intensive hours for classes absent from the workbook will be retained.")
                        : tr(" A brand-new intensive schedule will replace existing intensive hours not explicitly preserved.")
                )
        );
    m_importButton->setEnabled(valid);
}

void ScheduleImportReviewDialog::resizeForReviewStage()
{
    if (
        !m_reviewSplitter
        || !m_resolutionTabs
        || !m_previewWidget
        || !m_buttons
        || !layout()
        )
    {
        return;
    }

    m_teacherLayout->activate();
    m_classLayout->activate();
    if (m_reviewPage)
    {
        if (m_reviewPage->layout())
        {
            m_reviewPage->layout()->activate();
        }
    }
    layout()->activate();

    const int resolutionWidth =
        PreferredResolutionPaneWidth;

    const int previewWidth =
        InitialPreviewWidth;

    int reviewContentWidth =
        previewWidth
        + resolutionWidth
        + m_reviewSplitter->handleWidth();
    if (m_reviewPage)
    {
        if (m_reviewPage->layout())
        {
            const QMargins pageMargins =
                m_reviewPage->layout()->contentsMargins();
            reviewContentWidth +=
                pageMargins.left()
                + pageMargins.right();
        }
    }

    const QMargins outerMargins =
        layout()->contentsMargins();
    int targetWidth =
        std::max(
            reviewContentWidth,
            m_buttons->sizeHint().width()
            )
        + outerMargins.left()
        + outerMargins.right();
    targetWidth =
        std::min(
            targetWidth,
            MaximumReviewDialogWidth
            );
    int targetHeight =
        ReviewDialogHeight;

    if (QScreen* targetScreen = screen())
    {
        const QSize available =
            targetScreen->availableGeometry().size();
        targetWidth =
            std::min(targetWidth, available.width());
        targetHeight =
            std::min(targetHeight, available.height());
    }
    resize(
        std::max(1, targetWidth),
        std::max(1, targetHeight)
        );
    m_reviewSplitter->setSizes(
        {previewWidth, resolutionWidth}
        );
    updatePreviewVisibleRows();
}

void ScheduleImportReviewDialog::resizeEvent(
    QResizeEvent* event
    )
{
    QDialog::resizeEvent(event);

    if (!m_prepared)
    {
        return;
    }

    if (m_previewPane && m_previewPane->layout())
    {
        m_previewPane->layout()->activate();
    }
    updatePreviewVisibleRows();
}

void ScheduleImportReviewDialog::updatePreviewVisibleRows()
{
    if (
        !m_previewPane
        || !m_previewHeading
        || !m_previewWidget
        )
    {
        return;
    }

    auto* table =
        m_previewWidget->findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    if (!table)
    {
        return;
    }

    int maximumVisibleRows =
        MaximumPreviewVisibleRows;
    if (height() > ReviewDialogHeight)
    {
        const int availableTableHeight =
            m_previewPane->contentsRect().height()
            - m_previewHeading->height()
            - PreviewHeadingSpacer;
        int usedHeight =
            table->horizontalHeader()->height()
            + (2 * table->frameWidth());
        int visibleRows = 0;

        for (int row = 0; row < table->rowCount(); ++row)
        {
            const int rowHeight = table->rowHeight(row);
            if (usedHeight + rowHeight > availableTableHeight)
            {
                break;
            }

            usedHeight += rowHeight;
            ++visibleRows;
        }

        maximumVisibleRows =
            std::max(
                MaximumPreviewVisibleRows,
                visibleRows
                );
    }

    m_previewWidget->setMaximumVisibleRows(
        maximumVisibleRows
        );
}

void ScheduleImportReviewDialog::applyImport()
{
    updateReviewState();
    if (!m_importButton->isEnabled())
    {
        return;
    }

    const ScheduleImportPlan plan =
        importPlan();
    const QString confirmation =
        m_reviewSummary->text()
        + QStringLiteral("\n\n")
        + tr("Is this schedule valid and ready to import?");

    if (
        DialogServices::confirm(
            this,
            tr("Confirm Schedule Import"),
            confirmation,
            tr("Import"),
            tr("Cancel")
            ) != PromptChoice::Accepted
        )
    {
        return;
    }

    DataService* dataService =
        openScheduleImportDataService(m_services);
    const auto summary =
        dataService
            ? dataService->importSchedule(plan)
            : Result<ScheduleImportSummary>(
                std::unexpected(
                    tr("No Teacher Profile is open.")
                    )
                );

    if (!summary)
    {
        DialogServices::showWarning(
            this,
            tr("Import Schedule"),
            summary.error()
            );
        return;
    }

    DialogServices::showInformation(
        this,
        tr("Import Schedule"),
        tr("Schedule imported successfully.\n"
           "Korean teachers created: %1\n"
           "Korean teacher rooms updated: %2\n"
           "Classes created: %3\n"
           "Classes updated: %4\n"
           "Classes skipped: %5\n"
           "Schedules cleared: %6\n"
           "Ignored occupied cells: %7%8")
            .arg(summary->teachersCreated)
            .arg(summary->teachersUpdated)
            .arg(summary->classesCreated)
            .arg(summary->classesUpdated)
            .arg(summary->classesSkipped)
            .arg(summary->schedulesCleared)
            .arg(summary->ignoredCells)
            .arg(
                summary->profileNameUpdated
                    ? tr("\nMy Information name was updated.")
                    : QString()
                )
        );
    accept();
}

ScheduleImportPlan ScheduleImportReviewDialog::importPlan() const
{
    ScheduleImportReviewContext context;
    context.kind = m_request.kind;
    context.intensiveMode =
        m_replaceIntensiveRadio
        && m_replaceIntensiveRadio->isChecked()
            ? ScheduleImportIntensiveMode::ReplaceWithNew
            : ScheduleImportIntensiveMode::UpdateExisting;
    context.selectedUserName =
        m_preview.user.name;
    context.saveProfileNameIfBlank =
        m_request.profileName.trimmed().isEmpty();
    context.updateProfileName =
        m_request.updateProfileName;
    context.unknownCellsAcknowledged =
        !m_warningAcknowledgement
        || m_warningAcknowledgement->isChecked();
    context.candidates =
        m_preview.user.classes;
    context.intensiveSlotStates =
        m_preview.user.intensiveSlotStates;
    context.diagnostics =
        m_preview.user.diagnostics;

    QList<ScheduleImportTeacherResolution> teachers;
    for (const TeacherControl& control : m_teacherControls)
    {
        ScheduleImportTeacherResolution resolution;
        resolution.teacherKey =
            control.teacherKey;
        resolution.action =
            static_cast<ScheduleImportTeacherAction>(
                control.action->currentData(
                    ActionRole
                    ).toInt()
                );
        resolution.targetTeacherId =
            control.action->currentData(
                TargetRole
                ).toInt();
        resolution.selectedRoom =
            control.room->currentData().toString();
        teachers.append(resolution);
    }

    QList<ScheduleImportClassResolution> classes;
    for (const ClassControl& control : m_classControls)
    {
        ScheduleImportClassResolution resolution;
        resolution.candidateIndex =
            control.candidateIndex;
        resolution.action =
            static_cast<ScheduleImportClassAction>(
                control.action->currentData(
                    ActionRole
                    ).toInt()
                );
        resolution.targetClassId =
            control.action->currentData(
                TargetRole
                ).toInt();
        resolution.classColor =
            control.color;
        resolution.fontColor =
            ColorUtils::getContrastingFontColor(
                QColor(control.color)
                );
        classes.append(resolution);
    }

    return ScheduleImportReviewModel::buildPlan(
        context,
        teachers,
        classes
        );
}
