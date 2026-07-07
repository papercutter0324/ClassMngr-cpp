#pragma once

#include "core/result.h"
#include "core/utils/platform.h"
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
    QString templatePath;
    int currentClassId = -1;
    Scope scope = Scope::AllClasses;
    QList<int> selectedClassIds;
};

struct FillOperation
{
    QString sheet;
    QString cell;
    QString value;
};

struct RosterClassData
{
    Classroom classroom;
    ClassInfo info;
    Roster roster;
};

QStringList preferredTemplateSuffixes(
    Platform platform
    );

QString preferredBundledTemplatePath(
    Platform platform
    );

QList<int> resolveClassIds(
    Scope scope,
    int currentClassId,
    const QList<int>& selectedClassIds,
    const QList<Classroom>& classes
    );

QList<FillOperation> buildByDayFillOperations(
    const QList<RosterClassData>& classes,
    QString* errorMessage = nullptr
    );

bool isSupportedByDayTemplate(
    const QString& templatePath,
    QString* errorMessage = nullptr
    );

[[nodiscard]] Result printRosters(
    const Request& request
    );

} // namespace RosterTemplatePrintService
