#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// The repository adapter supplies simplified names and trimmed profile
// values. Empty values preserve the stored field because this import cannot
// clear an existing GS Team value.
struct GsTeamImportFields final
{
    std::u16string name;
    std::u16string koreanName;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string birthday;
};

// Only the GS Team fields owned by this import belong in this profile. The
// database identity is retained by the merge and stays outside the import.
struct GsTeamImportProfile final
{
    int id = -1;
    std::u16string name;
    std::u16string koreanName;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string birthday;
};

struct GsTeamImportUpdate final
{
    GsTeamImportProfile profile;
    bool changed = false;
};

// Merges normalized import-owned fields while retaining the matched database
// identity and preserving fields whose imported values are blank.
[[nodiscard]] inline GsTeamImportUpdate
mergeGsTeamImport(
    const GsTeamImportProfile& existing,
    const GsTeamImportFields& imported
    )
{
    GsTeamImportProfile merged{
        .id = existing.id,
        .name = imported.name.empty() ? existing.name : imported.name,
        .koreanName = imported.koreanName.empty()
            ? existing.koreanName : imported.koreanName,
        .position = imported.position.empty()
            ? existing.position : imported.position,
        .phoneNumber = imported.phoneNumber.empty()
            ? existing.phoneNumber : imported.phoneNumber,
        .birthday = imported.birthday.empty()
            ? existing.birthday : imported.birthday
    };

    const bool changed = merged.name != existing.name
        || merged.koreanName != existing.koreanName
        || merged.position != existing.position
        || merged.phoneNumber != existing.phoneNumber
        || merged.birthday != existing.birthday;

    return GsTeamImportUpdate{
        .profile = std::move(merged),
        .changed = changed
    };
}

} // namespace ClassMngr::Next::Application
