#include "core/qt_pdf_generation_adapter.h"

#include <QBuffer>
#include <QPainter>
#include <QPdfWriter>
#include <algorithm>

namespace {
class QtPdfGenerationSession final : public PdfGenerationSession
{
public:
    explicit QtPdfGenerationSession(const QString& path) : writer(path) {}
    explicit QtPdfGenerationSession(QIODevice& output) : writer(&output) {}
    bool configure(const PdfGenerationSpec& spec)
    {
        if (spec.creator) writer.setCreator(*spec.creator);
        if (spec.title) writer.setTitle(*spec.title);
        if (spec.resolutionDpi) writer.setResolution(*spec.resolutionDpi);
        return !spec.pageLayout || writer.setPageLayout(*spec.pageLayout);
    }
    QPaintDevice& paintDevice() override { return writer; }
    QPageLayout pageLayout() const override { return writer.pageLayout(); }
    int resolution() const override { return writer.resolution(); }
    QRect pageRectPixels() const override
    {
        return writer.pageLayout().fullRectPixels(std::max(1, writer.resolution()));
    }
    QPaintEngine::PaintEngineFeatures paintEngineFeatures() const override
    {
        QPaintEngine::PaintEngineFeatures result;
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly)) return result;
        QPdfWriter probeWriter(&buffer);
        QPainter probe(&probeWriter);
        if (!probe.isActive()) return result;
        const QPaintEngine* engine = probe.paintEngine();
        if (!engine) return result;
        for (unsigned bit = 0; bit < 32; ++bit) {
            const auto feature = QPaintEngine::PaintEngineFeatures::fromInt(1U << bit);
            if (engine->hasFeature(feature)) result |= feature;
        }
        probe.end();
        return result;
    }
    bool newPage() override { return writer.newPage(); }
private:
    QPdfWriter writer;
};
class QtPdfGenerationPort final : public PdfGenerationPort
{
public:
    std::unique_ptr<PdfGenerationSession> create(
        const QString& path, const PdfGenerationSpec& spec) override
    {
        auto session = std::make_unique<QtPdfGenerationSession>(path);
        if (!session->configure(spec)) return {};
        return session;
    }
    std::unique_ptr<PdfGenerationSession> create(
        QIODevice& output, const PdfGenerationSpec& spec) override
    {
        auto session = std::make_unique<QtPdfGenerationSession>(output);
        if (!session->configure(spec)) return {};
        return session;
    }
};
}
std::shared_ptr<PdfGenerationPort> makeQtPdfGenerationPort()
{
    return std::make_shared<QtPdfGenerationPort>();
}
