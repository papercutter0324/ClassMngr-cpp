#pragma once

#include <QStringList>

#include <utility>

namespace ClassMngr::Next::Ui
{

struct OutputFontFamilies final
{
    QStringList english;
    QStringList korean;
};

// Keep bundled-font priority shared by output features while letting the
// platform adapter provide the last-resort family.
[[nodiscard]] inline OutputFontFamilies makeOutputFontFamilies(
    const QStringList& registeredFamilies,
    const QString& outputFallbackFamily
    )
{
    const auto familyContaining = [&registeredFamilies](
        const QString& fragment) -> QString {
        for (const QString& family : registeredFamilies)
        {
            if (family.contains(fragment, Qt::CaseInsensitive))
            {
                return family;
            }
        }
        return {};
    };

    QStringList english{
        familyContaining(QStringLiteral("Inter")),
        familyContaining(QStringLiteral("Pretendard")),
        outputFallbackFamily
    };
    english.removeAll(QString{});
    english.removeDuplicates();

    QStringList korean{
        familyContaining(QStringLiteral("Pretendard")),
        familyContaining(QStringLiteral("Inter")),
        outputFallbackFamily
    };
    korean.removeAll(QString{});
    korean.removeDuplicates();

    return {
        .english = std::move(english),
        .korean = std::move(korean)
    };
}

}  // namespace ClassMngr::Next::Ui
