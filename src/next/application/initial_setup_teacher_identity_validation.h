#pragma once

namespace ClassMngr::Next::Application
{

// The feature boundary trims the three identity fields and passes whether
// each contains a value. The setup wizard requires any two of the three.
class InitialSetupTeacherIdentityRequirements final
{
public:
    [[nodiscard]] constexpr bool complete() const noexcept
    {
        return fieldsPresent() >= 2U;
    }

    // Number of additional fields needed to meet the two-field threshold.
    [[nodiscard]] constexpr unsigned int additionalFieldsRequired() const noexcept
    {
        return complete() ? 0U : 2U - fieldsPresent();
    }

    // These flags report every missing field for the prompt's ordered list.
    [[nodiscard]] constexpr bool englishNameMissing() const noexcept
    {
        return !m_englishNamePresent;
    }

    [[nodiscard]] constexpr bool koreanNameMissing() const noexcept
    {
        return !m_koreanNamePresent;
    }

    [[nodiscard]] constexpr bool preferredSpellingMissing() const noexcept
    {
        return !m_preferredSpellingPresent;
    }

private:
    constexpr InitialSetupTeacherIdentityRequirements(
        bool englishNamePresent,
        bool koreanNamePresent,
        bool preferredSpellingPresent
        ) noexcept
        : m_englishNamePresent(englishNamePresent),
          m_koreanNamePresent(koreanNamePresent),
          m_preferredSpellingPresent(preferredSpellingPresent)
    {
    }

    [[nodiscard]] constexpr unsigned int fieldsPresent() const noexcept
    {
        return static_cast<unsigned int>(m_englishNamePresent)
            + static_cast<unsigned int>(m_koreanNamePresent)
            + static_cast<unsigned int>(m_preferredSpellingPresent);
    }

    bool m_englishNamePresent;
    bool m_koreanNamePresent;
    bool m_preferredSpellingPresent;

    friend constexpr InitialSetupTeacherIdentityRequirements
    initialSetupTeacherIdentityRequirements(
        bool englishNamePresent,
        bool koreanNamePresent,
        bool preferredSpellingPresent
        ) noexcept;
};

[[nodiscard]] constexpr InitialSetupTeacherIdentityRequirements
initialSetupTeacherIdentityRequirements(
    bool englishNamePresent,
    bool koreanNamePresent,
    bool preferredSpellingPresent
    ) noexcept
{
    return InitialSetupTeacherIdentityRequirements(
        englishNamePresent,
        koreanNamePresent,
        preferredSpellingPresent
        );
}

} // namespace ClassMngr::Next::Application
