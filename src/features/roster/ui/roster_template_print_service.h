#pragma once

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

enum class Status
{
    Sent,
    Canceled,
    Failed
};

enum class Scope
{
    AllClasses,
    CurrentClass,
    SelectedClasses
};

enum class TemplateId
{
    ByDay,
    ClassRegister
};

struct Result
{
    Status status = Status::Failed;
    QString message;
};

struct Request
{
    QWidget* parent = nullptr;
    ApplicationServices* services = nullptr;
    int currentClassId = -1;
    Scope scope = Scope::AllClasses;
    QList<int> selectedClassIds;
    TemplateId templateId = TemplateId::ByDay;
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

QList<RosterCellValue> buildClassRegisterCellValues(
    const QList<RosterClassData>& classes,
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
    TemplateId templateId = TemplateId::ByDay
    );

[[nodiscard]] Result saveRostersPdf(
    const Request& request,
    const QString& documentPath
    );

[[nodiscard]] Result printRosters(
    const Request& request
    );

} // namespace RosterTemplatePrintService
