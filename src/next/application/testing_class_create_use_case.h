#pragma once

#include "next/application/testing_class_create.h"

namespace ClassMngr::Next::Application
{

class TestingClassCreateUseCase final
{
public:
    [[nodiscard]] static TestingClassCreateResult execute(
        const TestingClassCreateRequest& request,
        const TestingClassCreatePort& port
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return TestingClassCreateResult::failure(validation.error());
        }

        return port.createTestingClass(request);
    }
};

} // namespace ClassMngr::Next::Application
