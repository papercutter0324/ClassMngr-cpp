#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/sub_prep_print_source_query.h"

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Copies only the requested Sub Prep classes, selected-mode meetings, and
// referenced teacher facts from the legacy services. No legacy records or
// service pointers escape this call; the owning ApplicationServices remains
// the adapter's only dependency.
class ApplicationServicesSubPrepPrintSourcePort final
    : public Application::SubPrepPrintSourceReadPort
{
public:
    explicit ApplicationServicesSubPrepPrintSourcePort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesSubPrepPrintSourcePort(
        const ApplicationServicesSubPrepPrintSourcePort&
        ) = delete;
    ApplicationServicesSubPrepPrintSourcePort& operator=(
        const ApplicationServicesSubPrepPrintSourcePort&
        ) = delete;
    ApplicationServicesSubPrepPrintSourcePort(
        ApplicationServicesSubPrepPrintSourcePort&&
        ) = delete;
    ApplicationServicesSubPrepPrintSourcePort& operator=(
        ApplicationServicesSubPrepPrintSourcePort&&
        ) = delete;

    [[nodiscard]] Application::SubPrepPrintSourceReadResult loadSource(
        const Application::SubPrepPrintSourceRequest& request
        ) override
    {
        const auto requestValidation =
            Application::SubPrepPrintSourceQueryDetail::validateRequest(
                request
                );
        if (!requestValidation)
        {
            return Application::SubPrepPrintSourceReadResult::failure(
                requestValidation.error()
                );
        }
        if (request.selectedClassIds.empty() || request.selectedDays.empty())
        {
            return Application::SubPrepPrintSourceReadResult::success({});
        }

        try
        {
            std::vector<int> legacyClassIds;
            legacyClassIds.reserve(request.selectedClassIds.size());
            QList<int> legacyClassIdList;
            legacyClassIdList.reserve(
                static_cast<qsizetype>(request.selectedClassIds.size())
                );
            for (const Domain::ClassId& requestedClassId :
                 request.selectedClassIds)
            {
                const auto parsedId = legacyId(requestedClassId.value());
                if (!parsedId)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A selected class identifier must be a positive integer."
                        );
                }
                legacyClassIds.push_back(*parsedId);
                legacyClassIdList.append(*parsedId);
            }

            const auto selectedDays = selectedDaySet(request);
            QStringList selectedDayLabels;
            selectedDayLabels.reserve(
                static_cast<qsizetype>(request.selectedDays.size())
                );
            for (const auto day : request.selectedDays)
            {
                const auto label = weekdayLabel(day);
                if (!label)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A selected Sub Prep weekday is invalid."
                        );
                }
                selectedDayLabels.append(*label);
            }

            const ScheduleType legacyMode =
                request.mode == Application::ScheduleViewMode::Regular
                ? ScheduleType::Regular
                : ScheduleType::Intensive;

            ClassService* classService = m_services.classService();
            RosterService* rosterService = m_services.rosterService();
            TeacherService* teacherService = m_services.teacherService();
            if (!classService || !classService->isAvailable()
                || !rosterService || !rosterService->isAvailable()
                || !teacherService || !teacherService->isAvailable())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "A Sub Prep print source service is unavailable."
                    );
            }

            const ::Result<QList<ClassInfo>> loadedInfos =
                classService->classInfosForScheduleScope(
                    legacyClassIdList,
                    selectedDayLabels,
                    legacyMode,
                    static_cast<int>(
                        Application::kSubPrepPrintSourceMaxMeetingsPerClass
                        ),
                    static_cast<int>(
                        Application::kSubPrepPrintSourceMaxMeetings
                        )
                    );
            if (!loadedInfos)
            {
                if (!classService->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The class service became unavailable while loading Sub Prep print data."
                        );
                }
                return legacyFailure(
                    loadedInfos.error(),
                    "Scoped Sub Prep class information could not be loaded."
                    );
            }

            std::unordered_map<int, const ClassInfo*> infosByLegacyId;
            infosByLegacyId.reserve(
                static_cast<std::size_t>(loadedInfos->size())
                );
            std::size_t scopedMeetingCount = 0;
            for (const ClassInfo& info : loadedInfos.value())
            {
                if (info.classId <= 0
                    || !infosByLegacyId.emplace(info.classId, &info).second)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Scoped Sub Prep class information contains an invalid or duplicate class identifier."
                        );
                }

                const QList<ClassTime>& schedule =
                    legacyMode == ScheduleType::Intensive
                    ? info.intensiveTimes
                    : info.classTimes;
                if (schedule.size()
                    > static_cast<qsizetype>(
                        Application::kSubPrepPrintSourceMaxMeetingsPerClass
                        + 1
                        ))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Scoped Sub Prep class information exceeds the bounded schedule read limit."
                        );
                }
                if (schedule.size()
                    > static_cast<qsizetype>(
                        Application::kSubPrepPrintSourceMaxMeetings + 1
                        - scopedMeetingCount
                        ))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Scoped Sub Prep class information exceeds the bounded total schedule read limit."
                        );
                }
                scopedMeetingCount += static_cast<std::size_t>(schedule.size());
            }
            if (scopedMeetingCount
                > Application::kSubPrepPrintSourceMaxMeetings)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Sub Prep print meetings exceed the total bounded limit."
                    );
            }

            Application::SubPrepPrintSourceInput source;
            source.classes.reserve(request.selectedClassIds.size());
            std::size_t totalMeetings = 0;

            // The cache is lookup-only: insertion order in source.teachers is
            // the stable first-reference order from the requested classes.
            std::unordered_map<
                int,
                std::optional<Application::SubPrepPrintTeacher>
                > teachersByLegacyId;
            teachersByLegacyId.reserve(request.selectedClassIds.size());

            for (std::size_t index = 0;
                 index < request.selectedClassIds.size();
                 ++index)
            {
                const Domain::ClassId& requestedClassId =
                    request.selectedClassIds[index];
                const int legacyClassId = legacyClassIds[index];
                const auto infoEntry = infosByLegacyId.find(legacyClassId);
                if (infoEntry == infosByLegacyId.end())
                {
                    // The scoped SQL query omits classes without a meeting in
                    // the selected mode and day set.
                    continue;
                }

                const ClassInfo& info = *infoEntry->second;
                if (info.classId != legacyClassId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Loaded class information does not match the requested class."
                        );
                }

                // Copy the selected schedule subset before roster or teacher
                // reads. Preserve the legacy list order and exact day labels.
                Application::SubPrepPrintClass classRecord{
                    .id = requestedClassId,
                    .teacherId = std::nullopt,
                    .grade = utf8(info.classGrade),
                    .level = utf8(info.classLevel),
                    .classNotes = utf8(info.notes),
                    .classColor = utf8(info.classColor),
                    .fontColor = utf8(info.fontColor),
                    .studentCount = 0,
                    .meetings = {}
                };

                const QList<ClassTime>& schedule =
                    legacyMode == ScheduleType::Intensive
                    ? info.intensiveTimes
                    : info.classTimes;
                if (schedule.size()
                    > static_cast<qsizetype>(
                        Application::kSubPrepPrintSourceMaxMeetingsPerClass
                        + 1
                        ))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Scoped Sub Prep class information exceeds the bounded schedule read limit."
                        );
                }

                for (const ClassTime& meeting : schedule)
                {
                    const auto weekday = weekdayFor(meeting.day);
                    if (!weekday || !selectedDays.contains(*weekday))
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "Scoped Sub Prep class information contains a meeting outside the requested days."
                            );
                    }

                    classRecord.meetings.push_back({
                        .weekday = *weekday,
                        .startTime = utf8(meeting.startTime),
                        .endTime = utf8(meeting.endTime)
                    });
                }

                const int assignedTeacherId = info.teacherId;
                if (assignedTeacherId <= 0)
                {
                    continue;
                }

                auto teacher = teachersByLegacyId.find(assignedTeacherId);
                std::optional<Application::SubPrepPrintTeacher> newlyLoadedTeacher;
                if (teacher == teachersByLegacyId.end())
                {
                    const ::Result<Teacher> loadedTeacher =
                        teacherService->teacher(assignedTeacherId);
                    if (!loadedTeacher)
                    {
                        if (!teacherService->isAvailable())
                        {
                            return failure(
                                Domain::ErrorCode::NotFound,
                                "The teacher service became unavailable while loading Sub Prep print data."
                                );
                        }

                        if (isNotFound(loadedTeacher.error()))
                        {
                            teachersByLegacyId.emplace(
                                assignedTeacherId,
                                std::nullopt
                                );
                            continue;
                        }

                        return legacyFailure(
                            loadedTeacher.error(),
                            "A Sub Prep print teacher could not be loaded."
                            );
                    }

                    const Teacher& legacyTeacher = loadedTeacher.value();
                    if (legacyTeacher.id != assignedTeacherId)
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "Loaded teacher information does not match the class assignment."
                            );
                    }

                    Application::SubPrepPrintTeacher copiedTeacher{
                        .id = teacherId(assignedTeacherId),
                        .englishName = utf8(legacyTeacher.teacherEn),
                        .room = utf8(legacyTeacher.roomNumber),
                        .wifiName = utf8(legacyTeacher.wifiName),
                        .wifiPassword = utf8(legacyTeacher.wifiPassword),
                        .internetType = utf8(legacyTeacher.internetType),
                        .zoomId = utf8(legacyTeacher.zoomId),
                        .zoomPassword = utf8(legacyTeacher.zoomPassword),
                        .projectionType = utf8(legacyTeacher.projectionType),
                        .teacherNotes = utf8(legacyTeacher.notes)
                    };
                    newlyLoadedTeacher = std::move(copiedTeacher);
                }
                else if (!teacher->second.has_value())
                {
                    continue;
                }

                const Domain::TeacherId& outputTeacherId = newlyLoadedTeacher
                    ? newlyLoadedTeacher->id
                    : teacher->second->id;
                classRecord.teacherId = outputTeacherId;

                if (classRecord.meetings.size()
                    > Application::kSubPrepPrintSourceMaxMeetings - totalMeetings)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Sub Prep print meetings exceed the total bounded limit."
                        );
                }
                totalMeetings += classRecord.meetings.size();

                // Resolve the teacher before the roster read. Roster failures
                // retain the legacy count-zero fallback.
                const ::Result<int> studentCount =
                    rosterService->studentCount(legacyClassId);
                if (studentCount)
                {
                    classRecord.studentCount = static_cast<std::size_t>(
                        std::max(0, studentCount.value())
                        );
                }

                if (newlyLoadedTeacher)
                {
                    teacher = teachersByLegacyId.emplace(
                        assignedTeacherId,
                        *newlyLoadedTeacher
                        ).first;
                    source.teachers.push_back(
                        std::move(*newlyLoadedTeacher)
                        );
                }

                source.classes.push_back(std::move(classRecord));
            }

            return Application::SubPrepPrintSourceReadResult::success(
                std::move(source)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Sub Prep print source data could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Sub Prep print source data could not be loaded."
                );
        }
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
        if (error != std::errc{} || end != value.data() + value.size())
        {
            return std::nullopt;
        }

        if (parsed <= 0 || std::to_string(parsed) != value)
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

    [[nodiscard]] static std::string utf8(
        const QString& value
        )
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static std::optional<Application::SubPrepWeekday>
    weekdayFor(
        const QString& day
        )
    {
        using Application::SubPrepWeekday;
        if (day == QStringLiteral("Monday"))
        {
            return SubPrepWeekday::Monday;
        }
        if (day == QStringLiteral("Tuesday"))
        {
            return SubPrepWeekday::Tuesday;
        }
        if (day == QStringLiteral("Wednesday"))
        {
            return SubPrepWeekday::Wednesday;
        }
        if (day == QStringLiteral("Thursday"))
        {
            return SubPrepWeekday::Thursday;
        }
        if (day == QStringLiteral("Friday"))
        {
            return SubPrepWeekday::Friday;
        }
        if (day == QStringLiteral("Saturday"))
        {
            return SubPrepWeekday::Saturday;
        }
        if (day == QStringLiteral("Sunday"))
        {
            return SubPrepWeekday::Sunday;
        }
        return std::nullopt;
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

    [[nodiscard]] static std::unordered_set<Application::SubPrepWeekday>
    selectedDaySet(
        const Application::SubPrepPrintSourceRequest& request
        )
    {
        return std::unordered_set<Application::SubPrepWeekday>(
            request.selectedDays.cbegin(),
            request.selectedDays.cend()
            );
    }

    [[nodiscard]] static bool isNotFound(
        const QString& legacyError
        )
    {
        const QString normalized = legacyError.toLower();
        return normalized.contains(QStringLiteral("no matching record"))
            || normalized.contains(QStringLiteral("not found"))
            || normalized.contains(QStringLiteral("does not exist"));
    }

    [[nodiscard]] static Application::SubPrepPrintSourceReadResult
    legacyFailure(
        const QString& legacyError,
        const char* fallback
        )
    {
        const QByteArray errorBytes = legacyError.toUtf8();
        return failure(
            isNotFound(legacyError)
                ? Domain::ErrorCode::NotFound
                : Domain::ErrorCode::Technical,
            errorBytes.isEmpty()
                ? fallback
                : errorBytes.toStdString()
            );
    }

    [[nodiscard]] static Application::SubPrepPrintSourceReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::SubPrepPrintSourceReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
