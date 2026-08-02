// Shared PDF, cell-value, and layout helpers.


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
    Q_UNUSED(templateId);
    return QMarginsF();
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
        QStringLiteral("Rosters")
        );
    writer.setResolution(
        PdfPrintService::GeneratedPdfResolutionDpi
        );

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
