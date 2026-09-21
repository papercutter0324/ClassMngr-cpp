#pragma once

#include "next/domain/operation_result.h"

#include <optional>
#include <string>

namespace ClassMngr::Next::Application
{

// Typed saved-content settings for the Sub Prep page. Missing grading and
// special-instruction values remain distinguishable from present empty text;
// the page owns the existing default rules for those cases.
struct SubPrepPreferences final
{
    std::string classMaterials;
    std::optional<std::string> bookReportGrading;
    std::optional<std::string> bookReportSpecialInstructions;
    std::string subComments;

    friend bool operator==(
        const SubPrepPreferences&,
        const SubPrepPreferences&
        ) = default;
};

using SubPrepPreferencesResult =
    Domain::Result<SubPrepPreferences>;
using SubPrepPreferencesSaveResult = Domain::Result<void>;

class SubPrepPreferencesPort
{
public:
    virtual ~SubPrepPreferencesPort() = default;

    [[nodiscard]] virtual SubPrepPreferencesResult load() const = 0;

    [[nodiscard]] virtual SubPrepPreferencesSaveResult save(
        const SubPrepPreferences& preferences
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
