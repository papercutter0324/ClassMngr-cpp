#pragma once

#include "next/domain/operation_result.h"

#include <optional>
#include <string>

namespace ClassMngr::Next::Application
{

using TeacherImportLatestSourceDateReadResult =
    Domain::Result<std::optional<std::string>>;

class TeacherImportLatestSourceDateReadPort
{
public:
    virtual ~TeacherImportLatestSourceDateReadPort() = default;

    // A present value is the canonical ISO date (YYYY-MM-DD) of the latest
    // imported workbook. An absent value means no usable prior date exists.
    [[nodiscard]] virtual TeacherImportLatestSourceDateReadResult
        readLatestTeacherImportSourceDate() const = 0;
};

} // namespace ClassMngr::Next::Application
