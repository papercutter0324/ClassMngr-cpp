#include "features/sub_prep/services/sub_prep_package_service.h"
#include "core/fontmanager.h"
#include "core/qt_pdf_generation_adapter.h"
#include <utility>

namespace
{
RosterTemplateRenderer::Fonts legacySubPrepRosterFonts()
{
    QFont ui = FontManager::getUiFont(-1);
    QFont korean = FontManager::getKoreanFont(-1);
    korean.setStyleName(QStringLiteral("Regular"));
    const auto weightAxis = QFont::Tag::fromString(QStringLiteral("wght"));
    if (weightAxis)
    {
        korean.setVariableAxis(*weightAxis, static_cast<float>(QFont::Normal));
    }
    return {std::move(ui), std::move(korean)};
}

}

namespace SubPrepPackageService {
Result generate(Request request)
{
    const auto pdf = makeQtPdfGenerationPort();
    return generate(*pdf, std::move(request), legacySubPrepRosterFonts());
}
Result generateForNext(Request request)
{
    const auto pdf = makeQtPdfGenerationPort();
    return generateForNext(*pdf, std::move(request), legacySubPrepRosterFonts());
}
Result generate(Request request, const ClassMngr::Next::Application::SubPrepGenerationPlan& plan)
{
    const auto pdf = makeQtPdfGenerationPort();
    return generate(*pdf, std::move(request), plan, legacySubPrepRosterFonts());
}
}
