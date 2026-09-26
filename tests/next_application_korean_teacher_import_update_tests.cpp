#include "next/application/korean_teacher_import_update.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ClassMngr::Next::Application;
using ClassMngr::Next::Domain::KoreanTeacherKey;
using ClassMngr::Next::Domain::TeacherId;

namespace
{

TeacherId teacherId(const std::string& value)
{
    const auto id = TeacherId::fromString(value);
    if (!id)
    {
        throw std::runtime_error("test teacher id must be non-empty");
    }
    return *id;
}

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

KoreanTeacherImportProfile existingTeacher()
{
    return {
        .teacherId = teacherId("42"),
        .key = KoreanTeacherKey::fromName(u"\uD64D\uAE38\uB3D9D"),
        .teacherKr = u"\uD64D\uAE38\uB3D9D",
        .roomNumber = u"Old room",
        .birthday = u"03-07",
        .phoneNumber = u"010-0000-0000"
    };
}

} // namespace

int main()
{
    try
    {
        {
            const auto merged = mergeMatchedKoreanTeacherImport(
                existingTeacher(),
                {
                    .key = KoreanTeacherKey::fromName(u"\uD64D\uAE38\uB3D9 E4"),
                    .teacherKr = u"\uD64D\uAE38\uB3D9 E4",
                    .roomNumber = u"413",
                    .birthday = {},
                    .phoneNumber = u"010-1111-1111"
                }
                );
            require(merged.has_value(), "suffix name did not match by Hangul key");
            require(merged->profile.teacherId.value() == "42",
                    "matched teacher identity was not retained");
            require(merged->profile.teacherKr == u"\uD64D\uAE38\uB3D9",
                    "imported name was not canonicalized to its Hangul key");
            require(merged->profile.key.value() == u"\uD64D\uAE38\uB3D9",
                    "matched KoreanTeacherKey was not retained");
            require(merged->profile.roomNumber == u"413",
                    "non-empty room value was not applied");
            require(merged->profile.birthday == u"03-07",
                    "blank birthday did not preserve the stored value");
            require(merged->profile.phoneNumber == u"010-1111-1111",
                    "non-empty phone value was not applied");
            require(merged->changed,
                    "mixed sparse update was not marked as changed");
        }

        {
            const KoreanTeacherImportProfile existing{
                .teacherId = teacherId("7"),
                .key = KoreanTeacherKey::fromName(u"\uBC15\uBBFC\uC900"),
                .teacherKr = u"\uBC15\uBBFC\uC900",
                .roomNumber = u"510",
                .birthday = u"05-09",
                .phoneNumber = u"010-4444-4444"
            };
            const auto unchanged = mergeMatchedKoreanTeacherImport(
                existing,
                {
                    .key = KoreanTeacherKey::fromName(u"\uBC15\uBBFC\uC900"),
                    .teacherKr = u"\uBC15\uBBFC\uC900",
                    .roomNumber = {},
                    .birthday = {},
                    .phoneNumber = {}
                }
                );
            require(unchanged.has_value(), "matching no-op candidate was rejected");
            require(!unchanged->changed,
                    "all-blank sparse values should produce a no-op");
            require(unchanged->profile.teacherId == existing.teacherId,
                    "no-op did not retain the teacher identity");
            require(unchanged->profile.teacherKr == existing.teacherKr
                        && unchanged->profile.roomNumber == existing.roomNumber
                        && unchanged->profile.birthday == existing.birthday
                        && unchanged->profile.phoneNumber == existing.phoneNumber,
                    "all-blank sparse values changed the profile");
        }

        {
            const auto mismatch = mergeMatchedKoreanTeacherImport(
                existingTeacher(),
                {
                    .key = KoreanTeacherKey::fromName(u"\uAE40\uD558\uB298"),
                    .teacherKr = u"\uAE40\uD558\uB298",
                    .roomNumber = u"413",
                    .birthday = {},
                    .phoneNumber = {}
                }
                );
            require(!mismatch,
                    "different Hangul teacher keys should not produce an update");
        }

        {
            const auto inconsistentKey = mergeMatchedKoreanTeacherImport(
                existingTeacher(),
                {
                    .key = KoreanTeacherKey::fromName(u"\uD64D\uAE38\uB3D9"),
                    .teacherKr = u"\uAE40\uD558\uB298",
                    .roomNumber = {},
                    .birthday = {},
                    .phoneNumber = {}
                }
                );
            require(!inconsistentKey,
                    "an explicit key inconsistent with the imported name was accepted");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
