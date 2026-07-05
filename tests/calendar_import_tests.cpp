#include "features/my_info/calendar_event_campus_filter.h"
#include "features/my_info/calendar_event_sheet_parser.h"
#include "features/my_info/calendar_import_workbook.h"

#include <QtTest>

namespace
{
CalendarImport::Workbook baseWorkbook()
{
    CalendarImport::Workbook workbook;
    workbook.styles = {
        {},
        {QStringLiteral("CCCCCC"), QString()}
    };
    workbook.cells = {
        {1, 1, 0, QStringLiteral("JULY"), QString()},
        {1, 2, 0, QStringLiteral("2026"), QString()}
    };

    return workbook;
}
}

class CalendarImportTests : public QObject
{
    Q_OBJECT

private slots:
    void ignoresWeekendLegendEntries();
    void importsWeekendFontColorOverrides();
    void importsWeekdayFontColorWithoutFill();
    void ignoresWeekdayFontColorWithFill();
    void importsShiftedFirstCalendarRow();
    void appendsCampusCodesFromCellNotes();
    void appendsVariableLengthCampusCodesFromCellNotes();
};

void CalendarImportTests::ignoresWeekendLegendEntries()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.cells.append(
        {20, 26, 1, QStringLiteral("Weekend"), QString()}
        );
    workbook.cells.append(
        {3, 6, 1, QStringLiteral("4"), QString()}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 0);
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
    workbook.cells.append(
        {20, 26, 1, QStringLiteral("Weekend"), QString()}
        );
    workbook.cells.append(
        {21, 26, 2, QStringLiteral("Red Day"), QString()}
        );
    workbook.cells.append(
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
    workbook.cells.append(
        {20, 26, 2, QStringLiteral("Red Day"), QString()}
        );
    workbook.cells.append(
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
    workbook.cells.append(
        {20, 26, 2, QStringLiteral("Red Day"), QString()}
        );
    workbook.cells.append(
        {4, 1, 3, QStringLiteral("6"), QString()}
        );

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

    QCOMPARE(parsed.events.size(), 0);
}

void CalendarImportTests::importsShiftedFirstCalendarRow()
{
    CalendarImport::Workbook workbook;
    workbook.styles = {
        {},
        {QStringLiteral("FFF2CC"), QString()}
    };
    workbook.cells = {
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
    workbook.cells.append(
        {20, 26, 1, QStringLiteral("DYB Workshop"), QString()}
        );
    workbook.cells.append(
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
}

void CalendarImportTests::appendsVariableLengthCampusCodesFromCellNotes()
{
    CalendarImport::Workbook workbook =
        baseWorkbook();
    workbook.cells.append(
        {20, 26, 1, QStringLiteral("DYB Workshop"), QString()}
        );
    workbook.cells.append(
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

QTEST_APPLESS_MAIN(CalendarImportTests)

#include "calendar_import_tests.moc"
