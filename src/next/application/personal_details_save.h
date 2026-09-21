#pragma once

#include "next/application/personal_signature_preferences.h"
#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

struct PersonalDetailsSaveRequest final
{
    std::string name;
    std::string campus;
    std::string zoomLoginId;
    std::string zoomPassword;
    bool zoomNotAvailable = true;
    std::string signatureImage;
    PersonalSignatureMode signatureMode = PersonalSignatureMode::Image;
    std::string typedSignatureText;
    int typedSignatureFont = 0;
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
