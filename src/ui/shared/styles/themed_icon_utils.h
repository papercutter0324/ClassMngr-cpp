#pragma once

class QIcon;
class QPalette;

namespace ThemedIconUtils
{
enum class RecolorMode
{
    AllPixels,
    LightNeutralPixels,
    DarkGlyphOnLightBackground
};

QIcon recolor(
    const QIcon& source,
    const QPalette& palette,
    RecolorMode mode = RecolorMode::AllPixels
    );
}
