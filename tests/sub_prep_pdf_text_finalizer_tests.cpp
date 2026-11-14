#include "shared/roster/sub_prep_pdf_text_finalizer.h"
#include "shared/roster/roster_template_renderer.h"
#include "core/qt_pdf_generation_adapter.h"
#include <QtTest>
#include <QBuffer>
#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QFontDatabase>
#include <QPainter>
#include <QPdfDocument>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QRegularExpression>

namespace {
QFont font;
QFont uiFont;
struct Generated { QByteArray bytes; SubPrepPdfText::SourceJournal source; };
Generated generate(const QStringList& lines, bool capture, bool document = false) {
    Generated generated;
    QBuffer output(&generated.bytes); output.open(QIODevice::WriteOnly);
    {
        const auto pdfGeneration = makeQtPdfGenerationPort();
        auto session = pdfGeneration->create(output, {
            .creator = QStringLiteral("ClassMngr"),
            .resolutionDpi = 300
        });
        if (!session) return {};
        auto& writer = *session;
        std::unique_ptr<SubPrepPdfText::CaptureDevice> recording;
        if (capture) recording = std::make_unique<SubPrepPdfText::CaptureDevice>(writer, generated.source);
        QPainter painter(recording ? static_cast<QPaintDevice*>(recording.get()) : &writer.paintDevice());
        painter.setFont(font);
        if (document) {
            QTextDocument text; text.documentLayout()->setPaintDevice(&writer.paintDevice()); text.setDefaultFont(font);
            text.setHtml(QStringLiteral("<h2>Class Information</h2><table border='1'><tr><td>Student 1-1</td><td>Zoom: a • PW zoom-1</td></tr></table><p>한국어 학생</p>"));
            text.setPageSize(QSizeF(1800, 2500)); text.drawContents(&painter);
        } else {
            for (qsizetype i = 0; i < lines.size(); ++i)
                painter.drawText(QRectF(20, 20 + i * 120, 1800, 100), Qt::AlignLeft | Qt::AlignVCenter, lines.at(i));
        }
        painter.end(); if (recording) recording->finish();
    }
    return generated;
}
QString save(const QTemporaryDir& temporary, const QString& name, const QByteArray& bytes) {
    const auto path = temporary.filePath(name); QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) return {};
    return path;
}
QList<RosterTemplateRenderer::RosterClassData> c44Classes() {
    QList<RosterTemplateRenderer::RosterClassData> result;
    for (int classIndex = 0; classIndex < 96; ++classIndex) {
        RosterTemplateRenderer::RosterClassData data;
        data.classroom = {1000 + classIndex, QStringLiteral("Heavy Class %1").arg(classIndex + 1)};
        data.classInfo.classGrade = QStringLiteral("E%1").arg((classIndex / 16) + 1);
        data.classInfo.classLevel = QStringLiteral("Level %1").arg((classIndex % 4) + 1);
        const int teacherIndex = classIndex / 4;
        data.classInfo.teacherEn = QStringLiteral("Teacher %1").arg(teacherIndex + 1, 2, 10, QLatin1Char('0'));
        data.classInfo.roomNumber = QString::number(401 + teacherIndex);
        data.classInfo.zoomId = QStringLiteral("zoom.%1").arg(teacherIndex + 1);
        data.classInfo.zoomPassword = QStringLiteral("zoom-%1").arg(teacherIndex + 1);
        const int hour = 4 + (classIndex % 8);
        const QString start = QStringLiteral("%1:00 PM").arg(hour);
        data.classInfo.regularSchedule.append({QStringLiteral("Tuesday"), start,
            QStringLiteral("%1:50 PM").arg(hour)});
        data.roster.columns = {QStringLiteral("English"), QStringLiteral("Korean"), QStringLiteral("Allergies")};
        for (int studentIndex = 0; studentIndex < 25; ++studentIndex) {
            data.roster.rows.append({
                QStringLiteral("Student %1-%2").arg(classIndex + 1).arg(studentIndex + 1),
                QStringLiteral("Student Korean %1-%2").arg(classIndex + 1).arg(studentIndex + 1),
                studentIndex % 5 == 0 ? QStringLiteral("Check allergy plan") : QStringLiteral("None")});
        }
        result.append(std::move(data));
    }
    return result;
}
QString bytesSha256(const QImage& image) {
    const auto bytes = QByteArray(reinterpret_cast<const char*>(image.constBits()),
        static_cast<qsizetype>(image.sizeInBytes()));
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
bool writeJson(const QString& path, const QJsonObject& object) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Text)
        && file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) > 0;
}
}
class SubPrepPdfTextFinalizerTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        const auto uiPath = QFINDTESTDATA("../resources/assets/fonts/Inter.ttc");
        const int uiId = QFontDatabase::addApplicationFont(uiPath);
        QVERIFY(uiId >= 0); const auto uiFamilies = QFontDatabase::applicationFontFamilies(uiId);
        QVERIFY(!uiFamilies.isEmpty()); uiFont = QFont(uiFamilies.first());
        const auto path = QFINDTESTDATA("../resources/assets/fonts/PretendardVariable.ttf");
        const int id = QFontDatabase::addApplicationFont(path);
        QVERIFY(id >= 0); const auto families = QFontDatabase::applicationFontFamilies(id); QVERIFY(!families.isEmpty());
        font = QFont(families.first()); font.setStyleName("Regular"); font.setPixelSize(48);
        font.setVariableAxis(*QFont::Tag::fromString("wght"), 400);
    }
    void c44DailyPdfRepairsSourceTextWithoutChangingPixels() {
        const auto classes = c44Classes();
        QString error;
        const auto values = RosterTemplateRenderer::buildDailyCellValues(classes, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!values.isEmpty());

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto fonts = RosterTemplateRenderer::Fonts{uiFont, font};
        const auto beforePath = temporary.filePath("C44-before.pdf");
        const auto afterPath = temporary.filePath("C44-after.pdf");
        const auto beforeResult = RosterTemplateRenderer::saveCellValuesPdf(
            beforePath, RosterTemplateRenderer::TemplateId::Daily,
            RosterTemplateRenderer::templateOrientation(RosterTemplateRenderer::TemplateId::Daily), values, fonts);
        QVERIFY2(beforeResult.status == RosterTemplateRenderer::Status::Sent, qPrintable(beforeResult.message));
        const auto afterResult = RosterTemplateRenderer::saveCellValuesPdfForNextSubPrep(
            afterPath, RosterTemplateRenderer::TemplateId::Daily,
            RosterTemplateRenderer::templateOrientation(RosterTemplateRenderer::TemplateId::Daily), values, fonts);
        QVERIFY2(afterResult.status == RosterTemplateRenderer::Status::Sent, qPrintable(afterResult.message));

        QPdfDocument before, after;
        QCOMPARE(before.load(beforePath), QPdfDocument::Error::None);
        QCOMPARE(after.load(afterPath), QPdfDocument::Error::None);
        QCOMPARE(before.pageCount(), 16);
        QCOMPARE(after.pageCount(), before.pageCount());
        QString extracted;
        QJsonArray pageAudit;
        for (int page = 0; page < before.pageCount(); ++page) {
            const auto points = before.pagePointSize(page);
            QCOMPARE(points, after.pagePointSize(page));
            const QSize size(qRound(points.width() * 150.0 / 72.0), qRound(points.height() * 150.0 / 72.0));
            const QImage original = before.render(page, size);
            const QImage fixed = after.render(page, size);
            QVERIFY(!original.isNull());
            QVERIFY(!fixed.isNull());
            QCOMPARE(original, fixed);
            const QString pageText = after.getAllText(page).text();
            extracted += pageText;
            extracted += QLatin1Char('\n');
            pageAudit.append(QJsonObject{
                {QStringLiteral("page"), page + 1},
                {QStringLiteral("widthPt"), points.width()},
                {QStringLiteral("heightPt"), points.height()},
                {QStringLiteral("widthPxAt150Dpi"), size.width()},
                {QStringLiteral("heightPxAt150Dpi"), size.height()},
                {QStringLiteral("changedPixels"), 0},
                {QStringLiteral("beforeRasterSha256"), bytesSha256(original)},
                {QStringLiteral("afterRasterSha256"), bytesSha256(fixed)}
            });
        }

        int auditedNameCount = 0;
        for (int classIndex = 0; classIndex < 96; ++classIndex) {
            for (int studentIndex = 0; studentIndex < 25; ++studentIndex) {
                const QString expected = QStringLiteral("Student %1-%2").arg(classIndex + 1).arg(studentIndex + 1);
                const QRegularExpression exact(QStringLiteral("(?<![A-Za-z0-9-])%1(?![A-Za-z0-9-])")
                    .arg(QRegularExpression::escape(expected)));
                auto matches = exact.globalMatch(extracted);
                int occurrences = 0;
                while (matches.hasNext()) { matches.next(); ++occurrences; }
                QVERIFY2(occurrences == 1, qPrintable(expected));
                ++auditedNameCount;
            }
        }
        QCOMPARE(auditedNameCount, 2400);
        QCOMPARE(extracted.count(QChar(0x2022)), 96);
        int auditedHeaderCount = 0;
        for (int classIndex = 0; classIndex < 96; ++classIndex) {
            const int teacherIndex = classIndex / 4;
            const int hour = 4 + (classIndex % 8);
            const QString expectedHeader = QStringLiteral("E%1 Level %2 (%3 p.m. / Teacher %4 / Room %5 / Zoom: zoom.%6 • PW zoom-%6)")
                .arg((classIndex / 16) + 1)
                .arg((classIndex % 4) + 1)
                .arg(hour)
                .arg(teacherIndex + 1, 2, 10, QLatin1Char('0'))
                .arg(401 + teacherIndex)
                .arg(teacherIndex + 1);
            const int exactOccurrences = extracted.count(expectedHeader);
            QVERIFY2(exactOccurrences == 1, qPrintable(expectedHeader));
            ++auditedHeaderCount;
        }
        QCOMPARE(auditedHeaderCount, 96);
        QVERIFY(!extracted.contains(QChar(0x0009)));
        QVERIFY(!extracted.contains(QChar(0x001f)));
        QVERIFY(!extracted.contains(QChar(0x0015)));
        QVERIFY(!extracted.contains(QChar(0x000c)));

        const QString evidenceRoot = qEnvironmentVariable("CLASSMNGR_SUB_PREP_C53_RENDERER_UNIT_EVIDENCE_DIR").trimmed();
        if (!evidenceRoot.isEmpty()) {
            QVERIFY(QDir().mkpath(evidenceRoot));
            const QString retainedBefore = QDir(evidenceRoot).filePath("C44-before.pdf");
            const QString retainedAfter = QDir(evidenceRoot).filePath("C44-after.pdf");
            QFile::remove(retainedBefore);
            QFile::remove(retainedAfter);
            QVERIFY(QFile::copy(beforePath, retainedBefore));
            QVERIFY(QFile::copy(afterPath, retainedAfter));
            QVERIFY(writeJson(QDir(evidenceRoot).filePath("C44-raster-source-audit.json"), QJsonObject{
                {QStringLiteral("taskId"), QStringLiteral("phase7_c53_subprep_finalizer_implementation_02")},
                {QStringLiteral("sourceFixture"), QStringLiteral("96 classes x 25 Student n-n English names; source fields construct 96 Zoom bullets")},
                {QStringLiteral("expectedStudentNames"), 2400},
                {QStringLiteral("foundStudentNames"), auditedNameCount},
                {QStringLiteral("expectedHeaderBullets"), 96},
                {QStringLiteral("foundHeaderBullets"), extracted.count(QChar(0x2022))},
                {QStringLiteral("expectedHeaderStrings"), auditedHeaderCount},
                {QStringLiteral("sourceTextPass"), true},
                {QStringLiteral("renderer"), QStringLiteral("Qt QPdfDocument on both before/after PDFs")},
                {QStringLiteral("dpi"), 150},
                {QStringLiteral("changedPixelsPerPage"), 0},
                {QStringLiteral("pages"), pageAudit}
            }));
        }
    }
    void preservesPaintingAndUsesOriginalSource() {
        const QStringList expected{"Student 1-1 E4 • Hercules - normal", "Zoom: zoom.1 • PW zoom-1", "한국어 학생"};
        const auto baseline = generate(expected, false);
        const auto captured = generate(expected, true);
        QVERIFY2(captured.source.error.isEmpty(), qPrintable(captured.source.error));
        const auto finalized = SubPrepPdfText::finalize(captured.bytes, captured.source);
        QVERIFY2(finalized.has_value(), qPrintable(finalized ? QString() : finalized.error()));
        QTemporaryDir temporary; QVERIFY(temporary.isValid());
        QPdfDocument a, b, c;
        QCOMPARE(a.load(save(temporary, "original.pdf", baseline.bytes)), QPdfDocument::Error::None);
        QCOMPARE(b.load(save(temporary, "captured.pdf", captured.bytes)), QPdfDocument::Error::None);
        QCOMPARE(c.load(save(temporary, "final.pdf", *finalized)), QPdfDocument::Error::None);
        QCOMPARE(a.pageCount(), c.pageCount());
        for (int page = 0; page < a.pageCount(); ++page) {
            const auto points = a.pagePointSize(page); QCOMPARE(points, c.pagePointSize(page));
            const QSize size(qRound(points.width() * 150 / 72), qRound(points.height() * 150 / 72));
            QCOMPARE(a.render(page, size), b.render(page, size));
            QCOMPARE(a.render(page, size), c.render(page, size));
        }
        const auto actual = c.getAllText(0).text();
        for (const auto& line : expected) QVERIFY2(actual.contains(line), qPrintable(actual));
        for (const QChar forbidden : {QChar(9), QChar(0x1f), QChar(0x15), QChar(0x0c)}) QVERIFY(!actual.contains(forbidden));
        QVERIFY(finalized->startsWith(captured.bytes)); // All original painting bytes survive verbatim.
    }
    void preservesRichTextPainting() {
        const auto baseline = generate({}, false, true); const auto captured = generate({}, true, true);
        QVERIFY2(captured.source.error.isEmpty(), qPrintable(captured.source.error));
        const auto finalized = SubPrepPdfText::finalize(captured.bytes, captured.source);
        QVERIFY2(finalized.has_value(), qPrintable(finalized ? QString() : finalized.error()));
        QTemporaryDir temporary; QPdfDocument a, b;
        QCOMPARE(a.load(save(temporary, "original.pdf", baseline.bytes)), QPdfDocument::Error::None);
        QCOMPARE(b.load(save(temporary, "final.pdf", *finalized)), QPdfDocument::Error::None);
        QCOMPARE(a.render(0, QSize(1240, 1754)), b.render(0, QSize(1240, 1754)));
    }
    void rejectsConflictingSourceAliases() {
        auto generated = generate({"aa"}, true);
        QCOMPARE(generated.source.pages.size(), 1); QCOMPARE(generated.source.pages[0].size(), 1);
        generated.source.pages[0][0] = "ab";
        const auto result = SubPrepPdfText::finalize(generated.bytes, generated.source);
        QVERIFY(!result); QVERIFY(result.error().contains("conflicting source"));
    }
    void rejectsUnsupportedStructures_data() {
        QTest::addColumn<int>("mutation");
        QTest::newRow("version") << 0; QTest::newRow("xref-stream") << 1;
        QTest::newRow("encryption") << 2; QTest::newRow("incremental-input") << 3;
        QTest::newRow("unknown-producer") << 4; QTest::newRow("missing-map") << 5;
        QTest::newRow("missing-source") << 6; QTest::newRow("malformed-xref-offset") << 7;
        QTest::newRow("unsupported-content-filter") << 8; QTest::newRow("malformed-object-generation") << 9;
        QTest::newRow("unexpected-source-item") << 10; QTest::newRow("control-source-character") << 11;
        QTest::newRow("missing-page-resource") << 12;
    }
    void rejectsUnsupportedStructures() {
        QFETCH(int, mutation); auto generated = generate({"Student 1-1 •"}, true);
        if (mutation == 0) generated.bytes.replace("%PDF-1.4", "%PDF-1.5");
        if (mutation == 1) generated.bytes.replace("xref\n0 ", "xxxx\n0 ");
        if (mutation == 2 || mutation == 3) {
            const auto index = generated.bytes.lastIndexOf("/Size ");
            generated.bytes.insert(index, mutation == 2 ? "/Encrypt 1 0 R\n" : "/Prev 1\n");
        }
        if (mutation == 4) generated.bytes.replace(QByteArray::fromHex("0051007400200036002e00310032002e0030"), QByteArray::fromHex("0051007400200037002e00310032002e0030"));
        if (mutation == 5) generated.bytes.replace("/ToUnicode", "/NoUnicode");
        if (mutation == 6) generated.source.pages[0].clear();
        if (mutation == 7) {
            const auto start = generated.bytes.indexOf("startxref\n");
            QVERIFY(start >= 0);
            const auto digits = start + qsizetype(sizeof("startxref\n") - 1);
            const auto end = generated.bytes.indexOf('\n', digits);
            QVERIFY(end > digits);
            generated.bytes.replace(digits, end - digits, "0");
        }
        if (mutation == 8) { QVERIFY(generated.bytes.contains("/Filter /FlateDecode")); generated.bytes.replace("/Filter /FlateDecode", "/Filter /LZWDecode"); }
        if (mutation == 9) { QVERIFY(generated.bytes.contains(" 00000 n ")); generated.bytes.replace(" 00000 n ", " 00001 n "); }
        if (mutation == 10) generated.source.pages[0].append(QStringLiteral("unexpected"));
        if (mutation == 11) generated.source.pages[0][0] = QString(QChar(0));
        if (mutation == 12) { QVERIFY(generated.bytes.contains("/Resources")); generated.bytes.replace("/Resources", "/NoResources"); }
        QVERIFY(!SubPrepPdfText::finalize(generated.bytes, generated.source));
    }
    void rejectsUnsupportedClusterSource() {
        const auto generated = generate({QStringLiteral("a\u0301")}, true);
        QVERIFY(!generated.source.error.isEmpty());
        QVERIFY(!SubPrepPdfText::finalize(generated.bytes, generated.source));
    }
};
QTEST_MAIN(SubPrepPdfTextFinalizerTests)
#include "sub_prep_pdf_text_finalizer_tests.moc"
