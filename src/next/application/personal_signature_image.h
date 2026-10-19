#pragma once

#include "next/domain/operation_result.h"
#include "next/domain/prepared_personal_signature_image_bytes.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries prepared signature-image bytes as an
// opaque Domain value. Base64, image decoding, and image normalization stay in
// the Qt-boundary adapter. Repository failures remain distinguishable from a
// missing or invalid image, which is represented by empty bytes.
using PersonalSignatureImageResult =
    Domain::Result<Domain::PreparedPersonalSignatureImageBytes>;

class PersonalSignatureImagePort
{
public:
    virtual ~PersonalSignatureImagePort() = default;

    [[nodiscard]] virtual PersonalSignatureImageResult read() const = 0;
};

} // namespace ClassMngr::Next::Application
