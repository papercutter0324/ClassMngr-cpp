#pragma once

#include <QString>

#include <expected>
#include <memory>

namespace ClassMngr::Core
{

enum class FontResource
{
    Inter,
    Pretendard,
    JustAnotherHand,
    DancingScript,
    GreatVibes,
    Caveat
};

// A registration lease keeps an application font available while its family
// name is in use. Implementations release the registration when the last lease
// owner is destroyed.
class FontRegistrationLease
{
public:
    virtual ~FontRegistrationLease() = default;
    [[nodiscard]] virtual const QString& familyName() const noexcept = 0;
};

class FontRegistrationPort
{
public:
    virtual ~FontRegistrationPort() = default;
    [[nodiscard]] virtual std::expected<
        std::shared_ptr<const FontRegistrationLease>, QString>
    loadFont(FontResource resource) = 0;
};

}  // namespace ClassMngr::Core
