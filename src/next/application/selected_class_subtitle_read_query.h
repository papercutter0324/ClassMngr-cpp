#pragma once

#include "next/application/selected_class_subtitle_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

class SelectedClassSubtitleReadQuery final
{
public:
    explicit SelectedClassSubtitleReadQuery(
        const SelectedClassSubtitleReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] SelectedClassSubtitleReadResult execute(
        const Domain::ClassId& classId
        ) const
    {
        if (!isCanonicalPositiveInteger(classId.value()))
        {
            return SelectedClassSubtitleReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Selected class ID must be a canonical positive integer.",
                .recoverable = false
            });
        }

        auto source = m_port.readSelectedClassSubtitle(classId);
        if (!source)
        {
            return SelectedClassSubtitleReadResult::failure(source.error());
        }

        auto snapshot = std::move(source.value());
        if (snapshot.classId != classId)
        {
            return SelectedClassSubtitleReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A selected class subtitle read returned a different class identifier.",
                .recoverable = false
            });
        }

        return SelectedClassSubtitleReadResult::success(std::move(snapshot));
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveInteger(
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

    const SelectedClassSubtitleReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
