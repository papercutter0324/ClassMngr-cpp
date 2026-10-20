#include "next/application/gs_team_import_update.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>

using namespace ClassMngr::Next::Application;

static_assert(std::is_same_v<
    decltype(GsTeamImportProfile{}.id),
    GsTeamImportStoredRecordId>);
static_assert(!std::is_convertible_v<int, GsTeamImportStoredRecordId>);
static_assert(!std::is_convertible_v<std::int64_t, GsTeamImportStoredRecordId>);
static_assert(!std::is_convertible_v<GsTeamImportStoredRecordId, int>);
static_assert(!std::is_convertible_v<GsTeamImportStoredRecordId, std::int64_t>);
static_assert(GsTeamImportStoredRecordId{}.rawValue() == -1);

namespace
{

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

GsTeamImportProfile existingMember()
{
    return {
        .id = GsTeamImportStoredRecordId{9101},
        .name = u"Taylor",
        .koreanName = u"Kim Haneul",
        .position = u"M2",
        .phoneNumber = u"010-1111-2222",
        .birthday = u"05-09"
    };
}

} // namespace

int main()
{
    try
    {
        {
            const GsTeamImportProfile existing = existingMember();
            const auto merged = mergeGsTeamImport(
                existing,
                {
                    .name = u"Taylor Updated",
                    .koreanName = {},
                    .position = u"M3",
                    .phoneNumber = {},
                    .birthday = u"07-08"
                }
                );
            require(merged.changed, "mixed sparse values were not marked changed");
            require(merged.profile.id == existing.id,
                    "matched GS Team identity was not retained");
            require(merged.profile.name == u"Taylor Updated"
                        && merged.profile.position == u"M3"
                        && merged.profile.birthday == u"07-08",
                    "nonblank imported values were not applied");
            require(merged.profile.koreanName == existing.koreanName
                        && merged.profile.phoneNumber == existing.phoneNumber,
                    "blank imported values did not preserve stored fields");
            require(existing.name == u"Taylor" && existing.position == u"M2"
                        && existing.birthday == u"05-09",
                    "merging modified the input profile");
        }

        {
            const GsTeamImportProfile existing = existingMember();
            const auto unchanged = mergeGsTeamImport(
                existing,
                {
                    .name = u"Taylor",
                    .koreanName = {},
                    .position = {},
                    .phoneNumber = {},
                    .birthday = {}
                }
                );
            require(!unchanged.changed,
                    "blank optional values and same name should be unchanged");
            require(unchanged.profile.id == existing.id
                        && unchanged.profile.name == existing.name
                        && unchanged.profile.koreanName == existing.koreanName
                        && unchanged.profile.position == existing.position
                        && unchanged.profile.phoneNumber == existing.phoneNumber
                        && unchanged.profile.birthday == existing.birthday,
                    "no-op did not return a stable copy of the stored profile");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
