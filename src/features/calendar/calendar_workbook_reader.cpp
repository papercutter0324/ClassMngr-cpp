#include "calendar_workbook_reader.h"

#include <QHash>
#include <QIODevice>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <utility>

namespace CalendarImport
{
QString normalizedColor(
    QString color
    )
{
    color =
        color.trimmed().toUpper();

    if (color.startsWith(QLatin1Char('#')))
    {
        color.remove(0, 1);
    }

    if (color.size() == 8 && color.startsWith(QStringLiteral("FF")))
    {
        color.remove(0, 2);
    }

    return color;
}

namespace
{

int spreadsheetColumn(
    const QString& reference
    )
{
    int column = 0;

    for (const QChar character : reference)
    {
        if (!character.isLetter())
        {
            break;
        }

        const int digit =
            character.toUpper().unicode()
            - QLatin1Char('A').unicode()
            + 1;
        if (digit < 1 || digit > 26)
        {
            return CellPosition::MaxColumn + 1;
        }

        if (
            column
            > (CellPosition::MaxColumn - digit) / 26
            )
        {
            return CellPosition::MaxColumn + 1;
        }

        column = column * 26 + digit;
    }

    return column;
}

int spreadsheetRow(
    const QString& reference
    )
{
    QString digits;

    for (const QChar character : reference)
    {
        if (character.isDigit())
        {
            digits.append(character);
        }
    }

    bool ok = false;
    const int row =
        digits.toInt(&ok);

    return ok ? row : 0;
}

QStringList parseSharedStrings(
    const QByteArray& xmlData
    )
{
    QStringList strings;
    QXmlStreamReader xml(xmlData);
    bool inSharedString = false;
    QString current;

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement() && xml.name() == QStringLiteral("si"))
        {
            inSharedString = true;
            current.clear();
        }
        else if (
            inSharedString
            && xml.isStartElement()
            && xml.name() == QStringLiteral("t")
            )
        {
            current.append(
                xml.readElementText()
                );
        }
        else if (xml.isEndElement() && xml.name() == QStringLiteral("si"))
        {
            strings.append(current);
            inSharedString = false;
        }
    }

    return strings;
}

QString rgbHex(
    int red,
    int green,
    int blue
    )
{
    return QStringLiteral("%1%2%3")
        .arg(red, 2, 16, QLatin1Char('0'))
        .arg(green, 2, 16, QLatin1Char('0'))
        .arg(blue, 2, 16, QLatin1Char('0'))
        .toUpper();
}

double hueComponent(
    double first,
    double second,
    double hue
    )
{
    if (hue < 0.0)
    {
        hue += 1.0;
    }
    if (hue > 1.0)
    {
        hue -= 1.0;
    }
    if (hue < 1.0 / 6.0)
    {
        return first
            + (second - first) * 6.0 * hue;
    }
    if (hue < 0.5)
    {
        return second;
    }
    if (hue < 2.0 / 3.0)
    {
        return first
            + (second - first)
                * (2.0 / 3.0 - hue) * 6.0;
    }
    return first;
}

QString tintedColor(
    QString color,
    double tint
    )
{
    color = normalizedColor(color);
    if (color.size() != 6)
    {
        return color;
    }

    bool redOk = false;
    bool greenOk = false;
    bool blueOk = false;
    const double red =
        color.mid(0, 2).toInt(&redOk, 16) / 255.0;
    const double green =
        color.mid(2, 2).toInt(&greenOk, 16) / 255.0;
    const double blue =
        color.mid(4, 2).toInt(&blueOk, 16) / 255.0;
    if (!redOk || !greenOk || !blueOk)
    {
        return {};
    }

    const double maximum =
        std::max({red, green, blue});
    const double minimum =
        std::min({red, green, blue});
    double hue = 0.0;
    double saturation = 0.0;
    double luminance =
        (maximum + minimum) / 2.0;

    if (maximum != minimum)
    {
        const double difference =
            maximum - minimum;
        saturation =
            luminance > 0.5
                ? difference / (2.0 - maximum - minimum)
                : difference / (maximum + minimum);
        if (maximum == red)
        {
            hue =
                (green - blue) / difference
                + (green < blue ? 6.0 : 0.0);
        }
        else if (maximum == green)
        {
            hue =
                (blue - red) / difference + 2.0;
        }
        else
        {
            hue =
                (red - green) / difference + 4.0;
        }
        hue /= 6.0;
    }

    tint = std::clamp(tint, -1.0, 1.0);
    luminance =
        tint < 0.0
            ? luminance * (1.0 + tint)
            : luminance * (1.0 - tint) + tint;

    double tintedRed = luminance;
    double tintedGreen = luminance;
    double tintedBlue = luminance;
    if (saturation > 0.0)
    {
        const double second =
            luminance < 0.5
                ? luminance * (1.0 + saturation)
                : luminance + saturation
                    - luminance * saturation;
        const double first =
            2.0 * luminance - second;
        tintedRed =
            hueComponent(
                first,
                second,
                hue + 1.0 / 3.0
                );
        tintedGreen =
            hueComponent(first, second, hue);
        tintedBlue =
            hueComponent(
                first,
                second,
                hue - 1.0 / 3.0
                );
    }

    return rgbHex(
        static_cast<int>(
            std::lround(tintedRed * 255.0)
            ),
        static_cast<int>(
            std::lround(tintedGreen * 255.0)
            ),
        static_cast<int>(
            std::lround(tintedBlue * 255.0)
            )
        );
}

QVector<QString> parseThemeColors(
    const QByteArray& xmlData
    )
{
    QVector<QString> colors(12);
    colors[0] = QStringLiteral("FFFFFF");
    colors[1] = QStringLiteral("000000");
    static const QHash<QString, int> indices{
        {QStringLiteral("lt1"), 0},
        {QStringLiteral("dk1"), 1},
        {QStringLiteral("lt2"), 2},
        {QStringLiteral("dk2"), 3},
        {QStringLiteral("accent1"), 4},
        {QStringLiteral("accent2"), 5},
        {QStringLiteral("accent3"), 6},
        {QStringLiteral("accent4"), 7},
        {QStringLiteral("accent5"), 8},
        {QStringLiteral("accent6"), 9},
        {QStringLiteral("hlink"), 10},
        {QStringLiteral("folHlink"), 11}
    };
    QXmlStreamReader xml(xmlData);
    bool inColorScheme = false;
    int currentIndex = -1;

    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement())
        {
            const QString name =
                xml.name().toString();
            if (name == QStringLiteral("clrScheme"))
            {
                inColorScheme = true;
            }
            else if (
                inColorScheme
                && indices.contains(name)
                )
            {
                currentIndex = indices.value(name);
            }
            else if (
                currentIndex >= 0
                && (
                    name == QStringLiteral("srgbClr")
                    || name == QStringLiteral("sysClr")
                    )
                )
            {
                const QString value =
                    name == QStringLiteral("srgbClr")
                        ? xml.attributes()
                            .value(QStringLiteral("val"))
                            .toString()
                        : xml.attributes()
                            .value(QStringLiteral("lastClr"))
                            .toString();
                colors[currentIndex] =
                    normalizedColor(value);
            }
        }
        else if (xml.isEndElement())
        {
            const QString name =
                xml.name().toString();
            if (name == QStringLiteral("clrScheme"))
            {
                inColorScheme = false;
            }
            if (indices.contains(name))
            {
                currentIndex = -1;
            }
        }
    }

    return colors;
}

QVector<QString> parseIndexedColors(
    const QByteArray& xmlData
    )
{
    QVector<QString> colors;
    QXmlStreamReader xml(xmlData);
    bool inIndexedColors = false;

    while (!xml.atEnd())
    {
        xml.readNext();
        if (
            xml.isStartElement()
            && xml.name() == QStringLiteral("indexedColors")
            )
        {
            inIndexedColors = true;
        }
        else if (
            inIndexedColors
            && xml.isStartElement()
            && xml.name() == QStringLiteral("rgbColor")
            )
        {
            colors.append(
                normalizedColor(
                    xml.attributes()
                        .value(QStringLiteral("rgb"))
                        .toString()
                    )
                );
        }
        else if (
            xml.isEndElement()
            && xml.name() == QStringLiteral("indexedColors")
            )
        {
            inIndexedColors = false;
        }
    }

    return colors;
}

QString resolvedStyleColor(
    const QXmlStreamAttributes& attributes,
    const QVector<QString>& themeColors,
    const QVector<QString>& indexedColors
    )
{
    QString color =
        normalizedColor(
            attributes.value(QStringLiteral("rgb"))
                .toString()
            );
    bool ok = false;
    const int theme =
        attributes.value(QStringLiteral("theme"))
            .toInt(&ok);
    if (
        color.isEmpty()
        && ok
        && theme >= 0
        && theme < themeColors.size()
        )
    {
        color = themeColors[theme];
    }

    const int indexed =
        attributes.value(QStringLiteral("indexed"))
            .toInt(&ok);
    if (
        color.isEmpty()
        && ok
        && indexed >= 0
        && indexed < indexedColors.size()
        )
    {
        color = indexedColors[indexed];
    }

    const double tint =
        attributes.value(QStringLiteral("tint"))
            .toDouble(&ok);
    return ok
        ? tintedColor(color, tint)
        : normalizedColor(color);
}

QVector<Style> parseStyles(
    const QByteArray& xmlData,
    const QVector<QString>& themeColors
    )
{
    const QVector<QString> indexedColors =
        parseIndexedColors(xmlData);
    QXmlStreamReader xml(xmlData);
    QVector<QString> fillColors;
    QVector<QString> fontColors;
    QVector<bool> fillFlags;
    QVector<bool> fontBoldFlags;
    QVector<Style> styles;
    bool inFills = false;
    bool inFill = false;
    bool inFonts = false;
    bool inFont = false;
    bool inCellXfs = false;
    QString currentFill;
    QString currentFont;
    bool currentFilled = false;
    bool currentBold = false;

    while (!xml.atEnd())
    {
        xml.readNext();

        if (!xml.isStartElement() && !xml.isEndElement())
        {
            continue;
        }

        const QStringView name =
            xml.name();

        if (xml.isStartElement())
        {
            if (name == QStringLiteral("fills"))
            {
                inFills = true;
            }
            else if (name == QStringLiteral("fill") && inFills)
            {
                inFill = true;
                currentFill.clear();
                currentFilled = false;
            }
            else if (name == QStringLiteral("patternFill") && inFill)
            {
                const QString pattern =
                    xml.attributes().value(QStringLiteral("patternType")).toString();
                currentFilled = !pattern.isEmpty()
                    && pattern != QStringLiteral("none")
                    && pattern != QStringLiteral("gray125");
            }
            else if (
                name == QStringLiteral("fgColor")
                && inFill
                )
            {
                currentFill =
                    resolvedStyleColor(
                        xml.attributes(),
                        themeColors,
                        indexedColors
                        );
            }
            else if (name == QStringLiteral("fonts"))
            {
                inFonts = true;
            }
            else if (name == QStringLiteral("font") && inFonts)
            {
                inFont = true;
                currentFont.clear();
                currentBold = false;
            }
            else if (name == QStringLiteral("b") && inFont)
            {
                const QString value =
                    xml.attributes().value(QStringLiteral("val")).toString();
                currentBold = value.isEmpty()
                    || (value != QStringLiteral("0")
                        && value.compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0);
            }
            else if (
                name == QStringLiteral("color")
                && inFont
                )
            {
                currentFont =
                    resolvedStyleColor(
                        xml.attributes(),
                        themeColors,
                        indexedColors
                        );
            }
            else if (name == QStringLiteral("cellXfs"))
            {
                inCellXfs = true;
            }
            else if (name == QStringLiteral("xf") && inCellXfs)
            {
                bool fillOk = false;
                bool fontOk = false;
                const int fillId =
                    xml.attributes()
                        .value(QStringLiteral("fillId"))
                        .toInt(&fillOk);
                const int fontId =
                    xml.attributes()
                        .value(QStringLiteral("fontId"))
                        .toInt(&fontOk);

                Style style;
                if (
                    fillOk
                    && fillId >= 0
                    && fillId < fillColors.size()
                    )
                {
                    style.fillColor =
                        fillColors[fillId];
                    style.filled = fillFlags.value(fillId, false);
                }
                if (
                    fontOk
                    && fontId >= 0
                    && fontId < fontColors.size()
                    )
                {
                    style.fontColor =
                        fontColors[fontId];
                    style.bold = fontBoldFlags.value(fontId, false);
                }
                styles.append(style);
            }
        }
        else if (xml.isEndElement())
        {
            if (name == QStringLiteral("fill") && inFill)
            {
                fillColors.append(currentFill);
                fillFlags.append(
                    currentFilled
                    && normalizedColor(currentFill)
                        != QStringLiteral("FFFFFF")
                    );
                inFill = false;
            }
            else if (name == QStringLiteral("fills"))
            {
                inFills = false;
            }
            else if (name == QStringLiteral("font") && inFont)
            {
                fontColors.append(currentFont);
                fontBoldFlags.append(currentBold);
                inFont = false;
            }
            else if (name == QStringLiteral("fonts"))
            {
                inFonts = false;
            }
            else if (name == QStringLiteral("cellXfs"))
            {
                inCellXfs = false;
            }
        }
    }

    return styles;
}

QVector<Cell> parseSheet(
    const QByteArray& xmlData,
    const QStringList& sharedStrings,
    const QHash<CellPosition, QString>& notesByPosition
    )
{
    QVector<Cell> cells;
    QXmlStreamReader xml(xmlData);

    while (!xml.atEnd())
    {
        xml.readNext();

        if (!xml.isStartElement() || xml.name() != QStringLiteral("c"))
        {
            continue;
        }

        const QXmlStreamAttributes attributes =
            xml.attributes();
        const QString reference =
            attributes.value(QStringLiteral("r")).toString();
        Cell cell;
        cell.position = {
            spreadsheetRow(reference),
            spreadsheetColumn(reference)
        };

        if (!cell.position.isValid())
        {
            xml.skipCurrentElement();
            continue;
        }
        cell.note =
            notesByPosition.value(cell.position).trimmed();

        cell.style =
            attributes.value(QStringLiteral("s")).toInt();
        const QString type =
            attributes.value(QStringLiteral("t")).toString();

        while (!(xml.isEndElement() && xml.name() == QStringLiteral("c")))
        {
            xml.readNext();

            if (xml.isStartElement() && xml.name() == QStringLiteral("v"))
            {
                const QString rawValue =
                    xml.readElementText();

                if (type == QStringLiteral("s"))
                {
                    bool ok = false;
                    const int stringIndex =
                        rawValue.toInt(&ok);

                    if (
                        ok
                        && stringIndex >= 0
                        && stringIndex < sharedStrings.size()
                        )
                    {
                        cell.value =
                            sharedStrings[stringIndex];
                    }
                }
                else
                {
                    cell.value =
                        rawValue;
                }
            }
            else if (
                type == QStringLiteral("inlineStr")
                && xml.isStartElement()
                && xml.name() == QStringLiteral("t")
                )
            {
                cell.value.append(
                    xml.readElementText()
                    );
            }

            if (xml.atEnd())
            {
                break;
            }
        }

        if (!cell.value.trimmed().isEmpty())
        {
            cells.append(cell);
        }
    }

    return cells;
}

bool parseWorksheetRows(
    QIODevice& device,
    const QStringList& sharedStrings,
    const QHash<CellPosition, QString>& notesByPosition,
    const WorksheetRowConsumer& consumer,
    Worksheet* worksheet,
    bool collectMergedRanges
    )
{
    QXmlStreamReader xml(&device);
    QVector<Cell> currentRow;
    int currentRowNumber = 0;

    const auto flushRow = [&]()
    {
        if (currentRow.isEmpty())
        {
            return true;
        }

        const bool consumed = !consumer || consumer(currentRow);
        currentRow.clear();
        currentRowNumber = 0;
        return consumed;
    };

    while (!xml.atEnd())
    {
        xml.readNext();

        if (!xml.isStartElement())
        {
            continue;
        }

        if (
            collectMergedRanges
            && xml.name() == QStringLiteral("mergeCell")
            )
        {
            const QStringList endpoints =
                xml.attributes()
                    .value(QStringLiteral("ref"))
                    .toString()
                    .split(QLatin1Char(':'));
            if (!endpoints.isEmpty() && endpoints.size() <= 2)
            {
                CellRange range;
                range.first = {
                    spreadsheetRow(endpoints.first()),
                    spreadsheetColumn(endpoints.first())
                };
                const QString last = endpoints.size() == 2
                    ? endpoints.last()
                    : endpoints.first();
                range.last = {
                    spreadsheetRow(last),
                    spreadsheetColumn(last)
                };
                if (range.isValid())
                {
                    worksheet->mergedRanges.append(range);
                }
            }
            continue;
        }

        if (xml.name() != QStringLiteral("c"))
        {
            continue;
        }

        const QXmlStreamAttributes attributes = xml.attributes();
        const QString reference =
            attributes.value(QStringLiteral("r")).toString();
        Cell cell;
        cell.position = {
            spreadsheetRow(reference),
            spreadsheetColumn(reference)
        };

        if (!cell.position.isValid())
        {
            xml.skipCurrentElement();
            continue;
        }

        cell.note = notesByPosition.value(cell.position).trimmed();
        cell.style = attributes.value(QStringLiteral("s")).toInt();
        const QString type =
            attributes.value(QStringLiteral("t")).toString();

        while (!(xml.isEndElement() && xml.name() == QStringLiteral("c")))
        {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QStringLiteral("v"))
            {
                const QString rawValue = xml.readElementText();
                if (type == QStringLiteral("s"))
                {
                    bool ok = false;
                    const int stringIndex = rawValue.toInt(&ok);
                    if (
                        ok
                        && stringIndex >= 0
                        && stringIndex < sharedStrings.size()
                        )
                    {
                        cell.value = sharedStrings[stringIndex];
                    }
                }
                else
                {
                    cell.value = rawValue;
                }
            }
            else if (
                type == QStringLiteral("inlineStr")
                && xml.isStartElement()
                && xml.name() == QStringLiteral("t")
                )
            {
                cell.value.append(xml.readElementText());
            }

            if (xml.atEnd())
            {
                break;
            }
        }

        if (cell.value.trimmed().isEmpty())
        {
            continue;
        }

        if (
            currentRowNumber != 0
            && currentRowNumber != cell.position.row
            && !flushRow()
            )
        {
            return false;
        }
        currentRowNumber = cell.position.row;
        currentRow.append(std::move(cell));
    }

    if (xml.hasError())
    {
        return false;
    }

    return flushRow();
}

struct WorksheetReference
{
    QString name;
    QString relationshipId;
    bool visible = true;
};

QVector<WorksheetReference> parseWorksheetReferences(
    const QByteArray& xmlData
    )
{
    QVector<WorksheetReference> result;
    QXmlStreamReader xml(xmlData);

    while (!xml.atEnd())
    {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QStringLiteral("sheet"))
        {
            continue;
        }

        WorksheetReference reference;
        reference.name =
            xml.attributes().value(QStringLiteral("name")).toString();
        const QString state =
            xml.attributes().value(QStringLiteral("state")).toString();
        reference.visible =
            state.compare(QStringLiteral("hidden"), Qt::CaseInsensitive) != 0
            && state.compare(
                QStringLiteral("veryHidden"),
                Qt::CaseInsensitive
                ) != 0;
        for (const QXmlStreamAttribute& attribute : xml.attributes())
        {
            if (attribute.name() == QStringLiteral("id"))
            {
                reference.relationshipId = attribute.value().toString();
                break;
            }
        }
        if (!reference.relationshipId.isEmpty())
        {
            result.append(reference);
        }
    }

    return result;
}

QVector<CellRange> parseMergedRanges(
    const QByteArray& xmlData
    )
{
    QVector<CellRange> result;
    QXmlStreamReader xml(xmlData);

    while (!xml.atEnd())
    {
        xml.readNext();

        if (
            !xml.isStartElement()
            || xml.name() != QStringLiteral("mergeCell")
            )
        {
            continue;
        }

        const QString reference =
            xml.attributes().value(QStringLiteral("ref")).toString();
        const QStringList endpoints =
            reference.split(QLatin1Char(':'));

        if (endpoints.isEmpty() || endpoints.size() > 2)
        {
            continue;
        }

        const QString first = endpoints.first();
        const QString last =
            endpoints.size() == 2
                ? endpoints.last()
                : endpoints.first();

        CellRange range;
        range.first = {
            spreadsheetRow(first),
            spreadsheetColumn(first)
        };
        range.last = {
            spreadsheetRow(last),
            spreadsheetColumn(last)
        };

        if (range.isValid())
        {
            result.append(range);
        }
    }

    return result;
}

QString resolvedWorkbookRelationshipTarget(QString target)
{
    target = target.trimmed();
    if (target.startsWith(QLatin1Char('/')))
    {
        target.remove(0, 1);
        return target;
    }
    while (target.startsWith(QStringLiteral("../")))
    {
        target.remove(0, 3);
    }
    if (!target.startsWith(QStringLiteral("xl/")))
    {
        target.prepend(QStringLiteral("xl/"));
    }
    return target;
}

QHash<QString, QString> parseWorkbookRelationships(
    const QByteArray& xmlData
    )
{
    QHash<QString, QString> result;
    QXmlStreamReader xml(xmlData);

    while (!xml.atEnd())
    {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QStringLiteral("Relationship"))
        {
            continue;
        }
        const auto attributes = xml.attributes();
        const QString id = attributes.value(QStringLiteral("Id")).toString();
        const QString type = attributes.value(QStringLiteral("Type")).toString();
        if (!id.isEmpty() && type.endsWith(QStringLiteral("/worksheet")))
        {
            result.insert(
                id,
                resolvedWorkbookRelationshipTarget(
                    attributes.value(QStringLiteral("Target")).toString())
                );
        }
    }

    return result;
}

QHash<CellPosition, QString> parseCellNotes(
    const QByteArray& xmlData
    )
{
    QHash<CellPosition, QString> notes;
    QXmlStreamReader xml(xmlData);
    QString currentElement;
    CellPosition currentPosition;
    QString currentText;

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            const QStringView name =
                xml.name();

            if (
                name == QStringLiteral("comment")
                || name == QStringLiteral("threadedComment")
                )
            {
                const QString reference =
                    xml.attributes()
                        .value(QStringLiteral("ref"))
                        .toString();
                const CellPosition position{
                    spreadsheetRow(reference),
                    spreadsheetColumn(reference)
                };

                currentElement =
                    name.toString();
                currentPosition =
                    position.isValid() ? position : CellPosition{};
                currentText.clear();
            }
            else if (
                currentPosition.isValid()
                && (
                    name == QStringLiteral("t")
                    || (
                        currentElement == QStringLiteral("threadedComment")
                        && name == QStringLiteral("text")
                        )
                    )
                )
            {
                currentText.append(
                    xml.readElementText(
                        QXmlStreamReader::IncludeChildElements
                        )
                    );
            }
        }
        else if (xml.isEndElement())
        {
            const QStringView name =
                xml.name();

            if (
                !currentElement.isEmpty()
                && name == currentElement
                )
            {
                const QString note =
                    currentText.simplified();

                if (currentPosition.isValid() && !note.isEmpty())
                {
                    notes.insert(
                        currentPosition,
                        note
                        );
                }

                currentElement.clear();
                currentPosition = {};
                currentText.clear();
            }
        }
    }

    return notes;
}

QString resolvedWorksheetRelationshipTarget(
    QString target
    )
{
    target =
        target.trimmed();

    if (target.startsWith(QLatin1Char('/')))
    {
        target.remove(0, 1);
        return target;
    }

    while (target.startsWith(QStringLiteral("../")))
    {
        target.remove(0, 3);
        target.prepend(QStringLiteral("xl/"));
    }

    if (!target.startsWith(QStringLiteral("xl/")))
    {
        target.prepend(QStringLiteral("xl/worksheets/"));
    }

    return target;
}

QStringList worksheetNoteEntryNames(
    const QByteArray& xmlData
    )
{
    QStringList entryNames;
    QXmlStreamReader xml(xmlData);

    while (!xml.atEnd())
    {
        xml.readNext();

        if (
            !xml.isStartElement()
            || xml.name() != QStringLiteral("Relationship")
            )
        {
            continue;
        }

        const QXmlStreamAttributes attributes =
            xml.attributes();
        const QString type =
            attributes.value(QStringLiteral("Type")).toString();

        if (
            !type.endsWith(QStringLiteral("/comments"))
            && !type.endsWith(QStringLiteral("/threadedComment"))
            )
        {
            continue;
        }

        entryNames.append(
            resolvedWorksheetRelationshipTarget(
                attributes.value(QStringLiteral("Target")).toString()
                )
            );
    }

    entryNames.removeDuplicates();
    return entryNames;
}

QStringList worksheetNoteEntryNames(
    ArchiveReadSession& archive,
    const QString& worksheetEntryName
    )
{
    const int slash = worksheetEntryName.lastIndexOf(QLatin1Char('/'));
    const QString directory = slash >= 0
        ? worksheetEntryName.left(slash + 1)
        : QString();
    const QString fileName = slash >= 0
        ? worksheetEntryName.mid(slash + 1)
        : worksheetEntryName;
    const QString relationshipsName =
        directory + QStringLiteral("_rels/") + fileName + QStringLiteral(".rels");

    if (archive.contains(relationshipsName))
    {
        return worksheetNoteEntryNames(
            archive.readEntry(relationshipsName)
            );
    }

    QStringList entryNames;

    for (const QString& name : archive.entryNames())
    {
        if (
            (
                name.startsWith(QStringLiteral("xl/comments"))
                && name.endsWith(QStringLiteral(".xml"))
                )
            || (
                name.startsWith(QStringLiteral("xl/threadedComments/"))
                && name.endsWith(QStringLiteral(".xml"))
                )
            )
        {
            entryNames.append(name);
        }
    }

    entryNames.removeDuplicates();
    return entryNames;
}

QHash<CellPosition, QString> workbookCellNotes(
    ArchiveReadSession& archive,
    const QString& worksheetEntryName
    )
{
    QHash<CellPosition, QString> notes;
    const QStringList entryNames =
        worksheetNoteEntryNames(archive, worksheetEntryName);

    for (const QString& name : entryNames)
    {
        if (!archive.contains(name))
        {
            continue;
        }

        const QHash<CellPosition, QString> parsed =
            parseCellNotes(archive.readEntry(name));

        for (auto note = parsed.cbegin();
             note != parsed.cend();
             ++note)
        {
            notes.insert(
                note.key(),
                note.value()
                );
        }
    }

    return notes;
}
}

static bool forEachWorksheetFromArchive(
    ArchiveReadSession& archive,
    const WorksheetConsumer& consumer,
    QString* errorMessage
    )
{
    const QStringList requiredFiles{
        QStringLiteral("xl/workbook.xml"),
        QStringLiteral("xl/_rels/workbook.xml.rels")
    };

    for (const QString& file : requiredFiles)
    {
        if (!archive.contains(file))
        {
            if (errorMessage)
            {
                *errorMessage = archive.failed()
                    ? QStringLiteral("The downloaded spreadsheet could not be read.")
                    : QStringLiteral("The downloaded spreadsheet is missing %1.")
                          .arg(file);
            }
            return false;
        }
    }

    WorkbookMetadata metadata;
    if (archive.contains(QStringLiteral("xl/sharedStrings.xml")))
    {
        metadata.sharedStrings =
            parseSharedStrings(
                archive.readEntry(QStringLiteral("xl/sharedStrings.xml"))
                );
    }
    if (archive.contains(QStringLiteral("xl/styles.xml")))
    {
        QVector<QString> themeColors =
            parseThemeColors({});
        if (
            archive.contains(
                QStringLiteral("xl/theme/theme1.xml")
                )
            )
        {
            themeColors =
                parseThemeColors(
                    archive.readEntry(
                            QStringLiteral(
                                "xl/theme/theme1.xml"
                                )
                            )
                    );
        }
        metadata.styles =
            parseStyles(
                archive.readEntry(QStringLiteral("xl/styles.xml")),
                themeColors
                );
    }
    if (metadata.styles.isEmpty())
    {
        metadata.styles.append(Style{});
    }
    const QVector<WorksheetReference> worksheetReferences =
        parseWorksheetReferences(
            archive.readEntry(QStringLiteral("xl/workbook.xml"))
            );
    const QHash<QString, QString> relationshipTargets =
        parseWorkbookRelationships(
            archive.readEntry(QStringLiteral("xl/_rels/workbook.xml.rels"))
            );

    bool anyWorksheetHasCells = false;
    for (const WorksheetReference& reference : worksheetReferences)
    {
        const QString entryName =
            relationshipTargets.value(reference.relationshipId);
        if (entryName.isEmpty() || !archive.contains(entryName))
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("The spreadsheet is missing worksheet %1.")
                        .arg(reference.name);
            }
            return false;
        }

        Worksheet worksheet;
        worksheet.name = reference.name;
        worksheet.visible = reference.visible;
        {
            const QByteArray worksheetData =
                archive.readEntry(entryName);
            worksheet.cells = parseSheet(
                worksheetData,
                metadata.sharedStrings,
                workbookCellNotes(archive, entryName)
                );
            worksheet.mergedRanges =
                parseMergedRanges(worksheetData);
        }
        anyWorksheetHasCells =
            anyWorksheetHasCells || !worksheet.cells.isEmpty();

        if (consumer && !consumer(metadata, worksheet))
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("The downloaded spreadsheet could not be read.");
            }
            return false;
        }
    }

    if (worksheetReferences.isEmpty() || !anyWorksheetHasCells)
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return false;
    }

    if (archive.failed())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return false;
    }

    return true;
}

static bool forEachWorksheetRowsFromArchive(
    ArchiveReadSession& archive,
    const WorksheetStreamConsumer& consumer,
    QString* errorMessage
    )
{
    const QStringList requiredFiles{
        QStringLiteral("xl/workbook.xml"),
        QStringLiteral("xl/_rels/workbook.xml.rels")
    };

    for (const QString& file : requiredFiles)
    {
        if (!archive.contains(file))
        {
            if (errorMessage)
            {
                *errorMessage = archive.failed()
                    ? QStringLiteral("The downloaded spreadsheet could not be read.")
                    : QStringLiteral("The downloaded spreadsheet is missing %1.")
                          .arg(file);
            }
            return false;
        }
    }

    WorkbookMetadata metadata;
    if (archive.contains(QStringLiteral("xl/sharedStrings.xml")))
    {
        metadata.sharedStrings = parseSharedStrings(
            archive.readEntry(QStringLiteral("xl/sharedStrings.xml"))
            );
    }
    if (archive.contains(QStringLiteral("xl/styles.xml")))
    {
        QVector<QString> themeColors = parseThemeColors({});
        if (archive.contains(QStringLiteral("xl/theme/theme1.xml")))
        {
            themeColors = parseThemeColors(
                archive.readEntry(QStringLiteral("xl/theme/theme1.xml"))
                );
        }
        metadata.styles = parseStyles(
            archive.readEntry(QStringLiteral("xl/styles.xml")),
            themeColors
            );
    }
    if (metadata.styles.isEmpty())
    {
        metadata.styles.append(Style{});
    }

    const QVector<WorksheetReference> worksheetReferences =
        parseWorksheetReferences(
            archive.readEntry(QStringLiteral("xl/workbook.xml"))
            );
    const QHash<QString, QString> relationshipTargets =
        parseWorkbookRelationships(
            archive.readEntry(QStringLiteral("xl/_rels/workbook.xml.rels"))
            );

    bool anyWorksheetHasCells = false;
    for (const WorksheetReference& reference : worksheetReferences)
    {
        const QString entryName =
            relationshipTargets.value(reference.relationshipId);
        if (entryName.isEmpty() || !archive.contains(entryName))
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("The spreadsheet is missing worksheet %1.")
                        .arg(reference.name);
            }
            return false;
        }

        Worksheet worksheet;
        worksheet.name = reference.name;
        worksheet.visible = reference.visible;
        const QHash<CellPosition, QString> notes =
            workbookCellNotes(archive, entryName);

        bool discoveryInvoked = false;
        bool extractionInvoked = false;
        bool discoverySucceeded = false;
        bool extractionSucceeded = false;
        const WorksheetRowPass discoveryPass =
            [&](const WorksheetRowConsumer& rowConsumer)
            {
                if (discoveryInvoked || extractionInvoked)
                {
                    return false;
                }
                discoveryInvoked = true;
                worksheet.mergedRanges.clear();

                auto device = archive.openEntry(entryName);
                if (!device || !device->isValid())
                {
                    return false;
                }

                discoverySucceeded = parseWorksheetRows(
                    *device,
                    metadata.sharedStrings,
                    notes,
                    [&](const QVector<Cell>& row)
                    {
                        anyWorksheetHasCells =
                            anyWorksheetHasCells || !row.isEmpty();
                        return !rowConsumer || rowConsumer(row);
                    },
                    &worksheet,
                    true
                    );
                return discoverySucceeded;
            };
        const WorksheetRowPass extractionPass =
            [&](const WorksheetRowConsumer& rowConsumer)
            {
                if (!discoveryInvoked || !discoverySucceeded || extractionInvoked)
                {
                    return false;
                }
                extractionInvoked = true;

                auto device = archive.openEntry(entryName);
                if (!device || !device->isValid())
                {
                    return false;
                }

                extractionSucceeded = parseWorksheetRows(
                    *device,
                    metadata.sharedStrings,
                    notes,
                    [&](const QVector<Cell>& row)
                    {
                        anyWorksheetHasCells =
                            anyWorksheetHasCells || !row.isEmpty();
                        return !rowConsumer || rowConsumer(row);
                    },
                    &worksheet,
                    false
                    );
                return extractionSucceeded;
            };

        const bool consumerSucceeded = consumer
            ? consumer(metadata, worksheet, discoveryPass, extractionPass)
            : discoveryPass({}) && extractionPass({});
        if (
            !consumerSucceeded
            || !discoveryInvoked
            || !discoverySucceeded
            || !extractionInvoked
            || !extractionSucceeded
            )
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("The downloaded spreadsheet could not be read.");
            }
            return false;
        }
    }

    if (worksheetReferences.isEmpty() || !anyWorksheetHasCells)
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return false;
    }

    if (archive.failed())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return false;
    }

    return true;
}

bool forEachWorksheet(
    const QByteArray& data,
    ArchiveReadPort& archiveReader,
    const WorksheetConsumer& consumer,
    QString* errorMessage
    )
{
    std::unique_ptr<ArchiveReadSession> archive = archiveReader.open(data);
    if (!archive || !archive->isValid())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return false;
    }
    return forEachWorksheetFromArchive(*archive, consumer, errorMessage);
}

bool forEachWorksheet(
    QIODevice& device,
    ArchiveReadPort& archiveReader,
    const WorksheetConsumer& consumer,
    QString* errorMessage
    )
{
    std::unique_ptr<ArchiveReadSession> archive = archiveReader.open(device);
    if (!archive || !archive->isValid())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return false;
    }
    return forEachWorksheetFromArchive(*archive, consumer, errorMessage);
}

bool forEachWorksheetRows(
    const QByteArray& data,
    ArchiveReadPort& archiveReader,
    const WorksheetStreamConsumer& consumer,
    QString* errorMessage
    )
{
    std::unique_ptr<ArchiveReadSession> archive = archiveReader.open(data);
    if (!archive || !archive->isValid())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return false;
    }
    return forEachWorksheetRowsFromArchive(*archive, consumer, errorMessage);
}

bool forEachWorksheetRows(
    QIODevice& device,
    ArchiveReadPort& archiveReader,
    const WorksheetStreamConsumer& consumer,
    QString* errorMessage
    )
{
    std::unique_ptr<ArchiveReadSession> archive = archiveReader.open(device);
    if (!archive || !archive->isValid())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return false;
    }
    return forEachWorksheetRowsFromArchive(*archive, consumer, errorMessage);
}

Workbook parseWorkbook(
    const QByteArray& data,
    ArchiveReadPort& archiveReader,
    QString* errorMessage
    )
{
    Workbook workbook;
    bool haveMetadata = false;
    const bool succeeded =
        forEachWorksheet(
            data,
            archiveReader,
            [&workbook, &haveMetadata](
                const WorkbookMetadata& metadata,
                const Worksheet& worksheet
                )
            {
                if (!haveMetadata)
                {
                    workbook.sharedStrings = metadata.sharedStrings;
                    workbook.styles = metadata.styles;
                    haveMetadata = true;
                }
                workbook.worksheets.append(worksheet);
                return true;
            },
            errorMessage
            );
    if (!succeeded)
    {
        return {};
    }

    return workbook;
}

Workbook parseWorkbook(
    QIODevice& device,
    ArchiveReadPort& archiveReader,
    QString* errorMessage
    )
{
    Workbook workbook;
    bool haveMetadata = false;
    const bool succeeded =
        forEachWorksheet(
            device,
            archiveReader,
            [&workbook, &haveMetadata](
                const WorkbookMetadata& metadata,
                const Worksheet& worksheet
                )
            {
                if (!haveMetadata)
                {
                    workbook.sharedStrings = metadata.sharedStrings;
                    workbook.styles = metadata.styles;
                    haveMetadata = true;
                }
                workbook.worksheets.append(worksheet);
                return true;
            },
            errorMessage
            );
    if (!succeeded)
    {
        return {};
    }

    return workbook;
}
}
