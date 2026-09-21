#pragma once

#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

// Typed Sub Prep personal-Zoom display settings. The adapter owns primary /
// legacy key selection and best-effort migration; the page owns presentation.
struct SubPrepPersonalZoomPreferences final
{
    std::string loginId = "N/A";
    std::string password = "N/A";
    bool unavailable = true;

    friend bool operator==(
        const SubPrepPersonalZoomPreferences&,
        const SubPrepPersonalZoomPreferences&
        ) = default;
};

using SubPrepPersonalZoomPreferencesResult =
    Domain::Result<SubPrepPersonalZoomPreferences>;

class SubPrepPersonalZoomPreferencesPort
{
public:
    virtual ~SubPrepPersonalZoomPreferencesPort() = default;

    [[nodiscard]] virtual SubPrepPersonalZoomPreferencesResult load()
        const = 0;
};

} // namespace ClassMngr::Next::Application
