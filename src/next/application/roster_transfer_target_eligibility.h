#pragma once

#include "next/application/qt_compatible_text.h"

#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline bool hasValidRosterTransferSourceId(
    const int sourceClassId
    ) noexcept
{
    return sourceClassId > 0;
}

[[nodiscard]] inline bool isRosterTransferSourceEligible(
    const int sourceClassId,
    const std::u16string_view sourceGrade
    )
{
    return hasValidRosterTransferSourceId(sourceClassId)
        && !trimQtWhitespace(sourceGrade).empty();
}

// Call before loading candidate class details so invalid and current IDs do
// not cause a classInfo lookup.
[[nodiscard]] inline bool shouldReadRosterTransferTargetClassInfo(
    const int sourceClassId,
    const int targetClassId
    ) noexcept
{
    return hasValidRosterTransferSourceId(sourceClassId)
        && targetClassId > 0
        && targetClassId != sourceClassId;
}

[[nodiscard]] inline bool isRosterTransferTargetEligible(
    const int sourceClassId,
    const std::u16string_view sourceGrade,
    const int targetClassId,
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
