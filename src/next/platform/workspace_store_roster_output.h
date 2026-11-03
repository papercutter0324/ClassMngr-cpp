#pragma once
#include "next/output/roster_output.h"
#include "shared/roster/roster_template_print_truncation.h"
namespace ClassMngr::Next::Platform
{
[[nodiscard]] Output::RosterOutputChoicesResult rosterOutputChoicesAtPath(
    const QString&, const Domain::ClassId&, bool testingClass);
[[nodiscard]] std::expected<RosterTemplatePrintTruncation::Summary, QString>
rosterOutputTruncationAtPath(
    const QString&, const Output::RosterOutputRequest&);
[[nodiscard]] DocumentOutputResult saveRosterOutputPdfAtPath(
    const QString&, const Output::RosterOutputRequest&, const QString&,
    const RosterTemplateRenderer::Fonts& = {}, Output::RosterOutputMetrics* = nullptr);
}
