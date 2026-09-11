#pragma once

#include "classmngr/engine/result.h"

#include <string>
#include <string_view>

namespace classmngr::engine
{

class ApplicationSettingsService;

enum class SignatureMode
{
    Image = 0,
    Type = 1
};

struct PersonalDetails
{
    std::string name;
    std::string campus;
    std::string zoomLoginId;
    std::string zoomPassword;
    bool zoomNotAvailable = true;
    std::string signatureImageBase64;
    SignatureMode signatureMode = SignatureMode::Image;
    std::string typedSignatureText;
    int typedSignatureFont = 0;
};

class PersonalDetailsService final
{
public:
    explicit PersonalDetailsService(
        ApplicationSettingsService& settings
        );

    [[nodiscard]] Result<PersonalDetails> load();

    [[nodiscard]] Status save(
        const PersonalDetails& details
        );

    [[nodiscard]] Status saveCampus(
        std::string_view campus
        );

private:
    ApplicationSettingsService& m_settings;
};

} // namespace classmngr::engine
