#pragma once

#include <string>

namespace classmngr::engine
{

enum class TestingAssignmentKind
{
    PlainTesting,
    SpecialClass
};

struct TestingAssignment
{
    std::string day;
    std::string startTime;
    TestingAssignmentKind kind = TestingAssignmentKind::PlainTesting;
    std::string room;
    int classId = -1;
};

struct TestingBlock
{
    std::string day;
    std::string startTime;
    std::string room;
};

} // namespace classmngr::engine
