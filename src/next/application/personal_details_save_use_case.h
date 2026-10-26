#pragma once

#include "next/application/personal_details_save.h"

namespace ClassMngr::Next::Application
{

// Saves the complete Personal Details compatibility bundle atomically
// through its typed port. Presentation trims the required name using Qt's
// Unicode-aware rules; this boundary also rejects an empty value if called
// without the setup page.
class PersonalDetailsSaveUseCase final
{
public:
    explicit PersonalDetailsSaveUseCase(
        const PersonalDetailsSavePort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] PersonalDetailsSaveResult execute(
        const PersonalDetailsSaveRequest& request
        ) const
    {
        if (request.name.empty())
        {
            return PersonalDetailsSaveResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Your name is required.",
                .recoverable = true
            });
        }

        return m_port.save(request);
    }

private:
    const PersonalDetailsSavePort& m_port;
};

} // namespace ClassMngr::Next::Application
