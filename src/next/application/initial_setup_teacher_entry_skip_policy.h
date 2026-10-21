#pragma once

namespace ClassMngr::Next::Application
{

[[nodiscard]] constexpr bool initialSetupTeacherEntryShouldBeSkipped(
    bool allTeacherIdentityAndRoomFieldsEmpty,
    bool availableTeacherExists
    ) noexcept
{
    return allTeacherIdentityAndRoomFieldsEmpty && availableTeacherExists;
}

} // namespace ClassMngr::Next::Application
