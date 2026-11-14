#pragma once

#include "next/ui/output_font_family_policy.h"
#include "shared/roster/roster_template_renderer.h"

#include <QFont>

namespace ClassMngr::Next::Ui
{

[[nodiscard]] inline RosterTemplateRenderer::Fonts makeSubPrepRosterOutputFonts(
    const QStringList& registeredFamilies,
    const QString& outputFallbackFamily,
    const int applicationBasePointSize
    )
{
    const OutputFontFamilies families = makeOutputFontFamilies(
        registeredFamilies, outputFallbackFamily);

    QFont english;
    english.setFamilies(families.english);
    english.setPointSize(qMax(1, applicationBasePointSize));
    english.setWeight(QFont::Normal);
    english.setItalic(false);
    english.setHintingPreference(QFont::PreferFullHinting);
    english.setStyleStrategy(QFont::PreferAntialias);

    QFont korean;
    korean.setFamilies(families.korean);
    korean.setPointSize(qMax(1, applicationBasePointSize) + 1);
    korean.setWeight(QFont::Normal);
    korean.setItalic(false);
    korean.setStyleName(QStringLiteral("Regular"));
    const auto weightAxis = QFont::Tag::fromString(QStringLiteral("wght"));
    if (weightAxis)
    {
        korean.setVariableAxis(*weightAxis, static_cast<float>(QFont::Normal));
    }
    korean.setHintingPreference(QFont::PreferFullHinting);
    korean.setStyleStrategy(QFont::PreferAntialias);

    return {std::move(english), std::move(korean)};
}

}  // namespace ClassMngr::Next::Ui
