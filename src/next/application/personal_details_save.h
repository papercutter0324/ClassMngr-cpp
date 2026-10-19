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

} // namespace ClassMngr::Next::Application
