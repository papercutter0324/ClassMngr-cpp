#pragma once
#include "next/output/roster_output.h"
namespace ClassMngr::Next::Platform
{
[[nodiscard]] Output::RosterOutputChoicesResult rosterOutputChoicesAtPath(
    const QString&, const Domain::ClassId&, bool testingClass);
[[nodiscard]] DocumentOutputResult saveRosterOutputPdfAtPath(
    const QString&, const Output::RosterOutputRequest&, const QString&,
    const RosterTemplateRenderer::Fonts& = {}, Output::RosterOutputMetrics* = nullptr);
}
