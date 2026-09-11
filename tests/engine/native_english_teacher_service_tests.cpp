#include "classmngr/engine/native_english_teacher_service.h"
#include "classmngr/engine/open_database.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::NativeEnglishTeacher;
using classmngr::engine::NativeEnglishTeacherService;
using classmngr::engine::OpenDatabase;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineNativeEnglishTeacherServiceTests: "
              << message
              << '\n';
    return false;
}

NativeEnglishTeacher teacher(
    int id,
    std::string name,
    std::string position
    )
{
    NativeEnglishTeacher result;
    result.id = id;
    result.name = std::move(name);
    result.position = std::move(position);
    result.phoneNumber = " 010-1111-2222 ";
    result.birthday = " 05-09 ";
    result.nationality = " Canadian ";
    result.email = " alex@example.com ";
    return result;
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineNativeEnglishTeacherServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    NativeEnglishTeacherService service(database);
    bool passed = true;

    const std::vector<NativeEnglishTeacher> initial{
        teacher(-1, " Alice   Smith ", "Team Leader"),
        teacher(-1, "Bob", "NET"),
        teacher(-1, "김하늘", "Other")
    };
    passed &= expect(
        service.saveDirectory(initial, {}).has_value(),
        "initial Native English Teacher directory save failed"
        );

    const auto listed = service.list();
    passed &= expect(
        listed && listed->size() == 3
            && listed->at(0).name == "Alice Smith"
            && listed->at(0).position == "Team Leader"
            && listed->at(0).phoneNumber == "010-1111-2222"
            && listed->at(1).name == "Bob"
            && listed->at(2).name == "김하늘",
        "directory listing did not preserve normalization and position order"
        );

    if (listed && listed->size() == 3)
    {
        NativeEnglishTeacher updated = listed->at(0);
        updated.name = " Alice   Cooper ";
        updated.position = "Co-ordinator";
        NativeEnglishTeacher added = teacher(-1, "Charlie", "Unknown");
        passed &= expect(
            service.saveDirectory(
                {updated, listed->at(2), added},
                {listed->at(1).id, 0, -4}
                ).has_value(),
            "directory update/delete/add transaction failed"
            );

        const auto afterUpdate = service.list();
        passed &= expect(
            afterUpdate && afterUpdate->size() == 3
                && afterUpdate->at(0).id == updated.id
                && afterUpdate->at(0).name == "Alice Cooper"
                && afterUpdate->at(0).position == "Co-ordinator"
                && afterUpdate->at(1).name == "Charlie"
                && afterUpdate->at(2).name == "김하늘",
            "directory save did not produce the expected ordered result"
            );
    }

    const std::vector<NativeEnglishTeacher> duplicateNames{
        teacher(-1, " Alex ", "NET"),
        teacher(-1, "alex", "NET")
    };
    const auto beforeRejected = service.list();
    const auto rejected = service.saveDirectory(duplicateNames, {});
    passed &= expect(
        !rejected && rejected.error().code == ErrorCode::InvalidFormat,
        "case-insensitive duplicate names were not rejected"
        );
    const auto afterRejected = service.list();
    passed &= expect(
        beforeRejected && afterRejected
            && beforeRejected->size() == afterRejected->size(),
        "rejected directory save was not atomic"
        );

    const NativeEnglishTeacher emptyName = teacher(-1, "   ", "NET");
    const auto emptyRejected = service.saveDirectory({emptyName}, {});
    passed &= expect(
        !emptyRejected && emptyRejected.error().code == ErrorCode::InvalidFormat,
        "empty Native English Teacher names were not rejected"
        );

    return passed ? 0 : 1;
}
