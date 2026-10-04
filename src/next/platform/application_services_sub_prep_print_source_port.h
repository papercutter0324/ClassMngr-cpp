#pragma once

#include "core/application_services.h"
#include "core/startup_profiler.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
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
// referenced teacher facts from the active database repositories. No
// repository records or pointers escape this call.
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

            DatabaseSession* const session = m_services.databaseSession();
            if (!session || !session->isOpen())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The active database session for Sub Prep print data is unavailable."
                    );
            }

            ClassInfoRepository* const classRepository =
                session->classInfoRepository();
            TeacherRepository* const teacherRepository =
                session->teacherRepository();
            RosterRepository* const rosterRepository =
                session->rosterRepository();
            if (!classRepository || !teacherRepository || !rosterRepository)
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "A Sub Prep print source repository is unavailable."
                    );
            }

            const ::Result<QList<ClassInfo>> loadedInfos =
                classRepository->loadClassInfosForScheduleScope(
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
                if (!session->isOpen())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The active database session became unavailable while loading Sub Prep print data."
                        );
                }
                return repositoryFailure(
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

            QList<int> teacherReadIds;
            teacherReadIds.reserve(
                static_cast<qsizetype>(request.selectedClassIds.size())
                );
            std::unordered_set<int> teacherIdsSeen;
            teacherIdsSeen.reserve(request.selectedClassIds.size());
            for (const int legacyClassId : legacyClassIds)
            {
                const auto infoEntry = infosByLegacyId.find(legacyClassId);
                if (infoEntry == infosByLegacyId.end())
                {
                    continue;
                }

                const int assignedTeacherId = infoEntry->second->teacherId;
                if (assignedTeacherId > 0
                    && teacherIdsSeen.insert(assignedTeacherId).second)
                {
                    teacherReadIds.append(assignedTeacherId);
                }
            }

            using TeacherProfileBatchResult = ::Result<
                QList<TeacherProfileBatchReadRecord>
                >;
            std::optional<TeacherProfileBatchResult> loadedTeacherProfiles;
            std::unordered_map<
                int,
                const TeacherProfileBatchReadRecord*
                > teacherProfilesByLegacyId;
            teacherProfilesByLegacyId.reserve(teacherReadIds.size());
            if (!teacherReadIds.isEmpty())
            {
                loadedTeacherProfiles.emplace(
                    teacherRepository->loadTeacherProfileRecords(
                        teacherReadIds
                        )
                    );
                if (loadedTeacherProfiles->has_value())
                {
                    const QList<TeacherProfileBatchReadRecord>& records =
                        loadedTeacherProfiles->value();
                    if (records.size() != teacherReadIds.size())
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "The teacher profile batch did not match the requested assignments."
                            );
                    }

                    for (qsizetype index = 0;
                         index < records.size();
                         ++index)
                    {
                        const TeacherProfileBatchReadRecord& record =
                            records[index];
                        if (record.teacherId != teacherReadIds[index]
                            || !teacherProfilesByLegacyId.emplace(
                                record.teacherId,
                                &record
                                ).second)
                        {
                            return failure(
                                Domain::ErrorCode::Validation,
                                "The teacher profile batch returned an unexpected assignment order."
                                );
                        }
                    }
                }
            }

            QList<int> rosterReadClassIds;
            rosterReadClassIds.reserve(
                static_cast<qsizetype>(request.selectedClassIds.size())
                );
            std::unordered_set<int> rosterClassIdsSeen;
            rosterClassIdsSeen.reserve(request.selectedClassIds.size());

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
                    if (loadedTeacherProfiles
                        && !loadedTeacherProfiles->has_value())
                    {
                        if (!session->isOpen())
                        {
                            return failure(
                                Domain::ErrorCode::NotFound,
                                "The active database session became unavailable while loading Sub Prep print data."
                                );
                        }

                        return repositoryFailure(
                            loadedTeacherProfiles->error(),
                            "A Sub Prep print teacher could not be loaded."
                            );
                    }

                    const auto profileEntry =
                        teacherProfilesByLegacyId.find(assignedTeacherId);
                    if (profileEntry == teacherProfilesByLegacyId.end())
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "A requested teacher profile was not included in the batch read."
                            );
                    }

                    const ::Result<Teacher>& loadedTeacher =
                        profileEntry->second->profile;
                    if (!loadedTeacher)
                    {
                        if (!session->isOpen())
                        {
                            return failure(
                                Domain::ErrorCode::NotFound,
                                "The active database session became unavailable while loading Sub Prep print data."
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

                        return repositoryFailure(
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
                        .koreanName = utf8(legacyTeacher.teacherKr),
                        .preferredName = utf8(legacyTeacher.preferredName),
                        .preferredRomanization = utf8(
                            legacyTeacher.preferredRomanization
                            ),
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
                if (rosterClassIdsSeen.insert(legacyClassId).second)
                {
                    rosterReadClassIds.append(legacyClassId);
                }
            }

            // Scope and teacher filtering are complete. Read only counts for
            // classes that will be present in the output, while preserving
            // the independent count-zero fallback for each failed roster.
            if (!rosterReadClassIds.isEmpty())
            {
                const auto loadedRosterCounts =
                    rosterRepository->loadSubPrepStudentCountRecords(
                        rosterReadClassIds
                        );
                if (loadedRosterCounts
                    && loadedRosterCounts->size()
                        == rosterReadClassIds.size())
                {
                    std::unordered_map<
                        int,
                        const RosterRepository::SubPrepStudentCountReadEntry*
                        > rosterCountsByClassId;
                    rosterCountsByClassId.reserve(
                        static_cast<std::size_t>(loadedRosterCounts->size())
                        );
                    bool recordsMatchRequest = true;
                    for (std::size_t index = 0;
                         index < static_cast<std::size_t>(
                             loadedRosterCounts->size()
                             );
                         ++index)
                    {
                        const auto& entry = loadedRosterCounts->at(
                            static_cast<qsizetype>(index)
                            );
                        if (entry.classId
                                != rosterReadClassIds.at(
                                    static_cast<qsizetype>(index)
                                    )
                            || !rosterCountsByClassId.emplace(
                                entry.classId,
                                &entry
                                ).second)
                        {
                            recordsMatchRequest = false;
                            break;
                        }
                    }

                    if (recordsMatchRequest)
                    {
                        for (Application::SubPrepPrintClass& classRecord :
                             source.classes)
                        {
                            const auto legacyClassId =
                                legacyId(classRecord.id.value());
                            if (!legacyClassId)
                            {
                                continue;
                            }
                            const auto record = rosterCountsByClassId.find(
                                *legacyClassId
                                );
                            if (record == rosterCountsByClassId.end()
                                || !record->second->studentCount)
                            {
                                continue;
                            }

                            const auto& countRecord = *record->second;
                            classRecord.studentCount =
                                static_cast<std::size_t>(
                                    std::max(
                                        0,
                                        countRecord.studentCount.value()
                                        )
                                    );

                            // A metrics hook must not suppress this or later
                            // class records if instrumentation fails.
                            try
                            {
                                StartupProfiler::recordSubPrepRosterQuery(
                                    countRecord.classId,
                                    countRecord.columnCount,
                                    countRecord.rowCount,
                                    countRecord.cellCount,
                                    countRecord.studentCount.value()
                                    );
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                }
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
    repositoryFailure(
        const QString& repositoryError,
        const char* fallback
        )
    {
        const QByteArray errorBytes = repositoryError.toUtf8();
        return failure(
            isNotFound(repositoryError)
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
