#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/domain/operation_result.h"

#include <string_view>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline Domain::Result<void> validateTestingClassRequiredFields(
    std::u16string_view name,
    std::u16string_view grade,
    std::u16string_view level,
    std::u16string_view room
    )
{
    if (trimQtWhitespace(name).empty())
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Validation,
            .message = "Testing class name is required.",
            .recoverable = true
        });
    }

    if (trimQtWhitespace(grade).empty())
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Validation,
            .message = "Testing class grade is required.",
            .recoverable = true
        });
    }

    if (trimQtWhitespace(level).empty())
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Validation,
            .message = "Testing class level is required.",
            .recoverable = true
        });
    }

    if (trimQtWhitespace(room).empty())
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Validation,
            .message = "Testing class room is required.",
            .recoverable = true
        });
    }

    return Domain::Result<void>::success();
}

} // namespace ClassMngr::Next::Application
