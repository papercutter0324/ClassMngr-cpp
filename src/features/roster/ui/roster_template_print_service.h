#pragma once

#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"

#include <QList>
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

[[nodiscard]] Result saveRostersPdf(
    const QList<RosterClassData>& classes,
    const QString& documentPath
    );

[[nodiscard]] Result saveRostersPdf(
    const Request& request,
    const QString& documentPath
    );

[[nodiscard]] Result printRosters(
    const Request& request
    );

} // namespace RosterTemplatePrintService
