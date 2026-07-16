#include "features/roster/services/roster_template_print_service.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "data/data_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QColor>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QMarginsF>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPen>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QTemporaryDir>
#include <QTime>
#include <QVector>

#include <algorithm>

namespace RosterTemplatePrintService
{
namespace
{

// Template-specific implementation is split into focused include fragments.
// They share this translation unit so rendering helpers remain private.

#include "roster_template_print_shared_data.inc"
#include "roster_template_print_shared_layout.inc"
#include "roster_template_print_shared_rendering.inc"
#include "roster_template_print_daily_template.inc"
#include "roster_template_print_by_day_template.inc"
#include "roster_template_print_per_class_template.inc"
#include "roster_template_print_private_service.inc"

} // namespace

QList<TemplateId> availableTemplateIds()
{
    return {
        TemplateId::ByDay,
        TemplateId::Daily,
        TemplateId::PerClassWithExtraInfo
    };
}

QString templateDisplayName(
    TemplateId templateId
    )
{
    return templateTitle(templateId);
}

QPageLayout::Orientation templateOrientation(
    TemplateId templateId
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        return QPageLayout::Portrait;

    case TemplateId::Daily:
        return QPageLayout::Portrait;

    case TemplateId::ByDay:
    default:
        return QPageLayout::Landscape;
    }
}

QPageSize::PageSizeId templatePageSize(
    TemplateId templateId
    )
{
    Q_UNUSED(templateId);
    return RosterPdfPageSize;
}

QImage renderTemplatePreview(
    TemplateId templateId,
    const QSize& requestedSize
    )
{
    return renderPreviewImage(
        templateId,
        requestedSize,
        templateId == TemplateId::PerClassWithExtraInfo
            ? QStringLiteral("class|0")
            : DaySheets.first(),
        {},
        templateOrientation(templateId)
        );
}

QImage renderTemplatePreview(
    const Request& request,
    const QSize& requestedSize,
    bool liveData,
    QString* errorMessage
    )
{
    if (!liveData)
    {
        return renderPreviewImage(
            request.templateId,
            requestedSize,
            request.templateId == TemplateId::PerClassWithExtraInfo
                ? QStringLiteral("class|0")
                : DaySheets.first(),
            {},
            request.templateId == TemplateId::PerClassWithExtraInfo
                ? request.perClassExtraInfoOrientation
                : templateOrientation(request.templateId)
            );
    }

    QList<RosterClassData> rosterClasses;
    const Result loadResult =
        loadRosterClassesForRequest(
            request,
            &rosterClasses
            );

    if (loadResult.status != Status::Sent)
    {
        if (errorMessage)
        {
            *errorMessage = loadResult.message;
        }
        return {};
    }

    QString valueError;
    const QList<RosterCellValue> values =
        buildTemplateCellValues(
            request.templateId,
            rosterClasses,
            request.selectedExtraColumns,
            request.perClassExtraInfoOrientation,
            &valueError
            );

    if (!valueError.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = valueError;
        }
        return {};
    }

    const QStringList days =
        daysWithValues(
            request.templateId,
            values
            );

    if (days.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("No selected classes match this roster layout.");
        }
        return {};
    }

    const QString day =
        days.first();

    return renderPreviewImage(
        request.templateId,
        requestedSize,
        day,
        valuesForDay(
            values,
            day
            ),
        request.templateId == TemplateId::PerClassWithExtraInfo
            ? request.perClassExtraInfoOrientation
            : templateOrientation(request.templateId)
        );
}

QList<int> resolveClassIds(
    Scope scope,
    int currentClassId,
    const QList<int>& selectedClassIds,
    const QList<Classroom>& classes
    )
{
    QList<int> ids;

    switch (scope)
    {
    case Scope::CurrentClass:
        if (currentClassId > 0)
        {
            ids.append(currentClassId);
        }
        break;

    case Scope::SelectedClasses:
        for (int classId : selectedClassIds)
        {
            if (classId > 0 && !ids.contains(classId))
            {
                ids.append(classId);
            }
        }
        break;

    case Scope::AllClasses:
    default:
        for (const Classroom& classroom : classes)
        {
            if (classroom.id > 0 && !ids.contains(classroom.id))
            {
                ids.append(classroom.id);
            }
        }
        break;
    }

    return ids;
}

QList<RosterCellValue> buildByDayCellValues(
    const QList<RosterClassData>& classes,
    QString* errorMessage
    )
{
    QList<RosterCellValue> values;
    QSet<QString> occupiedSlots;

    for (const RosterClassData& data : classes)
    {
        const int englishColumn =
            rosterColumnIndex(data.roster, QStringLiteral("English"));
        const int koreanColumn =
            rosterColumnIndex(data.roster, QStringLiteral("Korean"));

        for (const ClassTime& time : data.info.classTimes)
        {
            const QString day =
                time.day.trimmed();

            if (!DaySheets.contains(day))
            {
                continue;
            }

            const int column =
                columnForStartTime(time.startTime);

            if (column < 0)
            {
                continue;
            }

            const QString slotKey =
                day + QLatin1Char('|') + QString::number(column);

            if (occupiedSlots.contains(slotKey))
            {
                if (errorMessage)
                {
                    *errorMessage =
                        QObject::tr(
                            "Multiple selected classes use the %1 %2 slot."
                            )
                            .arg(day, time.startTime);
                }
                return {};
            }

            occupiedSlots.insert(slotKey);

            appendCellValue(values, day, column, LevelRow, classLabel(data));
            appendCellValue(values, day, column, TeacherRoomRow, teacherRoomLabel(data.info));
            appendCellValue(values, day, column, WifiRow, data.info.wifiName);
            appendCellValue(values, day, column, WifiPasswordRow, data.info.wifiPassword);
            appendCellValue(values, day, column, ZoomRow, data.info.zoomId);
            appendCellValue(values, day, column, ZoomPasswordRow, data.info.zoomPassword);

            int writtenStudentCount = 0;
            for (const QStringList& row : data.roster.rows)
            {
                if (writtenStudentCount >= MaxStudentsPerClass)
                {
                    break;
                }

                const QString english =
                    rosterCell(row, englishColumn);
                const QString korean =
                    rosterCell(row, koreanColumn);

                if (english.isEmpty() && korean.isEmpty())
                {
                    continue;
                }

                const int outputRow =
                    FirstStudentRow + writtenStudentCount;

                appendCellValue(values, day, column, outputRow, english);
                appendCellValue(values, day, column + 1, outputRow, korean);
                ++writtenStudentCount;
            }
        }
    }

    return values;
}

QList<RosterCellValue> buildDailyCellValues(
    const QList<RosterClassData>& classes,
    QString* errorMessage
    )
{
    Q_UNUSED(errorMessage);

    QList<RosterCellValue> values;

    for (const QString& day : DaySheets)
    {
        QList<DailyClassSection> sections;
        int inputIndex = 0;

        for (const RosterClassData& data : classes)
        {
            for (const ClassTime& time : data.info.classTimes)
            {
                if (time.day.trimmed() != day)
                {
                    continue;
                }

                sections.append(
                    {
                        data,
                        time,
                        inputIndex
                    }
                    );
                ++inputIndex;
            }
        }

        std::stable_sort(
            sections.begin(),
            sections.end(),
            [](const DailyClassSection& left, const DailyClassSection& right)
            {
                const QTime leftTime =
                    parsedStartTime(left.time.startTime);
                const QTime rightTime =
                    parsedStartTime(right.time.startTime);

                if (leftTime.isValid() != rightTime.isValid())
                {
                    return leftTime.isValid();
                }

                if (
                    leftTime.isValid()
                    && rightTime.isValid()
                    && leftTime != rightTime
                    )
                {
                    return leftTime < rightTime;
                }

                const int labelCompare =
                    classLabel(left.data).localeAwareCompare(
                        classLabel(right.data)
                        );

                if (labelCompare != 0)
                {
                    return labelCompare < 0;
                }

                return left.inputIndex < right.inputIndex;
            }
            );

        for (int sectionIndex = 0;
             sectionIndex < sections.size();
             ++sectionIndex)
        {
            const DailyClassSection& section =
                sections.at(sectionIndex);
            const int pageIndex =
                sectionIndex / DailySectionsPerPage;
            const int pageSectionIndex =
                sectionIndex % DailySectionsPerPage;
            const QString pageKey =
                dailyPageKey(
                    day,
                    pageIndex
                    );
            const int headerRow =
                DailyFirstSectionRow
                + (pageSectionIndex * DailyRowsPerSection);

            appendCellValue(
                values,
                pageKey,
                DailyHeaderColumn,
                headerRow,
                dailyClassHeader(
                    section.data,
                    section.time
                    )
                );

            const int englishColumn =
                rosterColumnIndex(
                    section.data.roster,
                    QStringLiteral("English")
                    );
            const int koreanColumn =
                rosterColumnIndex(
                    section.data.roster,
                    QStringLiteral("Korean")
                    );

            int writtenStudentCount = 0;
            for (const QStringList& row : section.data.roster.rows)
            {
                if (writtenStudentCount >= DailyMaxStudentsPerClass)
                {
                    break;
                }

                const QString name =
                    dailyStudentName(
                        row,
                        englishColumn,
                        koreanColumn
                        );

                if (name.isEmpty())
                {
                    continue;
                }

                appendCellValue(
                    values,
                    pageKey,
                    DailyFirstStudentColumn
                        + (writtenStudentCount % DailyStudentColumnCount),
                    headerRow
                        + 1
                        + (writtenStudentCount / DailyStudentColumnCount),
                    name
                    );
                ++writtenStudentCount;
            }
        }
    }

    return values;
}

int perClassExtraInfoMaxExtraColumns(
    QPageLayout::Orientation orientation
    )
{
    return orientation == QPageLayout::Landscape
        ? PerClassLandscapeMaxExtraColumns
        : PerClassPortraitMaxExtraColumns;
}

QStringList availablePerClassExtraInfoColumns(
    const QList<RosterClassData>& classes
    )
{
    QStringList columns;

    for (const RosterClassData& data : classes)
    {
        for (const QString& column : data.roster.columns)
        {
            const QString trimmed =
                column.trimmed();

            if (
                isPerClassExtraInfoColumn(trimmed)
                && !containsColumnName(columns, trimmed)
                )
            {
                columns.append(trimmed);
            }
        }
    }

    return columns;
}

QList<RosterCellValue> buildPerClassExtraInfoCellValues(
    const QList<RosterClassData>& classes,
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation orientation,
    QString* errorMessage
    )
{
    Q_UNUSED(errorMessage);

    const QStringList extraColumns =
        limitedPerClassExtraColumns(
            selectedExtraColumns,
            orientation
            );
    QList<RosterCellValue> values;

    for (int classIndex = 0; classIndex < classes.size(); ++classIndex)
    {
        const RosterClassData& data =
            classes.at(classIndex);
        const QString pageKey =
            perClassPageKey(
                classIndex,
                data
                );

        appendCellValue(values, pageKey, 1, 1, QStringLiteral("Level"));
        appendCellValue(values, pageKey, 2, 1, classLabel(data));
        appendCellValue(values, pageKey, 3, 1, QStringLiteral("Room"));
        appendCellValue(values, pageKey, 4, 1, data.info.roomNumber);

        appendCellValue(values, pageKey, 1, 2, QStringLiteral("Days/Times"));
        appendCellValue(
            values,
            pageKey,
            2,
            2,
            perClassTimeLabels(data.info.classTimes).join(QStringLiteral("; "))
            );
        appendCellValue(values, pageKey, 3, 2, QStringLiteral("Wifi"));
        appendCellValue(values, pageKey, 4, 2, data.info.wifiName);

        appendCellValue(values, pageKey, 1, 3, QStringLiteral("Teacher"));
        appendCellValue(values, pageKey, 2, 3, teacherLabel(data.info));
        appendCellValue(values, pageKey, 3, 3, QStringLiteral("Wifi Password"));
        appendCellValue(values, pageKey, 4, 3, data.info.wifiPassword);

        appendCellValue(values, pageKey, 1, 4, QStringLiteral("ZOOM"));
        appendCellValue(values, pageKey, 2, 4, data.info.zoomId);
        appendCellValue(values, pageKey, 3, 4, QStringLiteral("Zoom Password"));
        appendCellValue(values, pageKey, 4, 4, data.info.zoomPassword);

        appendCellValue(values, pageKey, PerClassIndexColumn, PerClassHeaderRow, QStringLiteral("No."));
        appendCellValue(values, pageKey, PerClassEnglishColumn, PerClassHeaderRow, QStringLiteral("English Name"));
        appendCellValue(values, pageKey, PerClassKoreanColumn, PerClassHeaderRow, QStringLiteral("Korean Name"));

        for (int index = 0; index < extraColumns.size(); ++index)
        {
            appendCellValue(
                values,
                pageKey,
                PerClassFirstExtraColumn + index,
                PerClassHeaderRow,
                extraColumns.at(index)
                );
        }

        const int englishColumn =
            rosterColumnIndex(
                data.roster,
                QStringLiteral("English")
                );
        const int koreanColumn =
            rosterColumnIndex(
                data.roster,
                QStringLiteral("Korean")
                );
        QVector<int> extraColumnIndexes;
        extraColumnIndexes.reserve(extraColumns.size());

        for (const QString& extraColumn : extraColumns)
        {
            extraColumnIndexes.append(
                rosterColumnIndex(
                    data.roster,
                    extraColumn
                    )
                );
        }

        for (
            int rowIndex = 0;
            rowIndex < PerClassStudentRowCount;
            ++rowIndex
            )
        {
            const int outputRow =
                PerClassFirstStudentRow + rowIndex;

            appendCellValue(
                values,
                pageKey,
                PerClassIndexColumn,
                outputRow,
                QString::number(rowIndex + 1)
                );

            if (rowIndex >= data.roster.rows.size())
            {
                continue;
            }

            const QStringList& row =
                data.roster.rows.at(rowIndex);

            appendCellValue(
                values,
                pageKey,
                PerClassEnglishColumn,
                outputRow,
                rosterCell(row, englishColumn)
                );
            appendCellValue(
                values,
                pageKey,
                PerClassKoreanColumn,
                outputRow,
                rosterCell(row, koreanColumn)
                );

            for (int index = 0; index < extraColumnIndexes.size(); ++index)
            {
                appendCellValue(
                    values,
                    pageKey,
                    PerClassFirstExtraColumn + index,
                    outputRow,
                    rosterCell(
                        row,
                        extraColumnIndexes.at(index)
                        )
                    );
            }
        }
    }

    return values;
}

Result saveRostersPdf(
    const QList<RosterClassData>& classes,
    const QString& documentPath,
    TemplateId templateId,
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation perClassExtraInfoOrientation
    )
{
    if (documentPath.trimmed().isEmpty())
    {
        return failed(
            QObject::tr("No roster print file path was provided.")
            );
    }

    QString errorMessage;
    const QList<RosterCellValue> values =
        buildTemplateCellValues(
            templateId,
            classes,
            selectedExtraColumns,
            perClassExtraInfoOrientation,
            &errorMessage
            );

    if (!errorMessage.isEmpty())
    {
        return failed(errorMessage);
    }

    const QStringList days =
        daysWithValues(
            templateId,
            values
            );

    if (days.isEmpty())
    {
        return failed(
            QObject::tr("No selected classes match the roster print layout.")
            );
    }

    QPdfWriter writer(documentPath);
    if (!configureRosterPdfWriter(
            writer,
            templateId,
            templateId == TemplateId::PerClassWithExtraInfo
                ? perClassExtraInfoOrientation
                : templateOrientation(templateId)
            ))
    {
        return failed(
            QObject::tr("Unable to configure the roster print file.")
            );
    }

    QPainter painter;
    if (!painter.begin(&writer))
    {
        return failed(
            QObject::tr("Unable to create the roster print file.")
            );
    }

    const QRectF pageRect =
        rosterPdfPageRect(writer);
    const QRectF contentRect =
        templateContentRect(
            templateId,
            pageRect,
            writer.resolution()
            );

    if (
        pageRect.width() <= 0.0
        || pageRect.height() <= 0.0
        || contentRect.width() <= 0.0
        || contentRect.height() <= 0.0
        )
    {
        painter.end();
        return failed(
            QObject::tr("Unable to determine the roster print area.")
            );
    }

    for (int index = 0; index < days.size(); ++index)
    {
        if (
            index > 0
            && !writer.newPage()
            )
        {
            painter.end();
            return failed(
                QObject::tr("Unable to add a roster print page.")
                );
        }

        const QString& day =
            days.at(index);
        paintTemplateDay(
            templateId,
            painter,
            day,
            valuesForDay(
                values,
                day
                ),
            pageRect,
            contentRect,
            writer.resolution()
            );
    }

    if (!painter.end())
    {
        return failed(
            QObject::tr("The roster print file could not be completed.")
            );
    }

    return {
        Status::Sent,
        QObject::tr("Roster PDF created.")
    };
}

Result saveRostersPdf(
    const Request& request,
    const QString& documentPath
    )
{
    if (documentPath.trimmed().isEmpty())
    {
        return saveRostersPdf(
            QList<RosterClassData>(),
            documentPath,
            request.templateId,
            request.selectedExtraColumns,
            request.perClassExtraInfoOrientation
            );
    }

    QList<RosterClassData> rosterClasses;
    const Result loadResult =
        loadRosterClassesForRequest(
            request,
            &rosterClasses
            );

    if (loadResult.status != Status::Sent)
    {
        return loadResult;
    }

    return saveRostersPdf(
        rosterClasses,
        documentPath,
        request.templateId,
        request.selectedExtraColumns,
        request.perClassExtraInfoOrientation
        );
}

Result printRosters(
    const Request& request
    )
{
    QList<RosterClassData> rosterClasses;
    const Result loadResult =
        loadRosterClassesForRequest(
            request,
            &rosterClasses
            );

    if (loadResult.status != Status::Sent)
    {
        return loadResult;
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return failed(QObject::tr("Unable to create a temporary print folder."));
    }

    const QString pdfPath =
        temporaryDirectory.filePath(QStringLiteral("Rosters.pdf"));

    {
        const Result saveResult =
            saveRostersPdf(
                rosterClasses,
                pdfPath,
                request.templateId,
                request.selectedExtraColumns,
                request.perClassExtraInfoOrientation
                );

        if (saveResult.status != Status::Sent)
        {
            return failed(
                failedPdfMessage(saveResult.message)
                );
        }
    }

    QPdfDocument document;
    const QPdfDocument::Error loadError =
        document.load(pdfPath);

    if (
        loadError != QPdfDocument::Error::None
        || document.status() != QPdfDocument::Status::Ready
        || document.pageCount() <= 0
        )
    {
        return failed(QObject::tr("Unable to load the generated roster PDF."));
    }

    const PdfPrintService::Result printResult =
        PdfPrintService::printPdfDocument(
            {
                request.parent,
                &document,
                pdfPath,
                0,
                QObject::tr("Print Rosters"),
                request.templateId == TemplateId::PerClassWithExtraInfo
                    ? request.perClassExtraInfoOrientation
                    : templateOrientation(request.templateId),
                false,
                templatePageSize(request.templateId),
                true
            }
            );

    switch (printResult.status)
    {
    case PdfPrintService::Status::Sent:
        return sent();

    case PdfPrintService::Status::Canceled:
        return canceled();

    case PdfPrintService::Status::Failed:
    default:
        return failed(printResult.message);
    }
}

} // namespace RosterTemplatePrintService
