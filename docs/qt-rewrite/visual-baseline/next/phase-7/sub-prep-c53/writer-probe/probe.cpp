// Standalone diagnostic, not a production PDF renderer. See ../README.md.
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
#include <QPainterPath>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QRawFont>
#include <QSet>
#include <QTextLayout>
#include <QTextStream>

namespace {
const QString input = QStringLiteral("Student 1-1 E4 • Hercules - normal");

// A rejected public-API experiment: retain the shaped positions and replace
// punctuation only when its Unicode-mapped outline is an exact translation.
// Glyph IDs and offsets come from the resolved font, never from retained CIDs.
QList<QGlyphRun> canonicalRuns(QTextLayout& layout, QJsonArray* substitutions)
{
    auto runs = layout.glyphRuns(0, -1, QTextLayout::RetrieveGlyphIndexes
        | QTextLayout::RetrieveGlyphPositions | QTextLayout::RetrieveStringIndexes);
    for (auto& run : runs) {
        auto glyphs = run.glyphIndexes();
        auto positions = run.positions();
        const auto indexes = run.stringIndexes();
        const QRawFont raw = run.rawFont();
        for (qsizetype i = 0; i < glyphs.size(); ++i) {
            const auto character = input.at(indexes.at(i));
            if (character != u'-' && character != QChar(0x2022)) continue;
            const auto direct = raw.glyphIndexesForString(QString(character));
            if (direct.size() != 1 || !direct.first() || direct.first() == glyphs.at(i)) continue;
            const QPainterPath original = raw.pathForGlyph(glyphs.at(i));
            const QPainterPath canonical = raw.pathForGlyph(direct.first());
            const QPointF delta = original.boundingRect().topLeft() - canonical.boundingRect().topLeft();
            const bool equal = !canonical.isEmpty() && canonical.translated(delta) == original;
            substitutions->append(QJsonObject{
                {"unicode", int(character.unicode())}, {"shapedGlyph", int(glyphs.at(i))},
                {"cmapGlyph", int(direct.first())}, {"translatedOutlineEqual", equal},
                {"deltaX", delta.x()}, {"deltaY", delta.y()}});
            if (equal) {
                glyphs[i] = direct.first();
                positions[i] += delta;
            }
        }
        run.setGlyphIndexes(glyphs);
        run.setPositions(positions);
    }
    return runs;
}

QString extract(const QString& path, QJsonObject* result)
{
    QPdfDocument pdf;
    if (pdf.load(path) != QPdfDocument::Error::None || !pdf.pageCount())
        qFatal("Could not load PDF");
    QString text;
    QJsonArray geometry;
    for (int page = 0; page < pdf.pageCount(); ++page) {
        text += pdf.getAllText(page).text();
        const QSizeF size = pdf.pagePointSize(page);
        geometry.append(QJsonObject{{"widthPt", size.width()}, {"heightPt", size.height()}});
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) qFatal("Could not hash PDF");
    result->insert("sha256", QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex()));
    result->insert("pages", geometry);
    return text;
}

QJsonObject compareRaster(const QString& referencePath, const QString& candidatePath)
{
    QPdfDocument reference, candidate;
    if (reference.load(referencePath) != QPdfDocument::Error::None
        || candidate.load(candidatePath) != QPdfDocument::Error::None) qFatal("Could not load raster inputs");
    const auto points = reference.pagePointSize(0);
    const QSize size(qRound(points.width() * 150.0 / 72.0), qRound(points.height() * 150.0 / 72.0));
    const QImage a = reference.render(0, size).convertToFormat(QImage::Format_ARGB32);
    const QImage b = candidate.render(0, size).convertToFormat(QImage::Format_ARGB32);
    if (a.isNull() || b.isNull()) qFatal("Could not render raster inputs");
    qint64 changed = 0, absolute = 0;
    int maximum = 0;
    for (int y = 0; y < size.height(); ++y) {
        const auto* ar = reinterpret_cast<const QRgb*>(a.constScanLine(y));
        const auto* br = reinterpret_cast<const QRgb*>(b.constScanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            if (ar[x] != br[x]) ++changed;
            for (const int shift : {0, 8, 16, 24}) {
                const int delta = qAbs(int((ar[x] >> shift) & 255) - int((br[x] >> shift) & 255));
                absolute += delta;
                maximum = qMax(maximum, delta);
            }
        }
    }
    return {{"dpi", 150}, {"width", size.width()}, {"height", size.height()},
        {"changedPixels", changed}, {"maximumChannelDelta", maximum},
        {"meanAbsoluteChannelDelta", double(absolute) / (size.width() * size.height() * 4.0)},
        {"toleranceChangedPixels", 0}, {"exactRasterPass", changed == 0}};
}
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 3) qFatal("Usage: probe ABSOLUTE_FONT_PATH OUTPUT_DIRECTORY [C44_ROSTER_PDFS...]");
    QFile fontFile(QString::fromLocal8Bit(argv[1]));
    if (!fontFile.open(QIODevice::ReadOnly)) qFatal("Could not read font");
    const QByteArray fontBytes = fontFile.readAll();
    const int id = QFontDatabase::addApplicationFontFromData(fontBytes);
    const auto families = QFontDatabase::applicationFontFamilies(id);
    if (id < 0 || families.isEmpty()) qFatal("Could not load Pretendard");
    QFont font(families.first());
    font.setStyleName(QStringLiteral("Regular"));
    font.setPointSizeF(12);
    font.setVariableAxis(*QFont::Tag::fromString(QStringLiteral("wght")), 400);
    const QDir root(QString::fromLocal8Bit(argv[2]));
    if (!QDir().mkpath(root.absolutePath())) qFatal("Could not create output directory");
    const QStringList modes{"original", "no-shaping", "canonical-outline-trial"};
    QJsonArray variants, substitutions, fixtures;
    for (int mode = 0; mode < modes.size(); ++mode) {
        QFont selected = font;
        if (mode == 1) selected.setStyleStrategy(QFont::PreferNoShaping);
        const QString path = root.filePath(modes.at(mode) + QStringLiteral(".pdf"));
        {
            QPdfWriter writer(path);
            writer.setResolution(96);
            QPainter painter(&writer);
            if (!painter.isActive()) qFatal("Could not begin PDF painter");
            painter.setFont(selected);
            const QRectF rect(50, 50, 500, 100);
            if (mode < 2) painter.drawText(rect, Qt::AlignCenter, input);
            else {
                QTextLayout layout(input, selected);
                QTextOption option;
                option.setTextDirection(painter.layoutDirection());
                option.setAlignment(Qt::AlignLeft);
                layout.setTextOption(option);
                layout.beginLayout();
                QTextLine line = layout.createLine();
                line.setLineWidth(0x01000000);
                layout.endLayout();
                const auto runs = canonicalRuns(layout, &substitutions);
                const QPointF origin(rect.x() + (rect.width() - line.horizontalAdvance()) / 2,
                    rect.y() + (rect.height() - line.height()) / 2);
                painter.save();
                painter.setClipRect(rect, Qt::IntersectClip);
                for (const auto& run : runs) painter.drawGlyphRun(origin, run);
                painter.restore();
            }
            if (!painter.end()) qFatal("Could not end PDF painter");
        }
        QJsonObject result{{"path", modes.at(mode) + QStringLiteral(".pdf")}};
        const auto text = extract(path, &result);
        result.insert("expectedText", input);
        result.insert("actualText", text);
        result.insert("exactTextPass", text == input);
        if (mode) result.insert("rasterVersusOriginal", compareRaster(root.filePath("original.pdf"), path));
        variants.append(result);
    }
    for (int arg = 3; arg < argc; ++arg) {
        const QString path = QString::fromLocal8Bit(argv[arg]);
        QJsonObject fixture{{"path", path}};
        const QString text = extract(path, &fixture);
        const QRegularExpression names(QStringLiteral("Student\\s+(\\d+)-(\\d+)(?!\\d)"));
        auto matches = names.globalMatch(text);
        QSet<QString> actual;
        int found = 0;
        while (matches.hasNext()) { const auto match = matches.next(); actual.insert(match.captured(1) + '-' + match.captured(2)); ++found; }
        int missing = 0;
        for (int classIndex = 0; classIndex < 96; ++classIndex)
            for (int studentIndex = 0; studentIndex < 25; ++studentIndex)
                if (!actual.contains(QStringLiteral("%1-%2").arg(classIndex + 1).arg(studentIndex + 1))) ++missing;
        fixture.insert("expectedStudentNames", 96 * 25);
        fixture.insert("exactStudentNamesFound", found);
        fixture.insert("missingStudentNames", missing);
        fixture.insert("expectedHeaderBullets", 96);
        fixture.insert("actualHeaderBullets", text.count(QChar(0x2022)));
        fixture.insert("independentSourceTextPass", missing == 0 && found == 2400 && text.count(QChar(0x2022)) == 96);
        fixtures.append(fixture);
    }
    const QJsonObject manifest{{"taskId", "phase7_c53_subprep_output_implementation_01"},
        {"qtVersion", qVersion()}, {"platformPlugin", QGuiApplication::platformName()},
        {"fontSha256", QString::fromLatin1(QCryptographicHash::hash(fontBytes, QCryptographicHash::Sha256).toHex())},
        {"fontFamily", QFontInfo(font).family()}, {"fontStyle", QFontInfo(font).styleName()},
        {"fontWeightAxis", 400}, {"variants", variants},
        {"derivedSubstitutions", substitutions}, {"retainedC44IndependentChecks", fixtures}};
    QFile output(root.filePath(QStringLiteral("manifest.json")));
    if (!output.open(QIODevice::WriteOnly) || output.write(QJsonDocument(manifest).toJson()) < 0) qFatal("Could not write manifest");
    return 0; // Diagnostic records failed candidates; it never blesses a correction.
}
