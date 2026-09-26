#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/korean_teacher_key.h"

#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// The repository adapter trims imported QString values before creating this
// input. Empty values therefore mean "preserve the stored profile value".
struct KoreanTeacherImportFields final
{
    Domain::KoreanTeacherKey key;
    std::u16string teacherKr;
    std::u16string roomNumber;
    std::u16string birthday;
    std::u16string phoneNumber;
};

// The application owns only the fields this import is allowed to change.
// Other teacher profile fields remain outside this update contract.
struct KoreanTeacherImportProfile final
{
    Domain::TeacherId teacherId;
    Domain::KoreanTeacherKey key;
    std::u16string teacherKr;
    std::u16string roomNumber;
    std::u16string birthday;
    std::u16string phoneNumber;
};

struct KoreanTeacherImportUpdate final
{
    KoreanTeacherImportProfile profile;
    bool changed = false;
};

// Produces an update only when the stored and imported names have the same
// Hangul-only key. The stored identity is retained and the imported name is
// canonicalized to that key, matching the repository's established behavior.
[[nodiscard]] inline std::optional<KoreanTeacherImportUpdate>
mergeMatchedKoreanTeacherImport(
    const KoreanTeacherImportProfile& existing,
    const KoreanTeacherImportFields& imported
    )
{
    const Domain::KoreanTeacherKey existingNameKey =
        Domain::KoreanTeacherKey::fromName(existing.teacherKr);
    const Domain::KoreanTeacherKey importedNameKey =
        Domain::KoreanTeacherKey::fromName(imported.teacherKr);
    if (existing.key.empty() || imported.key.empty()
        || existing.key != imported.key
        || existingNameKey != existing.key
        || importedNameKey != imported.key)
    {
        return std::nullopt;
    }

    KoreanTeacherImportProfile merged{
        .teacherId = existing.teacherId,
        .key = imported.key,
        .teacherKr = imported.key.value(),
        .roomNumber = imported.roomNumber.empty()
            ? existing.roomNumber : imported.roomNumber,
        .birthday = imported.birthday.empty()
            ? existing.birthday : imported.birthday,
        .phoneNumber = imported.phoneNumber.empty()
            ? existing.phoneNumber : imported.phoneNumber
    };
    const bool changed = merged.teacherKr != existing.teacherKr
        || merged.roomNumber != existing.roomNumber
        || merged.birthday != existing.birthday
        || merged.phoneNumber != existing.phoneNumber;

    return KoreanTeacherImportUpdate{
        .profile = std::move(merged),
        .changed = changed
    };
}

} // namespace ClassMngr::Next::Application
