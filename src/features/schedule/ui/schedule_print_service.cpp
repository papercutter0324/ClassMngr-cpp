#include "schedule_print_service.h"

#include "core/fontmanager.h"

#include <algorithm>

#include <QAbstractPrintDialog>
#include <QDialog>
#include <QObject>
#include <QPainter>
#include <QPageLayout>
#include <QPen>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrinterInfo>
#include <QRectF>
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
            Qt::black,
            true
        };
    }

    if (resolvedTheme(style, currentTheme) == Theme::Light)
    {
        return {
            QColor(QStringLiteral("#f5f3ee")),
            QColor(QStringLiteral("#f5f3ee")),
            QColor(QStringLiteral("#deded8")),
            QColor(QStringLiteral("#546169")),
            QColor(QStringLiteral("#e9e8e3")),
            QColor(QStringLiteral("#27313a")),
            QColor(QStringLiteral("#c5c7c3")),
            QColor(QStringLiteral("#f5f3ee")),
            Qt::white,
            QColor(QStringLiteral("#DCDCDC")),
            QColor(QStringLiteral("#27313a")),
            false
        };
    }

    return {
        QColor(QStringLiteral("#1e1e1e")),
        QColor(QStringLiteral("#1e1e1e")),
        QColor(QStringLiteral("#303030")),
        QColor(QStringLiteral("#c8c8c8")),
        QColor(QStringLiteral("#2b2b2b")),
        QColor(QStringLiteral("#f0f0f0")),
        QColor(QStringLiteral("#454545")),
        QColor(QStringLiteral("#1e1e1e")),
        Qt::white,
        QColor(QStringLiteral("#DCDCDC")),
        QColor(QStringLiteral("#f0f0f0")),
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

QString koreanLine(
    const ScheduleEntry& entry
    )
{
    return QStringLiteral("%1 %2")
        .arg(entry.teacherKr.trimmed())
        .arg(entry.roomNumber.trimmed())
        .simplified();
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
        bounds.y() + ((bounds.height() - targetSize.height()) / 2.0),
        targetSize.width(),
        targetSize.height()
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
    bool excel
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
        koreanLine(entry)
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
    const PrintPalette& palette
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
                palette.excel
                );
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
                    palette.excel
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
            QObject::tr("ESSAY"),
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
            QObject::tr("Lunch"),
            printUiFont(
                palette.excel ? 15 : 18,
                QFont::Bold
                ),
            Qt::black
            );
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
                ? QObject::tr("Schedule")
                : userName.trimmed()
            : QObject::tr("Time");

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
            QObject::tr("No registered class meeting times available."),
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
                    palette
                    );
            }
        }
    }

    drawFooter(
        painter,
        tableRect,
        model,
        palette,
        HeaderHeight + (rowCount * RowHeight)
        );

    painter.restore();
}
}

Result printSchedule(
    const Request& request
    )
{
#ifdef Q_OS_LINUX
    if (QPrinterInfo::availablePrinterNames().isEmpty())
    {
        return failed(
            QObject::tr("No printers are available.")
            );
    }
#endif

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::NativeFormat);
    printer.setDocName(
        QObject::tr("Print from ClassMngr - Schedule")
        );
    printer.setCreator(
        QStringLiteral("ClassMngr")
        );
    QPageLayout pageLayout =
        printer.pageLayout();
    pageLayout.setOrientation(
        QPageLayout::Landscape
        );
    printer.setPageLayout(
        pageLayout
        );

    QPrintDialog dialog(
        &printer,
        request.parent
        );

    dialog.setWindowTitle(
        QObject::tr("Print Schedule")
        );
    dialog.setOptions(
        QAbstractPrintDialog::PrintCollateCopies
        | QAbstractPrintDialog::PrintShowPageSize
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return canceled();
    }

    QPainter painter;

    if (!painter.begin(&printer))
    {
        return failed(
            QObject::tr("Unable to start the print job.")
            );
    }

    QRectF printableRect =
        printer.pageRect(QPrinter::DevicePixel);

    if (
        printableRect.width() <= 0.0
        || printableRect.height() <= 0.0
        )
    {
        printableRect =
            QRectF(
                0.0,
                0.0,
                printer.width(),
                printer.height()
                );
    }

    if (
        printableRect.width() <= 0.0
        || printableRect.height() <= 0.0
        )
    {
        painter.end();
        return failed(
            QObject::tr("The selected printer does not provide a printable page area.")
            );
    }

    paintSchedule(
        painter,
        request.model,
        request.style,
        request.currentTheme,
        request.userName,
        printableRect
        );

    if (!painter.end())
    {
        return failed(
            QObject::tr("The print job could not be completed.")
            );
    }

    if (printer.printerState() == QPrinter::Error)
    {
        return failed(
            QObject::tr("The printer reported an error while printing.")
            );
    }

    return sent();
}
}
