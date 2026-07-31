#pragma once

#include <QString>

enum class TestingAssignmentKind
{
    PlainTesting,
    SpecialClass
};

struct TestingAssignment
{
    QString day;
    QString startTime;
    TestingAssignmentKind kind =
        TestingAssignmentKind::PlainTesting;
    QString room;
    int classId{-1};
};

struct TestingBlock
{
    QString day;
    QString startTime;
    QString room;
};
