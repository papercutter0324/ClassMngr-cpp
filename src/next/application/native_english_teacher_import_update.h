#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

class NativeEnglishTeacherImportStoredRecordId final
{
public:
    constexpr NativeEnglishTeacherImportStoredRecordId() noexcept = default;

    constexpr explicit NativeEnglishTeacherImportStoredRecordId(
        const std::int64_t rawValue
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr std::int64_t rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const NativeEnglishTeacherImportStoredRecordId&,
        const NativeEnglishTeacherImportStoredRecordId&
        ) noexcept = default;

private:
    std::int64_t m_rawValue = -1;
};

// The repository adapter supplies a simplified name and trimmed profile
// values. Empty optional fields mean "preserve the stored profile value".
struct NativeEnglishTeacherImportFields final
{
    std::u16string name;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string birthday;
    std::u16string nationality;
    std::u16string email;
};

// Native English teachers use their own 64-bit database identity; it is not a
// Domain::TeacherId because this table is separate from Korean teachers.
struct NativeEnglishTeacherImportProfile final
{
    NativeEnglishTeacherImportStoredRecordId id;
    std::u16string name;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string birthday;
    std::u16string nationality;
    std::u16string email;
};

struct NativeEnglishTeacherImportUpdate final
{
    NativeEnglishTeacherImportProfile profile;
    bool changed = false;
};

// Merges the import-owned fields while retaining the stored identity and
// preserving optional values that the source leaves blank.
[[nodiscard]] inline NativeEnglishTeacherImportUpdate
mergeNativeEnglishTeacherImport(
    const NativeEnglishTeacherImportProfile& existing,
    const NativeEnglishTeacherImportFields& imported
    )
{
    NativeEnglishTeacherImportProfile merged{
        .id = existing.id,
        .name = imported.name,
        .position = imported.position.empty()
            ? existing.position : imported.position,
        .phoneNumber = imported.phoneNumber.empty()
            ? existing.phoneNumber : imported.phoneNumber,
        .birthday = imported.birthday.empty()
            ? existing.birthday : imported.birthday,
        .nationality = imported.nationality.empty()
            ? existing.nationality : imported.nationality,
        .email = imported.email.empty()
            ? existing.email : imported.email
    };

    const bool changed = merged.name != existing.name
        || merged.position != existing.position
        || merged.phoneNumber != existing.phoneNumber
        || merged.birthday != existing.birthday
        || merged.nationality != existing.nationality
        || merged.email != existing.email;

    return NativeEnglishTeacherImportUpdate{
        .profile = std::move(merged),
        .changed = changed
    };
}

} // namespace ClassMngr::Next::Application
