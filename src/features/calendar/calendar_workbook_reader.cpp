#include "calendar_workbook_reader.h"

#include <cstdint>

#include <QHash>
#include <QXmlStreamReader>

#include <zlib.h>

#include <algorithm>

namespace CalendarImport
{
namespace
{
constexpr int LastMeaningfulRow = 1'048'576;
constexpr int ColumnCount = 16'384;
constexpr int CellPositionStride = ColumnCount + 1;
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

        column =
            column * 26
            + character.toUpper().unicode()
            - QLatin1Char('A').unicode()
            + 1;
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
    const QByteArray& compressed,
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
            const_cast<char*>(compressed.constData())
            );
    stream.avail_in =
        static_cast<uInt>(compressed.size());
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

    const QByteArray compressed =
        zipData.mid(
            dataOffset,
            static_cast<qsizetype>(entry.compressedSize)
            );

    if (entry.method == 0)
    {
        return compressed;
    }

    if (entry.method != 8)
    {
        return {};
    }

    return inflateRawDeflate(
        compressed,
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

QVector<Style> parseStyles(
    const QByteArray& xmlData
    )
{
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
                currentFilled = true;
                currentFill =
                    normalizedColor(
                        xml.attributes()
                            .value(QStringLiteral("rgb"))
                            .toString()
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
                    normalizedColor(
                        xml.attributes()
                            .value(QStringLiteral("rgb"))
                            .toString()
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
                fillFlags.append(currentFilled);
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
    const QHash<int, QString>& notesByPosition
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
        cell.row =
            spreadsheetRow(reference);
        cell.column =
            spreadsheetColumn(reference);
        cell.note =
            notesByPosition
                .value(cell.row * CellPositionStride + cell.column)
                .trimmed();

        if (
            cell.row <= 0
            || cell.row > LastMeaningfulRow
            || cell.column <= 0
            || cell.column > ColumnCount
            )
        {
            xml.skipCurrentElement();
            continue;
        }

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

QHash<int, QString> parseCellNotes(
    const QByteArray& xmlData
    )
{
    QHash<int, QString> notes;
    QXmlStreamReader xml(xmlData);
    QString currentElement;
    int currentPosition = 0;
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
                const int row =
                    spreadsheetRow(reference);
                const int column =
                    spreadsheetColumn(reference);

                currentElement =
                    name.toString();
                currentPosition =
                    row > 0 && column > 0
                        ? row * CellPositionStride + column
                        : 0;
                currentText.clear();
            }
            else if (
                currentPosition > 0
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

                if (currentPosition > 0 && !note.isEmpty())
                {
                    notes.insert(
                        currentPosition,
                        note
                        );
                }

                currentElement.clear();
                currentPosition = 0;
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

QHash<int, QString> workbookCellNotes(
    const QByteArray& data,
    const QHash<QString, ZipEntry>& entries,
    const QString& worksheetEntryName
    )
{
    QHash<int, QString> notes;
    const QStringList entryNames =
        worksheetNoteEntryNames(data, entries, worksheetEntryName);

    for (const QString& name : entryNames)
    {
        if (!entries.contains(name))
        {
            continue;
        }

        const QHash<int, QString> parsed =
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

Workbook parseWorkbook(
    const QByteArray& data,
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
            return {};
        }
    }

    Workbook workbook;
    if (entries.contains(QStringLiteral("xl/sharedStrings.xml")))
    {
        workbook.sharedStrings =
            parseSharedStrings(
                zipFileData(
                    data,
                    entries.value(QStringLiteral("xl/sharedStrings.xml"))
                    )
                );
    }
    if (entries.contains(QStringLiteral("xl/styles.xml")))
    {
        workbook.styles =
            parseStyles(
                zipFileData(
                    data,
                    entries.value(QStringLiteral("xl/styles.xml"))
                    )
                );
    }
    if (workbook.styles.isEmpty())
    {
        workbook.styles.append(Style{});
    }
    const QVector<WorksheetReference> worksheetReferences =
        parseWorksheetReferences(
            zipFileData(data, entries.value(QStringLiteral("xl/workbook.xml")))
            );
    const QHash<QString, QString> relationshipTargets =
        parseWorkbookRelationships(
            zipFileData(data, entries.value(QStringLiteral("xl/_rels/workbook.xml.rels")))
            );

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
            return {};
        }

        Worksheet worksheet;
        worksheet.name = reference.name;
        worksheet.cells = parseSheet(
            zipFileData(data, entries.value(entryName)),
            workbook.sharedStrings,
            workbookCellNotes(data, entries, entryName)
            );
        workbook.worksheets.append(worksheet);
    }

    if (!workbook.worksheets.isEmpty())
    {
        workbook.cells = workbook.worksheets.first().cells;
    }

    const bool allWorksheetsEmpty = std::all_of(
        workbook.worksheets.cbegin(), workbook.worksheets.cend(),
        [](const Worksheet& worksheet) { return worksheet.cells.isEmpty(); });
    if (workbook.worksheets.isEmpty() || allWorksheetsEmpty)
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("The downloaded spreadsheet could not be read.");
        }
        return {};
    }

    return workbook;
}
}
