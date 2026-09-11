#include "classmngr/engine/class_naming.h"

#include <iostream>
#include <string_view>

using namespace classmngr::engine;

namespace
{
bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }
    std::cerr << "ClassMngrEngineClassNamingServiceTests: "
              << message << '\n';
    return false;
}
} // namespace

int main()
{
    ClassInfo info;
    info.classGrade = "E4";
    info.classLevel = "Hercules";
    info.classTimes = {
        {"Monday", "4:00 PM", "4:50 PM"},
        {"Wednesday", "4:00 PM", "4:50 PM"}
    };

    Teacher teacher;
    teacher.id = 7;
    teacher.teacherEn = "Susan";

    bool passed = true;
    passed &= expect(
        ClassNamingService::classDisplayName(info, teacher)
            == "E4 Hercules \xE2\x80\xA2 Susan \xE2\x80\xA2 M/W (4:00)",
        "class display formatting changed"
        );
    passed &= expect(
        ClassNamingService::teacherDisplayName(teacher) == "Susan",
        "teacher display fallback changed"
        );

    Teacher koreanOnly;
    koreanOnly.id = 8;
    koreanOnly.teacherKr = "\xEA\xB9\x80\xEC\x84\xA0\x83\x9D";
    passed &= expect(
        ClassNamingService::teacherDisplayName(koreanOnly)
            == "\xEA\xB9\x80\xEC\x84\xA0\x83\x9D",
        "Korean teacher fallback changed"
        );

    Teacher alice;
    alice.id = 1;
    alice.teacherEn = "Alice";
    passed &= expect(
        ClassNamingService::teacherDisplayLessThan(alice, koreanOnly),
        "teacher ordering changed"
        );
    return passed ? 0 : 1;
}
