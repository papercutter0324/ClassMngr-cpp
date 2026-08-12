#pragma once

#include "domain/models/document_output_result.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"

#include <QList>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QSize>
#include <QString>

class ApplicationServices;
class QWidget;

namespace RosterTemplatePrintService
{

using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;

enum class Scope
{
    AllClasses,
    CurrentClass,
    SelectedClasses
};

enum class TemplateId
{
    ByDay,
    Daily,
    PerClassWithExtraInfo
};

struct Request
{
    QWidget* parent = nullptr;
    ApplicationServices* services = nullptr;
    int currentClassId = -1;
    Scope scope = Scope::AllClasses;
    QList<int> selectedClassIds;
    TemplateId templateId = TemplateId::ByDay;
    QStringList selectedExtraColumns;
    QPageLayout::Orientation perClassExtraInfoOrientation =
        QPageLayout::Portrait;
};

struct RosterCellValue
{
    QString day;
    int row = 0;
    int column = 0;
    QString value;
};

struct RosterClassData
{
    Classroom classroom;
    ClassInfo info;
    Roster roster;
};

QList<int> resolveClassIds(
    Scope scope,
    int currentClassId,
    const QList<int>& selectedClassIds,
    const QList<Classroom>& classes
    );

QList<RosterCellValue> buildByDayCellValues(
    const QList<RosterClassData>& classes,
    QString* errorMessage = nullptr
    );

QList<RosterCellValue> buildDailyCellValues(
    const QList<RosterClassData>& classes,
    QString* errorMessage = nullptr
    );

int perClassExtraInfoMaxExtraColumns(
    QPageLayout::Orientation orientation
    );

QStringList availablePerClassExtraInfoColumns(
    const QList<RosterClassData>& classes
    );

QList<RosterCellValue> buildPerClassExtraInfoCellValues(
    const QList<RosterClassData>& classes,
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation orientation,
    QString* errorMessage = nullptr
    );

QList<TemplateId> availableTemplateIds();

QString templateDisplayName(
    TemplateId templateId
    );

QPageLayout::Orientation templateOrientation(
    TemplateId templateId
    );

QPageSize::PageSizeId templatePageSize(
    TemplateId templateId
    );

QImage renderTemplatePreview(
    TemplateId templateId,
    const QSize& requestedSize
    );

QImage renderTemplatePreview(
    const Request& request,
    const QSize& requestedSize,
    bool liveData,
    QString* errorMessage = nullptr
    );

[[nodiscard]] Result saveRostersPdf(
    const QList<RosterClassData>& classes,
    const QString& documentPath,
    TemplateId templateId = TemplateId::ByDay,
    const QStringList& selectedExtraColumns = {},
    QPageLayout::Orientation perClassExtraInfoOrientation =
        QPageLayout::Portrait
    );

[[nodiscard]] Result saveRostersPdf(
    const Request& request,
    const QString& documentPath
    );

[[nodiscard]] Result printRosters(
    const Request& request
    );

} // namespace RosterTemplatePrintService
