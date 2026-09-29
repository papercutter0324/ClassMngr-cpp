#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct TestingTeacherChoice final
{
    Domain::TeacherId teacherId;
    std::u16string name;
    std::u16string room;

    friend bool operator==(
        const TestingTeacherChoice&,
        const TestingTeacherChoice&
        ) = default;
};

struct TestingTeacherChoicesReadQuery final
{
    friend bool operator==(
        const TestingTeacherChoicesReadQuery&,
        const TestingTeacherChoicesReadQuery&
        ) = default;
};

struct TestingTeacherChoicesSnapshot final
{
    std::vector<TestingTeacherChoice> choices;

    friend bool operator==(
        const TestingTeacherChoicesSnapshot&,
        const TestingTeacherChoicesSnapshot&
        ) = default;
};

using TestingTeacherChoicesReadResult =
    Domain::Result<TestingTeacherChoicesSnapshot>;

class TestingTeacherChoicesReadPort
{
public:
    virtual ~TestingTeacherChoicesReadPort() = default;

    [[nodiscard]] virtual TestingTeacherChoicesReadResult
    readTestingTeacherChoices(
        const TestingTeacherChoicesReadQuery& query
        ) const = 0;
};

class TestingTeacherChoicesReadQueryHandler final
{
public:
    [[nodiscard]] static TestingTeacherChoicesReadResult execute(
        const TestingTeacherChoicesReadQuery& query,
        const TestingTeacherChoicesReadPort& port
        )
    {
        return port.readTestingTeacherChoices(query);
    }
};

} // namespace ClassMngr::Next::Application
