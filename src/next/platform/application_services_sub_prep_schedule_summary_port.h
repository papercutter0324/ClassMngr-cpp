#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/sub_prep_schedule_summary_query.h"

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
            }

            if (legacyClassIds.isEmpty() || legacyDays.isEmpty())
            {
                return Application::SubPrepScheduleSummaryReadResult::success({});
            }

            ClassService* classService = m_services.classService();
            if (!classService || !classService->isAvailable())
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
                classService->subPrepClassSummaries(
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
                if (!classService->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The Sub Prep schedule summary service became unavailable while loading summaries."
                        );
                }
                return legacyFailure(records.error());
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

            Application::ClassSummaryProjectionInput input;
            input.classes.reserve(recordsByClassId.size());
            input.teachers.reserve(recordsByClassId.size());
            std::unordered_map<int, std::size_t> teacherIndexes;
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
                    formatMeetingTimes(record.meetings, legacyDays),
                    Application::kClassSummaryMaxMeetingTextLength
                    );
                if (!gradeText || !levelText || !displayLabel || !meetingText)
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

                    teacherIndexes.emplace(record.teacherId, input.teachers.size());
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
                    .studentCount = static_cast<std::size_t>(
                        record.studentCount
                        ),
                    .order = static_cast<std::int32_t>(requestIndex)
                });
            }

            return Application::SubPrepScheduleSummaryReadResult::success(
                std::move(input)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Sub Prep schedule summaries could not be loaded."
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
    struct Meeting final
    {
        QString day;
        QTime time;
        int dayOrder = 0;
    };

    struct TimeGroup final
    {
        QTime time;
        QStringList days;
        int firstDayOrder = 0;
    };

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
        const int legacyTeacherId
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
        static const QStringList days{
            QStringLiteral("Monday"),
            QStringLiteral("Tuesday"),
            QStringLiteral("Wednesday"),
            QStringLiteral("Thursday"),
            QStringLiteral("Friday"),
            QStringLiteral("Saturday"),
            QStringLiteral("Sunday")
        };
        const int index = days.indexOf(day);
        return index < 0 ? std::nullopt : std::optional<int>(index);
    }

    [[nodiscard]] static QTime parseTime(const QString& value)
    {
        const QStringList formats{
            QStringLiteral("h:mm AP"),
            QStringLiteral("h:mmAP"),
            QStringLiteral("hh:mm AP"),
            QStringLiteral("hh:mmAP"),
            QStringLiteral("H:mm"),
            QStringLiteral("HH:mm"),
            QStringLiteral("H:mm:ss"),
            QStringLiteral("HH:mm:ss")
        };
        for (const QString& format : formats)
        {
            const QTime parsed = QTime::fromString(value.trimmed(), format);
            if (parsed.isValid())
            {
                return parsed;
            }
        }
        return {};
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
        const QStringList& selectedDays
        )
    {
        std::vector<Meeting> meetings;
        meetings.reserve(static_cast<std::size_t>(source.size()));
        for (const SubPrepScheduleMeetingRecord& record : source)
        {
            const auto order = weekdayOrder(record.day);
            if (!order || !selectedDays.contains(record.day))
            {
                continue;
            }

            const QTime time = parseTime(record.startTime);
            if (!time.isValid())
            {
                continue;
            }
            meetings.push_back({record.day, time, *order});
        }

        std::sort(
            meetings.begin(),
            meetings.end(),
            [](const Meeting& left, const Meeting& right)
            {
                if (left.dayOrder != right.dayOrder)
                {
                    return left.dayOrder < right.dayOrder;
                }
                return left.time < right.time;
            }
            );

        std::vector<TimeGroup> groups;
        groups.reserve(meetings.size());
        for (const Meeting& meeting : meetings)
        {
            const auto group = std::find_if(
                groups.begin(),
                groups.end(),
                [&meeting](const TimeGroup& candidate)
                {
                    return candidate.time == meeting.time;
                }
                );
            if (group == groups.end())
            {
                groups.push_back({
                    meeting.time,
                    {meeting.day},
                    meeting.dayOrder
                });
            }
            else if (!group->days.contains(meeting.day))
            {
                group->days.append(meeting.day);
            }
        }

        std::sort(
            groups.begin(),
            groups.end(),
            [](const TimeGroup& left, const TimeGroup& right)
            {
                if (left.firstDayOrder != right.firstDayOrder)
                {
                    return left.firstDayOrder < right.firstDayOrder;
                }
                return left.time < right.time;
            }
            );

        QStringList labels;
        labels.reserve(static_cast<qsizetype>(groups.size()));
        for (const TimeGroup& group : groups)
        {
            QStringList days;
            days.reserve(group.days.size());
            for (const QString& day : group.days)
            {
                days.append(dayAbbreviation(day));
            }
            labels.append(QStringLiteral("%1 %2")
                .arg(days.join(QString()), compactTime(group.time)));
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
        const QString selected = record.teacherPreferredName.trimmed();
        if (!selected.isEmpty())
        {
            return selected;
        }
        const QString english = record.teacherEn.trimmed();
        if (!english.isEmpty())
        {
            return english;
        }
        const QString romanized =
            record.teacherPreferredRomanization.trimmed();
        if (!romanized.isEmpty())
        {
            return romanized;
        }
        const QString korean = record.teacherKr.trimmed();
        return korean.isEmpty() ? QStringLiteral("N/A") : korean;
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
    legacyFailure(const QString& legacyError)
    {
        const QString normalized = legacyError.toLower();
        if (normalized.contains(QStringLiteral("invalid"))
            || normalized.contains(QStringLiteral("unique")))
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                errorText(legacyError,
                    "The Sub Prep schedule summary request is invalid.")
                );
        }
        if (normalized.contains(QStringLiteral("exceeds"))
            || normalized.contains(QStringLiteral("limit"))
            || normalized.contains(QStringLiteral("validation")))
        {
            return failure(
                Domain::ErrorCode::Validation,
                errorText(legacyError,
                    "The Sub Prep schedule summary data failed validation.")
                );
        }
        return failure(
            Domain::ErrorCode::Technical,
            errorText(legacyError,
                "Sub Prep schedule summaries could not be loaded.")
            );
    }

    [[nodiscard]] static std::string errorText(
        const QString& legacyError,
        const char* fallback
        )
    {
        const QByteArray bytes = legacyError.toUtf8();
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
