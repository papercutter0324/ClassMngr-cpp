#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/rules/schedule_value_parser.h"
#include "next/application/sub_prep_meeting_time_grouping_policy.h"
#include "next/application/sub_prep_navigation_metadata_policy.h"
#include "next/application/sub_prep_navigation_schedule_order_policy.h"
#include "next/application/sub_prep_schedule_summary_query.h"
#include "next/domain/teacher_display_name.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QStringList>
#include <QTime>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Reads the requested Sub Prep class/day/mode scope through the active
// repository session and returns only copied Application summary values.
class ApplicationServicesSubPrepScheduleSummaryPort final
    : public Application::SubPrepScheduleSummaryReadPort
{
public:
    explicit ApplicationServicesSubPrepScheduleSummaryPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesSubPrepScheduleSummaryPort(
        const ApplicationServicesSubPrepScheduleSummaryPort&
        ) = delete;
    ApplicationServicesSubPrepScheduleSummaryPort& operator=(
        const ApplicationServicesSubPrepScheduleSummaryPort&
        ) = delete;
    ApplicationServicesSubPrepScheduleSummaryPort(
        ApplicationServicesSubPrepScheduleSummaryPort&&
        ) = delete;
    ApplicationServicesSubPrepScheduleSummaryPort& operator=(
        ApplicationServicesSubPrepScheduleSummaryPort&&
        ) = delete;

    [[nodiscard]] Application::SubPrepScheduleSummaryReadResult loadSummaries(
        const Application::SubPrepScheduleScopeRequest& request
        ) override
    {
        if (request.visibleClassIds.size()
            > Application::kSubPrepScheduleScopeMaxVisibleClasses)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Visible Sub Prep class scope exceeds its bounded limit."
                );
        }

        if (request.selectedDays.size() > 7)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Selected Sub Prep days exceed the weekly limit."
                );
        }

        if (request.mode != Application::ScheduleViewMode::Regular
            && request.mode != Application::ScheduleViewMode::Intensive)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "The Sub Prep schedule mode is invalid."
                );
        }

        try
        {
            QList<int> legacyClassIds;
            legacyClassIds.reserve(
                static_cast<qsizetype>(request.visibleClassIds.size())
                );
            std::vector<int> parsedClassIds;
            parsedClassIds.reserve(request.visibleClassIds.size());
            std::set<Domain::ClassId> seenClassIds;
            for (const Domain::ClassId& id : request.visibleClassIds)
            {
                if (!Application::ClassSummaryProjectionDetail::isValidId(id)
                    || !seenClassIds.insert(id).second)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Visible class identifiers must be non-blank, bounded, and unique."
                        );
                }

                const auto parsed = legacyId(id.value());
                if (!parsed)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Visible class identifiers must be canonical positive integers."
                        );
                }
                parsedClassIds.push_back(*parsed);
                legacyClassIds.append(*parsed);
            }

            QStringList legacyDays;
            legacyDays.reserve(
                static_cast<qsizetype>(request.selectedDays.size())
                );
            std::vector<Application::SubPrepWeekday> typedSelectedDays;
            typedSelectedDays.reserve(request.selectedDays.size());
            std::unordered_set<Application::SubPrepWeekday> seenDays;
            for (const Application::SubPrepWeekday day : request.selectedDays)
            {
                if (!seenDays.insert(day).second)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Selected Sub Prep weekdays must be unique."
                        );
                }
                const auto label = weekdayLabel(day);
                if (!label)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A selected Sub Prep weekday is invalid."
                        );
                }
                legacyDays.append(*label);
                typedSelectedDays.push_back(day);
            }
            const QStringList allLegacyDays{
                QStringLiteral("Monday"),
                QStringLiteral("Tuesday"),
                QStringLiteral("Wednesday"),
                QStringLiteral("Thursday"),
                QStringLiteral("Friday"),
                QStringLiteral("Saturday"),
                QStringLiteral("Sunday")
            };

            if (legacyClassIds.isEmpty() || legacyDays.isEmpty())
            {
                return Application::SubPrepScheduleSummaryReadResult::success({});
            }

            DatabaseSession* const session = m_services.databaseSession();
            if (!session || !session->isOpen())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The Sub Prep schedule summary service is unavailable."
                    );
            }

            ClassInfoRepository* const repository =
                session->classInfoRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The Sub Prep schedule summary service is unavailable."
                    );
            }

            const ScheduleType legacyMode =
                request.mode == Application::ScheduleViewMode::Regular
                ? ScheduleType::Regular
                : ScheduleType::Intensive;
            const ::Result<QList<SubPrepClassSummaryRecord>> records =
                repository->loadSubPrepClassSummaries(
                    legacyClassIds,
                    legacyDays,
                    legacyMode,
                    static_cast<int>(
                        Application::kSubPrepScheduleSummaryMaxMeetingsPerClass
                        ),
                    static_cast<int>(
                        Application::kSubPrepScheduleSummaryMaxMeetings
                        )
                    );
            if (!records)
            {
                if (!session->isOpen())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The Sub Prep schedule summary service became unavailable while loading summaries."
                        );
                }
                return repositoryFailure(records.error());
            }

            std::unordered_map<int, const SubPrepClassSummaryRecord*>
                recordsByClassId;
            recordsByClassId.reserve(
                static_cast<std::size_t>(records->size())
                );
            std::unordered_set<int> requestedLegacyIds(
                parsedClassIds.cbegin(),
                parsedClassIds.cend()
                );
            std::size_t totalMeetingCount = 0;
            for (const SubPrepClassSummaryRecord& record : records.value())
            {
                if (record.classId <= 0 || record.teacherId <= 0
                    || !requestedLegacyIds.contains(record.classId)
                    || !recordsByClassId.emplace(record.classId, &record).second)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "The Sub Prep schedule summary read returned an invalid or duplicate class."
                        );
                }

                if (record.meetings.size()
                    > Application::kSubPrepScheduleSummaryMaxMeetingsPerClass)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A visible Sub Prep class exceeds the bounded meeting limit."
                        );
                }
                if (record.meetings.size()
                    > Application::kSubPrepScheduleSummaryMaxMeetings
                        - totalMeetingCount)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Visible Sub Prep meetings exceed the bounded total limit."
                        );
                }
                totalMeetingCount += record.meetings.size();

                if (record.studentCount < 0
                    || static_cast<std::uint64_t>(record.studentCount)
                        > Application::kClassSummaryMaxStudentCount)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A visible Sub Prep class exceeds the bounded student count."
                        );
                }
            }

            std::unordered_map<int, NavigationMetadata>
                regularNavigationByClassId;
            const ::Result<QList<SubPrepClassSummaryRecord>> regularRecords =
                repository->loadSubPrepClassSummaries(
                    legacyClassIds,
                    allLegacyDays,
                    ScheduleType::Regular,
                    static_cast<int>(
                        Application::kSubPrepScheduleSummaryMaxMeetingsPerClass
                        ),
                    static_cast<int>(
                        Application::kSubPrepScheduleSummaryMaxMeetings
                        )
                    );
            if (regularRecords)
            {
                regularNavigationByClassId.reserve(
                    static_cast<std::size_t>(regularRecords->size())
                    );
                for (const SubPrepClassSummaryRecord& record
                     : regularRecords.value())
                {
                    if (record.classId <= 0 || record.teacherId <= 0
                        || !requestedLegacyIds.contains(record.classId)
                        || !regularNavigationByClassId.emplace(
                               record.classId,
                               navigationMetadataFor(
                                   record,
                                   Application::ScheduleViewMode::Regular
                                   )
                               ).second)
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "The Sub Prep preferred schedule read returned an invalid or duplicate class."
                            );
                    }
                }
            }

            const bool needsIntensiveNavigation =
                request.mode == Application::ScheduleViewMode::Intensive
                && std::any_of(
                    recordsByClassId.cbegin(),
                    recordsByClassId.cend(),
                    [&regularNavigationByClassId](const auto& entry)
                    {
                        return !regularNavigationByClassId.contains(
                            entry.first
                            );
                    }
                    );
            std::unordered_map<int, NavigationMetadata>
                intensiveNavigationByClassId;
            if (needsIntensiveNavigation)
            {
                const ::Result<QList<SubPrepClassSummaryRecord>> intensiveRecords =
                    repository->loadSubPrepClassSummaries(
                        legacyClassIds,
                        allLegacyDays,
                        ScheduleType::Intensive,
                        static_cast<int>(
                            Application::kSubPrepScheduleSummaryMaxMeetingsPerClass
                            ),
                        static_cast<int>(
                            Application::kSubPrepScheduleSummaryMaxMeetings
                            )
                        );
                if (intensiveRecords)
                {
                    intensiveNavigationByClassId.reserve(
                        static_cast<std::size_t>(intensiveRecords->size())
                        );
                    for (const SubPrepClassSummaryRecord& record
                         : intensiveRecords.value())
                    {
                        if (record.classId <= 0 || record.teacherId <= 0
                            || !requestedLegacyIds.contains(record.classId))
                        {
                            return failure(
                                Domain::ErrorCode::Validation,
                                "The Sub Prep intensive preferred schedule read returned an invalid class."
                                );
                        }

                        intensiveNavigationByClassId.try_emplace(
                            record.classId,
                            navigationMetadataFor(
                                record,
                                Application::ScheduleViewMode::Intensive
                                )
                            );
                    }
                }
            }

            Application::ClassSummaryProjectionInput input;
            input.classes.reserve(recordsByClassId.size());
            input.teachers.reserve(recordsByClassId.size());
            std::unordered_map<qint64, std::size_t> teacherIndexes;
            teacherIndexes.reserve(recordsByClassId.size());

            for (std::size_t requestIndex = 0;
                 requestIndex < parsedClassIds.size();
                 ++requestIndex)
            {
                const int legacyClassId = parsedClassIds[requestIndex];
                const auto recordEntry = recordsByClassId.find(legacyClassId);
                if (recordEntry == recordsByClassId.end())
                {
                    // Classes without a class-info row, usable teacher, or a
                    // matching selected schedule retain the legacy omission.
                    continue;
                }

                const SubPrepClassSummaryRecord& record =
                    *recordEntry->second;
                const NavigationMetadata selectedModeNavigation =
                    navigationMetadataFor(record, request.mode);
                const auto regularNavigationEntry =
                    regularNavigationByClassId.find(legacyClassId);
                const auto intensiveNavigationEntry =
                    intensiveNavigationByClassId.find(legacyClassId);
                const NavigationMetadata& navigation =
                    Application::selectSubPrepNavigationMetadata(
                        request.mode,
                        regularNavigationEntry
                                != regularNavigationByClassId.end()
                            ? &regularNavigationEntry->second
                            : nullptr,
                        intensiveNavigationEntry
                                != intensiveNavigationByClassId.end()
                            ? &intensiveNavigationEntry->second
                            : nullptr,
                        selectedModeNavigation
                        );
                const Domain::ClassId& requestedClassId =
                    request.visibleClassIds[requestIndex];
                const auto gradeText = requiredUtf8(
                    normalizedOrNa(record.classGrade),
                    Application::kClassSummaryMaxGradeLength
                    );
                const auto levelText = requiredUtf8(
                    normalizedOrNa(record.classLevel),
                    Application::kClassSummaryMaxLevelLength
                    );
                const auto displayLabel = requiredUtf8(
                    classLabel(record.classGrade, record.classLevel),
                    Application::kClassSummaryMaxDisplayLabelLength
                    );
                const auto meetingText = requiredUtf8(
                    formatMeetingTimes(record.meetings, typedSelectedDays),
                    Application::kClassSummaryMaxMeetingTextLength
                    );
                const auto navigationLabel = requiredUtf8(
                    navigation.label,
                    Application::kClassSummaryMaxNavigationLabelLength
                    );
                const auto navigationTeacherLabel = optionalUtf8(
                    navigationTeacherLabelFor(record),
                    Application::kClassSummaryMaxNavigationTeacherLabelLength
                    );
                if (!gradeText || !levelText || !displayLabel || !meetingText
                    || !navigationLabel || !navigationTeacherLabel)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A visible Sub Prep class exceeds a bounded summary text limit."
                        );
                }

                const Domain::TeacherId typedTeacherId =
                    teacherId(record.teacherId);
                const auto teacherEntry = teacherIndexes.find(record.teacherId);
                if (teacherEntry == teacherIndexes.end())
                {
                    const auto displayName = requiredUtf8(
                        preferredDisplayName(record),
                        Application::kTeacherSummaryMaxDisplayNameLength
                        );
                    const auto facilities = optionalUtf8(
                        teacherFacilities(record),
                        Application::kTeacherSummaryMaxFacilitiesLength
                        );
                    const auto notes = optionalUtf8(
                        record.teacherNotes,
                        Application::kTeacherSummaryMaxNotesLength
                        );
                    if (!displayName || !facilities || !notes)
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "A Sub Prep teacher summary exceeds a bounded text limit."
                            );
                    }
                    teacherIndexes.emplace(
                        record.teacherId,
                        input.teachers.size()
                        );
                    input.teachers.push_back({
                        .id = typedTeacherId,
                        .displayName = *displayName,
                        .facilities = *facilities,
                        .notes = *notes
                    });
                }

                input.classes.push_back({
                    .id = requestedClassId,
                    .teacherId = typedTeacherId,
                    .grade = *gradeText,
                    .level = *levelText,
                    .displayLabel = *displayLabel,
                    .meetingText = *meetingText,
                    .studentCount = static_cast<std::size_t>(record.studentCount),
                    .order = static_cast<std::int32_t>(requestIndex),
                    .navigationLabel = *navigationLabel,
                    .navigationTeacherLabel = *navigationTeacherLabel,
                    .navigationFirstDayOrder = navigation.firstDayOrder,
                    .navigationFirstTimeOrder = navigation.firstTimeOrder
                });
            }

            return Application::SubPrepScheduleSummaryReadResult::success(
                std::move(input)
                );

        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Sub Prep schedule summaries could not be loaded."
                );
        }
    }

private:
    struct NavigationMetadata final
    {
        QString label;
        std::int32_t firstDayOrder =
            Application::kClassSummaryUnknownNavigationOrder;
        std::int32_t firstTimeOrder =
            Application::kClassSummaryUnknownNavigationOrder;
    };

    [[nodiscard]] static NavigationMetadata navigationMetadataFor(
        const SubPrepClassSummaryRecord& record,
        const Application::ScheduleViewMode mode
        )
    {
        const auto [firstDayOrder, firstTimeOrder] =
            navigationScheduleOrder(record.meetings);
        return {
            .label = navigationLabelFor(record, mode),
            .firstDayOrder = firstDayOrder,
            .firstTimeOrder = firstTimeOrder
        };
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(
        const std::string& value
        )
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{} || end != value.data() + value.size()
            || parsed <= 0 || std::to_string(parsed) != value)
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static Domain::TeacherId teacherId(
        const qint64 legacyTeacherId
        )
    {
        return *Domain::TeacherId::fromString(
            std::to_string(legacyTeacherId)
            );
    }

    [[nodiscard]] static std::optional<QString> weekdayLabel(
        const Application::SubPrepWeekday day
        )
    {
        using Application::SubPrepWeekday;
        switch (day)
        {
        case SubPrepWeekday::Monday:
            return QStringLiteral("Monday");
        case SubPrepWeekday::Tuesday:
            return QStringLiteral("Tuesday");
        case SubPrepWeekday::Wednesday:
            return QStringLiteral("Wednesday");
        case SubPrepWeekday::Thursday:
            return QStringLiteral("Thursday");
        case SubPrepWeekday::Friday:
            return QStringLiteral("Friday");
        case SubPrepWeekday::Saturday:
            return QStringLiteral("Saturday");
        case SubPrepWeekday::Sunday:
            return QStringLiteral("Sunday");
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<int> weekdayOrder(
        const QString& day
        )
    {
        const auto parsed = ScheduleValueParser::parseWeekday(day);
        if (!parsed || parsed->text != day)
        {
            return std::nullopt;
        }
        return static_cast<int>(parsed->value);
    }

    [[nodiscard]] static std::optional<Application::SubPrepWeekday>
    typedWeekday(const QString& day)
    {
        const auto order = weekdayOrder(day);
        if (!order)
        {
            return std::nullopt;
        }
        // ScheduleValueParser numbers Monday as zero; the typed Application
        // weekday values intentionally start at one.
        return static_cast<Application::SubPrepWeekday>(*order + 1);
    }

    [[nodiscard]] static QTime parseTime(const QString& value)
    {
        const auto parsed = ScheduleValueParser::parseDisplayTime(value);
        return parsed ? *parsed : QTime{};
    }

    [[nodiscard]] static QString compactTime(const QTime& time)
    {
        if (!time.isValid())
        {
            return {};
        }
        const QString format = time.minute() == 0
            ? QStringLiteral("hap")
            : QStringLiteral("h:mmap");
        return time.toString(format).toLower();
    }

    [[nodiscard]] static QString compactNavigationStartTime(
        const QString& value
        )
    {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty())
        {
            return {};
        }

        const bool usesMeridiem =
            trimmed.contains(QStringLiteral("AM"), Qt::CaseInsensitive)
            || trimmed.contains(QStringLiteral("PM"), Qt::CaseInsensitive);
        const auto parsed =
            ScheduleValueParser::parseDisplayTime(trimmed);
        if (parsed)
        {
            if (!usesMeridiem)
            {
                return parsed->toString(QStringLiteral("H:mm"));
            }

            QString formatted = parsed->toString(QStringLiteral("h:mm AP"));
            formatted.remove(QStringLiteral(" AM"));
            formatted.remove(QStringLiteral(" PM"));
            return formatted;
        }

        QString fallback = trimmed;
        fallback.remove(QStringLiteral(" AM"), Qt::CaseInsensitive);
        fallback.remove(QStringLiteral(" PM"), Qt::CaseInsensitive);
        return fallback;
    }

    [[nodiscard]] static QString navigationDayCode(const QString& day)
    {
        if (day == QStringLiteral("Monday"))
        {
            return QStringLiteral("M");
        }
        if (day == QStringLiteral("Tuesday"))
        {
            return QStringLiteral("T");
        }
        if (day == QStringLiteral("Wednesday"))
        {
            return QStringLiteral("W");
        }
        if (day == QStringLiteral("Thursday"))
        {
            return QStringLiteral("Th");
        }
        if (day == QStringLiteral("Friday"))
        {
            return QStringLiteral("F");
        }
        if (day == QStringLiteral("Saturday"))
        {
            return QStringLiteral("Sat");
        }
        if (day == QStringLiteral("Sunday"))
        {
            return QStringLiteral("Sun");
        }
        return day.trimmed();
    }

    [[nodiscard]] static QString compressedNavigationDays(
        QStringList days
        )
    {
        days.removeDuplicates();
        std::sort(
            days.begin(),
            days.end(),
            [](const QString& left, const QString& right)
            {
                return weekdayOrder(left).value_or(1'000)
                    < weekdayOrder(right).value_or(1'000);
            }
            );

        QStringList codes;
        codes.reserve(days.size());
        for (const QString& day : days)
        {
            const QString code = navigationDayCode(day);
            if (!code.isEmpty())
            {
                codes.append(code);
            }
        }
        if (codes == QStringList{QStringLiteral("M"), QStringLiteral("W")})
        {
            return QStringLiteral("M/W");
        }
        if (codes == QStringList{QStringLiteral("M"), QStringLiteral("F")})
        {
            return QStringLiteral("M/F");
        }
        if (codes == QStringList{QStringLiteral("W"), QStringLiteral("F")})
        {
            return QStringLiteral("W/F");
        }
        if (codes == QStringList{
                QStringLiteral("M"),
                QStringLiteral("W"),
                QStringLiteral("F")
            })
        {
            return QStringLiteral("M/W/F");
        }
        if (codes == QStringList{
                QStringLiteral("T"),
                QStringLiteral("Th")
            })
        {
            return QStringLiteral("T/Th");
        }
        return codes.join(QStringLiteral("/"));
    }

    [[nodiscard]] static QString navigationScheduleText(
        const QList<SubPrepScheduleMeetingRecord>& meetings
        )
    {
        struct TimeGroup final
        {
            QString startTime;
            QStringList days;
        };

        QList<TimeGroup> groups;
        for (const SubPrepScheduleMeetingRecord& meeting : meetings)
        {
            const QString startTime =
                compactNavigationStartTime(meeting.startTime);
            if (startTime.isEmpty())
            {
                continue;
            }

            auto group = std::find_if(
                groups.begin(),
                groups.end(),
                [&startTime](const TimeGroup& candidate)
                {
                    return candidate.startTime == startTime;
                }
                );
            if (group == groups.end())
            {
                groups.append({startTime, {meeting.day.trimmed()}});
            }
            else
            {
                group->days.append(meeting.day.trimmed());
            }
        }

        QStringList labels;
        labels.reserve(groups.size());
        for (const TimeGroup& group : groups)
        {
            labels.append(QStringLiteral("%1 %2")
                .arg(
                    compressedNavigationDays(group.days),
                    group.startTime
                    ));
        }
        return labels.join(QStringLiteral("; "));
    }

    [[nodiscard]] static QString navigationLabelFor(
        const SubPrepClassSummaryRecord& record,
        const Application::ScheduleViewMode mode
        )
    {
        const QString level = record.classLevel.trimmed();
        const QString grade = record.classGrade.trimmed();
        const QString name = !level.isEmpty()
            ? level
            : !grade.isEmpty()
                ? grade
                : classLabel(record.classGrade, record.classLevel);
        const QString schedule = navigationScheduleText(record.meetings);
        const QString preferredSchedule = schedule.isEmpty()
            ? QObject::tr("No time")
            : mode == Application::ScheduleViewMode::Intensive
                ? QStringLiteral("%1 %2").arg(QObject::tr("Int"), schedule)
                : schedule;
        return QStringLiteral("%1 • %2").arg(name, preferredSchedule);
    }

    [[nodiscard]] static QString navigationTeacherLabelFor(
        const SubPrepClassSummaryRecord& record
        )
    {
        const QString english = record.teacherEn.trimmed();
        return english.isEmpty() ? record.teacherKr.trimmed() : english;
    }

    [[nodiscard]] static std::pair<std::int32_t, std::int32_t>
    navigationScheduleOrder(
        const QList<SubPrepScheduleMeetingRecord>& meetings
        )
    {
        std::vector<Application::SubPrepNavigationScheduleOrderFact> facts;
        facts.reserve(static_cast<std::size_t>(meetings.size()));
        for (const SubPrepScheduleMeetingRecord& meeting : meetings)
        {
            const std::optional<int> dayOrder = weekdayOrder(meeting.day);
            if (!dayOrder)
            {
                continue;
            }

            std::optional<Application::SubPrepNavigationStartMinute>
                startMinute;
            const QTime time = parseTime(meeting.startTime);
            if (time.isValid())
            {
                startMinute = Application::SubPrepNavigationStartMinute{
                    static_cast<std::int32_t>(
                        (time.hour() * 60) + time.minute()
                        )
                };
            }

            facts.push_back({
                static_cast<Domain::Weekday>(*dayOrder),
                startMinute
            });
        }

        return Application::calculateSubPrepNavigationScheduleOrder(facts);
    }

    [[nodiscard]] static QString dayAbbreviation(const QString& day)
    {
        if (day == QStringLiteral("Monday"))
        {
            return QCoreApplication::translate(
                "SubPrepClassInformation",
                "Mon"
                );
        }
        if (day == QStringLiteral("Tuesday"))
        {
            return QCoreApplication::translate(
                "SubPrepClassInformation",
                "Tues"
                );
        }
        if (day == QStringLiteral("Wednesday"))
        {
            return QCoreApplication::translate(
                "SubPrepClassInformation",
                "Wed"
                );
        }
        if (day == QStringLiteral("Thursday"))
        {
            return QCoreApplication::translate(
                "SubPrepClassInformation",
                "Thurs"
                );
        }
        if (day == QStringLiteral("Friday"))
        {
            return QCoreApplication::translate(
                "SubPrepClassInformation",
                "Fri"
                );
        }
        if (day == QStringLiteral("Saturday"))
        {
            return QCoreApplication::translate(
                "SubPrepClassInformation",
                "Sat"
                );
        }
        if (day == QStringLiteral("Sunday"))
        {
            return QCoreApplication::translate(
                "SubPrepClassInformation",
                "Sun"
                );
        }
        return day.trimmed();
    }

    [[nodiscard]] static QString formatMeetingTimes(
        const QList<SubPrepScheduleMeetingRecord>& source,
        const std::vector<Application::SubPrepWeekday>& selectedDays
        )
    {
        std::vector<Application::SubPrepMeetingTimeFact> meetings;
        meetings.reserve(static_cast<std::size_t>(source.size()));
        for (const SubPrepScheduleMeetingRecord& record : source)
        {
            const auto weekday = typedWeekday(record.day);
            if (!weekday)
            {
                continue;
            }

            const QTime time = parseTime(record.startTime);
            if (!time.isValid())
            {
                continue;
            }
            meetings.push_back({
                *weekday,
                Application::SubPrepMeetingSecondsOfDay(
                    static_cast<std::int32_t>(
                        ((time.hour() * 60) + time.minute()) * 60
                        + time.second()
                        )
                    )
            });
        }

        const std::vector<Application::SubPrepMeetingTimeGroup> groups =
            Application::groupSelectedSubPrepMeetingTimes(
                meetings,
                selectedDays
                );

        QStringList labels;
        labels.reserve(static_cast<qsizetype>(groups.size()));
        for (const Application::SubPrepMeetingTimeGroup& group : groups)
        {
            QStringList days;
            days.reserve(static_cast<qsizetype>(group.weekdays.size()));
            for (const Application::SubPrepWeekday day : group.weekdays)
            {
                const auto label = weekdayLabel(day);
                if (label)
                {
                    days.append(dayAbbreviation(*label));
                }
            }
            const QTime time = QTime::fromMSecsSinceStartOfDay(
                group.secondsOfDay.value() * 1000
                );
            labels.append(QStringLiteral("%1 %2")
                .arg(days.join(QString()), compactTime(time)));
        }
        return labels.isEmpty()
            ? QStringLiteral("N/A")
            : labels.join(QStringLiteral(" & "));
    }

    [[nodiscard]] static QString normalizedOrNa(const QString& value)
    {
        const QString normalized = value.trimmed();
        return normalized.isEmpty() ? QStringLiteral("N/A") : normalized;
    }

    [[nodiscard]] static QString classLabel(
        const QString& grade,
        const QString& level
        )
    {
        const QString normalizedGrade = grade.trimmed();
        const QString normalizedLevel = level.trimmed();
        if (!normalizedGrade.isEmpty() && !normalizedLevel.isEmpty())
        {
            return QStringLiteral("%1 %2").arg(
                normalizedGrade,
                normalizedLevel
                );
        }
        if (!normalizedGrade.isEmpty())
        {
            return normalizedGrade;
        }
        if (!normalizedLevel.isEmpty())
        {
            return normalizedLevel;
        }
        return QStringLiteral("N/A");
    }

    [[nodiscard]] static QString preferredDisplayName(
        const SubPrepClassSummaryRecord& record
        )
    {
        const std::u16string preferredName =
            record.teacherPreferredName.trimmed().toStdU16String();
        const std::u16string englishName =
            record.teacherEn.trimmed().toStdU16String();
        const std::u16string preferredRomanization =
            record.teacherPreferredRomanization.trimmed().toStdU16String();
        const std::u16string koreanName =
            record.teacherKr.trimmed().toStdU16String();
        const Domain::TeacherDisplayName selected =
            Domain::TeacherDisplayName::select(
                preferredName,
                englishName,
                preferredRomanization,
                koreanName
                );
        return selected.empty()
            ? QStringLiteral("N/A")
            : QString::fromStdU16String(selected.value());
    }

    [[nodiscard]] static QString teacherFacilities(
        const SubPrepClassSummaryRecord& record
        )
    {
        QStringList fields;
        const auto append = [&fields](const QString& label, const QString& value)
        {
            if (!value.trimmed().isEmpty())
            {
                fields.append(QStringLiteral("%1: %2").arg(label, value));
            }
        };
        append(QStringLiteral("Room"), record.teacherRoomNumber);
        append(QStringLiteral("WiFi"), record.teacherWifiName);
        append(QStringLiteral("WiFi Password"), record.teacherWifiPassword);
        append(QStringLiteral("Internet"), record.teacherInternetType);
        append(QStringLiteral("Zoom ID"), record.teacherZoomId);
        append(QStringLiteral("Zoom Password"), record.teacherZoomPassword);
        append(QStringLiteral("Projection"), record.teacherProjectionType);
        return fields.join(QStringLiteral("; "));
    }

    [[nodiscard]] static std::optional<std::string> requiredUtf8(
        const QString& value,
        const std::size_t maxLength
        )
    {
        if (value.trimmed().isEmpty()
            || static_cast<std::size_t>(value.size()) > maxLength)
        {
            return std::nullopt;
        }
        return optionalUtf8(value, maxLength);
    }

    [[nodiscard]] static std::optional<std::string> optionalUtf8(
        const QString& value,
        const std::size_t maxLength
        )
    {
        if (static_cast<std::size_t>(value.size()) > maxLength)
        {
            return std::nullopt;
        }
        const QByteArray bytes = value.toUtf8();
        if (static_cast<std::size_t>(bytes.size()) > maxLength)
        {
            return std::nullopt;
        }
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::SubPrepScheduleSummaryReadResult
    repositoryFailure(const QString& repositoryError)
    {
        const QString normalized = repositoryError.toLower();
        if (normalized.contains(QStringLiteral("invalid"))
            || normalized.contains(QStringLiteral("unique")))
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                errorText(repositoryError,
                    "The Sub Prep schedule summary request is invalid.")
                );
        }
        if (normalized.contains(QStringLiteral("exceeds"))
            || normalized.contains(QStringLiteral("limit"))
            || normalized.contains(QStringLiteral("validation")))
        {
            return failure(
                Domain::ErrorCode::Validation,
                errorText(repositoryError,
                    "The Sub Prep schedule summary data failed validation.")
                );
        }
        return failure(
            Domain::ErrorCode::Technical,
            errorText(repositoryError,
                "Sub Prep schedule summaries could not be loaded.")
            );
    }

    [[nodiscard]] static std::string errorText(
        const QString& repositoryError,
        const char* fallback
        )
    {
        const QByteArray bytes = repositoryError.toUtf8();
        return bytes.isEmpty() ? fallback : bytes.toStdString();
    }

    [[nodiscard]] static Application::SubPrepScheduleSummaryReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::SubPrepScheduleSummaryReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
