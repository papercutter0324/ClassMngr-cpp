// Daily roster template rendering.


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
