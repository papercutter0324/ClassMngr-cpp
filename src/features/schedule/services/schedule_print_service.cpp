#include "schedule_print_service.h"

#include "core/fontmanager.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <algorithm>

#include <QMarginsF>
#include <QObject>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPen>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QRectF>
#include <QTemporaryDir>
#include <QTime>

namespace SchedulePrintService
{
namespace
{
constexpr qreal TimeColumnWidth = 118.0;
constexpr qreal DayColumnWidth = 142.0;
constexpr qreal HeaderHeight = 52.0;
constexpr qreal RowHeight = 58.0;
constexpr qreal FooterHeight = 52.0;
constexpr qreal CellPadding = 5.0;
constexpr QPageSize::PageSizeId SchedulePdfPageSize = QPageSize::A4;
constexpr qreal SchedulePdfMarginInches = 0.5;
constexpr int SchedulePdfResolutionDpi = 300;

struct PrintPalette
{
    QColor pageBackground;
    QColor tableBackground;
    QColor headerBackground;
    QColor headerText;
    QColor timeBackground;
    QColor timeText;
    QColor grid;
    QColor emptyBackground;
    QColor essayBackground;
    QColor lunchBackground;
    QColor testingBackground;
    QColor testingMarker;
    QColor bodyText;
    bool excel = false;
};

Result failed(
    const QString& message
    )
{
    return {
        Status::Failed,
        message
    };
}

Result canceled()
{
    return {
        Status::Canceled,
        QString()
    };
}

Result sent()
{
    return {
        Status::Sent,
        QObject::tr("Print job sent.")
    };
}

Theme resolvedTheme(
    SchedulePrintStyle style,
    Theme currentTheme
    )
{
    switch (style)
    {
    case SchedulePrintStyle::LightTheme:
        return Theme::Light;

    case SchedulePrintStyle::DarkTheme:
        return Theme::Dark;

    case SchedulePrintStyle::CurrentAppearance:
    case SchedulePrintStyle::Excel:
    default:
        return currentTheme;
    }
}

PrintPalette paletteFor(
    SchedulePrintStyle style,
    Theme currentTheme
    )
{
    if (style == SchedulePrintStyle::Excel)
    {
        return {
            Qt::white,
            Qt::white,
            Qt::white,
            Qt::black,
            Qt::white,
            Qt::black,
            Qt::black,
            Qt::white,
            Qt::white,
            QColor(QStringLiteral("#DCDCDC")),
            QColor(QStringLiteral("#FFF0B8")),
            QColor(QStringLiteral("#B66A00")),
            Qt::black,
            true
        };
    }

    if (resolvedTheme(style, currentTheme) == Theme::Light)
    {
        return {
            Qt::white,
            Qt::white,
            QColor(QStringLiteral("#deded8")),
            QColor(QStringLiteral("#546169")),
            QColor(QStringLiteral("#e9e8e3")),
            QColor(QStringLiteral("#27313a")),
            QColor(QStringLiteral("#c5c7c3")),
            Qt::white,
            Qt::white,
            QColor(QStringLiteral("#DCDCDC")),
            QColor(QStringLiteral("#FFF0B8")),
            QColor(QStringLiteral("#B66A00")),
            Qt::black,
            false
        };
    }

    return {
        Qt::white,
        Qt::white,
        QColor(QStringLiteral("#303030")),
        QColor(QStringLiteral("#c8c8c8")),
        QColor(QStringLiteral("#2b2b2b")),
        QColor(QStringLiteral("#f0f0f0")),
        QColor(QStringLiteral("#454545")),
        Qt::white,
        Qt::white,
        QColor(QStringLiteral("#DCDCDC")),
        QColor(QStringLiteral("#4B3D20")),
        QColor(QStringLiteral("#FFD166")),
        Qt::black,
        false
    };
}

QString englishLine(
    const ScheduleEntry& entry,
    bool compact
    )
{
    QStringList parts;

    if (!entry.classGrade.trimmed().isEmpty())
    {
        parts.append(entry.classGrade.trimmed());
    }

    if (!entry.classLevel.trimmed().isEmpty())
    {
        parts.append(entry.classLevel.trimmed());
    }

    return parts.join(
        compact
            ? QStringLiteral("-")
            : QStringLiteral(" - ")
        );
}

QString excelDayLabel(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QStringLiteral("월(MON)");
    }

    if (day == QStringLiteral("Tuesday"))
    {
        return QStringLiteral("화(TUE)");
    }

    if (day == QStringLiteral("Wednesday"))
    {
        return QStringLiteral("수(WED)");
    }

    if (day == QStringLiteral("Thursday"))
    {
        return QStringLiteral("목(THU)");
    }

    if (day == QStringLiteral("Friday"))
    {
        return QStringLiteral("금(FRI)");
    }

    if (day == QStringLiteral("Saturday"))
    {
        return QStringLiteral("토(SAT)");
    }

    if (day == QStringLiteral("Sunday"))
    {
        return QStringLiteral("일(SUN)");
    }

    return day;
}

QString excelTimeLabel(
    const QString& rangeLabel
    )
{
    QString label =
        rangeLabel;

    label.remove(QStringLiteral(" AM"));
    label.remove(QStringLiteral(" PM"));
    label.replace(QStringLiteral(" -\n"), QStringLiteral("~"));
    label.replace(QStringLiteral("\n- "), QStringLiteral("~"));
    label.replace(QStringLiteral(" - "), QStringLiteral("~"));

    return label;
}

QColor classColor(
    const ScheduleEntry& entry
    )
{
    const QColor color(entry.classColor);

    return color.isValid()
        ? color
        : QColor(Qt::white);
}

QColor fontColor(
    const ScheduleEntry& entry
    )
{
    const QColor color(entry.fontColor);

    return color.isValid()
        ? color
        : QColor(Qt::black);
}

QRectF targetRectFor(
    const QSizeF& sourceSize,
    const QRectF& bounds
    )
{
    QSizeF targetSize =
        sourceSize;
    targetSize.scale(
        bounds.size(),
        Qt::KeepAspectRatio
        );

    return QRectF(
        bounds.x() + ((bounds.width() - targetSize.width()) / 2.0),
        bounds.y(),
        targetSize.width(),
        targetSize.height()
        );
}

QMarginsF schedulePdfMargins()
{
    return QMarginsF(
        0.0,
        0.0,
        0.0,
        0.0
        );
}

QPageLayout schedulePdfPageLayout(
    QPageLayout::Orientation orientation
    )
{
    return QPageLayout(
        QPageSize(SchedulePdfPageSize),
        orientation,
        schedulePdfMargins(),
        QPageLayout::Inch
        );
}

bool configureSchedulePdfWriter(
    QPdfWriter& writer,
    QPageLayout::Orientation orientation
    )
{
    writer.setCreator(
        QStringLiteral("ClassMngr")
        );
    writer.setTitle(
        QStringLiteral("Schedule")
        );
    writer.setResolution(SchedulePdfResolutionDpi);

    return writer.setPageLayout(
        schedulePdfPageLayout(orientation)
        );
}

QRectF schedulePdfPageRect(
    const QPdfWriter& writer
    )
{
    const QRect pageRect =
        writer.pageLayout().fullRectPixels(
            std::max(
                1,
                writer.resolution()
                )
            );

    if (
        pageRect.width() > 0
        && pageRect.height() > 0
        )
    {
        return QRectF(pageRect);
    }

    return QRectF(
        0.0,
        0.0,
        writer.width(),
        writer.height()
        );
}

QRectF schedulePdfContentRect(
    const QRectF& pageRect,
    int resolutionDpi
    )
{
    const qreal margin =
        SchedulePdfMarginInches
        * std::max(
            1,
            resolutionDpi
            );

    return pageRect.adjusted(
        margin,
        margin,
        -margin,
        -margin
        );
}

QFont printUiFont(
    int pixelSize,
    int weight = QFont::Normal
    )
{
    QFont font =
        FontManager::getUiFont(
            -1,
            weight
            );
    font.setPixelSize(pixelSize);
    return font;
}

QFont printKoreanFont(
    int pixelSize,
    int weight = QFont::Normal
    )
{
    QFont font =
        FontManager::getKoreanFont(
            -1,
            weight
            );
    font.setPixelSize(pixelSize);
    return font;
}

void drawCenteredText(
    QPainter& painter,
    const QRectF& rect,
    const QString& text,
    const QFont& font,
    const QColor& color,
    int flags = Qt::AlignCenter | Qt::TextWordWrap
    )
{
    painter.save();
    painter.setPen(color);
    painter.setFont(font);
    painter.drawText(
        rect.adjusted(
            CellPadding,
            CellPadding,
            -CellPadding,
            -CellPadding
            ),
        flags,
        text
        );
    painter.restore();
}

void drawEntry(
    QPainter& painter,
    const QRectF& rect,
    const ScheduleEntry& entry,
    bool excel,
    bool showEnglishNames
    )
{
    painter.save();
    painter.setPen(
        fontColor(entry)
        );

    QFont koreanFont =
        printKoreanFont(
            excel ? 15 : 13,
            QFont::DemiBold
            );
    QFont englishFont =
        printUiFont(
            excel ? 13 : 12,
            excel ? QFont::Bold : QFont::Normal
            );

    const QRectF textRect =
        rect.adjusted(
            CellPadding,
            CellPadding,
            -CellPadding,
            -CellPadding
            );

    if (entry.kind == ScheduleEntryKind::TestingClass)
    {
        const QString details =
            englishLine(
                entry,
                excel
                );
        const QString room =
            entry.roomNumber.trimmed();
        QFont nameFont =
            printUiFont(
                excel ? 14 : 13,
                QFont::Bold
                );
        painter.setFont(nameFont);
        painter.drawText(
            QRectF(
                textRect.left(),
                textRect.top(),
                textRect.width(),
                textRect.height() / 3.0
                ),
            Qt::AlignCenter | Qt::TextWordWrap,
            entry.className.trimmed()
            );

        painter.setFont(
            printUiFont(
                excel ? 12 : 11,
                QFont::DemiBold
                )
            );
        painter.drawText(
            QRectF(
                textRect.left(),
                textRect.top() + (textRect.height() / 3.0),
                textRect.width(),
                textRect.height() / 3.0
                ),
            Qt::AlignCenter | Qt::TextWordWrap,
            details
            );

        painter.setFont(
            printKoreanFont(
                excel ? 11 : 10,
                QFont::Normal
                )
            );
        painter.drawText(
            QRectF(
                textRect.left(),
                textRect.top() + (2.0 * textRect.height() / 3.0),
                textRect.width(),
                textRect.height() / 3.0
                ),
            Qt::AlignCenter | Qt::TextWordWrap,
            room
            );
        painter.restore();
        return;
    }

    const qreal middle =
        textRect.center().y();

    painter.setFont(koreanFont);
    painter.drawText(
        QRectF(
            textRect.left(),
            middle - 20.0,
            textRect.width(),
            20.0
            ),
        Qt::AlignCenter | Qt::TextWordWrap,
        scheduleTeacherRoomLine(
            entry,
            showEnglishNames
            )
        );

    painter.setFont(englishFont);
    painter.drawText(
        QRectF(
            textRect.left(),
            middle,
            textRect.width(),
            24.0
            ),
        Qt::AlignCenter | Qt::TextWordWrap,
        englishLine(
            entry,
            excel
            )
        );

    painter.restore();
}

void drawCellFrame(
    QPainter& painter,
    const QRectF& rect,
    const PrintPalette& palette,
    bool header = false
    )
{
    painter.save();

    QPen pen(
        palette.grid,
        header || palette.excel ? 1.2 : 0.8
        );

    if (palette.excel && !header)
    {
        pen.setStyle(Qt::DotLine);
    }

    painter.setPen(pen);
    painter.drawRect(rect);
    painter.restore();
}

void drawScheduleCell(
    QPainter& painter,
    const QRectF& rect,
    const ScheduleCellView& cell,
    const PrintPalette& palette,
    bool showEnglishNames
    )
{
    painter.save();

    if (!cell.entries.isEmpty())
    {
        painter.fillRect(
            rect,
            classColor(cell.entries.first())
            );

        if (cell.entries.size() == 1)
        {
            drawEntry(
                painter,
                rect,
                cell.entries.first(),
                palette.excel,
                showEnglishNames
                );

            if (
                cell.entries.first().kind
                    == ScheduleEntryKind::TestingClass
                )
            {
                painter.setPen(Qt::NoPen);
                painter.setBrush(
                    fontColor(cell.entries.first())
                    );
                constexpr qreal MarkerSize = 16.0;
                QPolygonF marker;
                marker
                    << rect.topRight() - QPointF(MarkerSize, 0.0)
                    << rect.topRight()
                    << rect.topRight() + QPointF(0.0, MarkerSize);
                painter.drawPolygon(marker);
            }
        }
        else
        {
            const qreal segmentHeight =
                rect.height() / cell.entries.size();

            for (int index = 0; index < cell.entries.size(); ++index)
            {
                drawEntry(
                    painter,
                    QRectF(
                        rect.left(),
                        rect.top() + (index * segmentHeight),
                        rect.width(),
                        segmentHeight
                        ),
                    cell.entries.at(index),
                    palette.excel,
                    showEnglishNames
                    );
            }
        }
    }
    else if (cell.slotState == scheduleEssaySlotState())
    {
        painter.fillRect(
            rect,
            palette.essayBackground
            );
        drawCenteredText(
            painter,
            rect,
            QStringLiteral("ESSAY"),
            printUiFont(
                palette.excel ? 17 : 18,
                QFont::Bold
                ),
            Qt::black
            );
    }
    else if (cell.slotState == scheduleLunchSlotState())
    {
        painter.fillRect(
            rect,
            palette.lunchBackground
            );
        drawCenteredText(
            painter,
            rect,
            QStringLiteral("Lunch"),
            printUiFont(
                palette.excel ? 15 : 18,
                QFont::Bold
                ),
            Qt::black
            );
    }
    else if (cell.slotState == scheduleTestingSlotState())
    {
        painter.fillRect(
            rect,
            palette.testingBackground
            );

        QString text =
            QObject::tr("Oral Testing");
        const QString room =
            cell.testingRoom.trimmed();
        if (!room.isEmpty())
        {
            text +=
                QObject::tr("\nRm: %1").arg(room);
        }

        drawCenteredText(
            painter,
            rect,
            text,
            printUiFont(
                palette.excel ? 14 : 15,
                QFont::Bold
                ),
            palette.excel
                ? Qt::black
                : palette.testingMarker
            );

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette.testingMarker);
        constexpr qreal MarkerSize = 16.0;
        QPolygonF marker;
        marker
            << rect.topRight() - QPointF(MarkerSize, 0.0)
            << rect.topRight()
            << rect.topRight() + QPointF(0.0, MarkerSize);
        painter.drawPolygon(marker);
    }
    else
    {
        painter.fillRect(
            rect,
            palette.emptyBackground
            );
    }

    painter.restore();

    drawCellFrame(
        painter,
        rect,
        palette
        );
}

void drawFooter(
    QPainter& painter,
    const QRectF& tableRect,
    const ScheduleViewModel& model,
    const PrintPalette& palette,
    qreal y
    )
{
    painter.fillRect(
        QRectF(
            tableRect.left(),
            y,
            tableRect.width(),
            FooterHeight
            ),
        palette.excel ? Qt::white : palette.tableBackground
        );

    if (!palette.excel || model.days.isEmpty())
    {
        return;
    }

    const int dayCount =
        static_cast<int>(model.days.size());
    const int firstSummaryDay =
        std::max(
            0,
            dayCount - 2
            );

    const qreal firstX =
        tableRect.left()
        + TimeColumnWidth
        + (firstSummaryDay * DayColumnWidth);
    const qreal width =
        (dayCount - firstSummaryDay) * DayColumnWidth;

    painter.save();
    painter.setPen(
        QPen(
            Qt::red,
            1.4
            )
        );
    painter.drawRect(
        QRectF(
            firstX,
            y,
            width,
            FooterHeight
            )
        );

    if (dayCount - firstSummaryDay >= 2)
    {
        painter.drawLine(
            QPointF(
                firstX + DayColumnWidth,
                y
                ),
            QPointF(
                firstX + DayColumnWidth,
                y + FooterHeight
                )
            );
    }

    painter.restore();

    drawCenteredText(
        painter,
        QRectF(
            firstX,
            y,
            DayColumnWidth,
            FooterHeight
            ),
        QString::number(model.summary.essayBlocks),
        printUiFont(
            16,
            QFont::Bold
            ),
        Qt::black,
        Qt::AlignRight | Qt::AlignVCenter
        );

    const qreal scheduledX =
        dayCount - firstSummaryDay >= 2
            ? firstX + DayColumnWidth
            : firstX;

    drawCenteredText(
        painter,
        QRectF(
            scheduledX,
            y,
            DayColumnWidth,
            FooterHeight
            ),
        QString::number(model.summary.scheduledBlocks),
        printUiFont(
            16,
            QFont::Bold
            ),
        Qt::black,
        Qt::AlignRight | Qt::AlignVCenter
        );
}

void paintSchedule(
    QPainter& painter,
    const ScheduleViewModel& model,
    const SchedulePrintStyle style,
    const Theme currentTheme,
    const QString& userName,
    bool showEnglishNames,
    const QRectF& bounds
    )
{
    const PrintPalette palette =
        paletteFor(
            style,
            currentTheme
            );

    const int rowCount =
        std::max(
            1,
            static_cast<int>(model.rows.size())
            );
    const qreal tableWidth =
        TimeColumnWidth + (DayColumnWidth * model.days.size());
    const qreal tableHeight =
        HeaderHeight
        + (RowHeight * rowCount)
        + (palette.excel ? FooterHeight : 0.0);

    const QRectF target =
        targetRectFor(
            QSizeF(
                tableWidth,
                tableHeight
                ),
            bounds
            );

    painter.fillRect(
        bounds,
        palette.pageBackground
        );

    painter.save();
    painter.translate(
        target.topLeft()
        );
    painter.scale(
        target.width() / tableWidth,
        target.height() / tableHeight
        );

    const QRectF tableRect(
        0.0,
        0.0,
        tableWidth,
        tableHeight
        );

    painter.fillRect(
        tableRect,
        palette.tableBackground
        );

    const QString leftHeader =
        palette.excel
            ? userName.trimmed().isEmpty()
                ? QStringLiteral("Schedule")
                : userName.trimmed()
            : QStringLiteral("Time");

    QRectF rect(
        0.0,
        0.0,
        TimeColumnWidth,
        HeaderHeight
        );
    painter.fillRect(
        rect,
        palette.headerBackground
        );
    drawCenteredText(
        painter,
        rect,
        leftHeader,
        printUiFont(
            palette.excel ? 17 : 14,
            QFont::Bold
            ),
        palette.headerText
        );
    drawCellFrame(
        painter,
        rect,
        palette,
        true
        );

    for (int dayIndex = 0; dayIndex < model.days.size(); ++dayIndex)
    {
        rect =
            QRectF(
                TimeColumnWidth + (dayIndex * DayColumnWidth),
                0.0,
                DayColumnWidth,
                HeaderHeight
                );
        painter.fillRect(
            rect,
            palette.headerBackground
            );
        drawCenteredText(
            painter,
            rect,
            palette.excel
                ? excelDayLabel(model.days.at(dayIndex))
                : model.days.at(dayIndex),
            printUiFont(
                palette.excel ? 17 : 14,
                QFont::Bold
                ),
            palette.headerText
            );
        drawCellFrame(
            painter,
            rect,
            palette,
            true
            );
    }

    if (model.rows.isEmpty())
    {
        rect =
            QRectF(
                0.0,
                HeaderHeight,
                tableWidth,
                RowHeight
                );
        painter.fillRect(
            rect,
            palette.emptyBackground
            );
        drawCenteredText(
            painter,
            rect,
            QStringLiteral("No registered class meeting times available."),
            printUiFont(
                15,
                QFont::DemiBold
                ),
            palette.bodyText
            );
        drawCellFrame(
            painter,
            rect,
            palette
            );
    }
    else
    {
        for (int rowIndex = 0; rowIndex < model.rows.size(); ++rowIndex)
        {
            const ScheduleRowView& row =
                model.rows.at(rowIndex);
            const qreal y =
                HeaderHeight + (rowIndex * RowHeight);

            rect =
                QRectF(
                    0.0,
                    y,
                    TimeColumnWidth,
                    RowHeight
                    );

            QColor timeBackground =
                palette.timeBackground;

            if (palette.excel)
            {
                const QTime time =
                    QTime::fromString(
                        row.timeLabel,
                        QStringLiteral("HH:mm")
                        );
                timeBackground =
                    time.isValid() && (time.hour() % 2 == 0)
                        ? QColor(QStringLiteral("#FFFF99"))
                        : QColor(QStringLiteral("#D9D9D9"));
            }

            painter.fillRect(
                rect,
                timeBackground
                );
            drawCenteredText(
                painter,
                rect,
                palette.excel
                    ? excelTimeLabel(row.timeRangeLabel)
                    : row.timeRangeLabel,
                printUiFont(
                    palette.excel ? 15 : 13,
                    QFont::DemiBold
                    ),
                palette.timeText
                );
            drawCellFrame(
                painter,
                rect,
                palette
                );

            for (int dayIndex = 0; dayIndex < row.cells.size(); ++dayIndex)
            {
                rect =
                    QRectF(
                        TimeColumnWidth + (dayIndex * DayColumnWidth),
                        y,
                        DayColumnWidth,
                        RowHeight
                        );

                drawScheduleCell(
                    painter,
                    rect,
                    row.cells.at(dayIndex),
                    palette,
                    showEnglishNames
                    );
            }
        }
    }

    if (palette.excel)
    {
        drawFooter(
            painter,
            tableRect,
            model,
            palette,
            HeaderHeight + (rowCount * RowHeight)
            );
    }

    painter.restore();
}
}

Result saveSchedulePdf(
    const Request& request,
    const QString& documentPath
    )
{
    if (documentPath.trimmed().isEmpty())
    {
        return failed(
            QObject::tr("No schedule print file path was provided.")
            );
    }

    QPdfWriter writer(documentPath);
    if (!configureSchedulePdfWriter(
            writer,
            request.pageOrientation
            ))
    {
        return failed(
            QObject::tr("Unable to configure the schedule print file.")
            );
    }

    QPainter painter;
    if (!painter.begin(&writer))
    {
        return failed(
            QObject::tr("Unable to create the schedule print file.")
            );
    }

    const QRectF pageRect =
        schedulePdfPageRect(
            writer
            );
    const QRectF printableRect =
        schedulePdfContentRect(
            pageRect,
            writer.resolution()
            );

    if (
        pageRect.width() <= 0.0
        || pageRect.height() <= 0.0
        || printableRect.width() <= 0.0
        || printableRect.height() <= 0.0
        )
    {
        painter.end();
        return failed(
            QObject::tr("Unable to determine the schedule print area.")
            );
    }

    painter.fillRect(
        pageRect,
        Qt::white
        );

    paintSchedule(
        painter,
        request.model,
        request.style,
        request.currentTheme,
        request.userName,
        request.showEnglishNames,
        printableRect
        );

    if (!painter.end())
    {
        return failed(
            QObject::tr("The schedule print file could not be completed.")
            );
    }

    return {
        Status::Sent,
        QObject::tr("Schedule PDF created.")
    };
}

Result printSchedule(
    const Request& request
    )
{
    QTemporaryDir temporaryDirectory;

    if (!temporaryDirectory.isValid())
    {
        return failed(
            QObject::tr("Unable to create a temporary print file.")
            );
    }

    const QString documentPath =
        temporaryDirectory.filePath(
            QStringLiteral("Schedule.pdf")
            );

    {
        const Result saveResult =
            saveSchedulePdf(
                request,
                documentPath
                );

        if (saveResult.status != Status::Sent)
        {
            return saveResult;
        }
    }

    QPdfDocument document;
    const QPdfDocument::Error loadError =
        document.load(documentPath);

    if (
        loadError != QPdfDocument::Error::None
        || document.status() != QPdfDocument::Status::Ready
        || document.pageCount() <= 0
        )
    {
        return failed(
            QObject::tr("Unable to load the schedule print file.")
            );
    }

    const PdfPrintService::Result result =
        PdfPrintService::printPdfDocument(
            {
                request.parent,
                &document,
                documentPath,
                0,
                QObject::tr("Print Schedule"),
                request.pageOrientation,
                false,
                SchedulePdfPageSize,
                true
            }
            );

    switch (result.status)
    {
    case PdfPrintService::Status::Sent:
        return sent();

    case PdfPrintService::Status::Canceled:
        return canceled();

    case PdfPrintService::Status::Failed:
    default:
        return failed(result.message);
    }
}
}
