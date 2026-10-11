#pragma once

#include "next/application/testing_class_create.h"

#include <utility>

namespace ClassMngr::Next::Application
{

class TestingClassCreateUseCase final
{
public:
    template <typename WeekdayCaseInsensitiveEquals>
    [[nodiscard]] static TestingClassCreateResult execute(
        const TestingClassCreateRequest& request,
        const TestingClassCreatePort& port,
        WeekdayCaseInsensitiveEquals&& weekdayCaseInsensitiveEquals
        )
    {
        const Domain::Result<void> validation = request.validate(
            std::forward<WeekdayCaseInsensitiveEquals>(
                weekdayCaseInsensitiveEquals
                )
            );
        if (!validation)
        {
            return TestingClassCreateResult::failure(validation.error());
        }

        return port.createTestingClass(request);
    }
};

} // namespace ClassMngr::Next::Application
