#pragma once

#include "next/application/evaluation_default_selection.h"

#include <functional>
#include <string_view>

namespace ClassMngr::Next::Application
{

// The feature supplies equality so this Qt-free policy keeps the caller's
// comparison semantics. Names are compared exactly; this policy does not trim.
template <typename Name, typename CaseInsensitiveEquals>
[[nodiscard]] inline bool isRosterEvaluationColumnName(
    const Name& name,
    CaseInsensitiveEquals&& caseInsensitiveEquals
    )
{
    for (const std::u16string_view evaluationName : kStoredEvaluationNames)
    {
        if (std::invoke(caseInsensitiveEquals, name, evaluationName))
        {
            return true;
        }
    }

    return std::invoke(
        caseInsensitiveEquals,
        name,
        std::u16string_view(u"Autumn")
        );
}

} // namespace ClassMngr::Next::Application
