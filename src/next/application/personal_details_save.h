#pragma once

#include "next/application/current_campus_preferences.h"
#include "next/application/personal_signature_preferences.h"
#include "next/domain/operation_result.h"
#include "next/domain/personal_signature_image_save_bytes.h"

#include <string>

namespace ClassMngr::Next::Application
{

struct PersonalDetailsSaveRequest final
{
    std::string name;
    CurrentCampusPreferenceValue campus;
    std::string zoomLoginId;
    std::string zoomPassword;
    bool zoomNotAvailable = true;
    Domain::PersonalSignatureImageSaveBytes signatureImage;
    PersonalSignatureMode signatureMode = PersonalSignatureMode::Image;
    std::string typedSignatureText;
    PersonalSignatureFontChoice typedSignatureFont =
        PersonalSignatureFontChoice::JustAnotherHand;
};

using PersonalDetailsSaveResult = Domain::Result<void>;

class PersonalDetailsSavePort
{
public:
    virtual ~PersonalDetailsSavePort() = default;

    [[nodiscard]] virtual PersonalDetailsSaveResult save(
        const PersonalDetailsSaveRequest& request
        ) const = 0;
};

// Initial Setup reads the complete compatibility bundle before presenting its
// editable fields. Keeping that read typed prevents a partial or failed read
// from being mistaken for empty settings and later overwriting saved values.
using PersonalDetailsReadResult =
    Domain::Result<PersonalDetailsSaveRequest>;

class PersonalDetailsReadPort
{
public:
    virtual ~PersonalDetailsReadPort() = default;

    [[nodiscard]] virtual PersonalDetailsReadResult read() const = 0;
};

class PersonalDetailsReadQuery final
{
public:
    explicit PersonalDetailsReadQuery(
        const PersonalDetailsReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] PersonalDetailsReadResult execute() const
    {
        return m_port.read();
    }

private:
    const PersonalDetailsReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
