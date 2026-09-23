#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/sub_prep_roster_output_source_query.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Reads only the selected classes and schedule facts from the active
// ApplicationServices session. RosterRepository projects requested columns
// under caller-supplied remaining budgets before this adapter copies them into
// the owning Application values.
class ApplicationServicesSubPrepRosterOutputSourcePort final
    : public Application::SubPrepRosterOutputSourceReadPort
{
public:
    explicit ApplicationServicesSubPrepRosterOutputSourcePort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesSubPrepRosterOutputSourcePort(
        const ApplicationServicesSubPrepRosterOutputSourcePort&
        ) = delete;
    ApplicationServicesSubPrepRosterOutputSourcePort& operator=(
        const ApplicationServicesSubPrepRosterOutputSourcePort&
        ) = delete;
    ApplicationServicesSubPrepRosterOutputSourcePort(
        ApplicationServicesSubPrepRosterOutputSourcePort&&
        ) = delete;
    ApplicationServicesSubPrepRosterOutputSourcePort& operator=(
        ApplicationServicesSubPrepRosterOutputSourcePort&&
        ) = delete;

    [[nodiscard]] Application::SubPrepRosterOutputSourceReadResult loadSource(
        const Application::SubPrepRosterOutputSourceRequest& request
        ) override
    {
        using Application::SubPrepRosterOutputSourceQueryDetail::validateRequest;
        const auto requestValidation = validateRequest(request);
        if (!requestValidation)
        {
            return failure(
                requestValidation.error().code,
                requestValidation.error().message
                );
        }
        if (request.selectedClassIds.empty() || request.selectedDays.empty())
        {
            return Application::SubPrepRosterOutputSourceReadResult::success({});
        }

        try
        {
            if (request.selectedExtraColumns.size() + 2
                > Application::kSubPrepRosterOutputMaxRosterColumns)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Selected roster columns exceed the output capacity."
                    );
            }

            QList<int> legacyClassIds;
            legacyClassIds.reserve(
                static_cast<qsizetype>(request.selectedClassIds.size())
                );
            for (const Domain::ClassId& classId : request.selectedClassIds)
            {
                const auto parsed = legacyId(classId.value());
                if (!parsed)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Selected class identifiers must be canonical positive integers."
                        );
                }
                legacyClassIds.append(*parsed);
            }

            QStringList selectedDays;
            selectedDays.reserve(
                static_cast<qsizetype>(request.selectedDays.size())
                );
            std::unordered_set<Application::SubPrepWeekday> selectedDaySet;
            for (const auto day : request.selectedDays)
            {
                const auto label = weekdayLabel(day);
                if (!label)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A selected roster-output weekday is invalid."
                        );
                }
                selectedDays.append(*label);
                selectedDaySet.insert(day);
            }

            QStringList rosterColumns{
                QStringLiteral("English"),
                QStringLiteral("Korean")
            };
            for (const std::string& encodedName : request.selectedExtraColumns)
            {
                const auto decoded = decodeUtf8(encodedName);
                if (!decoded)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "A selected roster column is not valid bounded UTF-8."
                        );
                }
                if (decoded->compare(
                        QStringLiteral("English"),
                        Qt::CaseInsensitive
                        ) == 0
                    || decoded->compare(
                        QStringLiteral("Korean"),
                        Qt::CaseInsensitive
                        ) == 0)
                {
                    // The name columns are already part of the output schema.
                    continue;
                }
                if (rosterColumns.contains(*decoded, Qt::CaseInsensitive))
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Selected roster columns must be unique without case."
                        );
                }
                rosterColumns.append(*decoded);
            }

            ClassService* classService = m_services.classService();
            TeacherService* teacherService = m_services.teacherService();
            RosterService* rosterService = m_services.rosterService();
            if (!m_services.hasOpenDatabase()
                || !classService || !classService->isAvailable()
                || !teacherService || !teacherService->isAvailable()
                || !rosterService || !rosterService->isAvailable())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The Sub Prep roster output services are unavailable."
                    );
            }

            const ScheduleType scheduleType =
                request.mode == Application::ScheduleViewMode::Intensive
                ? ScheduleType::Intensive
                : ScheduleType::Regular;
            const ::Result<QList<ClassInfo>> loadedSchedule =
                classService->classInfosForScheduleScope(
                    legacyClassIds,
                    selectedDays,
                    scheduleType,
                    static_cast<int>(
                        Application::kSubPrepPrintSourceMaxMeetingsPerClass
                        ),
                    static_cast<int>(
                        Application::kSubPrepPrintSourceMaxMeetings
                        ),
                    true
                    );
            if (!loadedSchedule)
            {
                if (!classService->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The class service became unavailable while loading roster output."
                        );
                }
                return legacyFailure(
                    loadedSchedule.error(),
                    "Selected Sub Prep roster schedule could not be loaded."
                    );
            }

            QHash<int, const ClassInfo*> scheduleByClassId;
            scheduleByClassId.reserve(loadedSchedule->size());
            std::size_t totalMeetings = 0;
            for (const ClassInfo& scheduleInfo : loadedSchedule.value())
            {
                if (scheduleInfo.classId <= 0
                    || scheduleByClassId.contains(scheduleInfo.classId))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Scoped roster schedule contains an invalid or duplicate class."
                        );
                }
                scheduleByClassId.insert(scheduleInfo.classId, &scheduleInfo);

                const QList<ClassTime>& meetings =
                    scheduleType == ScheduleType::Intensive
                    ? scheduleInfo.intensiveTimes
                    : scheduleInfo.classTimes;
                if (meetings.size()
                    > static_cast<qsizetype>(
                        Application::kSubPrepPrintSourceMaxMeetingsPerClass
                        ))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A selected class exceeds the roster-output meeting limit."
                        );
                }
                if (totalMeetings
                    > Application::kSubPrepPrintSourceMaxMeetings
                        - static_cast<std::size_t>(meetings.size()))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Roster-output meetings exceed their total limit."
                        );
                }
                totalMeetings += static_cast<std::size_t>(meetings.size());
                for (const ClassTime& meeting : meetings)
                {
                    const auto weekday = weekdayFor(meeting.day);
                    if (!weekday || !selectedDaySet.contains(*weekday))
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "Scoped roster schedule contains an unselected weekday."
                            );
                    }
                }
            }

            Application::SubPrepRosterOutputSourceInput source;
            source.classes.reserve(request.selectedClassIds.size());
            std::unordered_set<int> copiedTeacherIds;
            copiedTeacherIds.reserve(request.selectedClassIds.size());
            std::size_t totalRows = 0;
            std::size_t totalCells = 0;
            std::size_t totalTextBytes = 0;

            for (std::size_t selectedIndex = 0;
                 selectedIndex < request.selectedClassIds.size();
                 ++selectedIndex)
            {
                const int classId = legacyClassIds.at(
                    static_cast<qsizetype>(selectedIndex)
                    );
                const auto schedule = scheduleByClassId.constFind(classId);
                if (schedule == scheduleByClassId.cend())
                {
                    // Classes without a meeting in this selected scope do not
                    // have roster documents in the legacy package.
                    continue;
                }

                const ::Result<Classroom> classroom =
                    classService->classroom(classId);
                if (!classroom)
                {
                    if (!classService->isAvailable())
                    {
                        return failure(
                            Domain::ErrorCode::NotFound,
                            "The class service became unavailable while loading a selected class."
                            );
                    }
                    return legacyFailure(
                        classroom.error(),
                        "A selected Sub Prep class could not be loaded."
                        );
                }
                if (classroom->id != classId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Loaded class identity does not match the selected class."
                        );
                }

                const ::Result<ClassInfo> loadedInfo =
                    classService->classInfo(classId);
                if (!loadedInfo)
                {
                    if (!classService->isAvailable())
                    {
                        return failure(
                            Domain::ErrorCode::NotFound,
                            "The class service became unavailable while loading class output details."
                            );
                    }
                    return legacyFailure(
                        loadedInfo.error(),
                        "Selected Sub Prep class output details could not be loaded."
                        );
                }
                const ClassInfo& info = loadedInfo.value();
                const ClassInfo& scheduleInfo = *schedule.value();
                if (info.classId != classId
                    || info.teacherId != scheduleInfo.teacherId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Loaded class output details do not match the selected schedule."
                        );
                }

                Application::SubPrepRosterOutputClass classRecord{
                    request.selectedClassIds[selectedIndex]};
                if (!copyBoundedUtf8(
                    classroom->name,
                    Application::kSubPrepRosterOutputMaxClassNameBytes,
                    &totalTextBytes,
                    &classRecord.classroomName
                    )
                    || !copyBoundedUtf8(
                    info.classGrade,
                    Application::kSubPrepPrintSourceMaxGradeLength,
                    &totalTextBytes,
                    &classRecord.grade
                    )
                    || !copyBoundedUtf8(
                    info.classLevel,
                    Application::kSubPrepPrintSourceMaxLevelLength,
                    &totalTextBytes,
                    &classRecord.level
                    )
                    || !copyBoundedUtf8(
                    info.teacherEn,
                    Application::kSubPrepPrintSourceMaxEnglishNameLength,
                    &totalTextBytes,
                    &classRecord.classTeacherEnglishName
                    )
                    || !copyBoundedUtf8(
                    info.teacherKr,
                    Application::kSubPrepPrintSourceMaxKoreanNameLength,
                    &totalTextBytes,
                    &classRecord.classTeacherKoreanName
                    )
                    || !copyBoundedUtf8(
                    info.roomNumber,
                    Application::kSubPrepPrintSourceMaxRoomLength,
                    &totalTextBytes,
                    &classRecord.room
                    )
                    || !copyBoundedUtf8(
                    info.wifiName,
                    Application::kSubPrepPrintSourceMaxWifiNameLength,
                    &totalTextBytes,
                    &classRecord.wifiName
                    )
                    || !copyBoundedUtf8(
                    info.wifiPassword,
                    Application::kSubPrepPrintSourceMaxWifiPasswordLength,
                    &totalTextBytes,
                    &classRecord.wifiPassword
                    )
                    || !copyBoundedUtf8(
                    info.zoomId,
                    Application::kSubPrepPrintSourceMaxZoomIdLength,
                    &totalTextBytes,
                    &classRecord.zoomId
                    )
                    || !copyBoundedUtf8(
                    info.zoomPassword,
                    Application::kSubPrepPrintSourceMaxZoomPasswordLength,
                    &totalTextBytes,
                    &classRecord.zoomPassword
                    ))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Selected class output text exceeds its bounded source limit."
                        );
                }

                if (info.teacherId > 0)
                {
                    classRecord.teacherId = teacherId(info.teacherId);
                    if (copiedTeacherIds.insert(info.teacherId).second)
                    {
                        const ::Result<Teacher> loadedTeacher =
                            teacherService->teacher(info.teacherId);
                        if (!loadedTeacher)
                        {
                            if (!teacherService->isAvailable())
                            {
                                return failure(
                                    Domain::ErrorCode::NotFound,
                                    "The teacher service became unavailable while loading roster output."
                                    );
                            }
                            return legacyFailure(
                                loadedTeacher.error(),
                                "The selected class teacher could not be loaded."
                                );
                        }
                        if (loadedTeacher->id != info.teacherId)
                        {
                            return failure(
                                Domain::ErrorCode::Validation,
                                "Loaded teacher identity does not match the class assignment."
                                );
                        }

                        Application::SubPrepRosterOutputTeacher outputTeacher{
                            teacherId(info.teacherId)};
                        if (!copyBoundedUtf8(
                            loadedTeacher->teacherEn,
                            Application::kSubPrepPrintSourceMaxEnglishNameLength,
                            &totalTextBytes,
                            &outputTeacher.englishName
                            )
                            || !copyBoundedUtf8(
                            loadedTeacher->teacherKr,
                            Application::kSubPrepPrintSourceMaxKoreanNameLength,
                            &totalTextBytes,
                            &outputTeacher.koreanName
                            )
                            || !copyBoundedUtf8(
                            loadedTeacher->preferredName,
                            Application::kSubPrepPrintSourceMaxPreferredNameLength,
                            &totalTextBytes,
                            &outputTeacher.preferredName
                            )
                            || !copyBoundedUtf8(
                            loadedTeacher->preferredRomanization,
                            Application::kSubPrepPrintSourceMaxPreferredRomanizationLength,
                            &totalTextBytes,
                            &outputTeacher.preferredRomanization
                            ))
                        {
                            return failure(
                                Domain::ErrorCode::Validation,
                                "Selected teacher output text exceeds its bounded source limit."
                                );
                        }
                        source.teachers.push_back(std::move(outputTeacher));
                    }
                }

                const QList<ClassTime>& selectedMeetings =
                    scheduleType == ScheduleType::Intensive
                    ? scheduleInfo.intensiveTimes
                    : scheduleInfo.classTimes;
                classRecord.meetings.reserve(
                    static_cast<std::size_t>(selectedMeetings.size())
                    );
                for (const ClassTime& meeting : selectedMeetings)
                {
                    const auto weekday = weekdayFor(meeting.day);
                    if (!weekday || !selectedDaySet.contains(*weekday))
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "Selected class output contains an unselected meeting day."
                            );
                    }
                    Application::SubPrepRosterOutputMeeting outputMeeting;
                    outputMeeting.weekday = *weekday;
                    if (!copyBoundedUtf8(
                        meeting.startTime,
                        Application::kSubPrepPrintSourceMaxMeetingTimeLength,
                        &totalTextBytes,
                        &outputMeeting.startTime
                        )
                        || !copyBoundedUtf8(
                        meeting.endTime,
                        Application::kSubPrepPrintSourceMaxMeetingTimeLength,
                        &totalTextBytes,
                        &outputMeeting.endTime
                        ))
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "A selected meeting time exceeds its bounded source limit."
                            );
                    }
                    classRecord.meetings.push_back(std::move(outputMeeting));
                }

                const std::size_t rowsRemaining = std::min(
                    Application::kSubPrepRosterOutputMaxRowsPerClass,
                    Application::kSubPrepRosterOutputMaxTotalRows - totalRows
                    );
                const std::size_t cellsRemaining =
                    Application::kSubPrepRosterOutputMaxTotalCells - totalCells;
                const std::size_t textBytesRemaining =
                    Application::kSubPrepRosterOutputMaxTotalTextBytes
                    - totalTextBytes;
                const ::Result<Roster> loadedRoster =
                    rosterService->rosterForOutput(
                        classId,
                        rosterColumns,
                        rowsRemaining,
                        cellsRemaining,
                        textBytesRemaining
                        );
                if (!loadedRoster)
                {
                    if (!rosterService->isAvailable())
                    {
                        return failure(
                            Domain::ErrorCode::NotFound,
                            "The roster service became unavailable while loading selected output."
                            );
                    }
                    return legacyFailure(
                        loadedRoster.error(),
                        "A selected class roster could not be loaded within its output limits."
                        );
                }

                const Roster& roster = loadedRoster.value();
                if (roster.columns.size()
                        > static_cast<qsizetype>(
                            Application::kSubPrepRosterOutputMaxRosterColumns
                            )
                    || roster.rows.size()
                        > static_cast<qsizetype>(rowsRemaining))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A selected roster exceeds its output projection limits."
                        );
                }
                if (totalRows
                    > Application::kSubPrepRosterOutputMaxTotalRows
                        - static_cast<std::size_t>(roster.rows.size()))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Roster-output rows exceed their total limit."
                        );
                }
                totalRows += static_cast<std::size_t>(roster.rows.size());

                classRecord.rosterColumns.reserve(
                    static_cast<std::size_t>(roster.columns.size())
                    );
                for (const QString& column : roster.columns)
                {
                    std::string outputColumn;
                    if (!copyBoundedUtf8(
                            column,
                            Application::kSubPrepRosterOutputMaxColumnNameBytes,
                            &totalTextBytes,
                            &outputColumn
                            ))
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "A selected roster column exceeds its bounded text limit."
                            );
                    }
                    classRecord.rosterColumns.push_back(
                        std::move(outputColumn)
                        );
                }

                classRecord.rosterRows.reserve(
                    static_cast<std::size_t>(roster.rows.size())
                    );
                for (const QStringList& row : roster.rows)
                {
                    if (row.size() != roster.columns.size())
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "A bounded roster row has a mismatched cell count."
                            );
                    }
                    if (totalCells
                        > Application::kSubPrepRosterOutputMaxTotalCells
                            - static_cast<std::size_t>(row.size()))
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "Roster-output cells exceed their total limit."
                            );
                    }
                    totalCells += static_cast<std::size_t>(row.size());

                    std::vector<std::string> outputRow;
                    outputRow.reserve(static_cast<std::size_t>(row.size()));
                    for (const QString& cell : row)
                    {
                        std::string outputCell;
                        if (!copyBoundedUtf8(
                                cell,
                                Application::kSubPrepRosterOutputMaxCellBytes,
                                &totalTextBytes,
                                &outputCell
                                ))
                        {
                            return failure(
                                Domain::ErrorCode::Validation,
                                "A selected roster cell exceeds its bounded text limit."
                                );
                        }
                        outputRow.push_back(std::move(outputCell));
                    }
                    classRecord.rosterRows.push_back(std::move(outputRow));
                }

                source.classes.push_back(std::move(classRecord));
            }

            return Application::SubPrepRosterOutputSourceReadResult::success(
                std::move(source)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Sub Prep roster output data could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Sub Prep roster output data could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(const std::string& value)
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

    [[nodiscard]] static Domain::TeacherId teacherId(const int id)
    {
        return *Domain::TeacherId::fromString(std::to_string(id));
    }

    [[nodiscard]] static std::optional<QString> decodeUtf8(
        const std::string& value
        )
    {
        if (value.size() > Application::kSubPrepRosterOutputMaxColumnNameBytes)
        {
            return std::nullopt;
        }
        const QByteArray bytes(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
        const QString decoded = QString::fromUtf8(bytes);
        if (decoded.toUtf8() != bytes)
        {
            return std::nullopt;
        }
        return decoded;
    }

    [[nodiscard]] static bool copyBoundedUtf8(
        const QString& value,
        const std::size_t maxFieldBytes,
        std::size_t* totalBytes,
        std::string* output
        )
    {
        // Reject by UTF-16 length before creating a UTF-8 copy. UTF-8 uses at
        // least one byte for every UTF-16 code unit.
        if (static_cast<std::size_t>(value.size()) > maxFieldBytes)
        {
            return false;
        }

        const QByteArray bytes = value.toUtf8();
        const std::size_t byteCount = static_cast<std::size_t>(bytes.size());
        if (byteCount > maxFieldBytes
            || *totalBytes
                > Application::kSubPrepRosterOutputMaxTotalTextBytes
                    - byteCount)
        {
            return false;
        }
        *totalBytes += byteCount;
        *output = bytes.toStdString();
        return true;
    }

    [[nodiscard]] static std::optional<int> weekdayValue(
        const QString& day
        )
    {
        using Application::SubPrepWeekday;
        if (day == QStringLiteral("Monday"))
        {
            return static_cast<int>(SubPrepWeekday::Monday);
        }
        if (day == QStringLiteral("Tuesday"))
        {
            return static_cast<int>(SubPrepWeekday::Tuesday);
        }
        if (day == QStringLiteral("Wednesday"))
        {
            return static_cast<int>(SubPrepWeekday::Wednesday);
        }
        if (day == QStringLiteral("Thursday"))
        {
            return static_cast<int>(SubPrepWeekday::Thursday);
        }
        if (day == QStringLiteral("Friday"))
        {
            return static_cast<int>(SubPrepWeekday::Friday);
        }
        if (day == QStringLiteral("Saturday"))
        {
            return static_cast<int>(SubPrepWeekday::Saturday);
        }
        if (day == QStringLiteral("Sunday"))
        {
            return static_cast<int>(SubPrepWeekday::Sunday);
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<Application::SubPrepWeekday>
    weekdayFor(const QString& day)
    {
        const auto value = weekdayValue(day);
        if (!value)
        {
            return std::nullopt;
        }
        return static_cast<Application::SubPrepWeekday>(*value);
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

    [[nodiscard]] static Application::SubPrepRosterOutputSourceReadResult
    legacyFailure(const QString& message, const char* fallback)
    {
        const QString normalized = message.toLower();
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        if (normalized.contains(QStringLiteral("no matching record"))
            || normalized.contains(QStringLiteral("not found"))
            || normalized.contains(QStringLiteral("does not exist")))
        {
            code = Domain::ErrorCode::NotFound;
        }
        else if (normalized.contains(QStringLiteral("limit"))
                 || normalized.contains(QStringLiteral("exceed")))
        {
            code = Domain::ErrorCode::Validation;
        }
        else if (normalized.contains(QStringLiteral("invalid")))
        {
            code = Domain::ErrorCode::InvalidInput;
        }

        const QByteArray bytes = message.toUtf8();
        return failure(
            code,
            bytes.isEmpty() ? std::string(fallback) : bytes.toStdString()
            );
    }

    [[nodiscard]] static Application::SubPrepRosterOutputSourceReadResult
    failure(Domain::ErrorCode code, std::string message)
    {
        return Application::SubPrepRosterOutputSourceReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
