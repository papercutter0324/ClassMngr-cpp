#include "schedule_builder.h"

#include "app/services/feature_services.h"
#include "classmngr/engine/schedule_builder.h"

#include <utility>
#include <vector>

namespace
{
using EngineClassInfo = classmngr::engine::ClassInfo;
using EngineClassTime = classmngr::engine::ClassTime;
using classmngr::engine::ScheduleBuilderService;
using classmngr::engine::ScheduleReportBuildResult;
using classmngr::engine::ScheduleReportEntry;

EngineClassTime toPortable(const ClassTime& time)
{
    return {
        time.day.toStdString(),
        time.startTime.toStdString(),
        time.endTime.toStdString()
    };
}

EngineClassInfo toPortable(const ClassInfo& info)
{
    EngineClassInfo result;
    result.classId = info.classId;
    result.teacherId = info.teacherId;
    result.teacherKr = info.teacherKr.toStdString();
    result.teacherEn = info.teacherEn.toStdString();
    result.teacherPreferredName = info.teacherPreferredName.toStdString();
    result.roomNumber = info.roomNumber.toStdString();
    result.wifiName = info.wifiName.toStdString();
    result.wifiPassword = info.wifiPassword.toStdString();
    result.internetType = info.internetType.toStdString();
    result.zoomId = info.zoomId.toStdString();
    result.zoomPassword = info.zoomPassword.toStdString();
    result.projectionType = info.projectionType.toStdString();
    result.classGrade = info.classGrade.toStdString();
    result.classLevel = info.classLevel.toStdString();
    result.readingBook = info.readingBook.toStdString();
    result.essayBook = info.essayBook.toStdString();
    result.classColor = info.classColor.toStdString();
    result.fontColor = info.fontColor.toStdString();
    result.notes = info.notes.toStdString();
    result.timeFillerActivities = info.timeFillerActivities.toStdString();

    result.classTimes.reserve(info.classTimes.size());
    for (const ClassTime& time : info.classTimes)
    {
        result.classTimes.push_back(toPortable(time));
    }
    result.intensiveTimes.reserve(info.intensiveTimes.size());
    for (const ClassTime& time : info.intensiveTimes)
    {
        result.intensiveTimes.push_back(toPortable(time));
    }

    return result;
}

ScheduleEntry fromPortable(const ScheduleReportEntry& entry)
{
    ScheduleEntry result;
    result.classId = entry.classId;
    result.kind = entry.kind
        == classmngr::engine::ScheduleReportEntryKind::TestingClass
        ? ScheduleEntryKind::TestingClass
        : ScheduleEntryKind::RegularClass;
    result.className = QString::fromStdString(entry.className);
    result.teacherKr = QString::fromStdString(entry.teacherKr);
    result.teacherEn = QString::fromStdString(entry.teacherEn);
    result.teacherPreferredName =
        QString::fromStdString(entry.teacherPreferredName);
    result.roomNumber = QString::fromStdString(entry.roomNumber);
    result.classGrade = QString::fromStdString(entry.classGrade);
    result.classLevel = QString::fromStdString(entry.classLevel);
    result.classColor = QString::fromStdString(entry.classColor);
    result.fontColor = QString::fromStdString(entry.fontColor);
    return result;
}

ScheduleBuildResult fromPortable(const ScheduleReportBuildResult& source)
{
    ScheduleBuildResult result;
    result.scheduleOffset = source.scheduleOffset;
    result.uses55Endings = source.uses55Endings;

    result.days.reserve(static_cast<qsizetype>(source.days.size()));
    for (const std::string& day : source.days)
    {
        result.days.append(QString::fromStdString(day));
    }

    result.rows.reserve(static_cast<qsizetype>(source.rows.size()));
    for (const auto& row : source.rows)
    {
        result.rows.append(
            ScheduleRow{QString::fromStdString(row.label)}
            );
    }

    for (const auto& day : source.schedule)
    {
        auto& scheduleSlots =
            result.schedule[QString::fromStdString(day.first)];
        for (const auto& slot : day.second)
        {
            auto& entries =
                scheduleSlots[QString::fromStdString(slot.first)];
            entries.reserve(static_cast<qsizetype>(slot.second.size()));
            for (const ScheduleReportEntry& entry : slot.second)
            {
                entries.append(fromPortable(entry));
            }
        }
    }

    return result;
}
} // namespace

ScheduleBuilder::ScheduleBuilder(
    ClassService* classService
    )
    : m_classService(classService)
{
}

Result<ScheduleBuildResult> ScheduleBuilder::build(
    bool useIntensive,
    const QStringList& visibleDays
    ) const
{
    ScheduleBuildResult empty;
    if (
        !m_classService
        || !m_classService->isAvailable()
        )
    {
        return empty;
    }

    const Result<QList<ClassInfo>> classInfos =
        m_classService->scheduleClassInfos();
    if (!classInfos)
    {
        return std::unexpected(classInfos.error());
    }

    std::vector<EngineClassInfo> portableInfos;
    portableInfos.reserve(classInfos->size());
    for (const ClassInfo& info : *classInfos)
    {
        portableInfos.push_back(toPortable(info));
    }

    std::vector<std::string> portableDays;
    portableDays.reserve(visibleDays.size());
    for (const QString& day : visibleDays)
    {
        portableDays.push_back(day.toStdString());
    }

    return fromPortable(
        ScheduleBuilderService::build(
            portableInfos,
            useIntensive,
            portableDays
            )
        );
}
