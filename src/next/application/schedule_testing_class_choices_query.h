#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
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
        return port.readTestingClassChoices(query);
    }
};

} // namespace ClassMngr::Next::Application
