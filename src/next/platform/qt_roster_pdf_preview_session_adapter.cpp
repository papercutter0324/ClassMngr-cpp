#include "next/platform/roster_pdf_preview_session.h"

#include <QPdfDocument>
#include <QTemporaryDir>

namespace ClassMngr::Next::Platform
{
namespace
{

class QtRosterPdfPreviewSession final : public RosterPdfPreviewSession
{
public:
    QtRosterPdfPreviewSession() = default;
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
        const QPdfDocument::Error error = m_document.load(path());
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
        m_document.close();
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
        return m_document.render(pageIndex, target);
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
    [[nodiscard]] bool isReady() const noexcept
    {
        return isValid()
            && m_document.status() == QPdfDocument::Status::Ready
            && m_document.pageCount() > 0;
    }

    QTemporaryDir m_temporary;
    QPdfDocument m_document;
};

} // namespace

std::unique_ptr<RosterPdfPreviewSession> makeQtRosterPdfPreviewSession()
{
    return std::make_unique<QtRosterPdfPreviewSession>();
}

} // namespace ClassMngr::Next::Platform
