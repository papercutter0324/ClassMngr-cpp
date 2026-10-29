#pragma once
#include "domain/models/document_output_result.h"
#include <QFont>
#include <QImage>
#include <QList>
#include <QPageLayout>
#include <QPageSize>
#include <QStringList>
#include <functional>
#include <expected>
#include <optional>
namespace RosterTemplateRenderer {
using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;
enum class TemplateId
{
    ByDay,
    Daily,
    PerClassWithExtraInfo
};

struct RosterCellValue
{
    QString day;
    int row = 0;
    int column = 0;
    QString value;
};

struct RosterPrintScheduleTime
{
    QString day;
    QString startTime;
    QString endTime;
};

struct RosterPrintClassInfo
{
    QString classGrade;
    QString classLevel;
    QString teacherEn;
    QString teacherKr;
    QString roomNumber;
    QString wifiName;
    QString wifiPassword;
    QString zoomId;
    QString zoomPassword;
    QList<RosterPrintScheduleTime> regularSchedule;
};

struct RosterClassData
{
    struct ClassIdentity { int id = -1; QString name; } classroom;
    RosterPrintClassInfo classInfo;
    struct Rows { QStringList columns; QList<QStringList> rows; } roster;
};

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


struct Fonts { QFont ui = QFont(QStringLiteral("Inter")); QFont korean = QFont(QStringLiteral("Pretendard Variable")); };
struct Page { QString key; QList<RosterCellValue> values; };
using PageResult = std::expected<std::optional<Page>, QString>;
// The producer releases its class projections before returning each page.
using PageProducer = std::function<PageResult()>;
// Invoked after the page values and paint hash are destroyed. Carries counts only.
using PageReleased = std::function<void(qsizetype)>;
QList<RosterCellValue> buildTemplateCellValues(TemplateId, const QList<RosterClassData>&,
    const QStringList&, QPageLayout::Orientation, QString*);
QStringList daysWithValues(TemplateId, const QList<RosterCellValue>&);
QImage previewPage(TemplateId, const Page&, const QSize&, QPageLayout::Orientation, const Fonts& = {});
Result savePagesPdf(const QString&, TemplateId, QPageLayout::Orientation,
    const PageProducer&, const Fonts& = {}, const PageReleased& = {});
Result saveCellValuesPdf(const QString&, TemplateId, QPageLayout::Orientation,
    const QList<RosterCellValue>&, const Fonts& = {});
}
