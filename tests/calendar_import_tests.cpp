#include "features/calendar/calendar_event_campus_filter.h"
#include "features/calendar/academic_calendar_event_parser.h"
#include "features/calendar/calendar_workbook_reader.h"
#include "next/application/calendar_event_start_of_term_policy.h"

#include <QtTest>

#include <zlib.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
template<typename T>
concept HasFlatCellProjection = requires(T workbook) { workbook.cells; };

static_assert(!HasFlatCellProjection<CalendarImport::Workbook>,
    "Workbook cells must be owned only by worksheets.");
static_assert(std::is_same_v<
    decltype(CalendarImport::ParsedCalendarImport{}.skippedCount),
    ClassMngr::Next::Application::CalendarEventImportCount
    >);

CalendarImport::Workbook baseWorkbook()
{
    CalendarImport::Workbook workbook;
    workbook.worksheets.append(CalendarImport::Worksheet{});
    workbook.styles = {
        {},
        {QStringLiteral("CCCCCC"), QString()}
    };
    workbook.worksheets.first().cells = {
        {1, 1, 0, QStringLiteral("JULY"), QString()},
        {1, 2, 0, QStringLiteral("2026"), QString()}
    };

    return workbook;
}

void appendLittleEndian(
    QByteArray* data,
    quint32 value,
    int byteCount
    )
{
    for (int byte = 0; byte < byteCount; ++byte)
    {
        data->append(
            static_cast<char>((value >> (byte * 8)) & 0xff)
            );
    }
}

struct TestArchiveEntry
{
    QByteArray name;
    QByteArray contents;
    QByteArray archiveContents;
    quint32 crc = 0;
    quint32 localOffset = 0;
    quint16 method = 0;
};

QByteArray deflateRaw(
    const QByteArray& contents
    )
{
    z_stream stream{};
    if (
        deflateInit2(
            &stream,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -MAX_WBITS,
            8,
            Z_DEFAULT_STRATEGY
            ) != Z_OK
        )
    {
        return {};
    }

    QByteArray compressed;
    compressed.resize(
        static_cast<qsizetype>(
            deflateBound(
                &stream,
                static_cast<uLong>(contents.size())
                )
            )
        );
    stream.next_in =
        reinterpret_cast<Bytef*>(
            const_cast<char*>(contents.constData())
            );
    stream.avail_in =
        static_cast<uInt>(contents.size());
    stream.next_out =
        reinterpret_cast<Bytef*>(compressed.data());
    stream.avail_out =
        static_cast<uInt>(compressed.size());

    const int result =
        deflate(&stream, Z_FINISH);
    const qsizetype compressedSize =
        static_cast<qsizetype>(stream.total_out);
    deflateEnd(&stream);

    if (result != Z_STREAM_END)
    {
        return {};
    }

    compressed.truncate(compressedSize);
    return compressed;
}

QByteArray zipArchive(
    const QList<std::pair<QByteArray, QByteArray>>& files,
    bool deflated = false
    )
{
    QByteArray archive;
    QVector<TestArchiveEntry> entries;
    entries.reserve(files.size());

    for (const auto& [name, contents] : files)
    {
        TestArchiveEntry entry;
        entry.name = name;
        entry.contents = contents;
        entry.archiveContents = deflated
            ? deflateRaw(contents)
            : contents;
        entry.method = deflated ? 8 : 0;
        entry.crc = static_cast<quint32>(
            crc32(
                0L,
                reinterpret_cast<const Bytef*>(contents.constData()),
                static_cast<uInt>(contents.size())
                )
            );
        entry.localOffset = static_cast<quint32>(archive.size());

        appendLittleEndian(&archive, 0x04034b50, 4);
        appendLittleEndian(&archive, 20, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, entry.method, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, entry.crc, 4);
        appendLittleEndian(&archive, entry.archiveContents.size(), 4);
        appendLittleEndian(&archive, entry.contents.size(), 4);
        appendLittleEndian(&archive, entry.name.size(), 2);
        appendLittleEndian(&archive, 0, 2);
        archive.append(entry.name);
        archive.append(entry.archiveContents);
        entries.append(std::move(entry));
    }

    const quint32 centralDirectoryOffset =
        static_cast<quint32>(archive.size());
    for (const TestArchiveEntry& entry : entries)
    {
        appendLittleEndian(&archive, 0x02014b50, 4);
        appendLittleEndian(&archive, 20, 2);
        appendLittleEndian(&archive, 20, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, entry.method, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, entry.crc, 4);
        appendLittleEndian(&archive, entry.archiveContents.size(), 4);
        appendLittleEndian(&archive, entry.contents.size(), 4);
        appendLittleEndian(&archive, entry.name.size(), 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 4);
        appendLittleEndian(&archive, entry.localOffset, 4);
        archive.append(entry.name);
    }

    const quint32 centralDirectorySize =
        static_cast<quint32>(archive.size()) - centralDirectoryOffset;
    appendLittleEndian(&archive, 0x06054b50, 4);
    appendLittleEndian(&archive, 0, 2);
    appendLittleEndian(&archive, 0, 2);
    appendLittleEndian(&archive, entries.size(), 2);
    appendLittleEndian(&archive, entries.size(), 2);
    appendLittleEndian(&archive, centralDirectorySize, 4);
    appendLittleEndian(&archive, centralDirectoryOffset, 4);
    appendLittleEndian(&archive, 0, 2);

    return archive;
}

QByteArray calendarWorksheetXml(
    bool populated,
    const QByteArray& legendLabel = QByteArrayLiteral("New Semester")
    )
{
    if (!populated)
    {
        return QByteArrayLiteral(
            "<worksheet><sheetData/></worksheet>"
            );
    }

    QByteArray worksheet = QByteArrayLiteral(
        "<worksheet><sheetData>"
        "<row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>JULY</t></is></c>"
        "<c r=\"B1\" t=\"inlineStr\"><is><t>2026</t></is></c></row>"
        "<row r=\"4\"><c r=\"A4\" s=\"1\" t=\"inlineStr\"><is><t>6</t></is></c></row>"
        "<row r=\"20\"><c r=\"Z20\" s=\"1\" t=\"inlineStr\"><is><t>New Semester</t></is></c></row>"
        "</sheetData><mergeCells><mergeCell ref=\"A1:B1\"/></mergeCells></worksheet>"
        );
    worksheet.replace(QByteArrayLiteral("New Semester"), legendLabel);
    return worksheet;
}

QByteArray calendarWorkbookArchive(
    int worksheetCount,
    bool populatedWorksheets,
    bool omitLastWorksheet,
    bool malformedLastWorksheet = false,
    const QVector<QByteArray>& worksheetXmlContents = {}
    )
{
    QByteArray workbookXml = QByteArrayLiteral("<workbook><sheets>");
    QByteArray relationshipsXml = QByteArrayLiteral("<Relationships>");
    QList<std::pair<QByteArray, QByteArray>> files;
    for (int index = 0; index < worksheetCount; ++index)
    {
        const int sheetNumber = index + 1;
        const QByteArray suffix = QByteArray::number(sheetNumber);
        const QByteArray name =
            index == 0 ? QByteArrayLiteral("First")
                       : QByteArrayLiteral("Later");
        workbookXml.append(
            QByteArrayLiteral("<sheet name=\"") + name
            + QByteArrayLiteral("\" id=\"rId") + suffix
            + QByteArrayLiteral("\"/>")
            );
        relationshipsXml.append(
            QByteArrayLiteral("<Relationship Id=\"rId") + suffix
            + QByteArrayLiteral("\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet")
            + suffix + QByteArrayLiteral(".xml\"/>")
            );
        if (!(omitLastWorksheet && index == worksheetCount - 1))
        {
            QByteArray worksheetContents = index < worksheetXmlContents.size()
                ? worksheetXmlContents[index]
                : calendarWorksheetXml(populatedWorksheets);
            if (malformedLastWorksheet && index == worksheetCount - 1)
            {
                worksheetContents = QByteArrayLiteral(
                    "<worksheet><sheetData><row r=\"1\"><c r=\"A1\" "
                    "t=\"inlineStr\"><is><t>Broken"
                    );
            }
            files.append({
                QByteArrayLiteral("xl/worksheets/sheet") + suffix
                    + QByteArrayLiteral(".xml"),
                worksheetContents
            });
        }
    }
    workbookXml.append(QByteArrayLiteral("</sheets></workbook>"));
    relationshipsXml.append(QByteArrayLiteral("</Relationships>"));

    files.prepend({
        QByteArrayLiteral("xl/_rels/workbook.xml.rels"),
        relationshipsXml
    });
    files.prepend({
        QByteArrayLiteral("xl/workbook.xml"),
        workbookXml
    });
    if (populatedWorksheets)
    {
        files.append({
            QByteArrayLiteral("xl/sharedStrings.xml"),
            QByteArrayLiteral(
                "<sst><si><t>unused shared string</t></si></sst>"
                )
        });
        files.append({
            QByteArrayLiteral("xl/styles.xml"),
            QByteArrayLiteral(
                "<styleSheet><fonts><font/></fonts><fills>"
                "<fill><patternFill patternType=\"none\"/></fill>"
                "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFCCCCCC\"/></patternFill></fill>"
                "</fills><cellXfs><xf/><xf fillId=\"1\" fontId=\"0\"/></cellXfs></styleSheet>"
                )
        });
    }
    return zipArchive(files);
}

QByteArray singleSheetWorkbookArchive(
    const QByteArray& worksheetXml,
    bool withHighRowNote = false,
    bool deflated = false
    )
{
    QList<std::pair<QByteArray, QByteArray>> files{
        {
            QByteArrayLiteral("xl/workbook.xml"),
            QByteArrayLiteral(
                "<workbook><sheets><sheet name=\"First\" id=\"rId1\"/>"
                "</sheets></workbook>"
                )
        },
        {
            QByteArrayLiteral("xl/_rels/workbook.xml.rels"),
            QByteArrayLiteral(
                "<Relationships><Relationship Id=\"rId1\" "
                "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" "
                "Target=\"worksheets/sheet1.xml\"/>"
                "</Relationships>"
                )
        },
        {
            QByteArrayLiteral("xl/worksheets/sheet1.xml"),
            worksheetXml
        }
    };

    if (withHighRowNote)
    {
        files.append({
            QByteArrayLiteral("xl/worksheets/_rels/sheet1.xml.rels"),
            QByteArrayLiteral(
                "<Relationships><Relationship Id=\"rId2\" "
                "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments\" "
                "Target=\"../comments1.xml\"/>"
                "</Relationships>"
                )
        });
        files.append({
            QByteArrayLiteral("xl/comments1.xml"),
            QByteArrayLiteral(
                "<comments><commentList><comment ref=\"A1048576\">"
                "<text><t>Campus: BDG</t></text>"
                "</comment></commentList></comments>"
                )
        });
    }

    return zipArchive(files, deflated);
}
}

class CalendarImportTests : public QObject
{
    Q_OBJECT

private slots:
    void importsOnlyFirstWorksheetInCalendarOrder();
    void returnsNoEventsForMissingOrEmptyFirstWorksheet();
    void lastCellAtDuplicateCoordinateWinsInProjection();
    void calendarLookupDoesNotAliasWideColumns();
    void cancelledCalendarDatesAreSuppressed();
    void readerRejectsUnreadableWorkbook_data();
    void readerRejectsUnreadableWorkbook();
    void readerRejectsMalformedLaterWorksheetAndAllEmptyWorkbook();
    void readerPreservesStoredAndDeflatedWorkbookValuesAfterInputLifetime();
    void readerStreamsStoredAndDeflatedWorksheetRows();
    void readerStreamingPreservesCancellationAndLaterSheetFailure();
    void calendarByteParserUsesFirstWorksheetAndReportsReadCounts();
    void calendarByteParserDoesNotImportDistinctLaterWorksheetEvents();
    void calendarByteParserDiscardsPartialResultOnLaterSheetFailure_data();
    void calendarByteParserDiscardsPartialResultOnLaterSheetFailure();
    void calendarByteParserDiscardsSkippedCountOnLaterSheetFailure();
    void cellPositionsEnforceWorksheetBoundsAndHashByPair();
    void cellRangesAreTypedInclusiveAndRejectInvalidEndpoints();
    void readerSkipsMalformedMergedRangesAndPreservesValidBoundaries();
    void readerAttachesNotesAtTheMaximumRow();
    void ignoresWeekendLegendEntries();
    void importsNewSemesterLegendEntries();
    void importsWeekendFontColorOverrides();
    void importsWeekdayFontColorWithoutFill();
    void ignoresWeekdayFontColorWithFill();
    void importsShiftedFirstCalendarRow();
    void appendsCampusCodesFromCellNotes();
    void appendsVariableLengthCampusCodesFromCellNotes();
    void importSignatureUsesExactlyTheSixLegacyKeyFields();
    void importSignatureNormalizesTitleTypeAndTimeStatus();
    void importSignatureIgnoresTimesAndOtherNonKeyMetadata();
};

void CalendarImportTests::importsOnlyFirstWorksheetInCalendarOrder()
{
    CalendarImport::Workbook workbook = baseWorkbook();
    auto& firstSheet = workbook.worksheets.first();
    firstSheet.cells.append({20, 26, 1, QStringLiteral("New Semester"), {}});
    // Storage order differs from calendar order; output remains row/column order.
    firstSheet.cells.append({4, 2, 1, QStringLiteral("7"), {}});
    firstSheet.cells.append({4, 1, 1, QStringLiteral("6"), {}});
    CalendarImport::Worksheet laterSheet;
    laterSheet.cells = {
        {1, 1, 0, QStringLiteral("JUNE"), {}},
        {1, 2, 0, QStringLiteral("2027"), {}},
        {20, 26, 1, QStringLiteral("Later sheet event"), {}},
        {4, 1, 1, QStringLiteral("7"), {}}
    };
    workbook.worksheets.append(laterSheet);

    const auto parsed = CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QCOMPARE(parsed.skippedCount.rawValue(), 0);
    QCOMPARE(parsed.events.size(), 2);
    QCOMPARE(parsed.events.at(0).startDate, QDate(2026, 7, 6));
    QCOMPARE(parsed.events.at(1).startDate, QDate(2026, 7, 7));
    for (const auto& event : parsed.events)
        QCOMPARE(event.title, QStringLiteral("New Semester"));

    // Mutate the sole worksheet-owned source; there is no snapshot to synchronize.
    workbook.worksheets.first().cells[2].value = QStringLiteral("Updated semester");
    const auto updated = CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QCOMPARE(updated.skippedCount.rawValue(), 0);
    QCOMPARE(updated.events.size(), 2);
    QCOMPARE(updated.events.at(0).startDate, parsed.events.at(0).startDate);
    QCOMPARE(updated.events.at(1).startDate, parsed.events.at(1).startDate);
    for (const auto& event : updated.events)
        QCOMPARE(event.title, QStringLiteral("Updated semester"));
}

void CalendarImportTests::returnsNoEventsForMissingOrEmptyFirstWorksheet()
{
    CalendarImport::Workbook workbook;
    const auto missing = CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QVERIFY(missing.events.isEmpty());
    QCOMPARE(missing.skippedCount.rawValue(), 0);

    workbook = baseWorkbook();
    workbook.worksheets.first().cells.append(
        {20, 26, 1, QStringLiteral("New Semester"), {}});
    workbook.worksheets.first().cells.append({4, 1, 1, QStringLiteral("6"), {}});
    workbook.worksheets.prepend(CalendarImport::Worksheet{});
    const auto emptyFirst = CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QVERIFY(emptyFirst.events.isEmpty());
    QCOMPARE(emptyFirst.skippedCount.rawValue(), 0);
}

void CalendarImportTests::lastCellAtDuplicateCoordinateWinsInProjection()
{
    CalendarImport::Workbook workbook = baseWorkbook();
    auto& worksheet = workbook.worksheets.first();
    worksheet.cells.append(
        {20, 26, 1, QStringLiteral("New Semester"), QString()}
        );
    worksheet.cells.append({4, 2, 1, QStringLiteral("6"), QString()});
    worksheet.cells.append({4, 2, 1, QStringLiteral("7"), QString()});

    const auto parsed = CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 1);
    QCOMPARE(parsed.events.first().startDate, QDate(2026, 7, 7));
}

void CalendarImportTests::calendarLookupDoesNotAliasWideColumns()
{
    CalendarImport::Workbook workbook = baseWorkbook();
    auto& worksheet = workbook.worksheets.first();
    worksheet.cells.append({20, 26, 1, QStringLiteral("New Semester"), {}});
    worksheet.cells.append({3, 3, 1, QStringLiteral("1"), {}});
    worksheet.cells.append({4, 1, 1, QStringLiteral("6"), {}});
    // This month label collided with the July 6 cell under the old row * 100 key.
    worksheet.cells.append({3, 101, 0, QStringLiteral("AUGUST"), {}});

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 2);
    QCOMPARE(parsed.events.at(0).startDate, QDate(2026, 7, 1));
    QCOMPARE(parsed.events.at(1).startDate, QDate(2026, 7, 6));
}

void CalendarImportTests::cancelledCalendarDatesAreSuppressed()
{
    CalendarImport::Workbook workbook = baseWorkbook();
    auto& worksheet = workbook.worksheets.first();
    worksheet.cells.append(
        {20, 26, 1, QStringLiteral("New Semester"), QString()}
        );
    worksheet.cells.append({4, 1, 1, QStringLiteral("6"), QString()});
    worksheet.cells.append({9, 1, 0, QStringLiteral("6 - CANCELLED"), {}});

    const auto parsed = CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QVERIFY(parsed.events.isEmpty());
    QCOMPARE(parsed.skippedCount.rawValue(), 0);
}

void CalendarImportTests::readerRejectsUnreadableWorkbook_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("not-a-zip") << QByteArrayLiteral("not a workbook");
    QTest::newRow("truncated-zip") << QByteArray::fromHex("504b030414000000");
}

void CalendarImportTests::readerRejectsUnreadableWorkbook()
{
    QFETCH(QByteArray, bytes);
    QString error;
    const auto workbook = CalendarImport::parseWorkbook(bytes, &error);
    QCOMPARE(error, QStringLiteral("The downloaded spreadsheet is missing xl/workbook.xml."));
    QVERIFY(workbook.worksheets.isEmpty());
    const auto parsed = CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QVERIFY(parsed.events.isEmpty());
    QCOMPARE(parsed.skippedCount.rawValue(), 0);
}

void CalendarImportTests
    ::readerPreservesStoredAndDeflatedWorkbookValuesAfterInputLifetime()
{
    const QByteArray worksheetXml = QByteArrayLiteral(
        "<worksheet><sheetData>"
        "<row r=\"1\"><c r=\"A1\" t=\"inlineStr\">"
        "<is><t xml:space=\"preserve\"> First &amp; final </t></is></c></row>"
        "<row r=\"1048576\"><c r=\"A1048576\" t=\"inlineStr\"><is><t>Spring</t></is></c></row>"
        "</sheetData><mergeCells><mergeCell ref=\"A1:B2\"/></mergeCells></worksheet>"
        );

    QString storedError;
    const CalendarImport::Workbook stored =
        CalendarImport::parseWorkbook(
            singleSheetWorkbookArchive(worksheetXml, true),
            &storedError
            );
    QVERIFY2(storedError.isEmpty(), qPrintable(storedError));
    QVERIFY(!stored.worksheets.isEmpty());

    QString deflatedError;
    const CalendarImport::Workbook deflated =
        CalendarImport::parseWorkbook(
            singleSheetWorkbookArchive(worksheetXml, true, true),
            &deflatedError
            );
    QVERIFY2(deflatedError.isEmpty(), qPrintable(deflatedError));
    QCOMPARE(deflated.worksheets.size(), stored.worksheets.size());
    QCOMPARE(deflated.sharedStrings, stored.sharedStrings);
    QCOMPARE(deflated.styles.size(), stored.styles.size());

    const CalendarImport::Worksheet& storedSheet =
        stored.worksheets.first();
    const CalendarImport::Worksheet& deflatedSheet =
        deflated.worksheets.first();
    QCOMPARE(deflatedSheet.name, storedSheet.name);
    QCOMPARE(deflatedSheet.visible, storedSheet.visible);
    QCOMPARE(deflatedSheet.cells.size(), 2);
    QCOMPARE(deflatedSheet.cells.size(), storedSheet.cells.size());
    QCOMPARE(
        deflatedSheet.cells.at(0).position,
        (CalendarImport::CellPosition{1, 1})
        );
    QCOMPARE(deflatedSheet.cells.at(0).position, storedSheet.cells.at(0).position);
    QCOMPARE(deflatedSheet.cells.at(0).style, storedSheet.cells.at(0).style);
    QCOMPARE(deflatedSheet.cells.at(0).value, QStringLiteral(" First & final "));
    QCOMPARE(deflatedSheet.cells.at(0).value, storedSheet.cells.at(0).value);
    QCOMPARE(deflatedSheet.cells.at(0).note, storedSheet.cells.at(0).note);
    QCOMPARE(
        deflatedSheet.cells.at(1).position,
        (CalendarImport::CellPosition{1048576, 1})
        );
    QCOMPARE(deflatedSheet.cells.at(1).position, storedSheet.cells.at(1).position);
    QCOMPARE(deflatedSheet.cells.at(1).value, QStringLiteral("Spring"));
    QCOMPARE(deflatedSheet.cells.at(1).value, storedSheet.cells.at(1).value);
    QCOMPARE(deflatedSheet.cells.at(1).note, QStringLiteral("Campus: BDG"));
    QCOMPARE(deflatedSheet.cells.at(1).note, storedSheet.cells.at(1).note);
    QCOMPARE(deflatedSheet.mergedRanges.size(), 1);
    QCOMPARE(deflatedSheet.mergedRanges.size(), storedSheet.mergedRanges.size());
    QCOMPARE(
        deflatedSheet.mergedRanges.first().first,
        (CalendarImport::CellPosition{1, 1})
        );
    QCOMPARE(
        deflatedSheet.mergedRanges.first().last,
        (CalendarImport::CellPosition{2, 2})
        );
    QCOMPARE(
        deflatedSheet.mergedRanges.first().first,
        storedSheet.mergedRanges.first().first
        );
    QCOMPARE(
        deflatedSheet.mergedRanges.first().last,
        storedSheet.mergedRanges.first().last
        );
}

void CalendarImportTests::readerStreamsStoredAndDeflatedWorksheetRows()
{
    const QByteArray worksheetXml = QByteArrayLiteral(
        "<worksheet><sheetData>"
        "<row r=\"1\"><c r=\"A1\" t=\"inlineStr\">"
        "<is><t xml:space=\"preserve\"> First &amp; final </t></is></c></row>"
        "<row r=\"1048576\"><c r=\"A1048576\" s=\"2\" t=\"inlineStr\">"
        "<is><t>Spring</t></is></c></row>"
        "</sheetData><mergeCells><mergeCell ref=\"A1:B2\"/></mergeCells></worksheet>"
        );

    const auto streamRows =
        [&](const QByteArray& data,
            QVector<QVector<CalendarImport::Cell>>* discoveryRows,
            QVector<QVector<CalendarImport::Cell>>* extractionRows,
            bool* extractionSawMerges)
        {
            QString error;
            const bool succeeded = CalendarImport::forEachWorksheetRows(
                data,
                [&](
                    const CalendarImport::WorkbookMetadata&,
                    const CalendarImport::Worksheet& worksheet,
                    const CalendarImport::WorksheetRowPass& discoveryPass,
                    const CalendarImport::WorksheetRowPass& extractionPass
                    )
                {
                    if (!worksheet.cells.isEmpty())
                    {
                        return false;
                    }
                    if (!discoveryPass(
                            [discoveryRows](
                                const QVector<CalendarImport::Cell>& row
                                )
                            {
                                discoveryRows->append(row);
                                return true;
                            }
                            ))
                    {
                        return false;
                    }
                    if (
                        worksheet.mergedRanges.size() != 1
                        || worksheet.mergedRanges.first().first
                            != CalendarImport::CellPosition{1, 1}
                        )
                    {
                        return false;
                    }
                    *extractionSawMerges = true;
                    return extractionPass(
                        [extractionRows](
                            const QVector<CalendarImport::Cell>& row
                            )
                        {
                            extractionRows->append(row);
                            return true;
                        }
                        );
                },
                &error
                );
            QVERIFY2(succeeded, qPrintable(error));
            QVERIFY(error.isEmpty());
        };

    QVector<QVector<CalendarImport::Cell>> storedDiscoveryRows;
    QVector<QVector<CalendarImport::Cell>> storedExtractionRows;
    bool storedSawMerges = false;
    streamRows(
        singleSheetWorkbookArchive(worksheetXml, true),
        &storedDiscoveryRows,
        &storedExtractionRows,
        &storedSawMerges
        );
    QVector<QVector<CalendarImport::Cell>> deflatedDiscoveryRows;
    QVector<QVector<CalendarImport::Cell>> deflatedExtractionRows;
    bool deflatedSawMerges = false;
    streamRows(
        singleSheetWorkbookArchive(worksheetXml, true, true),
        &deflatedDiscoveryRows,
        &deflatedExtractionRows,
        &deflatedSawMerges
        );

    QVERIFY(storedSawMerges);
    QVERIFY(deflatedSawMerges);
    QCOMPARE(storedDiscoveryRows.size(), 2);
    QCOMPARE(storedExtractionRows.size(), storedDiscoveryRows.size());
    QCOMPARE(deflatedDiscoveryRows.size(), storedDiscoveryRows.size());
    QCOMPARE(deflatedExtractionRows.size(), storedExtractionRows.size());
    for (int rowIndex = 0; rowIndex < storedDiscoveryRows.size(); ++rowIndex)
    {
        const auto& stored = storedDiscoveryRows[rowIndex];
        const auto& deflated = deflatedDiscoveryRows[rowIndex];
        QCOMPARE(deflated.size(), stored.size());
        QCOMPARE(storedExtractionRows[rowIndex].size(), stored.size());
        QCOMPARE(deflatedExtractionRows[rowIndex].size(), stored.size());
        for (int cellIndex = 0; cellIndex < stored.size(); ++cellIndex)
        {
            const CalendarImport::Cell& expected = stored[cellIndex];
            const CalendarImport::Cell& actual = deflated[cellIndex];
            QCOMPARE(actual.position, expected.position);
            QCOMPARE(actual.style, expected.style);
            QCOMPARE(actual.value, expected.value);
            QCOMPARE(actual.note, expected.note);
            QCOMPARE(storedExtractionRows[rowIndex][cellIndex].value, expected.value);
            QCOMPARE(deflatedExtractionRows[rowIndex][cellIndex].value, expected.value);
        }
    }
    QCOMPARE(
        storedDiscoveryRows.last().first().position,
        (CalendarImport::CellPosition{1048576, 1})
        );
    QCOMPARE(storedDiscoveryRows.last().first().note, QStringLiteral("Campus: BDG"));
}

void CalendarImportTests
    ::readerStreamingPreservesCancellationAndLaterSheetFailure()
{
    QString cancellationError;
    int cancellationCallbacks = 0;
    const bool cancellationSucceeded =
        CalendarImport::forEachWorksheetRows(
            calendarWorkbookArchive(1, true, false),
            [&cancellationCallbacks](
                const CalendarImport::WorkbookMetadata&,
                const CalendarImport::Worksheet&,
                const CalendarImport::WorksheetRowPass& discoveryPass,
                const CalendarImport::WorksheetRowPass& extractionPass
                )
            {
                ++cancellationCallbacks;
                const bool discoverySucceeded = discoveryPass({});
                const bool extractionSucceeded = extractionPass(
                    [](const QVector<CalendarImport::Cell>&)
                    {
                        return false;
                    }
                    );
                return discoverySucceeded && extractionSucceeded;
            },
            &cancellationError
            );
    QVERIFY(!cancellationSucceeded);
    QCOMPARE(cancellationCallbacks, 1);
    QCOMPARE(
        cancellationError,
        QStringLiteral("The downloaded spreadsheet could not be read.")
        );

    QString laterSheetError;
    int completedWorksheets = 0;
    int completedPasses = 0;
    const bool laterSheetSucceeded =
        CalendarImport::forEachWorksheetRows(
            calendarWorkbookArchive(2, true, true),
            [&completedWorksheets, &completedPasses](
                const CalendarImport::WorkbookMetadata&,
                const CalendarImport::Worksheet&,
                const CalendarImport::WorksheetRowPass& discoveryPass,
                const CalendarImport::WorksheetRowPass& extractionPass
                )
            {
                ++completedWorksheets;
                const bool discoverySucceeded = discoveryPass({});
                const bool extractionSucceeded = extractionPass({});
                completedPasses +=
                    static_cast<int>(discoverySucceeded)
                    + static_cast<int>(extractionSucceeded);
                return discoverySucceeded && extractionSucceeded;
            },
            &laterSheetError
            );
    QVERIFY(!laterSheetSucceeded);
    QCOMPARE(completedWorksheets, 1);
    QCOMPARE(completedPasses, 2);
    QCOMPARE(
        laterSheetError,
        QStringLiteral("The spreadsheet is missing worksheet Later.")
        );
}

void CalendarImportTests
    ::calendarByteParserUsesFirstWorksheetAndReportsReadCounts()
{
    const QByteArray data = calendarWorkbookArchive(2, true, false);
    QString bufferedError;
    const CalendarImport::Workbook bufferedWorkbook =
        CalendarImport::parseWorkbook(data, &bufferedError);
    QVERIFY2(bufferedError.isEmpty(), qPrintable(bufferedError));
    QCOMPARE(bufferedWorkbook.worksheets.size(), 2);

    const CalendarImport::ParsedCalendarImport buffered =
        CalendarImport::parseCalendarEventsFromWorkbook(bufferedWorkbook);
    QCOMPARE(buffered.events.size(), 1);

    QString streamingError;
    CalendarImport::CalendarWorkbookReadCounts readCounts;
    const CalendarImport::ParsedCalendarImport streamed =
        CalendarImport::parseCalendarEventsFromWorkbookBytes(
            data,
            {},
            &streamingError,
            &readCounts
            );
    QVERIFY2(streamingError.isEmpty(), qPrintable(streamingError));
    QCOMPARE(streamed.events.size(), buffered.events.size());
    QCOMPARE(
        streamed.skippedCount.rawValue(),
        buffered.skippedCount.rawValue()
        );
    for (qsizetype index = 0; index < buffered.events.size(); ++index)
    {
        const CalendarEvent& expected = buffered.events[index];
        const CalendarEvent& actual = streamed.events[index];
        QCOMPARE(actual.title, expected.title);
        QCOMPARE(actual.eventType, expected.eventType);
        QCOMPARE(actual.startDate, expected.startDate);
        QCOMPARE(actual.endDate, expected.endDate);
        QCOMPARE(actual.allDay, expected.allDay);
        QCOMPARE(actual.timeStatus, expected.timeStatus);
    }

    int expectedCellCount = 0;
    int expectedMergedRangeCount = 0;
    for (const CalendarImport::Worksheet& worksheet :
         bufferedWorkbook.worksheets)
    {
        expectedCellCount += static_cast<int>(worksheet.cells.size());
        expectedMergedRangeCount +=
            static_cast<int>(worksheet.mergedRanges.size());
    }
    QCOMPARE(readCounts.worksheetCount, bufferedWorkbook.worksheets.size());
    QCOMPARE(readCounts.cellCount, expectedCellCount);
    QCOMPARE(readCounts.mergedRangeCount, expectedMergedRangeCount);
    QCOMPARE(readCounts.sharedStringCount, bufferedWorkbook.sharedStrings.size());
    QCOMPARE(readCounts.styleCount, bufferedWorkbook.styles.size());
    QCOMPARE(readCounts.cellCount, 8);
    QCOMPARE(readCounts.mergedRangeCount, 2);
    QCOMPARE(readCounts.sharedStringCount, 1);
    QCOMPARE(readCounts.styleCount, 2);
}

void CalendarImportTests::
calendarByteParserDoesNotImportDistinctLaterWorksheetEvents()
{
    const QVector<QByteArray> worksheetXmlContents{
        calendarWorksheetXml(true, QByteArrayLiteral("First Only")),
        calendarWorksheetXml(true, QByteArrayLiteral("Later Only"))
    };
    const QByteArray data = calendarWorkbookArchive(
        2,
        true,
        false,
        false,
        worksheetXmlContents
        );

    QString bufferedError;
    const CalendarImport::Workbook bufferedWorkbook =
        CalendarImport::parseWorkbook(data, &bufferedError);
    QVERIFY2(bufferedError.isEmpty(), qPrintable(bufferedError));
    const CalendarImport::ParsedCalendarImport buffered =
        CalendarImport::parseCalendarEventsFromWorkbook(bufferedWorkbook);
    QVERIFY(!buffered.events.isEmpty());
    QVERIFY(std::any_of(
        buffered.events.cbegin(),
        buffered.events.cend(),
        [](const CalendarEvent& event)
        {
            return event.title == QStringLiteral("First Only");
        }
        ));
    QVERIFY(std::none_of(
        buffered.events.cbegin(),
        buffered.events.cend(),
        [](const CalendarEvent& event)
        {
            return event.title == QStringLiteral("Later Only");
        }
        ));

    QString streamingError;
    CalendarImport::CalendarWorkbookReadCounts readCounts;
    const CalendarImport::ParsedCalendarImport streamed =
        CalendarImport::parseCalendarEventsFromWorkbookBytes(
            data,
            {},
            &streamingError,
            &readCounts
            );
    QVERIFY2(streamingError.isEmpty(), qPrintable(streamingError));
    QCOMPARE(streamed.events.size(), buffered.events.size());
    QCOMPARE(
        streamed.skippedCount.rawValue(),
        buffered.skippedCount.rawValue()
        );
    QVERIFY(std::any_of(
        streamed.events.cbegin(),
        streamed.events.cend(),
        [](const CalendarEvent& event)
        {
            return event.title == QStringLiteral("First Only");
        }
        ));
    QVERIFY(std::none_of(
        streamed.events.cbegin(),
        streamed.events.cend(),
        [](const CalendarEvent& event)
        {
            return event.title == QStringLiteral("Later Only");
        }
        ));

    int expectedCellCount = 0;
    int expectedMergedRangeCount = 0;
    for (const CalendarImport::Worksheet& worksheet :
         bufferedWorkbook.worksheets)
    {
        expectedCellCount += static_cast<int>(worksheet.cells.size());
        expectedMergedRangeCount +=
            static_cast<int>(worksheet.mergedRanges.size());
    }
    QCOMPARE(readCounts.worksheetCount, 2);
    QCOMPARE(readCounts.cellCount, expectedCellCount);
    QCOMPARE(readCounts.mergedRangeCount, expectedMergedRangeCount);
    QCOMPARE(readCounts.sharedStringCount, bufferedWorkbook.sharedStrings.size());
    QCOMPARE(readCounts.styleCount, bufferedWorkbook.styles.size());
    QCOMPARE(readCounts.cellCount, 8);
    QCOMPARE(readCounts.mergedRangeCount, 2);
    QCOMPARE(readCounts.sharedStringCount, 1);
    QCOMPARE(readCounts.styleCount, 2);
}

void CalendarImportTests::
calendarByteParserDiscardsPartialResultOnLaterSheetFailure_data()
{
    QTest::addColumn<bool>("omitLastWorksheet");
    QTest::addColumn<bool>("malformedLastWorksheet");
    QTest::newRow("missing later worksheet") << true << false;
    QTest::newRow("malformed later worksheet") << false << true;
}

void CalendarImportTests::
calendarByteParserDiscardsPartialResultOnLaterSheetFailure()
{
    QFETCH(bool, omitLastWorksheet);
    QFETCH(bool, malformedLastWorksheet);

    QString error;
    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbookBytes(
            calendarWorkbookArchive(
                2,
                true,
                omitLastWorksheet,
                malformedLastWorksheet
                ),
            {},
            &error
            );
    QVERIFY(!error.trimmed().isEmpty());
    QVERIFY(parsed.events.isEmpty());
    QCOMPARE(parsed.skippedCount.rawValue(), 0);
}

void CalendarImportTests::
calendarByteParserDiscardsSkippedCountOnLaterSheetFailure()
{
    const QByteArray skippedWorksheet =
        calendarWorksheetXml(true, QByteArrayLiteral("Weekend"));
    const QByteArray singleSheetData = calendarWorkbookArchive(
        1,
        true,
        false,
        false,
        {skippedWorksheet}
        );

    QString singleSheetError;
    const CalendarImport::Workbook singleSheetWorkbook =
        CalendarImport::parseWorkbook(singleSheetData, &singleSheetError);
    QVERIFY2(singleSheetError.isEmpty(), qPrintable(singleSheetError));
    const CalendarImport::ParsedCalendarImport singleSheetParsed =
        CalendarImport::parseCalendarEventsFromWorkbook(singleSheetWorkbook);
    QVERIFY(singleSheetParsed.events.isEmpty());
    QVERIFY(singleSheetParsed.skippedCount.rawValue() > 0);

    QString singleStreamError;
    const CalendarImport::ParsedCalendarImport singleSheetStreamed =
        CalendarImport::parseCalendarEventsFromWorkbookBytes(
            singleSheetData,
            {},
            &singleStreamError
            );
    QVERIFY2(singleStreamError.isEmpty(), qPrintable(singleStreamError));
    QVERIFY(singleSheetStreamed.events.isEmpty());
    QCOMPARE(singleSheetStreamed.skippedCount.rawValue(), 1);

    const QByteArray multiSheetData = calendarWorkbookArchive(
        2,
        true,
        true,
        false,
        {
            skippedWorksheet,
            calendarWorksheetXml(true, QByteArrayLiteral("Later Only"))
        }
        );
    QString streamingError;
    const CalendarImport::ParsedCalendarImport streamed =
        CalendarImport::parseCalendarEventsFromWorkbookBytes(
            multiSheetData,
            {},
            &streamingError
            );
    QVERIFY(!streamingError.trimmed().isEmpty());
    QVERIFY(streamed.events.isEmpty());
    QCOMPARE(streamed.skippedCount.rawValue(), 0);
}

void CalendarImportTests::readerRejectsMalformedLaterWorksheetAndAllEmptyWorkbook()
{
    const QByteArray malformedLater =
        calendarWorkbookArchive(2, true, true);
    CalendarImport::ParsedCalendarImport projection;
    int worksheetCallbacks = 0;
    QString error;
    const bool malformedSucceeded =
        CalendarImport::forEachWorksheet(
            malformedLater,
            [&projection, &worksheetCallbacks](
                const CalendarImport::WorkbookMetadata& metadata,
                const CalendarImport::Worksheet& worksheet
                )
            {
                ++worksheetCallbacks;
                if (worksheetCallbacks == 1)
                {
                    projection =
                        CalendarImport::parseCalendarEventsFromWorksheet(
                            metadata,
                            worksheet
                            );
                }
                return true;
            },
            &error
            );

    QVERIFY(!malformedSucceeded);
    QCOMPARE(worksheetCallbacks, 1);
    QCOMPARE(projection.events.size(), 1);
    QCOMPARE(error, QStringLiteral("The spreadsheet is missing worksheet Later."));

    error.clear();
    worksheetCallbacks = 0;
    bool worksheetsAreEmpty = true;
    const bool emptySucceeded =
        CalendarImport::forEachWorksheet(
            calendarWorkbookArchive(2, false, false),
            [&worksheetCallbacks, &worksheetsAreEmpty](
                const CalendarImport::WorkbookMetadata&,
                const CalendarImport::Worksheet& worksheet
                )
            {
                ++worksheetCallbacks;
                worksheetsAreEmpty =
                    worksheetsAreEmpty && worksheet.cells.isEmpty();
                return true;
            },
            &error
            );
    QVERIFY(!emptySucceeded);
    QCOMPARE(worksheetCallbacks, 2);
    QVERIFY(worksheetsAreEmpty);
    QCOMPARE(error, QStringLiteral("The downloaded spreadsheet could not be read."));
}

void CalendarImportTests::cellPositionsEnforceWorksheetBoundsAndHashByPair()
{
    using CalendarImport::CellPosition;

    QVERIFY((CellPosition{1, 1}.isValid()));
    QVERIFY((CellPosition{CellPosition::MaxRow, CellPosition::MaxColumn}.isValid()));
    QVERIFY(!CellPosition{}.isValid());
    QVERIFY((!CellPosition{0, 1}.isValid()));
    QVERIFY((!CellPosition{1, 0}.isValid()));
    QVERIFY((!CellPosition{-1, 1}.isValid()));
    QVERIFY((!CellPosition{1, -1}.isValid()));
    QVERIFY((!CellPosition{CellPosition::MaxRow + 1, 1}.isValid()));
    QVERIFY((!CellPosition{1, CellPosition::MaxColumn + 1}.isValid()));

    QHash<CellPosition, QString> cells;
    cells.insert({1, 101}, QStringLiteral("wide column"));
    cells.insert({2, 1}, QStringLiteral("next row"));
    cells.insert({1, 101}, QStringLiteral("last duplicate"));

    QCOMPARE(cells.size(), 2);
    QCOMPARE(cells.value({1, 101}), QStringLiteral("last duplicate"));
    QCOMPARE(cells.value({2, 1}), QStringLiteral("next row"));
}

void CalendarImportTests::cellRangesAreTypedInclusiveAndRejectInvalidEndpoints()
{
    using CalendarImport::CellPosition;
    using CalendarImport::CellRange;

    const CellRange range{{2, 3}, {4, 5}};
    QVERIFY(range.isValid());
    QVERIFY(range.contains({2, 3}));
    QVERIFY(range.contains({4, 5}));
    QVERIFY(range.contains({3, 4}));
    QVERIFY(!range.contains({1, 3}));
    QVERIFY(!range.contains({5, 5}));
    QVERIFY(!range.contains({3, 2}));
    QVERIFY(!range.contains({3, 6}));
    QVERIFY(!range.contains({}));

    const CellRange single{{1, 1}, {1, 1}};
    QVERIFY(single.isValid());
    QVERIFY(single.contains({1, 1}));
    QVERIFY(!single.contains({1, 2}));

    QVERIFY(!CellRange{}.isValid());
    QVERIFY(!CellRange{}.contains({1, 1}));
    QVERIFY(!(CellRange{{4, 3}, {3, 5}}).isValid());
    QVERIFY(!(CellRange{{2, 5}, {3, 4}}).isValid());
    QVERIFY(!(CellRange{{0, 1}, {1, 1}}).isValid());
    QVERIFY(!(CellRange{{1, 1}, {CellPosition::MaxRow + 1, 1}}).isValid());
    QVERIFY(!(CellRange{{1, 1}, {1, CellPosition::MaxColumn + 1}}).isValid());
}

void CalendarImportTests::readerSkipsMalformedMergedRangesAndPreservesValidBoundaries()
{
    const QByteArray worksheetXml = QByteArrayLiteral(
        "<worksheet><sheetData>"
        "<row r=\"1\"><c r=\"A1\"><v>1</v></c>"
        "<c r=\"B2\"><v>2</v></c><c r=\"C3\"><v>3</v></c>"
        "<c r=\"XFE1\"><v>4</v></c>"
        "<c r=\"XFD1048576\"><v>5</v></c></row>"
        "</sheetData><mergeCells>"
        "<mergeCell ref=\"A1\"/>"
        "<mergeCell ref=\"B2:C3\"/>"
        "<mergeCell ref=\"D4:C5\"/>"
        "<mergeCell ref=\"XFE1:XFE2\"/>"
        "<mergeCell ref=\"A1:XFE1\"/>"
        "<mergeCell ref=\"A1:A1048577\"/>"
        "<mergeCell ref=\"A1048576:XFD1048576\"/>"
        "</mergeCells></worksheet>"
        );
    QString error;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(
            singleSheetWorkbookArchive(worksheetXml),
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(workbook.worksheets.size(), 1);
    const CalendarImport::Worksheet& worksheet = workbook.worksheets.first();
    QCOMPARE(worksheet.cells.size(), 4);
    QCOMPARE(worksheet.cells.at(0).position, CalendarImport::CellPosition(1, 1));
    QCOMPARE(worksheet.cells.at(1).position, CalendarImport::CellPosition(2, 2));
    QCOMPARE(worksheet.cells.at(2).position, CalendarImport::CellPosition(3, 3));
    QCOMPARE(
        worksheet.cells.at(3).position,
        (CalendarImport::CellPosition{1048576, 16384})
        );

    QCOMPARE(worksheet.mergedRanges.size(), 3);
    QCOMPARE(worksheet.mergedRanges.at(0).first, (CalendarImport::CellPosition{1, 1}));
    QCOMPARE(worksheet.mergedRanges.at(0).last, (CalendarImport::CellPosition{1, 1}));
    QVERIFY(worksheet.mergedRanges.at(0).contains({1, 1}));
    QCOMPARE(worksheet.mergedRanges.at(1).first, (CalendarImport::CellPosition{2, 2}));
    QCOMPARE(worksheet.mergedRanges.at(1).last, (CalendarImport::CellPosition{3, 3}));
    QVERIFY(worksheet.mergedRanges.at(1).contains({3, 3}));
    QCOMPARE(
        worksheet.mergedRanges.at(2).last,
        (CalendarImport::CellPosition{1048576, 16384})
        );
}

void CalendarImportTests::readerAttachesNotesAtTheMaximumRow()
{
    const QByteArray worksheetXml = QByteArrayLiteral(
        "<worksheet><sheetData><row r=\"1048576\">"
        "<c r=\"A1048576\"><v>1</v></c>"
        "</row></sheetData></worksheet>"
        );
    QString error;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(
            singleSheetWorkbookArchive(worksheetXml, true),
            &error
            );

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(workbook.worksheets.size(), 1);
    QCOMPARE(workbook.worksheets.first().cells.size(), 1);
    const CalendarImport::Cell& cell =
        workbook.worksheets.first().cells.first();
    QCOMPARE(cell.position, (CalendarImport::CellPosition{1048576, 1}));
    QCOMPARE(cell.note, QStringLiteral("Campus: BDG"));
}

void CalendarImportTests::ignoresWeekendLegendEntries()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.worksheets.first().cells.append(
        {20, 26, 1, QStringLiteral("Weekend"), QString()}
        );
    workbook.worksheets.first().cells.append(
        {3, 6, 1, QStringLiteral("4"), QString()}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 0);
    QCOMPARE(parsed.skippedCount.rawValue(), 1);
}

void CalendarImportTests::importsNewSemesterLegendEntries()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.worksheets.first().cells.append(
        {20, 26, 1, QStringLiteral("New Semester"), QString()}
        );
    workbook.worksheets.first().cells.append(
        {4, 1, 1, QStringLiteral("6"), QString()}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.skippedCount.rawValue(), 0);
    QCOMPARE(parsed.events.size(), 1);
    QCOMPARE(parsed.events.first().title, QStringLiteral("New Semester"));
    QCOMPARE(parsed.events.first().eventType, QStringLiteral("Other"));
    const std::string title =
        parsed.events.first().title.toUtf8().toStdString();
    const std::string eventType =
        parsed.events.first().eventType.toUtf8().toStdString();
    QVERIFY(ClassMngr::Next::Application::
        CalendarEventStartOfTermPolicy::isStartOfTermEvent(
            title,
            eventType
            ));
    QCOMPARE(parsed.events.first().startDate, QDate(2026, 7, 6));
}

void CalendarImportTests::importsWeekendFontColorOverrides()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.styles.append(
        {QStringLiteral("FFCCCC"), QStringLiteral("FF0000")}
        );
    workbook.styles.append(
        {QStringLiteral("CCCCCC"), QStringLiteral("FF0000")}
        );
    workbook.worksheets.first().cells.append(
        {20, 26, 1, QStringLiteral("Weekend"), QString()}
        );
    workbook.worksheets.first().cells.append(
        {21, 26, 2, QStringLiteral("Red Day"), QString()}
        );
    workbook.worksheets.first().cells.append(
        {3, 6, 3, QStringLiteral("4"), QString()}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 1);
    QCOMPARE(parsed.events.first().title, QStringLiteral("Red Day"));
    QCOMPARE(parsed.events.first().eventType, QStringLiteral("Holiday"));
    QCOMPARE(parsed.events.first().startDate, QDate(2026, 7, 4));
}

void CalendarImportTests::importsWeekdayFontColorWithoutFill()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.styles.append(
        {QStringLiteral("FFCCCC"), QStringLiteral("FF0000")}
        );
    workbook.styles.append(
        {QString(), QStringLiteral("FF0000")}
        );
    workbook.worksheets.first().cells.append(
        {20, 26, 2, QStringLiteral("Red Day"), QString()}
        );
    workbook.worksheets.first().cells.append(
        {4, 1, 3, QStringLiteral("6"), QString()}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 1);
    QCOMPARE(parsed.events.first().title, QStringLiteral("Red Day"));
    QCOMPARE(parsed.events.first().eventType, QStringLiteral("Holiday"));
    QCOMPARE(parsed.events.first().startDate, QDate(2026, 7, 6));
}

void CalendarImportTests::ignoresWeekdayFontColorWithFill()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.styles.append(
        {QStringLiteral("FFCCCC"), QStringLiteral("FF0000")}
        );
    workbook.styles.append(
        {QStringLiteral("CCCCCC"), QStringLiteral("FF0000")}
        );
    workbook.worksheets.first().cells.append(
        {20, 26, 2, QStringLiteral("Red Day"), QString()}
        );
    workbook.worksheets.first().cells.append(
        {4, 1, 3, QStringLiteral("6"), QString()}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 0);
}

void CalendarImportTests::importsShiftedFirstCalendarRow()
{
    CalendarImport::Workbook workbook;
    workbook.worksheets.append(CalendarImport::Worksheet{});
    workbook.styles = {
        {},
        {QStringLiteral("FFF2CC"), QString()}
    };
    workbook.worksheets.first().cells = {
        {1, 1, 0, QStringLiteral("JUNE"), QString()},
        {1, 2, 0, QStringLiteral("2026"), QString()},
        {20, 26, 1, QStringLiteral("DYB Workshop"), QString()},
        {4, 1, 1, QStringLiteral("1"), QString()}
    };

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 1);
    QCOMPARE(parsed.events.first().title, QStringLiteral("DYB Workshop"));
    QCOMPARE(parsed.events.first().eventType, QStringLiteral("Workshop"));
    QCOMPARE(parsed.events.first().startDate, QDate(2026, 6, 1));
}

void CalendarImportTests::appendsCampusCodesFromCellNotes()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.worksheets.first().cells.append(
        {20, 26, 1, QStringLiteral("DYB Workshop"), QString()}
        );
    workbook.worksheets.first().cells.append(
        {4, 1, 1, QStringLiteral("6"), QStringLiteral("Campus: BDG")}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(
            workbook,
            {QStringLiteral("BDG")}
            );

    QCOMPARE(parsed.events.size(), 1);
    QCOMPARE(
        parsed.events.first().title,
        QStringLiteral("DYB Workshop (BDG)")
        );
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            parsed.events.first(),
            {QStringLiteral("BDG")},
            {QStringLiteral("BDG"), QStringLiteral("SNU")},
            false
            )
        );
    QVERIFY(
        !CalendarEventCampusFilter::eventMatchesCampus(
            parsed.events.first(),
            {QStringLiteral("SNU")},
            {QStringLiteral("BDG"), QStringLiteral("SNU")},
            false
            )
        );

    CalendarEvent normalizedBoundaryEvent = parsed.events.first();
    normalizedBoundaryEvent.title = QStringLiteral("  dyb workshop (s2)  ");
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            normalizedBoundaryEvent,
            {QStringLiteral(" s2 ")},
            {QStringLiteral("s2"), QStringLiteral("s20")},
            false
            )
        );
    normalizedBoundaryEvent.title = QStringLiteral("  dyb workshop (s20)  ");
    QVERIFY(
        !CalendarEventCampusFilter::eventMatchesCampus(
            normalizedBoundaryEvent,
            {QStringLiteral(" s2 ")},
            {QStringLiteral("s2"), QStringLiteral("s20")},
            false
            )
        );
    normalizedBoundaryEvent.title = QStringLiteral("  dyb workshop (xs2y)  ");
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            normalizedBoundaryEvent,
            {QStringLiteral("s2")},
            {QStringLiteral("s2")},
            false
            )
        );
    normalizedBoundaryEvent.title =
        QString(QChar(0x212A)) + QStringLiteral("S2");
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            normalizedBoundaryEvent,
            {QStringLiteral("BDG")},
            {QStringLiteral("S2")},
            false
            )
        );
    normalizedBoundaryEvent.title =
        QString(QChar(0x0131)) + QStringLiteral("S2");
    QVERIFY(
        !CalendarEventCampusFilter::eventMatchesCampus(
            normalizedBoundaryEvent,
            {QStringLiteral("BDG")},
            {QStringLiteral("S2")},
            false
            )
        );
}

void CalendarImportTests::appendsVariableLengthCampusCodesFromCellNotes()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.worksheets.first().cells.append(
        {20, 26, 1, QStringLiteral("DYB Workshop"), QString()}
        );
    workbook.worksheets.first().cells.append(
        {4, 1, 1, QStringLiteral("6"), QStringLiteral("Campus: S2")}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(
            workbook,
            {QStringLiteral("S2")}
            );

    QCOMPARE(parsed.events.size(), 1);
    QCOMPARE(
        parsed.events.first().title,
        QStringLiteral("DYB Workshop (S2)")
        );
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            parsed.events.first(),
            {QStringLiteral("S2")},
            {QStringLiteral("BDG"), QStringLiteral("S2")},
            false
            )
        );
    QVERIFY(
        !CalendarEventCampusFilter::eventMatchesCampus(
            parsed.events.first(),
            {QStringLiteral("BDG")},
            {QStringLiteral("BDG"), QStringLiteral("S2")},
            false
            )
        );
}

void CalendarImportTests::importSignatureUsesExactlyTheSixLegacyKeyFields()
{
    CalendarEvent event;
    event.title = QStringLiteral("Open House");
    event.eventType = QStringLiteral("Meeting");
    event.startDate = QDate(2026, 9, 23);
    event.endDate = QDate(2026, 9, 24);
    event.allDay = false;
    event.timeStatus = QStringLiteral("Timed");

    const auto signature =
        CalendarImport::calendarEventImportSignature(event);
    QCOMPARE(
        QString::fromStdU16String(signature.value()),
        QStringLiteral("Open House|Meeting|2026-09-23|2026-09-24|0|Timed")
        );

    auto changed = event;
    changed.title = QStringLiteral("Open House 2");
    QVERIFY(
        CalendarImport::calendarEventImportSignature(changed) != signature
        );

    changed = event;
    changed.eventType = QStringLiteral("Holiday");
    QVERIFY(
        CalendarImport::calendarEventImportSignature(changed) != signature
        );

    changed = event;
    changed.startDate = QDate(2026, 9, 22);
    QVERIFY(
        CalendarImport::calendarEventImportSignature(changed) != signature
        );

    changed = event;
    changed.endDate = QDate(2026, 9, 25);
    QVERIFY(
        CalendarImport::calendarEventImportSignature(changed) != signature
        );

    changed = event;
    changed.allDay = true;
    QVERIFY(
        CalendarImport::calendarEventImportSignature(changed) != signature
        );

    changed = event;
    changed.timeStatus = QStringLiteral("Unknown");
    QVERIFY(
        CalendarImport::calendarEventImportSignature(changed) != signature
        );
}

void CalendarImportTests::
importSignatureNormalizesTitleTypeAndTimeStatus()
{
    CalendarEvent normalized;
    normalized.title = QStringLiteral(" Open   House\n ");
    normalized.eventType = QStringLiteral(" Meeting ");
    normalized.startDate = QDate(2026, 9, 23);
    normalized.endDate = QDate(2026, 9, 23);
    normalized.timeStatus = QStringLiteral(" Timed ");

    CalendarEvent canonical = normalized;
    canonical.title = QStringLiteral("Open House");
    canonical.eventType = QStringLiteral("Meeting");
    canonical.timeStatus = QStringLiteral("Timed");
    QCOMPARE(
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(normalized).value()
            ),
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(canonical).value()
            )
        );

    normalized.eventType = QStringLiteral("not-a-calendar-type");
    canonical.eventType = QStringLiteral("Other");
    QCOMPARE(
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(normalized).value()
            ),
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(canonical).value()
            )
        );

    normalized.eventType = QStringLiteral("Meeting");
    canonical.eventType = QStringLiteral("Meeting");
    normalized.timeStatus = QStringLiteral("not-a-time-status");
    canonical.timeStatus = QStringLiteral("Timed");
    QCOMPARE(
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(normalized).value()
            ),
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(canonical).value()
            )
        );

    normalized.timeStatus = QStringLiteral("unknown");
    canonical.timeStatus = QStringLiteral("Timed");
    QCOMPARE(
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(normalized).value()
            ),
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(canonical).value()
            )
        );
}

void CalendarImportTests::
importSignatureIgnoresTimesAndOtherNonKeyMetadata()
{
    CalendarEvent event;
    event.title = QStringLiteral("Open House");
    event.eventType = QStringLiteral("Meeting");
    event.startDate = QDate(2026, 9, 23);
    event.endDate = QDate(2026, 9, 23);
    event.timeStatus = QStringLiteral("Timed");

    const auto signature =
        CalendarImport::calendarEventImportSignature(event);

    event.id = 42;
    event.repeatSeriesId = QStringLiteral("series-1");
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    QCOMPARE(
        QString::fromStdU16String(
            CalendarImport::calendarEventImportSignature(event).value()
            ),
        QString::fromStdU16String(signature.value())
        );
}

QTEST_APPLESS_MAIN(CalendarImportTests)

#include "calendar_import_tests.moc"
