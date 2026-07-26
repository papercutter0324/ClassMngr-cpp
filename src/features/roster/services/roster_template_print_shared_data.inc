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
constexpr int PerClassIndexColumn = 1;
constexpr int PerClassEnglishColumn = 2;
constexpr int PerClassKoreanColumn = 3;
constexpr int PerClassFirstExtraColumn = 4;
constexpr int PerClassPortraitMaxExtraColumns = 4;
constexpr int PerClassLandscapeMaxExtraColumns = 8;
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
constexpr char PerClassPageKeySeparator = '|';

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
        return QStringLiteral("Per Class with Extra Info");

    case TemplateId::Daily:
        return QStringLiteral("Daily");

    case TemplateId::ByDay:
    default:
        return QStringLiteral("By Day");
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
