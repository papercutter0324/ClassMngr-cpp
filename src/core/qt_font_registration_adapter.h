#pragma once

#include "core/font_registration_port.h"

namespace ClassMngr::Core
{

// Legacy composition adapter. Font selection and ownership stay behind
// FontRegistrationPort; shared feature code never calls QFontDatabase.
class QtFontRegistrationAdapter final : public FontRegistrationPort
{
public:
    [[nodiscard]] std::expected<
        std::shared_ptr<const FontRegistrationLease>, QString>
    loadFont(FontResource resource) override;
};

}  // namespace ClassMngr::Core
