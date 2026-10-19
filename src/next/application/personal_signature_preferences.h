#pragma once

#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

enum class PersonalSignatureMode
{
    Image,
    Type
};

enum class PersonalSignatureFontChoice
{
    JustAnotherHand = 0,
    DancingScript = 1,
    GreatVibes = 2,
    Caveat = 3
};

struct PersonalSignaturePreferences final
{
    PersonalSignatureMode mode = PersonalSignatureMode::Image;
    std::string typedSignatureText;
    PersonalSignatureFontChoice typedSignatureFont =
        PersonalSignatureFontChoice::JustAnotherHand;

    friend bool operator==(
        const PersonalSignaturePreferences&,
        const PersonalSignaturePreferences&
        ) = default;
};

using PersonalSignaturePreferencesResult =
    Domain::Result<PersonalSignaturePreferences>;

class PersonalSignaturePreferencesPort
{
public:
    virtual ~PersonalSignaturePreferencesPort() = default;

    [[nodiscard]] virtual PersonalSignaturePreferencesResult load()
        const = 0;
};

} // namespace ClassMngr::Next::Application
