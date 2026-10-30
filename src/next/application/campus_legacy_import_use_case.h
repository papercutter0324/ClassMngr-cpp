#pragma once

#include "next/application/campus_edit_capability.h"
#include "next/application/campus_legacy_import_port.h"

namespace ClassMngr::Next::Application
{

class CampusLegacyImportUseCase final
{
public:
    [[nodiscard]] static Domain::Result<std::size_t> execute(
        const CampusEditCapability capability,
        const CampusLegacyImportPort& port
        )
    {
        if (!capability.canEdit())
        {
            return Domain::Result<std::size_t>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Campus import is disabled for this session.",
                .recoverable = false
            });
        }

        return port.importLegacyCampuses();
    }
};

} // namespace ClassMngr::Next::Application
