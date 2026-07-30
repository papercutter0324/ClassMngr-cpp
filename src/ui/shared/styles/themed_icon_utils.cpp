#include "themed_icon_utils.h"

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QPalette>
#include <QPixmap>

#include <algorithm>

namespace
{
QColor iconColor(
    const QPalette& palette,
    QIcon::Mode mode
    )
{
    const QPalette::ColorGroup group =
        mode == QIcon::Disabled
            ? QPalette::Disabled
            : QPalette::Active;

    return palette.color(
        group,
        QPalette::ButtonText
        );
}

bool isLightNeutralPixel(
    QRgb pixel
    )
{
    const int red = qRed(pixel);
    const int green = qGreen(pixel);
    const int blue = qBlue(pixel);
    const int lowest = std::min({red, green, blue});
    const int highest = std::max({red, green, blue});

    return lowest >= 160
        && highest - lowest <= 24;
}

struct RecoloredPixmap
{
    QPixmap pixmap;
    bool changed = false;
};

RecoloredPixmap recolorPixmap(
    const QPixmap& source,
    const QColor& color,
    ThemedIconUtils::RecolorMode mode
    )
{
    QImage image =
        source.toImage().convertToFormat(
            QImage::Format_ARGB32
            );
    bool changed = false;

    for (int y = 0; y < image.height(); ++y)
    {
        auto* pixels =
            reinterpret_cast<QRgb*>(
                image.scanLine(y)
                );

        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb sourcePixel = pixels[x];

            if (
                qAlpha(sourcePixel) == 0
                || (
                    mode
                        == ThemedIconUtils::RecolorMode::LightNeutralPixels
                    && !isLightNeutralPixel(sourcePixel)
                    )
                )
            {
                continue;
            }

            pixels[x] = qRgba(
                color.red(),
                color.green(),
                color.blue(),
                qAlpha(sourcePixel)
                );
            changed = true;
        }
    }

    QPixmap recolored =
        QPixmap::fromImage(image);
    recolored.setDevicePixelRatio(
        source.devicePixelRatio()
        );

    return {
        .pixmap = recolored,
        .changed = changed
    };
}
}

QIcon ThemedIconUtils::recolor(
    const QIcon& source,
    const QPalette& palette,
    RecolorMode mode
    )
{
    QIcon recolored;
    bool changed = false;

    const QList<QSize> fallbackSizes{
        QSize(16, 16),
        QSize(20, 20),
        QSize(24, 24),
        QSize(32, 32),
        QSize(48, 48),
        QSize(64, 64)
    };
    const QList<QSize> sizes =
        source.availableSizes().isEmpty()
            ? fallbackSizes
            : source.availableSizes();

    constexpr QIcon::Mode modes[]{
        QIcon::Normal,
        QIcon::Disabled,
        QIcon::Active,
        QIcon::Selected
    };

    constexpr QIcon::State states[]{
        QIcon::Off,
        QIcon::On
    };

    for (const QIcon::Mode iconMode : modes)
    {
        for (const QIcon::State state : states)
        {
            for (const QSize& size : sizes)
            {
                const QPixmap sourcePixmap =
                    source.pixmap(
                        size,
                        iconMode,
                        state
                        );

                if (sourcePixmap.isNull())
                {
                    continue;
                }

                const RecoloredPixmap recoloredPixmap =
                    recolorPixmap(
                        sourcePixmap,
                        iconColor(palette, iconMode),
                        mode
                        );

                recolored.addPixmap(
                    recoloredPixmap.pixmap,
                    iconMode,
                    state
                    );
                changed = changed || recoloredPixmap.changed;
            }
        }
    }

    return changed
        ? recolored
        : source;
}
