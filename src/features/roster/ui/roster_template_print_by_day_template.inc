// By-day roster template rendering.


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
