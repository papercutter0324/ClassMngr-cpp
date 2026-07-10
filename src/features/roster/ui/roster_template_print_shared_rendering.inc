// Shared cell rendering helpers.


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
