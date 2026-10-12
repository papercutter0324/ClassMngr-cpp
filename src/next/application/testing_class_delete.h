#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

struct TestingClassDeleteRequest final
{
    Domain::ClassId classId;

    [[nodiscard]] Domain::Result<void> validate() const
    {
        if (!isCanonicalPositiveDecimalId(classId.value()))
        {
            return invalidInput(
                "Testing class ID must be a canonical positive integer."
                );
        }

        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] static Domain::Result<void> invalidInput(
        std::string message
        )
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = std::move(message),
            .recoverable = true
        });
    }
};

using TestingClassDeleteResult = Domain::Result<void>;

class TestingClassDeletePort
{
public:
    virtual ~TestingClassDeletePort() = default;

    [[nodiscard]] virtual TestingClassDeleteResult deleteTestingClass(
        const TestingClassDeleteRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
