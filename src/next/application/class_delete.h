#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

struct ClassDeleteRequest final
{
    Domain::ClassId classId;

    [[nodiscard]] Domain::Result<void> validate() const
    {
        if (!isCanonicalPositiveDecimalId(classId.value()))
        {
            return invalidInput();
        }

        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] static Domain::Result<void> invalidInput()
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Class ID must be a canonical positive integer.",
            .recoverable = true
        });
    }
};

using ClassDeleteResult = Domain::Result<void>;

class ClassDeletePort
{
public:
    virtual ~ClassDeletePort() = default;

    [[nodiscard]] virtual ClassDeleteResult deleteClass(
        const ClassDeleteRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
