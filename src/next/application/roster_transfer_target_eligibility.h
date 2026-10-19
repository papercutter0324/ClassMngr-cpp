#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/domain/domain_types.h"

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline bool hasValidRosterTransferClassId(
    const Domain::ClassId& classId
    ) noexcept
{
    // ClassId can wrap arbitrary text; enforce canonical positive int syntax.
    const std::string& value = classId.value();
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
        && parsed > 0;
}

[[nodiscard]] inline bool hasValidRosterTransferSourceId(
    const Domain::ClassId& sourceClassId
    ) noexcept
{
    return hasValidRosterTransferClassId(sourceClassId);
}

[[nodiscard]] inline bool isRosterTransferSourceEligible(
    const Domain::ClassId& sourceClassId,
    const std::u16string_view sourceGrade
    )
{
    return hasValidRosterTransferSourceId(sourceClassId)
        && !trimQtWhitespace(sourceGrade).empty();
}

// Call before loading candidate class details so invalid and current IDs do
// not cause a classInfo lookup.
[[nodiscard]] inline bool shouldReadRosterTransferTargetClassInfo(
    const Domain::ClassId& sourceClassId,
    const Domain::ClassId& targetClassId
    ) noexcept
{
    return hasValidRosterTransferSourceId(sourceClassId)
        && hasValidRosterTransferClassId(targetClassId)
        && targetClassId != sourceClassId;
}

[[nodiscard]] inline bool isRosterTransferTargetEligible(
    const Domain::ClassId& sourceClassId,
    const std::u16string_view sourceGrade,
    const Domain::ClassId& targetClassId,
    const std::u16string_view targetGrade
    )
{
    if (!isRosterTransferSourceEligible(sourceClassId, sourceGrade)
        || !shouldReadRosterTransferTargetClassInfo(
            sourceClassId,
            targetClassId
            ))
    {
        return false;
    }

    const std::u16string trimmedSourceGrade = trimQtWhitespace(sourceGrade);
    const std::u16string trimmedTargetGrade = trimQtWhitespace(targetGrade);
    return !trimmedTargetGrade.empty()
        && trimmedSourceGrade == trimmedTargetGrade;
}

} // namespace ClassMngr::Next::Application
