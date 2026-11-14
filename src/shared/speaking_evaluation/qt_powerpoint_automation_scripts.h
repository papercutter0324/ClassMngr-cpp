#pragma once

#include <QString>

namespace QtSpeakingEvalPowerPointScripts
{
inline constexpr auto CommentFontName = "Segoe UI Semibold";

[[nodiscard]] QString macTextArgument(
    const QString& value
    );
[[nodiscard]] QString windowsScript();
[[nodiscard]] QString macScript();
}
