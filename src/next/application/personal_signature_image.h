#pragma once

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries prepared signature-image bytes as an
// opaque binary string. Base64, image decoding, and image normalization stay
// in the Qt-boundary adapter.
class PersonalSignatureImagePort
{
public:
    virtual ~PersonalSignatureImagePort() = default;

    [[nodiscard]] virtual std::string read() const = 0;
};

} // namespace ClassMngr::Next::Application
