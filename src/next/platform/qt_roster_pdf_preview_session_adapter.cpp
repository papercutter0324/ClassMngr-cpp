#include "next/platform/roster_pdf_preview_session.h"
#include "core/process_memory_diagnostics.h"

#include <QPdfDocument>
#include <QTemporaryDir>

#include <algorithm>

namespace ClassMngr::Next::Platform
{
namespace
{

class QtRosterPdfPreviewSession final : public RosterPdfPreviewSession
{
public:
    QtRosterPdfPreviewSession()
    {
        record(QStringLiteral("preview-session-create"));
        const quint64 id = m_id;
        QObject::connect(&m_document, &QObject::destroyed, [id] {
            recordProcessMemoryDiagnostic(QStringLiteral("preview-document-destroyed"),
                QStringLiteral("roster_pdf_preview"), id);
        });
    }
    QtRosterPdfPreviewSession(const QtRosterPdfPreviewSession&) = delete;
    QtRosterPdfPreviewSession& operator=(const QtRosterPdfPreviewSession&) = delete;

    ~QtRosterPdfPreviewSession() override
    {
        release();
    }

    [[nodiscard]] bool isValid() const noexcept override
    {
        return m_temporary.isValid();
    }

    [[nodiscard]] QString path() const override
    {
        return m_temporary.filePath(QStringLiteral("Rosters.pdf"));
    }

    [[nodiscard]] bool loadGeneratedPdf() override
    {
        record(QStringLiteral("preview-load-before"));
        const QPdfDocument::Error error = m_document.load(path());
        record(QStringLiteral("preview-load-after"));
        if (error != QPdfDocument::Error::None
            || m_document.status() != QPdfDocument::Status::Ready
            || m_document.pageCount() <= 0)
        {
            m_document.close();
            return false;
        }
        return true;
    }

    void release() noexcept override
    {
        record(QStringLiteral("preview-close-before"));
        m_document.close();
        record(QStringLiteral("preview-close-after"));
    }

    [[nodiscard]] int pageCount() const noexcept override
    {
        return isReady() ? m_document.pageCount() : 0;
    }

    [[nodiscard]] QImage renderPage(
        int pageIndex,
        const QSize& maximumSize
        ) override
    {
        if (!isReady() || pageIndex < 0 || pageIndex >= m_document.pageCount())
        {
            return {};
        }
        const QSize target = m_document.pagePointSize(pageIndex)
            .toSize()
            .scaled(maximumSize, Qt::KeepAspectRatio);
        if (target.width() <= 0 || target.height() <= 0)
        {
            return {};
        }
        const quint64 renderId = nextProcessMemoryDiagnosticId();
        ++m_renderCount;
        const auto renderRecord = [&](const QString& phase, const QImage* image = nullptr) noexcept {
            if (m_renderCount > 32) return;
            try
            {
                recordProcessMemoryDiagnostic(QStringLiteral("qpdf-preview-render"),
                    QStringLiteral("roster_pdf_preview"), renderId, {
                        {QStringLiteral("session_id"), static_cast<qint64>(m_id)},
                        {QStringLiteral("phase"), phase},
                        {QStringLiteral("page_index"), pageIndex},
                        {QStringLiteral("target_width_pixels"), target.width()},
                        {QStringLiteral("target_height_pixels"), target.height()},
                        {QStringLiteral("image_present"), image && !image->isNull()},
                        {QStringLiteral("decoded_pixel_bytes"), image
                            ? QJsonValue(image->sizeInBytes()) : QJsonValue(QJsonValue::Null)},
                        {QStringLiteral("image_owner_after_return"), QStringLiteral("caller")}
                    });
            }
            catch (...) { }
        };
        renderRecord(QStringLiteral("before"));
        QImage image = m_document.render(pageIndex, target);
        m_maximumRenderBytes = std::max(m_maximumRenderBytes, image.sizeInBytes());
        renderRecord(image.isNull() ? QStringLiteral("failed") : QStringLiteral("after"), &image);
        return image;
    }

    [[nodiscard]] PdfPrintResult printReadyDocument(
        PdfPrintOperation& operation,
        const RosterPdfPrintRequest& request
        ) override
    {
        if (!isReady())
        {
            return {PdfPrintStatus::Failed, {}};
        }
        return operation.printReadyDocument({
            .parent = request.parent,
            .document = &m_document,
            .documentPath = path(),
            .currentPageIndex = 0,
            .dialogTitle = request.dialogTitle,
            .pageOrientation = request.pageOrientation,
            .fitToPageByDefault = request.fitToPageByDefault,
            .preferredPageSize = request.preferredPageSize,
            .lockPreferredPageSize = request.lockPreferredPageSize,
            .uiFont = request.uiFont,
            .dialogGeometryPreferences = request.dialogGeometryPreferences
        });
    }

private:
    void record(const QString& event) const noexcept
    {
        try
        {
            recordProcessMemoryDiagnostic(event, QStringLiteral("roster_pdf_preview"), m_id, {
                {QStringLiteral("status"), static_cast<int>(m_document.status())},
                {QStringLiteral("error"), static_cast<int>(m_document.error())},
                {QStringLiteral("page_count"), m_document.pageCount()},
                {QStringLiteral("render_count"), static_cast<qint64>(m_renderCount)},
                {QStringLiteral("render_detail_limit"), 32},
                {QStringLiteral("render_details_suppressed"), static_cast<qint64>(
                    m_renderCount > 32 ? m_renderCount - 32 : 0)},
                {QStringLiteral("maximum_decoded_pixel_bytes"), m_maximumRenderBytes}
            });
        }
        catch (...) { }
    }
    [[nodiscard]] bool isReady() const noexcept
    {
        return isValid()
            && m_document.status() == QPdfDocument::Status::Ready
            && m_document.pageCount() > 0;
    }

    quint64 m_id = nextProcessMemoryDiagnosticId();
    quint64 m_renderCount = 0;
    qsizetype m_maximumRenderBytes = 0;
    QTemporaryDir m_temporary;
    QPdfDocument m_document;
};

} // namespace

std::unique_ptr<RosterPdfPreviewSession> makeQtRosterPdfPreviewSession()
{
    return std::make_unique<QtRosterPdfPreviewSession>();
}

} // namespace ClassMngr::Next::Platform
