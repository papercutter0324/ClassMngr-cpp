#pragma once

#include "next/application/schedule_import_state_projection.h"

#include <charconv>
#include <set>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ScheduleImportStateSnapshotQuery final
{
    friend bool operator==(
        const ScheduleImportStateSnapshotQuery&,
        const ScheduleImportStateSnapshotQuery&
        ) = default;
};

struct ScheduleImportStateSnapshot final
{
    // Preserve each repository's ordering: teachers use teacher_en order and
    // classes use the existing class repository's name order.
    std::vector<ScheduleImportStateReadTeacherSnapshot> teachers;
    std::vector<ScheduleImportStateReadClassSnapshot> classes;

    friend bool operator==(
        const ScheduleImportStateSnapshot&,
        const ScheduleImportStateSnapshot&
        ) = default;
};

enum class ScheduleImportStateSnapshotFailureKind
{
    ActiveSessionUnavailable,
    RepositoryUnavailable,
    RepositoryReadFailed,
    InvalidSnapshot
};

enum class ScheduleImportStateSnapshotFailureSource
{
    Session,
    Classes,
    Teachers,
    ClassSchedules,
    Snapshot
};

struct ScheduleImportStateSnapshotFailure final
{
    ScheduleImportStateSnapshotFailureKind kind;
    ScheduleImportStateSnapshotFailureSource source;
    std::string message;

    friend bool operator==(
        const ScheduleImportStateSnapshotFailure&,
        const ScheduleImportStateSnapshotFailure&
        ) = default;
};

using ScheduleImportStateSnapshotOutcome = std::variant<
    ScheduleImportStateSnapshot,
    ScheduleImportStateSnapshotFailure
    >;

class ScheduleImportStateSnapshotReadPort
{
public:
    virtual ~ScheduleImportStateSnapshotReadPort() = default;

    [[nodiscard]] virtual ScheduleImportStateSnapshotOutcome
    readCurrentScheduleImportState(
        const ScheduleImportStateSnapshotQuery& query
        ) const = 0;
};

class ScheduleImportStateSnapshotQueryHandler final
{
public:
    [[nodiscard]] static ScheduleImportStateSnapshotOutcome execute(
        const ScheduleImportStateSnapshotQuery& query,
        const ScheduleImportStateSnapshotReadPort& port
        )
    {
        ScheduleImportStateSnapshotOutcome result =
            port.readCurrentScheduleImportState(query);
        auto* snapshot = std::get_if<ScheduleImportStateSnapshot>(&result);
        if (snapshot == nullptr)
        {
            return result;
        }

        std::set<std::string> teacherIds;
        for (const auto& teacher : snapshot->teachers)
        {
            if (!isCanonicalPositiveId(teacher.id.value())
                || !teacherIds.insert(teacher.id.value()).second)
            {
                return invalidSnapshot(
                    "The schedule import state source returned an invalid or duplicate teacher ID."
                    );
            }
        }

        std::set<std::string> classIds;
        for (const auto& classroom : snapshot->classes)
        {
            if (!isCanonicalPositiveId(classroom.id.value())
                || !classIds.insert(classroom.id.value()).second)
            {
                return invalidSnapshot(
                    "The schedule import state source returned an invalid or duplicate class ID."
                    );
            }
            if (classroom.teacherId.value() != "-1"
                && !isCanonicalPositiveId(classroom.teacherId.value()))
            {
                return invalidSnapshot(
                    "The schedule import state source returned an invalid teacher assignment."
                    );
            }
        }

        return result;
    }

private:
    [[nodiscard]] static ScheduleImportStateSnapshotOutcome invalidSnapshot(
        std::string message
        )
    {
        return ScheduleImportStateSnapshotFailure{
            ScheduleImportStateSnapshotFailureKind::InvalidSnapshot,
            ScheduleImportStateSnapshotFailureSource::Snapshot,
            std::move(message)
        };
    }

    [[nodiscard]] static bool isCanonicalPositiveId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return false;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0
            && std::to_string(parsed) == value;
    }
};

} // namespace ClassMngr::Next::Application
