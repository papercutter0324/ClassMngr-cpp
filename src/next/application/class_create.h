#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// Creates the blank regular class used by the Initial Setup Wizard.
struct ClassCreateRequest final
{
};

using ClassCreateResult = Domain::Result<Domain::ClassId>;

class ClassCreatePort
{
public:
    virtual ~ClassCreatePort() = default;

    [[nodiscard]] virtual ClassCreateResult createClass(
        const ClassCreateRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
