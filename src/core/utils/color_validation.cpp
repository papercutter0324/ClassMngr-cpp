#include "colorutils.h"

#include <QRegularExpression>

std::optional<QString> ColorUtils::canonicalHexColor(
    const QString& color
    )
{
    static const QRegularExpression hexColorExpression(
        QStringLiteral("^#[0-9A-Fa-f]{6}$")
        );

    const QString normalized = color.trimmed();
    if (!hexColorExpression.match(normalized).hasMatch())
    {
        return std::nullopt;
    }

    return normalized.toUpper();
}
