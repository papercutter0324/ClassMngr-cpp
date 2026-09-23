#pragma once

#include "features/roster/services/roster_template_print_service.h"
#include "features/sub_prep/services/sub_prep_print_service.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "next/application/sub_prep_roster_output_source_query.h"

#include <QDate>
#include <QList>
#include <QPageLayout>
#include <QString>
#include <QStringList>

#include <vector>

class QWidget;

namespace SubPrepPackageService
{
enum class Status
{
    Completed,
    Canceled,
    Failed
};

struct Request
{
    QWidget* parent = nullptr;
    ClassMngr::Next::Application::
        SubPrepRosterOutputSourceReadPort* rosterOutputSourceReadPort = nullptr;
    SubPrepPrintService::Request subPrep;
    QList<QDate> selectedDates;
    std::vector<ClassMngr::Next::Domain::ClassId> selectedClassIds;
    bool useIntensiveSchedule = false;
    bool createFolder = true;
    QString targetRoot;
    QString userName;
    bool replaceExisting = false;
    bool printPaperCopies = false;
    bool openFolderAfterGeneration = true;
    RosterTemplatePrintService::TemplateId rosterTemplate =
        RosterTemplatePrintService::TemplateId::ByDay;
    QStringList selectedExtraColumns;
    QPageLayout::Orientation perClassOrientation =
        QPageLayout::Portrait;
};

struct Result
{
    Status status = Status::Failed;
    QString message;
    QString outputDirectory;
    QStringList documentPaths;
    bool folderCreated = false;
    bool printCanceled = false;
};

[[nodiscard]] QString defaultTargetRoot();

[[nodiscard]] QString safePathComponent(
    const QString& value,
    const QString& fallback = QStringLiteral("Sub Prep")
    );

[[nodiscard]] QString datedFolderName(
    const QString& userName,
    const QList<QDate>& selectedDates
    );

[[nodiscard]] QList<int> classIdsForDays(
    const ScheduleViewModel& schedule,
    const QStringList& selectedDays
    );

// Takes ownership of this operation snapshot and releases its Sub Prep
// information-sheet input immediately after that PDF is rendered.
[[nodiscard]] Result generate(Request request);
}
