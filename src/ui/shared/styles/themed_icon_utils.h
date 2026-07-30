#pragma once

class QIcon;
class QPalette;

namespace ThemedIconUtils
{
enum class RecolorMode
{
    AllPixels,
    LightNeutralPixels
};

QIcon recolor(
    const QIcon& source,
    const QPalette& palette,
    RecolorMode mode = RecolorMode::AllPixels
    );
}
