#include "core/qt_pdf_generation_adapter.h"
#include "core/process_memory_diagnostics.h"

#include <QBuffer>
#include <QPainter>
#include <QPdfWriter>
#include <algorithm>
#include <utility>

namespace {
class QtPdfGenerationSession final : public PdfGenerationSession
{
public:
    explicit QtPdfGenerationSession(
        const QString& path, std::shared_ptr<ProcessMemoryDiagnostics> diagnostics)
        : m_diagnostics(std::move(diagnostics)), writer(std::make_unique<QPdfWriter>(path))
    { record(QStringLiteral("pdf-writer-create")); }
    explicit QtPdfGenerationSession(
        QIODevice& output, std::shared_ptr<ProcessMemoryDiagnostics> diagnostics)
        : m_diagnostics(std::move(diagnostics)), writer(std::make_unique<QPdfWriter>(&output))
    { record(QStringLiteral("pdf-writer-create")); }
    ~QtPdfGenerationSession() override
    {
        record(QStringLiteral("pdf-writer-release-before"));
        writer.reset();
        record(QStringLiteral("pdf-writer-release-after"));
    }
    bool configure(const PdfGenerationSpec& spec)
    {
        if (spec.creator) writer->setCreator(*spec.creator);
        if (spec.title) writer->setTitle(*spec.title);
        if (spec.resolutionDpi) writer->setResolution(*spec.resolutionDpi);
        return !spec.pageLayout || writer->setPageLayout(*spec.pageLayout);
    }
    QPaintDevice& paintDevice() override { return *writer; }
    QPageLayout pageLayout() const override { return writer->pageLayout(); }
    int resolution() const override { return writer->resolution(); }
    QRect pageRectPixels() const override
    {
        return writer->pageLayout().fullRectPixels(std::max(1, writer->resolution()));
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
    bool newPage() override
    {
        const bool result = writer->newPage();
        if (result) ++m_newPageCount;
        ++m_newPageAttempts;
        if (m_newPageAttempts <= 32)
            record(result ? QStringLiteral("pdf-writer-new-page")
                          : QStringLiteral("pdf-writer-new-page-failed"));
        return result;
    }
private:
    void record(const QString& event) noexcept
    {
        try
        {
            const QJsonObject details{
                {QStringLiteral("writer_present"), bool(writer)},
                {QStringLiteral("successful_new_page_count"), m_newPageCount},
                {QStringLiteral("new_page_attempt_count"), static_cast<qint64>(m_newPageAttempts)},
                {QStringLiteral("new_page_detail_limit"), 32},
                {QStringLiteral("new_page_details_suppressed"), static_cast<qint64>(
                    m_newPageAttempts > 32 ? m_newPageAttempts - 32 : 0)},
                {QStringLiteral("completion_meaning"), QStringLiteral(
                    "Writer object lifetime; release does not assert output success")}
            };
            if (m_diagnostics)
                m_diagnostics->record(event, QStringLiteral("pdf_generation"), m_id, details);
            else
                recordProcessMemoryDiagnostic(event, QStringLiteral("pdf_generation"), m_id, details);
        }
        catch (...) { }
    }
    std::shared_ptr<ProcessMemoryDiagnostics> m_diagnostics;
    quint64 m_id = nextProcessMemoryDiagnosticId();
    int m_newPageCount = 0;
    quint64 m_newPageAttempts = 0;
    std::unique_ptr<QPdfWriter> writer;
};
class QtPdfGenerationPort final : public PdfGenerationPort
{
public:
    explicit QtPdfGenerationPort(std::shared_ptr<ProcessMemoryDiagnostics> diagnostics)
        : m_diagnostics(std::move(diagnostics)) {}
    std::unique_ptr<PdfGenerationSession> create(
        const QString& path, const PdfGenerationSpec& spec) override
    {
        auto session = std::make_unique<QtPdfGenerationSession>(path, m_diagnostics);
        if (!session->configure(spec)) return {};
        return session;
    }
    std::unique_ptr<PdfGenerationSession> create(
        QIODevice& output, const PdfGenerationSpec& spec) override
    {
        auto session = std::make_unique<QtPdfGenerationSession>(output, m_diagnostics);
        if (!session->configure(spec)) return {};
        return session;
    }
private:
    std::shared_ptr<ProcessMemoryDiagnostics> m_diagnostics;
};
}
std::shared_ptr<PdfGenerationPort> makeQtPdfGenerationPort(
    std::shared_ptr<ProcessMemoryDiagnostics> diagnostics)
{
    return std::make_shared<QtPdfGenerationPort>(std::move(diagnostics));
}
