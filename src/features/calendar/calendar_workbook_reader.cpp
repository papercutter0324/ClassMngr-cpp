#include "calendar_workbook_reader.h"

#include <cstdint>

#include <QHash>
#include <QXmlStreamReader>

#include <zlib.h>

#include <algorithm>
#include <cmath>

namespace CalendarImport
{
namespace
{
constexpr int MinZipEocdSize = 22;
constexpr int MaxZipCommentSize = 0xffff;
constexpr quint32 ZipEocdSignature = 0x06054b50;
constexpr quint32 ZipCentralFileSignature = 0x02014b50;
constexpr quint32 ZipLocalFileSignature = 0x04034b50;

struct ZipEntry
{
    QString name;
    quint16 method = 0;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint32 localHeaderOffset = 0;
};

quint16 readLe16(
    const QByteArray& data,
    qsizetype offset
    )
{
    if (offset < 0 || offset + 2 > data.size())
    {
        return 0;
    }

    const auto* bytes =
        reinterpret_cast<const uchar*>(data.constData() + offset);
    return static_cast<quint16>(
        bytes[0]
        | (bytes[1] << 8)
        );
}

quint32 readLe32(
    const QByteArray& data,
    qsizetype offset
    )
{
    if (offset < 0 || offset + 4 > data.size())
    {
        return 0;
    }

    const auto* bytes =
        reinterpret_cast<const uchar*>(data.constData() + offset);
    return static_cast<quint32>(
        bytes[0]
        | (bytes[1] << 8)
        | (bytes[2] << 16)
        | (bytes[3] << 24)
        );
}
}

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

QByteArray inflateRawDeflate(
    const char* compressedData,
    qsizetype compressedSize,
    quint32 uncompressedSize
    )
{
    QByteArray output;
    output.resize(
        static_cast<qsizetype>(uncompressedSize)
        );

    if (uncompressedSize == 0)
    {
        return output;
    }

    z_stream stream{};
    stream.next_in =
        reinterpret_cast<Bytef*>(
            const_cast<char*>(compressedData)
            );
    stream.avail_in =
        static_cast<uInt>(compressedSize);
    stream.next_out =
        reinterpret_cast<Bytef*>(output.data());
    stream.avail_out =
        static_cast<uInt>(output.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
    {
        return {};
    }

    const int result =
        inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (result != Z_STREAM_END)
    {
        return {};
    }

    output.truncate(
        static_cast<qsizetype>(stream.total_out)
        );

    return output;
}

QHash<QString, ZipEntry> zipEntries(
    const QByteArray& zipData
    )
{
    QHash<QString, ZipEntry> entries;

    const qsizetype searchStart =
        qMax<qsizetype>(
            0,
            zipData.size() - MinZipEocdSize - MaxZipCommentSize
            );

    qsizetype eocdOffset = -1;
    for (qsizetype offset = zipData.size() - MinZipEocdSize;
         offset >= searchStart;
         --offset)
    {
        if (readLe32(zipData, offset) == ZipEocdSignature)
        {
            eocdOffset = offset;
            break;
        }
    }

    if (eocdOffset < 0)
    {
        return entries;
    }

    const quint16 entryCount =
        readLe16(zipData, eocdOffset + 10);
    const quint32 centralDirectoryOffset =
        readLe32(zipData, eocdOffset + 16);

    qsizetype offset =
        static_cast<qsizetype>(centralDirectoryOffset);

    for (int index = 0; index < entryCount; ++index)
    {
        if (
            offset + 46 > zipData.size()
            || readLe32(zipData, offset) != ZipCentralFileSignature
            )
        {
            return {};
        }

        const quint16 method =
            readLe16(zipData, offset + 10);
        const quint32 compressedSize =
            readLe32(zipData, offset + 20);
        const quint32 uncompressedSize =
            readLe32(zipData, offset + 24);
        const quint16 fileNameLength =
            readLe16(zipData, offset + 28);
        const quint16 extraLength =
            readLe16(zipData, offset + 30);
        const quint16 commentLength =
            readLe16(zipData, offset + 32);
        const quint32 localHeaderOffset =
            readLe32(zipData, offset + 42);

        if (offset + 46 + fileNameLength > zipData.size())
        {
            return {};
        }

        ZipEntry entry;
        entry.name =
            QString::fromUtf8(
                zipData.mid(offset + 46, fileNameLength)
                );
        entry.method =
            method;
        entry.compressedSize =
            compressedSize;
        entry.uncompressedSize =
            uncompressedSize;
        entry.localHeaderOffset =
            localHeaderOffset;

        entries.insert(
            entry.name,
            entry
            );

        offset +=
            46
            + fileNameLength
            + extraLength
            + commentLength;
    }

    return entries;
}

QByteArray zipFileData(
    const QByteArray& zipData,
    const ZipEntry& entry
    )
{
    const qsizetype localOffset =
        static_cast<qsizetype>(entry.localHeaderOffset);

    if (
        localOffset + 30 > zipData.size()
        || readLe32(zipData, localOffset) != ZipLocalFileSignature
        )
    {
        return {};
    }

    const quint16 fileNameLength =
        readLe16(zipData, localOffset + 26);
    const quint16 extraLength =
        readLe16(zipData, localOffset + 28);
    const qsizetype dataOffset =
        localOffset + 30 + fileNameLength + extraLength;

    if (
        dataOffset < 0
        || dataOffset + entry.compressedSize > zipData.size()
        )
    {
        return {};
    }

    if (entry.method == 0)
    {
        // Stored ZIP entries can be parsed directly from the caller-owned
        // archive. Every use is synchronous and stays within the lifetime of
        // zipData; the parsed workbook objects own their resulting strings.
        return QByteArray::fromRawData(
            zipData.constData() + dataOffset,
            static_cast<qsizetype>(entry.compressedSize)
            );
    }

    if (entry.method != 8)
    {
        return {};
    }

    return inflateRawDeflate(
        zipData.constData() + dataOffset,
        static_cast<qsizetype>(entry.compressedSize),
        entry.uncompressedSize
        );
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
    const QByteArray& data,
    const QHash<QString, ZipEntry>& entries,
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

    if (entries.contains(relationshipsName))
    {
        return worksheetNoteEntryNames(
            zipFileData(
                data,
                entries.value(relationshipsName)
                )
            );
    }

    QStringList entryNames;

    for (auto iterator = entries.cbegin();
         iterator != entries.cend();
         ++iterator)
    {
        const QString& name =
            iterator.key();

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
    const QByteArray& data,
    const QHash<QString, ZipEntry>& entries,
    const QString& worksheetEntryName
    )
{
    QHash<CellPosition, QString> notes;
    const QStringList entryNames =
        worksheetNoteEntryNames(data, entries, worksheetEntryName);

    for (const QString& name : entryNames)
    {
        if (!entries.contains(name))
        {
            continue;
        }

        const QHash<CellPosition, QString> parsed =
            parseCellNotes(
                zipFileData(data, entries.value(name))
                );

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

bool forEachWorksheet(
    const QByteArray& data,
    const WorksheetConsumer& consumer,
    QString* errorMessage
    )
{
    const QHash<QString, ZipEntry> entries =
        zipEntries(data);

    const QStringList requiredFiles{
        QStringLiteral("xl/workbook.xml"),
        QStringLiteral("xl/_rels/workbook.xml.rels")
    };

    for (const QString& file : requiredFiles)
    {
        if (!entries.contains(file))
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("The downloaded spreadsheet is missing %1.")
                        .arg(file);
            }
            return false;
        }
    }

    WorkbookMetadata metadata;
    if (entries.contains(QStringLiteral("xl/sharedStrings.xml")))
    {
        metadata.sharedStrings =
            parseSharedStrings(
                zipFileData(
                    data,
                    entries.value(QStringLiteral("xl/sharedStrings.xml"))
                    )
                );
    }
    if (entries.contains(QStringLiteral("xl/styles.xml")))
    {
        QVector<QString> themeColors =
            parseThemeColors({});
        if (
            entries.contains(
                QStringLiteral("xl/theme/theme1.xml")
                )
            )
        {
            themeColors =
                parseThemeColors(
                    zipFileData(
                        data,
                        entries.value(
                            QStringLiteral(
                                "xl/theme/theme1.xml"
                                )
                            )
                        )
                    );
        }
        metadata.styles =
            parseStyles(
                zipFileData(
                    data,
                    entries.value(QStringLiteral("xl/styles.xml"))
                    ),
                themeColors
                );
    }
    if (metadata.styles.isEmpty())
    {
        metadata.styles.append(Style{});
    }
    const QVector<WorksheetReference> worksheetReferences =
        parseWorksheetReferences(
            zipFileData(data, entries.value(QStringLiteral("xl/workbook.xml")))
            );
    const QHash<QString, QString> relationshipTargets =
        parseWorkbookRelationships(
            zipFileData(data, entries.value(QStringLiteral("xl/_rels/workbook.xml.rels")))
            );

    bool anyWorksheetHasCells = false;
    for (const WorksheetReference& reference : worksheetReferences)
    {
        const QString entryName =
            relationshipTargets.value(reference.relationshipId);
        if (entryName.isEmpty() || !entries.contains(entryName))
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
                zipFileData(data, entries.value(entryName));
            worksheet.cells = parseSheet(
                worksheetData,
                metadata.sharedStrings,
                workbookCellNotes(data, entries, entryName)
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

    return true;
}

Workbook parseWorkbook(
    const QByteArray& data,
    QString* errorMessage
    )
{
    Workbook workbook;
    bool haveMetadata = false;
    const bool succeeded =
        forEachWorksheet(
            data,
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
