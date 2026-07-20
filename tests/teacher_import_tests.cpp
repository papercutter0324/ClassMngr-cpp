#include "data/database/database_schema_manager.h"
#include "data/repositories/teacher_import_repository.h"
#include "features/teacher/import/sectioned_contact_list_template.h"
#include "features/teacher/import/teacher_import_file_validator.h"
#include "features/teacher/import/teacher_import_template_registry.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <QtTest>

#include <zlib.h>

#include <algorithm>

class TeacherImportTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesSectionedTemplate();
    void invalidVersionIsRecognizedButRejected();
    void readsNamedMultiSheetWorkbookMetadata();
    void validatorReportsRecognitionStatusesAndMetadata();
    void registryAcceptsAdditionalTemplateAdapters();
    void unreadableDataFailsValidation();
    void importsIntoSeparateTablesAndPreservesManualFields();
    void validatesExternalSampleWhenProvided();
};

namespace
{
void appendLe16(QByteArray& data, quint16 value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(QByteArray& data, quint32 value)
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

QByteArray storedZip(QList<TestZipEntry> entries)
{
    QByteArray result;
    for (TestZipEntry& entry : entries)
    {
        entry.localOffset = static_cast<quint32>(result.size());
        entry.crc = static_cast<quint32>(crc32(
            crc32(0L, Z_NULL, 0),
            reinterpret_cast<const Bytef*>(entry.contents.constData()),
            static_cast<uInt>(entry.contents.size())));
        appendLe32(result, 0x04034b50);
        appendLe16(result, 20);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe32(result, entry.crc);
        appendLe32(result, static_cast<quint32>(entry.contents.size()));
        appendLe32(result, static_cast<quint32>(entry.contents.size()));
        appendLe16(result, static_cast<quint16>(entry.name.size()));
        appendLe16(result, 0);
        result.append(entry.name);
        result.append(entry.contents);
    }

    const quint32 centralOffset = static_cast<quint32>(result.size());
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
        appendLe32(result, static_cast<quint32>(entry.contents.size()));
        appendLe32(result, static_cast<quint32>(entry.contents.size()));
        appendLe16(result, static_cast<quint16>(entry.name.size()));
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe32(result, 0);
        appendLe32(result, entry.localOffset);
        result.append(entry.name);
    }

    const quint32 centralSize = static_cast<quint32>(result.size()) - centralOffset;
    appendLe32(result, 0x06054b50);
    appendLe16(result, 0);
    appendLe16(result, 0);
    appendLe16(result, static_cast<quint16>(entries.size()));
    appendLe16(result, static_cast<quint16>(entries.size()));
    appendLe32(result, centralSize);
    appendLe32(result, centralOffset);
    appendLe16(result, 0);
    return result;
}

QByteArray testWorkbookData(
    const QString& date = QStringLiteral("26.07.09ver"),
    const QString& marker = QStringLiteral("M1"),
    const QString& name = QStringLiteral("홍길동 E4/6")
    )
{
    const QByteArray workbook = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
                  xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
          <sheets>
            <sheet name="Contacts" sheetId="1" r:id="rId1"/>
            <sheet name="Metadata" sheetId="2" r:id="rId2"/>
          </sheets>
        </workbook>)");
    const QByteArray relationships = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
          <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
          <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>
        </Relationships>)");
    const int dateSplit = date.size() / 2;
    const QByteArray sharedStrings = QStringLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count="2" uniqueCount="2">
          <si><r><t>%1</t></r><r><t>%2</t></r></si>
          <si><r><t>%3</t></r></si>
        </sst>)").arg(date.left(dateSplit), date.mid(dateSplit), marker).toUtf8();
    const QByteArray styles = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <fonts count="2"><font/><font><b/></font></fonts>
          <fills count="3"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FFFFFF00"/></patternFill></fill></fills>
          <cellXfs count="3"><xf fontId="0" fillId="0"/><xf fontId="1" fillId="0"/><xf fontId="0" fillId="2"/></cellXfs>
        </styleSheet>)");
    const QByteArray nameCell = name.isEmpty()
        ? QByteArray()
        : QStringLiteral(
            R"(<c r="B65" t="inlineStr"><is><t>413</t></is></c>
               <c r="C65" t="inlineStr"><is><r><t>%1</t></r></is></c>
               <c r="D65" t="inlineStr"><is><t>010-0000-0000</t></is></c>
               <c r="E65" t="inlineStr"><is><t>02/29</t></is></c>)")
              .arg(name).toUtf8();
    const QByteArray sheet1 = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>
          <row r="1"><c r="A1" t="s"><v>0</v></c></row>
          <row r="3"><c r="A3" t="s"><v>1</v></c></row>
          <row r="65">)")
        + nameCell
        + QByteArrayLiteral(R"(</row></sheetData></worksheet>)");
    const QByteArray sheet2 = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>
          <row r="70"><c r="A70" s="1" t="inlineStr"><is><t>Bold</t></is></c><c r="B70" s="2" t="inlineStr"><is><t>Filled</t></is></c></row>
        </sheetData></worksheet>)");

    return storedZip({
        {QByteArrayLiteral("xl/workbook.xml"), workbook},
        {QByteArrayLiteral("xl/_rels/workbook.xml.rels"), relationships},
        {QByteArrayLiteral("xl/sharedStrings.xml"), sharedStrings},
        {QByteArrayLiteral("xl/styles.xml"), styles},
        {QByteArrayLiteral("xl/worksheets/sheet1.xml"), sheet1},
        {QByteArrayLiteral("xl/worksheets/sheet2.xml"), sheet2}
    });
}

CalendarImport::Workbook sectionedWorkbook()
{
    CalendarImport::Workbook workbook;
    CalendarImport::Style leaderStyle;
    leaderStyle.filled = true;
    workbook.styles = {{}, leaderStyle};

    CalendarImport::Worksheet worksheet;
    worksheet.name = QStringLiteral("Contacts");
    worksheet.cells = {
        {1, 1, 0, QStringLiteral("26.07.09ver"), {}},
        {3, 1, 0, QStringLiteral("M1"), {}},
        {4, 2, 0, QStringLiteral("413"), {}},
        {4, 3, 0, QStringLiteral("홍길동B_E4/6"), {}},
        {4, 4, 0, QStringLiteral("010-0000-0000"), {}},
        {4, 5, 0, QStringLiteral("02/29"), {}},
        {46, 1, 0, QStringLiteral("Ntr"), {}},
        {47, 3, 1, QStringLiteral("Alex"), {}},
        {47, 5, 0, QStringLiteral("03/07"), {}},
        {48, 3, 0, QStringLiteral("Jamie"), {}},
        {48, 5, 0, QStringLiteral("04/08"), {}},
        {49, 3, 1, QStringLiteral("Morgan"), {}},
        {49, 4, 0, QStringLiteral("Instructor"), {}},
        {49, 5, 0, QStringLiteral("04/09"), {}},
        {57, 1, 0, QStringLiteral("CS"), {}},
        {58, 3, 0, QStringLiteral("김하늘M3"), {}},
        {58, 4, 0, QStringLiteral("010-1111-2222"), {}},
        {58, 5, 0, QStringLiteral("05/09"), {}},
        {60, 1, 0, QStringLiteral("GS"), {}},
        {61, 3, 0, QStringLiteral("TaylorC1"), {}},
        {61, 5, 0, QStringLiteral("06/10"), {}}
    };
    workbook.worksheets = {worksheet};
    workbook.cells = worksheet.cells;
    return workbook;
}

class MockImportTemplate final : public ITeacherImportTemplate
{
public:
    QString id() const override { return QStringLiteral("mock-template"); }
    QString displayName() const override { return QStringLiteral("Mock Template"); }
    bool recognizes(const CalendarImport::Workbook&) const override { return true; }
    QStringList discoveredSections(const CalendarImport::Workbook&) const override
    {
        return {QStringLiteral("Mock")};
    }
    Result<TeacherImportPreview> parse(const CalendarImport::Workbook&) const override
    {
        TeacherImportPreview preview;
        preview.templateId = id();
        preview.templateName = displayName();
        preview.sourceDate = QDate(2026, 1, 1);
        preview.nativeEnglishTeachers.append(
            {-1, QStringLiteral("Mock Teacher"), QStringLiteral("NET"),
             QString(), QString(), QString()});
        return preview;
    }
};
}

void TeacherImportTests::parsesSectionedTemplate()
{
    SectionedContactListTemplate importTemplate;
    const CalendarImport::Workbook workbook = sectionedWorkbook();
    QVERIFY(importTemplate.recognizes(workbook));
    const auto preview = importTemplate.parse(workbook);
    if (!preview)
    {
        QFAIL(qPrintable(preview.error()));
    }
    QCOMPARE(preview->sourceDate, QDate(2026, 7, 9));
    QCOMPARE(preview->koreanGroups.size(), 1);
    QCOMPARE(preview->koreanGroups.first().level, QStringLiteral("M1"));
    QCOMPARE(preview->koreanGroups.first().candidates.size(), 1);
    QCOMPARE(preview->koreanGroups.first().candidates.first().teacher.teacherKr,
             QStringLiteral("홍길동B"));
    QVERIFY(preview->koreanGroups.first().candidates.first().selectedByDefault);
    QCOMPARE(preview->koreanGroups.first().candidates.first().teacher.birthday,
             QStringLiteral("02-29"));
    QCOMPARE(preview->nativeEnglishTeachers.size(), 3);
    QCOMPARE(preview->nativeEnglishTeachers.at(0).position, QStringLiteral("Team Leader"));
    QCOMPARE(preview->nativeEnglishTeachers.at(1).position, QStringLiteral("NET"));
    QCOMPARE(preview->nativeEnglishTeachers.at(2).position, QStringLiteral("Instructor"));
    QCOMPARE(preview->gsTeamMembers.size(), 2);
    QCOMPARE(preview->gsTeamMembers.at(0).koreanName, QStringLiteral("김하늘"));
    QCOMPARE(preview->gsTeamMembers.at(0).position, QStringLiteral("Branch Manager"));
    QCOMPARE(preview->gsTeamMembers.at(1).name, QStringLiteral("Taylor"));
    QCOMPARE(preview->gsTeamMembers.at(1).position, QStringLiteral("C1"));
}

void TeacherImportTests::invalidVersionIsRecognizedButRejected()
{
    CalendarImport::Workbook workbook = sectionedWorkbook();
    workbook.worksheets.first().cells[0].value = QStringLiteral("not-a-date");
    workbook.cells = workbook.worksheets.first().cells;
    SectionedContactListTemplate importTemplate;
    QVERIFY(importTemplate.recognizes(workbook));
    const auto preview = importTemplate.parse(workbook);
    QVERIFY(!preview.has_value());
    QVERIFY(preview.error().contains(QStringLiteral("A1")));
}

void TeacherImportTests::readsNamedMultiSheetWorkbookMetadata()
{
    QString error;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(testWorkbookData(), &error);
    QVERIFY2(!workbook.worksheets.isEmpty(), qPrintable(error));
    QCOMPARE(workbook.worksheets.size(), 2);
    QCOMPARE(workbook.worksheets.at(0).name, QStringLiteral("Contacts"));
    QCOMPARE(workbook.worksheets.at(1).name, QStringLiteral("Metadata"));

    const auto findCell = [](const CalendarImport::Worksheet& worksheet, int row, int column) {
        return std::find_if(
            worksheet.cells.cbegin(), worksheet.cells.cend(),
            [row, column](const CalendarImport::Cell& cell) {
                return cell.row == row && cell.column == column;
            });
    };
    const auto name = findCell(workbook.worksheets.at(0), 65, 3);
    QVERIFY(name != workbook.worksheets.at(0).cells.cend());
    QCOMPARE(name->value, QStringLiteral("홍길동 E4/6"));
    const auto bold = findCell(workbook.worksheets.at(1), 70, 1);
    const auto filled = findCell(workbook.worksheets.at(1), 70, 2);
    QVERIFY(bold != workbook.worksheets.at(1).cells.cend());
    QVERIFY(filled != workbook.worksheets.at(1).cells.cend());
    QVERIFY(workbook.styles.at(bold->style).bold);
    QVERIFY(workbook.styles.at(filled->style).filled);
}

void TeacherImportTests::validatorReportsRecognitionStatusesAndMetadata()
{
    const TeacherImportTemplateRegistry registry =
        createDefaultTeacherImportTemplateRegistry();

    const auto valid = validateTeacherImportData(testWorkbookData(), registry);
    QCOMPARE(valid.status, TeacherImportFileStatus::Valid);
    QCOMPARE(valid.templateId, QStringLiteral("sectioned-contact-list-v1"));
    QCOMPARE(valid.sourceDate, QDate(2026, 7, 9));
    QCOMPARE(valid.discoveredSections, QStringList{QStringLiteral("M1")});
    QCOMPARE(valid.previewCounts.koreanTeachers, 1);
    QCOMPARE(valid.previewCounts.nativeEnglishTeachers, 0);
    QCOMPARE(valid.previewCounts.gsTeamMembers, 0);

    const auto unsupported = validateTeacherImportData(
        testWorkbookData(QStringLiteral("26.07.09ver"), QStringLiteral("Other")),
        registry);
    QCOMPARE(unsupported.status, TeacherImportFileStatus::UnsupportedTemplate);
    QVERIFY(!unsupported.diagnostics.isEmpty());

    const auto invalidDate = validateTeacherImportData(
        testWorkbookData(QStringLiteral("invalid-date")), registry);
    QCOMPARE(invalidDate.status, TeacherImportFileStatus::RecognizedButInvalid);
    QCOMPARE(invalidDate.discoveredSections, QStringList{QStringLiteral("M1")});
    QVERIFY(!invalidDate.diagnostics.isEmpty());

    const auto empty = validateTeacherImportData(
        testWorkbookData(QStringLiteral("26.07.09ver"), QStringLiteral("M1"), QString()),
        registry);
    QCOMPARE(empty.status, TeacherImportFileStatus::RecognizedButInvalid);
    QVERIFY(!empty.diagnostics.isEmpty());

    TeacherImportTemplateRegistry ambiguousRegistry =
        createDefaultTeacherImportTemplateRegistry();
    ambiguousRegistry.registerTemplate(std::make_unique<MockImportTemplate>());
    const auto ambiguous = validateTeacherImportData(
        testWorkbookData(), ambiguousRegistry);
    QCOMPARE(ambiguous.status, TeacherImportFileStatus::AmbiguousTemplate);

    TeacherImportTemplateRegistry mockRegistry;
    mockRegistry.registerTemplate(std::make_unique<MockImportTemplate>());
    const auto alternate = validateTeacherImportData(testWorkbookData(), mockRegistry);
    QCOMPARE(alternate.status, TeacherImportFileStatus::Valid);
    QCOMPARE(alternate.previewCounts.nativeEnglishTeachers, 1);
}

void TeacherImportTests::registryAcceptsAdditionalTemplateAdapters()
{
    TeacherImportTemplateRegistry registry;
    registry.registerTemplate(std::make_unique<MockImportTemplate>());
    const auto matches = registry.matchingTemplates(sectionedWorkbook());
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.first()->id(), QStringLiteral("mock-template"));
}

void TeacherImportTests::unreadableDataFailsValidation()
{
    const TeacherImportTemplateRegistry registry =
        createDefaultTeacherImportTemplateRegistry();
    const TeacherImportFileValidation validation =
        validateTeacherImportData(QByteArrayLiteral("not an xlsx"), registry);
    QCOMPARE(validation.status, TeacherImportFileStatus::Unreadable);
    QVERIFY(!validation.diagnostics.isEmpty());
}

void TeacherImportTests::importsIntoSeparateTablesAndPreservesManualFields()
{
    const QString connectionName =
        QStringLiteral("teacher-import-test-%1").arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        DatabaseSchemaManager::ensureSchema(database);

        QSqlQuery seed(database);
        QVERIFY(seed.exec(R"(
            INSERT INTO native_english_teachers
                (name, position, phone_number, birthday, nationality, email)
            VALUES ('Alex', 'NET', '010-9999-9999', '03-07', 'Canadian', 'alex@example.com')
        )"));

        TeacherImportPlan plan;
        plan.templateId = QStringLiteral("sectioned-contact-list-v1");
        plan.sourceDate = QDate(2026, 7, 9);
        Teacher korean;
        korean.teacherKr = QStringLiteral("홍길동");
        korean.roomNumber = QStringLiteral("413");
        plan.koreanTeachers.append(korean);
        plan.nativeEnglishTeachers.append(
            {-1, QStringLiteral("Alex"), QStringLiteral("Team Leader"),
             QString(), QStringLiteral("03-07"), QString()});
        plan.gsTeamMembers.append(
            {-1, QString(), QStringLiteral("김하늘"), QStringLiteral("Branch Manager"),
             QStringLiteral("010-1111-2222"), QStringLiteral("05-09")});

        TeacherImportRepository repository(database);
        const auto imported = repository.importTeachers(plan);
        if (!imported)
        {
            QFAIL(qPrintable(imported.error()));
        }
        QCOMPARE(imported->koreanTeachers.created, 1);
        QCOMPARE(imported->nativeEnglishTeachers.updated, 1);
        QCOMPARE(imported->gsTeamMembers.created, 1);

        QSqlQuery counts(database);
        QVERIFY(counts.exec(QStringLiteral("SELECT COUNT(*) FROM teachers")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 1);
        QVERIFY(counts.exec(QStringLiteral("SELECT COUNT(*) FROM native_english_teachers")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 1);
        QVERIFY(counts.exec(QStringLiteral("SELECT COUNT(*) FROM gs_team")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 1);

        QVERIFY(counts.exec(QStringLiteral(
            "SELECT position, phone_number, nationality, email FROM native_english_teachers")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toString(), QStringLiteral("Team Leader"));
        QCOMPARE(counts.value(1).toString(), QStringLiteral("010-9999-9999"));
        QCOMPARE(counts.value(2).toString(), QStringLiteral("Canadian"));
        QCOMPARE(counts.value(3).toString(), QStringLiteral("alex@example.com"));

        TeacherImportPlan older = plan;
        older.templateId = QStringLiteral("alternate-template-v2");
        older.sourceDate = QDate(2026, 1, 1);
        QVERIFY(repository.importTeachers(older).has_value());
        QSqlQuery dateQuery(database);
        dateQuery.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key=?"));
        dateQuery.addBindValue(QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        QVERIFY(dateQuery.exec());
        QVERIFY(dateQuery.next());
        QCOMPARE(dateQuery.value(0).toString(), QStringLiteral("2026-07-09"));

        TeacherImportPlan duplicatePlan;
        duplicatePlan.templateId = QStringLiteral("duplicate-test");
        duplicatePlan.sourceDate = QDate(2026, 8, 1);
        Teacher duplicateRollbackTeacher;
        duplicateRollbackTeacher.teacherKr = QStringLiteral("롤백교사");
        duplicatePlan.koreanTeachers.append(duplicateRollbackTeacher);
        duplicatePlan.nativeEnglishTeachers.append(
            {-1, QStringLiteral("Duplicate"), QStringLiteral("NET"),
             QString(), QString(), QString()});
        duplicatePlan.nativeEnglishTeachers.append(
            {-1, QStringLiteral(" duplicate "), QStringLiteral("NET"),
             QString(), QString(), QString()});
        QVERIFY(!repository.importTeachers(duplicatePlan).has_value());
        QVERIFY(counts.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr='롤백교사'")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 0);

        QVERIFY(seed.exec(R"(
            INSERT INTO native_english_teachers
                (name, position, phone_number, birthday, nationality)
            VALUES ('Jamie', 'NET', '', '', '')
        )"));
        QVERIFY(seed.exec(R"(
            INSERT INTO native_english_teachers
                (name, position, phone_number, birthday, nationality)
            VALUES (' jamie ', 'NET', '', '', '')
        )"));
        TeacherImportPlan ambiguousPlan;
        ambiguousPlan.templateId = QStringLiteral("ambiguous-test");
        ambiguousPlan.sourceDate = QDate(2026, 8, 2);
        Teacher ambiguousRollbackTeacher;
        ambiguousRollbackTeacher.teacherKr = QStringLiteral("원자성교사");
        ambiguousPlan.koreanTeachers.append(ambiguousRollbackTeacher);
        ambiguousPlan.nativeEnglishTeachers.append(
            {-1, QStringLiteral("JAMIE"), QStringLiteral("Team Leader"),
             QString(), QString(), QString()});
        QVERIFY(!repository.importTeachers(ambiguousPlan).has_value());
        QVERIFY(counts.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr='원자성교사'")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 0);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TeacherImportTests::validatesExternalSampleWhenProvided()
{
    const QString path = qEnvironmentVariable("CLASSMNGR_TEACHER_IMPORT_SAMPLE");
    if (path.isEmpty())
    {
        QSKIP("Set CLASSMNGR_TEACHER_IMPORT_SAMPLE to validate an external workbook.");
    }
    const TeacherImportFileValidation validation = validateTeacherImportFile(path);
    QVERIFY2(validation.isValid(), qPrintable(validation.diagnostics.join('\n')));
    QCOMPARE(validation.templateId, QStringLiteral("sectioned-contact-list-v1"));
    int koreanCount = 0;
    for (const auto& group : validation.preview.koreanGroups)
    {
        koreanCount += group.candidates.size();
    }
    QCOMPARE(koreanCount, 38);
    QCOMPARE(validation.preview.nativeEnglishTeachers.size(), 10);
    QCOMPARE(validation.preview.gsTeamMembers.size(), 9);
}

QTEST_GUILESS_MAIN(TeacherImportTests)

#include "teacher_import_tests.moc"
