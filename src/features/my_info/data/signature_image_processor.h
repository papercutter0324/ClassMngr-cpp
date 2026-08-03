#pragma once

#include <QByteArray>
#include <QImage>

namespace SignatureImage
{
[[nodiscard]] QImage removeWhiteBackground(
    const QImage& source
    );

[[nodiscard]] QByteArray prepareForEmbedding(
    const QImage& source
    );

[[nodiscard]] QByteArray prepareForEmbedding(
    const QByteArray& encodedSource
    );
}
