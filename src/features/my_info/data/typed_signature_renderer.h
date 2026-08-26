#pragma once

#include <QByteArray>
#include <QFont>
#include <QImage>
#include <QSize>
#include <QString>

enum class TypedSignatureFont
{
    JustAnotherHand = 0,
    DancingScript = 1,
    GreatVibes = 2,
    Caveat = 3
};

namespace TypedSignature
{
[[nodiscard]] TypedSignatureFont fontFromStoredValue(int value);

[[nodiscard]] QString displayName(TypedSignatureFont font);

[[nodiscard]] QFont fontFor(
    TypedSignatureFont font,
    int pixelSize
    );

[[nodiscard]] QImage render(
    const QString& text,
    TypedSignatureFont font,
    const QSize& size
    );

[[nodiscard]] QByteArray renderForEmbedding(
    const QString& text,
    TypedSignatureFont font
    );
}
