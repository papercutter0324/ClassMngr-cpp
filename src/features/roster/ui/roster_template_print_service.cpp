#include "features/roster/ui/roster_template_print_service.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "data/data_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QColor>
#include <QFontMetricsF>
#include <QHash>
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
constexpr int DayTitleRow = 1;
constexpr int TimeRow = 2;
constexpr int LevelRow = 3;
constexpr int TeacherRoomRow = 4;
constexpr int FirstStudentRow = 5;
constexpr int LastStudentRow = 29;
constexpr int WifiRow = 30;
constexpr int WifiPasswordRow = 31;
constexpr int ZoomRow = 32;
constexpr int ZoomPasswordRow = 33;
constexpr int RowCount = ZoomPasswordRow;
constexpr int ColumnCount = 13;
constexpr int MaxStudentsPerClass = LastStudentRow - FirstStudentRow + 1;

constexpr qreal ColumnWidthInches = 0.6833;
constexpr qreal TitleRowHeightInches = 0.3000;
constexpr qreal NormalRowHeightInches = 0.2000;
constexpr qreal SectionEndRowHeightInches = 0.2083;
constexpr qreal RosterPdfMarginInches = 0.5;
constexpr int RosterPdfResolutionDpi = 300;
constexpr QPageSize::PageSizeId RosterPdfPageSize = QPageSize::A4;

const QStringList DaySheets{
    QStringLiteral("Monday"),
    QStringLiteral("Tuesday"),
    QStringLiteral("Wednesday"),
    QStringLiteral("Thursday"),
    QStringLiteral("Friday")
};

const QList<int> SlotColumns{2, 4, 6, 8, 10, 12};

const QStringList TimeLabels{
    QStringLiteral("4pm"),
    QStringLiteral("5pm"),
    QStringLiteral("6pm"),
    QStringLiteral("7pm"),
    QStringLiteral("8pm"),
    QStringLiteral("9pm")
};

struct RosterPdfPalette
{
    QColor pageBackground = Qt::white;
    QColor cellBackground = Qt::white;
    QColor timeBackground = QColor(QStringLiteral("#95B3D7"));
    QColor classInfoBackground = QColor(QStringLiteral("#B8CCE4"));
    QColor footerBackground = QColor(QStringLiteral("#DCE6F1"));
    QColor grid = Qt::black;
    QColor text = Qt::black;
};

struct CellBorders
{
    bool top = false;
    bool right = false;
    bool bottom = false;
    bool left = false;
};

struct RosterPrintLayout
{
    QVector<qreal> columnWidths;
    QVector<qreal> rowHeights;
};

Result failed(
    const QString& message
    )
{
    return {
        Status::Failed,
        message.trimmed().isEmpty()
            ? QObject::tr("Roster printing failed.")
            : message
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
        QObject::tr("Roster print job sent.")
    };
}

int columnForStartTime(
    const QString& startTime
    )
{
    const QString trimmed =
        startTime.trimmed();

    QTime time =
        QTime::fromString(trimmed, QStringLiteral("h:mm AP"));

    if (!time.isValid())
    {
        time =
            QTime::fromString(trimmed, QStringLiteral("h:mm ap"));
    }

    if (!time.isValid())
    {
        time =
            QTime::fromString(trimmed, QStringLiteral("H:mm"));
    }

    if (!time.isValid())
    {
        return -1;
    }

    int hour =
        time.hour();

    if (hour > 12)
    {
        hour -= 12;
    }

    switch (hour)
    {
    case 4:
        return 2;
    case 5:
        return 4;
    case 6:
        return 6;
    case 7:
        return 8;
    case 8:
        return 10;
    case 9:
        return 12;
    default:
        return -1;
    }
}

int rosterColumnIndex(
    const Roster& roster,
    const QString& name
    )
{
    for (int index = 0; index < roster.columns.size(); ++index)
    {
        if (roster.columns.at(index).compare(name, Qt::CaseInsensitive) == 0)
        {
            return index;
        }
    }

    return -1;
}

QString rosterCell(
    const QStringList& row,
    int column
    )
{
    if (column < 0 || column >= row.size())
    {
        return {};
    }

    return row.at(column).trimmed();
}

QString classLabel(
    const RosterClassData& data
    )
{
    QStringList parts;

    if (!data.info.classGrade.trimmed().isEmpty())
    {
        parts.append(data.info.classGrade.trimmed());
    }

    if (!data.info.classLevel.trimmed().isEmpty())
    {
        parts.append(data.info.classLevel.trimmed());
    }

    const QString label =
        parts.join(QLatin1Char(' ')).trimmed();

    return label.isEmpty()
        ? data.classroom.name.trimmed()
        : label;
}

QString teacherRoomLabel(
    const ClassInfo& info
    )
{
    QString teacher =
        info.teacherEn.trimmed();

    if (teacher.isEmpty())
    {
        teacher =
            info.teacherKr.trimmed();
    }

    const QString room =
        info.roomNumber.trimmed();

    if (teacher.isEmpty())
    {
        return room.isEmpty()
            ? QString()
            : QStringLiteral("(%1)").arg(room);
    }

    return room.isEmpty()
        ? teacher
        : QStringLiteral("%1 (%2)").arg(teacher, room);
}

void appendCellValue(
    QList<RosterCellValue>& values,
    const QString& day,
    int column,
    int row,
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    if (trimmed.isEmpty())
    {
        return;
    }

    values.append(
        {
            day,
            row,
            column,
            trimmed
        }
        );
}

QString failedPdfMessage(
    const QString& message
    )
{
    return message.trimmed().isEmpty()
        ? QObject::tr("Unable to create the roster print file.")
        : message;
}

QMarginsF rosterPdfMargins()
{
    return QMarginsF(
        0.0,
        0.0,
        0.0,
        0.0
        );
}

QPageLayout rosterPdfPageLayout()
{
    return QPageLayout(
        QPageSize(RosterPdfPageSize),
        QPageLayout::Landscape,
        rosterPdfMargins(),
        QPageLayout::Inch
        );
}

bool configureRosterPdfWriter(
    QPdfWriter& writer
    )
{
    writer.setCreator(
        QStringLiteral("ClassMngr")
        );
    writer.setTitle(
        QObject::tr("Rosters")
        );
    writer.setResolution(RosterPdfResolutionDpi);

    return writer.setPageLayout(
        rosterPdfPageLayout()
        );
}

QRectF rosterPdfPageRect(
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

QRectF rosterPdfContentRect(
    const QRectF& pageRect,
    int resolutionDpi
    )
{
    const qreal margin =
        RosterPdfMarginInches
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

qreal columnWidth(
    int resolutionDpi
    )
{
    return ColumnWidthInches
        * std::max(
            1,
            resolutionDpi
            );
}

qreal baseRowHeight(
    int row,
    int resolutionDpi
    )
{
    qreal inches = NormalRowHeightInches;

    if (row == DayTitleRow)
    {
        inches = TitleRowHeightInches;
    }
    else if (
        row == TeacherRoomRow
        || row == LastStudentRow
        || row == ZoomPasswordRow
        )
    {
        inches = SectionEndRowHeightInches;
    }

    return inches
        * std::max(
            1,
            resolutionDpi
            );
}

qreal rowTop(
    int row,
    const RosterPrintLayout& layout
    )
{
    qreal top = 0.0;

    for (int index = 0; index < row - 1 && index < layout.rowHeights.size(); ++index)
    {
        top += layout.rowHeights.at(index);
    }

    return top;
}

qreal columnLeft(
    int column,
    const RosterPrintLayout& layout
    )
{
    qreal left = 0.0;

    for (int index = 0;
         index < column - 1 && index < layout.columnWidths.size();
         ++index)
    {
        left += layout.columnWidths.at(index);
    }

    return left;
}

qreal spannedColumnWidth(
    int column,
    int columnSpan,
    const RosterPrintLayout& layout
    )
{
    qreal width = 0.0;

    for (int index = column - 1;
         index < column - 1 + columnSpan && index < layout.columnWidths.size();
         ++index)
    {
        width += layout.columnWidths.at(index);
    }

    return width;
}

qreal spannedRowHeight(
    int row,
    int rowSpan,
    const RosterPrintLayout& layout
    )
{
    qreal height = 0.0;

    for (int index = row - 1;
         index < row - 1 + rowSpan && index < layout.rowHeights.size();
         ++index)
    {
        height += layout.rowHeights.at(index);
    }

    return height;
}

bool isEnglishNameColumn(
    int column
    )
{
    return SlotColumns.contains(column);
}

RosterPrintLayout rosterPrintLayout(
    const QRectF& contentRect,
    int resolutionDpi
    )
{
    RosterPrintLayout layout;
    layout.columnWidths.reserve(ColumnCount);
    layout.rowHeights.reserve(RowCount);

    const qreal baseColumnWidth =
        columnWidth(resolutionDpi);
    const qreal baseTableWidth =
        ColumnCount * baseColumnWidth;
    const qreal extraWidthPerEnglishColumn =
        std::max(
            0.0,
            contentRect.width() - baseTableWidth
            )
        / SlotColumns.size();

    for (int column = 1; column <= ColumnCount; ++column)
    {
        layout.columnWidths.append(
            baseColumnWidth
            + (isEnglishNameColumn(column) ? extraWidthPerEnglishColumn : 0.0)
            );
    }

    qreal baseTableHeight = 0.0;
    for (int row = 1; row <= RowCount; ++row)
    {
        baseTableHeight += baseRowHeight(row, resolutionDpi);
    }

    const qreal extraHeightPerRow =
        std::max(
            0.0,
            contentRect.height() - baseTableHeight
            )
        / RowCount;

    for (int row = 1; row <= RowCount; ++row)
    {
        layout.rowHeights.append(
            baseRowHeight(row, resolutionDpi) + extraHeightPerRow
            );
    }

    return layout;
}

QRectF sourceCellRect(
    int row,
    int column,
    int rowSpan,
    int columnSpan,
    const RosterPrintLayout& layout
    )
{
    return QRectF(
        columnLeft(column, layout),
        rowTop(row, layout),
        spannedColumnWidth(column, columnSpan, layout),
        spannedRowHeight(row, rowSpan, layout)
        );
}

qreal borderWidth(
    int resolutionDpi
    )
{
    constexpr qreal BorderWidthPoints = 2.0;

    return BorderWidthPoints
        * std::max(
            1,
            resolutionDpi
            )
        / 72.0;
}

QFont printUiFont(
    qreal pointSize,
    int resolutionDpi,
    int weight = QFont::Normal
    )
{
    QFont font =
        FontManager::getUiFont(
            -1,
            weight
            );
    font.setPixelSize(
        std::max(
            1,
            qRound(pointSize * resolutionDpi / 72.0)
            )
        );
    return font;
}

QFont printKoreanFont(
    qreal pointSize,
    int resolutionDpi,
    int weight = QFont::Normal
    )
{
    QFont font =
        FontManager::getKoreanFont(
            -1,
            weight
            );
    font.setPixelSize(
        std::max(
            1,
            qRound(pointSize * resolutionDpi / 72.0)
            )
        );
    return font;
}

QFont fittedFont(
    const QString& text,
    const QFont& baseFont,
    const QRectF& rect
    )
{
    QFont font =
        baseFont;

    if (text.trimmed().isEmpty())
    {
        return font;
    }

    const qreal minimumPixelSize =
        std::max(
            6.0,
            baseFont.pixelSize() * 0.72
            );

    while (font.pixelSize() > minimumPixelSize)
    {
        const QFontMetricsF metrics(font);

        if (
            metrics.horizontalAdvance(text) <= rect.width()
            && metrics.height() <= rect.height()
            )
        {
            return font;
        }

        font.setPixelSize(font.pixelSize() - 1);
    }

    return font;
}

void drawCellBorders(
    QPainter& painter,
    const QRectF& rect,
    const CellBorders& borders,
    const QColor& color,
    qreal width
    )
{
    if (
        !borders.top
        && !borders.right
        && !borders.bottom
        && !borders.left
        )
    {
        return;
    }

    painter.save();
    painter.setPen(
        QPen(
            color,
            width,
            Qt::SolidLine,
            Qt::SquareCap,
            Qt::MiterJoin
            )
        );

    const qreal inset =
        width / 2.0;
    const qreal left =
        rect.left() + inset;
    const qreal right =
        rect.left() + rect.width() - inset;
    const qreal top =
        rect.top() + inset;
    const qreal bottom =
        rect.top() + rect.height() - inset;

    if (borders.top)
    {
        painter.drawLine(
            QPointF(left, top),
            QPointF(right, top)
            );
    }

    if (borders.right)
    {
        painter.drawLine(
            QPointF(right, top),
            QPointF(right, bottom)
            );
    }

    if (borders.bottom)
    {
        painter.drawLine(
            QPointF(left, bottom),
            QPointF(right, bottom)
            );
    }

    if (borders.left)
    {
        painter.drawLine(
            QPointF(left, top),
            QPointF(left, bottom)
            );
    }

    painter.restore();
}

void drawCell(
    QPainter& painter,
    const QRectF& rect,
    const QString& text,
    const QFont& font,
    const QColor& fill,
    const RosterPdfPalette& palette,
    const CellBorders& borders,
    qreal borderLineWidth,
    int alignment = Qt::AlignCenter
    )
{
    painter.save();

    painter.fillRect(
        rect,
        fill
        );
    drawCellBorders(
        painter,
        rect,
        borders,
        palette.grid,
        borderLineWidth
        );

    if (!text.trimmed().isEmpty())
    {
        constexpr qreal CellPadding = 5.0;

        const QRectF textRect =
            rect.adjusted(
                CellPadding,
                1.5,
                -CellPadding,
                -1.5
                );
        QFont displayFont =
            fittedFont(
                text,
                font,
                textRect
                );
        QFontMetricsF metrics(displayFont);
        QString displayText =
            text;

        if (metrics.horizontalAdvance(displayText) > textRect.width())
        {
            displayText =
                metrics.elidedText(
                    displayText,
                    Qt::ElideRight,
                    textRect.width()
                    );
        }

        painter.setPen(palette.text);
        painter.setFont(displayFont);
        painter.drawText(
            textRect,
            alignment,
            displayText
            );
    }

    painter.restore();
}

CellBorders allBorders()
{
    return {
        true,
        true,
        true,
        true
    };
}

CellBorders verticalBorders()
{
    return {
        false,
        true,
        false,
        true
    };
}

CellBorders topVerticalBorders()
{
    return {
        true,
        true,
        false,
        true
    };
}

CellBorders bottomVerticalBorders()
{
    return {
        false,
        true,
        true,
        true
    };
}

CellBorders leftBorder()
{
    return {
        false,
        false,
        false,
        true
    };
}

CellBorders leftBottomBorder()
{
    return {
        false,
        false,
        true,
        true
    };
}

CellBorders rightBorder()
{
    return {
        false,
        true,
        false,
        false
    };
}

CellBorders rightBottomBorder()
{
    return {
        false,
        true,
        true,
        false
    };
}

QString cellKey(
    int row,
    int column
    )
{
    return QStringLiteral("%1:%2")
        .arg(row)
        .arg(column);
}

QHash<QString, QString> valuesForDay(
    const QList<RosterCellValue>& values,
    const QString& day
    )
{
    QHash<QString, QString> result;

    for (const RosterCellValue& value : values)
    {
        if (value.day != day)
        {
            continue;
        }

        result.insert(
            cellKey(
                value.row,
                value.column
                ),
            value.value
            );
    }

    return result;
}

QString cellValue(
    const QHash<QString, QString>& values,
    int row,
    int column
    )
{
    return values.value(
        cellKey(
            row,
            column
            )
        );
}

void paintRosterDay(
    QPainter& painter,
    const QString& day,
    const QHash<QString, QString>& values,
    const QRectF& pageRect,
    const QRectF& contentRect,
    int resolutionDpi
    )
{
    const RosterPdfPalette palette;
    const RosterPrintLayout layout =
        rosterPrintLayout(
            contentRect,
            resolutionDpi
            );

    const QPointF tableOrigin(
        contentRect.left(),
        contentRect.top()
        );

    painter.fillRect(
        pageRect,
        palette.pageBackground
        );

    painter.save();
    painter.translate(tableOrigin);

    const QFont titleFont =
        printUiFont(
            16.0,
            resolutionDpi,
            QFont::Bold
            );
    const QFont labelFont =
        printUiFont(
            10.0,
            resolutionDpi,
            QFont::Bold
            );
    const QFont bodyFont =
        printUiFont(
            9.0,
            resolutionDpi
            );
    const QFont koreanFont =
        printKoreanFont(
            9.0,
            resolutionDpi
            );
    const qreal heavyBorderWidth =
        borderWidth(resolutionDpi);

    drawCell(
        painter,
        sourceCellRect(DayTitleRow, 1, 1, 13, layout),
        day,
        titleFont,
        palette.cellBackground,
        palette,
        allBorders(),
        heavyBorderWidth
        );

    drawCell(
        painter,
        sourceCellRect(TimeRow, 1, 1, 1, layout),
        QObject::tr("Time"),
        labelFont,
        palette.timeBackground,
        palette,
        topVerticalBorders(),
        heavyBorderWidth
        );

    for (int index = 0; index < SlotColumns.size(); ++index)
    {
        const int column =
            SlotColumns.at(index);
        drawCell(
            painter,
            sourceCellRect(TimeRow, column, 1, 2, layout),
            TimeLabels.at(index),
            labelFont,
            palette.timeBackground,
            palette,
            topVerticalBorders(),
            heavyBorderWidth
            );
    }

    drawCell(
        painter,
        sourceCellRect(LevelRow, 1, 1, 1, layout),
        QObject::tr("Level"),
        labelFont,
        palette.classInfoBackground,
        palette,
        verticalBorders(),
        heavyBorderWidth
        );
    drawCell(
        painter,
        sourceCellRect(TeacherRoomRow, 1, 1, 1, layout),
        QObject::tr("KT / Rm"),
        labelFont,
        palette.classInfoBackground,
        palette,
        bottomVerticalBorders(),
        heavyBorderWidth
        );

    for (int column : SlotColumns)
    {
        drawCell(
            painter,
            sourceCellRect(LevelRow, column, 1, 2, layout),
            cellValue(values, LevelRow, column),
            labelFont,
            palette.classInfoBackground,
            palette,
            verticalBorders(),
            heavyBorderWidth
            );
        drawCell(
            painter,
            sourceCellRect(TeacherRoomRow, column, 1, 2, layout),
            cellValue(values, TeacherRoomRow, column),
            labelFont,
            palette.classInfoBackground,
            palette,
            bottomVerticalBorders(),
            heavyBorderWidth
            );
    }

    for (int row = FirstStudentRow; row <= LastStudentRow; ++row)
    {
        const bool lastStudentRow =
            row == LastStudentRow;

        drawCell(
            painter,
            sourceCellRect(row, 1, 1, 1, layout),
            QString::number(row - FirstStudentRow + 1),
            bodyFont,
            palette.cellBackground,
            palette,
            lastStudentRow ? bottomVerticalBorders() : verticalBorders(),
            heavyBorderWidth
            );

        for (int column = 2; column <= ColumnCount; ++column)
        {
            const bool koreanColumn =
                (column % 2) == 1;
            const CellBorders borders =
                koreanColumn
                    ? (lastStudentRow ? rightBottomBorder() : rightBorder())
                    : (lastStudentRow ? leftBottomBorder() : leftBorder());

            drawCell(
                painter,
                sourceCellRect(row, column, 1, 1, layout),
                cellValue(values, row, column),
                koreanColumn ? koreanFont : bodyFont,
                palette.cellBackground,
                palette,
                borders,
                heavyBorderWidth,
                Qt::AlignCenter
                );
        }
    }

    const QList<QPair<int, QString>> footerRows{
        {WifiRow, QObject::tr("Wi-Fi")},
        {WifiPasswordRow, QObject::tr("Wi-Fi PW")},
        {ZoomRow, QObject::tr("Zoom")},
        {ZoomPasswordRow, QObject::tr("Zoom PW")}
    };

    for (const auto& rowLabel : footerRows)
    {
        drawCell(
            painter,
            sourceCellRect(rowLabel.first, 1, 1, 1, layout),
            rowLabel.second,
            labelFont,
            palette.footerBackground,
            palette,
            rowLabel.first == WifiRow
                ? topVerticalBorders()
                : rowLabel.first == ZoomPasswordRow
                    ? bottomVerticalBorders()
                    : verticalBorders(),
            heavyBorderWidth
            );

        for (int column : SlotColumns)
        {
            drawCell(
                painter,
                sourceCellRect(rowLabel.first, column, 1, 2, layout),
                cellValue(values, rowLabel.first, column),
                bodyFont,
                palette.footerBackground,
                palette,
                rowLabel.first == WifiRow
                    ? topVerticalBorders()
                    : rowLabel.first == ZoomPasswordRow
                        ? bottomVerticalBorders()
                        : verticalBorders(),
                heavyBorderWidth
                );
        }
    }

    painter.restore();
}

QStringList daysWithValues(
    const QList<RosterCellValue>& values
    )
{
    QSet<QString> included;

    for (const RosterCellValue& value : values)
    {
        if (DaySheets.contains(value.day))
        {
            included.insert(value.day);
        }
    }

    QStringList days;
    for (const QString& day : DaySheets)
    {
        if (included.contains(day))
        {
            days.append(day);
        }
    }

    return days;
}

QList<RosterClassData> loadRosterClassData(
    DataService* dataService,
    const QList<int>& classIds
    )
{
    QList<RosterClassData> result;

    if (!dataService)
    {
        return result;
    }

    for (int classId : classIds)
    {
        RosterClassData data;
        data.classroom =
            dataService->getClassById(classId);
        data.info =
            dataService->loadClassInfo(classId);
        data.roster =
            dataService->loadRoster(classId);

        if (data.classroom.id > 0)
        {
            result.append(data);
        }
    }

    return result;
}
} // namespace

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

Result saveRostersPdf(
    const QList<RosterClassData>& classes,
    const QString& documentPath
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
        buildByDayCellValues(
            classes,
            &errorMessage
            );

    if (!errorMessage.isEmpty())
    {
        return failed(errorMessage);
    }

    const QStringList days =
        daysWithValues(values);

    if (days.isEmpty())
    {
        return failed(
            QObject::tr("No selected classes match the roster print layout.")
            );
    }

    QPdfWriter writer(documentPath);
    if (!configureRosterPdfWriter(writer))
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
        rosterPdfContentRect(
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
        paintRosterDay(
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

Result printRosters(
    const Request& request
    )
{
    if (!request.services || !request.services->hasOpenDatabase())
    {
        return failed(QObject::tr("No database is open."));
    }

    DataService* dataService =
        request.services->dataService();

    if (!dataService)
    {
        return failed(QObject::tr("Roster data is not available."));
    }

    const QList<Classroom> classes =
        dataService->getClasses();
    const QList<int> classIds =
        resolveClassIds(
            request.scope,
            request.currentClassId,
            request.selectedClassIds,
            classes
            );

    if (classIds.isEmpty())
    {
        return failed(QObject::tr("No classes were selected for printing."));
    }

    const QList<RosterClassData> rosterClasses =
        loadRosterClassData(dataService, classIds);

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
                pdfPath
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
                QPageLayout::Landscape,
                false,
                RosterPdfPageSize,
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
