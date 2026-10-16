#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ScheduleTestingClassChoice final
{
    Domain::ClassId classId;
    std::u16string name;
    std::u16string grade;
    std::u16string level;
    std::u16string room;

    friend bool operator==(
        const ScheduleTestingClassChoice&,
        const ScheduleTestingClassChoice&
        ) = default;
};

struct ScheduleTestingClassChoicesReadQuery final
{
    friend bool operator==(
        const ScheduleTestingClassChoicesReadQuery&,
        const ScheduleTestingClassChoicesReadQuery&
        ) = default;
};

struct ScheduleTestingClassChoicesSnapshot final
{
    std::vector<ScheduleTestingClassChoice> choices;

    friend bool operator==(
        const ScheduleTestingClassChoicesSnapshot&,
        const ScheduleTestingClassChoicesSnapshot&
        ) = default;
};

using ScheduleTestingClassChoicesReadResult =
    Domain::Result<ScheduleTestingClassChoicesSnapshot>;

class ScheduleTestingClassChoicesReadPort
{
public:
    virtual ~ScheduleTestingClassChoicesReadPort() = default;

    [[nodiscard]] virtual ScheduleTestingClassChoicesReadResult
    readTestingClassChoices(
        const ScheduleTestingClassChoicesReadQuery& query
        ) const = 0;
};

class ScheduleTestingClassChoicesReadQueryHandler final
{
public:
    [[nodiscard]] static ScheduleTestingClassChoicesReadResult execute(
        const ScheduleTestingClassChoicesReadQuery& query,
        const ScheduleTestingClassChoicesReadPort& port
        )
    {
        ScheduleTestingClassChoicesReadResult source =
            port.readTestingClassChoices(query);
        if (!source)
        {
            return ScheduleTestingClassChoicesReadResult::failure(
                source.error()
                );
        }

        auto snapshot = std::move(source.value());
        std::unordered_set<std::string> classIds;
        for (const ScheduleTestingClassChoice& choice : snapshot.choices)
        {
            if (!isCanonicalPositiveDecimalId(choice.classId.value()))
            {
                return ScheduleTestingClassChoicesReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message =
                        "Testing class choices contain an invalid class identifier.",
                    .recoverable = false
                });
            }

            if (!classIds.insert(choice.classId.value()).second)
            {
                return ScheduleTestingClassChoicesReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message =
                        "Testing class choices contain a duplicate class identifier.",
                    .recoverable = false
                });
            }
        }

        return ScheduleTestingClassChoicesReadResult::success(
            std::move(snapshot)
            );
    }
};

} // namespace ClassMngr::Next::Application
