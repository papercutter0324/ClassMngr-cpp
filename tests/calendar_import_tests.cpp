#include "features/calendar/calendar_event_campus_filter.h"
#include "features/calendar/academic_calendar_event_parser.h"
#include "features/calendar/calendar_workbook_reader.h"
#include "next/application/calendar_event_start_of_term_policy.h"

#include <QtTest>

#include <zlib.h>

#include <string>
#include <utility>

namespace
{
template<typename T>
concept HasFlatCellProjection = requires(T workbook) { workbook.cells; };

static_assert(!HasFlatCellProjection<CalendarImport::Workbook>,
    "Workbook cells must be owned only by worksheets.");

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
    quint32 crc = 0;
    quint32 localOffset = 0;
};

QByteArray storedZipArchive(
    const QList<std::pair<QByteArray, QByteArray>>& files
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
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, entry.crc, 4);
        appendLittleEndian(&archive, entry.contents.size(), 4);
        appendLittleEndian(&archive, entry.contents.size(), 4);
        appendLittleEndian(&archive, entry.name.size(), 2);
        appendLittleEndian(&archive, 0, 2);
        archive.append(entry.name);
        archive.append(entry.contents);
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
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, 0, 2);
        appendLittleEndian(&archive, entry.crc, 4);
        appendLittleEndian(&archive, entry.contents.size(), 4);
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
    bool populated
    )
{
    if (!populated)
    {
        return QByteArrayLiteral(
            "<worksheet><sheetData/></worksheet>"
            );
    }

    return QByteArrayLiteral(
        "<worksheet><sheetData>"
        "<row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>JULY</t></is></c>"
        "<c r=\"B1\" t=\"inlineStr\"><is><t>2026</t></is></c></row>"
        "<row r=\"4\"><c r=\"A4\" s=\"1\" t=\"inlineStr\"><is><t>6</t></is></c></row>"
        "<row r=\"20\"><c r=\"Z20\" s=\"1\" t=\"inlineStr\"><is><t>New Semester</t></is></c></row>"
        "</sheetData></worksheet>"
        );
}

QByteArray calendarWorkbookArchive(
    int worksheetCount,
    bool populatedWorksheets,
    bool omitLastWorksheet
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
            files.append({
                QByteArrayLiteral("xl/worksheets/sheet") + suffix
                    + QByteArrayLiteral(".xml"),
                calendarWorksheetXml(populatedWorksheets)
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
            QByteArrayLiteral("xl/styles.xml"),
            QByteArrayLiteral(
                "<styleSheet><fonts><font/></fonts><fills>"
                "<fill><patternFill patternType=\"none\"/></fill>"
                "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFCCCCCC\"/></patternFill></fill>"
                "</fills><cellXfs><xf/><xf fillId=\"1\" fontId=\"0\"/></cellXfs></styleSheet>"
                )
        });
    }
    return storedZipArchive(files);
}
}

class CalendarImportTests : public QObject
{
    Q_OBJECT

private slots:
    void importsOnlyFirstWorksheetInCalendarOrder();
    void returnsNoEventsForMissingOrEmptyFirstWorksheet();
    void lastCellAtDuplicateCoordinateWinsInProjection();
    void cancelledCalendarDatesAreSuppressed();
    void readerRejectsUnreadableWorkbook_data();
    void readerRejectsUnreadableWorkbook();
    void readerRejectsMalformedLaterWorksheetAndAllEmptyWorkbook();
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
    QCOMPARE(parsed.skippedCount, 0);
    QCOMPARE(parsed.events.size(), 2);
    QCOMPARE(parsed.events.at(0).startDate, QDate(2026, 7, 6));
    QCOMPARE(parsed.events.at(1).startDate, QDate(2026, 7, 7));
    for (const auto& event : parsed.events)
        QCOMPARE(event.title, QStringLiteral("New Semester"));

    // Mutate the sole worksheet-owned source; there is no snapshot to synchronize.
    workbook.worksheets.first().cells[2].value = QStringLiteral("Updated semester");
    const auto updated = CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QCOMPARE(updated.skippedCount, 0);
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
    QCOMPARE(missing.skippedCount, 0);

    workbook = baseWorkbook();
    workbook.worksheets.first().cells.append(
        {20, 26, 1, QStringLiteral("New Semester"), {}});
    workbook.worksheets.first().cells.append({4, 1, 1, QStringLiteral("6"), {}});
    workbook.worksheets.prepend(CalendarImport::Worksheet{});
    const auto emptyFirst = CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QVERIFY(emptyFirst.events.isEmpty());
    QCOMPARE(emptyFirst.skippedCount, 0);
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
    QCOMPARE(parsed.skippedCount, 0);
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
    QCOMPARE(parsed.skippedCount, 0);
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

    QCOMPARE(parsed.skippedCount, 0);
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
