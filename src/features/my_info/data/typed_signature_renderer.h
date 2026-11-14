#pragma once

#include "core/font_registration_port.h"

#include <QByteArray>
#include <QFont>
#include <QImage>
#include <QSize>
#include <QString>

#include <array>
#include <memory>

enum class TypedSignatureFont
{
    JustAnotherHand = 0,
    DancingScript = 1,
    GreatVibes = 2,
    Caveat = 3
};

class TypedSignatureFontSet final
{
public:
    [[nodiscard]] static TypedSignatureFontSet load(
        ClassMngr::Core::FontRegistrationPort& port);

    [[nodiscard]] QString familyFor(TypedSignatureFont font) const;

private:
    std::array<std::shared_ptr<const ClassMngr::Core::FontRegistrationLease>, 4>
        m_registrations;
};

namespace TypedSignature
{
[[nodiscard]] TypedSignatureFont fontFromStoredValue(int value);

[[nodiscard]] QString displayName(TypedSignatureFont font);

[[nodiscard]] QFont fontFor(
    TypedSignatureFont font,
    int pixelSize,
    const TypedSignatureFontSet* fontSet = nullptr
    );

[[nodiscard]] QImage render(
    const QString& text,
    TypedSignatureFont font,
    const QSize& size,
    const TypedSignatureFontSet* fontSet = nullptr
    );

[[nodiscard]] QByteArray renderForEmbedding(
    const QString& text,
    TypedSignatureFont font,
    const TypedSignatureFontSet* fontSet = nullptr
    );
}
