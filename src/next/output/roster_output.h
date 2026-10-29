#pragma once

#include "next/domain/domain_types.h"
#include "shared/roster/roster_template_renderer.h"

#include <functional>
#include <vector>

namespace ClassMngr::Next::Output
{
enum class RosterOutputScope { AllClasses, CurrentClass, SelectedClasses };
struct RosterOutputRequest final
{
    Domain::ClassId currentClassId;
    bool testingClass = false;
    RosterOutputScope scope = RosterOutputScope::AllClasses;
    std::vector<Domain::ClassId> selectedClassIds;
    RosterTemplateRenderer::TemplateId templateId = RosterTemplateRenderer::TemplateId::ByDay;
    QStringList extraColumns;
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
};
struct RosterOutputClass final { Domain::ClassId classId; QString name; QStringList extraColumns; };
struct RosterOutputChoices final
{
    QList<RosterOutputClass> classes;
    QStringList extraColumns;
};
using RosterOutputChoicesResult = std::expected<RosterOutputChoices, QString>;
// Text is confined to the selected page payload. Diagnostics carry counts only.
struct RosterOutputMetrics final
{
    qsizetype pagesReleased = 0;
    qsizetype peakProjectedClasses = 0;
    qsizetype peakProjectedRows = 0;
    qsizetype peakPageCells = 0;
    qsizetype scannedCells = 0;
};
}
