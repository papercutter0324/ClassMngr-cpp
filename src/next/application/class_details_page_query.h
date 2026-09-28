#pragma once

#include "next/application/class_details_page_read_port.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kClassDetailsPageMaxIdentifierLength = 256;

using ClassDetailsPageQueryResult = ClassDetailsPageReadResult;

// This query validates only the typed request and snapshot identity. Source
// outcomes remain independent so a partial read is still useful to the page.
class ClassDetailsPageQuery final
{
public:
    explicit ClassDetailsPageQuery(
        ClassDetailsPageReadPort& readPort
        ) noexcept
        : m_readPort(readPort)
    {
    }

    [[nodiscard]] ClassDetailsPageQueryResult execute(
        const Domain::ClassId& classId
        ) const
    {
        if (!isValidClassId(classId))
        {
            return ClassDetailsPageQueryResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Selected class identifier must be non-blank and bounded.",
                .recoverable = false
            });
        }

        auto source = m_readPort.readClassDetailsPage(classId);
        if (!source)
        {
            return ClassDetailsPageQueryResult::failure(source.error());
        }

        auto snapshot = std::move(source.value());
        if (snapshot.classId != classId)
        {
            return ClassDetailsPageQueryResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A class details page read returned a different class identifier.",
                .recoverable = false
            });
        }

        return ClassDetailsPageQueryResult::success(std::move(snapshot));
    }

private:
    [[nodiscard]] static bool isValidClassId(
        const Domain::ClassId& classId
        )
    {
        const std::string& value = classId.value();
        if (value.empty() || value.size() > kClassDetailsPageMaxIdentifierLength)
        {
            return false;
        }

        for (const unsigned char character : value)
        {
            if (std::isspace(character) != 0)
            {
                return false;
            }
        }
        return true;
    }

    ClassDetailsPageReadPort& m_readPort;
};

} // namespace ClassMngr::Next::Application
