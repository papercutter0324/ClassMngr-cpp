#include "data/database/database_schema_manager.h"
#include "data/repositories/schedule_import_repository.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/schedule/import/schedule_workbook_parser.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTime>
#include <QUuid>
#include <QtTest>

#include <zlib.h>

class ScheduleImportTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesUsersMergesAndDiagnostics();
    void cancellationStopsWorkbookParsing();
    void rejectsAmbiguousIntensiveTimesWithCellDiagnostic();
    void convertsIntensiveTimesAcrossNoon();
    void appliesIntensiveSlotStatesSnapshot();
    void validatesCourseMeetingPatterns();
    void partitionsRepeatedCourseIntoValidClasses();
    void filtersClassOptionsByGradeAndDayGroup();
    void ranksTeacherAndClassMatches();
    void reportsScheduleInventoryStates_data();
    void reportsScheduleInventoryStates();
    void regularImportMatchesIntensiveOnlyClasses();
    void intensiveModesPreserveOrReplaceAbsentHours();
    void fullSnapshotPreservesUnrelatedData();
    void skippedExactMatchPreservesItsSchedule();
    void rejectsDuplicateExistingTargetsBeforeWrites();
    void conflictsRollBackBeforeWrites();
    void writeFailureRollsBackEveryChange();
    void validatesExternalWorkbookWhenProvided();
};

namespace
{
void appendLe16(
    QByteArray& data,
    quint16 value
    )
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(
    QByteArray& data,
    quint32 value
    )
{
    appendLe16(data, static_cast<quint16>(value & 0xffff));
    appendLe16(data, static_cast<quint16>((value >> 16) & 0xffff));
}

struct TestZipEntry
{
    QByteArray name;
    QByteArray contents;
    quint32 crc = 0;
    quint32 localOffset = 0;
};

QByteArray storedZip(
    QList<TestZipEntry> entries
    )
{
    QByteArray result;
    for (TestZipEntry& entry : entries)
    {
        entry.localOffset =
            static_cast<quint32>(result.size());
        entry.crc =
            static_cast<quint32>(
                crc32(
                    crc32(0L, Z_NULL, 0),
                    reinterpret_cast<const Bytef*>(
                        entry.contents.constData()
                        ),
                    static_cast<uInt>(
                        entry.contents.size()
                        )
                    )
                );
        appendLe32(result, 0x04034b50);
        appendLe16(result, 20);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe32(result, entry.crc);
        appendLe32(
            result,
            static_cast<quint32>(entry.contents.size())
            );
        appendLe32(
            result,
            static_cast<quint32>(entry.contents.size())
            );
        appendLe16(
            result,
            static_cast<quint16>(entry.name.size())
            );
        appendLe16(result, 0);
        result.append(entry.name);
        result.append(entry.contents);
    }

    const quint32 centralOffset =
        static_cast<quint32>(result.size());
    for (const TestZipEntry& entry : entries)
    {
        appendLe32(result, 0x02014b50);
        appendLe16(result, 20);
        appendLe16(result, 20);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe32(result, entry.crc);
        appendLe32(
            result,
            static_cast<quint32>(entry.contents.size())
            );
        appendLe32(
            result,
            static_cast<quint32>(entry.contents.size())
            );
        appendLe16(
            result,
            static_cast<quint16>(entry.name.size())
            );
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe32(result, 0);
        appendLe32(result, entry.localOffset);
        result.append(entry.name);
    }

    const quint32 centralSize =
        static_cast<quint32>(result.size())
        - centralOffset;
    appendLe32(result, 0x06054b50);
    appendLe16(result, 0);
    appendLe16(result, 0);
    appendLe16(
        result,
        static_cast<quint16>(entries.size())
        );
    appendLe16(
        result,
        static_cast<quint16>(entries.size())
        );
    appendLe32(result, centralSize);
    appendLe32(result, centralOffset);
    appendLe16(result, 0);
    return result;
}

QByteArray scheduleWorkbookData()
{
    const QByteArray workbook = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
                  xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
          <sheets>
            <sheet name="Current" sheetId="1" r:id="rId1"/>
            <sheet name="Archive" state="hidden" sheetId="2" r:id="rId2"/>
          </sheets>
        </workbook>)");
    const QByteArray relationships = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
          <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
          <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>
        </Relationships>)");
    const QByteArray styles = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <fonts count="1"><font><color rgb="FF000000"/></font></fonts>
          <fills count="4">
            <fill><patternFill patternType="none"/></fill>
            <fill><patternFill patternType="gray125"/></fill>
            <fill><patternFill patternType="solid"><fgColor rgb="FF6D9EEB"/></patternFill></fill>
            <fill><patternFill patternType="solid"><fgColor theme="4" tint="0.4"/></patternFill></fill>
          </fills>
          <cellXfs count="3">
            <xf fontId="0" fillId="0"/>
            <xf fontId="0" fillId="2"/>
            <xf fontId="0" fillId="3"/>
          </cellXfs>
        </styleSheet>)");
    const QByteArray theme = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">
          <a:themeElements>
            <a:clrScheme name="Test">
              <a:accent1><a:srgbClr val="4F81BD"/></a:accent1>
            </a:clrScheme>
          </a:themeElements>
        </a:theme>)");
    const QByteArray sheet1 = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <sheetData>
            <row r="1">
              <c r="A1" t="inlineStr"><is><t>Alice</t></is></c>
              <c r="B1" t="inlineStr"><is><t>월(MON)</t></is></c>
              <c r="C1" t="inlineStr"><is><t>화(TUE)</t></is></c>
              <c r="D1" t="inlineStr"><is><t>수(WED)</t></is></c>
              <c r="E1" t="inlineStr"><is><t>목(THU)</t></is></c>
              <c r="F1" t="inlineStr"><is><t>금(FRI)</t></is></c>
              <c r="H1" t="inlineStr"><is><t>Sub</t></is></c>
              <c r="I1" t="inlineStr"><is><t>MON</t></is></c>
              <c r="J1" t="inlineStr"><is><t>TUE</t></is></c>
              <c r="K1" t="inlineStr"><is><t>WED</t></is></c>
              <c r="L1" t="inlineStr"><is><t>THU</t></is></c>
              <c r="M1" t="inlineStr"><is><t>FRI</t></is></c>
              <c r="O1" t="inlineStr"><is><t>Bob</t></is></c>
              <c r="P1" t="inlineStr"><is><t>MON</t></is></c>
              <c r="Q1" t="inlineStr"><is><t>TUE</t></is></c>
              <c r="R1" t="inlineStr"><is><t>WED</t></is></c>
              <c r="S1" t="inlineStr"><is><t>THU</t></is></c>
              <c r="T1" t="inlineStr"><is><t>FRI</t></is></c>
            </row>
            <row r="2">
              <c r="A2" t="inlineStr"><is><t>4:00~4:55</t></is></c>
              <c r="B2" s="1" t="inlineStr"><is><t>홍길동TR (413)&#10;E5-Zeus</t></is></c>
              <c r="C2" t="inlineStr"><is><t>Meeting</t></is></c>
              <c r="D2" s="1" t="inlineStr"><is><t>홍길동 (413) 4:15~5:00&#10;E5-Zeus</t></is></c>
              <c r="H2" t="inlineStr"><is><t>4:00~4:55</t></is></c>
              <c r="I2" t="inlineStr"><is><t>ESSAY</t></is></c>
              <c r="O2" t="inlineStr"><is><t>4:00~4:55</t></is></c>
              <c r="P2" s="2" t="inlineStr"><is><t>김하늘 (414)&#10;E5-Apollo</t></is></c>
            </row>
            <row r="3">
              <c r="A3" t="inlineStr"><is><t>5:00~5:55</t></is></c>
              <c r="H3" t="inlineStr"><is><t>5:00~5:55</t></is></c>
              <c r="I3" t="inlineStr"><is><t>ESSAY</t></is></c>
              <c r="O3" t="inlineStr"><is><t>5:00~5:55</t></is></c>
            </row>
          </sheetData>
          <mergeCells count="1"><mergeCell ref="B2:B3"/></mergeCells>
        </worksheet>)");
    const QByteArray sheet2 = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <sheetData><row r="1"><c r="A1" t="inlineStr"><is><t>Hidden data</t></is></c></row></sheetData>
        </worksheet>)");

    return storedZip({
        {QByteArrayLiteral("xl/workbook.xml"), workbook},
        {QByteArrayLiteral("xl/_rels/workbook.xml.rels"), relationships},
        {QByteArrayLiteral("xl/styles.xml"), styles},
        {QByteArrayLiteral("xl/theme/theme1.xml"), theme},
        {QByteArrayLiteral("xl/worksheets/sheet1.xml"), sheet1},
        {QByteArrayLiteral("xl/worksheets/sheet2.xml"), sheet2}
    });
}

QByteArray singleSheetWorkbookData(
    const QByteArray& sheet
    )
{
    const QByteArray workbook = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
                  xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
          <sheets><sheet name="Intensive" sheetId="1" r:id="rId1"/></sheets>
        </workbook>)");
    const QByteArray relationships = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
          <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
        </Relationships>)");
    return storedZip({
        {QByteArrayLiteral("xl/workbook.xml"), workbook},
        {QByteArrayLiteral("xl/_rels/workbook.xml.rels"), relationships},
        {QByteArrayLiteral("xl/worksheets/sheet1.xml"), sheet}
    });
}

void execOrFail(
    QSqlQuery& query,
    const QString& sql
    )
{
    if (!query.exec(sql))
    {
        QFAIL(qPrintable(query.lastError().text()));
    }
}
}

void ScheduleImportTests::parsesUsersMergesAndDiagnostics()
{
    const auto parsed =
        parseScheduleImportWorkbook(
            scheduleWorkbookData(),
            ScheduleImportKind::Normal
            );
    const QString parseError =
        parsed.has_value() ? QString() : parsed.error();
    QVERIFY2(parsed.has_value(), qPrintable(parseError));
    QCOMPARE(parsed->sheets.size(), 2);
    QVERIFY(parsed->sheets.first().visible);
    QVERIFY(!parsed->sheets.last().visible);
    QCOMPARE(parsed->sheets.first().users.size(), 2);
    QCOMPARE(
        parsed->sheets.first().users.last().name,
        QStringLiteral("Bob")
        );
    QCOMPARE(
        parsed->sheets.first()
            .users.last().classes.first().importedColors,
        QStringList{QStringLiteral("#95B3D7")}
        );
    QVERIFY(
        !parsed->sheets.first()
            .users.last().classes.first()
            .meetingPatternError.isEmpty()
        );

    const ScheduleImportUserBlock& user =
        parsed->sheets.first().users.first();
    QCOMPARE(user.name, QStringLiteral("Alice"));
    QCOMPARE(user.classes.size(), 1);
    QCOMPARE(user.diagnostics.size(), 1);
    QCOMPARE(
        user.diagnostics.first().cellReference,
        QStringLiteral("C2")
        );

    const ScheduleImportClassCandidate& candidate =
        user.classes.first();
    QCOMPARE(candidate.teacherKr, QStringLiteral("홍길동"));
    QCOMPARE(candidate.rooms, QStringList{QStringLiteral("413")});
    QCOMPARE(
        candidate.importedColors,
        QStringList{QStringLiteral("#6D9EEB")}
        );
    QVERIFY(candidate.meetingPatternError.isEmpty());
    QCOMPARE(candidate.classGrade, QStringLiteral("E5"));
    QCOMPARE(candidate.classLevel, QStringLiteral("Zeus"));
    QCOMPARE(candidate.times.size(), 2);
    QCOMPARE(candidate.times.first().day, QStringLiteral("Monday"));
    QCOMPARE(candidate.times.first().startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(candidate.times.first().endTime, QStringLiteral("5:55 PM"));
    QCOMPARE(candidate.times.last().day, QStringLiteral("Wednesday"));
    QCOMPARE(candidate.times.last().startTime, QStringLiteral("4:15 PM"));
    QCOMPARE(candidate.times.last().endTime, QStringLiteral("5:00 PM"));
    QCOMPARE(
        normalizedScheduleImportUserName(
            QStringLiteral(" Alice-Jones ")
            ),
        QStringLiteral("alicejones")
        );
}

void ScheduleImportTests::cancellationStopsWorkbookParsing()
{
    int cancellationChecks = 0;
    const auto parsed = parseScheduleImportWorkbook(
        scheduleWorkbookData(),
        ScheduleImportKind::Normal,
        [&cancellationChecks]()
        {
            ++cancellationChecks;
            return cancellationChecks >= 1;
        }
        );

    QVERIFY(!parsed.has_value());
    QVERIFY(parsed.error().contains(QStringLiteral("cancelled")));
    QCOMPARE(cancellationChecks, 1);
}

void ScheduleImportTests
    ::rejectsAmbiguousIntensiveTimesWithCellDiagnostic()
{
    QByteArray workbook =
        scheduleWorkbookData();
    workbook.replace(
        QByteArrayLiteral("4:00~4:55"),
        QByteArrayLiteral("9:00~9:55")
        );
    workbook.replace(
        QByteArrayLiteral("5:00~5:55"),
        QByteArrayLiteral("9:30~9:55")
        );

    const auto parsed =
        parseScheduleImportWorkbook(
            workbook,
            ScheduleImportKind::Intensive
            );
    QVERIFY(!parsed.has_value());
    QVERIFY(parsed.error().contains(QStringLiteral("B2")));
    QVERIFY(
        parsed.error().contains(
            QStringLiteral("AM-to-PM")
            )
    );
}

void ScheduleImportTests::convertsIntensiveTimesAcrossNoon()
{
    const QByteArray sheet = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <sheetData>
            <row r="1">
              <c r="A1" t="inlineStr"><is><t>Alice</t></is></c>
              <c r="B1" t="inlineStr"><is><t>월</t></is></c>
              <c r="C1" t="inlineStr"><is><t>화</t></is></c>
              <c r="D1" t="inlineStr"><is><t>수</t></is></c>
              <c r="E1" t="inlineStr"><is><t>목</t></is></c>
              <c r="F1" t="inlineStr"><is><t>금</t></is></c>
            </row>
            <row r="2">
              <c r="A2" t="inlineStr"><is><t>9:00~9:55</t></is></c>
              <c r="C2" t="inlineStr"><is><t>홍 길동TR (413)&#10;E6-Song's</t></is></c>
            </row>
            <row r="3"><c r="A3" t="inlineStr"><is><t>10:00~10:55</t></is></c></row>
            <row r="4">
              <c r="A4" t="inlineStr"><is><t>11:00~11:55</t></is></c>
              <c r="B4" t="inlineStr"><is><t>Lunch</t></is></c>
            </row>
            <row r="5"><c r="A5" t="inlineStr"><is><t>12:00~12:55</t></is></c></row>
            <row r="6">
              <c r="A6" t="inlineStr"><is><t>1:00~1:55</t></is></c>
              <c r="E6" t="inlineStr"><is><t>홍길동 (413)&#10;E6-Song's</t></is></c>
            </row>
          </sheetData>
        </worksheet>)");

    const auto parsed =
        parseScheduleImportWorkbook(
            singleSheetWorkbookData(sheet),
            ScheduleImportKind::Intensive
            );
    const QString error =
        parsed.has_value() ? QString() : parsed.error();
    QVERIFY2(parsed.has_value(), qPrintable(error));
    QCOMPARE(parsed->sheets.first().users.size(), 1);
    const ScheduleImportClassCandidate& candidate =
        parsed->sheets.first().users.first().classes.first();
    QCOMPARE(candidate.classLevel, QStringLiteral("Song's"));
    QCOMPARE(candidate.teacherKr, QStringLiteral("홍길동"));
    QCOMPARE(candidate.times.size(), 2);
    QVERIFY(candidate.meetingPatternError.isEmpty());
    QCOMPARE(candidate.times.first().startTime, QStringLiteral("9:00 AM"));
    QCOMPARE(candidate.times.last().startTime, QStringLiteral("1:00 PM"));

    const auto stateFor =
        [&parsed](const QString& day, const QString& startTime)
        {
            for (const IntensiveSlotState& state :
                 parsed->sheets.first().users.first().intensiveSlotStates)
            {
                if (state.day == day && state.startTime == startTime)
                {
                    return state.state;
                }
            }
            return QString();
        };
    QCOMPARE(
        parsed->sheets.first().users.first().intensiveSlotStates.size(),
        65
        );
    QCOMPARE(
        stateFor(QStringLiteral("Monday"), QStringLiteral("11:00")),
        QStringLiteral("lunch")
        );
    QCOMPARE(
        stateFor(QStringLiteral("Monday"), QStringLiteral("10:00")),
        QStringLiteral("empty")
        );
    QCOMPARE(
        stateFor(QStringLiteral("Friday"), QStringLiteral("20:00")),
        QStringLiteral("empty")
        );
}

void ScheduleImportTests::appliesIntensiveSlotStatesSnapshot()
{
    const QString connectionName =
        QStringLiteral("schedule-import-intensive-states-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO intensive_slot_states "
                "(day, start_time, state) "
                "VALUES ('Friday', '20:00', 'lunch')"
                )
            );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("김하늘");
        candidate.teacherKr = QStringLiteral("김하늘");
        candidate.rooms = {QStringLiteral("413")};
        candidate.classGrade = QStringLiteral("E5");
        candidate.classLevel = QStringLiteral("Zeus");
        candidate.times = {
            {
                QStringLiteral("Monday"),
                QStringLiteral("9:00 AM"),
                QStringLiteral("9:55 AM")
            },
            {
                QStringLiteral("Wednesday"),
                QStringLiteral("9:00 AM"),
                QStringLiteral("9:55 AM")
            }
        };

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Intensive;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {candidate};
        plan.intensiveSlotStates = {
            {QStringLiteral("Monday"), QStringLiteral("09:00"), QStringLiteral("essay")},
            {QStringLiteral("Monday"), QStringLiteral("10:00"), QStringLiteral("lunch")},
            {QStringLiteral("Tuesday"), QStringLiteral("09:00"), QStringLiteral("empty")}
        };
        plan.teachers = {
            {
                QStringLiteral("김하늘"),
                ScheduleImportTeacherAction::Create,
                -1,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {0, ScheduleImportClassAction::CreateNew, -1}
        };

        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);
        const QString error =
            imported.has_value() ? QString() : imported.error();
        QVERIFY2(imported.has_value(), qPrintable(error));

        execOrFail(
            query,
            QStringLiteral(
                "SELECT day, start_time, state "
                "FROM intensive_slot_states ORDER BY day, start_time"
                )
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Monday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("09:00"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("essay"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Monday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("10:00"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("lunch"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Tuesday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("09:00"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("empty"));
        QVERIFY(!query.next());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::validatesCourseMeetingPatterns()
{
    const auto candidate =
        [](
            const QString& grade,
            const QString& level,
            const QStringList& days
            )
        {
            ScheduleImportClassCandidate result;
            result.classGrade = grade;
            result.classLevel = level;
            for (const QString& day : days)
            {
                result.times.append(
                    {
                        day,
                        QStringLiteral("4:00 PM"),
                        QStringLiteral("4:55 PM")
                    }
                    );
            }
            return result;
        };
    const auto valid =
        [&candidate](
            const QString& grade,
            const QString& level,
            const QStringList& days
            )
        {
            return scheduleImportMeetingPatternError(
                candidate(grade, level, days)
                ).isEmpty();
        };

    QVERIFY(valid(
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {QStringLiteral("Monday"), QStringLiteral("Wednesday")}
        ));
    QVERIFY(!valid(
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {QStringLiteral("Monday")}
        ));
    QVERIFY(valid(
        QStringLiteral("E5"),
        QStringLiteral("Athena"),
        {
            QStringLiteral("Monday"),
            QStringLiteral("Wednesday"),
            QStringLiteral("Friday")
        }
        ));
    QVERIFY(!valid(
        QStringLiteral("E5"),
        QStringLiteral("Athena"),
        {QStringLiteral("Monday"), QStringLiteral("Wednesday")}
        ));
    QVERIFY(valid(
        QStringLiteral("E6"),
        QStringLiteral("Hera"),
        {QStringLiteral("Friday")}
        ));
    QVERIFY(!valid(
        QStringLiteral("E6"),
        QStringLiteral("Hera"),
        {QStringLiteral("Monday"), QStringLiteral("Wednesday")}
        ));
    QVERIFY(valid(
        QStringLiteral("E6"),
        QStringLiteral("Song's"),
        {QStringLiteral("Tuesday"), QStringLiteral("Thursday")}
        ));
    QVERIFY(valid(
        QStringLiteral("M1"),
        QStringLiteral("Song's"),
        {QStringLiteral("Monday"), QStringLiteral("Friday")}
        ));
    QVERIFY(valid(
        QStringLiteral("M1"),
        QStringLiteral("Song's"),
        {QStringLiteral("Wednesday"), QStringLiteral("Friday")}
        ));
    QVERIFY(valid(
        QStringLiteral("M2"),
        QStringLiteral("Ursa"),
        {QStringLiteral("Tuesday")}
        ));
    QVERIFY(valid(
        QStringLiteral("M2"),
        QStringLiteral("Ursa"),
        {QStringLiteral("Thursday")}
        ));
    QVERIFY(!valid(
        QStringLiteral("M2"),
        QStringLiteral("Ursa"),
        {QStringLiteral("Tuesday"), QStringLiteral("Thursday")}
        ));
    QVERIFY(valid(
        QStringLiteral("M3"),
        QStringLiteral("Song's"),
        {QStringLiteral("Tuesday"), QStringLiteral("Thursday")}
        ));
    QVERIFY(valid(
        QStringLiteral("M3"),
        QStringLiteral("Zeus"),
        {
            QStringLiteral("Monday"),
            QStringLiteral("Tuesday"),
            QStringLiteral("Thursday")
        }
        ));
}

void ScheduleImportTests::partitionsRepeatedCourseIntoValidClasses()
{
    const QByteArray sheet = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <sheetData>
            <row r="1">
              <c r="A1" t="inlineStr"><is><t>Alice</t></is></c>
              <c r="B1" t="inlineStr"><is><t>MON</t></is></c>
              <c r="C1" t="inlineStr"><is><t>TUE</t></is></c>
              <c r="D1" t="inlineStr"><is><t>WED</t></is></c>
              <c r="E1" t="inlineStr"><is><t>THU</t></is></c>
              <c r="F1" t="inlineStr"><is><t>FRI</t></is></c>
            </row>
            <row r="2">
              <c r="A2" t="inlineStr"><is><t>4:00~4:55</t></is></c>
              <c r="B2" t="inlineStr"><is><t>홍길동 (413)&#10;E5-Zeus</t></is></c>
              <c r="C2" t="inlineStr"><is><t>홍길동 (414)&#10;E5-Zeus</t></is></c>
              <c r="E2" t="inlineStr"><is><t>홍길동 (414)&#10;E5-Zeus</t></is></c>
              <c r="F2" t="inlineStr"><is><t>홍길동 (413)&#10;E5-Zeus</t></is></c>
            </row>
          </sheetData>
        </worksheet>)");

    const auto parsed =
        parseScheduleImportWorkbook(
            singleSheetWorkbookData(sheet),
            ScheduleImportKind::Normal
            );
    const QString error =
        parsed.has_value() ? QString() : parsed.error();
    QVERIFY2(parsed.has_value(), qPrintable(error));

    const QList<ScheduleImportClassCandidate>& classes =
        parsed->sheets.first().users.first().classes;
    QCOMPARE(classes.size(), 2);

    QStringList patterns;
    for (const ScheduleImportClassCandidate& candidate : classes)
    {
        QVERIFY(candidate.meetingPatternError.isEmpty());
        QStringList days;
        for (const ClassTime& time : candidate.times)
        {
            days.append(time.day);
        }
        patterns.append(days.join(QLatin1Char('/')));
    }
    patterns.sort();
    QCOMPARE(
        patterns,
        QStringList({
            QStringLiteral("Monday/Friday"),
            QStringLiteral("Tuesday/Thursday")
        })
        );
}

void ScheduleImportTests::filtersClassOptionsByGradeAndDayGroup()
{
    ScheduleImportClassCandidate candidate;
    candidate.classGrade = QStringLiteral("E5");
    candidate.times = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
        }
    };

    ClassInfo existing;
    existing.classGrade = QStringLiteral("e5");
    existing.classTimes = {
        {
            QStringLiteral("Wednesday"),
            QStringLiteral("5:00 PM"),
            QStringLiteral("5:55 PM")
        }
    };
    QVERIFY(
        scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );
    QVERIFY(
        scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Intensive
            )
        );

    existing.classTimes.first().day =
        QStringLiteral("Tuesday");
    QVERIFY(
        !scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );

    candidate.times.first().day =
        QStringLiteral("Tuesday");
    existing.classTimes.first().day =
        QStringLiteral("Thursday");
    QVERIFY(
        scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );

    candidate.times.first().day =
        QStringLiteral("Monday");
    existing.classTimes.first().day =
        QStringLiteral("Wednesday");
    existing.intensiveTimes = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
        }
    };
    QVERIFY(
        !scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Intensive
            )
        );

    candidate.times.first().day =
        QStringLiteral("Thursday");
    QVERIFY(
        scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Intensive
            )
        );
    existing.classTimes.first().day =
        QStringLiteral("Tuesday");
    QVERIFY(
        scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );

    existing.classTimes.first().day =
        QStringLiteral("Friday");
    QVERIFY(
        !scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );

    existing.classTimes.first().day =
        QStringLiteral("Tuesday");
    existing.classGrade = QStringLiteral("E6");
    QVERIFY(
        !scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );

    existing.classGrade = QStringLiteral("E5");
    existing.classLevel = QStringLiteral("Zeus");
    candidate.classLevel = QStringLiteral("Zeus");
    existing.classTimes.clear();
    existing.intensiveTimes.first().day =
        QStringLiteral("Thursday");
    QVERIFY(
        scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );

    candidate.times.first().day =
        QStringLiteral("Monday");
    QVERIFY(
        !scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );

    existing.intensiveTimes.clear();
    QVERIFY(
        scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );
    existing.classLevel = QStringLiteral("Athena");
    QVERIFY(
        !scheduleImportClassOptionIsEligible(
            candidate,
            existing,
            ScheduleImportKind::Normal
            )
        );
}

void ScheduleImportTests::ranksTeacherAndClassMatches()
{
    const QString connectionName =
        QStringLiteral("schedule-import-matching-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES ('홍길동', '413')"
                )
            );
        const int matchingTeacherId =
            query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES ('김하늘', '414')"
                )
            );
        const int otherTeacherId =
            query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES ('홍길동', '999')"
                )
            );
        const int wrongRoomTeacherId =
            query.lastInsertId().toInt();

        const auto addClass =
            [&query](
                const QString& name,
                int teacherId,
                const QString& grade,
                const QString& level,
                const QString& day
                )
            {
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO classes (name) VALUES ('%1')"
                        )
                        .arg(name)
                    );
                const int classId =
                    query.lastInsertId().toInt();
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO class_info "
                        "(class_id, teacher_id, class_grade, class_level) "
                        "VALUES (%1, %2, '%3', '%4')"
                        )
                        .arg(classId)
                        .arg(teacherId)
                        .arg(grade, level)
                    );
                if (!day.isEmpty())
                {
                    execOrFail(
                        query,
                        QStringLiteral(
                            "INSERT INTO class_times "
                            "(class_id, day, start_time, end_time) "
                            "VALUES (%1, '%2', '4:00 PM', '4:55 PM')"
                            )
                            .arg(classId)
                            .arg(day)
                        );
                }
                return classId;
            };

        const int exact =
            addClass(
                QStringLiteral("1 Exact"),
                matchingTeacherId,
                QStringLiteral("E5"),
                QStringLiteral("Zeus"),
                QStringLiteral("Monday")
                );
        const int sameTeacherRoomDayGroup =
            addClass(
                QStringLiteral("2 Same teacher, room, and day group"),
                matchingTeacherId,
                QStringLiteral("E5"),
                QStringLiteral("Zeus"),
                QStringLiteral("Friday")
                );
        const int sameCourse =
            addClass(
                QStringLiteral("3 Course"),
                otherTeacherId,
                QStringLiteral("E5"),
                QStringLiteral("Zeus"),
                QStringLiteral("Wednesday")
                );
        addClass(
            QStringLiteral("4 Grade"),
            matchingTeacherId,
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("Monday")
            );
        addClass(
            QStringLiteral("5 Wrong day group"),
            matchingTeacherId,
            QStringLiteral("E5"),
            QStringLiteral("Zeus"),
            QStringLiteral("Tuesday")
            );
        addClass(
            QStringLiteral("6 Wrong grade"),
            matchingTeacherId,
            QStringLiteral("E6"),
            QStringLiteral("Hera"),
            QStringLiteral("Monday")
            );
        addClass(
            QStringLiteral("7 Wrong grade and day group"),
            otherTeacherId,
            QStringLiteral("M1"),
            QStringLiteral("Solis"),
            QStringLiteral("Thursday")
            );
        const int wrongRoom =
            addClass(
                QStringLiteral("8 Wrong room"),
                wrongRoomTeacherId,
                QStringLiteral("E5"),
                QStringLiteral("Zeus"),
                QStringLiteral("Monday")
                );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("홍길동");
        candidate.teacherKr = QStringLiteral("홍길동");
        candidate.rooms = {QStringLiteral("413")};
        candidate.classGrade = QStringLiteral("E5");
        candidate.classLevel = QStringLiteral("Zeus");
        candidate.times = {
            {
                QStringLiteral("Monday"),
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:55 PM")
            }
        };
        ScheduleImportUserBlock user;
        user.name = QStringLiteral("Alice");
        user.classes = {candidate};

        ScheduleImportRepository repository(database);
        const auto preview =
            repository.preview(
                user,
                ScheduleImportKind::Normal
                );
        QVERIFY(preview.has_value());
        QCOMPARE(preview->teachers.size(), 1);
        QCOMPARE(
            preview->teachers.first().matchingTeacherIds,
            QList<int>({
                matchingTeacherId,
                wrongRoomTeacherId
            })
            );
        QCOMPARE(
            preview->teachers.first().affectedClassCount,
            6
            );
        QCOMPARE(preview->classes.size(), 1);
        QCOMPARE(
            preview->classes.first().matchingClassIds,
            QList<int>({
                exact,
                sameTeacherRoomDayGroup,
                wrongRoom,
                sameCourse
            })
            );
        QCOMPARE(
            preview->classes.first().suggestedClassId,
            exact
            );
        QVERIFY(preview->classes.first().exactMatch);
        QCOMPARE(
            preview->classes.first().matchConfidence,
            ScheduleImportClassMatchConfidence::Confident
            );
        QCOMPARE(preview->inventory.classCount, 8);
        QVERIFY(preview->inventory.hasRegularHours);
        QVERIFY(!preview->inventory.hasIntensiveHours);

        const auto intensivePreview =
            repository.preview(
                user,
                ScheduleImportKind::Intensive
                );
        QVERIFY(intensivePreview.has_value());
        QCOMPARE(
            intensivePreview->classes.first().matchingClassIds,
            preview->classes.first().matchingClassIds
            );
        QCOMPARE(
            intensivePreview->classes.first().suggestedClassId,
            exact
            );
        QVERIFY(
            !intensivePreview->classes.first().exactMatch
            );
        QCOMPARE(
            intensivePreview->classes.first().matchConfidence,
            ScheduleImportClassMatchConfidence::Possible
            );

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::reportsScheduleInventoryStates_data()
{
    QTest::addColumn<bool>("classExists");
    QTest::addColumn<bool>("hasRegularHours");
    QTest::addColumn<bool>("hasIntensiveHours");

    QTest::newRow("no classes")
        << false << false << false;
    QTest::newRow("classes without hours")
        << true << false << false;
    QTest::newRow("regular hours only")
        << true << true << false;
    QTest::newRow("intensive hours only")
        << true << false << true;
    QTest::newRow("regular and intensive hours")
        << true << true << true;
}

void ScheduleImportTests::reportsScheduleInventoryStates()
{
    QFETCH(bool, classExists);
    QFETCH(bool, hasRegularHours);
    QFETCH(bool, hasIntensiveHours);

    const QString connectionName =
        QStringLiteral("schedule-import-inventory-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        if (classExists)
        {
            execOrFail(
                query,
                QStringLiteral(
                    "INSERT INTO classes (name) VALUES ('Inventory')"
                    )
                );
            const int classId =
                query.lastInsertId().toInt();
            execOrFail(
                query,
                QStringLiteral(
                    "INSERT INTO class_info "
                    "(class_id, class_grade, class_level) "
                    "VALUES (%1, 'E5', 'Zeus')"
                    )
                    .arg(classId)
                );
            if (hasRegularHours)
            {
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO class_times "
                        "(class_id, day, start_time, end_time) "
                        "VALUES (%1, 'Monday', '4:00 PM', '4:55 PM')"
                        )
                        .arg(classId)
                    );
            }
            if (hasIntensiveHours)
            {
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO class_intensive_times "
                        "(class_id, day, start_time, end_time) "
                        "VALUES (%1, 'Wednesday', '9:00 AM', '9:50 AM')"
                        )
                        .arg(classId)
                    );
            }
        }

        ScheduleImportUserBlock user;
        ScheduleImportRepository repository(database);
        for (ScheduleImportKind kind :
             {ScheduleImportKind::Normal,
              ScheduleImportKind::Intensive})
        {
            const auto preview =
                repository.preview(user, kind);
            QVERIFY(preview.has_value());
            QCOMPARE(
                preview->inventory.classCount,
                classExists ? 1 : 0
                );
            QCOMPARE(
                preview->inventory.hasRegularHours,
                hasRegularHours
                );
            QCOMPARE(
                preview->inventory.hasIntensiveHours,
                hasIntensiveHours
                );
        }

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::regularImportMatchesIntensiveOnlyClasses()
{
    const QString connectionName =
        QStringLiteral("schedule-import-cross-kind-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES ('홍길동', '413')"
                )
            );
        const int teacherId =
            query.lastInsertId().toInt();

        const auto addClass =
            [&query, teacherId](
                const QString& level,
                const QString& intensiveDay
                )
            {
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO classes (name) VALUES ('%1')"
                        )
                        .arg(level)
                    );
                const int classId =
                    query.lastInsertId().toInt();
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO class_info "
                        "(class_id, teacher_id, class_grade, class_level) "
                        "VALUES (%1, %2, 'E5', '%3')"
                        )
                        .arg(classId)
                        .arg(teacherId)
                        .arg(level)
                    );
                if (!intensiveDay.isEmpty())
                {
                    execOrFail(
                        query,
                        QStringLiteral(
                            "INSERT INTO class_intensive_times "
                            "(class_id, day, start_time, end_time) "
                            "VALUES (%1, '%2', '9:00 AM', '9:50 AM')"
                            )
                            .arg(classId)
                            .arg(intensiveDay)
                        );
                }
                return classId;
            };

        const int sameFamily =
            addClass(
                QStringLiteral("Zeus"),
                QStringLiteral("Monday")
                );
        addClass(
            QStringLiteral("Zeus"),
            QStringLiteral("Tuesday")
            );
        const int noHours =
            addClass(
                QStringLiteral("Zeus"),
                QString()
                );
        addClass(
            QStringLiteral("Athena"),
            QString()
            );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("홍길동");
        candidate.teacherKr = QStringLiteral("홍길동");
        candidate.rooms = {QStringLiteral("413")};
        candidate.classGrade = QStringLiteral("E5");
        candidate.classLevel = QStringLiteral("Zeus");
        candidate.times = {
            {
                QStringLiteral("Monday"),
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:55 PM")
            }
        };
        ScheduleImportUserBlock user;
        user.name = QStringLiteral("Alice");
        user.classes = {candidate};

        ScheduleImportRepository repository(database);
        const auto preview =
            repository.preview(
                user,
                ScheduleImportKind::Normal
                );
        QVERIFY(preview.has_value());
        QCOMPARE(preview->inventory.classCount, 4);
        QVERIFY(!preview->inventory.hasRegularHours);
        QVERIFY(preview->inventory.hasIntensiveHours);
        QCOMPARE(
            preview->classes.first().matchingClassIds,
            QList<int>({sameFamily, noHours})
            );
        QCOMPARE(
            preview->classes.first().suggestedClassId,
            sameFamily
            );
        QVERIFY(!preview->classes.first().exactMatch);
        QCOMPARE(
            preview->classes.first().matchConfidence,
            ScheduleImportClassMatchConfidence::Possible
            );

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::intensiveModesPreserveOrReplaceAbsentHours()
{
    const QString connectionName =
        QStringLiteral("schedule-import-intensive-mode-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES ('홍길동', '413')"
                )
            );
        const int teacherId =
            query.lastInsertId().toInt();

        const auto addClass =
            [&query, teacherId](
                const QString& level,
                const QString& intensiveDay
                )
            {
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO classes (name) VALUES ('%1')"
                        )
                        .arg(level)
                    );
                const int classId =
                    query.lastInsertId().toInt();
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO class_info "
                        "(class_id, teacher_id, class_grade, class_level) "
                        "VALUES (%1, %2, 'E5', '%3')"
                        )
                        .arg(classId)
                        .arg(teacherId)
                        .arg(level)
                    );
                execOrFail(
                    query,
                    QStringLiteral(
                        "INSERT INTO class_intensive_times "
                        "(class_id, day, start_time, end_time) "
                        "VALUES (%1, '%2', '9:00 AM', '9:50 AM')"
                        )
                        .arg(classId)
                        .arg(intensiveDay)
                    );
                return classId;
            };

        const int importedClass =
            addClass(
                QStringLiteral("Zeus"),
                QStringLiteral("Monday")
                );
        const int absentClass =
            addClass(
                QStringLiteral("Apollo"),
                QStringLiteral("Tuesday")
                );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Friday', '4:00 PM', '4:55 PM')"
                )
                .arg(importedClass)
            );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("홍길동");
        candidate.teacherKr = QStringLiteral("홍길동");
        candidate.rooms = {QStringLiteral("413")};
        candidate.classGrade = QStringLiteral("E5");
        candidate.classLevel = QStringLiteral("Zeus");
        candidate.times = {
            {
                QStringLiteral("Monday"),
                QStringLiteral("10:00 AM"),
                QStringLiteral("10:50 AM")
            },
            {
                QStringLiteral("Wednesday"),
                QStringLiteral("10:00 AM"),
                QStringLiteral("10:50 AM")
            }
        };

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Intensive;
        plan.intensiveMode =
            ScheduleImportIntensiveMode::UpdateExisting;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {candidate};
        plan.teachers = {
            {
                QStringLiteral("홍길동"),
                ScheduleImportTeacherAction::Reuse,
                teacherId,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::UpdateExisting,
                importedClass,
                QStringLiteral("#123456"),
                QStringLiteral("#FFFFFF")
            }
        };

        ScheduleImportRepository repository(database);
        const auto updated =
            repository.apply(plan);
        const QString updateError =
            updated.has_value() ? QString() : updated.error();
        QVERIFY2(updated.has_value(), qPrintable(updateError));
        QCOMPARE(updated->schedulesCleared, 0);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT COUNT(*) FROM class_intensive_times "
                "WHERE class_id=%1"
                )
                .arg(absentClass)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);

        query.finish();
        plan.intensiveMode =
            ScheduleImportIntensiveMode::ReplaceWithNew;
        const auto replaced =
            repository.apply(plan);
        const QString replaceError =
            replaced.has_value() ? QString() : replaced.error();
        QVERIFY2(replaced.has_value(), qPrintable(replaceError));
        QCOMPARE(replaced->schedulesCleared, 1);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT COUNT(*) FROM class_intensive_times "
                "WHERE class_id=%1"
                )
                .arg(absentClass)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        execOrFail(
            query,
            QStringLiteral(
                "SELECT COUNT(*) FROM class_times "
                "WHERE class_id=%1"
                )
                .arg(importedClass)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::fullSnapshotPreservesUnrelatedData()
{
    const QString connectionName =
        QStringLiteral("schedule-import-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers "
                "(teacher_kr, teacher_en, room_number, notes) "
                "VALUES ('홍길동', 'Daniel', '413', 'Keep Teacher Notes')"
                )
            );
        const int teacherId =
            query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral("INSERT INTO classes (name) VALUES ('Custom One')")
            );
        const int classOne =
            query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, reading_book, notes) "
                "VALUES (%1, %2, 'E5', 'Zeus', 'Keep Book', 'Keep Notes')"
                )
                .arg(classOne)
                .arg(teacherId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES "
                "(%1, 'Tuesday', '4:00 PM', '4:55 PM'), "
                "(%1, 'Thursday', '4:00 PM', '4:55 PM')"
                )
                .arg(classOne)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_intensive_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Wednesday', '1:00 PM', '1:55 PM')"
                )
                .arg(classOne)
            );

        execOrFail(
            query,
            QStringLiteral("INSERT INTO classes (name) VALUES ('Absent')")
            );
        const int classTwo =
            query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level) "
                "VALUES (%1, %2, 'E6', 'Hera')"
                )
                .arg(classTwo)
                .arg(teacherId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Friday', '5:00 PM', '5:55 PM')"
                )
                .arg(classTwo)
            );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("홍길동");
        candidate.teacherKr = QStringLiteral("홍길동");
        candidate.rooms = {QStringLiteral("414")};
        candidate.classGrade = QStringLiteral("E5");
        candidate.classLevel = QStringLiteral("Zeus");
        candidate.times = {
            {
                QStringLiteral("Tuesday"),
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:55 PM")
            },
            {
                QStringLiteral("Thursday"),
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:55 PM")
            }
        };
        ScheduleImportUserBlock user;
        user.name = QStringLiteral("Alice");
        user.classes = {candidate};

        ScheduleImportRepository repository(database);
        const auto preview =
            repository.preview(
                user,
                ScheduleImportKind::Normal
                );
        QVERIFY(preview.has_value());
        QCOMPARE(
            preview->classes.first().matchingClassIds,
            QList<int>{classOne}
            );
        QCOMPARE(preview->classes.first().suggestedClassId, classOne);
        QVERIFY(!preview->classes.first().exactMatch);

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Alice");
        plan.saveProfileNameIfBlank = true;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {candidate};
        plan.teachers = {
            {
                QStringLiteral("홍길동"),
                ScheduleImportTeacherAction::UpdateRoom,
                teacherId,
                QStringLiteral("414")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::UpdateExisting,
                classOne,
                QStringLiteral("#123456"),
                QStringLiteral("#FFFFFF")
            }
        };

        const auto imported =
            repository.apply(plan);
        const QString importError =
            imported.has_value() ? QString() : imported.error();
        QVERIFY2(
            imported.has_value(),
            qPrintable(importError)
            );
        QCOMPARE(imported->classesUpdated, 1);
        QCOMPARE(imported->teachersUpdated, 1);
        QCOMPARE(imported->schedulesCleared, 1);
        QVERIFY(imported->profileNameUpdated);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT day FROM class_times WHERE class_id=%1 "
                "ORDER BY id"
                )
                .arg(classOne)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Tuesday"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Thursday"));
        QVERIFY(!query.next());

        execOrFail(
            query,
            QStringLiteral(
                "SELECT COUNT(*) FROM class_times WHERE class_id=%1"
                )
                .arg(classTwo)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT COUNT(*) FROM class_intensive_times WHERE class_id=%1"
                )
                .arg(classOne)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT name FROM classes WHERE id=%1"
                )
                .arg(classOne)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Custom One"));

        execOrFail(
            query,
            QStringLiteral(
                "SELECT reading_book, notes, class_color, font_color "
                "FROM class_info WHERE class_id=%1"
                )
                .arg(classOne)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Keep Book"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("Keep Notes"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("#123456"));
        QCOMPARE(query.value(3).toString(), QStringLiteral("#FFFFFF"));

        execOrFail(
            query,
            QStringLiteral(
                "SELECT teacher_en, room_number, notes "
                "FROM teachers WHERE id=%1"
                )
                .arg(teacherId)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Daniel"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("414"));
        QCOMPARE(
            query.value(2).toString(),
            QStringLiteral("Keep Teacher Notes")
            );

        execOrFail(
            query,
            QStringLiteral(
                "SELECT value FROM app_settings WHERE key='myInfo/name'"
                )
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Alice"));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::skippedExactMatchPreservesItsSchedule()
{
    const QString connectionName =
        QStringLiteral("schedule-import-skip-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES ('홍길동', '413')"
                )
            );
        const int teacherId =
            query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO classes (name) VALUES ('Existing Name')"
                )
            );
        const int classId =
            query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level) "
                "VALUES (%1, %2, 'E5', 'Zeus')"
                )
                .arg(classId)
                .arg(teacherId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Monday', '4:00 PM', '4:55 PM')"
                )
                .arg(classId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('myInfo/name', 'Existing User')"
                )
            );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("홍길동");
        candidate.teacherKr = QStringLiteral("홍길동");
        candidate.rooms = {QStringLiteral("413")};
        candidate.classGrade = QStringLiteral("E5");
        candidate.classLevel = QStringLiteral("Zeus");
        candidate.times = {
            {
                QStringLiteral("Tuesday"),
                QStringLiteral("5:00 PM"),
                QStringLiteral("5:55 PM")
            }
        };
        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Alice");
        plan.saveProfileNameIfBlank = true;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {candidate};
        plan.teachers = {
            {
                QStringLiteral("홍길동"),
                ScheduleImportTeacherAction::Reuse,
                teacherId,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::Skip,
                classId
            }
        };

        ScheduleImportRepository repository(database);
        const auto imported =
            repository.apply(plan);
        const QString error =
            imported.has_value() ? QString() : imported.error();
        QVERIFY2(imported.has_value(), qPrintable(error));
        QCOMPARE(imported->classesSkipped, 1);
        QCOMPARE(imported->schedulesCleared, 0);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT day, start_time, end_time "
                "FROM class_times WHERE class_id=%1"
                )
                .arg(classId)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Monday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("4:00 PM"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("4:55 PM"));
        QVERIFY(!query.next());
        execOrFail(
            query,
            QStringLiteral(
                "SELECT value FROM app_settings "
                "WHERE key='myInfo/name'"
                )
            );
        QVERIFY(query.next());
        QCOMPARE(
            query.value(0).toString(),
            QStringLiteral("Existing User")
            );

        query.finish();
        plan.updateProfileName = true;
        const auto updatedProfileImport =
            repository.apply(plan);
        const QString updateError =
            updatedProfileImport.has_value()
                ? QString()
                : updatedProfileImport.error();
        QVERIFY2(
            updatedProfileImport.has_value(),
            qPrintable(updateError)
            );
        QVERIFY(updatedProfileImport->profileNameUpdated);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT value FROM app_settings "
                "WHERE key='myInfo/name'"
                )
            );
        QVERIFY(query.next());
        QCOMPARE(
            query.value(0).toString(),
            QStringLiteral("Alice")
            );
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::rejectsDuplicateExistingTargetsBeforeWrites()
{
    const QString connectionName =
        QStringLiteral("schedule-import-duplicate-target-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        ScheduleImportPlan plan;
        plan.candidates = {
            ScheduleImportClassCandidate{},
            ScheduleImportClassCandidate{}
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::UpdateExisting,
                42
            },
            {
                1,
                ScheduleImportClassAction::UpdateExisting,
                42
            }
        };

        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);
        QVERIFY(!imported.has_value());
        QVERIFY(
            imported.error().contains(
                QStringLiteral("unique existing target")
                )
            );
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::conflictsRollBackBeforeWrites()
{
    const QString connectionName =
        QStringLiteral("schedule-import-conflict-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        const auto candidate =
            [](const QString& teacher, const QString& level)
            {
                ScheduleImportClassCandidate result;
                result.teacherKey = teacher;
                result.teacherKr = teacher;
                result.rooms = {QStringLiteral("413")};
                result.classGrade = QStringLiteral("E5");
                result.classLevel = level;
                result.times = {
                    {
                        QStringLiteral("Monday"),
                        QStringLiteral("4:00 PM"),
                        QStringLiteral("4:55 PM")
                    },
                    {
                        QStringLiteral("Wednesday"),
                        QStringLiteral("4:00 PM"),
                        QStringLiteral("4:55 PM")
                    }
                };
                return result;
            };

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {
            candidate(QStringLiteral("김하늘"), QStringLiteral("Zeus")),
            candidate(QStringLiteral("박바다"), QStringLiteral("Apollo"))
        };
        plan.teachers = {
            {
                QStringLiteral("김하늘"),
                ScheduleImportTeacherAction::Create,
                -1,
                QStringLiteral("413")
            },
            {
                QStringLiteral("박바다"),
                ScheduleImportTeacherAction::Create,
                -1,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {0, ScheduleImportClassAction::CreateNew, -1},
            {1, ScheduleImportClassAction::CreateNew, -1}
        };

        ScheduleImportRepository repository(database);
        const auto imported =
            repository.apply(plan);
        QVERIFY(!imported.has_value());
        QVERIFY(imported.error().contains(QStringLiteral("overlaps")));

        QSqlQuery query(database);
        execOrFail(query, QStringLiteral("SELECT COUNT(*) FROM teachers"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        execOrFail(query, QStringLiteral("SELECT COUNT(*) FROM classes"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::writeFailureRollsBackEveryChange()
{
    const QString connectionName =
        QStringLiteral("schedule-import-rollback-%1")
            .arg(QUuid::createUuid().toString());
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDir.filePath(QStringLiteral("profile.db"))
            );
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        execOrFail(
            query,
            QStringLiteral(
                "CREATE TRIGGER reject_imported_time "
                "BEFORE INSERT ON class_times "
                "BEGIN "
                "SELECT RAISE(ABORT, 'forced schedule failure'); "
                "END"
                )
            );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("김하늘");
        candidate.teacherKr = QStringLiteral("김하늘");
        candidate.rooms = {QStringLiteral("413")};
        candidate.classGrade = QStringLiteral("E5");
        candidate.classLevel = QStringLiteral("Zeus");
        candidate.times = {
            {
                QStringLiteral("Monday"),
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:55 PM")
            },
            {
                QStringLiteral("Wednesday"),
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:55 PM")
            }
        };

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Alice");
        plan.saveProfileNameIfBlank = true;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {candidate};
        plan.teachers = {
            {
                QStringLiteral("김하늘"),
                ScheduleImportTeacherAction::Create,
                -1,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {0, ScheduleImportClassAction::CreateNew, -1}
        };

        ScheduleImportRepository repository(database);
        const auto imported =
            repository.apply(plan);
        QVERIFY(!imported.has_value());
        QVERIFY(
            imported.error().contains(
                QStringLiteral("forced schedule failure")
                )
            );

        execOrFail(query, QStringLiteral("SELECT COUNT(*) FROM teachers"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        execOrFail(query, QStringLiteral("SELECT COUNT(*) FROM classes"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        execOrFail(
            query,
            QStringLiteral(
                "SELECT COUNT(*) FROM app_settings "
                "WHERE key='myInfo/name'"
                )
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::validatesExternalWorkbookWhenProvided()
{
    const QString path =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_IMPORT_SAMPLE"
            );
    if (path.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_SCHEDULE_IMPORT_SAMPLE to validate an external workbook."
            );
    }

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto parsed =
        parseScheduleImportWorkbook(
            file.readAll(),
            ScheduleImportKind::Normal
            );
    const QString parseError =
        parsed.has_value() ? QString() : parsed.error();
    QVERIFY2(parsed.has_value(), qPrintable(parseError));
    QVERIFY(parsed->sheets.size() >= 2);

    int visibleUsers = 0;
    int coloredClasses = 0;
    QStringList invalidSchedules;
    bool checkedFirstUser = false;
    for (const ScheduleImportSheet& sheet : parsed->sheets)
    {
        if (sheet.visible)
        {
            visibleUsers += sheet.users.size();
            if (!checkedFirstUser && !sheet.users.isEmpty())
            {
                checkedFirstUser = true;
                for (const ScheduleImportClassCandidate& candidate :
                     sheet.users.first().classes)
                {
                    if (!candidate.meetingPatternError.isEmpty())
                    {
                        invalidSchedules.append(
                            QStringLiteral("%1 / %2 / %3 %4: %5")
                                .arg(
                                    sheet.name,
                                    sheet.users.first().name,
                                    candidate.classGrade,
                                    candidate.classLevel,
                                    candidate.meetingPatternError
                                    )
                            );
                    }
                }
            }
            for (const ScheduleImportUserBlock& user : sheet.users)
            {
                for (const ScheduleImportClassCandidate& candidate :
                     user.classes)
                {
                    if (!candidate.importedColors.isEmpty())
                    {
                        ++coloredClasses;
                    }
                    for (const ClassTime& time : candidate.times)
                    {
                        const QTime start =
                            QTime::fromString(
                                time.startTime,
                                QStringLiteral("h:mm AP")
                                );
                        const QTime end =
                            QTime::fromString(
                                time.endTime,
                                QStringLiteral("h:mm AP")
                                );
                        if (
                            !start.isValid()
                            || !end.isValid()
                            || end <= start
                            )
                        {
                            invalidSchedules.append(
                                QStringLiteral(
                                    "%1 / %2 / %3 %4: invalid time %5 %6-%7"
                                    )
                                    .arg(
                                        sheet.name,
                                        user.name,
                                        candidate.classGrade,
                                        candidate.classLevel,
                                        time.day,
                                        time.startTime,
                                        time.endTime
                                        )
                                );
                        }
                    }
                }
            }
        }
    }
    QVERIFY(visibleUsers > 0);
    QVERIFY(coloredClasses > 0);
    const QString invalidMessage =
        invalidSchedules.join(QLatin1Char('\n'));
    QVERIFY2(
        invalidSchedules.isEmpty(),
        qPrintable(invalidMessage)
        );
}

QTEST_GUILESS_MAIN(ScheduleImportTests)

#include "schedule_import_tests.moc"
