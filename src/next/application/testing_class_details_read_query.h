#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>

namespace ClassMngr::Next::Application
{

struct TestingClassDetailsReadQuery final
{
    Domain::ClassId classId;

    friend bool operator==(
        const TestingClassDetailsReadQuery&,
        const TestingClassDetailsReadQuery&
        ) = default;
};

struct TestingClassDetailsSnapshot final
{
    Domain::ClassId classId;
    std::u16string name;
    std::u16string grade;
    std::u16string level;
    std::u16string room;
    std::optional<Domain::TeacherId> teacherId;
    std::u16string classColor;
    std::u16string fontColor;
    std::u16string notes;

    friend bool operator==(
        const TestingClassDetailsSnapshot&,
        const TestingClassDetailsSnapshot&
        ) = default;
};

using TestingClassDetailsReadResult =
    Domain::Result<TestingClassDetailsSnapshot>;

class TestingClassDetailsReadPort
{
public:
    virtual ~TestingClassDetailsReadPort() = default;

    [[nodiscard]] virtual TestingClassDetailsReadResult
    readTestingClassDetails(
        const TestingClassDetailsReadQuery& query
        ) const = 0;
};

class TestingClassDetailsReadQueryHandler final
{
public:
    [[nodiscard]] static TestingClassDetailsReadResult execute(
        const TestingClassDetailsReadQuery& query,
        const TestingClassDetailsReadPort& port
        )
    {
        auto loaded = port.readTestingClassDetails(query);
        if (!loaded)
        {
            return TestingClassDetailsReadResult::failure(loaded.error());
        }
        if (loaded.value().classId != query.classId)
        {
            return TestingClassDetailsReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message =
                    "Testing class details returned a different class identifier.",
                .recoverable = false
            });
        }
        return loaded;
    }
};

} // namespace ClassMngr::Next::Application
