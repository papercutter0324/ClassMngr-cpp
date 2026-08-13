#include "pdf_viewer_page_p.h"

void PdfViewerPage::exportFile()
{
    const QString sourcePath =
        exportSourcePath();

    if (sourcePath.trimmed().isEmpty())
    {
        QMessageBox::warning(
            this,
            tr("Export File"),
            tr("No file is available to export.")
            );
        return;
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString exportFileName =
        m_documentDescriptor.exportFileName.trimmed().isEmpty()
            ? sourceInfo.fileName()
            : m_documentDescriptor.exportFileName;

    const QString suggestedPath =
        QDir(
            defaultExportDirectory()
            ).filePath(
                exportFileName
                );

    const std::optional<SaveFileSelection> selection =
        DialogServices::fileDialogs().saveFileWithOptions(
            SaveFileRequest{
                .parent = this,
                .title = tr("Export File"),
                .purpose = FileDialogPurpose::GeneratedPdf,
                .initialDirectory = QFileInfo(suggestedPath).absolutePath(),
                .suggestedFileName = QFileInfo(suggestedPath).fileName(),
                .nameFilters = exportFileFilter(sourceInfo.suffix())
                    .split(QStringLiteral(";;")),
                .defaultSuffix = sourceInfo.suffix(),
                .openAfterSavingText = tr("Open after saving")
            }
            );

    if (!selection)
    {
        return;
    }

    QString targetPath = selection->path;

    if (
        !sourceInfo.suffix().isEmpty()
        && QFileInfo(targetPath).suffix().isEmpty()
        )
    {
        targetPath +=
            QStringLiteral(".%1")
                .arg(sourceInfo.suffix());
    }

    QString errorMessage;

    if (
        !copyFileTo(
            sourcePath,
            targetPath,
            &errorMessage
            )
        )
    {
        QMessageBox::warning(
            this,
            tr("Export File"),
            errorMessage
            );
        return;
    }

    if (
        selection->openAfterSaving
        && !QDesktopServices::openUrl(
            QUrl::fromLocalFile(targetPath)
            )
        )
    {
        QMessageBox::warning(
            this,
            tr("Open File"),
            tr("Unable to open the exported file:\n%1")
                .arg(targetPath)
            );
    }
}

void PdfViewerPage::printFile()
{
    int currentPageIndex =
        0;
    const int pageCount =
        m_document ? m_document->pageCount() : 0;

    if (
        pageCount > 0
        &&
        m_view
        && m_view->pageNavigator()
        )
    {
        currentPageIndex =
            std::clamp(
                m_view->pageNavigator()->currentPage(),
                0,
                pageCount - 1
                );
    }

    const PdfPrintService::Result result =
        PdfPrintService::printPdfDocument(
            {
                this,
                m_document,
                m_currentFilePath,
                currentPageIndex,
                tr("Print File")
            }
            );

    if (result.status == PdfPrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            tr("Print File"),
            result.message
            );
        return;
    }

    if (result.status == PdfPrintService::Status::Sent)
    {
        showStatusMessage(
            result.message
            );
    }
}
