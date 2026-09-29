#pragma once

#include "next/application/testing_class_delete.h"

namespace ClassMngr::Next::Application
{

class TestingClassDeleteUseCase final
{
public:
    [[nodiscard]] static TestingClassDeleteResult execute(
        const TestingClassDeleteRequest& request,
        const TestingClassDeletePort& port
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return TestingClassDeleteResult::failure(validation.error());
        }

        return port.deleteTestingClass(request);
    }
};

} // namespace ClassMngr::Next::Application
