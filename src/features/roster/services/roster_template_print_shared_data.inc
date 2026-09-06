// Shared data, layout, and PDF configuration helpers.

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

constexpr qreal ColumnWidthInches = 0.6833;
constexpr qreal TitleRowHeightInches = 0.3000;
constexpr qreal NormalRowHeightInches = 0.2000;
constexpr qreal SectionEndRowHeightInches = 0.2083;
constexpr qreal RosterPdfMarginInches = 0.5;
constexpr QPageSize::PageSizeId RosterPdfPageSize = QPageSize::A4;

constexpr qreal A4WidthInches = 8.268;
constexpr qreal A4HeightInches = 11.693;

constexpr int DailyTitleRow = 1;
constexpr int DailyFirstSectionRow = 3;
constexpr int DailyRowsPerSection = 7;
constexpr int DailyHeaderColumn = 1;
constexpr int DailyFirstStudentColumn = 2;
constexpr int DailyStudentColumnCount = 5;
constexpr int DailyStudentRowCount = 5;
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
constexpr int PerClassIndexColumn = 1;
constexpr int PerClassEnglishColumn = 2;
constexpr int PerClassKoreanColumn = 3;
constexpr int PerClassFirstExtraColumn = 4;
constexpr int PerClassStudentRowCount = 23;
constexpr qreal PerClassMarginHorizontalInches = 0.5000;
constexpr qreal PerClassMarginVerticalInches = 0.5000;
constexpr qreal PerClassInfoRowHeightInches = 0.2700;
constexpr qreal PerClassInfoTableGapInches = 0.2500;
constexpr qreal PerClassHeaderRowHeightInches = 0.2800;
constexpr qreal PerClassStudentRowHeightInches = 0.2300;
constexpr qreal PerClassIndexColumnWidthInches = 0.3500;
constexpr qreal PerClassExtraColumnWidthInches = 0.9700;
constexpr qreal PerClassCellPaddingInches = 0.0350;
constexpr qreal PerClassVerticalCellPaddingInches = 0.0150;
constexpr qreal PerClassBorderWidthInches = 0.0060;

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

QString templateTitle(
    TemplateId templateId
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        return QStringLiteral("Per Class with Extra Info");

    case TemplateId::Daily:
        return QStringLiteral("Daily");

    case TemplateId::ByDay:
    default:
        return QStringLiteral("By Day");
    }
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
