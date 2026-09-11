#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

struct TestingClass
{
    int classId = -1;
    std::string name;
    std::string grade;
    std::string level;
    std::string room;
    int teacherId = -1;
    std::string classColor = "#FFFFFF";
    std::string fontColor = "#000000";
    std::string notes;
};

[[nodiscard]] std::vector<std::string> testingClassMixedLevels();
[[nodiscard]] std::vector<std::string> testingClassGrades();
[[nodiscard]] std::vector<std::string> testingClassLevelsForGrade(
    std::string_view grade
    );

} // namespace classmngr::engine
