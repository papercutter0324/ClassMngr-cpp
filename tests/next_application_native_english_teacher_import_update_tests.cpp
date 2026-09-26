#include "next/application/native_english_teacher_import_update.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace ClassMngr::Next::Application;

namespace
{

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

NativeEnglishTeacherImportProfile existingTeacher()
{
    return {
        .id = 8104,
        .name = u"Alex",
        .position = u"NET",
        .phoneNumber = u"010-9999-8888",
        .birthday = u"02-01",
        .nationality = u"Canadian",
        .email = u"alex@example.com"
    };
}

} // namespace

int main()
{
    try
    {
        {
            const NativeEnglishTeacherImportProfile existing = existingTeacher();
            const auto merged = mergeNativeEnglishTeacherImport(
                existing,
                {
                    .name = u"Alex",
                    .position = u"Team Leader",
                    .phoneNumber = {},
                    .birthday = u"03-07",
                    .nationality = {},
                    .email = {}
                }
                );
            require(merged.changed, "nonblank imported values were not marked changed");
            require(merged.profile.id == existing.id,
                    "matched native teacher identity was not retained");
            require(merged.profile.name == u"Alex",
                    "the adapter-normalized name was not applied");
            require(merged.profile.position == u"Team Leader",
                    "nonblank position was not applied");
            require(merged.profile.birthday == u"03-07",
                    "nonblank birthday was not applied");
            require(merged.profile.phoneNumber == existing.phoneNumber
                        && merged.profile.nationality == existing.nationality
                        && merged.profile.email == existing.email,
                    "blank imported values did not preserve stored values");
            require(existing.position == u"NET" && existing.birthday == u"02-01",
                    "merging modified the input profile");
        }

        {
            const NativeEnglishTeacherImportProfile existing = existingTeacher();
            const auto unchanged = mergeNativeEnglishTeacherImport(
                existing,
                {
                    .name = u"Alex",
                    .position = {},
                    .phoneNumber = {},
                    .birthday = {},
                    .nationality = {},
                    .email = {}
                }
                );
            require(!unchanged.changed,
                    "blank optional values with an unchanged name should be a no-op");
            require(unchanged.profile.id == existing.id
                        && unchanged.profile.name == existing.name
                        && unchanged.profile.position == existing.position
                        && unchanged.profile.phoneNumber == existing.phoneNumber
                        && unchanged.profile.birthday == existing.birthday
                        && unchanged.profile.nationality == existing.nationality
                        && unchanged.profile.email == existing.email,
                    "no-op did not return a stable copy of the stored profile");
        }

        {
            const auto renamed = mergeNativeEnglishTeacherImport(
                existingTeacher(),
                {
                    .name = u"Alexandra",
                    .position = {},
                    .phoneNumber = {},
                    .birthday = {},
                    .nationality = {},
                    .email = {}
                }
                );
            require(renamed.changed, "a changed normalized name was ignored");
            require(renamed.profile.name == u"Alexandra",
                    "the simplified adapter name was not retained in the result");
            require(renamed.profile.id == 8104,
                    "renaming changed the matched database identity");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
