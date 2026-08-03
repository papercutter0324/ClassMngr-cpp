#include "signature_image_processor.h"

#include <QBuffer>

#include <algorithm>

namespace
{
int recoveredChannel(
    int channel,
    int whiteComponent,
    int opacity
    )
{
    return std::clamp(
        (
            ((channel - whiteComponent) * 255)
            + (opacity / 2)
            )
            / opacity,
        0,
        255
        );
}
}

namespace SignatureImage
{
QImage removeWhiteBackground(
    const QImage& source
    )
{
    if (source.isNull())
    {
        return {};
    }

    QImage result =
        source.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < result.height(); ++y)
    {
        auto* line =
            reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < result.width(); ++x)
        {
            const QRgb pixel = line[x];
            const int whiteComponent =
                std::min({
                    qRed(pixel),
                    qGreen(pixel),
                    qBlue(pixel)
                    });
            const int backgroundRemovalOpacity =
                255 - whiteComponent;
            const int opacity =
                (
                    (qAlpha(pixel) * backgroundRemovalOpacity)
                    + 127
                    )
                    / 255;

            if (opacity == 0)
            {
                line[x] = qRgba(0, 0, 0, 0);
                continue;
            }

            line[x] =
                qRgba(
                    recoveredChannel(
                        qRed(pixel),
                        whiteComponent,
                        backgroundRemovalOpacity
                        ),
                    recoveredChannel(
                        qGreen(pixel),
                        whiteComponent,
                        backgroundRemovalOpacity
                        ),
                    recoveredChannel(
                        qBlue(pixel),
                        whiteComponent,
                        backgroundRemovalOpacity
                        ),
                    opacity
                    );
        }
    }

    return result;
}

QByteArray prepareForEmbedding(
    const QImage& source
    )
{
    const QImage prepared =
        removeWhiteBackground(source);
    if (prepared.isNull())
    {
        return {};
    }

    QByteArray encodedImage;
    QBuffer buffer(&encodedImage);
    if (!buffer.open(QIODevice::WriteOnly)
        || !prepared.save(&buffer, "PNG"))
    {
        return {};
    }

    return encodedImage;
}

QByteArray prepareForEmbedding(
    const QByteArray& encodedSource
    )
{
    if (encodedSource.isEmpty())
    {
        return {};
    }

    QImage source;
    if (!source.loadFromData(encodedSource))
    {
        return {};
    }

    return prepareForEmbedding(source);
}
}
