#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterColumnNamesReadSnapshot final
{
    Domain::ClassId classId;
    Domain::Result<std::vector<std::u16string>> columnsResult;

    friend bool operator==(
        const RosterColumnNamesReadSnapshot& left,
        const RosterColumnNamesReadSnapshot& right
        )
    {
        if (left.classId != right.classId
            || left.columnsResult.hasValue() != right.columnsResult.hasValue())
        {
            return false;
        }

        return left.columnsResult.hasValue()
            ? left.columnsResult.value() == right.columnsResult.value()
            : left.columnsResult.error() == right.columnsResult.error();
    }
};

} // namespace ClassMngr::Next::Application
