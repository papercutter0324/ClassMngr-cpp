// Per-class roster template rendering.


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

QVector<qreal> perClassStudentColumnWidths(
    const QHash<QString, QString>& values,
    int columnCount,
    int resolutionDpi
    )
{
    QVector<qreal> widths(columnCount + 1);
    widths[PerClassIndexColumn] =
        perClassInches(
            PerClassIndexColumnWidthInches,
            resolutionDpi
            );

    const QFont nameFont =
        printKoreanFont(
            10.0,
            resolutionDpi
            );
    const QFont headerFont =
        printKoreanFont(
            8.2,
            resolutionDpi,
            QFont::Bold
            );
    const QFontMetricsF nameMetrics(nameFont);
    const QFontMetricsF headerMetrics(headerFont);
    const qreal horizontalPadding =
        perClassInches(
            PerClassCellPaddingInches * 4.0,
            resolutionDpi
            );

    for (int column = PerClassEnglishColumn;
         column <= PerClassKoreanColumn;
         ++column)
    {
        qreal widestText = 0.0;

        for (int row = PerClassHeaderRow;
             row < PerClassFirstStudentRow + PerClassStudentRowCount;
             ++row)
        {
            const QString text = cellValue(values, row, column);
            widestText = std::max(
                widestText,
                row == PerClassHeaderRow
                    ? headerMetrics.horizontalAdvance(text)
                    : nameMetrics.horizontalAdvance(text)
                );
        }

        widths[column] = std::max(
            perClassInches(0.7500, resolutionDpi),
            widestText + horizontalPadding
            );
    }

    for (int column = PerClassFirstExtraColumn;
         column <= columnCount;
         ++column)
    {
        widths[column] =
            perClassInches(
                PerClassExtraColumnWidthInches,
                resolutionDpi
                );
    }

    return widths;
}

QVector<qreal> perClassInfoColumnWidths(
    const QHash<QString, QString>& values,
    int resolutionDpi
    )
{
    QVector<qreal> widths(5);
    const QFont labelFont =
        printKoreanFont(
            8.5,
            resolutionDpi,
            QFont::Bold
            );
    const QFont valueFont =
        printKoreanFont(
            8.5,
            resolutionDpi
            );
    const QFontMetricsF labelMetrics(labelFont);
    const QFontMetricsF valueMetrics(valueFont);
    const qreal horizontalPadding =
        perClassInches(
            PerClassCellPaddingInches * 4.0,
            resolutionDpi
            );

    for (int column = 1; column <= 4; ++column)
    {
        const QFontMetricsF& metrics =
            column == 1 || column == 3
                ? labelMetrics
                : valueMetrics;
        qreal widestText = 0.0;

        for (int row = 1; row <= 4; ++row)
        {
            widestText = std::max(
                widestText,
                metrics.horizontalAdvance(
                    cellValue(values, row, column)
                    )
                );
        }

        widths[column] = widestText + horizontalPadding;
    }

    return widths;
}

QRectF perClassStudentCellRect(
    const QRectF& contentRect,
    qreal tableTop,
    int row,
    int column,
    int resolutionDpi,
    const QVector<qreal>& columnWidths
    )
{
    qreal left =
        contentRect.left();

    for (int currentColumn = 1; currentColumn < column; ++currentColumn)
    {
        left += columnWidths.at(currentColumn);
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
        columnWidths.at(column),
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
        const qreal verticalPadding =
            perClassInches(
                PerClassVerticalCellPaddingInches,
                resolutionDpi
                );
        drawDailyText(
            painter,
            rect.adjusted(
                padding,
                verticalPadding,
                -padding,
                -verticalPadding
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
    const QVector<qreal> columnWidths =
        perClassStudentColumnWidths(
            values,
            columnCount,
            resolutionDpi
            );
    const QVector<qreal> infoColumnWidths =
        perClassInfoColumnWidths(
            values,
            resolutionDpi
            );
    const qreal infoRowHeight =
        perClassInches(
            PerClassInfoRowHeightInches,
            resolutionDpi
            );
    const qreal tableTop =
        contentRect.top()
        + (infoRowHeight * 4.0)
        + perClassInches(
            PerClassInfoTableGapInches,
            resolutionDpi
            );
    const QFont labelFont =
        printKoreanFont(
            8.5,
            resolutionDpi,
            QFont::Bold
            );
    const QFont valueFont =
        printKoreanFont(
            8.5,
            resolutionDpi
            );
    const QFont headerFont =
        printKoreanFont(
            8.2,
            resolutionDpi,
            QFont::Bold
            );
    const QFont studentFont =
        printKoreanFont(
            10.0,
            resolutionDpi
            );
    QFont notesFont =
        printKoreanFont(
            8.5,
            resolutionDpi,
            QFont::Bold
            );
    notesFont.setUnderline(true);
    const QColor labelBackground(QStringLiteral("#DCE6F1"));
    const QColor headerBackground(QStringLiteral("#B8CCE4"));

    qreal infoTableWidth = 0.0;
    for (int column = 1; column <= 4; ++column)
    {
        infoTableWidth += infoColumnWidths.at(column);
    }

    const qreal notesLeft =
        contentRect.left()
        + infoTableWidth
        + perClassInches(
            PerClassInfoTableGapInches,
            resolutionDpi
            );
    const bool portraitLayout =
        pageRect.height() > pageRect.width();
    const qreal studentTableBottom =
        tableTop
        + perClassInches(
            PerClassHeaderRowHeightInches
            + (PerClassStudentRowHeightInches * PerClassStudentRowCount),
            resolutionDpi
            );
    const QRectF notesRect =
        portraitLayout
            ? QRectF(
                contentRect.left(),
                studentTableBottom
                    + perClassInches(
                        PerClassInfoTableGapInches,
                        resolutionDpi
                        ),
                contentRect.width(),
                std::max(
                    0.0,
                    contentRect.bottom()
                    - studentTableBottom
                    - perClassInches(
                        PerClassInfoTableGapInches,
                        resolutionDpi
                        )
                    )
                )
            : QRectF(
                notesLeft,
                contentRect.top(),
                std::max(0.0, contentRect.right() - notesLeft),
                infoRowHeight * 4.0
                );

    for (int row = 1; row <= 4; ++row)
    {
        qreal infoCellLeft =
            contentRect.left();

        for (int column = 1; column <= 4; ++column)
        {
            const QRectF rect(
                infoCellLeft,
                contentRect.top() + ((row - 1) * infoRowHeight),
                infoColumnWidths.at(column),
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
                Qt::AlignCenter,
                resolutionDpi
                );

            infoCellLeft +=
                infoColumnWidths.at(column);
        }
    }

    if (notesRect.width() > 0.0)
    {
        drawPerClassCell(
            painter,
            notesRect,
            QStringLiteral("Notes:"),
            notesFont,
            Qt::white,
            Qt::black,
            Qt::AlignLeft | Qt::AlignTop,
            resolutionDpi
            );
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
                resolutionDpi,
                columnWidths
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
                    resolutionDpi,
                    columnWidths
                    ),
                cellValue(
                    values,
                    row,
                    column
                    ),
                studentFont,
                Qt::white,
                Qt::black,
                Qt::AlignCenter,
                resolutionDpi
                );
        }
    }
}
