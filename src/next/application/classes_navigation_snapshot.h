#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassesNavigationClass final
{
    Domain::ClassId classId;
    std::u16string className;

    friend bool operator==(
        const ClassesNavigationClass&,
        const ClassesNavigationClass&
        ) = default;
};

struct ClassesNavigationScheduleRow final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;

    friend bool operator==(
        const ClassesNavigationScheduleRow&,
        const ClassesNavigationScheduleRow&
        ) = default;
};

struct ClassesNavigationDetails final
{
    std::u16string grade;
    std::u16string level;
    std::vector<ClassesNavigationScheduleRow> regularSchedule;
    std::vector<ClassesNavigationScheduleRow> intensiveSchedule;
    std::u16string teacherEnglishName;
    std::u16string teacherKoreanName;

    friend bool operator==(
        const ClassesNavigationDetails&,
        const ClassesNavigationDetails&
        ) = default;
};

using ClassesNavigationDetailsResult = Domain::Result<ClassesNavigationDetails>;

struct ClassesNavigationEntry final
{
    Domain::ClassId classId;
    std::u16string className;
    ClassesNavigationDetailsResult details =
        ClassesNavigationDetailsResult::success({});

    friend bool operator==(
        const ClassesNavigationEntry& left,
        const ClassesNavigationEntry& right
        )
    {
        if (left.classId != right.classId || left.className != right.className
            || static_cast<bool>(left.details)
                != static_cast<bool>(right.details))
        {
            return false;
        }

        return left.details
            ? left.details.value() == right.details.value()
            : left.details.error() == right.details.error();
    }
};

struct ClassesNavigationSnapshot final
{
    std::vector<ClassesNavigationEntry> classes;

    friend bool operator==(
        const ClassesNavigationSnapshot&,
        const ClassesNavigationSnapshot&
        ) = default;
};

struct ClassesNavigationSnapshotQuery final
{
    std::vector<ClassesNavigationClass> classes;

    friend bool operator==(
        const ClassesNavigationSnapshotQuery&,
        const ClassesNavigationSnapshotQuery&
        ) = default;
};

using ClassesNavigationSnapshotResult =
    Domain::Result<ClassesNavigationSnapshot>;

class ClassesNavigationSnapshotReadPort
{
public:
    virtual ~ClassesNavigationSnapshotReadPort() = default;

    [[nodiscard]] virtual ClassesNavigationSnapshotResult readClasses(
        const ClassesNavigationSnapshotQuery& query
        ) const = 0;
};

class ClassesNavigationSnapshotQueryHandler final
{
public:
    [[nodiscard]] static ClassesNavigationSnapshotResult execute(
        const ClassesNavigationSnapshotQuery& query,
        const ClassesNavigationSnapshotReadPort& port
        )
    {
        std::set<std::string> classIds;
        for (const ClassesNavigationClass& source : query.classes)
        {
            if (!isCanonicalPositiveDecimalId(source.classId.value()))
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Class IDs must be canonical positive integers."
                    );
            }

            if (!classIds.insert(source.classId.value()).second)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Class IDs must be unique."
                    );
            }
        }

        if (query.classes.empty())
        {
            return ClassesNavigationSnapshotResult::success({});
        }

        ClassesNavigationSnapshotResult source = port.readClasses(query);
        if (!source)
        {
            return ClassesNavigationSnapshotResult::failure(source.error());
        }

        if (source.value().classes.size() != query.classes.size())
        {
            return failure(
                Domain::ErrorCode::Validation,
                "A classes navigation read returned a different number of entries."
                );
        }

        for (std::size_t index = 0; index < query.classes.size(); ++index)
        {
            const ClassesNavigationClass& requested = query.classes[index];
            const ClassesNavigationEntry& returned =
                source.value().classes[index];
            if (returned.classId != requested.classId
                || returned.className != requested.className)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "A classes navigation read changed class identity or order."
                    );
            }
        }

        return source;
    }

private:
    [[nodiscard]] static ClassesNavigationSnapshotResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return ClassesNavigationSnapshotResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::InvalidInput
        });
    }
};

} // namespace ClassMngr::Next::Application
