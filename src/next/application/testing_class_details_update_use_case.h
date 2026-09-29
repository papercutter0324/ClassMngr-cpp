#pragma once

#include "next/application/testing_class_details_update.h"

namespace ClassMngr::Next::Application
{

class TestingClassDetailsUpdateUseCase final
{
public:
    [[nodiscard]] static TestingClassDetailsUpdateResult execute(
        const TestingClassDetailsUpdateRequest& request,
        const TestingClassDetailsUpdatePort& port
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return TestingClassDetailsUpdateResult::failure(
                validation.error()
                );
        }

        return port.updateTestingClassDetails(request);
    }
};

} // namespace ClassMngr::Next::Application
