#pragma once
#include "next/output/roster_output.h"
#include "shared/roster/roster_template_print_truncation.h"
#include <QPageLayout>
class QWidget;
namespace ClassMngr::Next::Ui
{
struct RosterOutputActions final
{
    std::function<Output::RosterOutputChoicesResult(const QString&, const Domain::ClassId&, bool)> readChoices;
    std::function<std::expected<RosterTemplatePrintTruncation::Summary, QString>(
        const QString&, const Output::RosterOutputRequest&)> inspectTruncation;
    std::function<DocumentOutputResult(const QString&, const Output::RosterOutputRequest&, const QString&,
        const RosterTemplateRenderer::Fonts&)> savePdf;
    // Optional UI seams allow deterministic cancellation/failure checks.
    std::function<QString(QWidget*)> chooseSavePath;
    std::function<DocumentOutputResult(QWidget*, const QString&, QPageLayout::Orientation)> printPdf;
    RosterTemplateRenderer::Fonts fonts;
};
}
