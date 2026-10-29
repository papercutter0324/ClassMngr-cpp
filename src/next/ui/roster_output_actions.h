#pragma once
#include "next/output/roster_output.h"
#include <QPageLayout>
class QWidget;
namespace ClassMngr::Next::Ui
{
struct RosterOutputActions final
{
    std::function<Output::RosterOutputChoicesResult(const QString&, const Domain::ClassId&, bool)> readChoices;
    std::function<DocumentOutputResult(const QString&, const Output::RosterOutputRequest&, const QString&,
        const RosterTemplateRenderer::Fonts&)> savePdf;
    // Optional UI seams allow deterministic cancellation/failure checks.
    std::function<QString(QWidget*)> chooseSavePath;
    std::function<DocumentOutputResult(QWidget*, const QString&, QPageLayout::Orientation)> printPdf;
    RosterTemplateRenderer::Fonts fonts;
};
}
