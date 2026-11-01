#pragma once

#include "next/application/class_teacher_assignments_read_snapshot.h"
#include "next/application/teacher_directory_read_port.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace ClassMngr::Next::Application
{

// Keep the directory's established ordering while projecting only unique
// teachers with a regular-class assignment and a directory entry.
[[nodiscard]] inline std::vector<TeacherDirectoryEntry>
assignedTeacherDirectoryEntries(
    const TeacherDirectoryReadSnapshot& directory,
    const ClassTeacherAssignmentsReadSnapshot& assignments
    )
{
    std::unordered_set<std::string> assignedTeacherIds;
    assignedTeacherIds.reserve(assignments.assignments.size());
    for (const ClassTeacherAssignmentReadRow& assignment : assignments.assignments)
    {
        if (assignment.teacherId)
        {
            assignedTeacherIds.insert(assignment.teacherId->value());
        }
    }

    std::unordered_set<std::string> includedTeacherIds;
    includedTeacherIds.reserve(assignedTeacherIds.size());
    std::vector<TeacherDirectoryEntry> assignedTeachers;
    assignedTeachers.reserve(assignedTeacherIds.size());
    for (const TeacherDirectoryEntry& teacher : directory.teachers)
    {
        if (assignedTeacherIds.contains(teacher.teacherId.value())
            && includedTeacherIds.insert(teacher.teacherId.value()).second)
        {
            assignedTeachers.push_back(teacher);
        }
    }
    return assignedTeachers;
}

} // namespace ClassMngr::Next::Application
