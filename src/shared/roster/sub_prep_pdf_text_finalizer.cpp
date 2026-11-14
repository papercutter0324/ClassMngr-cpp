#include "shared/roster/sub_prep_pdf_text_finalizer.h"
#include "core/pdf_generation_port.h"

#include <QHash>
#include <QMap>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QTextLayout>
#include <QGlyphRun>
#include <QSet>
#include <algorithm>
#include <limits>

namespace SubPrepPdfText {
namespace {
QString problem(const QString& detail) {
    return QStringLiteral("Sub Prep PDF text mapping could not be finalized: %1").arg(detail);
}

bool supportedSource(const QTextItem& item, QPaintDevice* device) {
    if (item.renderFlags().testFlag(QTextItem::RightToLeft)) return false;
    const QString text = item.text();
    for (const QChar character : text) {
        const auto category = character.category();
        if (character.isSurrogate() || character.unicode() < 32
            || category == QChar::Mark_NonSpacing || category == QChar::Mark_SpacingCombining
            || category == QChar::Mark_Enclosing || category == QChar::Other_Format)
            return false;
        // These scripts have no positional source reordering in this supported
        // one-character/one-glyph path. Complex-script clusters require actual
        // Qt cluster metadata, which the public QTextItem API does not expose.
        const auto script = character.script();
        if (script != QChar::Script_Common && script != QChar::Script_Latin
            && script != QChar::Script_Greek && script != QChar::Script_Cyrillic
            && script != QChar::Script_Hangul && script != QChar::Script_Han
            && script != QChar::Script_Hiragana && script != QChar::Script_Katakana)
            return false;
    }
    QTextLayout layout(text, item.font(), device);
    layout.beginLayout();
    auto line = layout.createLine();
    if (!line.isValid()) { layout.endLayout(); return false; }
    line.setLineWidth(0x01000000);
    layout.endLayout();
    QList<qsizetype> indexes;
    for (const auto& run : layout.glyphRuns(0, -1, QTextLayout::RetrieveGlyphIndexes | QTextLayout::RetrieveStringIndexes)) {
        if (run.isRightToLeft() || run.glyphIndexes().size() != run.stringIndexes().size()) return false;
        for (const auto index : run.stringIndexes()) indexes.append(index);
    }
    if (indexes.size() != text.size()) return false;
    for (qsizetype index = 0; index < indexes.size(); ++index)
        if (indexes.at(index) != index) return false;
    return true;
}

class ForwardEngine final : public QPaintEngine {
public:
    ForwardEngine(PdfGenerationSession& writer, SourceJournal& source)
        : QPaintEngine(writer.paintEngineFeatures()), target(&writer.paintDevice()), journal(source) {}
    Type type() const override { return Pdf; }
    bool begin(QPaintDevice*) override { return target.isActive(); }
    bool end() override { return true; }
    void updateState(const QPaintEngineState& state) override {
        const auto* source = state.painter();
        target.setWorldTransform(source->combinedTransform());
        target.setPen(source->pen()); target.setBrush(source->brush());
        target.setBrushOrigin(source->brushOrigin()); target.setFont(source->font());
        target.setBackground(source->background()); target.setBackgroundMode(source->backgroundMode());
        target.setCompositionMode(source->compositionMode()); target.setOpacity(source->opacity());
        target.setRenderHints(target.renderHints(), false); target.setRenderHints(source->renderHints());
        target.setClipping(false);
        if (source->hasClipping()) target.setClipPath(source->clipPath(), Qt::ReplaceClip);
    }
    void drawTextItem(const QPointF& position, const QTextItem& item) override {
        if (journal.error.isEmpty() && !supportedSource(item, paintDevice()))
            journal.error = problem(QStringLiteral("unsupported text shaping or source order"));
        journal.pages.last().append(item.text());
        target.drawTextItem(position, item);
    }
    void drawRects(const QRect* r, int n) override { target.drawRects(r, n); }
    void drawRects(const QRectF* r, int n) override { target.drawRects(r, n); }
    void drawLines(const QLine* r, int n) override { target.drawLines(r, n); }
    void drawLines(const QLineF* r, int n) override { target.drawLines(r, n); }
    void drawEllipse(const QRect& r) override { target.drawEllipse(r); }
    void drawEllipse(const QRectF& r) override { target.drawEllipse(r); }
    void drawPath(const QPainterPath& p) override { target.drawPath(p); }
    void drawPoints(const QPoint* p, int n) override { target.drawPoints(p, n); }
    void drawPoints(const QPointF* p, int n) override { target.drawPoints(p, n); }
    void drawPolygon(const QPointF* p, int n, PolygonDrawMode mode) override {
        if (mode == PolylineMode) target.drawPolyline(p, n);
        else if (mode == ConvexMode) target.drawConvexPolygon(p, n);
        else target.drawPolygon(p, n, mode == WindingMode ? Qt::WindingFill : Qt::OddEvenFill);
    }
    void drawPolygon(const QPoint* p, int n, PolygonDrawMode mode) override {
        if (mode == PolylineMode) target.drawPolyline(p, n);
        else if (mode == ConvexMode) target.drawConvexPolygon(p, n);
        else target.drawPolygon(p, n, mode == WindingMode ? Qt::WindingFill : Qt::OddEvenFill);
    }
    void drawPixmap(const QRectF& r, const QPixmap& p, const QRectF& s) override { target.drawPixmap(r, p, s); }
    void drawImage(const QRectF& r, const QImage& p, const QRectF& s, Qt::ImageConversionFlags f) override { target.drawImage(r, p, s, f); }
    void drawTiledPixmap(const QRectF& r, const QPixmap& p, const QPointF& s) override { target.drawTiledPixmap(r, p, s); }
    QPainter target;
    SourceJournal& journal;
};
}

struct CaptureDevice::Impl {
    Impl(PdfGenerationSession& writer, SourceJournal& source) : writer(writer), engine(writer, source) {}
    PdfGenerationSession& writer;
    ForwardEngine engine;
};
CaptureDevice::CaptureDevice(PdfGenerationSession& writer, SourceJournal& source) : m_impl(std::make_unique<Impl>(writer, source)) {}
CaptureDevice::~CaptureDevice() = default;
QPaintEngine* CaptureDevice::paintEngine() const { return &m_impl->engine; }
bool CaptureDevice::finish() { return m_impl->engine.target.end(); }
bool CaptureDevice::newPage() {
    if (!m_impl->writer.newPage()) return false;
    m_impl->engine.journal.pages.append(QList<QString>{});
    return true;
}
int CaptureDevice::metric(PaintDeviceMetric which) const {
    const auto& writer = m_impl->writer.paintDevice();
    switch (which) {
    case PdmWidth: return writer.width(); case PdmHeight: return writer.height();
    case PdmWidthMM: return writer.widthMM(); case PdmHeightMM: return writer.heightMM();
    case PdmDpiX: return writer.logicalDpiX(); case PdmDpiY: return writer.logicalDpiY();
    case PdmPhysicalDpiX: return writer.physicalDpiX(); case PdmPhysicalDpiY: return writer.physicalDpiY();
    case PdmDepth: return writer.depth(); case PdmNumColors: return writer.colorCount();
    case PdmDevicePixelRatio: return 1; case PdmDevicePixelRatioScaled: return int(devicePixelRatioFScale());
    case PdmDevicePixelRatioF_EncodedA: case PdmDevicePixelRatioF_EncodedB: return encodeMetricF(which, 1.0);
    }
    return 0;
}


namespace {
struct Rejected { QString reason; };
void require(bool condition, const char* reason) { if (!condition) throw Rejected{QString::fromLatin1(reason)}; }
int number(const QString& text) {
    bool ok = false; const int value = text.toInt(&ok);
    require(ok && value >= 0, "invalid PDF number"); return value;
}
QRegularExpressionMatch uniqueMatch(const QString& text, const QString& pattern) {
    QRegularExpression expression(pattern);
    auto matches = expression.globalMatch(text);
    require(matches.hasNext(), "required Qt PDF field is missing");
    const auto result = matches.next();
    require(!matches.hasNext(), "ambiguous Qt PDF field"); return result;
}
QString latin(const QByteArray& bytes) { return QString::fromLatin1(bytes); }
struct PdfObject { qint64 offset = 0; QByteArray body; };
class QtPdf {
public:
    explicit QtPdf(const QByteArray& bytes) : bytes(bytes) {
        require(bytes.size() < 64 * 1024 * 1024 && bytes.startsWith("%PDF-1.4\n"), "unsupported PDF size/version");
        const auto end = uniqueMatch(latin(bytes.right(100)), QStringLiteral("startxref\\s+(\\d+)\\s+%%EOF\\s*$"));
        oldXref = number(end.captured(1));
        require(oldXref > 0 && oldXref < bytes.size(), "invalid xref offset");
        const QByteArray tail = bytes.mid(oldXref);
        if (!tail.startsWith("xref\n0 "))
            throw Rejected{QStringLiteral("unsupported xref layout (%1)").arg(QString::fromLatin1(tail.left(16).toHex()))};
        const qsizetype trailerPosition = tail.indexOf("trailer\n");
        require(trailerPosition > 0, "missing classic trailer");
        const auto lines = tail.left(trailerPosition).split('\n');
        require(lines.size() >= 4 && lines.first() == "xref", "unsupported xref table");
        size = number(latin(lines.at(1).mid(2)));
        require(size > 1 && size <= 100000 && lines.size() == size + 3, "unsupported xref count");
        require(lines.at(2).trimmed() == "0000000000 65535 f", "invalid free xref entry");
        QList<QPair<qint64, int>> ordered;
        for (int id = 1; id < size; ++id) {
            const auto match = uniqueMatch(latin(lines.at(id + 2)), QStringLiteral("^(\\d{10}) 00000 n ?$"));
            const int offset = number(match.captured(1));
            require(offset > 0 && offset < oldXref, "xref entry outside Qt object area");
            objects.insert(id, {offset, {}}); ordered.append({offset, id});
        }
        std::sort(ordered.begin(), ordered.end());
        for (qsizetype index = 0; index < ordered.size(); ++index) {
            const auto [offset, id] = ordered.at(index);
            const qint64 next = index + 1 < ordered.size() ? ordered.at(index + 1).first : oldXref;
            require(next > offset, "duplicate xref offset");
            QByteArray raw = bytes.mid(offset, next - offset).trimmed();
            const QByteArray header = QByteArray::number(id) + " 0 obj\n";
            require(raw.startsWith(header) && raw.endsWith("endobj"), "invalid Qt object boundary");
            raw = raw.mid(header.size()); raw.chop(6); objects[id].body = raw.trimmed();
        }
        const QString trailerTail = latin(tail.mid(trailerPosition + 8));
        const auto trailerMatch = uniqueMatch(trailerTail, QStringLiteral("^\\s*(<<[\\s\\S]*?>>)\\s*startxref\\s+\\d+\\s+%%EOF\\s*$"));
        trailer = trailerMatch.captured(1).toLatin1();
        require(!trailer.contains("/Encrypt") && !trailer.contains("/Prev") && !trailer.contains("/XRefStm"), "encrypted or incremental PDF is unsupported");
        require(number(uniqueMatch(latin(trailer), QStringLiteral("/Size\\s+(\\d+)\\b")).captured(1)) == size, "trailer size mismatch");
        root = reference(trailer, "Root");
        const int info = reference(trailer, "Info");
        const auto infoBody = body(info);
        require(infoBody.contains(QByteArray::fromHex("feff0051007400200036002e00310032002e0030")), "PDF is not from the supported Qt 6.12.0 writer");
        for (const auto& object : std::as_const(objects)) {
            const QByteArray dictionary = object.body.left(object.body.indexOf("\nstream\n") < 0 ? object.body.size() : object.body.indexOf("\nstream\n"));
            require(!dictionary.contains("/ObjStm") && !dictionary.contains("/Type /XRef") && !dictionary.contains("/Encrypt"), "unsupported PDF object structure");
        }
    }
    QByteArray body(int id) const { require(objects.contains(id), "unknown PDF object reference"); return objects.value(id).body; }
    int reference(const QByteArray& dictionary, const char* name) const {
        const auto match = uniqueMatch(latin(dictionary), QStringLiteral("/%1\\s+(\\d+)\\s+0\\s+R\\b").arg(QString::fromLatin1(name)));
        const int id = number(match.captured(1)); require(objects.contains(id), "dangling PDF reference"); return id;
    }
    QByteArray stream(int id) const {
        const auto object = body(id); const qsizetype start = object.indexOf("\nstream\n");
        require(start > 0, "expected Qt stream object");
        const auto dictionary = object.left(start);
        require(!dictionary.contains("/DecodeParms") && !dictionary.contains("/DP"), "unsupported stream decode parameters");
        const auto length = uniqueMatch(latin(dictionary), QStringLiteral("/Length\\s+(\\d+)(?:\\s+(\\d+)\\s+R)?\\b"));
        int count = number(length.captured(1));
        if (!length.captured(2).isEmpty()) {
            require(length.captured(2) == QStringLiteral("0"), "unsupported stream-length generation");
            count = number(latin(body(count)).trimmed());
        }
        require(count >= 0 && start + 8 + count <= object.size(), "invalid stream length");
        require(object.mid(start + 8 + count).trimmed() == "endstream", "stream boundary mismatch");
        QByteArray decoded = object.mid(start + 8, count);
        if (dictionary.contains("/Filter")) {
            require(uniqueMatch(latin(dictionary), QStringLiteral("/Filter\\s+(/\\w+)\\b")).captured(1) == QStringLiteral("/FlateDecode"), "unsupported stream filter");
            QByteArray compressed(4, '\0'); compressed[3] = '\1'; compressed += decoded;
            decoded = qUncompress(compressed);
            require(!decoded.isEmpty() && decoded.size() < 64 * 1024 * 1024, "invalid or excessive Flate stream");
        }
        return decoded;
    }
    QList<int> pages() const {
        const auto catalog = body(root);
        require(catalog.contains("/Type /Catalog"), "unsupported PDF catalog");
        const auto tree = body(reference(catalog, "Pages"));
        require(tree.contains("/Type /Pages"), "unsupported PDF page tree");
        const auto kids = uniqueMatch(latin(tree), QStringLiteral("/Kids\\s*\\[([^]]*)\\]"));
        QString remainder = kids.captured(1);
        QRegularExpression refs(QStringLiteral("(\\d+)\\s+0\\s+R\\b"));
        auto matches = refs.globalMatch(remainder); QList<int> result;
        while (matches.hasNext()) result.append(number(matches.next().captured(1)));
        remainder.remove(refs);
        require(remainder.trimmed().isEmpty() && !result.isEmpty(), "unsupported page references");
        require(number(uniqueMatch(latin(tree), QStringLiteral("/Count\\s+(\\d+)\\b")).captured(1)) == result.size(), "page-tree count mismatch");
        QSet<int> unique;
        for (int page : result) {
            const auto object = body(page);
            require(object.contains("/Type /Page\n") && !unique.contains(page), "nested or repeated page tree is unsupported");
            unique.insert(page);
        }
        return result;
    }
    QHash<QString, int> fonts(const QByteArray& resource) const {
        const auto block = uniqueMatch(latin(resource), QStringLiteral("/Font\\s*<<([^>]*)>>"));
        const QString raw = block.captured(1);
        QRegularExpression entries(QStringLiteral("/(F\\d+)\\s+(\\d+)\\s+0\\s+R\\b"));
        QHash<QString, int> fonts; auto matches = entries.globalMatch(raw);
        while (matches.hasNext()) { const auto entry = matches.next(); require(!fonts.contains(entry.captured(1)), "ambiguous font resource"); fonts.insert(entry.captured(1), number(entry.captured(2))); }
        QString remainder = raw; remainder.remove(entries); require(remainder.trimmed().isEmpty(), "unsupported font resource dictionary");
        return fonts;
    }
    const QByteArray& bytes;
    QMap<int, PdfObject> objects;
    int size = 0, root = 0; qint64 oldXref = 0;
    QByteArray trailer;
};

QByteArray cmap(const QMap<quint16, QChar>& mapping) {
    QByteArray result = "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n"
        "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
        "/CMapName /ClassMngr-SubPrep-Source def\n/CMapType 2 def\n"
        "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n";
    auto it = mapping.cbegin();
    while (it != mapping.cend()) {
        const int count = std::min<qsizetype>(100, std::distance(it, mapping.cend()));
        result += QByteArray::number(count) + " beginbfchar\n";
        for (int i = 0; i < count; ++i, ++it) {
            result += '<' + QByteArray::number(it.key(), 16).rightJustified(4, '0') + "> <"
                + QByteArray::number(it.value().unicode(), 16).rightJustified(4, '0') + ">\n";
        }
        result += "endbfchar\n";
    }
    result += "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n";
    return result;
}
}

std::expected<QByteArray, QString> finalize(const QByteArray& generatedPdf, const SourceJournal& source) {
    if (!source.error.isEmpty()) return std::unexpected(source.error);
    try {
        QtPdf pdf(generatedPdf);
        for (auto it = pdf.objects.cbegin(); it != pdf.objects.cend(); ++it) {
            const QByteArray object = it.value().body;
            const qsizetype streamStart = object.indexOf("\nstream\n");
            const QByteArray dictionary = object.left(streamStart < 0 ? object.size() : streamStart);
            if (streamStart >= 0) {
                static_cast<void>(pdf.stream(it.key()));
            } else {
                require(!dictionary.contains("/Filter"), "stream filter without a stream object");
            }
        }
        const auto pages = pdf.pages();
        require(pages.size() == source.pages.size(), "source/PDF page counts differ");
        QMap<int, QMap<quint16, QChar>> mappings;
        for (qsizetype pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
            const auto page = pdf.body(pages.at(pageIndex));
            const auto fonts = pdf.fonts(pdf.body(pdf.reference(page, "Resources")));
            const auto content = pdf.stream(pdf.reference(page, "Contents"));
            const QString text = latin(content);
            QRegularExpression blocks(QStringLiteral("\\bBT\\s+([\\s\\S]*?)\\bET\\b"));
            auto items = blocks.globalMatch(text); qsizetype itemIndex = 0;
            QString outside = text; outside.remove(blocks);
            require(!outside.contains(QRegularExpression(QStringLiteral("\\b(?:Tj|TJ|BT|ET|Tf)\\b"))), "text outside supported Qt text blocks");
            while (items.hasNext()) {
                const QString block = items.next().captured(1);
                require(itemIndex < source.pages.at(pageIndex).size(), "PDF has uncaptured text items");
                const QString& expected = source.pages.at(pageIndex).at(itemIndex++);
                require(!expected.isEmpty(), "empty source text item");
                const auto fontName = uniqueMatch(block, QStringLiteral("/(F\\d+)\\s+[0-9.]+\\s+Tf\\b")).captured(1);
                require(fonts.contains(fontName), "text uses an unknown font resource");
                const int font = fonts.value(fontName);
                const auto fontObject = pdf.body(font);
                require(fontObject.contains("/Subtype /Type0") && fontObject.contains("/Encoding /Identity-H"), "unsupported text font encoding");
                const int map = pdf.reference(fontObject, "ToUnicode");
                // Ensure this is an actual supported Qt CMap before replacing it.
                const auto originalMap = pdf.stream(map);
                require(originalMap.contains("/CMapType 2 def") && originalMap.contains("begincmap"), "unsupported ToUnicode object");
                require(!block.contains(QRegularExpression(QStringLiteral("\\bTJ\\b|[)\\]]\\s*Tj|['\"]"))), "unsupported text-show operator");
                QRegularExpression shows(QStringLiteral("<([0-9A-Fa-f]+)>\\s+Tj\\b"));
                auto glyphs = shows.globalMatch(block); QList<quint16> codes;
                while (glyphs.hasNext()) {
                    const QString hex = glyphs.next().captured(1);
                    require(hex.size() == 4, "unsupported text code width");
                    bool ok = false; const auto code = hex.toUShort(&ok, 16);
                    require(ok && code != 0, "missing glyph or invalid text code"); codes.append(code);
                }
                QString other = block; other.remove(shows);
                require(!other.contains(QRegularExpression(QStringLiteral("\\bTj\\b"))), "unrecognized text-show syntax");
                require(codes.size() == expected.size() || codes.size() == expected.size() * 2, "source/glyph counts differ; cluster metadata is required");
                if (codes.size() == expected.size() * 2)
                    for (qsizetype i = 0; i < expected.size(); ++i)
                        require(codes.at(i) == codes.at(i + expected.size()), "unsupported repeated glyph painting");
                auto& mapping = mappings[map];
                for (qsizetype i = 0; i < codes.size(); ++i) {
                    const QChar character = expected.at(i % expected.size());
                    require(character.unicode() >= 32 && !character.isSurrogate(), "unsupported source character");
                    const auto code = codes.at(i);
                    require(!mapping.contains(code) || mapping.value(code) == character, "one emitted code has conflicting source characters");
                    mapping.insert(code, character);
                }
            }
            require(itemIndex == source.pages.at(pageIndex).size(), "source contains text items absent from the PDF");
        }
        require(!mappings.isEmpty(), "no source-associated PDF text");
        QByteArray output = generatedPdf;
        if (!output.endsWith('\n')) output += '\n';
        auto objects = pdf.objects;
        for (auto it = mappings.cbegin(); it != mappings.cend(); ++it) {
            const QByteArray stream = cmap(it.value());
            objects[it.key()].offset = output.size();
            output += QByteArray::number(it.key()) + " 0 obj\n<< /Length " + QByteArray::number(stream.size()) + " >>\nstream\n"
                + stream + "endstream\nendobj\n";
        }
        const qint64 xref = output.size();
        output += "xref\n0 " + QByteArray::number(pdf.size) + "\n0000000000 65535 f \n";
        for (int id = 1; id < pdf.size; ++id)
            output += QByteArray::number(objects.value(id).offset).rightJustified(10, '0') + " 00000 n \n";
        QByteArray trailer = pdf.trailer; trailer.chop(2);
        trailer += "/Prev " + QByteArray::number(pdf.oldXref) + "\n>>";
        output += "trailer\n" + trailer + "\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
        return output;
    } catch (const Rejected& error) {
        return std::unexpected(problem(error.reason));
    }
}
}
