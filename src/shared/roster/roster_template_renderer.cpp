#include "shared/roster/roster_template_renderer.h"
#include <QColor>
#include <QAtomicInteger>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QFileInfo>
#include <QMarginsF>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QSaveFile>
#include <QPdfWriter>
#include <QPen>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QTemporaryDir>
#include <QTime>
#include <QVector>

#include <algorithm>
#include <array>
#include <charconv>
#include <string>


namespace RosterTemplateRenderer {
namespace {
thread_local Fonts activeFonts;
struct FontScope { Fonts previous = activeFonts; explicit FontScope(const Fonts& fonts) { activeFonts = fonts; } ~FontScope() { activeFonts = previous; } };
QImage renderPreviewImage(TemplateId, const QSize&, const QString&, const QHash<QString, QString>&, QPageLayout::Orientation);
#include "roster_template_print_shared_data.inc"
#include "roster_template_print_shared_layout.inc"
#include "roster_template_print_shared_rendering.inc"
#include "roster_template_print_daily_template.inc"
#include "roster_template_print_by_day_template.inc"
#include "roster_template_print_per_class_template.inc"
}
// Private orchestration and preview helpers.


QStringList daysWithValues(
    TemplateId templateId,
    const QList<RosterCellValue>& values
    )
{
    if (templateId == TemplateId::PerClassWithExtraInfo)
    {
        QStringList pages;

        for (const RosterCellValue& value : values)
        {
            if (!pages.contains(value.day))
            {
                pages.append(value.day);
            }
        }

        return pages;
    }

    QList<QString> included;

    for (const RosterCellValue& value : values)
    {
        if (
            DaySheets.contains(baseDayName(value.day))
            && !included.contains(value.day)
            )
        {
            included.append(value.day);
        }
    }

    QStringList days;
    for (const QString& day : DaySheets)
    {
        QList<QString> matchingPages;
        for (const QString& includedDay : std::as_const(included))
        {
            if (baseDayName(includedDay) == day)
            {
                matchingPages.append(includedDay);
            }
        }

        std::sort(
            matchingPages.begin(),
            matchingPages.end(),
            [](const QString& left, const QString& right)
            {
                return dailyPageIndex(left) < dailyPageIndex(right);
            }
            );

        days.append(matchingPages);
    }

    return days;
}

QList<RosterCellValue> buildTemplateCellValues(
    TemplateId templateId,
    const QList<RosterClassData>& classes,
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation perClassExtraInfoOrientation,
    QString* errorMessage
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        return buildPerClassExtraInfoCellValues(
            classes,
            selectedExtraColumns,
            perClassExtraInfoOrientation,
            errorMessage
            );

    case TemplateId::Daily:
        return buildDailyCellValues(
            classes,
            errorMessage
            );

    case TemplateId::ByDay:
    default:
        return buildByDayCellValues(
            classes,
            errorMessage
            );
    }
}


namespace {
void paintTemplateDay(
    TemplateId templateId,
    QPainter& painter,
    const QString& day,
    const QHash<QString, QString>& values,
    const QRectF& pageRect,
    const QRectF& contentRect,
    int resolutionDpi
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        Q_UNUSED(day);
        paintPerClassWithExtraInfoPage(
            painter,
            values,
            pageRect,
            contentRect,
            resolutionDpi
            );
        break;

    case TemplateId::Daily:
        paintDailyDay(
            painter,
            day,
            values,
            pageRect,
            contentRect,
            resolutionDpi
            );
        break;

    case TemplateId::ByDay:
    default:
        paintRosterDay(
            painter,
            day,
            values,
            pageRect,
            contentRect,
            resolutionDpi
            );
        break;
    }
}

QSize previewPagePixelSize(
    TemplateId templateId,
    const QSize& requestedSize,
    QPageLayout::Orientation orientation
    )
{
    const QSize fallbackSize(
        420,
        260
        );
    const QSize boundedSize =
        requestedSize.isValid()
            ? requestedSize
            : fallbackSize;

    const bool landscape =
        orientation == QPageLayout::Landscape;
    const qreal pageWidth =
        landscape
            ? A4HeightInches
            : A4WidthInches;
    const qreal pageHeight =
        landscape
            ? A4WidthInches
            : A4HeightInches;
    const qreal scale =
        std::min(
            boundedSize.width() / pageWidth,
            boundedSize.height() / pageHeight
            );

    return QSize(
        std::max(
            1,
            qRound(pageWidth * scale)
            ),
        std::max(
            1,
            qRound(pageHeight * scale)
            )
        );
}

QImage renderPreviewImage(
    TemplateId templateId,
    const QSize& requestedSize,
    const QString& day,
    const QHash<QString, QString>& values,
    QPageLayout::Orientation orientation
    )
{
    const QSize pageSize =
        previewPagePixelSize(
            templateId,
            requestedSize,
            orientation
            );
    QImage image(
        pageSize,
        QImage::Format_ARGB32_Premultiplied
        );
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(
        QPainter::Antialiasing,
        false
        );
    painter.setRenderHint(
        QPainter::TextAntialiasing,
        true
        );

    const bool landscape =
        orientation == QPageLayout::Landscape;
    const qreal pageWidthInches =
        landscape
            ? A4HeightInches
            : A4WidthInches;
    const int resolutionDpi =
        std::max(
            1,
            qRound(image.width() / pageWidthInches)
            );
    const QRectF pageRect(
        0.0,
        0.0,
        image.width(),
        image.height()
        );
    const QRectF contentRect =
        templateContentRect(
            templateId,
            pageRect,
            resolutionDpi
            );

    paintTemplateDay(
        templateId,
        painter,
        day,
        values,
        pageRect,
        contentRect,
        resolutionDpi
        );

    painter.end();
    return image;
}
}
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

        for (const RosterPrintScheduleTime& time :
             data.classInfo.regularSchedule)
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
            appendCellValue(
                values,
                day,
                column,
                TeacherRoomRow,
                teacherRoomLabel(data.classInfo)
                );
            appendCellValue(values, day, column, WifiRow, data.classInfo.wifiName);
            appendCellValue(values, day, column, WifiPasswordRow, data.classInfo.wifiPassword);
            appendCellValue(values, day, column, ZoomRow, data.classInfo.zoomId);
            appendCellValue(values, day, column, ZoomPasswordRow, data.classInfo.zoomPassword);

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
            for (const RosterPrintScheduleTime& time :
                 data.classInfo.regularSchedule)
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
        appendCellValue(values, pageKey, 4, 1, data.classInfo.roomNumber);

        appendCellValue(values, pageKey, 1, 2, QStringLiteral("Days/Times"));
        appendCellValue(
            values,
            pageKey,
            2,
            2,
            perClassTimeLabels(data.classInfo.regularSchedule).join(
                QStringLiteral("; ")
                )
            );
        appendCellValue(values, pageKey, 3, 2, QStringLiteral("Wifi"));
        appendCellValue(values, pageKey, 4, 2, data.classInfo.wifiName);

        appendCellValue(values, pageKey, 1, 3, QStringLiteral("Teacher"));
        appendCellValue(values, pageKey, 2, 3, teacherLabel(data.classInfo));
        appendCellValue(values, pageKey, 3, 3, QStringLiteral("Wifi Password"));
        appendCellValue(values, pageKey, 4, 3, data.classInfo.wifiPassword);

        appendCellValue(values, pageKey, 1, 4, QStringLiteral("ZOOM"));
        appendCellValue(values, pageKey, 2, 4, data.classInfo.zoomId);
        appendCellValue(values, pageKey, 3, 4, QStringLiteral("Zoom Password"));
        appendCellValue(values, pageKey, 4, 4, data.classInfo.zoomPassword);

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


QImage previewPage(TemplateId id, const Page& page, const QSize& size,
    QPageLayout::Orientation orientation, const Fonts& fonts)
{
    FontScope scope(fonts);
    return renderPreviewImage(id, size, page.key, valuesForDay(page.values, page.key), orientation);
}
Result savePagesPdf(const QString& path, TemplateId id, QPageLayout::Orientation orientation,
    const PageProducer& produce, const Fonts& fonts, const PageReleased& released)
{
    if (path.trimmed().isEmpty() || !produce) return failed(QObject::tr("No roster print file path was provided."));
    FontScope scope(fonts);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return failed(QObject::tr("Unable to create the roster print file."));
    {
        QPdfWriter writer(&file);
        if (!configureRosterPdfWriter(writer, id, orientation)) return failed(QObject::tr("Unable to configure the roster print file."));
        QPainter painter;
        qsizetype pages = 0;
        while (true)
        {
            qsizetype cellCount = 0;
            {
                auto next = produce();
                if (!next) return failed(next.error());
                if (!*next) break;
                const auto& page = **next;
                if (pages == 0 && !painter.begin(&writer)) return failed(QObject::tr("Unable to create the roster print file."));
                if (pages > 0 && !writer.newPage()) return failed(QObject::tr("Unable to add a roster print page."));
                const QRectF rect = rosterPdfPageRect(writer);
                paintTemplateDay(id, painter, page.key, valuesForDay(page.values, page.key), rect,
                    templateContentRect(id, rect, writer.resolution()), writer.resolution());
                cellCount = page.values.size();
                ++pages;
            }
            if (released) released(cellCount);
        }
        if (!pages) return failed(QObject::tr("No selected classes match the roster print layout."));
        if (!painter.end()) return failed(QObject::tr("The roster print file could not be completed."));
    }
    if (!file.commit()) return failed(QObject::tr("The roster print file could not be completed."));
    return {Status::Sent, QObject::tr("Roster PDF created.")};
}
Result saveCellValuesPdf(const QString& path, TemplateId id, QPageLayout::Orientation orientation,
    const QList<RosterCellValue>& values, const Fonts& fonts)
{
    const auto days = daysWithValues(id, values);
    qsizetype index = 0;
    return savePagesPdf(path, id, orientation, [&]() -> PageResult {
        if (index >= days.size()) return std::optional<Page>{};
        Page page{days.at(index++), {}};
        for (const auto& value : values) if (value.day == page.key) page.values.append(value);
        return std::optional<Page>(std::move(page));
    }, fonts);
}
}
