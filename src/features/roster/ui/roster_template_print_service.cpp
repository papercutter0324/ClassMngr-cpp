#include "features/roster/ui/roster_template_print_service.h"

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

constexpr qreal A4WidthInches = 8.268;
constexpr qreal A4HeightInches = 11.693;

constexpr int RegisterTimeRowTop = 2;
constexpr int RegisterWifiRowTop = 3;
constexpr int RegisterZoomRowTop = 4;
constexpr int RegisterNameHeaderRowTop = 5;
constexpr int RegisterFirstStudentRowTop = 6;
constexpr int RegisterLastStudentRowTop = 27;
constexpr int RegisterTimeRowBottom = 28;
constexpr int RegisterWifiRowBottom = 29;
constexpr int RegisterZoomRowBottom = 30;
constexpr int RegisterNameHeaderRowBottom = 31;
constexpr int RegisterFirstStudentRowBottom = 32;
constexpr int RegisterLastStudentRowBottom = 53;
constexpr int RegisterRowCount = 53;
constexpr int RegisterColumnCount = 15;
constexpr int RegisterMaxStudentsPerClass =
    RegisterLastStudentRowTop - RegisterFirstStudentRowTop + 1;
constexpr qreal RegisterMarginHorizontalInches = 0.3902;
constexpr qreal RegisterMarginVerticalInches = 0.2298;

constexpr int DailyTitleRow = 1;
constexpr int DailyFirstSectionRow = 3;
constexpr int DailyRowsPerSection = 7;
constexpr int DailyHeaderColumn = 1;
constexpr int DailyFirstStudentColumn = 2;
constexpr int DailyStudentColumnCount = 5;
constexpr int DailyStudentRowCount = 5;
constexpr int DailyMaxStudentsPerClass =
    DailyStudentColumnCount * DailyStudentRowCount;
constexpr int DailySectionsPerPage = 6;
constexpr qreal DailyMarginHorizontalInches = 0.2200;
constexpr qreal DailyMarginVerticalInches = 0.1800;
constexpr qreal DailyTitleHeightInches = 0.4300;
constexpr qreal DailyTitleGapInches = 0.2100;
constexpr qreal DailyHeaderHeightInches = 0.3100;
constexpr qreal DailyStudentRowHeightInches = 0.2400;
constexpr qreal DailySectionGapInches = 0.2450;
constexpr qreal DailyRowMarkerWidthInches = 0.2600;
constexpr qreal DailyColumnGapInches = 0.1400;
constexpr qreal DailyCheckboxWidthInches = 0.2450;
constexpr qreal DailyCheckboxHeightInches = 0.1450;
constexpr qreal DailyNameGapInches = 0.0650;
constexpr char DailyPageKeySeparator = '|';

constexpr int PerClassHeaderRow = 5;
constexpr int PerClassFirstStudentRow = 6;
constexpr int PerClassEnglishColumn = 1;
constexpr int PerClassKoreanColumn = 2;
constexpr int PerClassFirstExtraColumn = 3;
constexpr int PerClassPortraitMaxExtraColumns = 3;
constexpr int PerClassLandscapeMaxExtraColumns = 6;
constexpr int PerClassStudentRowCount = 25;
constexpr qreal PerClassMarginHorizontalInches = 0.7481;
constexpr qreal PerClassMarginVerticalInches = 0.4000;
constexpr qreal PerClassInfoRowHeightInches = 0.2700;
constexpr qreal PerClassHeaderRowHeightInches = 0.3000;
constexpr qreal PerClassStudentRowHeightInches = 0.2500;
constexpr qreal PerClassNameColumnWidthInches = 1.4000;
constexpr qreal PerClassExtraColumnWidthInches = 0.9700;
constexpr qreal PerClassCellPaddingInches = 0.0350;
constexpr qreal PerClassBorderWidthInches = 0.0060;
constexpr char PerClassPageKeySeparator = '|';

const QStringList DaySheets{
    QStringLiteral("Monday"),
    QStringLiteral("Tuesday"),
    QStringLiteral("Wednesday"),
    QStringLiteral("Thursday"),
    QStringLiteral("Friday")
};

const QList<int> SlotColumns{2, 4, 6, 8, 10, 12};

const QList<int> RegisterSlotStartColumns{1, 6, 11, 1, 6, 11};

const QStringList TimeLabels{
    QStringLiteral("4pm"),
    QStringLiteral("5pm"),
    QStringLiteral("6pm"),
    QStringLiteral("7pm"),
    QStringLiteral("8pm"),
    QStringLiteral("9pm")
};

const QList<qreal> RegisterColumnWidthInches{
    0.2340,
    0.3833,
    0.6173,
    0.6173,
    0.6173,
    0.2645,
    0.3527,
    0.6173,
    0.6173,
    0.6180,
    0.2319,
    0.3854,
    0.6173,
    0.6173,
    0.6180
};

constexpr qreal RegisterBorderHeavyInches = 0.0312;
constexpr qreal RegisterBorderMediumInches = 0.0208;
constexpr qreal RegisterBorderInnerInches = 0.0104;
constexpr qreal RegisterBorderThinInches = 0.0069;
constexpr qreal RegisterBorderHairlineInches = 0.0034;
constexpr qreal RegisterCellPaddingInches = 0.0201;

const QColor RegisterHeaderTextColor(QStringLiteral("#0070C0"));
const QColor RegisterHeaderFillColor(QStringLiteral("#D9D9D9"));

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

struct CellBorderWidths
{
    qreal top = 0.0;
    qreal right = 0.0;
    qreal bottom = 0.0;
    qreal left = 0.0;
};

struct RosterPrintLayout
{
    QVector<qreal> columnWidths;
    QVector<qreal> rowHeights;
};

struct DailyClassSection
{
    RosterClassData data;
    ClassTime time;
    int inputIndex = 0;
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

QString templateTitle(
    TemplateId templateId
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        return QObject::tr("Per Class with Extra Info");

    case TemplateId::Daily:
        return QObject::tr("Daily");

    case TemplateId::ClassRegister:
        return QObject::tr("Class Register");

    case TemplateId::ByDay:
    default:
        return QObject::tr("By Day");
    }
}

QTime parsedStartTime(
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

    return time;
}

QString dailyPageKey(
    const QString& day,
    int pageIndex
    )
{
    return pageIndex <= 0
        ? day
        : QStringLiteral("%1%2%3")
            .arg(day)
            .arg(QLatin1Char(DailyPageKeySeparator))
            .arg(pageIndex);
}

QString baseDayName(
    const QString& day
    )
{
    const int separatorIndex =
        day.indexOf(QLatin1Char(DailyPageKeySeparator));

    return separatorIndex < 0
        ? day
        : day.left(separatorIndex);
}

int dailyPageIndex(
    const QString& day
    )
{
    const int separatorIndex =
        day.indexOf(QLatin1Char(DailyPageKeySeparator));

    if (separatorIndex < 0)
    {
        return 0;
    }

    bool ok = false;
    const int pageIndex =
        day.mid(separatorIndex + 1).toInt(&ok);

    return ok
        ? pageIndex
        : 0;
}

QString dailyTimeLabel(
    const QString& startTime
    )
{
    const QTime time =
        parsedStartTime(startTime);

    if (!time.isValid())
    {
        return startTime.trimmed();
    }

    const int hour =
        time.hour() % 12 == 0
            ? 12
            : time.hour() % 12;
    const QString suffix =
        time.hour() < 12
            ? QStringLiteral("a.m.")
            : QStringLiteral("p.m.");

    if (time.minute() == 0)
    {
        return QStringLiteral("%1 %2")
            .arg(hour)
            .arg(suffix);
    }

    return QStringLiteral("%1:%2 %3")
        .arg(hour)
        .arg(time.minute(), 2, 10, QLatin1Char('0'))
        .arg(suffix);
}

int columnForStartTime(
    const QString& startTime
    )
{
    const QTime time =
        parsedStartTime(startTime);

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

QString teacherLabel(
    const ClassInfo& info
    )
{
    const QString teacher =
        info.teacherEn.trimmed();

    return teacher.isEmpty()
        ? info.teacherKr.trimmed()
        : teacher;
}

QString fallbackText(
    const QString& fallback,
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        ? fallback
        : trimmed;
}

QString dailyClassHeader(
    const RosterClassData& data,
    const ClassTime& time
    )
{
    const QString teacher =
        fallbackText(
            QStringLiteral("N/A"),
            teacherLabel(data.info)
            );
    const QString room =
        fallbackText(
            QStringLiteral("N/A"),
            data.info.roomNumber
            );
    const QString zoom =
        fallbackText(
            QStringLiteral("N/A"),
            data.info.zoomId
            );
    const QString password =
        data.info.zoomPassword.trimmed();

    QString zoomLabel =
        QStringLiteral("Zoom: %1")
            .arg(zoom);

    if (!password.isEmpty())
    {
        zoomLabel +=
            QStringLiteral(" ")
            + QString(QChar(0x2022))
            + QStringLiteral(" PW %1")
                .arg(password);
    }

    return QStringLiteral("%1 (%2 / %3 / Room %4 / %5)")
        .arg(
            classLabel(data),
            dailyTimeLabel(time.startTime),
            teacher,
            room,
            zoomLabel
            );
}

QString dailyStudentName(
    const QStringList& row,
    int englishColumn,
    int koreanColumn
    )
{
    const QString english =
        rosterCell(
            row,
            englishColumn
            );

    return english.isEmpty()
        ? rosterCell(
            row,
            koreanColumn
            )
        : english;
}

QString perClassPageKey(
    int pageIndex,
    const RosterClassData& data
    )
{
    return QStringLiteral("class%1%2%3")
        .arg(QLatin1Char(PerClassPageKeySeparator))
        .arg(pageIndex)
        .arg(
            data.classroom.id > 0
                ? QStringLiteral("%1%2")
                    .arg(QLatin1Char(PerClassPageKeySeparator))
                    .arg(data.classroom.id)
                : QString()
            );
}

bool containsColumnName(
    const QStringList& columns,
    const QString& name
    )
{
    for (const QString& column : columns)
    {
        if (column.compare(name, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }

    return false;
}

bool isPerClassExtraInfoColumn(
    const QString& name
    )
{
    const QString trimmed =
        name.trimmed();

    return !trimmed.isEmpty()
        && trimmed.compare(QStringLiteral("English"), Qt::CaseInsensitive) != 0
        && trimmed.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) != 0
        && trimmed.compare(QStringLiteral("Winter"), Qt::CaseInsensitive) != 0
        && trimmed.compare(QStringLiteral("Speech Contest"), Qt::CaseInsensitive) != 0
        && trimmed.compare(QStringLiteral("Summer"), Qt::CaseInsensitive) != 0
        && trimmed.compare(QStringLiteral("Fall"), Qt::CaseInsensitive) != 0
        && trimmed.compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) != 0;
}

QStringList limitedPerClassExtraColumns(
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation orientation
    )
{
    QStringList columns;
    const int maxColumns =
        perClassExtraInfoMaxExtraColumns(orientation);

    for (const QString& column : selectedExtraColumns)
    {
        const QString trimmed =
            column.trimmed();

        if (
            !isPerClassExtraInfoColumn(trimmed)
            || containsColumnName(columns, trimmed)
            )
        {
            continue;
        }

        columns.append(trimmed);

        if (columns.size() >= maxColumns)
        {
            break;
        }
    }

    return columns;
}

QStringList perClassTimeLabels(
    const QList<ClassTime>& classTimes
    )
{
    QStringList labels;

    for (const ClassTime& time : classTimes)
    {
        const QString day =
            time.day.trimmed();
        const QString timeLabel =
            dailyTimeLabel(time.startTime);

        if (day.isEmpty() && timeLabel.isEmpty())
        {
            continue;
        }

        if (day.isEmpty())
        {
            labels.append(timeLabel);
        }
        else if (timeLabel.isEmpty())
        {
            labels.append(day);
        }
        else
        {
            labels.append(
                QStringLiteral("%1 - %2")
                    .arg(
                        day.left(3),
                        timeLabel
                        )
                );
        }
    }

    return labels;
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

QMarginsF rosterPdfMargins(
    TemplateId templateId
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        return QMarginsF(
            0.0,
            0.0,
            0.0,
            0.0
            );

    case TemplateId::Daily:
        return QMarginsF(
            0.0,
            0.0,
            0.0,
            0.0
            );

    case TemplateId::ClassRegister:
        return QMarginsF(
            0.0,
            0.0,
            0.0,
            0.0
            );

    case TemplateId::ByDay:
    default:
        return QMarginsF(
            0.0,
            0.0,
            0.0,
            0.0
            );
    }
}

QPageLayout rosterPdfPageLayout(
    TemplateId templateId,
    QPageLayout::Orientation orientation
    )
{
    return QPageLayout(
        QPageSize(templatePageSize(templateId)),
        orientation,
        rosterPdfMargins(templateId),
        QPageLayout::Inch
        );
}

QPageLayout rosterPdfPageLayout(
    TemplateId templateId
    )
{
    return rosterPdfPageLayout(
        templateId,
        templateOrientation(templateId)
        );
}

int registerSlotIndexForStartTime(
    const QString& startTime
    )
{
    const int byDayColumn =
        columnForStartTime(startTime);

    if (byDayColumn < 0)
    {
        return -1;
    }

    return (byDayColumn - 2) / 2;
}

int registerSlotStartColumn(
    int slotIndex
    )
{
    if (slotIndex < 0 || slotIndex >= RegisterSlotStartColumns.size())
    {
        return -1;
    }

    return RegisterSlotStartColumns.at(slotIndex);
}

int registerTimeRow(
    int slotIndex
    )
{
    return slotIndex < 3
        ? RegisterTimeRowTop
        : RegisterTimeRowBottom;
}

int registerWifiRow(
    int slotIndex
    )
{
    return slotIndex < 3
        ? RegisterWifiRowTop
        : RegisterWifiRowBottom;
}

int registerZoomRow(
    int slotIndex
    )
{
    return slotIndex < 3
        ? RegisterZoomRowTop
        : RegisterZoomRowBottom;
}

int registerNameHeaderRow(
    int slotIndex
    )
{
    return slotIndex < 3
        ? RegisterNameHeaderRowTop
        : RegisterNameHeaderRowBottom;
}

int registerFirstStudentRow(
    int slotIndex
    )
{
    return slotIndex < 3
        ? RegisterFirstStudentRowTop
        : RegisterFirstStudentRowBottom;
}

bool configureRosterPdfWriter(
    QPdfWriter& writer,
    TemplateId templateId,
    QPageLayout::Orientation orientation
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
        rosterPdfPageLayout(
            templateId,
            orientation
            )
        );
}

bool configureRosterPdfWriter(
    QPdfWriter& writer,
    TemplateId templateId
    )
{
    return configureRosterPdfWriter(
        writer,
        templateId,
        templateOrientation(templateId)
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

QRectF registerPdfContentRect(
    const QRectF& pageRect,
    int resolutionDpi
    )
{
    const qreal horizontalMargin =
        RegisterMarginHorizontalInches
        * std::max(
            1,
            resolutionDpi
            );
    const qreal verticalMargin =
        RegisterMarginVerticalInches
        * std::max(
            1,
            resolutionDpi
            );

    return pageRect.adjusted(
        horizontalMargin,
        verticalMargin,
        -horizontalMargin,
        -verticalMargin
        );
}

QRectF dailyPdfContentRect(
    const QRectF& pageRect,
    int resolutionDpi
    )
{
    const qreal horizontalMargin =
        DailyMarginHorizontalInches
        * std::max(
            1,
            resolutionDpi
            );
    const qreal verticalMargin =
        DailyMarginVerticalInches
        * std::max(
            1,
            resolutionDpi
            );

    return pageRect.adjusted(
        horizontalMargin,
        verticalMargin,
        -horizontalMargin,
        -verticalMargin
        );
}

QRectF perClassPdfContentRect(
    const QRectF& pageRect,
    int resolutionDpi
    )
{
    const qreal horizontalMargin =
        PerClassMarginHorizontalInches
        * std::max(
            1,
            resolutionDpi
            );
    const qreal verticalMargin =
        PerClassMarginVerticalInches
        * std::max(
            1,
            resolutionDpi
            );

    return pageRect.adjusted(
        horizontalMargin,
        verticalMargin,
        -horizontalMargin,
        -verticalMargin
        );
}

QRectF templateContentRect(
    TemplateId templateId,
    const QRectF& pageRect,
    int resolutionDpi
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        return perClassPdfContentRect(
            pageRect,
            resolutionDpi
            );

    case TemplateId::Daily:
        return dailyPdfContentRect(
            pageRect,
            resolutionDpi
            );

    case TemplateId::ClassRegister:
        return registerPdfContentRect(
            pageRect,
            resolutionDpi
            );

    case TemplateId::ByDay:
    default:
        return rosterPdfContentRect(
            pageRect,
            resolutionDpi
            );
    }
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

qreal registerBaseRowHeight(
    int row,
    int resolutionDpi
    )
{
    qreal inches = 0.2000;

    if (row == DayTitleRow)
    {
        inches = 0.2798;
    }
    else if (row == RegisterTimeRowTop)
    {
        inches = 0.0715;
    }
    else if (row == 8)
    {
        inches = 0.2111;
    }

    return inches
        * std::max(
            1,
            resolutionDpi
            );
}

bool isAutoRegisterRow(
    int row
    )
{
    return row == RegisterWifiRowTop
        || row == RegisterZoomRowTop
        || row == RegisterNameHeaderRowTop
        || row == RegisterTimeRowBottom
        || row == RegisterWifiRowBottom
        || row == RegisterZoomRowBottom
        || row == RegisterNameHeaderRowBottom;
}

RosterPrintLayout registerPrintLayout(
    const QRectF& contentRect,
    int resolutionDpi
    )
{
    RosterPrintLayout layout;
    layout.columnWidths.reserve(RegisterColumnCount);
    layout.rowHeights.reserve(RegisterRowCount);

    qreal baseTableWidth = 0.0;
    for (qreal inches : RegisterColumnWidthInches)
    {
        baseTableWidth +=
            inches
            * std::max(
                1,
                resolutionDpi
                );
    }

    const qreal widthScale =
        baseTableWidth > 0.0 && baseTableWidth > contentRect.width()
            ? contentRect.width() / baseTableWidth
            : 1.0;

    for (qreal inches : RegisterColumnWidthInches)
    {
        layout.columnWidths.append(
            inches
            * std::max(
                1,
                resolutionDpi
                )
            * widthScale
            );
    }

    qreal explicitTableHeight = 0.0;
    int autoRowCount = 0;
    for (int row = 1; row <= RegisterRowCount; ++row)
    {
        if (isAutoRegisterRow(row))
        {
            ++autoRowCount;
            continue;
        }

        explicitTableHeight +=
            registerBaseRowHeight(
                row,
                resolutionDpi
                );
    }

    const qreal fallbackAutoRowHeight =
        registerBaseRowHeight(
            RegisterFirstStudentRowTop,
            resolutionDpi
            );
    const qreal autoRowHeight =
        autoRowCount > 0
            ? std::max(
                fallbackAutoRowHeight,
                (contentRect.height() - explicitTableHeight) / autoRowCount
                )
            : fallbackAutoRowHeight;

    for (int row = 1; row <= RegisterRowCount; ++row)
    {
        layout.rowHeights.append(
            isAutoRegisterRow(row)
                ? autoRowHeight
                : registerBaseRowHeight(
                    row,
                    resolutionDpi
                    )
            );
    }

    return layout;
}

qreal layoutWidth(
    const RosterPrintLayout& layout
    )
{
    qreal width = 0.0;

    for (qreal columnWidth : layout.columnWidths)
    {
        width += columnWidth;
    }

    return width;
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

QFont printRegisterFont(
    qreal pointSize,
    int resolutionDpi,
    int weight = QFont::Normal
    )
{
    QFont font(QStringLiteral("Malgun Gothic"));
    font.setWeight(static_cast<QFont::Weight>(weight));
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

qreal dailyInches(
    qreal inches,
    int resolutionDpi
    )
{
    return inches
        * std::max(
            1,
            resolutionDpi
            );
}

qreal dailySectionTop(
    int sectionIndex,
    int resolutionDpi
    )
{
    return dailyInches(
        DailyTitleHeightInches
            + DailyTitleGapInches
            + (DailyHeaderHeightInches
               + (DailyStudentRowHeightInches * DailyStudentRowCount)
               + DailySectionGapInches)
                * sectionIndex,
        resolutionDpi
        );
}

void drawDailyText(
    QPainter& painter,
    const QRectF& rect,
    const QString& text,
    const QFont& font,
    const QColor& color,
    int alignment,
    qreal horizontalPadding = 0.0
    )
{
    const QString trimmed =
        text.trimmed();

    if (trimmed.isEmpty())
    {
        return;
    }

    painter.save();

    const QRectF textRect =
        rect.adjusted(
            horizontalPadding,
            0.0,
            -horizontalPadding,
            0.0
            );
    QFont displayFont =
        fittedFont(
            trimmed,
            font,
            textRect
            );
    QFontMetricsF metrics(displayFont);
    QString displayText =
        trimmed;

    if (metrics.horizontalAdvance(displayText) > textRect.width())
    {
        displayText =
            metrics.elidedText(
                displayText,
                Qt::ElideRight,
                textRect.width()
                );
    }

    painter.setPen(color);
    painter.setFont(displayFont);
    painter.drawText(
        textRect,
        alignment,
        displayText
        );

    painter.restore();
}

void drawDailyCheckbox(
    QPainter& painter,
    const QRectF& rect,
    int resolutionDpi
    )
{
    painter.save();
    QPen pen(QColor(QStringLiteral("#17325C")));
    pen.setWidthF(
        std::max(
            1.0,
            dailyInches(
                0.0045,
                resolutionDpi
                )
            )
        );
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);
    painter.restore();
}

QString dailyHeaderLevelText(
    const QString& header
    )
{
    const QString trimmed =
        header.trimmed();
    const int detailsIndex =
        trimmed.indexOf(QStringLiteral(" ("));

    return detailsIndex < 0
        ? trimmed
        : trimmed.left(detailsIndex);
}

QString dailyHeaderDetailsText(
    const QString& header,
    const QString& level
    )
{
    return header.trimmed().mid(level.size()).trimmed();
}

void drawDailyHeaderText(
    QPainter& painter,
    const QRectF& rect,
    const QString& header,
    const QFont& levelFont,
    const QFont& detailsFont,
    const QColor& color
    )
{
    const QString level =
        dailyHeaderLevelText(header);

    if (level.isEmpty())
    {
        return;
    }

    painter.save();

    QFont displayLevelFont =
        fittedFont(
            level,
            levelFont,
            rect
            );
    QFontMetricsF levelMetrics(displayLevelFont);
    QString displayLevel =
        level;

    if (levelMetrics.horizontalAdvance(displayLevel) > rect.width())
    {
        displayLevel =
            levelMetrics.elidedText(
                displayLevel,
                Qt::ElideRight,
                rect.width()
                );
    }

    painter.setPen(color);
    painter.setFont(displayLevelFont);

    const qreal baseline =
        rect.center().y()
        + ((levelMetrics.ascent() - levelMetrics.descent()) / 2.0);
    painter.drawText(
        QPointF(
            rect.left(),
            baseline
            ),
        displayLevel
        );

    const QString details =
        dailyHeaderDetailsText(
            header,
            level
            );
    const qreal detailsLeft =
        rect.left()
        + levelMetrics.horizontalAdvance(displayLevel)
        + (details.isEmpty() ? 0.0 : levelMetrics.horizontalAdvance(QLatin1Char(' ')));
    const QRectF detailsRect(
        detailsLeft,
        rect.top(),
        std::max(
            0.0,
            rect.right() - detailsLeft
            ),
        rect.height()
        );

    if (!details.trimmed().isEmpty() && detailsRect.width() > 0.0)
    {
        drawDailyText(
            painter,
            detailsRect,
            details,
            detailsFont,
            color,
            Qt::AlignLeft | Qt::AlignVCenter
            );
    }

    painter.restore();
}

void paintDailySection(
    QPainter& painter,
    int sectionIndex,
    const QHash<QString, QString>& values,
    const QRectF& contentRect,
    int resolutionDpi
    )
{
    const int headerRow =
        DailyFirstSectionRow + (sectionIndex * DailyRowsPerSection);
    const QString header =
        cellValue(
            values,
            headerRow,
            DailyHeaderColumn
            );

    if (header.trimmed().isEmpty())
    {
        return;
    }

    const qreal top =
        contentRect.top()
        + dailySectionTop(
            sectionIndex,
            resolutionDpi
            );
    const qreal markerWidth =
        dailyInches(
            DailyRowMarkerWidthInches,
            resolutionDpi
            );
    const qreal columnGap =
        dailyInches(
            DailyColumnGapInches,
            resolutionDpi
            );
    const qreal headerHeight =
        dailyInches(
            DailyHeaderHeightInches,
            resolutionDpi
            );
    const qreal rowHeight =
        dailyInches(
            DailyStudentRowHeightInches,
            resolutionDpi
            );
    const qreal checkboxWidth =
        dailyInches(
            DailyCheckboxWidthInches,
            resolutionDpi
            );
    const qreal checkboxHeight =
        dailyInches(
            DailyCheckboxHeightInches,
            resolutionDpi
            );
    const qreal nameGap =
        dailyInches(
            DailyNameGapInches,
            resolutionDpi
            );
    const qreal nameAreaWidth =
        contentRect.width() - markerWidth;
    const qreal columnWidth =
        (nameAreaWidth - (columnGap * (DailyStudentColumnCount - 1)))
        / DailyStudentColumnCount;

    const QFont headerLevelFont =
        printKoreanFont(
            15.5,
            resolutionDpi,
            QFont::Bold
            );
    const QFont headerDetailsFont =
        printKoreanFont(
            11.5,
            resolutionDpi
            );
    const QFont nameFont =
        printKoreanFont(
            12.8,
            resolutionDpi
            );
    const QFont markerFont =
        printUiFont(
            7.8,
            resolutionDpi
            );
    const QColor markerColor(QStringLiteral("#7A7A7A"));

    drawDailyHeaderText(
        painter,
        QRectF(
            contentRect.left() + markerWidth,
            top,
            contentRect.width() - markerWidth,
            headerHeight
            ),
        header,
        headerLevelFont,
        headerDetailsFont,
        Qt::black
        );

    const qreal studentTop =
        top + headerHeight;

    for (int row = 0; row < DailyStudentRowCount; ++row)
    {
        const qreal rowTop =
            studentTop + (row * rowHeight);
        drawDailyText(
            painter,
            QRectF(
                contentRect.left(),
                rowTop,
                markerWidth * 0.72,
                rowHeight
                ),
            QString::number((row * DailyStudentColumnCount) + 1),
            markerFont,
            markerColor,
            Qt::AlignRight | Qt::AlignVCenter
            );

        for (int column = 0; column < DailyStudentColumnCount; ++column)
        {
            const qreal cellLeft =
                contentRect.left()
                + markerWidth
                + (column * (columnWidth + columnGap));
            const QRectF checkboxRect(
                cellLeft,
                rowTop + ((rowHeight - checkboxHeight) / 2.0),
                checkboxWidth,
                checkboxHeight
                );
            const QRectF nameRect(
                checkboxRect.right() + nameGap,
                rowTop,
                columnWidth - checkboxWidth - nameGap,
                rowHeight
                );

            drawDailyCheckbox(
                painter,
                checkboxRect,
                resolutionDpi
                );
            drawDailyText(
                painter,
                nameRect,
                cellValue(
                    values,
                    headerRow + 1 + row,
                    DailyFirstStudentColumn + column
                    ),
                nameFont,
                Qt::black,
                Qt::AlignLeft | Qt::AlignVCenter
                );
        }
    }
}

void paintDailyDay(
    QPainter& painter,
    const QString& day,
    const QHash<QString, QString>& values,
    const QRectF& pageRect,
    const QRectF& contentRect,
    int resolutionDpi
    )
{
    const RosterPdfPalette palette;

    painter.fillRect(
        pageRect,
        palette.pageBackground
        );

    const QFont titleFont =
        printUiFont(
            17.0,
            resolutionDpi,
            QFont::Black
            );

    drawDailyText(
        painter,
        QRectF(
            contentRect.left(),
            contentRect.top(),
            contentRect.width(),
            dailyInches(
                DailyTitleHeightInches,
                resolutionDpi
                )
            ),
        baseDayName(day).toUpper(),
        titleFont,
        Qt::black,
        Qt::AlignLeft | Qt::AlignVCenter
        );

    for (int sectionIndex = 0;
         sectionIndex < DailySectionsPerPage;
         ++sectionIndex)
    {
        paintDailySection(
            painter,
            sectionIndex,
            values,
            contentRect,
            resolutionDpi
            );
    }
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

qreal registerBorderWidth(
    qreal inches,
    int resolutionDpi
    )
{
    return std::max(
        0.5,
        inches
        * std::max(
            1,
            resolutionDpi
            )
        );
}

CellBorderWidths registerBorders(
    qreal top,
    qreal right,
    qreal bottom,
    qreal left,
    int resolutionDpi
    )
{
    return {
        registerBorderWidth(
            top,
            resolutionDpi
            ),
        registerBorderWidth(
            right,
            resolutionDpi
            ),
        registerBorderWidth(
            bottom,
            resolutionDpi
            ),
        registerBorderWidth(
            left,
            resolutionDpi
            )
    };
}

void drawRegisterLine(
    QPainter& painter,
    const QPointF& start,
    const QPointF& end,
    qreal width,
    const QColor& color
    )
{
    if (width <= 0.0)
    {
        return;
    }

    painter.setPen(
        QPen(
            color,
            width,
            Qt::SolidLine,
            Qt::SquareCap,
            Qt::MiterJoin
            )
        );
    painter.drawLine(
        start,
        end
        );
}

void drawRegisterCellBorders(
    QPainter& painter,
    const QRectF& rect,
    const CellBorderWidths& borders,
    const QColor& color
    )
{
    drawRegisterLine(
        painter,
        QPointF(rect.left(), rect.top() + borders.top / 2.0),
        QPointF(rect.right(), rect.top() + borders.top / 2.0),
        borders.top,
        color
        );
    drawRegisterLine(
        painter,
        QPointF(rect.right() - borders.right / 2.0, rect.top()),
        QPointF(rect.right() - borders.right / 2.0, rect.bottom()),
        borders.right,
        color
        );
    drawRegisterLine(
        painter,
        QPointF(rect.left(), rect.bottom() - borders.bottom / 2.0),
        QPointF(rect.right(), rect.bottom() - borders.bottom / 2.0),
        borders.bottom,
        color
        );
    drawRegisterLine(
        painter,
        QPointF(rect.left() + borders.left / 2.0, rect.top()),
        QPointF(rect.left() + borders.left / 2.0, rect.bottom()),
        borders.left,
        color
        );
}

void drawRegisterCell(
    QPainter& painter,
    const QRectF& rect,
    const QString& text,
    const QFont& font,
    const QColor& fill,
    const QColor& textColor,
    const CellBorderWidths& borders,
    int resolutionDpi,
    int alignment = Qt::AlignCenter
    )
{
    painter.save();
    painter.fillRect(
        rect,
        fill
        );
    drawRegisterCellBorders(
        painter,
        rect,
        borders,
        Qt::black
        );

    if (!text.trimmed().isEmpty())
    {
        const qreal padding =
            RegisterCellPaddingInches
            * std::max(
                1,
                resolutionDpi
                );
        const QRectF textRect =
            rect.adjusted(
                padding,
                0.0,
                -padding,
                0.0
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

        painter.setPen(textColor);
        painter.setFont(displayFont);
        painter.drawText(
            textRect,
            alignment,
            displayText
            );
    }

    painter.restore();
}

QString labelValue(
    const QString& label,
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        ? label
        : QStringLiteral("%1 %2").arg(label, trimmed);
}

QString valueOrLabel(
    const QString& label,
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        ? label
        : trimmed;
}

CellBorderWidths registerBlockBorders(
    int columnOffset,
    qreal top,
    qreal bottom,
    qreal firstInner,
    qreal secondInner,
    int resolutionDpi
    )
{
    qreal left = firstInner;
    qreal right = secondInner;

    if (columnOffset == 0)
    {
        left =
            RegisterBorderHeavyInches;
    }

    if (columnOffset == 4)
    {
        right =
            RegisterBorderHeavyInches;
    }

    return registerBorders(
        top,
        right,
        bottom,
        left,
        resolutionDpi
        );
}

void paintRegisterBlock(
    QPainter& painter,
    int slotIndex,
    const QHash<QString, QString>& values,
    const RosterPrintLayout& layout,
    int resolutionDpi
    )
{
    const RosterPdfPalette palette;
    const int column =
        registerSlotStartColumn(slotIndex);

    if (column < 0)
    {
        return;
    }

    const int timeRow =
        registerTimeRow(slotIndex);
    const int wifiRow =
        registerWifiRow(slotIndex);
    const int zoomRow =
        registerZoomRow(slotIndex);
    const int nameHeaderRow =
        registerNameHeaderRow(slotIndex);
    const int firstStudentRow =
        registerFirstStudentRow(slotIndex);

    const QFont tinyLabelFont =
        printRegisterFont(
            8.0,
            resolutionDpi,
            QFont::Bold
            );
    const QFont labelFont =
        printRegisterFont(
            10.0,
            resolutionDpi
            );
    const QFont bodyFont =
        printRegisterFont(
            8.0,
            resolutionDpi
            );

    drawRegisterCell(
        painter,
        sourceCellRect(timeRow, column, 1, 2, layout),
        TimeLabels.value(slotIndex),
        tinyLabelFont,
        palette.cellBackground,
        RegisterHeaderTextColor,
        registerBorders(
            RegisterBorderHeavyInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            RegisterBorderHeavyInches,
            resolutionDpi
            ),
        resolutionDpi
        );
    drawRegisterCell(
        painter,
        sourceCellRect(timeRow, column + 2, 1, 1, layout),
        valueOrLabel(
            QObject::tr("Level"),
            cellValue(values, timeRow, column + 2)
            ),
        tinyLabelFont,
        palette.cellBackground,
        RegisterHeaderTextColor,
        registerBlockBorders(
            2,
            RegisterBorderHeavyInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            resolutionDpi
            ),
        resolutionDpi
        );
    drawRegisterCell(
        painter,
        sourceCellRect(timeRow, column + 3, 1, 1, layout),
        valueOrLabel(
            QObject::tr("KT"),
            cellValue(values, timeRow, column + 3)
            ),
        tinyLabelFont,
        palette.cellBackground,
        RegisterHeaderTextColor,
        registerBlockBorders(
            3,
            RegisterBorderHeavyInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            resolutionDpi
            ),
        resolutionDpi
        );
    drawRegisterCell(
        painter,
        sourceCellRect(timeRow, column + 4, 1, 1, layout),
        valueOrLabel(
            QObject::tr("Room"),
            cellValue(values, timeRow, column + 4)
            ),
        tinyLabelFont,
        palette.cellBackground,
        RegisterHeaderTextColor,
        registerBorders(
            RegisterBorderHeavyInches,
            RegisterBorderHeavyInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            resolutionDpi
            ),
        resolutionDpi
        );

    drawRegisterCell(
        painter,
        sourceCellRect(wifiRow, column, 1, 3, layout),
        labelValue(
            QObject::tr("Wifi:"),
            cellValue(values, wifiRow, column)
            ),
        labelFont,
        palette.cellBackground,
        palette.text,
        registerBorders(
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            RegisterBorderHeavyInches,
            resolutionDpi
            ),
        resolutionDpi,
        Qt::AlignVCenter | Qt::AlignLeft
        );
    drawRegisterCell(
        painter,
        sourceCellRect(wifiRow, column + 3, 1, 2, layout),
        labelValue(
            QObject::tr("PW:"),
            cellValue(values, wifiRow, column + 3)
            ),
        labelFont,
        palette.cellBackground,
        palette.text,
        registerBorders(
            RegisterBorderMediumInches,
            RegisterBorderHeavyInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            resolutionDpi
            ),
        resolutionDpi,
        Qt::AlignVCenter | Qt::AlignLeft
        );
    drawRegisterCell(
        painter,
        sourceCellRect(zoomRow, column, 1, 3, layout),
        labelValue(
            QObject::tr("Zoom:"),
            cellValue(values, zoomRow, column)
            ),
        labelFont,
        palette.cellBackground,
        palette.text,
        registerBorders(
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            RegisterBorderHeavyInches,
            resolutionDpi
            ),
        resolutionDpi,
        Qt::AlignVCenter | Qt::AlignLeft
        );
    drawRegisterCell(
        painter,
        sourceCellRect(zoomRow, column + 3, 1, 2, layout),
        labelValue(
            QObject::tr("PW:"),
            cellValue(values, zoomRow, column + 3)
            ),
        labelFont,
        palette.cellBackground,
        palette.text,
        registerBorders(
            RegisterBorderMediumInches,
            RegisterBorderHeavyInches,
            RegisterBorderMediumInches,
            RegisterBorderMediumInches,
            resolutionDpi
            ),
        resolutionDpi,
        Qt::AlignVCenter | Qt::AlignLeft
        );

    drawRegisterCell(
        painter,
        sourceCellRect(nameHeaderRow, column, 1, 3, layout),
        QObject::tr("English"),
        labelFont,
        RegisterHeaderFillColor,
        palette.text,
        registerBorders(
            RegisterBorderMediumInches,
            RegisterBorderHairlineInches,
            RegisterBorderHairlineInches,
            RegisterBorderHeavyInches,
            resolutionDpi
            ),
        resolutionDpi
        );
    drawRegisterCell(
        painter,
        sourceCellRect(nameHeaderRow, column + 3, 1, 2, layout),
        QObject::tr("Korean"),
        labelFont,
        RegisterHeaderFillColor,
        palette.text,
        registerBorders(
            RegisterBorderMediumInches,
            RegisterBorderHeavyInches,
            RegisterBorderHairlineInches,
            RegisterBorderHairlineInches,
            resolutionDpi
            ),
        resolutionDpi
        );

    for (int studentIndex = 0; studentIndex < RegisterMaxStudentsPerClass; ++studentIndex)
    {
        const int row =
            firstStudentRow + studentIndex;
        const bool firstStudent =
            studentIndex == 0;
        const bool lastStudent =
            studentIndex == RegisterMaxStudentsPerClass - 1;
        const qreal topBorder =
            firstStudent
                ? RegisterBorderHairlineInches
                : RegisterBorderThinInches;
        const qreal bottomBorder =
            lastStudent
                ? RegisterBorderHeavyInches
                : RegisterBorderThinInches;

        drawRegisterCell(
            painter,
            sourceCellRect(row, column, 1, 1, layout),
            QString::number(studentIndex + 1),
            bodyFont,
            palette.cellBackground,
            palette.text,
            registerBorders(
                topBorder,
                RegisterBorderHairlineInches,
                bottomBorder,
                RegisterBorderHeavyInches,
                resolutionDpi
                ),
            resolutionDpi
            );
        drawRegisterCell(
            painter,
            sourceCellRect(row, column + 1, 1, 2, layout),
            cellValue(values, row, column + 1),
            bodyFont,
            palette.cellBackground,
            palette.text,
            registerBorders(
                topBorder,
                RegisterBorderHairlineInches,
                bottomBorder,
                RegisterBorderHairlineInches,
                resolutionDpi
                ),
            resolutionDpi
            );
        drawRegisterCell(
            painter,
            sourceCellRect(row, column + 3, 1, 2, layout),
            cellValue(values, row, column + 3),
            bodyFont,
            palette.cellBackground,
            palette.text,
            registerBorders(
                topBorder,
                RegisterBorderHeavyInches,
                bottomBorder,
                RegisterBorderHairlineInches,
                resolutionDpi
                ),
            resolutionDpi
            );
    }
}

void paintClassRegisterDay(
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
        registerPrintLayout(
            contentRect,
            resolutionDpi
            );
    const qreal tableWidth =
        layoutWidth(layout);
    const QPointF tableOrigin(
        contentRect.left()
            + std::max(
                0.0,
                (contentRect.width() - tableWidth) / 2.0
                ),
        contentRect.top()
        );

    painter.fillRect(
        pageRect,
        palette.pageBackground
        );

    painter.save();
    painter.translate(tableOrigin);

    const QFont titleFont =
        printRegisterFont(
            10.0,
            resolutionDpi
            );

    drawRegisterCell(
        painter,
        sourceCellRect(DayTitleRow, 1, 1, RegisterColumnCount, layout),
        day.toUpper(),
        titleFont,
        palette.cellBackground,
        palette.text,
        registerBorders(
            RegisterBorderHeavyInches,
            RegisterBorderHeavyInches,
            RegisterBorderMediumInches,
            RegisterBorderHeavyInches,
            resolutionDpi
            ),
        resolutionDpi
        );

    for (int slotIndex = 0; slotIndex < TimeLabels.size(); ++slotIndex)
    {
        paintRegisterBlock(
            painter,
            slotIndex,
            values,
            layout,
            resolutionDpi
            );
    }

    painter.restore();
}

qreal perClassInches(
    qreal inches,
    int resolutionDpi
    )
{
    return inches
        * std::max(
            1,
            resolutionDpi
            );
}

int perClassStudentColumnCount(
    const QHash<QString, QString>& values
    )
{
    int columnCount =
        PerClassKoreanColumn;

    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
    {
        const QStringList parts =
            it.key().split(QLatin1Char(':'));

        if (parts.size() != 2)
        {
            continue;
        }

        bool rowOk = false;
        bool columnOk = false;
        const int row =
            parts.at(0).toInt(&rowOk);
        const int column =
            parts.at(1).toInt(&columnOk);

        if (
            rowOk
            && columnOk
            && row == PerClassHeaderRow
            && column > columnCount
            )
        {
            columnCount = column;
        }
    }

    return columnCount;
}

qreal perClassStudentColumnWidth(
    int column,
    int resolutionDpi
    )
{
    return perClassInches(
        column <= PerClassKoreanColumn
            ? PerClassNameColumnWidthInches
            : PerClassExtraColumnWidthInches,
        resolutionDpi
        );
}

qreal perClassTableWidth(
    int columnCount,
    int resolutionDpi
    )
{
    qreal width = 0.0;

    for (int column = 1; column <= columnCount; ++column)
    {
        width +=
            perClassStudentColumnWidth(
                column,
                resolutionDpi
                );
    }

    return width;
}

QRectF perClassStudentCellRect(
    const QRectF& contentRect,
    qreal tableTop,
    int row,
    int column,
    int resolutionDpi
    )
{
    qreal left =
        contentRect.left();

    for (int currentColumn = 1; currentColumn < column; ++currentColumn)
    {
        left +=
            perClassStudentColumnWidth(
                currentColumn,
                resolutionDpi
                );
    }

    const qreal rowHeight =
        row == PerClassHeaderRow
            ? perClassInches(
                PerClassHeaderRowHeightInches,
                resolutionDpi
                )
            : perClassInches(
                PerClassStudentRowHeightInches,
                resolutionDpi
                );
    const qreal top =
        tableTop
        + perClassInches(
            PerClassHeaderRowHeightInches,
            resolutionDpi
            )
        + ((row - PerClassFirstStudentRow)
           * perClassInches(
                PerClassStudentRowHeightInches,
                resolutionDpi
                ));

    return QRectF(
        left,
        row == PerClassHeaderRow
            ? tableTop
            : top,
        perClassStudentColumnWidth(
            column,
            resolutionDpi
            ),
        rowHeight
        );
}

void drawPerClassCell(
    QPainter& painter,
    const QRectF& rect,
    const QString& text,
    const QFont& font,
    const QColor& background,
    const QColor& textColor,
    int alignment,
    int resolutionDpi
    )
{
    painter.save();
    painter.fillRect(
        rect,
        background
        );

    QPen pen(QColor(QStringLiteral("#808080")));
    pen.setWidthF(
        std::max(
            1.0,
            perClassInches(
                PerClassBorderWidthInches,
                resolutionDpi
                )
            )
        );
    painter.setPen(pen);
    painter.drawRect(rect);

    if (!text.trimmed().isEmpty())
    {
        const qreal padding =
            perClassInches(
                PerClassCellPaddingInches,
                resolutionDpi
                );
        drawDailyText(
            painter,
            rect.adjusted(
                padding,
                0.0,
                -padding,
                0.0
                ),
            text,
            font,
            textColor,
            alignment
            );
    }

    painter.restore();
}

void paintPerClassWithExtraInfoPage(
    QPainter& painter,
    const QHash<QString, QString>& values,
    const QRectF& pageRect,
    const QRectF& contentRect,
    int resolutionDpi
    )
{
    const RosterPdfPalette palette;
    painter.fillRect(
        pageRect,
        palette.pageBackground
        );

    const int columnCount =
        perClassStudentColumnCount(values);
    const qreal tableWidth =
        std::min(
            contentRect.width(),
            perClassTableWidth(
                columnCount,
                resolutionDpi
                )
            );
    const qreal infoRowHeight =
        perClassInches(
            PerClassInfoRowHeightInches,
            resolutionDpi
            );
    const qreal tableTop =
        contentRect.top()
        + (infoRowHeight * 4.0);
    const qreal infoColumnWidth =
        tableWidth / 4.0;

    const QFont labelFont =
        printKoreanFont(
            9.5,
            resolutionDpi,
            QFont::Bold
            );
    const QFont valueFont =
        printKoreanFont(
            9.5,
            resolutionDpi
            );
    const QFont headerFont =
        printKoreanFont(
            9.2,
            resolutionDpi,
            QFont::Bold
            );
    const QFont studentFont =
        printKoreanFont(
            9.0,
            resolutionDpi
            );
    const QColor labelBackground(QStringLiteral("#DCE6F1"));
    const QColor headerBackground(QStringLiteral("#B8CCE4"));

    for (int row = 1; row <= 4; ++row)
    {
        for (int column = 1; column <= 4; ++column)
        {
            const QRectF rect(
                contentRect.left() + ((column - 1) * infoColumnWidth),
                contentRect.top() + ((row - 1) * infoRowHeight),
                infoColumnWidth,
                infoRowHeight
                );
            const bool labelColumn =
                column == 1 || column == 3;

            drawPerClassCell(
                painter,
                rect,
                cellValue(
                    values,
                    row,
                    column
                    ),
                labelColumn ? labelFont : valueFont,
                labelColumn ? labelBackground : Qt::white,
                Qt::black,
                labelColumn
                    ? Qt::AlignCenter
                    : (Qt::AlignLeft | Qt::AlignVCenter),
                resolutionDpi
                );
        }
    }

    for (int column = 1; column <= columnCount; ++column)
    {
        drawPerClassCell(
            painter,
            perClassStudentCellRect(
                contentRect,
                tableTop,
                PerClassHeaderRow,
                column,
                resolutionDpi
                ),
            cellValue(
                values,
                PerClassHeaderRow,
                column
                ),
            headerFont,
            headerBackground,
            Qt::black,
            Qt::AlignCenter,
            resolutionDpi
            );
    }

    for (int rowOffset = 0; rowOffset < PerClassStudentRowCount; ++rowOffset)
    {
        const int row =
            PerClassFirstStudentRow + rowOffset;

        for (int column = 1; column <= columnCount; ++column)
        {
            drawPerClassCell(
                painter,
                perClassStudentCellRect(
                    contentRect,
                    tableTop,
                    row,
                    column,
                    resolutionDpi
                    ),
                cellValue(
                    values,
                    row,
                    column
                    ),
                studentFont,
                Qt::white,
                Qt::black,
                Qt::AlignLeft | Qt::AlignVCenter,
                resolutionDpi
                );
        }
    }
}

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

Result loadRosterClassesForRequest(
    const Request& request,
    QList<RosterClassData>* rosterClasses
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

    if (rosterClasses)
    {
        *rosterClasses =
            loadRosterClassData(
                dataService,
                classIds
                );
    }

    return sent();
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

    case TemplateId::ClassRegister:
        return buildClassRegisterCellValues(
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

    case TemplateId::ClassRegister:
        paintClassRegisterDay(
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
} // namespace

QList<TemplateId> availableTemplateIds()
{
    return {
        TemplateId::ByDay,
        TemplateId::ClassRegister,
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

    case TemplateId::ClassRegister:
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
            rowIndex < data.roster.rows.size()
                && rowIndex < PerClassStudentRowCount;
            ++rowIndex
            )
        {
            const QStringList& row =
                data.roster.rows.at(rowIndex);
            const int outputRow =
                PerClassFirstStudentRow + rowIndex;

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

QList<RosterCellValue> buildClassRegisterCellValues(
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

            const int slotIndex =
                registerSlotIndexForStartTime(time.startTime);

            if (slotIndex < 0)
            {
                continue;
            }

            const QString slotKey =
                day + QLatin1Char('|') + QString::number(slotIndex);

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

            const int column =
                registerSlotStartColumn(slotIndex);
            const int timeRow =
                registerTimeRow(slotIndex);
            const int wifiRow =
                registerWifiRow(slotIndex);
            const int zoomRow =
                registerZoomRow(slotIndex);
            const int firstStudentRow =
                registerFirstStudentRow(slotIndex);

            appendCellValue(values, day, column + 2, timeRow, classLabel(data));
            appendCellValue(values, day, column + 3, timeRow, teacherLabel(data.info));
            appendCellValue(values, day, column + 4, timeRow, data.info.roomNumber);
            appendCellValue(values, day, column, wifiRow, data.info.wifiName);
            appendCellValue(values, day, column + 3, wifiRow, data.info.wifiPassword);
            appendCellValue(values, day, column, zoomRow, data.info.zoomId);
            appendCellValue(values, day, column + 3, zoomRow, data.info.zoomPassword);

            int writtenStudentCount = 0;
            for (const QStringList& row : data.roster.rows)
            {
                if (writtenStudentCount >= RegisterMaxStudentsPerClass)
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
                    firstStudentRow + writtenStudentCount;

                appendCellValue(values, day, column + 1, outputRow, english);
                appendCellValue(values, day, column + 3, outputRow, korean);
                ++writtenStudentCount;
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
