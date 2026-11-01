// Diagnostic font experiment only. Does not call or alter production services.
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QRawFont>
#include <QRegularExpression>
#include <QSet>
#include <QTextLayout>

namespace {
const QString context = QStringLiteral("Student 1-1 E4 • Hercules - normal / zoom-1 / wifi-2 / 14 - 18");
QString sha(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
QByteArray read(const QString& path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) qFatal("Cannot read input"); return f.readAll(); }
QFont loadFont(const QByteArray& bytes) {
    int id = QFontDatabase::addApplicationFontFromData(bytes);
    auto names = QFontDatabase::applicationFontFamilies(id);
    if (id < 0 || names.isEmpty()) qFatal("Qt rejected font");
    QFont font(names.first()); font.setStyleName("Regular"); font.setPixelSize(12);
    font.setVariableAxis(*QFont::Tag::fromString("wght"), 400);
    return font;
}
QJsonObject glyphEvidence(const QFont& font) {
    const QRawFont raw = QRawFont::fromFont(font);
    if (!raw.isValid()) qFatal("Invalid raw font");
    QJsonArray cmap, shaped, derived;
    QList<int> characters;
    for (int c = 0; c <= 32; ++c) characters.append(c);
    characters.append(45); characters.append(0x2022);
    for (int c : characters) {
        const auto ids = raw.glyphIndexesForString(QString(QChar(c)));
        if (ids.size() != 1) qFatal("Unexpected raw cmap width");
        cmap.append(QJsonObject{{"unicode", c}, {"glyph", int(ids.first())}});
    }
    QTextLayout layout(context, font); layout.beginLayout(); auto line = layout.createLine(); line.setLineWidth(10000); layout.endLayout();
    const auto runs = layout.glyphRuns(0, -1, QTextLayout::RetrieveGlyphIndexes | QTextLayout::RetrieveGlyphPositions | QTextLayout::RetrieveStringIndexes);
    QSet<int> derivedCharacters;
    for (const auto& run : runs) {
        const auto glyphs = run.glyphIndexes(); const auto positions = run.positions(); const auto indexes = run.stringIndexes();
        for (qsizetype i = 0; i < glyphs.size(); ++i) {
            const auto c = context.at(indexes.at(i));
            const auto direct = run.rawFont().glyphIndexesForString(QString(c));
            shaped.append(QJsonObject{{"sourceIndex", int(indexes.at(i))}, {"unicode", int(c.unicode())},
                {"glyph", int(glyphs.at(i))}, {"x", positions.at(i).x()}, {"y", positions.at(i).y()}});
            if ((c == u'-' || c == QChar(0x2022)) && direct.size() == 1 && direct.first() != glyphs.at(i) && !derivedCharacters.contains(c.unicode())) {
                derived.append(QJsonObject{{"unicode", int(c.unicode())}, {"cmapGlyph", int(direct.first())}, {"shapedGlyph", int(glyphs.at(i))}});
                derivedCharacters.insert(c.unicode());
            }
        }
    }
    return {{"family", QFontInfo(font).family()}, {"style", QFontInfo(font).styleName()},
        {"rawFontValid", raw.isValid()}, {"rawCmap", cmap}, {"context", context},
        {"shapedGlyphs", shaped}, {"derivedSubstitutions", derived}};
}
void makePdf(const QString& path, const QFont& font) {
    QPdfWriter writer(path); writer.setResolution(96); writer.setCreator("C53 diagnostic only");
    QPainter painter(&writer); if (!painter.isActive()) qFatal("Cannot open PDF painter");
    for (int page = 0; page < 16; ++page) {
        if (page && !writer.newPage()) qFatal("Cannot add PDF page");
        for (int slot = 0; slot < 6; ++slot) {
            const int classIndex = page * 6 + slot;
            const int teacher = classIndex / 4 + 1;
            const qreal x = 12 + (slot % 3) * 248;
            const qreal y = 22 + (slot / 3) * 480;
            QFont header = font; header.setPixelSize(8); painter.setFont(header);
            painter.drawText(QPointF(x, y), QStringLiteral("E%1 Level %2 (Zoom: zoom.%3 • PW zoom-%3)")
                .arg(classIndex / 16 + 1).arg(classIndex % 4 + 1).arg(teacher));
            painter.setFont(font);
            for (int student = 0; student < 25; ++student)
                painter.drawText(QPointF(x, y + 20 + student * 16), QStringLiteral("Student %1-%2").arg(classIndex + 1).arg(student + 1));
        }
        painter.setFont(font); painter.drawText(QPointF(12, 1030), context);
    }
    if (!painter.end()) qFatal("Cannot finish PDF");
}
QJsonObject inspectPdf(const QString& path) {
    QPdfDocument pdf; if (pdf.load(path) != QPdfDocument::Error::None || pdf.pageCount() != 16) qFatal("Cannot inspect PDF");
    QString text; QJsonArray geometry;
    for (int page = 0; page < pdf.pageCount(); ++page) {
        text += pdf.getAllText(page).text(); text += QChar('\n');
        const QSizeF size = pdf.pagePointSize(page); geometry.append(QJsonObject{{"widthPt", size.width()}, {"heightPt", size.height()}});
    }
    QSet<QString> actual;
    auto matches = QRegularExpression(QStringLiteral("Student (\\d+)-(\\d+)(?!\\d)")).globalMatch(text);
    while (matches.hasNext()) { auto m = matches.next(); actual.insert(m.captured(1) + '-' + m.captured(2)); }
    int missing = 0, headers = 0;
    for (int c = 1; c <= 96; ++c) {
        for (int s = 1; s <= 25; ++s) if (!actual.contains(QStringLiteral("%1-%2").arg(c).arg(s))) ++missing;
        const int t = (c - 1) / 4 + 1;
        if (text.contains(QStringLiteral("E%1 Level %2 (Zoom: zoom.%3 • PW zoom-%3)")
            .arg((c - 1) / 16 + 1).arg((c - 1) % 4 + 1).arg(t))) ++headers;
    }
    return {{"sha256", sha(read(path))}, {"pages", geometry}, {"missingExpectedStudentNames", missing},
        {"expectedStudentNameCount", 2400}, {"expectedHeaderCount", 96}, {"headersWithExactSourceFragment", headers},
        {"bulletCount", text.count(QChar(0x2022))}, {"expectedBulletCountIncludingContexts", 112},
        {"exactContextCopies", text.count(context)}, {"expectedContextCopies", 16},
        {"sourceTextPass", missing == 0 && headers == 96 && text.count(context) == 16}};
}
QJsonArray raster(const QString& leftPath, const QString& rightPath) {
    QPdfDocument left, right;
    if (left.load(leftPath) != QPdfDocument::Error::None || right.load(rightPath) != QPdfDocument::Error::None || left.pageCount() != right.pageCount()) qFatal("Cannot compare PDFs");
    QJsonArray pages;
    for (int page = 0; page < left.pageCount(); ++page) {
        const auto points = left.pagePointSize(page); if (right.pagePointSize(page) != points) qFatal("Page geometry changed");
        const QSize size(qRound(points.width() * 150.0 / 72.0), qRound(points.height() * 150.0 / 72.0));
        const auto a = left.render(page, size).convertToFormat(QImage::Format_ARGB32);
        const auto b = right.render(page, size).convertToFormat(QImage::Format_ARGB32);
        if (a.isNull() || b.isNull()) qFatal("Cannot render page");
        qint64 changed = 0;
        for (int y = 0; y < size.height(); ++y) {
            const auto ar = reinterpret_cast<const QRgb*>(a.constScanLine(y)); const auto br = reinterpret_cast<const QRgb*>(b.constScanLine(y));
            for (int x = 0; x < size.width(); ++x) if (ar[x] != br[x]) ++changed;
        }
        pages.append(QJsonObject{{"page", page + 1}, {"width", size.width()}, {"height", size.height()}, {"dpi", 150},
            {"changedPixels", changed}, {"toleranceChangedPixels", 0}, {"exactRasterPass", changed == 0}});
    }
    return pages;
}
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    if (argc < 3 || argc > 4) qFatal("Usage: font_variant_probe ORIGINAL_FONT OUTPUT_DIRECTORY [VARIANT_FONT]");
    const QDir root(QString::fromLocal8Bit(argv[2])); if (!QDir().mkpath(root.absolutePath())) qFatal("Cannot create output directory");
    const auto originalBytes = read(QString::fromLocal8Bit(argv[1])); const QFont original = loadFont(originalBytes);
    const auto originalEvidence = glyphEvidence(original);
    QJsonObject report{{"taskId", "phase7_c53_subprep_output_implementation_01"}, {"qtVersion", qVersion()},
        {"platformPlugin", QGuiApplication::platformName()}, {"fontSha256", sha(originalBytes)},
        {"derivedSubstitutions", originalEvidence.value("derivedSubstitutions")}, {"rawCmap", originalEvidence.value("rawCmap")},
        {"originalGlyphEvidence", originalEvidence}};
    const auto originalPath = root.filePath("original.pdf"); makePdf(originalPath, original); report.insert("originalPdf", inspectPdf(originalPath));
    if (argc == 4) {
        const auto variantBytes = read(QString::fromLocal8Bit(argv[3])); const QFont variant = loadFont(variantBytes);
        const auto variantEvidence = glyphEvidence(variant);
        report.insert("variantFontSha256", sha(variantBytes)); report.insert("variantGlyphEvidence", variantEvidence);
        report.insert("sameShapedGlyphsAndPositions", originalEvidence.value("shapedGlyphs") == variantEvidence.value("shapedGlyphs"));
        const auto variantPath = root.filePath("variant.pdf"); makePdf(variantPath, variant); report.insert("variantPdf", inspectPdf(variantPath));
        report.insert("allPageRasterComparison", raster(originalPath, variantPath));
    }
    QFile output(root.filePath("manifest.json")); if (!output.open(QIODevice::WriteOnly) || output.write(QJsonDocument(report).toJson()) < 0) qFatal("Cannot write manifest");
}
