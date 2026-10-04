#include "data/database/database_schema_manager.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/schedule_import_repository.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/schedule/import/schedule_workbook_parser.h"
#include "next/application/schedule_import_apply_use_case.h"
#include "next/domain/schedule_entry.h"

#include <QCryptographicHash>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QTime>
#include <QUuid>
#include <QtTest>

#include <QVariant>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

class ScheduleImportTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesUsersMergesAndDiagnostics();
    void rejectsAmbiguousIntensiveTimesWithCellDiagnostic();
    void convertsIntensiveTimesAcrossNoon();
    void appliesIntensiveSlotStatesSnapshot();
    void validatesCourseMeetingPatterns();
    void partitionsRepeatedCourseIntoValidClasses();
    void filtersClassOptionsByGradeAndDayGroup();
    void ranksTeacherAndClassMatches();
    void previewsAndAppliesBaselineGeneratedWorkbookAgainstSeededDatabase();
    void previewsAndAppliesBaselineGeneratedIntensiveWorkbookAgainstSeededDatabase();
    void previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase();
    void previewsAndAppliesSyntheticIntensiveWorkbookAgainstSeededDatabase();
    void rejectsInvalidCourseAndPatternAtApplyBoundaryBeforeWrites();
    void previewsAndRejectsCheckedInOverlapWorkbookBeforeWrites();
    void reportsScheduleInventoryStates_data();
    void reportsScheduleInventoryStates();
    void regularImportMatchesIntensiveOnlyClasses();
    void intensiveModesPreserveOrReplaceAbsentHours();
    void fullSnapshotPreservesUnrelatedData();
    void skippedExactMatchPreservesItsSchedule();
    void rejectsDuplicateExistingTargetsBeforeWrites();
    void preservesNonpositiveExistingTeacherTargetsAtApply_data();
    void preservesNonpositiveExistingTeacherTargetsAtApply();
    void convertsNonpositiveCreateAndSkipTeacherTargetsAtApply_data();
    void convertsNonpositiveCreateAndSkipTeacherTargetsAtApply();
    void characterizesClassTargetsAtApply_data();
    void characterizesClassTargetsAtApply();
    void staleSelectedClassPreservesPersistedSnapshotBeforeWrites();
    void conflictsRollBackBeforeWrites();
    void writeFailureRollsBackEveryChange();
    void applyUsesBatchedClassInfoByIdAndPreservesClassOrder();
    void applyBatchReadFailureRollsBackBeforeWrites();
    void applyTeacherReadFailureRollsBackBeforeWrites();
    void typedApplyRejectsStaleSelectedClassBeforeWrites();
    void typedApplyPreservesExactTargetIdsThroughStateValidation();
    void typedApplyRejectsOverlappingSchedulesBeforeWrites();
    void typedWriteFailureRollsBackEveryChange();
    void typedIntensiveApplyMapsModeAndSlotState();
    void typedAndLegacyApplyShareNormalAndIntensiveResults();
    void typedApplyRejectsInvalidEnumsBeforeWrites();
    void typedApplyRejectsMalformedResolutionShapesBeforeWrites();
    void seededWriteFailureRollsBackEveryChange();
    void validatesExternalWorkbookWhenProvided();
};

namespace
{
ClassMngr::Next::Application::ScheduleImportApplyCandidate typedCandidate(
    const std::u16string& teacherKey,
    const std::u16string& grade,
    const std::u16string& level
    )
{
    using namespace ClassMngr::Next::Application;

    ScheduleImportApplyCandidate candidate;
    candidate.teacherKey = teacherKey;
    candidate.teacherName = teacherKey;
    candidate.rooms = {u"413"};
    candidate.grade = grade;
    candidate.level = level;
    candidate.times = {
        {u"Monday", u"4:00 PM", u"4:55 PM"},
        {u"Wednesday", u"4:00 PM", u"4:55 PM"}
    };
    return candidate;
}

ClassMngr::Next::Application::ScheduleImportApplyRequest typedCreateRequest(
    std::vector<ClassMngr::Next::Application::ScheduleImportApplyCandidate> candidates
    )
{
    using namespace ClassMngr::Next::Application;

    ScheduleImportApplyRequest request;
    request.diagnosticsAcknowledged = true;
    request.candidates = std::move(candidates);
    for (const ScheduleImportApplyCandidate& candidate : request.candidates)
    {
        request.teachers.push_back({
            candidate.teacherKey,
            ScheduleImportReviewTeacherAction::Create,
            std::nullopt,
            u"413"
        });
        request.classes.push_back({
            static_cast<int>(request.classes.size()),
            ScheduleImportReviewClassAction::CreateNew,
            std::nullopt,
            u"#123456",
            u"#FFFFFF"
        });
    }
    return request;
}

QString typedApplyFailureMessage(
    const ScheduleImportTypedApplyResult& result
    )
{
    return result.has_value()
        ? QString()
        : QString::fromStdU16String(result.error().message);
}

template <typename Id>
int legacyTestId(const std::optional<Id>& id)
{
    if (!id)
    {
        return -1;
    }
    bool ok = false;
    const int value = QString::fromStdString(id->value()).toInt(&ok);
    return ok ? value : -1;
}

ScheduleImportPlan legacyPlanFromTypedRequest(
    const ClassMngr::Next::Application::ScheduleImportApplyRequest& request
    )
{
    using namespace ClassMngr::Next::Application;

    ScheduleImportPlan plan;
    plan.kind = request.intensiveSchedule
        ? ScheduleImportKind::Intensive
        : ScheduleImportKind::Normal;
    plan.intensiveMode = request.intensiveMode
            == ScheduleImportPlanIntensiveMode::ReplaceWithNew
        ? ScheduleImportIntensiveMode::ReplaceWithNew
        : ScheduleImportIntensiveMode::UpdateExisting;
    plan.selectedUserName = QString::fromStdU16String(request.selectedUserName);
    plan.saveProfileNameIfBlank = request.saveProfileNameIfBlank;
    plan.updateProfileName = request.updateProfileName;
    plan.unknownCellsAcknowledged = request.diagnosticsAcknowledged;
    for (const ScheduleImportApplyCandidate& source : request.candidates)
    {
        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QString::fromStdU16String(source.teacherKey);
        candidate.teacherKr = QString::fromStdU16String(source.teacherName);
        candidate.classGrade = QString::fromStdU16String(source.grade);
        candidate.classLevel = QString::fromStdU16String(source.level);
        candidate.meetingPatternError = QString::fromStdU16String(
            source.meetingPatternError
            );
        for (const std::u16string& room : source.rooms)
        {
            candidate.rooms.append(QString::fromStdU16String(room));
        }
        for (const std::u16string& color : source.importedColors)
        {
            candidate.importedColors.append(QString::fromStdU16String(color));
        }
        for (const ScheduleImportApplyTime& time : source.times)
        {
            candidate.times.push_back({
                QString::fromStdU16String(time.day),
                QString::fromStdU16String(time.startTime),
                QString::fromStdU16String(time.endTime)
            });
        }
        for (const std::u16string& cell : source.sourceCells)
        {
            candidate.sourceCells.append(QString::fromStdU16String(cell));
        }
        plan.candidates.append(std::move(candidate));
    }
    for (const ScheduleImportApplySlotState& state : request.intensiveSlotStates)
    {
        plan.intensiveSlotStates.push_back({
            QString::fromStdU16String(state.day),
            QString::fromStdU16String(state.startTime),
            QString::fromStdU16String(state.state)
        });
    }
    for (const ScheduleImportApplyDiagnostic& diagnostic : request.diagnostics)
    {
        plan.diagnostics.push_back({
            QString::fromStdU16String(diagnostic.sheetName),
            QString::fromStdU16String(diagnostic.userName),
            QString::fromStdU16String(diagnostic.cellReference),
            QString::fromStdU16String(diagnostic.value),
            QString::fromStdU16String(diagnostic.message)
        });
    }
    for (const ScheduleImportApplyTeacher& source : request.teachers)
    {
        ScheduleImportTeacherAction action = ScheduleImportTeacherAction::Create;
        switch (source.action)
        {
        case ScheduleImportReviewTeacherAction::Reuse:
            action = ScheduleImportTeacherAction::Reuse;
            break;
        case ScheduleImportReviewTeacherAction::UpdateRoom:
            action = ScheduleImportTeacherAction::UpdateRoom;
            break;
        case ScheduleImportReviewTeacherAction::Create:
            action = ScheduleImportTeacherAction::Create;
            break;
        case ScheduleImportReviewTeacherAction::Skip:
            action = ScheduleImportTeacherAction::Skip;
            break;
        default:
            Q_UNREACHABLE();
        }
        plan.teachers.push_back({
            QString::fromStdU16String(source.teacherKey),
            action,
            legacyTestId(source.targetTeacherId),
            QString::fromStdU16String(source.selectedRoom)
        });
    }
    for (const ScheduleImportApplyClass& source : request.classes)
    {
        ScheduleImportClassAction action = ScheduleImportClassAction::CreateNew;
        switch (source.action)
        {
        case ScheduleImportReviewClassAction::UpdateExisting:
            action = ScheduleImportClassAction::UpdateExisting;
            break;
        case ScheduleImportReviewClassAction::CreateNew:
            action = ScheduleImportClassAction::CreateNew;
            break;
        case ScheduleImportReviewClassAction::Skip:
            action = ScheduleImportClassAction::Skip;
            break;
        default:
            Q_UNREACHABLE();
        }
        plan.classes.push_back({
            source.candidateIndex,
            action,
            legacyTestId(source.targetClassId),
            QString::fromStdU16String(source.classColor),
            QString::fromStdU16String(source.fontColor)
        });
    }
    return plan;
}

void compareScheduleImportSummaries(
    const ScheduleImportSummary& legacy,
    const ScheduleImportSummary& typed
    )
{
    QCOMPARE(typed.teachersCreated, legacy.teachersCreated);
    QCOMPARE(typed.teachersUpdated, legacy.teachersUpdated);
    QCOMPARE(typed.classesCreated, legacy.classesCreated);
    QCOMPARE(typed.classesUpdated, legacy.classesUpdated);
    QCOMPARE(typed.classesSkipped, legacy.classesSkipped);
    QCOMPARE(typed.schedulesCleared, legacy.schedulesCleared);
    QCOMPARE(typed.ignoredCells, legacy.ignoredCells);
    QCOMPARE(typed.profileNameUpdated, legacy.profileNameUpdated);
}

std::optional<ClassMngr::Next::Domain::ScheduleEntry> domainEntry(
    int classId,
    const QString& day,
    const QString& startTime,
    const QString& endTime
    )
{
    using namespace ClassMngr::Next::Domain;

    static const QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    const auto minutes = [](const QString& value)
    {
        for (const QString& format : {
                 QStringLiteral("h:mm AP"),
                 QStringLiteral("h:mmAP"),
                 QStringLiteral("H:mm"),
                 QStringLiteral("HH:mm")
             })
        {
            const QTime time = QTime::fromString(value.trimmed(), format);
            if (time.isValid())
            {
                return time.hour() * 60 + time.minute();
            }
        }
        return -1;
    };

    const auto typedClassId = ClassId::fromString(
        std::to_string(classId)
        );
    const auto time = ScheduleTime::fromMinutes(
        days.indexOf(day),
        minutes(startTime),
        minutes(endTime)
        );
    if (!typedClassId || !time)
    {
        return std::nullopt;
    }
    return ScheduleEntry(*typedClassId, *time);
}

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

ScheduleImportClassCandidate stateValidationCandidate(
    const QString& teacherKey,
    const QString& classLevel
    )
{
    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = teacherKey;
    candidate.teacherKr = teacherKey;
    candidate.rooms = {QStringLiteral("413")};
    candidate.classGrade = QStringLiteral("E5");
    candidate.classLevel = classLevel;
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
    return candidate;
}

QString sentinelTeacherName()
{
    return QString::fromUtf8(
        "\xEA\xB9\x80\xED\x95\x98\xEB\x8A\x98"
        );
}

QString foreignTeacherName()
{
    return QString::fromUtf8(
        "\xEB\xB0\x95\xEB\xB0\x94\xEB\x8B\xA4"
        );
}

QStringList persistedScheduleImportSnapshot(
    QSqlDatabase& database,
    bool includeSQLiteSequence = false
    )
{
    const QList<QString> statements{
        QStringLiteral("SELECT * FROM teachers ORDER BY id"),
        QStringLiteral("SELECT * FROM classes ORDER BY id"),
        QStringLiteral("SELECT * FROM class_info ORDER BY class_id"),
        QStringLiteral(
            "SELECT * FROM class_times "
            "ORDER BY class_id, day, start_time, end_time, id"
            ),
        QStringLiteral(
            "SELECT * FROM class_intensive_times "
            "ORDER BY class_id, day, start_time, end_time, id"
            ),
        QStringLiteral(
            "SELECT * FROM intensive_slot_states "
            "ORDER BY day, start_time, id"
            ),
        QStringLiteral("SELECT * FROM app_settings ORDER BY key")
    };

    QStringList rows;
    for (const QString& statement : statements)
    {
        QSqlQuery query(database);
        execOrFail(query, statement);
        const int statementRowStart = rows.size();
        while (query.next())
        {
            QStringList columns;
            const QSqlRecord record = query.record();
            for (int column = 0; column < record.count(); ++column)
            {
                const QVariant value = query.value(column);
                columns.append(
                    value.isNull()
                        ? QStringLiteral("<NULL>")
                        : value.toString()
                    );
            }
            rows.append(
                statement + QChar(0x1e) + columns.join(QChar(0x1f))
                );
        }
        if (rows.size() == statementRowStart)
        {
            rows.append(statement + QChar(0x1e) + QStringLiteral("<empty>"));
        }
    }

    if (includeSQLiteSequence)
    {
        const QString statement =
            QStringLiteral(
                "SELECT name, seq FROM sqlite_sequence ORDER BY name"
                );
        QSqlQuery query(database);
        execOrFail(query, statement);
        const int statementRowStart = rows.size();
        while (query.next())
        {
            QStringList columns;
            for (int column = 0; column < query.record().count(); ++column)
            {
                const QVariant value = query.value(column);
                columns.append(
                    value.isNull()
                        ? QStringLiteral("<NULL>")
                        : value.toString()
                    );
            }
            rows.append(
                statement + QChar(0x1e) + columns.join(QChar(0x1f))
                );
        }
        if (rows.size() == statementRowStart)
        {
            rows.append(statement + QChar(0x1e) + QStringLiteral("<empty>"));
        }
    }
    return rows;
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
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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
    QVERIFY(valid(
        QStringLiteral(" e4 "),
        QStringLiteral(" Theseus "),
        {QStringLiteral("Wednesday"), QStringLiteral("Monday")}
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
    QVERIFY(valid(
        QStringLiteral(" e5 "),
        QStringLiteral(" aThEnA "),
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
        QStringLiteral(" e6 "),
        QStringLiteral(" sOnG'S "),
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
    QVERIFY(!valid(
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {QStringLiteral("Monday"), QStringLiteral("Monday")}
        ));
    QVERIFY(!valid(
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {QStringLiteral("Monday"), QStringLiteral("Saturday")}
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
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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

void ScheduleImportTests::
previewsAndAppliesBaselineGeneratedWorkbookAgainstSeededDatabase()
{
    // The source-generated synthetic workbook helper is byte-identical at
    // baseline 48fc5c5c and current 31057c00. These literal outcomes were
    // captured on both revisions with the same generated bytes and database
    // seed; this does not claim parity for any production workbook.
    const QByteArray workbookBytes = scheduleWorkbookData();
    QCOMPARE(workbookBytes.size(), 5352);
    QCOMPARE(
        QCryptographicHash::hash(
            workbookBytes,
            QCryptographicHash::Sha256
            ).toHex(),
        QByteArrayLiteral(
            "24cf273fe36278747c811ad04bb5cbf4a32a2970f50fc6abed394b5809d48b49"
            )
        );

    const auto parsed = parseScheduleImportWorkbook(
        workbookBytes,
        ScheduleImportKind::Normal
        );
    const QString parseError =
        parsed.has_value() ? QString() : parsed.error();
    QVERIFY2(parsed.has_value(), qPrintable(parseError));
    QCOMPARE(parsed->sheets.size(), 2);
    QCOMPARE(parsed->sheets.first().name, QStringLiteral("Current"));
    QVERIFY(parsed->sheets.first().visible);
    QCOMPARE(parsed->sheets.first().users.size(), 2);
    QCOMPARE(parsed->sheets.last().name, QStringLiteral("Archive"));
    QVERIFY(!parsed->sheets.last().visible);

    const ScheduleImportUserBlock user =
        parsed->sheets.first().users.first();
    QCOMPARE(user.name, QStringLiteral("Alice"));
    QCOMPARE(user.headerCell, QStringLiteral("A1"));
    QCOMPARE(user.classes.size(), 1);
    QCOMPARE(user.diagnostics.size(), 1);
    QCOMPARE(user.diagnostics.first().cellReference, QStringLiteral("C2"));
    QCOMPARE(user.diagnostics.first().value, QStringLiteral("Meeting"));
    QCOMPARE(
        user.diagnostics.first().message,
        QStringLiteral(
            "This occupied timetable cell was not recognized as a class."
            )
        );

    const QString teacherName = QString::fromUtf8(
        "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99"
        );
    const ScheduleImportClassCandidate candidate = user.classes.first();
    QCOMPARE(candidate.teacherKey, teacherName);
    QCOMPARE(candidate.teacherKr, teacherName);
    QCOMPARE(candidate.rooms, QStringList{QStringLiteral("413")});
    QCOMPARE(candidate.importedColors, QStringList{QStringLiteral("#6D9EEB")});
    QCOMPARE(candidate.classGrade, QStringLiteral("E5"));
    QCOMPARE(candidate.classLevel, QStringLiteral("Zeus"));
    QCOMPARE(
        candidate.sourceCells,
        (QStringList{QStringLiteral("B2"), QStringLiteral("D2")})
        );
    QVERIFY(candidate.meetingPatternError.isEmpty());
    QCOMPARE(candidate.times.size(), 2);
    QCOMPARE(candidate.times[0].day, QStringLiteral("Monday"));
    QCOMPARE(candidate.times[0].startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(candidate.times[0].endTime, QStringLiteral("5:55 PM"));
    QCOMPARE(candidate.times[1].day, QStringLiteral("Wednesday"));
    QCOMPARE(candidate.times[1].startTime, QStringLiteral("4:15 PM"));
    QCOMPARE(candidate.times[1].endTime, QStringLiteral("5:00 PM"));

    const QString connectionName =
        QStringLiteral("schedule-import-baseline-generated-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        query.prepare(
            QStringLiteral(
                "INSERT INTO teachers (id, teacher_kr, room_number) "
                "VALUES (17, ?, '413')"
                )
            );
        query.addBindValue(candidate.teacherKr);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (id, teacher_kr, room_number) "
                "VALUES (23, 'Retained teacher', '999')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO classes (id, name) "
                "VALUES (40, 'Retained unrelated class')"
                )
            );
        query.prepare(
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, "
                "class_color, font_color) "
                "VALUES (40, 23, 'M1', 'Solis', '#123456', '#654321')"
                )
            );
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (40, 'Friday', '7:00 PM', '7:55 PM')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('preserved-setting', 'keep')"
                )
            );

        ScheduleImportRepository repository(database);
        const auto preview =
            repository.preview(user, ScheduleImportKind::Normal);
        const QString previewError =
            preview.has_value() ? QString() : preview.error();
        QVERIFY2(preview.has_value(), qPrintable(previewError));
        QCOMPARE(preview->inventory.classCount, 1);
        QVERIFY(preview->inventory.hasRegularHours);
        QVERIFY(!preview->inventory.hasIntensiveHours);
        QCOMPARE(preview->teachers.size(), 1);
        QCOMPARE(preview->teachers.first().teacherKey, teacherName);
        QCOMPARE(preview->teachers.first().teacherKr, teacherName);
        QCOMPARE(
            preview->teachers.first().importedRooms,
            QStringList{QStringLiteral("413")}
            );
        QCOMPARE(
            preview->teachers.first().matchingTeacherIds,
            QList<int>{17}
            );
        QCOMPARE(preview->teachers.first().affectedClassCount, 0);
        QCOMPARE(preview->classes.size(), 1);
        QCOMPARE(preview->classes.first().candidateIndex, 0);
        QVERIFY(preview->classes.first().matchingClassIds.isEmpty());
        QCOMPARE(preview->classes.first().suggestedClassId, -1);
        QVERIFY(!preview->classes.first().exactMatch);
        QCOMPARE(
            preview->classes.first().matchConfidence,
            ScheduleImportClassMatchConfidence::None
            );

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = user.name;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = user.classes;
        plan.diagnostics = user.diagnostics;
        plan.teachers = {
            {
                candidate.teacherKey,
                ScheduleImportTeacherAction::Reuse,
                17,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::CreateNew,
                -1,
                QStringLiteral("#6D9EEB"),
                QStringLiteral("#000000")
            }
        };

        const auto applied = repository.apply(plan);
        const QString applyError =
            applied.has_value() ? QString() : applied.error();
        QVERIFY2(applied.has_value(), qPrintable(applyError));
        QCOMPARE(applied->teachersCreated, 0);
        QCOMPARE(applied->teachersUpdated, 0);
        QCOMPARE(applied->classesCreated, 1);
        QCOMPARE(applied->classesUpdated, 0);
        QCOMPARE(applied->classesSkipped, 0);
        QCOMPARE(applied->schedulesCleared, 1);
        QCOMPARE(applied->ignoredCells, 1);
        QVERIFY(!applied->profileNameUpdated);

        // Normal import replaces the regular schedule snapshot. Class 40 and
        // its metadata/settings remain, while its seeded Friday time is cleared.
        // The snapshot helper sorts rows by each table's stable key. Normalize
        // each result to its table name and field values, preserving NULLs.
        QStringList normalizedState;
        for (const QString& snapshotRow :
             persistedScheduleImportSnapshot(database))
        {
            const QStringList parts = snapshotRow.split(QChar(0x1e));
            QVERIFY(parts.size() == 2);
            const QString statement = parts.first();
            const int tableStart = QStringLiteral("SELECT * FROM ").size();
            const int tableEnd = statement.indexOf(QLatin1Char(' '), tableStart);
            QVERIFY(tableEnd > tableStart);
            QString fields = parts.last();
            fields.replace(QChar(0x1f), QLatin1Char('|'));
            normalizedState.append(
                statement.mid(tableStart, tableEnd - tableStart)
                + QLatin1Char('|') + fields
                );
        }
        const QStringList expectedState{
            QStringLiteral("teachers|17|") + teacherName
                + QStringLiteral("|<NULL>|<NULL>|<NULL>|413|<NULL>|<NULL>|<NULL>|<NULL>|WiFi|<NULL>|<NULL>|HDMI|<NULL>"),
            QStringLiteral("teachers|23|Retained teacher|<NULL>|<NULL>|<NULL>|999|<NULL>|<NULL>|<NULL>|<NULL>|WiFi|<NULL>|<NULL>|HDMI|<NULL>"),
            QStringLiteral("classes|40|Retained unrelated class"),
            QStringLiteral("classes|41|E5 Zeus"),
            QStringLiteral("class_info|40|23|M1|Solis|<NULL>|<NULL>|#123456|#654321|<NULL>|<NULL>"),
            QStringLiteral("class_info|41|17|E5|Zeus|<NULL>|<NULL>|#6D9EEB|#000000|<NULL>|<NULL>"),
            QStringLiteral("class_times|2|41|Monday|4:00 PM|5:55 PM"),
            QStringLiteral("class_times|3|41|Wednesday|4:15 PM|5:00 PM"),
            QStringLiteral("class_intensive_times|<empty>"),
            QStringLiteral("intensive_slot_states|<empty>"),
            QStringLiteral("app_settings|preserved-setting|keep")
        };
        QCOMPARE(normalizedState, expectedState);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::
previewsAndAppliesBaselineGeneratedIntensiveWorkbookAgainstSeededDatabase()
{
    // Both the workbook wrapper and this inline intensive worksheet were
    // present at legacy baseline 48fc5c5c. The pinned workbook and
    // parsed/persisted slot transcripts matched on that source snapshot and
    // current code with this same database seed. This is synthetic evidence,
    // not historical production-workbook parity.
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
    const QByteArray workbookBytes = singleSheetWorkbookData(sheet);
    QCOMPARE(workbookBytes.size(), 2359);
    QCOMPARE(
        QCryptographicHash::hash(
            workbookBytes,
            QCryptographicHash::Sha256
            ).toHex(),
        QByteArrayLiteral(
            "228fc2ce924f2fd4ee340500c92178386868bc83231b076b74e081dd628b93b4"
            )
        );

    const auto parsed = parseScheduleImportWorkbook(
        workbookBytes,
        ScheduleImportKind::Intensive
        );
    const QString parseError =
        parsed.has_value() ? QString() : parsed.error();
    QVERIFY2(parsed.has_value(), qPrintable(parseError));
    QCOMPARE(parsed->sheets.size(), 1);
    QCOMPARE(parsed->sheets.first().name, QStringLiteral("Intensive"));
    QVERIFY(parsed->sheets.first().visible);
    QCOMPARE(parsed->sheets.first().users.size(), 1);

    const ScheduleImportUserBlock user = parsed->sheets.first().users.first();
    QCOMPARE(user.name, QStringLiteral("Alice"));
    QVERIFY(user.diagnostics.isEmpty());
    QCOMPARE(user.classes.size(), 1);
    const ScheduleImportClassCandidate& candidate = user.classes.first();
    const QString teacherName = QString::fromUtf8(
        "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99"
        );
    QCOMPARE(candidate.teacherKey, teacherName);
    QCOMPARE(candidate.teacherKr, teacherName);
    QCOMPARE(candidate.rooms, QStringList{QStringLiteral("413")});
    QCOMPARE(candidate.classGrade, QStringLiteral("E6"));
    QCOMPARE(candidate.classLevel, QStringLiteral("Song's"));
    QCOMPARE(
        candidate.sourceCells,
        (QStringList{QStringLiteral("C2"), QStringLiteral("E6")})
        );
    QVERIFY(candidate.meetingPatternError.isEmpty());
    QCOMPARE(candidate.times.size(), 2);
    QCOMPARE(candidate.times[0].day, QStringLiteral("Tuesday"));
    QCOMPARE(candidate.times[0].startTime, QStringLiteral("9:00 AM"));
    QCOMPARE(candidate.times[0].endTime, QStringLiteral("9:55 AM"));
    QCOMPARE(candidate.times[1].day, QStringLiteral("Thursday"));
    QCOMPARE(candidate.times[1].startTime, QStringLiteral("1:00 PM"));
    QCOMPARE(candidate.times[1].endTime, QStringLiteral("1:55 PM"));

    QCOMPARE(user.intensiveSlotStates.size(), 65);
    const auto slotState = [&user](const QString& day, const QString& startTime)
    {
        for (const IntensiveSlotState& state : user.intensiveSlotStates)
        {
            if (state.day == day && state.startTime == startTime)
            {
                return state.state;
            }
        }
        return QString();
    };
    QCOMPARE(
        slotState(QStringLiteral("Monday"), QStringLiteral("11:00")),
        QStringLiteral("lunch")
        );
    QCOMPARE(
        slotState(QStringLiteral("Monday"), QStringLiteral("10:00")),
        QStringLiteral("empty")
        );
    QCOMPARE(
        slotState(QStringLiteral("Friday"), QStringLiteral("20:00")),
        QStringLiteral("empty")
        );
    QStringList parsedSlotStates;
    for (const IntensiveSlotState& state : user.intensiveSlotStates)
    {
        parsedSlotStates.append(
            QStringLiteral("%1|%2|%3")
                .arg(state.day, state.startTime, state.state)
            );
    }
    parsedSlotStates.sort(Qt::CaseSensitive);
    QCOMPARE(
        std::count_if(
            user.intensiveSlotStates.cbegin(),
            user.intensiveSlotStates.cend(),
            [](const IntensiveSlotState& state)
            {
                return state.state == QStringLiteral("lunch");
            }
            ),
        1
        );
    QCOMPARE(
        std::count_if(
            user.intensiveSlotStates.cbegin(),
            user.intensiveSlotStates.cend(),
            [](const IntensiveSlotState& state)
            {
                return state.state == QStringLiteral("empty");
            }
            ),
        62
        );
    QCOMPARE(
        std::count_if(
            user.intensiveSlotStates.cbegin(),
            user.intensiveSlotStates.cend(),
            [](const IntensiveSlotState& state)
            {
                return state.state == QStringLiteral("essay");
            }
            ),
        2
        );
    QCOMPARE(
        QCryptographicHash::hash(
            parsedSlotStates.join(QLatin1Char('\n')).toUtf8(),
            QCryptographicHash::Sha256
            ).toHex(),
        QByteArrayLiteral(
            "7fdba13a48788441556050e17b6d0b5ca52b2b9df5a6d0c357815a6458f4a5cc"
            )
        );

    constexpr int teacherId = 7301;
    constexpr int targetClassId = 7401;
    constexpr int unrelatedClassId = 7402;
    const QString connectionName =
        QStringLiteral("schedule-import-baseline-intensive-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        query.prepare(
            QStringLiteral(
                "INSERT INTO teachers (id, teacher_kr, room_number) "
                "VALUES (?, ?, ?)"
                )
            );
        query.addBindValue(teacherId);
        query.addBindValue(teacherName);
        query.addBindValue(QStringLiteral("413"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO classes (id, name) VALUES "
                "(7401, 'E6 Song''s target'), (7402, 'Unrelated class')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, "
                "class_color, font_color) VALUES "
                "(7401, 7301, 'E6', 'Song''s', '#112233', '#FFFFFF'), "
                "(7402, 7301, 'E5', 'Zeus', '#445566', '#FFFFFF')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(id, class_id, day, start_time, end_time) VALUES "
                "(8101, 7401, 'Monday', '4:00 PM', '4:55 PM'), "
                "(8102, 7402, 'Friday', '5:00 PM', '5:55 PM')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_intensive_times "
                "(id, class_id, day, start_time, end_time) VALUES "
                "(8201, 7401, 'Tuesday', '9:00 AM', '9:50 AM'), "
                "(8202, 7401, 'Thursday', '1:00 PM', '1:50 PM'), "
                "(8203, 7402, 'Friday', '9:00 AM', '9:50 AM')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO intensive_slot_states "
                "(day, start_time, state) VALUES "
                "('Friday', '20:00', 'lunch')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('preserved-setting', 'keep')"
                )
            );

        ScheduleImportRepository repository(database);
        const auto preview = repository.preview(
            user,
            ScheduleImportKind::Intensive
            );
        const QString previewError =
            preview.has_value() ? QString() : preview.error();
        QVERIFY2(preview.has_value(), qPrintable(previewError));
        QCOMPARE(preview->kind, ScheduleImportKind::Intensive);
        QCOMPARE(preview->inventory.classCount, 2);
        QVERIFY(preview->inventory.hasRegularHours);
        QVERIFY(preview->inventory.hasIntensiveHours);
        QCOMPARE(preview->teachers.size(), 1);
        QCOMPARE(preview->teachers.first().teacherKey, teacherName);
        QCOMPARE(preview->teachers.first().teacherKr, teacherName);
        QCOMPARE(
            preview->teachers.first().matchingTeacherIds,
            QList<int>{teacherId}
            );
        QCOMPARE(preview->teachers.first().affectedClassCount, 2);
        QCOMPARE(preview->classes.size(), 1);
        QCOMPARE(preview->classes.first().candidateIndex, 0);
        QCOMPARE(
            preview->classes.first().matchingClassIds,
            QList<int>{targetClassId}
            );
        QCOMPARE(
            preview->classes.first().suggestedClassId,
            targetClassId
            );
        QVERIFY(preview->classes.first().exactMatch);
        QCOMPARE(
            preview->classes.first().matchConfidence,
            ScheduleImportClassMatchConfidence::Confident
            );

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Intensive;
        plan.intensiveMode = ScheduleImportIntensiveMode::UpdateExisting;
        plan.selectedUserName = user.name;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = user.classes;
        plan.intensiveSlotStates = user.intensiveSlotStates;
        plan.teachers = {
            {
                candidate.teacherKey,
                ScheduleImportTeacherAction::Reuse,
                teacherId,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::UpdateExisting,
                targetClassId,
                QStringLiteral("#112233"),
                QStringLiteral("#FFFFFF")
            }
        };

        const auto applied = repository.apply(plan);
        const QString applyError =
            applied.has_value() ? QString() : applied.error();
        QVERIFY2(applied.has_value(), qPrintable(applyError));
        QCOMPARE(applied->teachersCreated, 0);
        QCOMPARE(applied->teachersUpdated, 0);
        QCOMPARE(applied->classesCreated, 0);
        QCOMPARE(applied->classesUpdated, 1);
        QCOMPARE(applied->classesSkipped, 0);
        QCOMPARE(applied->schedulesCleared, 0);
        QCOMPARE(applied->ignoredCells, 0);
        QVERIFY(!applied->profileNameUpdated);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT class_id, day, start_time, end_time "
                "FROM class_intensive_times "
                "ORDER BY class_id, day, start_time, end_time"
                )
            );
        QStringList persistedIntensiveSchedules;
        while (query.next())
        {
            persistedIntensiveSchedules.append(
                QStringLiteral("%1|%2|%3|%4")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString(),
                        query.value(3).toString()
                        )
                );
        }
        const QStringList expectedIntensiveSchedules{
            QStringLiteral("7401|Thursday|1:00 PM|1:55 PM"),
            QStringLiteral("7401|Tuesday|9:00 AM|9:55 AM"),
            QStringLiteral("7402|Friday|9:00 AM|9:50 AM")
        };
        QCOMPARE(persistedIntensiveSchedules, expectedIntensiveSchedules);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT day, start_time, state "
                "FROM intensive_slot_states ORDER BY day, start_time"
                )
            );
        QStringList persistedSlotStates;
        while (query.next())
        {
            persistedSlotStates.append(
                QStringLiteral("%1|%2|%3")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString()
                        )
                );
        }
        QCOMPARE(persistedSlotStates.size(), 65);
        const QByteArray persistedSlotStatesHash =
            QCryptographicHash::hash(
                persistedSlotStates.join(QLatin1Char('\n')).toUtf8(),
                QCryptographicHash::Sha256
                ).toHex();
        QCOMPARE(
            persistedSlotStatesHash,
            QByteArrayLiteral(
                "7fdba13a48788441556050e17b6d0b5ca52b2b9df5a6d0c357815a6458f4a5cc"
                )
            );
        QVERIFY(
            persistedSlotStates.contains(
                QStringLiteral("Monday|11:00|lunch")
                )
            );
        QVERIFY(
            persistedSlotStates.contains(
                QStringLiteral("Monday|10:00|empty")
                )
            );
        QVERIFY(
            persistedSlotStates.contains(
                QStringLiteral("Friday|20:00|empty")
                )
            );

        execOrFail(
            query,
            QStringLiteral(
                "SELECT id, class_id, day, start_time, end_time "
                "FROM class_times ORDER BY id"
                )
            );
        QStringList persistedRegularSchedules;
        while (query.next())
        {
            persistedRegularSchedules.append(
                QStringLiteral("%1|%2|%3|%4|%5")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString(),
                        query.value(3).toString(),
                        query.value(4).toString()
                        )
                );
        }
        const QStringList expectedRegularSchedules{
            QStringLiteral("8101|7401|Monday|4:00 PM|4:55 PM"),
            QStringLiteral("8102|7402|Friday|5:00 PM|5:55 PM")
        };
        QCOMPARE(persistedRegularSchedules, expectedRegularSchedules);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT c.id, c.name, ci.teacher_id, ci.class_grade, "
                "ci.class_level, ci.class_color, ci.font_color "
                "FROM classes c JOIN class_info ci ON ci.class_id=c.id "
                "ORDER BY c.id"
                )
            );
        QStringList persistedClasses;
        while (query.next())
        {
            persistedClasses.append(
                QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString(),
                        query.value(3).toString(),
                        query.value(4).toString(),
                        query.value(5).toString(),
                        query.value(6).toString()
                        )
                );
        }
        const QStringList expectedClasses{
            QStringLiteral("7401|E6 Song's target|7301|E6|Song's|#112233|#FFFFFF"),
            QStringLiteral("7402|Unrelated class|7301|E5|Zeus|#445566|#FFFFFF")
        };
        QCOMPARE(persistedClasses, expectedClasses);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT id, teacher_kr, room_number FROM teachers ORDER BY id"
                )
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), teacherId);
        QCOMPARE(query.value(1).toString(), teacherName);
        QCOMPARE(query.value(2).toString(), QStringLiteral("413"));
        QVERIFY(!query.next());

        execOrFail(
            query,
            QStringLiteral("SELECT key, value FROM app_settings ORDER BY key")
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("preserved-setting"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("keep"));
        QVERIFY(!query.next());

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::
previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase()
{
    // Differential expectations were checked against legacy 48fc5c5c using the
    // same seeded database and schedule_review.xlsx. The fixture was added in
    // f5fdcc4a after that baseline; this is common-input differential evidence.
    QFile file(
        QStringLiteral(
            CLASSMNGR_SOURCE_DIR
            "/tests/fixtures/imports/schedule_review.xlsx"
            )
        );
    QVERIFY2(
        file.open(QIODevice::ReadOnly),
        qPrintable(file.errorString())
        );
    const auto workbook = parseScheduleImportWorkbook(
        file.readAll(),
        ScheduleImportKind::Normal
        );
    const QString parseError =
        workbook.has_value() ? QString() : workbook.error();
    QVERIFY2(workbook.has_value(), qPrintable(parseError));

    QCOMPARE(workbook->sheets.size(), 2);
    QCOMPARE(workbook->sheets[0].name, QStringLiteral("Current"));
    QVERIFY(workbook->sheets[0].visible);
    QCOMPARE(workbook->sheets[0].users.size(), 1);
    const ScheduleImportUserBlock user = workbook->sheets[0].users[0];
    QCOMPARE(user.name, QStringLiteral("Alice"));
    QCOMPARE(user.headerCell, QStringLiteral("A1"));
    QVERIFY(user.diagnostics.isEmpty());
    QCOMPARE(workbook->sheets[1].name, QStringLiteral("Alternate"));
    QVERIFY(workbook->sheets[1].visible);
    QCOMPARE(user.classes.size(), 3);

    const ScheduleImportClassCandidate& first = user.classes[0];
    QCOMPARE(first.teacherKey, QString::fromUtf8("\xEB\xB0\x95\xEC\x84\xA0\xEC\x83\x9D"));
    QCOMPARE(first.teacherKr, QString::fromUtf8("\xEB\xB0\x95\xEC\x84\xA0\xEC\x83\x9D"));
    QCOMPARE(first.rooms, QStringList{QStringLiteral("415")});
    QCOMPARE(first.classGrade, QStringLiteral("M3"));
    QCOMPARE(first.classLevel, QStringLiteral("Song's"));
    QVERIFY(first.importedColors.isEmpty());
    QCOMPARE(first.sourceCells, (QStringList{QStringLiteral("B2"), QStringLiteral("F2")}));
    QVERIFY(first.meetingPatternError.isEmpty());
    QCOMPARE(first.times.size(), 2);
    QCOMPARE(first.times[0].day, QStringLiteral("Monday"));
    QCOMPARE(first.times[0].startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(first.times[0].endTime, QStringLiteral("4:55 PM"));
    QCOMPARE(first.times[1].day, QStringLiteral("Friday"));
    QCOMPARE(first.times[1].startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(first.times[1].endTime, QStringLiteral("4:55 PM"));

    const ScheduleImportClassCandidate& target = user.classes[1];
    QCOMPARE(target.teacherKey, QString::fromUtf8("\xEC\xB5\x9C\xEC\x84\xA0\xEC\x83\x9D"));
    QCOMPARE(target.teacherKr, QString::fromUtf8("\xEC\xB5\x9C\xEC\x84\xA0\xEC\x83\x9D"));
    QCOMPARE(target.rooms, QStringList{QStringLiteral("416")});
    QCOMPARE(target.classGrade, QStringLiteral("E4"));
    QCOMPARE(target.classLevel, QStringLiteral("Hercules"));
    QCOMPARE(target.importedColors, QStringList{QStringLiteral("#6D9EEB")});
    QCOMPARE(target.sourceCells, (QStringList{QStringLiteral("C3"), QStringLiteral("E3")}));
    QVERIFY(target.meetingPatternError.isEmpty());
    QCOMPARE(target.times.size(), 2);
    QCOMPARE(target.times[0].day, QStringLiteral("Tuesday"));
    QCOMPARE(target.times[0].startTime, QStringLiteral("5:00 PM"));
    QCOMPARE(target.times[0].endTime, QStringLiteral("5:55 PM"));
    QCOMPARE(target.times[1].day, QStringLiteral("Thursday"));
    QCOMPARE(target.times[1].startTime, QStringLiteral("5:00 PM"));
    QCOMPARE(target.times[1].endTime, QStringLiteral("5:55 PM"));

    const ScheduleImportClassCandidate& third = user.classes[2];
    QCOMPARE(third.teacherKey, QString::fromUtf8("\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D"));
    QCOMPARE(third.teacherKr, QString::fromUtf8("\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D"));
    QCOMPARE(third.rooms, QStringList{QStringLiteral("413")});
    QCOMPARE(third.classGrade, QStringLiteral("E4"));
    QCOMPARE(third.classLevel, QStringLiteral("Theseus"));
    QVERIFY(third.importedColors.isEmpty());
    QCOMPARE(third.sourceCells, (QStringList{QStringLiteral("B4"), QStringLiteral("D4")}));
    QVERIFY(third.meetingPatternError.isEmpty());
    QCOMPARE(third.times.size(), 2);
    QCOMPARE(third.times[0].day, QStringLiteral("Monday"));
    QCOMPARE(third.times[0].startTime, QStringLiteral("6:00 PM"));
    QCOMPARE(third.times[0].endTime, QStringLiteral("6:55 PM"));
    QCOMPARE(third.times[1].day, QStringLiteral("Wednesday"));
    QCOMPARE(third.times[1].startTime, QStringLiteral("6:00 PM"));
    QCOMPARE(third.times[1].endTime, QStringLiteral("6:55 PM"));

    const QString connectionName =
        QStringLiteral("schedule-import-fixture-preview-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        query.prepare(
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES (?, ?)"
                )
            );
        query.addBindValue(QString::fromUtf8("\xEC\xB5\x9C\xEC\x84\xA0\xEC\x83\x9D"));
        query.addBindValue(QStringLiteral(" 416 "));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const int choiTeacherId = query.lastInsertId().toInt();

        query.prepare(
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES (?, ?)"
                )
            );
        query.addBindValue(QString::fromUtf8("\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D"));
        query.addBindValue(QStringLiteral("413"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const int kimTeacherId = query.lastInsertId().toInt();

        query.prepare(
            QStringLiteral("INSERT INTO classes (id, name) VALUES (?, ?)")
            );
        query.addBindValue(42);
        query.addBindValue(QStringLiteral("A weaker Hercules candidate"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        query.prepare(
            QStringLiteral("INSERT INTO classes (id, name) VALUES (?, ?)")
            );
        query.addBindValue(43);
        query.addBindValue(QStringLiteral("B exact Hercules candidate"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

        query.prepare(
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level) "
                "VALUES (?, ?, ?, ?)"
                )
            );
        query.addBindValue(42);
        query.addBindValue(kimTeacherId);
        query.addBindValue(QStringLiteral("E4"));
        query.addBindValue(QStringLiteral("Hercules"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        query.prepare(
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level) "
                "VALUES (?, ?, ?, ?)"
                )
            );
        query.addBindValue(43);
        query.addBindValue(choiTeacherId);
        query.addBindValue(QStringLiteral("E4"));
        query.addBindValue(QStringLiteral("Hercules"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

        query.prepare(
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)"
                )
            );
        query.addBindValue(42);
        query.addBindValue(QStringLiteral("Tuesday"));
        query.addBindValue(QStringLiteral("4:00 PM"));
        query.addBindValue(QStringLiteral("4:50 PM"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        query.prepare(
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)"
                )
            );
        query.addBindValue(43);
        query.addBindValue(QStringLiteral("Tuesday"));
        query.addBindValue(QStringLiteral("5:00 PM"));
        query.addBindValue(QStringLiteral("5:55 PM"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        query.prepare(
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)"
                )
            );
        query.addBindValue(43);
        query.addBindValue(QStringLiteral("Thursday"));
        query.addBindValue(QStringLiteral("5:00 PM"));
        query.addBindValue(QStringLiteral("5:55 PM"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

        query.prepare(
            QStringLiteral(
                "INSERT INTO classes (id, name) VALUES (?, ?)"
                )
            );
        query.addBindValue(77);
        query.addBindValue(QStringLiteral("Unrelated retained class"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        query.prepare(
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, "
                "class_color, font_color) VALUES (?, ?, ?, ?, ?, ?)"
                )
            );
        query.addBindValue(77);
        query.addBindValue(kimTeacherId);
        query.addBindValue(QStringLiteral("M2"));
        query.addBindValue(QStringLiteral("Atlas"));
        query.addBindValue(QStringLiteral("#1A2B3C"));
        query.addBindValue(QStringLiteral("#FFFFFF"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        query.prepare(
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)"
                )
            );
        query.addBindValue(77);
        query.addBindValue(QStringLiteral("Friday"));
        query.addBindValue(QStringLiteral("7:00 PM"));
        query.addBindValue(QStringLiteral("7:55 PM"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

        ScheduleImportRepository repository(database);
        const auto preview = repository.preview(user, ScheduleImportKind::Normal);
        const QString previewError =
            preview.has_value() ? QString() : preview.error();
        QVERIFY2(preview.has_value(), qPrintable(previewError));
        QCOMPARE(preview->kind, ScheduleImportKind::Normal);
        QCOMPARE(preview->user.classes.size(), user.classes.size());
        QCOMPARE(preview->teachers.size(), 3);
        const QList<QList<int>> expectedTeacherMatches{
            {}, {choiTeacherId}, {kimTeacherId}
        };
        const QList<int> expectedAffectedClassCounts{0, 1, 2};
        for (int index = 0; index < preview->teachers.size(); ++index)
        {
            const ScheduleImportTeacherPreview& teacher = preview->teachers[index];
            QCOMPARE(teacher.teacherKey, user.classes[index].teacherKey);
            QCOMPARE(teacher.teacherKr, user.classes[index].teacherKr);
            QCOMPARE(teacher.importedRooms, user.classes[index].rooms);
            QCOMPARE(teacher.matchingTeacherIds, expectedTeacherMatches[index]);
            QCOMPARE(teacher.affectedClassCount, expectedAffectedClassCounts[index]);
        }
        QCOMPARE(preview->classes.size(), 3);
        const ScheduleImportClassPreview& noMatch = preview->classes[0];
        QCOMPARE(noMatch.candidateIndex, 0);
        QVERIFY(noMatch.matchingClassIds.isEmpty());
        QCOMPARE(noMatch.suggestedClassId, -1);
        QVERIFY(!noMatch.exactMatch);
        QCOMPARE(
            noMatch.matchConfidence,
            ScheduleImportClassMatchConfidence::None
            );
        const ScheduleImportClassPreview& match = preview->classes[1];
        QCOMPARE(match.candidateIndex, 1);
        QCOMPARE(match.matchingClassIds, (QList<int>{43, 42}));
        QCOMPARE(match.suggestedClassId, 43);
        QVERIFY(match.exactMatch);
        QCOMPARE(
            match.matchConfidence,
            ScheduleImportClassMatchConfidence::Confident
            );
        const ScheduleImportClassPreview& thirdNoMatch = preview->classes[2];
        QCOMPARE(thirdNoMatch.candidateIndex, 2);
        QVERIFY(thirdNoMatch.matchingClassIds.isEmpty());
        QCOMPARE(thirdNoMatch.suggestedClassId, -1);
        QVERIFY(!thirdNoMatch.exactMatch);
        QCOMPARE(thirdNoMatch.matchConfidence, ScheduleImportClassMatchConfidence::None);
        QCOMPARE(preview->inventory.classCount, 3);
        QVERIFY(preview->inventory.hasRegularHours);
        QVERIFY(!preview->inventory.hasIntensiveHours);
        QCOMPARE(preview->initiallyAbsentClassIds, (QList<int>{42, 77}));

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = user.name;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = user.classes;
        const QString parkTeacherKey = user.classes[0].teacherKey;
        const QString choiTeacherKey = user.classes[1].teacherKey;
        const QString kimTeacherKey = user.classes[2].teacherKey;
        plan.teachers = {
            {
                parkTeacherKey,
                ScheduleImportTeacherAction::Create,
                -1,
                QStringLiteral("415")
            },
            {
                choiTeacherKey,
                ScheduleImportTeacherAction::Reuse,
                choiTeacherId,
                QStringLiteral("416")
            },
            {
                kimTeacherKey,
                ScheduleImportTeacherAction::Reuse,
                kimTeacherId,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::CreateNew,
                -1,
                QStringLiteral("#778899"),
                QStringLiteral("#000000")
            },
            {
                1,
                ScheduleImportClassAction::UpdateExisting,
                43,
                QStringLiteral("#223344"),
                QStringLiteral("#FFFFFF")
            },
            {
                2,
                ScheduleImportClassAction::CreateNew,
                -1,
                QStringLiteral("#556677"),
                QStringLiteral("#000000")
            }
        };

        const auto applied = repository.apply(plan);
        const QString applyError =
            applied.has_value() ? QString() : applied.error();
        QVERIFY2(applied.has_value(), qPrintable(applyError));
        QCOMPARE(applied->teachersCreated, 1);
        QCOMPARE(applied->teachersUpdated, 0);
        QCOMPARE(applied->classesCreated, 2);
        QCOMPARE(applied->classesUpdated, 1);
        QCOMPARE(applied->classesSkipped, 0);
        QCOMPARE(applied->schedulesCleared, 2);
        QCOMPARE(applied->ignoredCells, 0);
        QVERIFY(!applied->profileNameUpdated);

        for (int index = 0; index < plan.candidates.size(); ++index)
        {
            int expectedClassId = index == 1 ? 43 : -1;
            if (expectedClassId < 0)
            {
                query.prepare(
                    QStringLiteral(
                        "SELECT class_id FROM class_info "
                        "WHERE class_grade=? AND class_level=?"
                        )
                    );
                query.addBindValue(plan.candidates[index].classGrade);
                query.addBindValue(plan.candidates[index].classLevel);
                QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
                QVERIFY(query.next());
                expectedClassId = query.value(0).toInt();
                QVERIFY(expectedClassId > 0);
                QVERIFY(!query.next());
            }

            query.prepare(
                QStringLiteral(
                    "SELECT day, start_time, end_time FROM class_times "
                    "WHERE class_id=? ORDER BY id"
                    )
                );
            query.addBindValue(expectedClassId);
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
            const QList<ClassTime>& expectedTimes =
                plan.candidates[index].times;
            for (const ClassTime& expected : expectedTimes)
            {
                QVERIFY(query.next());
                QCOMPARE(query.value(0).toString(), expected.day);
                QCOMPARE(query.value(1).toString(), expected.startTime);
                QCOMPARE(query.value(2).toString(), expected.endTime);
            }
            QVERIFY(!query.next());
        }

        QStringList persistedTeachers;
        int parkTeacherId = -1;
        const QString parkName =
            QString::fromUtf8("\xEB\xB0\x95\xEC\x84\xA0\xEC\x83\x9D");
        execOrFail(
            query,
            QStringLiteral(
                "SELECT id, teacher_kr, room_number FROM teachers ORDER BY id"
                )
            );
        while (query.next())
        {
            const QString teacherName = query.value(1).toString();
            if (teacherName == parkName)
            {
                parkTeacherId = query.value(0).toInt();
            }
            persistedTeachers.append(
                teacherName
                + QLatin1Char('|')
                + query.value(2).toString()
                );
        }
        QVERIFY(parkTeacherId > 0);
        QCOMPARE(
            persistedTeachers,
            QStringList({
                QString::fromUtf8("\xEC\xB5\x9C\xEC\x84\xA0\xEC\x83\x9D")
                    + QStringLiteral("| 416 "),
                QString::fromUtf8("\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D")
                    + QStringLiteral("|413"),
                QString::fromUtf8("\xEB\xB0\x95\xEC\x84\xA0\xEC\x83\x9D")
                    + QStringLiteral("|415")
            })
            );

        QStringList persistedClasses;
        execOrFail(
            query,
            QStringLiteral(
                "SELECT c.name, t.teacher_kr, ci.class_grade, ci.class_level, "
                "ci.class_color, ci.font_color "
                "FROM classes c JOIN class_info ci ON ci.class_id=c.id "
                "JOIN teachers t ON t.id=ci.teacher_id "
                "ORDER BY c.name"
                )
            );
        while (query.next())
        {
            persistedClasses.append(
                query.value(0).toString()
                + QLatin1Char('|')
                + query.value(1).toString()
                + QLatin1Char('|')
                + query.value(2).toString()
                + QLatin1Char('|')
                + query.value(3).toString()
                + QLatin1Char('|')
                + query.value(4).toString()
                + QLatin1Char('|')
                + query.value(5).toString()
                );
        }
        QCOMPARE(
            persistedClasses,
            QStringList({
                QStringLiteral("A weaker Hercules candidate|%1|E4|Hercules|#FFFFFF|#000000")
                    .arg(third.teacherKr),
                QStringLiteral("B exact Hercules candidate|%1|E4|Hercules|#223344|#FFFFFF")
                    .arg(target.teacherKr),
                QStringLiteral("E4 Theseus|%1|E4|Theseus|#556677|#000000")
                    .arg(third.teacherKr),
                QStringLiteral("M3 Song's|%1|M3|Song's|#778899|#000000")
                    .arg(first.teacherKr),
                QStringLiteral("Unrelated retained class|%1|M2|Atlas|#1A2B3C|#FFFFFF")
                    .arg(third.teacherKr)
            })
            );

        QStringList persistedTimes;
        execOrFail(
            query,
            QStringLiteral(
                "SELECT c.id, c.name, ct.day, ct.start_time, ct.end_time "
                "FROM class_times ct JOIN classes c ON c.id=ct.class_id "
                "ORDER BY c.name, ct.day"
                )
        );
        std::vector<ClassMngr::Next::Domain::ScheduleEntry>
            persistedDomainEntries;
        while (query.next())
        {
            persistedTimes.append(
                query.value(1).toString()
                + QLatin1Char('|')
                + query.value(2).toString()
                + QLatin1Char('|')
                + query.value(3).toString()
                + QLatin1Char('|')
                + query.value(4).toString()
                );
            const auto entry = domainEntry(
                query.value(0).toInt(),
                query.value(2).toString(),
                query.value(3).toString(),
                query.value(4).toString()
                );
            QVERIFY(entry.has_value());
            persistedDomainEntries.push_back(*entry);
        }
        QCOMPARE(
            persistedTimes,
            QStringList({
                QStringLiteral("B exact Hercules candidate|Thursday|5:00 PM|5:55 PM"),
                QStringLiteral("B exact Hercules candidate|Tuesday|5:00 PM|5:55 PM"),
                QStringLiteral("E4 Theseus|Monday|6:00 PM|6:55 PM"),
                QStringLiteral("E4 Theseus|Wednesday|6:00 PM|6:55 PM"),
                QStringLiteral("M3 Song's|Friday|4:00 PM|4:55 PM"),
                QStringLiteral("M3 Song's|Monday|4:00 PM|4:55 PM")
            })
            );

        std::vector<int> resolvedClassIds(plan.candidates.size(), -1);
        QSqlQuery classIdQuery(database);
        for (const ScheduleImportClassResolution& resolution : plan.classes)
        {
            const int candidateIndex = resolution.candidateIndex;
            if (
                resolution.action
                    == ScheduleImportClassAction::UpdateExisting
                )
            {
                resolvedClassIds[static_cast<std::size_t>(candidateIndex)] =
                    resolution.targetClassId;
                continue;
            }

            const ScheduleImportClassCandidate& candidate =
                plan.candidates[candidateIndex];
            const QString className = QStringLiteral("%1 %2")
                .arg(candidate.classGrade, candidate.classLevel)
                .simplified();
            classIdQuery.prepare(
                QStringLiteral("SELECT id FROM classes WHERE name=?")
                );
            classIdQuery.addBindValue(className);
            QVERIFY2(
                classIdQuery.exec(),
                qPrintable(classIdQuery.lastError().text())
                );
            QVERIFY(classIdQuery.next());
            resolvedClassIds[static_cast<std::size_t>(candidateIndex)] =
                classIdQuery.value(0).toInt();
            QVERIFY(!classIdQuery.next());
        }

        std::vector<ClassMngr::Next::Domain::ScheduleEntry>
            expectedDomainEntries;
        for (int candidateIndex = 0;
             candidateIndex < plan.candidates.size();
             ++candidateIndex)
        {
            const int resolvedClassId =
                resolvedClassIds[static_cast<std::size_t>(candidateIndex)];
            QVERIFY(resolvedClassId > 0);
            for (const ClassTime& time :
                 plan.candidates[candidateIndex].times)
            {
                const auto entry = domainEntry(
                    resolvedClassId,
                    time.day,
                    time.startTime,
                    time.endTime
                    );
                QVERIFY(entry.has_value());
                expectedDomainEntries.push_back(*entry);
            }
        }
        std::sort(
            persistedDomainEntries.begin(),
            persistedDomainEntries.end()
            );
        std::sort(
            expectedDomainEntries.begin(),
            expectedDomainEntries.end()
            );
        QVERIFY(persistedDomainEntries == expectedDomainEntries);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT COUNT(*) FROM class_times "
                "WHERE class_id IN (42, 77)"
                )
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::
rejectsInvalidCourseAndPatternAtApplyBoundaryBeforeWrites()
{
    QFile file(
        QStringLiteral(
            CLASSMNGR_SOURCE_DIR
            "/tests/fixtures/imports/schedule_review.xlsx"
            )
        );
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const auto workbook = parseScheduleImportWorkbook(
        file.readAll(),
        ScheduleImportKind::Normal
        );
    const QString parseError =
        workbook.has_value() ? QString() : workbook.error();
    QVERIFY2(workbook.has_value(), qPrintable(parseError));
    QCOMPARE(workbook->sheets.first().users.size(), 1);

    const ScheduleImportUserBlock user =
        workbook->sheets.first().users.first();
    QCOMPARE(user.classes.size(), 3);

    const QString connectionName =
        QStringLiteral("schedule-import-invalid-course-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number, notes) "
                "VALUES ('Sentinel Teacher', 'R-KEEP', 'Retain teacher row')"
                )
            );
        const int retainedTeacherId = query.lastInsertId().toInt();
        QVERIFY(retainedTeacherId > 0);
        constexpr int retainedClassId = 9001;
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO classes (id, name) "
                "VALUES (%1, 'Sentinel Class')"
                )
                .arg(retainedClassId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, "
                "reading_book, essay_book, class_color, font_color, "
                "notes, time_filler_activities) "
                "VALUES (%1, %2, 'M2', 'Ursa', 'Keep Reading', 'Keep Essay', "
                "'#1A2B3C', '#FFFFFF', 'Retain class info', 'Retain filler')"
                )
                .arg(retainedClassId)
                .arg(retainedTeacherId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Friday', '7:00 PM', '7:55 PM')"
                )
                .arg(retainedClassId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('myInfo/name', 'Retained User')"
                )
            );

        const auto snapshotRows = [&query](const QString& sql)
        {
            execOrFail(query, sql);
            QStringList rows;
            while (query.next())
            {
                QStringList columns;
                const QSqlRecord record = query.record();
                for (int column = 0; column < record.count(); ++column)
                {
                    const QVariant value = query.value(column);
                    columns.append(
                        value.isNull()
                            ? QStringLiteral("<NULL>")
                            : value.toString()
                        );
                }
                rows.append(columns.join(QChar(0x1f)));
            }
            return rows;
        };
        const QStringList teachersBefore = snapshotRows(
            QStringLiteral("SELECT * FROM teachers ORDER BY id")
            );
        const QStringList classesBefore = snapshotRows(
            QStringLiteral("SELECT * FROM classes ORDER BY id")
            );
        const QStringList classInfoBefore = snapshotRows(
            QStringLiteral("SELECT * FROM class_info ORDER BY class_id")
            );
        const QStringList classTimesBefore = snapshotRows(
            QStringLiteral(
                "SELECT * FROM class_times "
                "ORDER BY class_id, day, start_time, end_time, id"
                )
            );
        const QStringList settingsBefore = snapshotRows(
            QStringLiteral("SELECT * FROM app_settings ORDER BY key")
            );

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Must not be saved");
        plan.updateProfileName = true;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = user.classes;
        plan.candidates.last().classGrade = QStringLiteral("E4");
        plan.candidates.last().classLevel = QStringLiteral("Zeus");
        for (int index = 0; index < plan.candidates.size(); ++index)
        {
            const ScheduleImportClassCandidate& candidate =
                plan.candidates[index];
            plan.teachers.append(
                {
                    candidate.teacherKey,
                    ScheduleImportTeacherAction::Create,
                    -1,
                    candidate.rooms.value(0)
                }
                );
            plan.classes.append(
                {
                    index,
                    ScheduleImportClassAction::CreateNew,
                    -1
                }
                );
        }

        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);
        QVERIFY(!imported.has_value());
        QVERIFY(
            imported.error().contains(QStringLiteral("invalid class"))
            );

        QCOMPARE(
            snapshotRows(QStringLiteral("SELECT * FROM teachers ORDER BY id")),
            teachersBefore
            );
        QCOMPARE(
            snapshotRows(QStringLiteral("SELECT * FROM classes ORDER BY id")),
            classesBefore
            );
        QCOMPARE(
            snapshotRows(
                QStringLiteral("SELECT * FROM class_info ORDER BY class_id")
                ),
            classInfoBefore
            );
        QCOMPARE(
            snapshotRows(
                QStringLiteral(
                    "SELECT * FROM class_times "
                    "ORDER BY class_id, day, start_time, end_time, id"
                    )
                ),
            classTimesBefore
            );
        QCOMPARE(
            snapshotRows(
                QStringLiteral("SELECT * FROM app_settings ORDER BY key")
                ),
            settingsBefore
            );

        QCOMPARE(user.classes.last().classGrade, QStringLiteral("E4"));
        QCOMPARE(user.classes.last().classLevel, QStringLiteral("Theseus"));
        QCOMPARE(user.classes.last().times.size(), 2);
        plan.candidates.last().classGrade = user.classes.last().classGrade;
        plan.candidates.last().classLevel = user.classes.last().classLevel;
        plan.candidates.last().times = {user.classes.last().times.first()};

        const auto rejectedPattern = repository.apply(plan);
        QVERIFY(!rejectedPattern.has_value());
        QVERIFY(
            rejectedPattern.error().contains(QStringLiteral("meeting pattern"))
            );

        QCOMPARE(
            snapshotRows(QStringLiteral("SELECT * FROM teachers ORDER BY id")),
            teachersBefore
            );
        QCOMPARE(
            snapshotRows(QStringLiteral("SELECT * FROM classes ORDER BY id")),
            classesBefore
            );
        QCOMPARE(
            snapshotRows(
                QStringLiteral("SELECT * FROM class_info ORDER BY class_id")
                ),
            classInfoBefore
            );
        QCOMPARE(
            snapshotRows(
                QStringLiteral(
                    "SELECT * FROM class_times "
                    "ORDER BY class_id, day, start_time, end_time, id"
                    )
                ),
            classTimesBefore
            );
        QCOMPARE(
            snapshotRows(
                QStringLiteral("SELECT * FROM app_settings ORDER BY key")
                ),
            settingsBefore
            );

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::
previewsAndRejectsCheckedInOverlapWorkbookBeforeWrites()
{
    // Common-input differential coverage: the exact fixture SHA-256
    // 2de93c4abdc5e82390adede250e8313501a38d4be2053e929c4adbed6d745312
    // produced these semantic results at legacy 48fc5c5c and current 1236e9cb.
    // The fixture was added in 3121d90c, after the legacy baseline; this does
    // not establish parity against a historical production workbook.
    QFile file(
        QStringLiteral(
            CLASSMNGR_SOURCE_DIR
            "/tests/fixtures/imports/schedule_overlap_conflict.xlsx"
            )
        );
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const auto workbook = parseScheduleImportWorkbook(
        file.readAll(),
        ScheduleImportKind::Normal
        );
    const QString parseError =
        workbook.has_value() ? QString() : workbook.error();
    QVERIFY2(workbook.has_value(), qPrintable(parseError));
    QCOMPARE(workbook->sheets.size(), 1);
    QVERIFY(workbook->sheets.first().visible);
    QCOMPARE(workbook->sheets.first().users.size(), 1);

    const ScheduleImportUserBlock user =
        workbook->sheets.first().users.first();
    QCOMPARE(user.classes.size(), 2);
    const QString firstTeacher = QString::fromUtf8(
        "\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D"
        );
    const QString secondTeacher = QString::fromUtf8(
        "\xEC\x9D\xB4\xEC\x84\xA0\xEC\x83\x9D"
        );
    QCOMPARE(user.classes[0].teacherKey, firstTeacher);
    QCOMPARE(user.classes[0].teacherKr, firstTeacher);
    QCOMPARE(user.classes[0].rooms, QStringList{QStringLiteral("413")});
    QCOMPARE(user.classes[1].teacherKey, secondTeacher);
    QCOMPARE(user.classes[1].teacherKr, secondTeacher);
    QCOMPARE(user.classes[1].rooms, QStringList{QStringLiteral("415")});
    QVERIFY(user.classes[0].meetingPatternError.isEmpty());
    QVERIFY(user.classes[1].meetingPatternError.isEmpty());
    QCOMPARE(user.classes[0].times.size(), 2);
    QCOMPARE(user.classes[1].times.size(), 2);
    QCOMPARE(user.classes[0].times[0].day, QStringLiteral("Monday"));
    QCOMPARE(user.classes[0].times[0].startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(user.classes[1].times[0].day, QStringLiteral("Monday"));
    QCOMPARE(user.classes[1].times[0].startTime, QStringLiteral("4:30 PM"));

    const QString connectionName =
        QStringLiteral("schedule-import-overlap-fixture-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery seedQuery(database);
        execOrFail(
            seedQuery,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number, notes) "
                "VALUES ('Preserved Teacher', 'R-KEEP', 'Preserved notes')"
                )
            );
        const int preservedTeacherId = seedQuery.lastInsertId().toInt();
        QVERIFY(preservedTeacherId > 0);
        constexpr int preservedClassId = 9901;
        execOrFail(
            seedQuery,
            QStringLiteral(
                "INSERT INTO classes (id, name) "
                "VALUES (%1, 'Preserved Class')"
                ).arg(preservedClassId)
            );
        execOrFail(
            seedQuery,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, "
                "class_color, font_color, notes) "
                "VALUES (%1, %2, 'M2', 'Ursa', '#112233', '#FFFFFF', "
                "'Preserved class info')"
                )
                .arg(preservedClassId)
                .arg(preservedTeacherId)
            );
        execOrFail(
            seedQuery,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Sunday', '7:00 PM', '7:55 PM')"
                ).arg(preservedClassId)
            );
        execOrFail(
            seedQuery,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('myInfo/name', 'Preserved User')"
                )
            );

        const auto snapshotRows = [&seedQuery](const QString& sql)
        {
            execOrFail(seedQuery, sql);
            QStringList rows;
            while (seedQuery.next())
            {
                QStringList columns;
                const QSqlRecord record = seedQuery.record();
                for (int column = 0; column < record.count(); ++column)
                {
                    const QVariant value = seedQuery.value(column);
                    columns.append(
                        value.isNull()
                            ? QStringLiteral("<NULL>")
                            : value.toString()
                        );
                }
                rows.append(columns.join(QChar(0x1f)));
            }
            return rows;
        };
        const QStringList teachersBefore = snapshotRows(
            QStringLiteral("SELECT * FROM teachers ORDER BY id")
            );
        const QStringList classesBefore = snapshotRows(
            QStringLiteral("SELECT * FROM classes ORDER BY id")
            );
        const QStringList classInfoBefore = snapshotRows(
            QStringLiteral("SELECT * FROM class_info ORDER BY class_id")
            );
        const QStringList classTimesBefore = snapshotRows(
            QStringLiteral(
                "SELECT * FROM class_times "
                "ORDER BY class_id, day, start_time, end_time, id"
                )
            );
        const QStringList settingsBefore = snapshotRows(
            QStringLiteral("SELECT * FROM app_settings ORDER BY key")
            );

        ScheduleImportRepository repository(database);
        const auto preview = repository.preview(
            user,
            ScheduleImportKind::Normal
            );
        const QString previewError =
            preview.has_value() ? QString() : preview.error();
        QVERIFY2(preview.has_value(), qPrintable(previewError));
        QCOMPARE(preview->user.classes.size(), 2);
        QCOMPARE(preview->classes.size(), 2);
        QCOMPARE(preview->inventory.classCount, 1);
        QVERIFY(preview->inventory.hasRegularHours);
        QVERIFY(!preview->inventory.hasIntensiveHours);
        QCOMPARE(preview->initiallyAbsentClassIds, QList<int>{preservedClassId});
        QCOMPARE(preview->teachers.size(), 2);
        for (int index = 0; index < user.classes.size(); ++index)
        {
            const ScheduleImportTeacherPreview& teacher = preview->teachers[index];
            QCOMPARE(teacher.teacherKey, user.classes[index].teacherKey);
            QCOMPARE(teacher.teacherKr, user.classes[index].teacherKr);
            QCOMPARE(teacher.importedRooms, user.classes[index].rooms);
            QVERIFY(teacher.matchingTeacherIds.isEmpty());
            QCOMPARE(teacher.affectedClassCount, 0);
            const ScheduleImportClassPreview& candidate = preview->classes[index];
            QCOMPARE(candidate.candidateIndex, index);
            QVERIFY(candidate.matchingClassIds.isEmpty());
            QCOMPARE(candidate.suggestedClassId, -1);
            QVERIFY(!candidate.exactMatch);
            QCOMPARE(candidate.matchConfidence, ScheduleImportClassMatchConfidence::None);
        }
        const QStringList persistedBefore = persistedScheduleImportSnapshot(database);

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = user.name;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = user.classes;
        for (int candidateIndex = 0;
             candidateIndex < user.classes.size();
             ++candidateIndex)
        {
            const ScheduleImportClassCandidate& candidate =
                user.classes[candidateIndex];
            plan.classes.append(
                {
                    candidateIndex,
                    ScheduleImportClassAction::CreateNew,
                    -1
                }
                );
            bool hasTeacher = false;
            for (const ScheduleImportTeacherResolution& teacher : plan.teachers)
            {
                if (teacher.teacherKey == candidate.teacherKey)
                {
                    hasTeacher = true;
                    break;
                }
            }
            if (!hasTeacher)
            {
                plan.teachers.append(
                    {
                        candidate.teacherKey,
                        ScheduleImportTeacherAction::Create,
                        -1,
                        candidate.rooms.value(0)
                    }
                    );
            }
        }

        const auto imported = repository.apply(plan);
        QVERIFY(!imported.has_value());
        QCOMPARE(imported.error(), QStringLiteral(
            "The proposed schedule overlaps: E4 Hercules conflicts with "
            "E4 Theseus on Monday."
            ));
        QCOMPARE(persistedScheduleImportSnapshot(database), persistedBefore);

        // Explicit columns omit schema defaults and incidental row IDs, giving
        // the same normalized post-rejection state as the legacy harness.
        const QChar separator(0x1f);
        QCOMPARE(snapshotRows(QStringLiteral(
            "SELECT id, teacher_kr, room_number, notes FROM teachers ORDER BY id"
            )), (QStringList{QStringList{
                QStringLiteral("1"), QStringLiteral("Preserved Teacher"),
                QStringLiteral("R-KEEP"), QStringLiteral("Preserved notes")
            }.join(separator)}));
        QCOMPARE(snapshotRows(QStringLiteral(
            "SELECT id, name FROM classes ORDER BY id"
            )), (QStringList{QStringList{
                QStringLiteral("9901"), QStringLiteral("Preserved Class")
            }.join(separator)}));
        QCOMPARE(snapshotRows(QStringLiteral(
            "SELECT class_id, teacher_id, class_grade, class_level, "
            "class_color, font_color, notes FROM class_info ORDER BY class_id"
            )), (QStringList{QStringList{
                QStringLiteral("9901"), QStringLiteral("1"),
                QStringLiteral("M2"), QStringLiteral("Ursa"),
                QStringLiteral("#112233"), QStringLiteral("#FFFFFF"),
                QStringLiteral("Preserved class info")
            }.join(separator)}));
        QCOMPARE(snapshotRows(QStringLiteral(
            "SELECT class_id, day, start_time, end_time FROM class_times "
            "ORDER BY class_id, day, start_time, end_time"
            )), (QStringList{QStringList{
                QStringLiteral("9901"), QStringLiteral("Sunday"),
                QStringLiteral("7:00 PM"), QStringLiteral("7:55 PM")
            }.join(separator)}));
        QCOMPARE(snapshotRows(QStringLiteral(
            "SELECT key, value FROM app_settings ORDER BY key"
            )), (QStringList{QStringList{
                QStringLiteral("myInfo/name"), QStringLiteral("Preserved User")
            }.join(separator)}));
        QVERIFY(snapshotRows(QStringLiteral(
            "SELECT class_id, day, start_time, end_time FROM class_intensive_times"
            )).isEmpty());
        QVERIFY(snapshotRows(QStringLiteral(
            "SELECT day, start_time FROM intensive_slot_states"
            )).isEmpty());
        QCOMPARE(
            snapshotRows(QStringLiteral("SELECT * FROM teachers ORDER BY id")),
            teachersBefore
            );
        QCOMPARE(
            snapshotRows(QStringLiteral("SELECT * FROM classes ORDER BY id")),
            classesBefore
            );
        QCOMPARE(
            snapshotRows(
                QStringLiteral("SELECT * FROM class_info ORDER BY class_id")
                ),
            classInfoBefore
            );
        QCOMPARE(
            snapshotRows(
                QStringLiteral(
                    "SELECT * FROM class_times "
                    "ORDER BY class_id, day, start_time, end_time, id"
                    )
                ),
            classTimesBefore
            );
        QCOMPARE(
            snapshotRows(QStringLiteral("SELECT * FROM app_settings ORDER BY key")),
            settingsBefore
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
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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

void ScheduleImportTests::
previewsAndAppliesSyntheticIntensiveWorkbookAgainstSeededDatabase()
{
    QFile file(
        QStringLiteral(
            CLASSMNGR_SOURCE_DIR
            "/tests/fixtures/imports/schedule_intensive_synthetic_worksheet.xml"
            )
        );
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));

    const auto workbook = parseScheduleImportWorkbook(
        singleSheetWorkbookData(file.readAll()),
        ScheduleImportKind::Intensive
        );
    const QString parseError =
        workbook.has_value() ? QString() : workbook.error();
    QVERIFY2(workbook.has_value(), qPrintable(parseError));
    QCOMPARE(workbook->sheets.size(), 1);
    QCOMPARE(workbook->sheets.first().name, QStringLiteral("Intensive"));
    QCOMPARE(workbook->sheets.first().users.size(), 1);

    const ScheduleImportUserBlock user =
        workbook->sheets.first().users.first();
    QCOMPARE(user.name, QStringLiteral("Alice"));
    QVERIFY(user.diagnostics.isEmpty());
    QCOMPARE(user.classes.size(), 1);

    const QString expectedTeacher = QString::fromUtf8(
        "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99"
        );
    const ScheduleImportClassCandidate& candidate = user.classes.first();
    QCOMPARE(candidate.teacherKey, expectedTeacher);
    QCOMPARE(candidate.teacherKr, expectedTeacher);
    QCOMPARE(candidate.rooms, QStringList{QStringLiteral("413")});
    QCOMPARE(candidate.classGrade, QStringLiteral("E5"));
    QCOMPARE(candidate.classLevel, QStringLiteral("Zeus"));
    QVERIFY(candidate.meetingPatternError.isEmpty());
    const QStringList expectedCandidateRows{
        QStringLiteral("Monday|10:00 AM|10:55 AM"),
        QStringLiteral("Wednesday|10:00 AM|10:55 AM")
    };
    QStringList candidateRows;
    for (const ClassTime& time : candidate.times)
    {
        candidateRows.append(
            QStringLiteral("%1|%2|%3")
                .arg(time.day, time.startTime, time.endTime)
            );
    }
    QCOMPARE(candidateRows, expectedCandidateRows);

    constexpr int teacherId = 7301;
    constexpr int targetClassId = 7401;
    const QString connectionName =
        QStringLiteral("schedule-import-synthetic-intensive-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        query.prepare(
            QStringLiteral(
                "INSERT INTO teachers (id, teacher_kr, room_number) "
                "VALUES (?, ?, ?)"
                )
            );
        query.addBindValue(teacherId);
        query.addBindValue(expectedTeacher);
        query.addBindValue(QStringLiteral("413"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO classes (id, name) VALUES "
                "(7401, 'E5 Zeus target'), (7402, 'E5 Apollo untouched')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, "
                "class_color, font_color) VALUES "
                "(7401, 7301, 'E5', 'Zeus', '#112233', '#FFFFFF'), "
                "(7402, 7301, 'E5', 'Apollo', '#445566', '#FFFFFF')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(id, class_id, day, start_time, end_time) VALUES "
                "(8101, 7401, 'Tuesday', '4:00 PM', '4:55 PM'), "
                "(8102, 7402, 'Thursday', '5:00 PM', '5:55 PM')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_intensive_times "
                "(id, class_id, day, start_time, end_time) VALUES "
                "(8201, 7401, 'Monday', '9:00 AM', '9:50 AM'), "
                "(8202, 7401, 'Wednesday', '9:00 AM', '9:50 AM'), "
                "(8203, 7402, 'Friday', '9:00 AM', '9:50 AM')"
                )
            );

        execOrFail(
            query,
            QStringLiteral(
                "SELECT id, class_id, day, start_time, end_time "
                "FROM class_times WHERE class_id IN (7401, 7402) "
                "ORDER BY id"
                )
            );
        QStringList regularRowsBefore;
        while (query.next())
        {
            regularRowsBefore.append(
                QStringLiteral("%1|%2|%3|%4|%5")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString(),
                        query.value(3).toString(),
                        query.value(4).toString()
                        )
                );
        }
        const QStringList expectedRegularRows{
            QStringLiteral("8101|7401|Tuesday|4:00 PM|4:55 PM"),
            QStringLiteral("8102|7402|Thursday|5:00 PM|5:55 PM")
        };
        QCOMPARE(regularRowsBefore, expectedRegularRows);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT id, class_id, day, start_time, end_time "
                "FROM class_intensive_times WHERE class_id=7402 ORDER BY id"
                )
            );
        QStringList untouchedIntensiveRowsBefore;
        while (query.next())
        {
            untouchedIntensiveRowsBefore.append(
                QStringLiteral("%1|%2|%3|%4|%5")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString(),
                        query.value(3).toString(),
                        query.value(4).toString()
                        )
                );
        }
        const QStringList expectedUntouchedIntensiveRows{
            QStringLiteral("8203|7402|Friday|9:00 AM|9:50 AM")
        };
        QCOMPARE(
            untouchedIntensiveRowsBefore,
            expectedUntouchedIntensiveRows
            );

        ScheduleImportRepository repository(database);
        const auto preview =
            repository.preview(user, ScheduleImportKind::Intensive);
        const QString previewError =
            preview.has_value() ? QString() : preview.error();
        QVERIFY2(preview.has_value(), qPrintable(previewError));
        QCOMPARE(preview->kind, ScheduleImportKind::Intensive);
        QCOMPARE(preview->inventory.classCount, 2);
        QVERIFY(preview->inventory.hasRegularHours);
        QVERIFY(preview->inventory.hasIntensiveHours);
        QCOMPARE(preview->teachers.size(), 1);
        QCOMPARE(
            preview->teachers.first().matchingTeacherIds,
            QList<int>({teacherId})
            );
        QCOMPARE(preview->classes.size(), 1);
        QCOMPARE(
            preview->classes.first().matchingClassIds,
            QList<int>({targetClassId})
            );
        QCOMPARE(preview->classes.first().suggestedClassId, targetClassId);
        QVERIFY(preview->classes.first().exactMatch);
        QCOMPARE(
            preview->classes.first().matchConfidence,
            ScheduleImportClassMatchConfidence::Confident
            );

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Intensive;
        plan.intensiveMode = ScheduleImportIntensiveMode::UpdateExisting;
        plan.selectedUserName = user.name;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = user.classes;
        plan.intensiveSlotStates = user.intensiveSlotStates;
        plan.teachers = {
            {
                candidate.teacherKey,
                ScheduleImportTeacherAction::Reuse,
                teacherId,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::UpdateExisting,
                targetClassId,
                QStringLiteral("#112233"),
                QStringLiteral("#FFFFFF")
            }
        };

        const auto updated = repository.apply(plan);
        const QString applyError =
            updated.has_value() ? QString() : updated.error();
        QVERIFY2(updated.has_value(), qPrintable(applyError));
        QCOMPARE(updated->classesCreated, 0);
        QCOMPARE(updated->classesUpdated, 1);
        QCOMPARE(updated->teachersCreated, 0);
        QCOMPARE(updated->schedulesCleared, 0);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT class_id, day, start_time, end_time "
                "FROM class_intensive_times WHERE class_id=7401 "
                "ORDER BY day, start_time, end_time, id"
                )
            );
        QStringList persistedTargetRows;
        while (query.next())
        {
            persistedTargetRows.append(
                QStringLiteral("%1|%2|%3|%4")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString(),
                        query.value(3).toString()
                        )
                );
        }
        const QStringList expectedPersistedTargetRows{
            QStringLiteral("7401|Monday|10:00 AM|10:55 AM"),
            QStringLiteral("7401|Wednesday|10:00 AM|10:55 AM")
        };
        QCOMPARE(persistedTargetRows, expectedPersistedTargetRows);

        execOrFail(
            query,
            QStringLiteral(
                "SELECT id, class_id, day, start_time, end_time "
                "FROM class_intensive_times WHERE class_id=7402 ORDER BY id"
                )
            );
        QStringList untouchedIntensiveRowsAfter;
        while (query.next())
        {
            untouchedIntensiveRowsAfter.append(
                QStringLiteral("%1|%2|%3|%4|%5")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString(),
                        query.value(3).toString(),
                        query.value(4).toString()
                        )
                );
        }
        QCOMPARE(
            untouchedIntensiveRowsAfter,
            untouchedIntensiveRowsBefore
            );

        execOrFail(
            query,
            QStringLiteral(
                "SELECT id, class_id, day, start_time, end_time "
                "FROM class_times WHERE class_id IN (7401, 7402) "
                "ORDER BY id"
                )
            );
        QStringList regularRowsAfter;
        while (query.next())
        {
            regularRowsAfter.append(
                QStringLiteral("%1|%2|%3|%4|%5")
                    .arg(
                        query.value(0).toString(),
                        query.value(1).toString(),
                        query.value(2).toString(),
                        query.value(3).toString(),
                        query.value(4).toString()
                        )
                );
        }
        QCOMPARE(regularRowsAfter, regularRowsBefore);

        execOrFail(
            query,
            QStringLiteral("SELECT id FROM classes ORDER BY id")
            );
        QStringList classIdsAfter;
        while (query.next())
        {
            classIdsAfter.append(query.value(0).toString());
        }
        const QStringList expectedClassIds{
            QStringLiteral("7401"),
            QStringLiteral("7402")
        };
        QCOMPARE(classIdsAfter, expectedClassIds);
        execOrFail(
            query,
            QStringLiteral("SELECT id FROM teachers ORDER BY id")
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), teacherId);
        QVERIFY(!query.next());

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::intensiveModesPreserveOrReplaceAbsentHours()
{
    const QString connectionName =
        QStringLiteral("schedule-import-intensive-mode-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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
                "SELECT id, day, start_time, end_time "
                "FROM class_intensive_times WHERE class_id=%1"
                )
                .arg(absentClass)
            );
        QVERIFY(query.next());
        const QStringList absentRowsBefore{
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString()
        };
        QVERIFY(!query.next());
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
                "SELECT day, start_time, end_time "
                "FROM class_intensive_times WHERE class_id=%1 ORDER BY id"
                )
                .arg(importedClass)
            );
        for (const ClassTime& expected : candidate.times)
        {
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), expected.day);
            QCOMPARE(query.value(1).toString(), expected.startTime);
            QCOMPARE(query.value(2).toString(), expected.endTime);
        }
        QVERIFY(!query.next());

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
        execOrFail(
            query,
            QStringLiteral(
                "SELECT id, day, start_time, end_time "
                "FROM class_intensive_times WHERE class_id=%1"
                )
                .arg(absentClass)
        );
        QVERIFY(query.next());
        const QStringList absentRowsAfter{
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString()
        };
        QCOMPARE(absentRowsAfter, absentRowsBefore);
        QVERIFY(!query.next());

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
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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
                "VALUES "
                "(%1, 'Tuesday', '4:00 PM', '4:55 PM'), "
                "(%1, 'Monday', '4:00 PM', '4:55 PM')"
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
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:55 PM")
            },
            {
                QStringLiteral("Monday"),
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
        execOrFail(
            query,
            QStringLiteral(
                "SELECT day, start_time, end_time FROM class_times "
                "WHERE class_id=%1 ORDER BY id"
                )
                .arg(classId)
            );
        QStringList orderedScheduleBefore;
        while (query.next())
        {
            orderedScheduleBefore.append(
                QStringList{
                    query.value(0).toString(),
                    query.value(1).toString(),
                    query.value(2).toString()
                }.join(QLatin1Char('|'))
                );
        }
        QCOMPARE(
            orderedScheduleBefore,
            (QStringList{
                QStringLiteral("Tuesday|4:00 PM|4:55 PM"),
                QStringLiteral("Monday|4:00 PM|4:55 PM")
            })
            );

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
                "FROM class_times WHERE class_id=%1 ORDER BY id"
                )
                .arg(classId)
            );
        QStringList orderedScheduleAfter;
        while (query.next())
        {
            orderedScheduleAfter.append(
                QStringList{
                    query.value(0).toString(),
                    query.value(1).toString(),
                    query.value(2).toString()
                }.join(QLatin1Char('|'))
                );
        }
        QCOMPARE(orderedScheduleAfter, orderedScheduleBefore);

        // Pin the complete baseline-derived post-apply state, including raw
        // class_times IDs and sqlite_sequence after the row re-materialization.
        const QByteArray persistedAfterHash =
            QCryptographicHash::hash(
                persistedScheduleImportSnapshot(database, true)
                    .join(QChar(0x1d))
                    .toUtf8(),
                QCryptographicHash::Sha256
                ).toHex();
        QCOMPARE(
            persistedAfterHash,
            QByteArrayLiteral(
                "08ad64ed3d853e52a1a686c1683d0d1fe8a289026e21d21081e09ee4840c5ebe"
                )
            );

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
    QFile file(
        QStringLiteral(
            CLASSMNGR_SOURCE_DIR
            "/tests/fixtures/imports/schedule_large_conflict.xlsx"
            )
        );
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const auto workbook = parseScheduleImportWorkbook(
        file.readAll(),
        ScheduleImportKind::Normal
        );
    const QString parseError =
        workbook.has_value() ? QString() : workbook.error();
    QVERIFY2(workbook.has_value(), qPrintable(parseError));

    int currentSheetIndex = -1;
    for (int index = 0; index < workbook->sheets.size(); ++index)
    {
        if (workbook->sheets[index].name == QStringLiteral("Current"))
        {
            currentSheetIndex = index;
            break;
        }
    }
    QVERIFY(currentSheetIndex >= 0);
    const ScheduleImportSheet& currentSheet =
        workbook->sheets[currentSheetIndex];

    int aliceIndex = -1;
    for (int index = 0; index < currentSheet.users.size(); ++index)
    {
        if (currentSheet.users[index].name == QStringLiteral("Alice"))
        {
            aliceIndex = index;
            break;
        }
    }
    QVERIFY(aliceIndex >= 0);
    const ScheduleImportUserBlock& alice = currentSheet.users[aliceIndex];

    QList<ScheduleImportClassCandidate> herculesCandidates;
    for (const ScheduleImportClassCandidate& candidate : alice.classes)
    {
        if (
            candidate.classGrade == QStringLiteral("E4")
            && candidate.classLevel == QStringLiteral("Hercules")
            )
        {
            herculesCandidates.append(candidate);
        }
    }
    QCOMPARE(herculesCandidates.size(), 2);

    const QString kimName = QString::fromUtf8(
        "\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D"
        );
    const QString leeName = QString::fromUtf8(
        "\xEC\x9D\xB4\xEC\x84\xA0\xEC\x83\x9D"
        );
    int kimCandidateIndex = -1;
    int leeCandidateIndex = -1;
    for (int index = 0; index < herculesCandidates.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate =
            herculesCandidates[index];
        if (candidate.teacherKr == kimName)
        {
            kimCandidateIndex = index;
        }
        else if (candidate.teacherKr == leeName)
        {
            leeCandidateIndex = index;
        }
    }
    QVERIFY(kimCandidateIndex >= 0);
    QVERIFY(leeCandidateIndex >= 0);
    const ScheduleImportClassCandidate& kimCandidate =
        herculesCandidates[kimCandidateIndex];
    const ScheduleImportClassCandidate& leeCandidate =
        herculesCandidates[leeCandidateIndex];
    QVERIFY(kimCandidate.teacherKey != leeCandidate.teacherKey);
    QCOMPARE(kimCandidate.teacherKey, kimName);
    QCOMPARE(kimCandidate.rooms, QStringList{QStringLiteral("413")});
    QCOMPARE(kimCandidate.times.size(), 2);
    QCOMPARE(kimCandidate.times[0].day, QStringLiteral("Monday"));
    QCOMPARE(kimCandidate.times[0].startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(kimCandidate.times[0].endTime, QStringLiteral("4:55 PM"));
    QCOMPARE(kimCandidate.times[1].day, QStringLiteral("Wednesday"));
    QCOMPARE(kimCandidate.times[1].startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(kimCandidate.times[1].endTime, QStringLiteral("4:55 PM"));
    QCOMPARE(leeCandidate.teacherKey, leeName);
    QCOMPARE(leeCandidate.rooms, QStringList{QStringLiteral("512")});
    QCOMPARE(leeCandidate.times.size(), 2);
    QCOMPARE(leeCandidate.times[0].day, QStringLiteral("Tuesday"));
    QCOMPARE(leeCandidate.times[0].startTime, QStringLiteral("5:00 PM"));
    QCOMPARE(leeCandidate.times[0].endTime, QStringLiteral("5:55 PM"));
    QCOMPARE(leeCandidate.times[1].day, QStringLiteral("Thursday"));
    QCOMPARE(leeCandidate.times[1].startTime, QStringLiteral("5:00 PM"));
    QCOMPARE(leeCandidate.times[1].endTime, QStringLiteral("5:55 PM"));

    const QString connectionName =
        QStringLiteral("schedule-import-duplicate-target-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        query.prepare(
            QStringLiteral(
                "INSERT INTO teachers "
                "(teacher_kr, room_number, notes) VALUES (?, ?, ?)"
                )
            );
        query.addBindValue(kimName);
        query.addBindValue(QStringLiteral("413"));
        query.addBindValue(QStringLiteral("Retain Kim teacher row"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const int kimTeacherId = query.lastInsertId().toInt();
        QVERIFY(kimTeacherId > 0);

        query.prepare(
            QStringLiteral(
                "INSERT INTO teachers "
                "(teacher_kr, room_number, notes) VALUES (?, ?, ?)"
                )
            );
        query.addBindValue(leeName);
        query.addBindValue(QStringLiteral("512"));
        query.addBindValue(QStringLiteral("Retain Lee teacher row"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const int leeTeacherId = query.lastInsertId().toInt();
        QVERIFY(leeTeacherId > 0);

        constexpr int targetClassId = 8801;
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO classes (id, name) "
                "VALUES (%1, 'Retained Hercules destination')"
                ).arg(targetClassId)
            );
        query.prepare(
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, "
                "reading_book, essay_book, class_color, font_color, notes, "
                "time_filler_activities) "
                "VALUES (?, ?, 'E4', 'Hercules', 'Keep Reading', "
                "'Keep Essay', '#123456', '#FFFFFF', 'Keep class info', "
                "'Keep filler')"
                )
            );
        query.addBindValue(targetClassId);
        query.addBindValue(kimTeacherId);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Monday', '9:00 AM', '9:55 AM')"
                ).arg(targetClassId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_intensive_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Wednesday', '10:00 AM', '10:55 AM')"
                ).arg(targetClassId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO intensive_slot_states "
                "(day, start_time, state) "
                "VALUES ('Wednesday', '10:00 AM', 'Available')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('myInfo/name', 'Retained profile')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('schedule-import-snapshot', 'preserve-me')"
                )
            );

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Alice Updated");
        plan.updateProfileName = true;
        plan.unknownCellsAcknowledged = true;
        plan.diagnostics = alice.diagnostics;
        plan.candidates = herculesCandidates;
        plan.teachers = {
            {
                kimCandidate.teacherKey,
                ScheduleImportTeacherAction::Reuse,
                kimTeacherId,
                QStringLiteral("413")
            },
            {
                leeCandidate.teacherKey,
                ScheduleImportTeacherAction::Reuse,
                leeTeacherId,
                QStringLiteral("512")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::UpdateExisting,
                targetClassId
            },
            {
                1,
                ScheduleImportClassAction::UpdateExisting,
                targetClassId
            }
        };

        const QStringList before = persistedScheduleImportSnapshot(database);
        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);
        QVERIFY(!imported.has_value());
        QCOMPARE(
            imported.error(),
            QStringLiteral(
                "Each updated class must have a unique existing target."
                )
            );
        QCOMPARE(persistedScheduleImportSnapshot(database), before);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::
preservesNonpositiveExistingTeacherTargetsAtApply_data()
{
    QTest::addColumn<int>("action");
    QTest::addColumn<int>("targetTeacherId");
    QTest::addColumn<bool>("seedSelectedTeacher");

    const int reuse =
        static_cast<int>(ScheduleImportTeacherAction::Reuse);
    const int updateRoom =
        static_cast<int>(ScheduleImportTeacherAction::UpdateRoom);
    for (const int targetId : {-1, 0})
    {
        const QString idLabel = QString::number(targetId);
        QTest::newRow(qPrintable("reuse-present-" + idLabel))
            << reuse << targetId << true;
        QTest::newRow(qPrintable("reuse-absent-" + idLabel))
            << reuse << targetId << false;
        QTest::newRow(qPrintable("room-update-present-" + idLabel))
            << updateRoom << targetId << true;
        QTest::newRow(qPrintable("room-update-absent-" + idLabel))
            << updateRoom << targetId << false;
    }
}

void ScheduleImportTests::
preservesNonpositiveExistingTeacherTargetsAtApply()
{
    QFETCH(int, action);
    QFETCH(int, targetTeacherId);
    QFETCH(bool, seedSelectedTeacher);

    const QString connectionName =
        QStringLiteral("schedule-import-teacher-sentinel-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        const QString teacherName = sentinelTeacherName();
        QSqlQuery query(database);
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('schedule-import-snapshot', 'preserve-me')"
                )
            );
        if (seedSelectedTeacher)
        {
            query.prepare(
                QStringLiteral(
                    "INSERT INTO teachers (id, teacher_kr, room_number) "
                    "VALUES (?, ?, '413')"
                    )
                );
            query.addBindValue(targetTeacherId);
            query.addBindValue(teacherName);
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        }

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Must not be saved");
        plan.updateProfileName = true;
        plan.unknownCellsAcknowledged = true;
        ScheduleImportClassCandidate candidate =
            stateValidationCandidate(teacherName, QStringLiteral("Zeus"));
        candidate.rooms = {
            QStringLiteral("413"),
            QStringLiteral("414")
        };
        plan.candidates = {candidate};
        plan.teachers = {
            {
                teacherName,
                static_cast<ScheduleImportTeacherAction>(action),
                targetTeacherId,
                QStringLiteral("414")
            }
        };
        plan.classes = {
            {0, ScheduleImportClassAction::Skip, -1}
        };

        const QStringList before = persistedScheduleImportSnapshot(database);
        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);
        if (!seedSelectedTeacher)
        {
            QVERIFY(!imported.has_value());
            QVERIFY(
                imported.error().contains(
                    QStringLiteral("selected Korean teacher is no longer available")
                    )
                );
            QCOMPARE(persistedScheduleImportSnapshot(database), before);
        }
        else
        {
            const QString error =
                imported.has_value() ? QString() : imported.error();
            QVERIFY2(imported.has_value(), qPrintable(error));
            QCOMPARE(imported->classesSkipped, 1);

            execOrFail(
                query,
                QStringLiteral(
                    "SELECT room_number FROM teachers WHERE id=%1"
                    ).arg(targetTeacherId)
                );
            QVERIFY(query.next());
            if (action == static_cast<int>(ScheduleImportTeacherAction::UpdateRoom))
            {
                QCOMPARE(imported->teachersUpdated, 1);
                QCOMPARE(query.value(0).toString(), QStringLiteral("414"));
            }
            else
            {
                QCOMPARE(imported->teachersUpdated, 0);
                QCOMPARE(query.value(0).toString(), QStringLiteral("413"));
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::
convertsNonpositiveCreateAndSkipTeacherTargetsAtApply_data()
{
    QTest::addColumn<int>("action");
    QTest::addColumn<int>("targetTeacherId");
    QTest::addColumn<bool>("expectedSuccess");

    const int create =
        static_cast<int>(ScheduleImportTeacherAction::Create);
    const int skip =
        static_cast<int>(ScheduleImportTeacherAction::Skip);
    for (const int targetId : {-1, 0})
    {
        const QString idLabel = QString::number(targetId);
        QTest::newRow(qPrintable("create-no-target-" + idLabel))
            << create << targetId << true;
        QTest::newRow(qPrintable("skip-no-target-" + idLabel))
            << skip << targetId << true;
    }
    QTest::newRow("create-positive-foreign-target")
        << create << 8241 << false;
    QTest::newRow("skip-positive-foreign-target")
        << skip << 8241 << false;
}

void ScheduleImportTests::
convertsNonpositiveCreateAndSkipTeacherTargetsAtApply()
{
    QFETCH(int, action);
    QFETCH(int, targetTeacherId);
    QFETCH(bool, expectedSuccess);

    const QString connectionName =
        QStringLiteral("schedule-import-create-skip-sentinel-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        const QString teacherName = sentinelTeacherName();
        QSqlQuery query(database);
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('schedule-import-snapshot', 'preserve-me')"
                )
            );
        if (targetTeacherId > 0)
        {
            query.prepare(
                QStringLiteral(
                    "INSERT INTO teachers (id, teacher_kr, room_number) "
                    "VALUES (?, ?, 'foreign-room')"
                    )
                );
            query.addBindValue(targetTeacherId);
            query.addBindValue(foreignTeacherName());
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        }

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Must not be saved");
        plan.updateProfileName = true;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {
            stateValidationCandidate(teacherName, QStringLiteral("Zeus"))
        };
        plan.teachers = {
            {
                teacherName,
                static_cast<ScheduleImportTeacherAction>(action),
                targetTeacherId,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {0, ScheduleImportClassAction::Skip, -1}
        };

        const QStringList before = persistedScheduleImportSnapshot(database);
        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);
        if (!expectedSuccess)
        {
            QVERIFY(!imported.has_value());
            QVERIFY(
                imported.error().contains(
                    QStringLiteral("cannot have an existing target")
                    )
                );
            QCOMPARE(persistedScheduleImportSnapshot(database), before);
        }
        else
        {
            const QString error =
                imported.has_value() ? QString() : imported.error();
            QVERIFY2(imported.has_value(), qPrintable(error));
            QCOMPARE(imported->classesSkipped, 1);
            if (action == static_cast<int>(ScheduleImportTeacherAction::Create))
            {
                QCOMPARE(imported->teachersCreated, 1);
                execOrFail(
                    query,
                    QStringLiteral(
                        "SELECT teacher_kr FROM teachers ORDER BY id"
                        )
                    );
                QVERIFY(query.next());
                QCOMPARE(query.value(0).toString(), teacherName);
                QVERIFY(!query.next());
            }
            else
            {
                QCOMPARE(imported->teachersCreated, 0);
                execOrFail(
                    query,
                    QStringLiteral("SELECT COUNT(*) FROM teachers")
                    );
                QVERIFY(query.next());
                QCOMPARE(query.value(0).toInt(), 0);
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::characterizesClassTargetsAtApply_data()
{
    QTest::addColumn<int>("action");
    QTest::addColumn<int>("targetClassId");
    QTest::addColumn<bool>("seedExistingClass");
    QTest::addColumn<bool>("exactExistingClass");
    QTest::addColumn<QString>("expectedError");

    const int updateExisting =
        static_cast<int>(ScheduleImportClassAction::UpdateExisting);
    const int createNew =
        static_cast<int>(ScheduleImportClassAction::CreateNew);
    const int skip =
        static_cast<int>(ScheduleImportClassAction::Skip);
    QTest::newRow("create-new-negative-sentinel")
        << createNew << -1 << false << false << QString();
    QTest::newRow("create-new-zero-sentinel")
        << createNew << 0 << false << false << QString();
    QTest::newRow("create-new-positive-target-rejected-upstream")
        << createNew << 8101 << false << false
        << QStringLiteral("cannot have an existing target");
    QTest::newRow("targetless-skip-negative-sentinel")
        << skip << -1 << false << false << QString();
    QTest::newRow("targetless-skip-zero-sentinel")
        << skip << 0 << false << false << QString();
    QTest::newRow("skip-keeps-positive-exact-target")
        << skip << 8102 << true << true << QString();
    QTest::newRow("skip-rejects-positive-nonmatch")
        << skip << 8103 << true << false
        << QStringLiteral("unique exact existing match");
    QTest::newRow("update-nonpositive-target-rejected-upstream")
        << updateExisting << -1 << false << false
        << QStringLiteral("unique existing target");
    QTest::newRow("update-zero-target-rejected-upstream")
        << updateExisting << 0 << false << false
        << QStringLiteral("unique existing target");
    QTest::newRow("update-positive-stale-target-rejected")
        << updateExisting << 8104 << false << false
        << QStringLiteral("selected class is no longer available");
}

void ScheduleImportTests::characterizesClassTargetsAtApply()
{
    QFETCH(int, action);
    QFETCH(int, targetClassId);
    QFETCH(bool, seedExistingClass);
    QFETCH(bool, exactExistingClass);
    QFETCH(QString, expectedError);

    const QString connectionName =
        QStringLiteral("schedule-import-class-sentinel-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        const QString teacherName = sentinelTeacherName();
        const bool needsExistingTeacher =
            targetClassId > 0
            && (action == static_cast<int>(ScheduleImportClassAction::Skip)
                || action
                    == static_cast<int>(ScheduleImportClassAction::UpdateExisting));
        int existingTeacherId = -1;
        QSqlQuery query(database);
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('schedule-import-snapshot', 'preserve-me')"
                )
            );
        if (needsExistingTeacher)
        {
            query.prepare(
                QStringLiteral(
                    "INSERT INTO teachers (teacher_kr, room_number) "
                    "VALUES (?, '413')"
                    )
                );
            query.addBindValue(teacherName);
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
            existingTeacherId = query.lastInsertId().toInt();
            QVERIFY(existingTeacherId > 0);
        }
        if (seedExistingClass)
        {
            execOrFail(
                query,
                QStringLiteral(
                    "INSERT INTO classes (id, name) "
                    "VALUES (%1, 'Existing Class')"
                    ).arg(targetClassId)
                );
            execOrFail(
                query,
                QStringLiteral(
                    "INSERT INTO class_info "
                    "(class_id, teacher_id, class_grade, class_level) "
                    "VALUES (%1, %2, 'E5', '%3')"
                    )
                    .arg(targetClassId)
                    .arg(existingTeacherId)
                    .arg(exactExistingClass
                             ? QStringLiteral("Zeus")
                             : QStringLiteral("Apollo"))
                );
            execOrFail(
                query,
                QStringLiteral(
                    "INSERT INTO class_times "
                    "(class_id, day, start_time, end_time) "
                    "VALUES (%1, 'Tuesday', '2:00 PM', '2:55 PM')"
                    ).arg(targetClassId)
                );
        }

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Must not be saved");
        plan.updateProfileName = true;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {
            stateValidationCandidate(teacherName, QStringLiteral("Zeus"))
        };
        plan.teachers = {
            {
                teacherName,
                needsExistingTeacher
                    ? ScheduleImportTeacherAction::Reuse
                    : ScheduleImportTeacherAction::Create,
                needsExistingTeacher ? existingTeacherId : -1,
                QStringLiteral("413")
            }
        };
        plan.classes = {
            {
                0,
                static_cast<ScheduleImportClassAction>(action),
                targetClassId
            }
        };

        const QStringList before = persistedScheduleImportSnapshot(database);
        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);
        if (!expectedError.isEmpty())
        {
            QVERIFY(!imported.has_value());
            QVERIFY2(
                imported.error().contains(expectedError),
                qPrintable(imported.error())
                );
            QCOMPARE(persistedScheduleImportSnapshot(database), before);
        }
        else
        {
            const QString error =
                imported.has_value() ? QString() : imported.error();
            QVERIFY2(imported.has_value(), qPrintable(error));
            if (action == static_cast<int>(ScheduleImportClassAction::CreateNew))
            {
                QCOMPARE(imported->classesCreated, 1);
                QCOMPARE(imported->classesSkipped, 0);
                QCOMPARE(imported->teachersCreated, 1);
                execOrFail(
                    query,
                    QStringLiteral("SELECT COUNT(*) FROM classes")
                    );
                QVERIFY(query.next());
                QCOMPARE(query.value(0).toInt(), 1);
            }
            else
            {
                QCOMPARE(imported->classesSkipped, 1);
                if (targetClassId > 0)
                {
                    QCOMPARE(imported->schedulesCleared, 0);
                    execOrFail(
                        query,
                        QStringLiteral(
                            "SELECT day, start_time, end_time "
                            "FROM class_times WHERE class_id=%1"
                            ).arg(targetClassId)
                        );
                    QVERIFY(query.next());
                    QCOMPARE(query.value(0).toString(), QStringLiteral("Tuesday"));
                    QCOMPARE(query.value(1).toString(), QStringLiteral("2:00 PM"));
                    QCOMPARE(query.value(2).toString(), QStringLiteral("2:55 PM"));
                    QVERIFY(!query.next());
                }
                else
                {
                    QCOMPARE(imported->teachersCreated, 1);
                    execOrFail(
                        query,
                        QStringLiteral("SELECT COUNT(*) FROM classes")
                        );
                    QVERIFY(query.next());
                    QCOMPARE(query.value(0).toInt(), 0);
                }
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::
staleSelectedClassPreservesPersistedSnapshotBeforeWrites()
{
    const QString connectionName =
        QStringLiteral("schedule-import-stale-class-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());
        QSqlQuery query(database);

        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) "
                "VALUES ('김하늘', '413')"
                )
            );
        const int teacherId = query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral("INSERT INTO classes (name) VALUES ('Existing')")
            );
        const int classId = query.lastInsertId().toInt();
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level) "
                "VALUES (%1, %2, 'E4', 'Ares')"
                )
                .arg(classId)
                .arg(teacherId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Tuesday', '3:00 PM', '3:50 PM')"
                )
                .arg(classId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) "
                "VALUES ('schedule-import-snapshot', 'preserve-me')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "CREATE TRIGGER reject_premature_teacher_update "
                "BEFORE UPDATE ON teachers "
                "BEGIN "
                "SELECT RAISE(ABORT, 'write reached before state validation'); "
                "END"
                )
            );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("김하늘");
        candidate.teacherKr = QStringLiteral("김하늘");
        candidate.rooms = {QStringLiteral("999")};
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
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {candidate};
        plan.teachers = {
            {
                QStringLiteral("김하늘"),
                ScheduleImportTeacherAction::UpdateRoom,
                teacherId,
                QStringLiteral("999")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::UpdateExisting,
                classId + 1000
            }
        };

        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);
        QVERIFY(!imported.has_value());
        QVERIFY(
            imported.error().contains(
                QStringLiteral("selected class is no longer available")
                )
            );

        execOrFail(
            query,
            QStringLiteral("SELECT room_number FROM teachers WHERE id=%1")
                .arg(teacherId)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("413"));
        execOrFail(
            query,
            QStringLiteral(
                "SELECT class_grade, class_level FROM class_info "
                "WHERE class_id=%1"
                )
                .arg(classId)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("E4"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("Ares"));
        execOrFail(
            query,
            QStringLiteral(
                "SELECT day, start_time, end_time FROM class_times "
                "WHERE class_id=%1"
                )
                .arg(classId)
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Tuesday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("3:00 PM"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("3:50 PM"));
        QVERIFY(!query.next());
        execOrFail(
            query,
            QStringLiteral(
                "SELECT value FROM app_settings "
                "WHERE key='schedule-import-snapshot'"
                )
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("preserve-me"));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::conflictsRollBackBeforeWrites()
{
    const QString connectionName =
        QStringLiteral("schedule-import-conflict-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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

void ScheduleImportTests::applyUsesBatchedClassInfoByIdAndPreservesClassOrder()
{
    using namespace ClassMngr::Next::Application;

    const QString connectionName =
        QStringLiteral("schedule-import-batched-class-info-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, room_number) "
            "VALUES (11, ?, '413')"));
        query.addBindValue(QString::fromUtf16(u"\uAE40"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        execOrFail(query, QStringLiteral(
            "INSERT INTO classes (id, name) VALUES "
            "(101, 'Same name'), (102, 'Same name'), (103, 'Same name')"));
        execOrFail(query, QStringLiteral(
            "CREATE INDEX classes_name_desc_id "
            "ON classes (name ASC, id DESC)"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, teacher_id, class_grade, class_level, "
            "class_color, font_color) "
            "VALUES (101, 11, 'E5', 'Zeus', '#123456', '#FFFFFF')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, teacher_id, class_grade, class_level, "
            "class_color, font_color) "
            "VALUES (102, NULL, 'E4', 'Ares', '', '')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_times "
            "(id, class_id, day, start_time, end_time) VALUES "
            "(20, 101, 'Tuesday', '2:00 PM', '2:50 PM'), "
            "(10, 101, 'Monday', '4:00 PM', '4:55 PM'), "
            "(30, 103, 'Friday', '1:00 PM', '1:50 PM')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_intensive_times "
            "(id, class_id, day, start_time, end_time) VALUES "
            "(20, 101, 'Thursday', '2:00 PM', '2:50 PM'), "
            "(10, 101, 'Monday', '4:00 PM', '4:55 PM'), "
            "(30, 103, 'Friday', '3:00 PM', '3:50 PM')"));

        ClassRepository classRepository(database);
        const auto existingClasses = classRepository.getClasses();
        QVERIFY(existingClasses.has_value());
        QCOMPARE(existingClasses->size(), 3);
        QCOMPARE(existingClasses->at(0).id, 103);
        QCOMPARE(existingClasses->at(1).id, 102);
        QCOMPARE(existingClasses->at(2).id, 101);

        ClassInfoRepository classInfoRepository(database);
        const auto classInfos = classInfoRepository.loadScheduleClassInfos();
        QVERIFY(classInfos.has_value());
        QCOMPARE(classInfos->size(), 3);
        QCOMPARE(classInfos->at(0).classId, 101);
        QCOMPARE(classInfos->at(1).classId, 102);
        QCOMPARE(classInfos->at(2).classId, 103);
        QCOMPARE(classInfos->at(0).classColor, QStringLiteral("#123456"));
        QCOMPARE(classInfos->at(1).teacherId, -1);
        QCOMPARE(classInfos->at(1).classColor, QStringLiteral("#FFFFFF"));
        QCOMPARE(classInfos->at(1).fontColor, QStringLiteral("#000000"));
        QCOMPARE(classInfos->at(2).teacherId, -1);
        QCOMPARE(classInfos->at(2).classColor, QStringLiteral("#FFFFFF"));
        QCOMPARE(classInfos->at(2).fontColor, QStringLiteral("#000000"));
        QCOMPARE(classInfos->at(0).classTimes.size(), 2);
        QCOMPARE(classInfos->at(0).classTimes.at(0).day, QStringLiteral("Monday"));
        QCOMPARE(classInfos->at(0).classTimes.at(1).day, QStringLiteral("Tuesday"));
        QCOMPARE(classInfos->at(0).intensiveTimes.size(), 2);
        QCOMPARE(classInfos->at(0).intensiveTimes.at(0).day, QStringLiteral("Monday"));
        QCOMPARE(classInfos->at(0).intensiveTimes.at(1).day, QStringLiteral("Thursday"));
        QCOMPARE(classInfos->at(2).classTimes.size(), 1);
        QCOMPARE(classInfos->at(2).intensiveTimes.size(), 1);
        const ScheduleClassInfoReadMetrics& metrics =
            classInfoRepository.scheduleClassInfoReadMetrics();
        QCOMPARE(metrics.scheduleClassInfosCallCount, 1);
        QCOMPARE(metrics.singleClassInfoReadCount, 0);
        QCOMPARE(metrics.metadataStatementCount, 1);
        QCOMPARE(metrics.regularScheduleStatementCount, 1);
        QCOMPARE(metrics.intensiveScheduleStatementCount, 1);

        const auto teacherId =
            ClassMngr::Next::Domain::TeacherId::fromString("11");
        const auto classId =
            ClassMngr::Next::Domain::ClassId::fromString("101");
        QVERIFY(teacherId.has_value());
        QVERIFY(classId.has_value());

        auto skippedExactClass = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        skippedExactClass.teachers[0].action =
            ScheduleImportReviewTeacherAction::Reuse;
        skippedExactClass.teachers[0].targetTeacherId = *teacherId;
        skippedExactClass.classes[0].action =
            ScheduleImportReviewClassAction::Skip;
        skippedExactClass.classes[0].targetClassId = *classId;

        ScheduleImportRepository repository(database);
        const auto skippedResult = repository.applyTyped(skippedExactClass);
        QVERIFY2(
            skippedResult.has_value(),
            qPrintable(typedApplyFailureMessage(skippedResult))
            );
        QCOMPARE(skippedResult->classesSkipped, 1);

        auto regularConflict = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus"),
            typedCandidate(u"\uBC15", u"E5", u"Apollo")
        });
        regularConflict.teachers[0].action =
            ScheduleImportReviewTeacherAction::Reuse;
        regularConflict.teachers[0].targetTeacherId = *teacherId;
        regularConflict.classes[0].action =
            ScheduleImportReviewClassAction::Skip;
        regularConflict.classes[0].targetClassId = *classId;
        const QStringList beforeRegularConflict =
            persistedScheduleImportSnapshot(database, true);
        const auto regularResult = repository.applyTyped(regularConflict);
        QVERIFY(!regularResult.has_value());
        QVERIFY(regularResult.error().stateValidationError.has_value());
        QCOMPARE(
            regularResult.error().stateValidationError->code,
            ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
            );
        QCOMPARE(
            regularResult.error().stateValidationError->classLabel,
            std::string("E5 Zeus")
            );
        QCOMPARE(
            regularResult.error().stateValidationError->conflictingClassLabel,
            std::string("E5 Apollo")
            );
        QCOMPARE(
            persistedScheduleImportSnapshot(database, true),
            beforeRegularConflict
            );

        auto intensiveConflict = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus"),
            typedCandidate(u"\uBC15", u"E5", u"Apollo")
        });
        intensiveConflict.intensiveSchedule = true;
        intensiveConflict.teachers[0].action =
            ScheduleImportReviewTeacherAction::Reuse;
        intensiveConflict.teachers[0].targetTeacherId = *teacherId;
        intensiveConflict.classes[0].action =
            ScheduleImportReviewClassAction::Skip;
        intensiveConflict.classes[0].targetClassId = *classId;
        const QStringList beforeIntensiveConflict =
            persistedScheduleImportSnapshot(database, true);
        const auto intensiveResult = repository.applyTyped(intensiveConflict);
        QVERIFY(!intensiveResult.has_value());
        QVERIFY(intensiveResult.error().stateValidationError.has_value());
        QCOMPARE(
            intensiveResult.error().stateValidationError->code,
            ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
            );
        QCOMPARE(
            intensiveResult.error().stateValidationError->classLabel,
            std::string("E5 Zeus")
            );
        QCOMPARE(
            intensiveResult.error().stateValidationError->conflictingClassLabel,
            std::string("E5 Apollo")
            );
        QCOMPARE(
            persistedScheduleImportSnapshot(database, true),
            beforeIntensiveConflict
            );

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::applyBatchReadFailureRollsBackBeforeWrites()
{
    using namespace ClassMngr::Next::Application;

    const QString connectionName =
        QStringLiteral("schedule-import-batch-read-failure-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, room_number) "
            "VALUES (11, ?, '413')"));
        query.addBindValue(QString::fromUtf16(u"\uAE40"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        execOrFail(query, QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (101, 'Existing')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, teacher_id, class_grade, class_level) "
            "VALUES (101, 11, 'E3', 'Low')"));
        execOrFail(query, QStringLiteral(
            "CREATE TRIGGER reject_premature_apply_write "
            "BEFORE UPDATE ON teachers BEGIN "
            "SELECT RAISE(ABORT, 'write ran before batch read completed'); "
            "END"));

        const QStringList beforeFailure =
            persistedScheduleImportSnapshot(database);
        execOrFail(query, QStringLiteral(
            "DROP TABLE class_intensive_times"));

        auto request = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        const auto teacherId =
            ClassMngr::Next::Domain::TeacherId::fromString("11");
        QVERIFY(teacherId.has_value());
        request.teachers[0].action =
            ScheduleImportReviewTeacherAction::UpdateRoom;
        request.teachers[0].targetTeacherId = *teacherId;
        request.teachers[0].selectedRoom = u"413";

        ScheduleImportRepository repository(database);
        const auto failed = repository.applyTyped(request);
        QVERIFY(!failed.has_value());
        QVERIFY2(
            typedApplyFailureMessage(failed).contains(
                QStringLiteral("class_intensive_times")),
            qPrintable(typedApplyFailureMessage(failed))
            );
        QVERIFY(!typedApplyFailureMessage(failed).contains(
            QStringLiteral("write ran before batch read completed")));

        execOrFail(query, QStringLiteral(
            "CREATE TABLE class_intensive_times ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "class_id INTEGER, day TEXT, start_time TEXT, end_time TEXT, "
            "UNIQUE(day, start_time))"));
        QCOMPARE(
            persistedScheduleImportSnapshot(database),
            beforeFailure
            );
        execOrFail(query, QStringLiteral(
            "SELECT room_number FROM teachers WHERE id=11"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("413"));

        QVERIFY(database.transaction());
        QVERIFY(database.rollback());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::applyTeacherReadFailureRollsBackBeforeWrites()
{
    using namespace ClassMngr::Next::Application;

    const QString connectionName =
        QStringLiteral("schedule-import-teacher-read-failure-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, teacher_en, room_number) "
            "VALUES (11, ?, 'Existing English Name', '413')"));
        query.addBindValue(QString::fromUtf16(u"\uAE40"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        execOrFail(query, QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (101, 'Existing')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, teacher_id, class_grade, class_level) "
            "VALUES (101, 11, 'E3', 'Low')"));
        const QStringList beforeFailure =
            persistedScheduleImportSnapshot(database);

        execOrFail(query, QStringLiteral(
            "ALTER TABLE teachers RENAME COLUMN teacher_kr "
            "TO teacher_kr_unavailable"));

        ScheduleImportRepository repository(database);
        const auto failed = repository.applyTyped(typedCreateRequest({
            typedCandidate(u"\uBC15", u"E5", u"Zeus")
        }));
        QVERIFY(!failed.has_value());
        QVERIFY2(
            typedApplyFailureMessage(failed).contains(
                QStringLiteral("teacher_kr"),
                Qt::CaseInsensitive
                ),
            qPrintable(typedApplyFailureMessage(failed))
            );

        execOrFail(query, QStringLiteral(
            "ALTER TABLE teachers RENAME COLUMN teacher_kr_unavailable "
            "TO teacher_kr"));
        QCOMPARE(
            persistedScheduleImportSnapshot(database),
            beforeFailure
            );

        QVERIFY2(database.transaction(), qPrintable(database.lastError().text()));
        QVERIFY(database.rollback());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::typedApplyRejectsStaleSelectedClassBeforeWrites()
{
    const QString connectionName =
        QStringLiteral("schedule-import-typed-stale-class-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO teachers (teacher_kr, room_number) VALUES (?, '413')"));
        query.addBindValue(QString::fromUtf16(u"\uAE40"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const int teacherId = query.lastInsertId().toInt();
        QVERIFY(teacherId > 0);
        execOrFail(query, QStringLiteral("INSERT INTO classes (name) VALUES ('Existing')"));
        const int classId = query.lastInsertId().toInt();
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info (class_id, teacher_id, class_grade, class_level) "
            "VALUES (%1, %2, 'E4', 'Ares')").arg(classId).arg(teacherId));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (%1, 'Tuesday', '3:00 PM', '3:50 PM')").arg(classId));
        execOrFail(query, QStringLiteral(
            "INSERT INTO app_settings (key, value) "
            "VALUES ('schedule-import-snapshot', 'preserve-me')"));
        execOrFail(query, QStringLiteral(
            "CREATE TRIGGER reject_premature_typed_teacher_update "
            "BEFORE UPDATE ON teachers BEGIN "
            "SELECT RAISE(ABORT, 'write reached before state validation'); END"));

        auto request = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        const auto typedTeacherId =
            ClassMngr::Next::Domain::TeacherId::fromString(
                std::to_string(teacherId));
        const auto staleClassId =
            ClassMngr::Next::Domain::ClassId::fromString(
                std::to_string(classId + 1000));
        QVERIFY(typedTeacherId.has_value());
        QVERIFY(staleClassId.has_value());
        request.candidates[0].rooms = {u"999"};
        request.teachers[0].action =
            ClassMngr::Next::Application::ScheduleImportReviewTeacherAction::UpdateRoom;
        request.teachers[0].targetTeacherId = *typedTeacherId;
        request.teachers[0].selectedRoom = u"999";
        request.classes[0].action =
            ClassMngr::Next::Application::ScheduleImportReviewClassAction::UpdateExisting;
        request.classes[0].targetClassId = *staleClassId;

        const QStringList before = persistedScheduleImportSnapshot(database);
        ScheduleImportRepository repository(database);
        const auto imported = repository.applyTyped(request);
        QVERIFY(!imported.has_value());
        QVERIFY(imported.error().stateValidationError.has_value());
        QCOMPARE(
            imported.error().stateValidationError->code,
            ClassMngr::Next::Application::
                ScheduleImportStateValidationErrorCode::SelectedClassUnavailable
            );
        const QString staleError = typedApplyFailureMessage(imported);
        QVERIFY2(staleError.contains(
            QStringLiteral("selected class is no longer available")),
            qPrintable(staleError));
        QVERIFY(!imported.error().policyIssue.has_value());
        QVERIFY(!imported.error().teacherTargetIssue.has_value());
        QCOMPARE(persistedScheduleImportSnapshot(database), before);

        const auto unavailableTeacherId =
            ClassMngr::Next::Domain::TeacherId::fromString(
                std::to_string(teacherId + 1000));
        QVERIFY(unavailableTeacherId.has_value());
        request.teachers[0].targetTeacherId = *unavailableTeacherId;
        request.classes[0].action =
            ClassMngr::Next::Application::ScheduleImportReviewClassAction::CreateNew;
        request.classes[0].targetClassId.reset();
        const auto staleTeacher = repository.applyTyped(request);
        QVERIFY(!staleTeacher.has_value());
        QVERIFY(staleTeacher.error().stateValidationError.has_value());
        QCOMPARE(
            staleTeacher.error().stateValidationError->code,
            ClassMngr::Next::Application::
                ScheduleImportStateValidationErrorCode::SelectedTeacherUnavailable
            );
        QVERIFY(!staleTeacher.error().policyIssue.has_value());
        QVERIFY(!staleTeacher.error().teacherTargetIssue.has_value());
        QCOMPARE(persistedScheduleImportSnapshot(database), before);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::typedApplyPreservesExactTargetIdsThroughStateValidation()
{
    const QString connectionName =
        QStringLiteral("schedule-import-typed-exact-target-ids-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, room_number) "
            "VALUES (1, ?, '413')"));
        query.addBindValue(QString::fromUtf16(u"\uAE40"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        execOrFail(query, QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (1, 'Existing class')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, teacher_id, class_grade, class_level) "
            "VALUES (1, 1, 'E5', 'Zeus')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_times "
            "(class_id, day, start_time, end_time) "
            "VALUES (1, 'Friday', '2:00 PM', '2:55 PM')"));

        const auto canonicalTeacherId =
            ClassMngr::Next::Domain::TeacherId::fromString("1");
        const auto canonicalClassId =
            ClassMngr::Next::Domain::ClassId::fromString("1");
        const auto noncanonicalTeacherId =
            ClassMngr::Next::Domain::TeacherId::fromString("01");
        const auto noncanonicalClassId =
            ClassMngr::Next::Domain::ClassId::fromString("01");
        QVERIFY(canonicalTeacherId.has_value());
        QVERIFY(canonicalClassId.has_value());
        QVERIFY(noncanonicalTeacherId.has_value());
        QVERIFY(noncanonicalClassId.has_value());

        ScheduleImportRepository repository(database);
        auto canonicalTargets = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        canonicalTargets.teachers[0].action =
            ClassMngr::Next::Application::ScheduleImportReviewTeacherAction::Reuse;
        canonicalTargets.teachers[0].targetTeacherId = *canonicalTeacherId;
        canonicalTargets.classes[0].action =
            ClassMngr::Next::Application::
                ScheduleImportReviewClassAction::UpdateExisting;
        canonicalTargets.classes[0].targetClassId = *canonicalClassId;

        const auto canonicalResult = repository.applyTyped(canonicalTargets);
        const QString canonicalError = typedApplyFailureMessage(canonicalResult);
        QVERIFY2(
            canonicalResult.has_value(),
            qPrintable(canonicalError)
            );
        QCOMPARE(canonicalResult->classesUpdated, 1);
        QCOMPARE(canonicalResult->teachersCreated, 0);

        auto noncanonicalTeacherTarget = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        noncanonicalTeacherTarget.teachers[0].action =
            ClassMngr::Next::Application::ScheduleImportReviewTeacherAction::Reuse;
        noncanonicalTeacherTarget.teachers[0].targetTeacherId =
            *noncanonicalTeacherId;
        const QStringList beforeTeacherRejection =
            persistedScheduleImportSnapshot(database, true);
        const auto noncanonicalTeacherResult =
            repository.applyTyped(noncanonicalTeacherTarget);
        QVERIFY(!noncanonicalTeacherResult.has_value());
        QVERIFY(noncanonicalTeacherResult.error().stateValidationError.has_value());
        QCOMPARE(
            noncanonicalTeacherResult.error().stateValidationError->code,
            ClassMngr::Next::Application::
                ScheduleImportStateValidationErrorCode::SelectedTeacherUnavailable
            );
        QVERIFY2(
            typedApplyFailureMessage(noncanonicalTeacherResult).contains(
                QStringLiteral("selected Korean teacher is no longer available"),
                Qt::CaseInsensitive),
            qPrintable(typedApplyFailureMessage(noncanonicalTeacherResult))
            );
        QCOMPARE(
            persistedScheduleImportSnapshot(database, true),
            beforeTeacherRejection
            );

        auto noncanonicalClassTarget = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        noncanonicalClassTarget.teachers[0].action =
            ClassMngr::Next::Application::ScheduleImportReviewTeacherAction::Reuse;
        noncanonicalClassTarget.teachers[0].targetTeacherId =
            *canonicalTeacherId;
        noncanonicalClassTarget.classes[0].action =
            ClassMngr::Next::Application::
                ScheduleImportReviewClassAction::UpdateExisting;
        noncanonicalClassTarget.classes[0].targetClassId =
            *noncanonicalClassId;
        const QStringList beforeClassRejection =
            persistedScheduleImportSnapshot(database, true);
        const auto noncanonicalClassResult =
            repository.applyTyped(noncanonicalClassTarget);
        QVERIFY(!noncanonicalClassResult.has_value());
        QVERIFY(noncanonicalClassResult.error().stateValidationError.has_value());
        QCOMPARE(
            noncanonicalClassResult.error().stateValidationError->code,
            ClassMngr::Next::Application::
                ScheduleImportStateValidationErrorCode::SelectedClassUnavailable
            );
        QVERIFY2(
            typedApplyFailureMessage(noncanonicalClassResult).contains(
                QStringLiteral("selected class is no longer available"),
                Qt::CaseInsensitive),
            qPrintable(typedApplyFailureMessage(noncanonicalClassResult))
            );
        QCOMPARE(
            persistedScheduleImportSnapshot(database, true),
            beforeClassRejection
            );

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::typedApplyRejectsOverlappingSchedulesBeforeWrites()
{
    const QString connectionName =
        QStringLiteral("schedule-import-typed-overlap-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        auto request = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus"),
            typedCandidate(u"\uBC15", u"E5", u"Apollo")
        });
        ScheduleImportRepository repository(database);
        const QStringList before = persistedScheduleImportSnapshot(database);
        const auto imported = repository.applyTyped(request);
        QVERIFY(!imported.has_value());
        QVERIFY(imported.error().stateValidationError.has_value());
        QCOMPARE(
            imported.error().stateValidationError->code,
            ClassMngr::Next::Application::
                ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
            );
        QCOMPARE(
            imported.error().stateValidationError->classLabel,
            std::string("E5 Zeus")
            );
        QCOMPARE(
            imported.error().stateValidationError->conflictingClassLabel,
            std::string("E5 Apollo")
            );
        QCOMPARE(imported.error().stateValidationError->day, std::string("Monday"));
        QCOMPARE(
            imported.error().stateValidationError->startTime,
            std::string("4:00 PM")
            );
        QCOMPARE(
            imported.error().stateValidationError->endTime,
            std::string("4:55 PM")
            );
        QVERIFY(!imported.error().policyIssue.has_value());
        QVERIFY(!imported.error().teacherTargetIssue.has_value());
        QVERIFY(typedApplyFailureMessage(imported).contains(QStringLiteral("overlaps")));

        QSqlQuery query(database);
        execOrFail(query, QStringLiteral("SELECT COUNT(*) FROM teachers"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        execOrFail(query, QStringLiteral("SELECT COUNT(*) FROM classes"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        execOrFail(query, QStringLiteral("SELECT COUNT(*) FROM class_times"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        QCOMPARE(persistedScheduleImportSnapshot(database), before);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::typedWriteFailureRollsBackEveryChange()
{
    const QString connectionName =
        QStringLiteral("schedule-import-typed-rollback-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        execOrFail(query, QStringLiteral(
            "CREATE TRIGGER reject_typed_imported_time "
            "BEFORE INSERT ON class_times BEGIN "
            "SELECT RAISE(ABORT, 'forced typed schedule failure'); END"));

        auto request = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        request.selectedUserName = u"Alice";
        request.saveProfileNameIfBlank = true;
        const QStringList before = persistedScheduleImportSnapshot(database, true);

        ScheduleImportRepository repository(database);
        const auto imported = repository.applyTyped(request);
        QVERIFY(!imported.has_value());
        QVERIFY(!imported.error().policyIssue.has_value());
        QVERIFY(!imported.error().teacherTargetIssue.has_value());
        QVERIFY(!imported.error().stateValidationError.has_value());
        QVERIFY(typedApplyFailureMessage(imported).contains(
            QStringLiteral("forced typed schedule failure")));
        QCOMPARE(persistedScheduleImportSnapshot(database, true), before);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::typedIntensiveApplyMapsModeAndSlotState()
{
    const QString connectionName =
        QStringLiteral("schedule-import-typed-intensive-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO teachers (teacher_kr, room_number) VALUES (?, '500')"));
        query.addBindValue(QString::fromUtf16(u"\uBC15"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const int existingTeacherId = query.lastInsertId().toInt();
        QVERIFY(existingTeacherId > 0);
        execOrFail(query, QStringLiteral(
            "INSERT INTO classes (name) VALUES ('Old intensive class')"));
        const int existingClassId = query.lastInsertId().toInt();
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info (class_id, teacher_id, class_grade, class_level) "
            "VALUES (%1, %2, 'E5', 'Ares')")
                .arg(existingClassId)
                .arg(existingTeacherId));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (%1, 'Friday', '10:00 AM', '10:55 AM')")
                .arg(existingClassId));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_intensive_times (class_id, day, start_time, end_time) "
            "VALUES (%1, 'Friday', '2:00 PM', '2:55 PM')")
                .arg(existingClassId));

        auto request = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E4", u"Hercules")
        });
        request.intensiveSchedule = true;
        request.intensiveMode =
            ClassMngr::Next::Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew;
        request.selectedUserName = u"Alice";
        request.saveProfileNameIfBlank = true;
        request.diagnostics.push_back({
            u"Sheet", u"Alice", u"B12", u"?", u"Ignored cell"
        });
        request.intensiveSlotStates.push_back({
            u"Tuesday", u"15:00", u"essay"
        });

        ScheduleImportRepository repository(database);
        const auto imported = repository.applyTyped(request);
        const QString importError = typedApplyFailureMessage(imported);
        QVERIFY2(imported.has_value(), qPrintable(importError));
        QCOMPARE(imported->teachersCreated, 1);
        QCOMPARE(imported->classesCreated, 1);
        QCOMPARE(imported->schedulesCleared, 1);
        QCOMPARE(imported->ignoredCells, 1);
        QVERIFY(imported->profileNameUpdated);

        execOrFail(query, QStringLiteral(
            "SELECT COUNT(*) FROM class_intensive_times WHERE class_id=%1")
                .arg(existingClassId));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        execOrFail(query, QStringLiteral(
            "SELECT day, start_time, end_time FROM class_intensive_times "
            "ORDER BY day, start_time"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Monday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("4:00 PM"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("4:55 PM"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Wednesday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("4:00 PM"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("4:55 PM"));
        QVERIFY(!query.next());

        execOrFail(query, QStringLiteral(
            "SELECT day, start_time, state FROM intensive_slot_states"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Tuesday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("15:00"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("essay"));
        QVERIFY(!query.next());

        execOrFail(query, QStringLiteral(
            "SELECT value FROM app_settings WHERE key='myInfo/name'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Alice"));
        QVERIFY(!query.next());

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::typedAndLegacyApplyShareNormalAndIntensiveResults()
{
    using namespace ClassMngr::Next::Application;

    const auto compareApplyPaths = [this](
        const QString& label,
        const auto& makeRequest,
        const auto& verifySummary
        )
    {
        const QString typedConnectionName =
            QStringLiteral("schedule-import-typed-legacy-%1-typed-%2")
                .arg(label, QUuid::createUuid().toString());
        const QString legacyConnectionName =
            QStringLiteral("schedule-import-typed-legacy-%1-legacy-%2")
                .arg(label, QUuid::createUuid().toString());
        {
            QSqlDatabase typedDatabase = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), typedConnectionName);
            typedDatabase.setDatabaseName(QStringLiteral(":memory:"));
            QVERIFY(typedDatabase.open());
            QVERIFY(DatabaseSchemaManager::ensureSchema(typedDatabase).has_value());

            QSqlDatabase legacyDatabase = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), legacyConnectionName);
            legacyDatabase.setDatabaseName(QStringLiteral(":memory:"));
            QVERIFY(legacyDatabase.open());
            QVERIFY(DatabaseSchemaManager::ensureSchema(legacyDatabase).has_value());

            ScheduleImportApplyRequest typedRequest = makeRequest(typedDatabase);
            ScheduleImportApplyRequest legacyRequest = makeRequest(legacyDatabase);
            ScheduleImportRepository typedRepository(typedDatabase);
            ScheduleImportRepository legacyRepository(legacyDatabase);
            const auto typedResult = typedRepository.applyTyped(typedRequest);
            const QString typedError = typedApplyFailureMessage(typedResult);
            QVERIFY2(typedResult.has_value(), qPrintable(typedError));

            const auto legacyResult = legacyRepository.apply(
                legacyPlanFromTypedRequest(legacyRequest));
            const QString legacyError = legacyResult.has_value()
                ? QString()
                : legacyResult.error();
            QVERIFY2(legacyResult.has_value(), qPrintable(legacyError));

            compareScheduleImportSummaries(*legacyResult, *typedResult);
            QCOMPARE(
                persistedScheduleImportSnapshot(typedDatabase),
                persistedScheduleImportSnapshot(legacyDatabase)
                );
            verifySummary(*typedResult);

            typedDatabase.close();
            legacyDatabase.close();
        }
        QSqlDatabase::removeDatabase(typedConnectionName);
        QSqlDatabase::removeDatabase(legacyConnectionName);
    };

    const auto seedNormalRequest = [](QSqlDatabase& database)
    {
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO teachers (teacher_kr, room_number) VALUES (?, ?)"));
        query.addBindValue(QString::fromUtf16(u"\uAE40"));
        query.addBindValue(QStringLiteral("413"));
        if (!query.exec())
        {
            qFatal("Unable to seed comparison teacher: %s",
                   qPrintable(query.lastError().text()));
        }
        const int updateTeacherId = query.lastInsertId().toInt();
        query.prepare(QStringLiteral(
            "INSERT INTO teachers (teacher_kr, room_number) VALUES (?, ?)"));
        query.addBindValue(QString::fromUtf16(u"\uCD5C"));
        query.addBindValue(QStringLiteral("500"));
        if (!query.exec())
        {
            qFatal("Unable to seed comparison teacher: %s",
                   qPrintable(query.lastError().text()));
        }
        const int skippedTeacherId = query.lastInsertId().toInt();

        execOrFail(query, QStringLiteral(
            "INSERT INTO classes (name) VALUES ('E5 Zeus')"));
        const int updateClassId = query.lastInsertId().toInt();
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, teacher_id, class_grade, class_level, class_color, font_color) "
            "VALUES (%1, %2, 'E5', 'Zeus', '#FFFFFF', '#000000')")
                .arg(updateClassId)
                .arg(updateTeacherId));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (%1, 'Monday', '3:00 PM', '3:55 PM')")
                .arg(updateClassId));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (%1, 'Wednesday', '3:00 PM', '3:55 PM')")
                .arg(updateClassId));

        execOrFail(query, QStringLiteral(
            "INSERT INTO classes (name) VALUES ('E5 Hera')"));
        const int skippedClassId = query.lastInsertId().toInt();
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, teacher_id, class_grade, class_level, class_color, font_color) "
            "VALUES (%1, %2, 'E6', 'Hera', '#FFFFFF', '#000000')")
                .arg(skippedClassId)
                .arg(skippedTeacherId));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (%1, 'Friday', '2:00 PM', '2:55 PM')")
                .arg(skippedClassId));

        auto request = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus"),
            typedCandidate(u"\uBC15", u"E5", u"Apollo"),
            typedCandidate(u"\uCD5C", u"E6", u"Hera")
        });
        request.diagnostics.push_back({
            u"Sheet", u"Alice", u"B12", u"?", u"Ignored cell"
        });
        request.candidates[0].times = {
            {u"Monday", u"5:00 PM", u"5:55 PM"},
            {u"Wednesday", u"5:00 PM", u"5:55 PM"}
        };
        request.candidates[1].times = {
            {u"Monday", u"4:00 PM", u"4:55 PM"},
            {u"Wednesday", u"4:00 PM", u"4:55 PM"}
        };
        request.candidates[2].times = {
            {u"Friday", u"2:00 PM", u"2:55 PM"}
        };
        request.candidates[0].rooms = {u"999"};
        request.candidates[1].rooms = {u"777"};
        request.candidates[2].rooms = {u"500"};

        const auto updateTeacher = ClassMngr::Next::Domain::TeacherId::fromString(
            std::to_string(updateTeacherId));
        const auto updateClass = ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(updateClassId));
        const auto skippedClass = ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(skippedClassId));
        request.teachers[0].action = ScheduleImportReviewTeacherAction::UpdateRoom;
        request.teachers[0].targetTeacherId = updateTeacher.value();
        request.teachers[0].selectedRoom = u"999";
        request.teachers[1].selectedRoom = u"777";
        request.teachers[2].action = ScheduleImportReviewTeacherAction::Skip;
        request.teachers[2].targetTeacherId.reset();
        request.teachers[2].selectedRoom.clear();
        request.classes[0].action = ScheduleImportReviewClassAction::UpdateExisting;
        request.classes[0].targetClassId = updateClass.value();
        request.classes[0].classColor = u"#123456";
        request.classes[0].fontColor = u"#FFFFFF";
        request.classes[2].action = ScheduleImportReviewClassAction::Skip;
        request.classes[2].targetClassId = skippedClass.value();
        return request;
    };

    compareApplyPaths(
        QStringLiteral("normal"),
        seedNormalRequest,
        [](const ScheduleImportSummary& summary)
        {
            QCOMPARE(summary.teachersCreated, 1);
            QCOMPARE(summary.teachersUpdated, 1);
            QCOMPARE(summary.classesCreated, 1);
            QCOMPARE(summary.classesUpdated, 1);
            QCOMPARE(summary.classesSkipped, 1);
            QCOMPARE(summary.ignoredCells, 1);
        }
        );

    const auto intensiveRequestForMode = [](const ScheduleImportPlanIntensiveMode mode)
    {
        return [mode](QSqlDatabase& database)
        {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "INSERT INTO teachers (teacher_kr, room_number) VALUES (?, '500')"));
            query.addBindValue(QString::fromUtf16(u"\uBC15"));
            if (!query.exec())
            {
                qFatal("Unable to seed comparison teacher: %s",
                       qPrintable(query.lastError().text()));
            }
            const int teacherId = query.lastInsertId().toInt();
            execOrFail(query, QStringLiteral(
                "INSERT INTO classes (name) VALUES ('E4 Apollo')"));
            const int classId = query.lastInsertId().toInt();
            execOrFail(query, QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level) "
                "VALUES (%1, %2, 'E4', 'Apollo')")
                    .arg(classId)
                    .arg(teacherId));
            execOrFail(query, QStringLiteral(
                "INSERT INTO class_times (class_id, day, start_time, end_time) "
                "VALUES (%1, 'Tuesday', '9:00 AM', '9:55 AM')")
                    .arg(classId));
            execOrFail(query, QStringLiteral(
                "INSERT INTO class_intensive_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Friday', '2:00 PM', '2:55 PM')")
                    .arg(classId));

            auto request = typedCreateRequest({
                typedCandidate(u"\uAE40", u"E4", u"Hercules")
            });
            request.intensiveSchedule = true;
            request.intensiveMode = mode;
            request.selectedUserName = u"Alice";
            request.saveProfileNameIfBlank = true;
            request.diagnostics.push_back({
                u"Sheet", u"Alice", u"B12", u"?", u"Ignored cell"
            });
            request.intensiveSlotStates.push_back({
                u"Tuesday", u"15:00", u"essay"
            });
            return request;
        };
    };

    for (const auto mode : {
             ScheduleImportPlanIntensiveMode::UpdateExisting,
             ScheduleImportPlanIntensiveMode::ReplaceWithNew
             })
    {
        compareApplyPaths(
            mode == ScheduleImportPlanIntensiveMode::UpdateExisting
                ? QStringLiteral("intensive-update")
                : QStringLiteral("intensive-replace"),
            intensiveRequestForMode(mode),
            [](const ScheduleImportSummary& summary)
            {
                QCOMPARE(summary.teachersCreated, 1);
                QCOMPARE(summary.classesCreated, 1);
                QCOMPARE(summary.ignoredCells, 1);
                QVERIFY(summary.profileNameUpdated);
            }
            );
    }
}

void ScheduleImportTests::typedApplyRejectsInvalidEnumsBeforeWrites()
{
    const QString connectionName =
        QStringLiteral("schedule-import-typed-invalid-enum-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        ScheduleImportRepository repository(database);
        const QStringList before = persistedScheduleImportSnapshot(database);

        auto invalidTeacherAction = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        invalidTeacherAction.teachers[0].action =
            static_cast<ClassMngr::Next::Application::
                ScheduleImportReviewTeacherAction>(255);
        QVERIFY(!repository.applyTyped(invalidTeacherAction).has_value());
        QCOMPARE(persistedScheduleImportSnapshot(database), before);

        auto invalidClassAction = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        invalidClassAction.classes[0].action =
            static_cast<ClassMngr::Next::Application::
                ScheduleImportReviewClassAction>(255);
        QVERIFY(!repository.applyTyped(invalidClassAction).has_value());
        QCOMPARE(persistedScheduleImportSnapshot(database), before);

        auto invalidIntensiveMode = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        invalidIntensiveMode.intensiveSchedule = true;
        invalidIntensiveMode.intensiveMode =
            static_cast<ClassMngr::Next::Application::
                ScheduleImportPlanIntensiveMode>(255);
        QVERIFY(!repository.applyTyped(invalidIntensiveMode).has_value());
        QCOMPARE(persistedScheduleImportSnapshot(database), before);

        auto invalidNormalScheduleMode = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        invalidNormalScheduleMode.intensiveMode =
            ClassMngr::Next::Application::ScheduleImportPlanIntensiveMode::Invalid;
        const auto invalidNormalModeResult =
            repository.applyTyped(invalidNormalScheduleMode);
        QVERIFY(!invalidNormalModeResult.has_value());
        QVERIFY(invalidNormalModeResult.error().policyIssue.has_value());
        QCOMPARE(
            invalidNormalModeResult.error().policyIssue->code,
            ClassMngr::Next::Application::
                ScheduleImportPlanEligibilityIssueCode::InvalidIntensiveMode
            );
        QVERIFY2(
            typedApplyFailureMessage(invalidNormalModeResult).contains(
                QStringLiteral("Choose how the existing intensive schedule should be handled.")),
            qPrintable(typedApplyFailureMessage(invalidNormalModeResult))
            );
        QCOMPARE(persistedScheduleImportSnapshot(database), before);

        invalidNormalScheduleMode.diagnosticsAcknowledged = false;
        invalidNormalScheduleMode.diagnostics.push_back({});
        const auto invalidModeAndDiagnosticsResult =
            repository.applyTyped(invalidNormalScheduleMode);
        QVERIFY(!invalidModeAndDiagnosticsResult.has_value());
        QVERIFY(invalidModeAndDiagnosticsResult.error().policyIssue.has_value());
        QCOMPARE(
            invalidModeAndDiagnosticsResult.error().policyIssue->code,
            ClassMngr::Next::Application::
                ScheduleImportPlanEligibilityIssueCode::UnacknowledgedDiagnostics
            );
        QVERIFY2(
            typedApplyFailureMessage(invalidModeAndDiagnosticsResult).contains(
                QStringLiteral("Unrecognized timetable cells must be acknowledged")),
            qPrintable(typedApplyFailureMessage(invalidModeAndDiagnosticsResult))
            );
        QCOMPARE(persistedScheduleImportSnapshot(database), before);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::typedApplyRejectsMalformedResolutionShapesBeforeWrites()
{
    const QString connectionName =
        QStringLiteral("schedule-import-typed-malformed-resolution-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        execOrFail(query, QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, room_number) "
            "VALUES (1, 'Existing teacher', '413')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (1, 'Existing class')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, teacher_id, class_grade, class_level) "
            "VALUES (1, 1, 'E4', 'Hercules')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO class_times "
            "(class_id, day, start_time, end_time) "
            "VALUES (1, 'Friday', '2:00 PM', '2:55 PM')"));
        execOrFail(query, QStringLiteral(
            "INSERT INTO app_settings (key, value) "
            "VALUES ('schedule-import-snapshot', 'preserve-me')"));

        const auto existingTeacherId =
            ClassMngr::Next::Domain::TeacherId::fromString(std::string("1"));
        const auto existingClassId =
            ClassMngr::Next::Domain::ClassId::fromString(std::string("1"));
        QVERIFY(existingTeacherId.has_value());
        QVERIFY(existingClassId.has_value());

        ScheduleImportRepository repository(database);
        const QStringList before = persistedScheduleImportSnapshot(database, true);

        auto outOfRangeCandidate = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        outOfRangeCandidate.classes[0].candidateIndex = 1;
        const auto outOfRangeResult = repository.applyTyped(outOfRangeCandidate);
        QVERIFY(!outOfRangeResult.has_value());
        QVERIFY(outOfRangeResult.error().policyIssue.has_value());
        QCOMPARE(
            outOfRangeResult.error().policyIssue->code,
            ClassMngr::Next::Application::
                ScheduleImportPlanEligibilityIssueCode::InvalidReviewDecision
            );
        QVERIFY2(
            typedApplyFailureMessage(outOfRangeResult).contains(
                QStringLiteral("invalid or duplicate resolution")),
            qPrintable(typedApplyFailureMessage(outOfRangeResult))
            );
        QCOMPARE(persistedScheduleImportSnapshot(database, true), before);

        auto createdTeacherWithTarget = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        createdTeacherWithTarget.teachers[0].targetTeacherId = *existingTeacherId;
        const auto createdTeacherResult =
            repository.applyTyped(createdTeacherWithTarget);
        QVERIFY(!createdTeacherResult.has_value());
        QVERIFY(createdTeacherResult.error().teacherTargetIssue.has_value());
        QCOMPARE(
            createdTeacherResult.error().teacherTargetIssue->code,
            ClassMngr::Next::Application::
                ScheduleImportApplyTeacherTargetIssueCode::NonExistingTeacherHasTarget
            );
        QVERIFY2(
            typedApplyFailureMessage(createdTeacherResult).contains(
                QStringLiteral("cannot use an existing teacher"),
                Qt::CaseInsensitive),
            qPrintable(typedApplyFailureMessage(createdTeacherResult))
            );
        QCOMPARE(persistedScheduleImportSnapshot(database, true), before);

        auto reusedTeacherWithoutTarget = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        reusedTeacherWithoutTarget.teachers[0].action =
            ClassMngr::Next::Application::ScheduleImportReviewTeacherAction::Reuse;
        const auto reusedTeacherResult =
            repository.applyTyped(reusedTeacherWithoutTarget);
        QVERIFY(!reusedTeacherResult.has_value());
        QVERIFY(reusedTeacherResult.error().teacherTargetIssue.has_value());
        QCOMPARE(
            reusedTeacherResult.error().teacherTargetIssue->code,
            ClassMngr::Next::Application::
                ScheduleImportApplyTeacherTargetIssueCode::ExistingTeacherMissingTarget
            );
        QVERIFY2(
            typedApplyFailureMessage(reusedTeacherResult).contains(
                QStringLiteral("Choose an existing Korean teacher")),
            qPrintable(typedApplyFailureMessage(reusedTeacherResult))
            );
        QCOMPARE(persistedScheduleImportSnapshot(database, true), before);

        auto createdClassWithTarget = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        createdClassWithTarget.classes[0].targetClassId = *existingClassId;
        const auto createdClassResult = repository.applyTyped(createdClassWithTarget);
        QVERIFY(!createdClassResult.has_value());
        QVERIFY(createdClassResult.error().policyIssue.has_value());
        QVERIFY2(
            typedApplyFailureMessage(createdClassResult).contains(
                QStringLiteral("newly created class cannot have an existing target"),
                Qt::CaseInsensitive),
            qPrintable(typedApplyFailureMessage(createdClassResult))
            );
        QCOMPARE(persistedScheduleImportSnapshot(database, true), before);

        auto updatedClassWithoutTarget = typedCreateRequest({
            typedCandidate(u"\uAE40", u"E5", u"Zeus")
        });
        updatedClassWithoutTarget.classes[0].action =
            ClassMngr::Next::Application::
                ScheduleImportReviewClassAction::UpdateExisting;
        const auto updatedClassResult =
            repository.applyTyped(updatedClassWithoutTarget);
        QVERIFY(!updatedClassResult.has_value());
        QVERIFY(updatedClassResult.error().policyIssue.has_value());
        QVERIFY2(
            typedApplyFailureMessage(updatedClassResult).contains(
                QStringLiteral("updated class must have a unique existing target"),
                Qt::CaseInsensitive),
            qPrintable(typedApplyFailureMessage(updatedClassResult))
            );
        QCOMPARE(persistedScheduleImportSnapshot(database, true), before);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ScheduleImportTests::seededWriteFailureRollsBackEveryChange()
{
    const QString connectionName =
        QStringLiteral("schedule-import-seeded-rollback-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "INSERT INTO teachers "
                "(teacher_kr, teacher_en, room_number, notes) "
                "VALUES (?, 'Existing English Name', '413', 'Keep teacher notes')"
                )
            );
        query.addBindValue(sentinelTeacherName());
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const int teacherId = query.lastInsertId().toInt();
        QVERIFY(teacherId > 0);

        constexpr int targetClassId = 9201;
        constexpr int unrelatedClassId = 9202;
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO classes (id, name) VALUES "
                "(%1, 'Target class before import'), "
                "(%2, 'Unrelated class remains')"
                )
                .arg(targetClassId)
                .arg(unrelatedClassId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, "
                "reading_book, essay_book, class_color, font_color, notes, "
                "time_filler_activities) VALUES "
                "(%1, %2, 'E5', 'Zeus', 'Keep Target Reading', "
                "'Keep Target Essay', '#123456', '#FFFFFF', "
                "'Keep target notes', 'Keep target filler'), "
                "(%3, %2, 'E5', 'Zeus', 'Keep Other Reading', "
                "'Keep Other Essay', '#654321', '#000000', "
                "'Keep unrelated notes', 'Keep unrelated filler')"
                )
                .arg(targetClassId)
                .arg(teacherId)
                .arg(unrelatedClassId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) VALUES "
                "(%1, 'Tuesday', '4:00 PM', '4:55 PM'), "
                "(%1, 'Thursday', '4:00 PM', '4:55 PM'), "
                "(%2, 'Friday', '11:00 AM', '11:55 AM')"
                )
                .arg(targetClassId)
                .arg(unrelatedClassId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO class_intensive_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (%1, 'Wednesday', '2:00 PM', '2:55 PM')"
                )
                .arg(targetClassId)
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO intensive_slot_states "
                "(day, start_time, state) "
                "VALUES ('Sunday', '3:00 PM', 'lunch')"
                )
            );
        execOrFail(
            query,
            QStringLiteral(
                "INSERT INTO app_settings (key, value) VALUES "
                "('myInfo/name', 'Existing profile'), "
                "('schedule-import-rollback', 'preserve setting')"
                )
            );

        execOrFail(
            query,
            QStringLiteral(
                "CREATE TRIGGER reject_seeded_wednesday_time "
                "BEFORE INSERT ON class_times "
                "WHEN NEW.day = 'Wednesday' "
                "AND EXISTS (SELECT 1 FROM class_times "
                "WHERE class_id=%1 AND day='Monday' "
                "AND start_time='8:00 AM') "
                "AND NOT EXISTS (SELECT 1 FROM class_times "
                "WHERE class_id=%1 AND day IN ('Tuesday', 'Thursday')) "
                "AND NOT EXISTS (SELECT 1 FROM class_times "
                "WHERE class_id=%2 AND day='Friday') "
                "AND EXISTS (SELECT 1 FROM teachers "
                "WHERE id=%3 AND room_number='414') "
                "AND EXISTS (SELECT 1 FROM class_info "
                "WHERE class_id=%1 AND teacher_id=%3 "
                "AND class_level='Apollo' "
                "AND class_color='#AABBCC' AND font_color='#112233') "
                "BEGIN "
                "SELECT RAISE(ABORT, "
                "'F100_INJECTED_WEDNESDAY_INSERT_FAILURE'); "
                "END"
                )
                .arg(targetClassId)
                .arg(unrelatedClassId)
                .arg(teacherId)
            );

        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = sentinelTeacherName();
        candidate.teacherKr = sentinelTeacherName();
        candidate.rooms = {QStringLiteral("414")};
        candidate.classGrade = QStringLiteral("E5");
        candidate.classLevel = QStringLiteral("Apollo");
        candidate.times = {
            {
                QStringLiteral("Monday"),
                QStringLiteral("8:00 AM"),
                QStringLiteral("8:55 AM")
            },
            {
                QStringLiteral("Wednesday"),
                QStringLiteral("9:00 AM"),
                QStringLiteral("9:55 AM")
            }
        };

        ScheduleImportPlan plan;
        plan.kind = ScheduleImportKind::Normal;
        plan.selectedUserName = QStringLiteral("Updated profile");
        plan.updateProfileName = true;
        plan.unknownCellsAcknowledged = true;
        plan.candidates = {candidate};
        plan.teachers = {
            {
                candidate.teacherKey,
                ScheduleImportTeacherAction::UpdateRoom,
                teacherId,
                QStringLiteral("414")
            }
        };
        plan.classes = {
            {
                0,
                ScheduleImportClassAction::UpdateExisting,
                targetClassId,
                QStringLiteral("#AABBCC"),
                QStringLiteral("#112233")
            }
        };

        const QStringList before =
            persistedScheduleImportSnapshot(database, true);
        ScheduleImportRepository repository(database);
        const auto imported = repository.apply(plan);

        QString normalizedFailure = QStringLiteral("success");
        if (!imported.has_value()
            && imported.error().contains(
                QStringLiteral("F100_INJECTED_WEDNESDAY_INSERT_FAILURE")
                ))
        {
            // The fixture raises this unique marker only for the later
            // Wednesday class_times insert, independent of localized context.
            normalizedFailure = QStringLiteral(
                "class_times.wednesday_insert.injected"
                );
        }
        QCOMPARE(
            normalizedFailure,
            QStringLiteral("class_times.wednesday_insert.injected")
            );
        QCOMPARE(
            persistedScheduleImportSnapshot(database, true),
            before
            );

        execOrFail(
            query,
            QStringLiteral(
                "SELECT name FROM classes WHERE id=%1"
                ).arg(unrelatedClassId)
            );
        QVERIFY(query.next());
        QCOMPARE(
            query.value(0).toString(),
            QStringLiteral("Unrelated class remains")
            );
        QVERIFY(!query.next());

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
