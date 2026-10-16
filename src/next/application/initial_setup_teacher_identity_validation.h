#pragma once

namespace ClassMngr::Next::Application
{

// The feature boundary trims the three identity fields and passes whether
// each contains a value. The setup wizard requires any two of the three.
struct InitialSetupTeacherIdentityRequirements final
{
    bool complete{false};
    // Number of additional fields needed to meet the two-field threshold.
    unsigned int additionalFieldsRequired{};
    // These flags report every missing field for the prompt's ordered list.
    bool englishNameMissing{false};
    bool koreanNameMissing{false};
    bool preferredSpellingMissing{false};
};

[[nodiscard]] constexpr InitialSetupTeacherIdentityRequirements
initialSetupTeacherIdentityRequirements(
    bool englishNamePresent,
    bool koreanNamePresent,
    bool preferredSpellingPresent
    ) noexcept
{
    const unsigned int fieldsPresent =
        static_cast<unsigned int>(englishNamePresent)
        + static_cast<unsigned int>(koreanNamePresent)
        + static_cast<unsigned int>(preferredSpellingPresent);
    const bool complete = fieldsPresent >= 2;

    return {
        .complete = complete,
        .additionalFieldsRequired = complete ? 0U : 2U - fieldsPresent,
        .englishNameMissing = !englishNamePresent,
        .koreanNameMissing = !koreanNamePresent,
        .preferredSpellingMissing = !preferredSpellingPresent
    };
}

} // namespace ClassMngr::Next::Application
