#include "data/database/database_schema_manager.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "features/teacher/import/sectioned_contact_list_template.h"
#include "features/teacher/import/teacher_import_file_validator.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "features/teacher/import/teacher_import_template_registry.h"
#include "next/application/import_review_session.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <vector>

class TeacherImportTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesSectionedTemplate();
    void invalidVersionIsRecognizedButRejected();
    void readsNamedMultiSheetWorkbookMetadata();
    void validatorReportsRecognitionStatusesAndMetadata();
    void rejectsGeneratedWorkbookWithInvalidUtf8SharedString();
    void rejectsGeneratedWorkbookWithInvalidDate();
    void appliesGeneratedWorkbookWithExplicitReviewDecision();
    void registryAcceptsAdditionalTemplateAdapters();
    void unreadableDataFailsValidation();
    void reviewContractAcceptsAllSelectedAndNone();
    void reviewContractRejectsInvalidDecisions();
    void matchesStoredKoreanTeacherAfterRemovingSuffix();
    void rejectsAmbiguousKoreanTeacherMatchWithoutWrites();
    void preservesRawNativeAndGsAmbiguityLabels();
    void rollsBackAllTeacherWritesWhenLatestDateSaveFails();
    void importsIntoSeparateTablesAndPreservesManualFields();
    void gsTeamImportMergesSparseMatchedRecordsAndSkipsNoOpUpdates();
    void importsCheckedInWorkbookUsingValidatedReviewChoices();
    void teacherNameKeyAdapterPreservesCodeUnitFiltering();
    void sectionedParserRejectsNameThatFiltersToEmpty();
    void sortsGsTeamPositions();
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
          <fills count="4"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FFFFFF00"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor theme="0"/></patternFill></fill></fills>
          <cellXfs count="4"><xf fontId="0" fillId="0"/><xf fontId="1" fillId="0"/><xf fontId="0" fillId="2"/><xf fontId="0" fillId="3"/></cellXfs>
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
          <row r="70"><c r="A70" s="1" t="inlineStr"><is><t>Bold</t></is></c><c r="B70" s="2" t="inlineStr"><is><t>Filled</t></is></c><c r="C70" s="3" t="inlineStr"><is><t>White</t></is></c></row>
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

QByteArray testWorkbookDataWithInvalidUtf8SharedString()
{
    const QByteArray validWorkbook = testWorkbookData();
    const auto readLe16 = [](const QByteArray& bytes, int offset) {
        const quint16 low = static_cast<quint8>(bytes.at(offset));
        const quint16 high = static_cast<quint8>(bytes.at(offset + 1));
        return static_cast<quint16>(low | (high << 8));
    };
    const auto readLe32 = [&readLe16](const QByteArray& bytes, int offset) {
        const quint32 low = readLe16(bytes, offset);
        const quint32 high = readLe16(bytes, offset + 2);
        return low | (high << 16);
    };

    QList<TestZipEntry> entries;
    int offset = 0;
    while (offset + 4 <= validWorkbook.size()
           && readLe32(validWorkbook, offset) == 0x04034b50)
    {
        const quint16 nameLength = readLe16(validWorkbook, offset + 26);
        const quint16 extraLength = readLe16(validWorkbook, offset + 28);
        const quint32 compressedSize = readLe32(validWorkbook, offset + 18);
        const quint32 uncompressedSize = readLe32(validWorkbook, offset + 22);
        Q_ASSERT(compressedSize == uncompressedSize);

        const int nameOffset = offset + 30;
        const QByteArray name = validWorkbook.mid(nameOffset, nameLength);
        const int contentsOffset = nameOffset + nameLength + extraLength;
        QByteArray contents = validWorkbook.mid(contentsOffset, uncompressedSize);
        if (name == QByteArrayLiteral("xl/sharedStrings.xml"))
        {
            const QByteArray validMarker = QByteArrayLiteral("<t>M1</t>");
            const int markerOffset = contents.indexOf(validMarker);
            Q_ASSERT(markerOffset >= 0);
            Q_ASSERT(contents.indexOf(validMarker, markerOffset + validMarker.size()) == -1);
            contents[markerOffset + 3] = static_cast<char>(0xff);
        }
        entries.append({name, contents});
        offset = contentsOffset + compressedSize;
    }
    Q_ASSERT(entries.size() == 6);
    return storedZip(entries);
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
        {61, 3, 1, QStringLiteral("TaylorM3"), {}},
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

QString teacherImportReviewFixturePath()
{
    const QString sourceDirectory =
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
    return QDir(sourceDirectory).filePath(
        QStringLiteral("fixtures/teacher_import/sectioned_review.xlsx"));
}

ClassMngr::Next::Application::TeacherImportReviewResolution resolveReview(
    const TeacherImportReview& review
    )
{
    using namespace ClassMngr::Next::Application;
    std::vector<TeacherImportCandidateGroup> groups;
    std::vector<TeacherImportGroupDecision> decisions;
    groups.reserve(static_cast<std::size_t>(review.candidateGroups.size()));
    decisions.reserve(static_cast<std::size_t>(review.groupSelections.size()));
    for (const KoreanTeacherImportGroup& group : review.candidateGroups)
    {
        groups.push_back({group.level.toUtf8().toStdString(),
                          static_cast<std::size_t>(group.candidates.size())});
    }
    for (const TeacherImportGroupSelection& selection : review.groupSelections)
    {
        TeacherImportGroupMode mode;
        switch (selection.mode)
        {
        case TeacherImportSelectionMode::All:
            mode = TeacherImportGroupMode::All;
            break;
        case TeacherImportSelectionMode::Selected:
            mode = TeacherImportGroupMode::Selected;
            break;
        case TeacherImportSelectionMode::None:
            mode = TeacherImportGroupMode::None;
            break;
        default:
            mode = static_cast<TeacherImportGroupMode>(-1);
            break;
        }
        TeacherImportGroupDecision decision{
            selection.level.toUtf8().toStdString(), mode, {}};
        for (const int index : selection.selectedCandidateIndexes)
        {
            decision.selectedCandidateIndexes.push_back(index);
        }
        decisions.push_back(std::move(decision));
    }
    return resolveTeacherImportReview(groups, decisions);
}

QList<Teacher> selectedKoreanTeachers(const TeacherImportReview& review)
{
    QList<Teacher> selected;
    const auto resolution = resolveReview(review);
    if (!resolution.accepted())
    {
        return selected;
    }
    for (std::size_t groupIndex = 0;
         groupIndex < resolution.selectedCandidateIndexes.size();
         ++groupIndex)
    {
        const KoreanTeacherImportGroup& group =
            review.candidateGroups.at(static_cast<qsizetype>(groupIndex));
        for (const std::size_t candidateIndex :
             resolution.selectedCandidateIndexes[groupIndex])
        {
            selected.append(group.candidates.at(
                static_cast<qsizetype>(candidateIndex)).teacher);
        }
    }
    return selected;
}
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
             QStringLiteral("홍길동"));
    QVERIFY(preview->koreanGroups.first().candidates.first().selectedByDefault);
    QCOMPARE(preview->koreanGroups.first().candidates.first().teacher.birthday,
             QStringLiteral("02-29"));
    QCOMPARE(preview->nativeEnglishTeachers.size(), 3);
    QCOMPARE(preview->nativeEnglishTeachers.at(0).position, QStringLiteral("Team Leader"));
    QCOMPARE(preview->nativeEnglishTeachers.at(1).position, QStringLiteral("NET"));
    QCOMPARE(preview->nativeEnglishTeachers.at(2).position, QStringLiteral("Instructor"));
    QCOMPARE(preview->gsTeamMembers.size(), 2);
    QCOMPARE(preview->gsTeamMembers.at(0).koreanName, QStringLiteral("김하늘"));
    QCOMPARE(preview->gsTeamMembers.at(0).position, QStringLiteral("M3"));
    QCOMPARE(preview->gsTeamMembers.at(1).name, QStringLiteral("Taylor"));
    QCOMPARE(preview->gsTeamMembers.at(1).position, QStringLiteral("Branch Manager"));
}

void TeacherImportTests::teacherNameKeyAdapterPreservesCodeUnitFiltering()
{
    QString value;
    value.append(QChar(u'A'));
    value.append(QChar(0x3131));
    value.append(QChar(0x1100));
    value.append(QChar(0xd800));
    value.append(QChar(u' '));

    QString expected;
    expected.append(QChar(0x3131));
    expected.append(QChar(0x1100));

    QCOMPARE(TeacherImportNameUtils::hangulOnly(value), expected);
    QVERIFY(TeacherImportNameUtils::hangulOnly(QStringLiteral("English 123"))
                .isEmpty());
    QVERIFY(TeacherImportNameUtils::isHangul(QChar(0x3131)));
    QVERIFY(!TeacherImportNameUtils::isHangul(QChar(0x312f)));

    QString composed;
    composed.append(QChar(0xd55c));
    QString decomposed;
    decomposed.append(QChar(0x1112));
    decomposed.append(QChar(0x1161));
    decomposed.append(QChar(0x11ab));
    QString compatibility;
    compatibility.append(QChar(0x3131));
    QCOMPARE(TeacherImportNameUtils::hangulOnly(composed), composed);
    QCOMPARE(TeacherImportNameUtils::hangulOnly(decomposed), decomposed);
    QVERIFY(
        TeacherImportNameUtils::hangulOnly(composed)
        != TeacherImportNameUtils::hangulOnly(decomposed)
        );
    QCOMPARE(TeacherImportNameUtils::hangulOnly(compatibility), compatibility);
}

void TeacherImportTests::sectionedParserRejectsNameThatFiltersToEmpty()
{
    CalendarImport::Workbook workbook = sectionedWorkbook();
    auto& worksheet = workbook.worksheets.first();
    const auto koreanName = std::find_if(
        worksheet.cells.begin(),
        worksheet.cells.end(),
        [](const CalendarImport::Cell& cell)
        {
            return cell.row == 4 && cell.column == 3;
        }
        );
    QVERIFY(koreanName != worksheet.cells.end());
    koreanName->value = QStringLiteral("English only");

    SectionedContactListTemplate importTemplate;
    const auto preview = importTemplate.parse(workbook);
    QVERIFY(!preview.has_value());
    QCOMPARE(
        preview.error(),
        QStringLiteral("Korean teacher name in row 4 is empty.")
        );
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
    const auto white = findCell(workbook.worksheets.at(1), 70, 3);
    QVERIFY(bold != workbook.worksheets.at(1).cells.cend());
    QVERIFY(filled != workbook.worksheets.at(1).cells.cend());
    QVERIFY(white != workbook.worksheets.at(1).cells.cend());
    QVERIFY(workbook.styles.at(bold->style).bold);
    QVERIFY(workbook.styles.at(filled->style).filled);
    QVERIFY(!workbook.styles.at(white->style).filled);
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

void TeacherImportTests::rejectsGeneratedWorkbookWithInvalidUtf8SharedString()
{
    const QByteArray workbook = testWorkbookDataWithInvalidUtf8SharedString();

    const QByteArray workbookSha256 =
        QCryptographicHash::hash(workbook, QCryptographicHash::Sha256).toHex();
    qInfo().noquote() << "F98_INPUT_SIZE=" << workbook.size();
    qInfo().noquote() << "F98_INPUT_SHA256=" << workbookSha256;
    QCOMPARE(workbook.size(), 3422);
    QCOMPARE(workbookSha256, QByteArrayLiteral(
        "7386d4eae0e7f8d54467b57f05ec909cd0c1e7f392d865f9dc7e52faf3cc8165"));

    const TeacherImportTemplateRegistry registry =
        createDefaultTeacherImportTemplateRegistry();
    const TeacherImportFileValidation validation =
        validateTeacherImportData(workbook, registry);
    QCOMPARE(validation.status, TeacherImportFileStatus::UnsupportedTemplate);
    QVERIFY(validation.templateId.isEmpty());
    QVERIFY(!validation.sourceDate.isValid());
    QVERIFY(validation.discoveredSections.isEmpty());
    QCOMPARE(validation.previewCounts.koreanTeachers, 0);
    QCOMPARE(validation.previewCounts.nativeEnglishTeachers, 0);
    QCOMPARE(validation.previewCounts.gsTeamMembers, 0);
    QVERIFY(validation.preview.templateId.isEmpty());
    QVERIFY(!validation.preview.sourceDate.isValid());
    QVERIFY(validation.preview.koreanGroups.isEmpty());
    QVERIFY(validation.preview.nativeEnglishTeachers.isEmpty());
    QVERIFY(validation.preview.gsTeamMembers.isEmpty());
}

void TeacherImportTests::rejectsGeneratedWorkbookWithInvalidDate()
{
    const QByteArray workbook = testWorkbookData(QStringLiteral("invalid-date"));
    const QByteArray workbookSha256 =
        QCryptographicHash::hash(workbook, QCryptographicHash::Sha256).toHex();
    qInfo().noquote() << "F94_INPUT_SIZE=" << workbook.size();
    qInfo().noquote() << "F94_INPUT_SHA256=" << workbookSha256;
    QCOMPARE(workbook.size(), 3423);
    QCOMPARE(workbookSha256, QByteArrayLiteral(
        "256b29c2f27bfe787007aaa6df28e5e084dc3788863cbf4b6a09fb265f0685d0"));

    const TeacherImportTemplateRegistry registry =
        createDefaultTeacherImportTemplateRegistry();
    const TeacherImportFileValidation validation =
        validateTeacherImportData(workbook, registry);
    QCOMPARE(validation.status, TeacherImportFileStatus::RecognizedButInvalid);
    QCOMPARE(validation.templateId, QStringLiteral("sectioned-contact-list-v1"));
    QCOMPARE(validation.sourceDate, QDate());
    QCOMPARE(validation.discoveredSections, QStringList{QStringLiteral("M1")});
    QCOMPARE(validation.diagnostics, QStringList{
        QStringLiteral("Cell A1 must contain a version date such as 26.07.09ver.")});
}

void TeacherImportTests::appliesGeneratedWorkbookWithExplicitReviewDecision()
{
    const QByteArray workbook = testWorkbookData();
    const QByteArray workbookSha256 =
        QCryptographicHash::hash(workbook, QCryptographicHash::Sha256).toHex();
    QCOMPARE(workbook.size(), 3422);
    QCOMPARE(workbookSha256, QByteArrayLiteral(
        "9cdccb43d7fe5e5e1abb83630ede8b18e6dd2c4824dbb288dc81d60371496daa"));
    qInfo().noquote() << "F92_INPUT_SIZE=" << workbook.size();
    qInfo().noquote() << "F92_INPUT_SHA256=" << workbookSha256;

    const TeacherImportTemplateRegistry registry =
        createDefaultTeacherImportTemplateRegistry();
    const TeacherImportFileValidation validation =
        validateTeacherImportData(workbook, registry);
    QCOMPARE(validation.status, TeacherImportFileStatus::Valid);
    QCOMPARE(validation.templateId, QStringLiteral("sectioned-contact-list-v1"));
    QCOMPARE(validation.sourceDate, QDate(2026, 7, 9));
    QCOMPARE(validation.discoveredSections, QStringList{QStringLiteral("M1")});
    QCOMPARE(validation.preview.koreanGroups.size(), 1);
    QCOMPARE(validation.preview.koreanGroups.first().level, QStringLiteral("M1"));
    QCOMPARE(validation.preview.koreanGroups.first().candidates.size(), 1);

    const KoreanTeacherImportCandidate candidate =
        validation.preview.koreanGroups.first().candidates.first();
    QCOMPARE(candidate.teacher.teacherKr, QStringLiteral("홍길동"));
    QCOMPARE(candidate.teacher.roomNumber, QStringLiteral("413"));
    QCOMPARE(candidate.teacher.birthday, QStringLiteral("02-29"));
    QCOMPARE(candidate.teacher.phoneNumber, QStringLiteral("010-0000-0000"));
    QVERIFY(candidate.selectedByDefault);

    TeacherImportPlan plan;
    plan.templateId = validation.templateId;
    plan.sourceDate = validation.sourceDate;
    plan.nativeEnglishTeachers = validation.preview.nativeEnglishTeachers;
    plan.gsTeamMembers = validation.preview.gsTeamMembers;
    plan.review.emplace();
    plan.review->candidateGroups = validation.preview.koreanGroups;
    plan.review->groupSelections.append({
        QStringLiteral("M1"), TeacherImportSelectionMode::Selected, {0}});
    plan.koreanTeachers.append(candidate.teacher);

    const QString connectionName =
        QStringLiteral("teacher-import-workbook-apply-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        const auto seedKoreanTeacher = [&database](
                                               const int id,
                                               const QStringList& fields) {
            QSqlQuery seed(database);
            seed.prepare(QStringLiteral(
                "INSERT INTO teachers "
                "(id, teacher_kr, teacher_en, preferred_romanization, "
                "preferred_name, room_number, birthday, phone_number, "
                "wifi_name, wifi_password, internet_type, zoom_id, "
                "zoom_password, projection_type, notes) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
            seed.addBindValue(id);
            for (const QString& field : fields)
            {
                seed.addBindValue(field);
            }
            const bool inserted = seed.exec();
            if (!inserted)
            {
                qWarning().noquote() << "F92 seed error:" << seed.lastError().text();
            }
            return inserted;
        };
        QVERIFY(seedKoreanTeacher(7201, {
            QStringLiteral("홍길동D"), QStringLiteral("Manual English"),
            QStringLiteral("Manual Romanization"), QStringLiteral("Manual preferred"),
            QStringLiteral("Old room"), QStringLiteral("01-01"),
            QStringLiteral("010-1111-1111"), QStringLiteral("Manual WiFi"),
            QStringLiteral("Manual WiFi Password"), QStringLiteral("LAN"),
            QStringLiteral("manual.zoom"), QStringLiteral("manual.zoom.password"),
            QStringLiteral("Any"), QStringLiteral("Manual notes")}));
        QVERIFY(seedKoreanTeacher(7202, {
            QStringLiteral("박민준"), QStringLiteral("Other English"),
            QStringLiteral("Other Romanization"), QStringLiteral("Other preferred"),
            QStringLiteral("510"), QStringLiteral("05-09"),
            QStringLiteral("010-5555-5555"), QStringLiteral("Other WiFi"),
            QStringLiteral("Other WiFi Password"), QStringLiteral("WiFi"),
            QStringLiteral("other.zoom"), QStringLiteral("other.zoom.password"),
            QStringLiteral("Any"), QStringLiteral("Other notes")}));

        QSqlQuery seed(database);
        seed.prepare(QStringLiteral(
            "INSERT INTO native_english_teachers "
            "(id, name, position, phone_number, birthday, nationality, email) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)"));
        seed.addBindValue(8104);
        seed.addBindValue(QStringLiteral("Alex"));
        seed.addBindValue(QStringLiteral("NET"));
        seed.addBindValue(QStringLiteral("010-9999-8888"));
        seed.addBindValue(QStringLiteral("02-01"));
        seed.addBindValue(QStringLiteral("Canadian"));
        seed.addBindValue(QStringLiteral("alex@example.com"));
        QVERIFY(seed.exec());
        seed.prepare(QStringLiteral(
            "INSERT INTO gs_team "
            "(id, name, korean_name, position, phone_number, birthday) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        seed.addBindValue(8201);
        seed.addBindValue(QStringLiteral("Taylor"));
        seed.addBindValue(QStringLiteral("김하늘"));
        seed.addBindValue(QStringLiteral("Branch Manager"));
        seed.addBindValue(QStringLiteral("010-6666-5555"));
        seed.addBindValue(QStringLiteral("06-10"));
        QVERIFY(seed.exec());

        seed.prepare(QStringLiteral(
            "INSERT INTO app_settings (key, value) VALUES (?, ?)"));
        seed.addBindValue(QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        seed.addBindValue(QStringLiteral("2026-07-01"));
        QVERIFY(seed.exec());
        seed.prepare(QStringLiteral(
            "INSERT INTO app_settings (key, value) VALUES (?, ?)"));
        seed.addBindValue(QStringLiteral("unrelated/theme"));
        seed.addBindValue(QStringLiteral("dark"));
        QVERIFY(seed.exec());

        TeacherImportRepository repository(database);
        const auto imported = repository.importTeachers(plan);
        QVERIFY2(imported.has_value(),
                 imported.has_value() ? "" : qPrintable(imported.error()));
        QCOMPARE(imported->koreanTeachers.created, 0);
        QCOMPARE(imported->koreanTeachers.updated, 1);
        QCOMPARE(imported->koreanTeachers.unchanged, 0);
        QCOMPARE(imported->nativeEnglishTeachers.created, 0);
        QCOMPARE(imported->nativeEnglishTeachers.updated, 0);
        QCOMPARE(imported->nativeEnglishTeachers.unchanged, 0);
        QCOMPARE(imported->gsTeamMembers.created, 0);
        QCOMPARE(imported->gsTeamMembers.updated, 0);
        QCOMPARE(imported->gsTeamMembers.unchanged, 0);

        QList<QStringList> persistedTeachers;
        QSqlQuery persisted(database);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT teacher_kr, teacher_en, preferred_romanization, "
            "preferred_name, room_number, birthday, phone_number, wifi_name, "
            "wifi_password, internet_type, zoom_id, zoom_password, "
            "projection_type, notes FROM teachers "
            "ORDER BY teacher_kr COLLATE BINARY")));
        while (persisted.next())
        {
            QStringList fields;
            for (int column = 0; column < 14; ++column)
            {
                fields.append(persisted.value(column).toString());
            }
            persistedTeachers.append(fields);
        }
        QCOMPARE(persistedTeachers.size(), 2);
        QCOMPARE(persistedTeachers.at(0), QStringList({
            QStringLiteral("박민준"), QStringLiteral("Other English"),
            QStringLiteral("Other Romanization"), QStringLiteral("Other preferred"),
            QStringLiteral("510"), QStringLiteral("05-09"),
            QStringLiteral("010-5555-5555"), QStringLiteral("Other WiFi"),
            QStringLiteral("Other WiFi Password"), QStringLiteral("WiFi"),
            QStringLiteral("other.zoom"), QStringLiteral("other.zoom.password"),
            QStringLiteral("Any"), QStringLiteral("Other notes")}));
        QCOMPARE(persistedTeachers.at(1), QStringList({
            QStringLiteral("홍길동"), QStringLiteral("Manual English"),
            QStringLiteral("Manual Romanization"), QStringLiteral("Manual preferred"),
            QStringLiteral("413"), QStringLiteral("02-29"),
            QStringLiteral("010-0000-0000"), QStringLiteral("Manual WiFi"),
            QStringLiteral("Manual WiFi Password"), QStringLiteral("LAN"),
            QStringLiteral("manual.zoom"), QStringLiteral("manual.zoom.password"),
            QStringLiteral("Any"), QStringLiteral("Manual notes")}));

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT name, position, phone_number, birthday, nationality, email "
            "FROM native_english_teachers ORDER BY name COLLATE BINARY")));
        QVERIFY(persisted.next());
        const QStringList persistedNative{
            persisted.value(0).toString(), persisted.value(1).toString(),
            persisted.value(2).toString(), persisted.value(3).toString(),
            persisted.value(4).toString(), persisted.value(5).toString()};
        QCOMPARE(persistedNative, QStringList({
            QStringLiteral("Alex"), QStringLiteral("NET"),
            QStringLiteral("010-9999-8888"), QStringLiteral("02-01"),
            QStringLiteral("Canadian"), QStringLiteral("alex@example.com")}));
        QVERIFY(!persisted.next());

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT name, korean_name, position, phone_number, birthday "
            "FROM gs_team ORDER BY name COLLATE BINARY")));
        QVERIFY(persisted.next());
        const QStringList persistedGs{
            persisted.value(0).toString(), persisted.value(1).toString(),
            persisted.value(2).toString(), persisted.value(3).toString(),
            persisted.value(4).toString()};
        QCOMPARE(persistedGs, QStringList({
            QStringLiteral("Taylor"), QStringLiteral("김하늘"),
            QStringLiteral("Branch Manager"), QStringLiteral("010-6666-5555"),
            QStringLiteral("06-10")}));
        QVERIFY(!persisted.next());

        QVERIFY(persisted.prepare(QStringLiteral(
            "SELECT value FROM app_settings WHERE key=?")));
        persisted.bindValue(0, QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        QVERIFY(persisted.exec());
        QVERIFY(persisted.next());
        const QString latestSourceDate = persisted.value(0).toString();
        QCOMPARE(latestSourceDate, QStringLiteral("2026-07-09"));
        QVERIFY(!persisted.next());
        QVERIFY(persisted.prepare(QStringLiteral(
            "SELECT value FROM app_settings WHERE key=?")));
        persisted.addBindValue(QStringLiteral("unrelated/theme"));
        QVERIFY(persisted.exec());
        QVERIFY(persisted.next());
        const QString unrelatedSetting = persisted.value(0).toString();
        QCOMPARE(unrelatedSetting, QStringLiteral("dark"));
        QVERIFY(!persisted.next());

        QJsonArray candidateTranscript;
        candidateTranscript.append(candidate.teacher.teacherKr);
        candidateTranscript.append(candidate.teacher.roomNumber);
        candidateTranscript.append(candidate.teacher.birthday);
        candidateTranscript.append(candidate.teacher.phoneNumber);
        QJsonArray teacherTranscript;
        for (const QStringList& fields : persistedTeachers)
        {
            QJsonArray row;
            for (const QString& field : fields)
            {
                row.append(field);
            }
            teacherTranscript.append(row);
        }
        QJsonObject transcript{
            {QStringLiteral("inputSha256"), QString::fromLatin1(workbookSha256)},
            {QStringLiteral("inputSize"), workbook.size()},
            {QStringLiteral("templateId"), validation.templateId},
            {QStringLiteral("sourceDate"), validation.sourceDate.toString(Qt::ISODate)},
            {QStringLiteral("decision"), QStringLiteral("M1:selected:0")},
            {QStringLiteral("candidate"), candidateTranscript},
            {QStringLiteral("counts"), QJsonObject{
                {QStringLiteral("korean"), QJsonArray{0, 1, 0}},
                {QStringLiteral("nativeEnglish"), QJsonArray{0, 0, 0}},
                {QStringLiteral("gsTeam"), QJsonArray{0, 0, 0}}}},
            {QStringLiteral("teachers"), teacherTranscript},
            {QStringLiteral("nativeEnglishTeachers"), QJsonArray{
                QStringLiteral("Alex"), QStringLiteral("NET"),
                QStringLiteral("010-9999-8888"), QStringLiteral("02-01"),
                QStringLiteral("Canadian"), QStringLiteral("alex@example.com")}},
            {QStringLiteral("gsTeamMembers"), QJsonArray{
                QStringLiteral("Taylor"), QStringLiteral("김하늘"),
                QStringLiteral("Branch Manager"), QStringLiteral("010-6666-5555"),
                QStringLiteral("06-10")}},
            {QStringLiteral("latestSourceDate"), latestSourceDate},
            {QStringLiteral("unrelatedSetting"), unrelatedSetting}};
        const QByteArray semanticTranscript =
            QJsonDocument(transcript).toJson(QJsonDocument::Compact);
        const QByteArray semanticSha256 = QCryptographicHash::hash(
            semanticTranscript, QCryptographicHash::Sha256).toHex();
        qInfo().noquote() << "F92_SEMANTIC_TRANSCRIPT="
                          << QString::fromUtf8(semanticTranscript);
        qInfo().noquote() << "F92_SEMANTIC_SHA256=" << semanticSha256;

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
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

void TeacherImportTests::reviewContractAcceptsAllSelectedAndNone()
{
    using namespace ClassMngr::Next::Application;
    const std::vector<TeacherImportCandidateGroup> groups{
        {"M1", 3}, {"M2", 2}, {"H1", 1}};
    const std::vector<TeacherImportGroupDecision> decisions{
        {"H1", TeacherImportGroupMode::All, {}},
        {"M1", TeacherImportGroupMode::Selected, {2, 0}},
        {"M2", TeacherImportGroupMode::None, {}}
    };

    const auto resolution = resolveTeacherImportReview(groups, decisions);
    QVERIFY(resolution.accepted());
    QCOMPARE(resolution.selectedCandidateIndexes.size(), std::size_t(3));
    QCOMPARE(resolution.selectedCandidateIndexes[0].size(), std::size_t(2));
    QCOMPARE(resolution.selectedCandidateIndexes[0][0], std::size_t(0));
    QCOMPARE(resolution.selectedCandidateIndexes[0][1], std::size_t(2));
    QVERIFY(resolution.selectedCandidateIndexes[1].empty());
    QCOMPARE(resolution.selectedCandidateIndexes[2].size(), std::size_t(1));
    QCOMPARE(resolution.selectedCandidateIndexes[2][0], std::size_t(0));
}

void TeacherImportTests::reviewContractRejectsInvalidDecisions()
{
    using namespace ClassMngr::Next::Application;
    const std::vector<TeacherImportCandidateGroup> groups{{"M1", 2}, {"H1", 1}};
    const auto issueFor = [&groups](
                              const std::vector<TeacherImportGroupDecision>& decisions,
                              const std::vector<TeacherImportCandidateGroup>& candidates = {}) {
        const auto resolution = resolveTeacherImportReview(
            candidates.empty() ? groups : candidates, decisions);
        return resolution.issue;
    };

    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::All, {}},
                 {"H1", TeacherImportGroupMode::All, {}}},
                 {{"M1", 2}, {"M1", 1}})),
             static_cast<int>(TeacherImportReviewIssue::DuplicateCandidateGroup));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::All, {}},
                 {"H1", TeacherImportGroupMode::All, {}}},
                 {{"", 2}, {"H1", 1}})),
             static_cast<int>(TeacherImportReviewIssue::EmptyGroupId));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::All, {}},
                 {"M1", TeacherImportGroupMode::None, {}}})),
             static_cast<int>(TeacherImportReviewIssue::DuplicateDecisionGroup));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::All, {}},
                 {"Unknown", TeacherImportGroupMode::None, {}}})),
             static_cast<int>(TeacherImportReviewIssue::UnknownDecisionGroup));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::All, {}}})),
             static_cast<int>(TeacherImportReviewIssue::MissingDecisionGroup));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", static_cast<TeacherImportGroupMode>(99), {}},
                 {"H1", TeacherImportGroupMode::All, {}}})),
             static_cast<int>(TeacherImportReviewIssue::InvalidMode));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::All, {0}},
                 {"H1", TeacherImportGroupMode::All, {}}})),
             static_cast<int>(TeacherImportReviewIssue::UnexpectedCandidateIndexes));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::Selected, {1, 1}},
                 {"H1", TeacherImportGroupMode::All, {}}})),
             static_cast<int>(TeacherImportReviewIssue::DuplicateCandidateIndex));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::Selected, {-1}},
                 {"H1", TeacherImportGroupMode::All, {}}})),
             static_cast<int>(TeacherImportReviewIssue::CandidateIndexOutOfRange));
    QCOMPARE(static_cast<int>(issueFor({
                 {"M1", TeacherImportGroupMode::Selected, {2}},
                 {"H1", TeacherImportGroupMode::All, {}}})),
             static_cast<int>(TeacherImportReviewIssue::CandidateIndexOutOfRange));
}

void TeacherImportTests::matchesStoredKoreanTeacherAfterRemovingSuffix()
{
    const QString connectionName =
        QStringLiteral("teacher-import-suffix-test-%1").arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO teachers (teacher_kr, room_number) VALUES ('홍길동D', 'Old Room')")));

        TeacherImportPlan plan;
        plan.templateId = QStringLiteral("sectioned-contact-list-v1");
        plan.sourceDate = QDate(2026, 7, 9);
        Teacher korean;
        korean.teacherKr = QStringLiteral("홍길동");
        korean.roomNumber = QStringLiteral("413");
        plan.koreanTeachers.append(korean);

        TeacherImportRepository repository(database);
        const auto imported = repository.importTeachers(plan);
        if (!imported)
        {
            QFAIL(qPrintable(imported.error()));
        }
        QCOMPARE(imported->koreanTeachers.created, 0);
        QCOMPARE(imported->koreanTeachers.updated, 1);

        QVERIFY(query.exec(QStringLiteral(
            "SELECT teacher_kr, room_number FROM teachers")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("홍길동"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("413"));
        QVERIFY(!query.next());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TeacherImportTests::rejectsAmbiguousKoreanTeacherMatchWithoutWrites()
{
    const QString connectionName =
        QStringLiteral("teacher-import-korean-ambiguous-test-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        const QString firstStoredName = QString::fromUtf8(
            "\xEA\xB9\x80\xED\x95\x98\xEB\x8A\x98 A");
        const QString secondStoredName = QString::fromUtf8(
            "\xEA\xB9\x80\xED\x95\x98\xEB\x8A\x98 B");
        const QString ambiguousName = QString::fromUtf8(
            "\xEA\xB9\x80\xED\x95\x98\xEB\x8A\x98");
        const QString precedingNewName = QString::fromUtf8(
            "\xEC\x9D\xB4\xEC\x84\x9C\xEC\x97\xB0");

        QSqlQuery seed(database);
        seed.prepare(QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, room_number) VALUES (?, ?, ?)"));
        seed.addBindValue(9201);
        seed.addBindValue(firstStoredName);
        seed.addBindValue(QStringLiteral("Room A"));
        QVERIFY(seed.exec());
        seed.prepare(QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, room_number) VALUES (?, ?, ?)"));
        seed.addBindValue(9202);
        seed.addBindValue(secondStoredName);
        seed.addBindValue(QStringLiteral("Room B"));
        QVERIFY(seed.exec());

        TeacherImportPlan plan;
        plan.templateId = QStringLiteral("korean-ambiguous-match-test");
        plan.sourceDate = QDate(2026, 9, 27);
        Teacher precedingTeacher;
        precedingTeacher.teacherKr = precedingNewName;
        plan.koreanTeachers.append(precedingTeacher);
        Teacher ambiguousTeacher;
        ambiguousTeacher.teacherKr = ambiguousName;
        ambiguousTeacher.roomNumber = QStringLiteral("Should not persist");
        plan.koreanTeachers.append(ambiguousTeacher);

        TeacherImportRepository repository(database);
        const auto imported = repository.importTeachers(plan);
        QVERIFY(!imported.has_value());
        QCOMPARE(
            imported.error(),
            QStringLiteral("More than one stored Korean teacher matches %1.")
                .arg(ambiguousName));

        QSqlQuery persisted(database);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT id, teacher_kr, room_number FROM teachers ORDER BY id")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 9201);
        QCOMPARE(persisted.value(1).toString(), firstStoredName);
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("Room A"));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 9202);
        QCOMPARE(persisted.value(1).toString(), secondStoredName);
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("Room B"));
        QVERIFY(!persisted.next());

        QSqlQuery newTeacherQuery(database);
        newTeacherQuery.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr=?"));
        newTeacherQuery.addBindValue(precedingNewName);
        QVERIFY(newTeacherQuery.exec());
        QVERIFY(newTeacherQuery.next());
        QCOMPARE(newTeacherQuery.value(0).toInt(), 0);

        QSqlQuery dateQuery(database);
        dateQuery.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM app_settings WHERE key=?"));
        dateQuery.addBindValue(QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        QVERIFY(dateQuery.exec());
        QVERIFY(dateQuery.next());
        QCOMPARE(dateQuery.value(0).toInt(), 0);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TeacherImportTests::preservesRawNativeAndGsAmbiguityLabels()
{
    const QString connectionName =
        QStringLiteral("teacher-import-raw-ambiguity-label-test-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery seed(database);
        seed.prepare(QStringLiteral(
            "INSERT INTO native_english_teachers (name) VALUES (?)"));
        seed.addBindValue(QStringLiteral("Jamie"));
        QVERIFY(seed.exec());
        seed.prepare(QStringLiteral(
            "INSERT INTO native_english_teachers (name) VALUES (?)"));
        seed.addBindValue(QStringLiteral(" jamie "));
        QVERIFY(seed.exec());

        const QString koreanName = QString::fromUtf8(
            "\xEA\xB9\x80\xED\x95\x98\xEB\x8A\x98");
        seed.prepare(QStringLiteral(
            "INSERT INTO gs_team (name, korean_name) VALUES (?, ?)"));
        seed.addBindValue(QStringLiteral("Taylor"));
        seed.addBindValue(koreanName);
        QVERIFY(seed.exec());
        seed.prepare(QStringLiteral(
            "INSERT INTO gs_team (name, korean_name) VALUES (?, ?)"));
        seed.addBindValue(QStringLiteral("Casey"));
        seed.addBindValue(koreanName);
        QVERIFY(seed.exec());

        TeacherImportRepository repository(database);
        TeacherImportPlan nativePlan;
        nativePlan.templateId = QStringLiteral("raw-native-ambiguity-label");
        nativePlan.sourceDate = QDate(2026, 9, 27);
        const QString paddedNativeName = QStringLiteral("  JAMIE  ");
        nativePlan.nativeEnglishTeachers.append({
            -1, paddedNativeName, QStringLiteral("NET"), QString(),
            QString(), QString(), QString()
        });
        const auto nativeResult = repository.importTeachers(nativePlan);
        QVERIFY(!nativeResult.has_value());
        QCOMPARE(
            nativeResult.error(),
            QStringLiteral(
                "More than one stored Native English Teacher matches   JAMIE  ."));

        TeacherImportPlan gsPlan;
        gsPlan.templateId = QStringLiteral("raw-gs-ambiguity-label");
        gsPlan.sourceDate = QDate(2026, 9, 27);
        const QString paddedKoreanName =
            QStringLiteral("  ") + koreanName + QStringLiteral("  ");
        gsPlan.gsTeamMembers.append({
            -1, QStringLiteral("Any Name"), paddedKoreanName,
            QString(), QString(), QString()
        });
        const auto gsResult = repository.importTeachers(gsPlan);
        QVERIFY(!gsResult.has_value());
        QCOMPARE(
            gsResult.error(),
            QStringLiteral("More than one stored GS Team member matches   ")
                + koreanName + QStringLiteral("  ."));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TeacherImportTests::rollsBackAllTeacherWritesWhenLatestDateSaveFails()
{
    const QString connectionName =
        QStringLiteral("teacher-import-date-rollback-test-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery trigger(database);
        QVERIFY(trigger.exec(R"(
            CREATE TRIGGER fail_teacher_import_date_insert
            BEFORE INSERT ON app_settings
            WHEN NEW.key='teacher_import/latest_source_date'
            BEGIN
                SELECT RAISE(ABORT, 'injected latest-date write failure');
            END
        )"));

        TeacherImportPlan plan;
        plan.templateId = QStringLiteral("teacher-import-atomic-date-failure");
        plan.sourceDate = QDate(2026, 9, 27);
        Teacher korean;
        korean.teacherKr = QString::fromUtf8(
            "\xEA\xB9\x80\xED\x95\x98\xEB\x8A\x98");
        plan.koreanTeachers.append(korean);
        plan.nativeEnglishTeachers.append({
            -1, QStringLiteral("Alex"), QStringLiteral("NET"),
            QStringLiteral("010-1111-2222"), QString(), QString(), QString()
        });
        plan.gsTeamMembers.append({
            -1, QStringLiteral("Taylor"), QString(), QStringLiteral("M2"),
            QString(), QString()
        });

        TeacherImportRepository repository(database);
        const auto imported = repository.importTeachers(plan);
        QVERIFY(!imported.has_value());
        QVERIFY(imported.error().startsWith(
            QStringLiteral("Saving the teacher import date failed")));

        QSqlQuery persisted(database);
        QVERIFY(persisted.exec(QStringLiteral("SELECT COUNT(*) FROM teachers")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM native_english_teachers")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);
        QVERIFY(persisted.exec(QStringLiteral("SELECT COUNT(*) FROM gs_team")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM app_settings "
            "WHERE key='teacher_import/latest_source_date'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TeacherImportTests::importsIntoSeparateTablesAndPreservesManualFields()
{
    const QString connectionName =
        QStringLiteral("teacher-import-test-%1").arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery seed(database);
        QVERIFY(seed.exec(R"(
            INSERT INTO native_english_teachers
                (name, position, phone_number, birthday, nationality, email)
            VALUES ('Alex', 'NET', '010-9999-9999', '03-07', 'Canadian', 'alex@example.com')
        )"));

        // This source-generated plan is synthetic evidence, not workbook or
        // historical-production-workbook parity evidence.
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
        QCOMPARE(imported->koreanTeachers.updated, 0);
        QCOMPARE(imported->koreanTeachers.unchanged, 0);
        QCOMPARE(imported->nativeEnglishTeachers.created, 0);
        QCOMPARE(imported->nativeEnglishTeachers.updated, 1);
        QCOMPARE(imported->nativeEnglishTeachers.unchanged, 0);
        QCOMPARE(imported->gsTeamMembers.created, 1);
        QCOMPARE(imported->gsTeamMembers.updated, 0);
        QCOMPARE(imported->gsTeamMembers.unchanged, 0);

        QSqlQuery persisted(database);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT teacher_kr, teacher_en, preferred_romanization, preferred_name, "
            "room_number, birthday, phone_number, wifi_name, wifi_password, "
            "internet_type, zoom_id, zoom_password, projection_type, notes "
            "FROM teachers")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QStringLiteral("홍길동"));
        QCOMPARE(persisted.value(1).toString(), QString());
        QCOMPARE(persisted.value(2).toString(), QString());
        QCOMPARE(persisted.value(3).toString(), QString());
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("413"));
        QCOMPARE(persisted.value(5).toString(), QString());
        QCOMPARE(persisted.value(6).toString(), QString());
        QCOMPARE(persisted.value(7).toString(), QString());
        QCOMPARE(persisted.value(8).toString(), QString());
        QCOMPARE(persisted.value(9).toString(), QStringLiteral("WiFi"));
        QCOMPARE(persisted.value(10).toString(), QString());
        QCOMPARE(persisted.value(11).toString(), QString());
        QCOMPARE(persisted.value(12).toString(), QStringLiteral("HDMI"));
        QCOMPARE(persisted.value(13).toString(), QString());
        QVERIFY(!persisted.next());

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT name, position, phone_number, birthday, nationality, email "
            "FROM native_english_teachers")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QStringLiteral("Alex"));
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("Team Leader"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("010-9999-9999"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("03-07"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("Canadian"));
        QCOMPARE(persisted.value(5).toString(), QStringLiteral("alex@example.com"));
        QVERIFY(!persisted.next());

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT name, korean_name, position, phone_number, birthday FROM gs_team")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QString());
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("김하늘"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("Branch Manager"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("010-1111-2222"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("05-09"));
        QVERIFY(!persisted.next());

        QSqlQuery counts(database);
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
        const QString duplicateKoreanKey = QString::fromUtf8("\xED\x95\x9C");
        Teacher firstWithKoreanKey;
        firstWithKoreanKey.teacherKr = QStringLiteral("First ") + duplicateKoreanKey;
        Teacher secondWithKoreanKey;
        secondWithKoreanKey.teacherKr = duplicateKoreanKey + QStringLiteral(" Second");
        duplicatePlan.koreanTeachers.append(firstWithKoreanKey);
        duplicatePlan.koreanTeachers.append(secondWithKoreanKey);
        duplicatePlan.nativeEnglishTeachers.append(
            {-1, QStringLiteral("Duplicate"), QStringLiteral("NET"),
             QString(), QString(), QString()});
        duplicatePlan.nativeEnglishTeachers.append(
            {-1, QStringLiteral(" duplicate "), QStringLiteral("NET"),
             QString(), QString(), QString()});
        const auto duplicateImport = repository.importTeachers(duplicatePlan);
        QVERIFY(!duplicateImport.has_value());
        QCOMPARE(
            duplicateImport.error(),
            QStringLiteral("The import contains a duplicate Korean teacher name.")
            );
        QVERIFY(counts.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr='롤백교사'")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 0);
        QVERIFY(counts.exec(QStringLiteral("SELECT COUNT(*) FROM teachers")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 1);

        const auto expectPreWriteError = [&repository](
                                              const TeacherImportPlan& candidate,
                                              const QString& expectedError) {
            const auto rejected = repository.importTeachers(candidate);
            if (rejected)
            {
                QFAIL("Invalid teacher import plan was accepted.");
                return;
            }
            QCOMPARE(rejected.error(), expectedError);
        };

        TeacherImportPlan emptyKoreanPlan;
        emptyKoreanPlan.templateId = QStringLiteral("empty-korean-name-test");
        emptyKoreanPlan.sourceDate = QDate(2026, 8, 3);
        Teacher emptyKoreanTeacher;
        emptyKoreanTeacher.teacherKr = QStringLiteral("English only");
        emptyKoreanPlan.koreanTeachers.append(emptyKoreanTeacher);
        const auto emptyKoreanImport =
            repository.importTeachers(emptyKoreanPlan);
        QVERIFY(!emptyKoreanImport.has_value());
        QCOMPARE(
            emptyKoreanImport.error(),
            QStringLiteral("Every imported Korean teacher must have a name.")
            );
        QVERIFY(counts.exec(QStringLiteral("SELECT COUNT(*) FROM teachers")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 1);

        TeacherImportPlan invalidDatePlan;
        invalidDatePlan.templateId = QStringLiteral("invalid-date-test");
        Teacher invalidDateTeacher;
        invalidDateTeacher.teacherKr = QStringLiteral("missing name key");
        invalidDatePlan.koreanTeachers.append(invalidDateTeacher);
        expectPreWriteError(
            invalidDatePlan,
            QStringLiteral("The teacher import date is invalid."));

        TeacherImportPlan emptyNativePlan;
        emptyNativePlan.templateId = QStringLiteral("empty-native-test");
        emptyNativePlan.sourceDate = QDate(2026, 8, 4);
        emptyNativePlan.nativeEnglishTeachers.append(
            {-1, QStringLiteral(" \t "), QStringLiteral("NET"),
             QString(), QString(), QString()});
        expectPreWriteError(
            emptyNativePlan,
            QStringLiteral("Every imported Native English Teacher must have a name."));

        TeacherImportPlan duplicateNativePlan;
        duplicateNativePlan.templateId = QStringLiteral("duplicate-native-test");
        duplicateNativePlan.sourceDate = QDate(2026, 8, 5);
        duplicateNativePlan.nativeEnglishTeachers.append(
            {-1, QStringLiteral("Duplicate Native"), QStringLiteral("NET"),
             QString(), QString(), QString()});
        duplicateNativePlan.nativeEnglishTeachers.append(
            {-1, QStringLiteral(" duplicate native "), QStringLiteral("NET"),
             QString(), QString(), QString()});
        expectPreWriteError(
            duplicateNativePlan,
            QStringLiteral("The import contains a duplicate Native English Teacher name."));

        TeacherImportPlan emptyGsPlan;
        emptyGsPlan.templateId = QStringLiteral("empty-gs-test");
        emptyGsPlan.sourceDate = QDate(2026, 8, 6);
        emptyGsPlan.gsTeamMembers.append(GsTeamMember{});
        expectPreWriteError(
            emptyGsPlan,
            QStringLiteral("Every imported GS Team member must have a name."));

        TeacherImportPlan duplicateGsPlan;
        duplicateGsPlan.templateId = QStringLiteral("duplicate-gs-test");
        duplicateGsPlan.sourceDate = QDate(2026, 8, 7);
        duplicateGsPlan.gsTeamMembers.append(
            {-1, QStringLiteral("GS Member"), QString(), QString(), QString(), QString()});
        duplicateGsPlan.gsTeamMembers.append(
            {-1, QStringLiteral(" gs member "), QString(), QString(), QString(), QString()});
        expectPreWriteError(
            duplicateGsPlan,
            QStringLiteral("The import contains a duplicate GS Team name."));

        QVERIFY(counts.exec(QStringLiteral("SELECT COUNT(*) FROM teachers")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 1);
        QVERIFY(counts.exec(QStringLiteral("SELECT COUNT(*) FROM native_english_teachers")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 1);
        QVERIFY(counts.exec(QStringLiteral("SELECT COUNT(*) FROM gs_team")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 1);
        QSqlQuery unchangedDateQuery(database);
        unchangedDateQuery.prepare(QStringLiteral(
            "SELECT value FROM app_settings WHERE key=?"));
        unchangedDateQuery.addBindValue(QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        QVERIFY(unchangedDateQuery.exec());
        QVERIFY(unchangedDateQuery.next());
        QCOMPARE(unchangedDateQuery.value(0).toString(), QStringLiteral("2026-07-09"));

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
        const auto ambiguousImport = repository.importTeachers(ambiguousPlan);
        QVERIFY(!ambiguousImport.has_value());
        QCOMPARE(
            ambiguousImport.error(),
            QStringLiteral(
                "More than one stored Native English Teacher matches JAMIE."));
        QVERIFY(counts.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr='원자성교사'")));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 0);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TeacherImportTests::gsTeamImportMergesSparseMatchedRecordsAndSkipsNoOpUpdates()
{
    const QString connectionName =
        QStringLiteral("gs-team-import-update-test-%1").arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        const QString koreanName = QString::fromUtf8(
            "\xEA\xB9\x80\xED\x95\x98\xEB\x8A\x98");
        const QString otherKoreanName = QString::fromUtf8(
            "\xEC\x9D\xB4\xEC\x84\x9C\xEC\x97\xB0");
        QSqlQuery seed(database);
        seed.prepare(QStringLiteral(
            "INSERT INTO gs_team "
            "(id, name, korean_name, position, phone_number, birthday) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        seed.addBindValue(9101);
        seed.addBindValue(QStringLiteral("Taylor"));
        seed.addBindValue(koreanName);
        seed.addBindValue(QStringLiteral("M2"));
        seed.addBindValue(QStringLiteral("010-1111-2222"));
        seed.addBindValue(QStringLiteral("05-09"));
        QVERIFY(seed.exec());
        seed.prepare(QStringLiteral(
            "INSERT INTO gs_team "
            "(id, name, korean_name, position, phone_number, birthday) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        seed.addBindValue(9102);
        seed.addBindValue(QStringLiteral("Casey"));
        seed.addBindValue(otherKoreanName);
        seed.addBindValue(QStringLiteral("NET"));
        seed.addBindValue(QStringLiteral("010-3333-4444"));
        seed.addBindValue(QStringLiteral("04-12"));
        QVERIFY(seed.exec());

        QVERIFY(seed.exec(R"(
            CREATE TABLE gs_team_import_update_probe (updates INTEGER NOT NULL)
        )"));
        QVERIFY(seed.exec(R"(
            INSERT INTO gs_team_import_update_probe (updates) VALUES (0)
        )"));
        QVERIFY(seed.exec(R"(
            CREATE TRIGGER gs_team_import_update_probe_trigger
            AFTER UPDATE ON gs_team
            BEGIN
                UPDATE gs_team_import_update_probe SET updates=updates+1;
            END
        )"));

        TeacherImportPlan plan;
        plan.templateId = QStringLiteral("gs-team-sparse-update-test");
        plan.sourceDate = QDate(2026, 9, 10);
        plan.gsTeamMembers.append({
            -1,
            QStringLiteral(" Taylor   Updated "),
            QStringLiteral("  ") + koreanName + QStringLiteral("  "),
            QStringLiteral("  M3  "),
            QStringLiteral("   "),
            QStringLiteral(" 07-08 ")
        });
        plan.gsTeamMembers.append({
            -1,
            QStringLiteral(" Jordan "),
            QString(),
            QStringLiteral(" M1 "),
            QStringLiteral("010-5555-6666"),
            QStringLiteral("08-15")
        });
        plan.gsTeamMembers.append({
            -1,
            QStringLiteral(" Casey "),
            QStringLiteral(" \t "),
            QStringLiteral(" "),
            QString(),
            QString()
        });

        TeacherImportRepository repository(database);
        const auto imported = repository.importTeachers(plan);
        QVERIFY2(imported.has_value(),
                 imported.has_value() ? "" : qPrintable(imported.error()));
        QCOMPARE(imported->gsTeamMembers.created, 1);
        QCOMPARE(imported->gsTeamMembers.updated, 1);
        QCOMPARE(imported->gsTeamMembers.unchanged, 1);
        QCOMPARE(imported->koreanTeachers.total(), 0);
        QCOMPARE(imported->nativeEnglishTeachers.total(), 0);

        QSqlQuery persisted(database);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT id, name, korean_name, position, phone_number, birthday "
            "FROM gs_team WHERE id=9101")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 9101);
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("Taylor Updated"));
        QCOMPARE(persisted.value(2).toString(), koreanName);
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("M3"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("010-1111-2222"));
        QCOMPARE(persisted.value(5).toString(), QStringLiteral("07-08"));
        QVERIFY(!persisted.next());

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT id, name, korean_name, position, phone_number, birthday "
            "FROM gs_team WHERE id=9102")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 9102);
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("Casey"));
        QCOMPARE(persisted.value(2).toString(), otherKoreanName);
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("NET"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("010-3333-4444"));
        QCOMPARE(persisted.value(5).toString(), QStringLiteral("04-12"));
        QVERIFY(!persisted.next());

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT name, korean_name, position, phone_number, birthday "
            "FROM gs_team WHERE name='Jordan'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QStringLiteral("Jordan"));
        QCOMPARE(persisted.value(1).toString(), QString());
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("M1"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("010-5555-6666"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("08-15"));
        QVERIFY(!persisted.next());

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT updates FROM gs_team_import_update_probe")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 1);

        QSqlQuery duplicate(database);
        duplicate.prepare(QStringLiteral(
            "INSERT INTO gs_team (name, korean_name) VALUES (?, ?)"));
        duplicate.addBindValue(QStringLiteral("Second Taylor"));
        duplicate.addBindValue(koreanName);
        QVERIFY(duplicate.exec());

        TeacherImportPlan ambiguousPlan;
        ambiguousPlan.templateId = QStringLiteral("gs-team-ambiguous-match-test");
        ambiguousPlan.sourceDate = QDate(2026, 9, 11);
        ambiguousPlan.gsTeamMembers.append({
            -1, QStringLiteral("Any Name"), koreanName, QString(), QString(), QString()
        });
        const auto ambiguous = repository.importTeachers(ambiguousPlan);
        QVERIFY(!ambiguous.has_value());
        QCOMPARE(
            ambiguous.error(),
            QStringLiteral("More than one stored GS Team member matches %1.")
                .arg(koreanName));
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT updates FROM gs_team_import_update_probe")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 1);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM gs_team WHERE name='Jordan'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 1);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TeacherImportTests::importsCheckedInWorkbookUsingValidatedReviewChoices()
{
    QFile fixture(teacherImportReviewFixturePath());
    QVERIFY2(fixture.open(QIODevice::ReadOnly),
             qPrintable(QStringLiteral("Unable to open fixture: %1")
                            .arg(fixture.fileName())));
    const TeacherImportTemplateRegistry registry =
        createDefaultTeacherImportTemplateRegistry();
    const TeacherImportFileValidation validation =
        validateTeacherImportData(fixture.readAll(), registry);
    QVERIFY2(validation.isValid(),
             qPrintable(validation.diagnostics.join(QLatin1Char('\n'))));
    QCOMPARE(validation.templateId, QStringLiteral("sectioned-contact-list-v1"));
    QCOMPARE(validation.preview.sourceDate, QDate(2026, 9, 1));
    QCOMPARE(validation.preview.koreanGroups.size(), 3);
    QCOMPARE(validation.preview.nativeEnglishTeachers.size(), 1);
    QCOMPARE(validation.preview.nativeEnglishTeachers.at(0).name,
             QStringLiteral("Alex"));
    QCOMPARE(validation.preview.nativeEnglishTeachers.at(0).position,
             QStringLiteral("Team Leader"));
    QCOMPARE(validation.preview.nativeEnglishTeachers.at(0).birthday,
             QStringLiteral("03-07"));
    QCOMPARE(validation.preview.gsTeamMembers.size(), 1);
    QCOMPARE(validation.preview.koreanGroups.at(0).level, QStringLiteral("M1"));
    QCOMPARE(validation.preview.koreanGroups.at(0).candidates.size(), 2);
    QCOMPARE(validation.preview.koreanGroups.at(1).level, QStringLiteral("M2"));
    QCOMPARE(validation.preview.koreanGroups.at(2).level, QStringLiteral("H1"));

    TeacherImportPlan plan;
    plan.templateId = validation.templateId;
    plan.sourceDate = validation.sourceDate;
    plan.nativeEnglishTeachers = validation.preview.nativeEnglishTeachers;
    plan.gsTeamMembers = validation.preview.gsTeamMembers;
    plan.review.emplace();
    plan.review->candidateGroups = validation.preview.koreanGroups;
    plan.review->groupSelections = {
        {QStringLiteral("M1"), TeacherImportSelectionMode::Selected, {0}},
        {QStringLiteral("M2"), TeacherImportSelectionMode::None, {}},
        {QStringLiteral("H1"), TeacherImportSelectionMode::All, {}}
    };
    QVERIFY(resolveReview(*plan.review).accepted());
    plan.koreanTeachers = selectedKoreanTeachers(*plan.review);
    QCOMPARE(plan.koreanTeachers.size(), 2);
    // Keep parser output asserted above and exercise the Qt normalization
    // performed by the repository adapter on real import-plan values.
    plan.nativeEnglishTeachers[0].name = QStringLiteral(" \t Alex   \t ");
    plan.nativeEnglishTeachers[0].position = QStringLiteral(" \t Team Leader \t ");
    plan.nativeEnglishTeachers[0].birthday = QStringLiteral(" \t 03-07 \t ");
    QCOMPARE(plan.koreanTeachers.at(0).teacherKr, QStringLiteral("홍길동"));
    QCOMPARE(plan.koreanTeachers.at(1).teacherKr, QStringLiteral("박민준"));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString connectionName =
        QStringLiteral("teacher-import-reviewed-%1").arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("teacher-import.sqlite")));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery seed(database);
        QVERIFY2(seed.exec(QString::fromUtf8(R"(
            INSERT INTO teachers
                (id, teacher_kr, teacher_en, preferred_romanization,
                 preferred_name, room_number, birthday, phone_number,
                 wifi_name, wifi_password, internet_type, zoom_id,
                 zoom_password, projection_type, notes)
            VALUES
                (7001, '홍길동D', 'Manual English', 'Manual Romanization',
                 'Manual preferred', 'Old room', 'Old birthday',
                 '010-0000-0000', 'Manual WiFi', 'Manual WiFi Password',
                 'LAN', 'manual.zoom', 'manual.zoom.password',
                 'Any', 'Manual notes')
        )")), qPrintable(seed.lastError().text()));
        QVERIFY(seed.exec(R"(
            INSERT INTO native_english_teachers
                (id, name, position, phone_number, birthday, nationality, email)
            VALUES (8104, '  Alex  ', 'NET', '010-9999-8888', '02-01',
                    'Canadian', 'alex@example.com')
        )"));

        TeacherImportRepository repository(database);
        const auto imported = repository.importTeachers(plan);
        QVERIFY2(imported.has_value(),
                 imported.has_value() ? "" : qPrintable(imported.error()));
        QCOMPARE(imported->koreanTeachers.created, 1);
        QCOMPARE(imported->koreanTeachers.updated, 1);
        QCOMPARE(imported->koreanTeachers.unchanged, 0);
        QCOMPARE(imported->nativeEnglishTeachers.created, 0);
        QCOMPARE(imported->nativeEnglishTeachers.updated, 1);
        QCOMPARE(imported->nativeEnglishTeachers.unchanged, 0);
        QCOMPARE(imported->gsTeamMembers.created, 1);
        QCOMPARE(imported->gsTeamMembers.updated, 0);
        QCOMPARE(imported->gsTeamMembers.unchanged, 0);

        QSqlQuery persisted(database);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT id, teacher_kr, room_number, birthday, phone_number, "
            "teacher_en, preferred_romanization, preferred_name, wifi_name, "
            "wifi_password, internet_type, zoom_id, zoom_password, "
            "projection_type, notes "
            "FROM teachers ORDER BY room_number")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 7001);
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("홍길동"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("413"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("02-29"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("010-1111-1111"));
        QCOMPARE(persisted.value(5).toString(), QStringLiteral("Manual English"));
        QCOMPARE(persisted.value(6).toString(), QStringLiteral("Manual Romanization"));
        QCOMPARE(persisted.value(7).toString(), QStringLiteral("Manual preferred"));
        QCOMPARE(persisted.value(8).toString(), QStringLiteral("Manual WiFi"));
        QCOMPARE(persisted.value(9).toString(), QStringLiteral("Manual WiFi Password"));
        QCOMPARE(persisted.value(10).toString(), QStringLiteral("LAN"));
        QCOMPARE(persisted.value(11).toString(), QStringLiteral("manual.zoom"));
        QCOMPARE(persisted.value(12).toString(), QStringLiteral("manual.zoom.password"));
        QCOMPARE(persisted.value(13).toString(), QStringLiteral("Any"));
        QCOMPARE(persisted.value(14).toString(), QStringLiteral("Manual notes"));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("박민준"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("510"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("05-09"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("010-4444-4444"));
        QVERIFY(!persisted.next());

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr='홍길동'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 1);

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr IN ('김하늘', '이서연')")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT id, position, phone_number, birthday, nationality, email "
            "FROM native_english_teachers WHERE name='Alex'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 8104);
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("Team Leader"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("010-9999-8888"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("03-07"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("Canadian"));
        QCOMPARE(persisted.value(5).toString(), QStringLiteral("alex@example.com"));
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM native_english_teachers WHERE name='Alex'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 1);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT name, position, phone_number, birthday FROM gs_team")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QStringLiteral("Taylor"));
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("M2"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("010-5555-5555"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("06-10"));

        QVERIFY(persisted.exec(R"(
            CREATE TABLE teacher_import_update_probe (updates INTEGER NOT NULL)
        )"));
        QVERIFY(persisted.exec(R"(
            INSERT INTO teacher_import_update_probe (updates) VALUES (0)
        )"));
        QVERIFY(persisted.exec(R"(
            CREATE TRIGGER teacher_import_update_probe_trigger
            BEFORE UPDATE ON teachers
            BEGIN
                UPDATE teacher_import_update_probe SET updates=updates+1;
            END
        )"));
        QVERIFY(persisted.exec(R"(
            CREATE TABLE native_teacher_import_update_probe (updates INTEGER NOT NULL)
        )"));
        QVERIFY(persisted.exec(R"(
            INSERT INTO native_teacher_import_update_probe (updates) VALUES (0)
        )"));
        QVERIFY(persisted.exec(R"(
            CREATE TRIGGER native_teacher_import_update_probe_trigger
            BEFORE UPDATE ON native_english_teachers
            BEGIN
                UPDATE native_teacher_import_update_probe SET updates=updates+1;
            END
        )"));

        TeacherImportPlan unchangedPlan;
        unchangedPlan.templateId = plan.templateId;
        unchangedPlan.sourceDate = plan.sourceDate;
        unchangedPlan.nativeEnglishTeachers = plan.nativeEnglishTeachers;
        Teacher sparseKorean;
        sparseKorean.teacherKr = plan.koreanTeachers.at(0).teacherKr;
        unchangedPlan.koreanTeachers.append(sparseKorean);
        const auto unchanged = repository.importTeachers(unchangedPlan);
        QVERIFY2(unchanged.has_value(),
                 unchanged.has_value() ? "" : qPrintable(unchanged.error()));
        QCOMPARE(unchanged->koreanTeachers.created, 0);
        QCOMPARE(unchanged->koreanTeachers.updated, 0);
        QCOMPARE(unchanged->koreanTeachers.unchanged, 1);
        QCOMPARE(unchanged->nativeEnglishTeachers.updated, 0);
        QCOMPARE(unchanged->nativeEnglishTeachers.unchanged, 1);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT updates FROM teacher_import_update_probe")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT updates FROM native_teacher_import_update_probe")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);

        TeacherImportPlan rejected = plan;
        rejected.sourceDate = QDate();
        rejected.review->groupSelections[0].selectedCandidateIndexes.append(99);
        Teacher injected;
        injected.teacherKr = QStringLiteral("신규유입");
        rejected.koreanTeachers.append(injected);
        const auto invalidReview = repository.importTeachers(rejected);
        QVERIFY(!invalidReview.has_value());
        QCOMPARE(
            invalidReview.error(),
            QStringLiteral("The teacher import review choices are invalid."));

        TeacherImportPlan mismatchedSelection = plan;
        mismatchedSelection.sourceDate = QDate();
        mismatchedSelection.koreanTeachers.append(injected);
        const auto mismatchedReview =
            repository.importTeachers(mismatchedSelection);
        QVERIFY(!mismatchedReview.has_value());
        QCOMPARE(
            mismatchedReview.error(),
            QStringLiteral("The reviewed Korean teachers do not match the import selection."));

        QVERIFY(persisted.exec(QStringLiteral("SELECT COUNT(*) FROM teachers")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 2);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr='신규유입'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM native_english_teachers")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 1);
        QVERIFY(persisted.exec(QStringLiteral("SELECT COUNT(*) FROM gs_team")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 1);
        persisted.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key=?"));
        persisted.addBindValue(QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        QVERIFY(persisted.exec());
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QStringLiteral("2026-09-01"));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TeacherImportTests::sortsGsTeamPositions()
{
    const QString connectionName =
        QStringLiteral("gs-team-order-test-%1").arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        const QStringList positions{
            QStringLiteral("C1"),
            QStringLiteral("M1"),
            QStringLiteral("Branch Manager"),
            QStringLiteral("C3"),
            QStringLiteral("M3"),
            QStringLiteral("C2"),
            QStringLiteral("M2")
        };
        QSqlQuery query(database);
        for (const QString& position : positions)
        {
            query.prepare(QStringLiteral(
                "INSERT INTO gs_team (name, position) VALUES (?, ?)"));
            query.addBindValue(position + QStringLiteral(" member"));
            query.addBindValue(position);
            QVERIFY(query.exec());
        }

        GsTeamRepository repository(database);
        const Result<QList<GsTeamMember>> members = repository.getAll();
        QVERIFY(members);
        QCOMPARE(members->size(), positions.size());
        const QStringList expectedPositions{
            QStringLiteral("Branch Manager"),
            QStringLiteral("M3"),
            QStringLiteral("M2"),
            QStringLiteral("M1"),
            QStringLiteral("C3"),
            QStringLiteral("C2"),
            QStringLiteral("C1")
        };
        for (int index = 0; index < expectedPositions.size(); ++index)
        {
            QCOMPARE(members->at(index).position, expectedPositions.at(index));
        }
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
    const auto positionFor = [&validation](const QString& koreanName) {
        for (const GsTeamMember& member : validation.preview.gsTeamMembers)
        {
            if (member.koreanName == koreanName)
            {
                return member.position;
            }
        }
        return QString();
    };
    QCOMPARE(positionFor(QStringLiteral("동경태")), QStringLiteral("Branch Manager"));
    QCOMPARE(positionFor(QStringLiteral("황은미")), QStringLiteral("M3"));
}

QTEST_GUILESS_MAIN(TeacherImportTests)

#include "teacher_import_tests.moc"
