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

    const QString suggestedPath =
        QDir(
            defaultExportDirectory()
            ).filePath(
                sourceInfo.fileName()
                );

    QFileDialog dialog(
        this,
        tr("Export File"),
        QFileInfo(suggestedPath).absolutePath(),
        exportFileFilter(
            sourceInfo.suffix()
            )
        );
    dialog.setAcceptMode(
        QFileDialog::AcceptSave
        );
    dialog.setFileMode(
        QFileDialog::AnyFile
        );
    dialog.setOption(
        QFileDialog::DontUseNativeDialog,
        true
        );
    dialog.setDefaultSuffix(
        sourceInfo.suffix()
        );
    dialog.setLabelText(
        QFileDialog::LookIn,
        tr("Look in:")
        );
    dialog.setLabelText(
        QFileDialog::FileName,
        tr("File name:")
        );
    dialog.setLabelText(
        QFileDialog::FileType,
        tr("Files of type:")
        );
    dialog.setLabelText(
        QFileDialog::Accept,
        tr("Save")
        );
    dialog.setLabelText(
        QFileDialog::Reject,
        tr("Cancel")
        );
    dialog.selectFile(
        QFileInfo(suggestedPath).fileName()
        );

    auto* openAfterSavingCheck =
        new QCheckBox(
            tr("Open after saving"),
            &dialog
            );

    if (auto* gridLayout = qobject_cast<QGridLayout*>(dialog.layout()))
    {
        gridLayout->addWidget(
            openAfterSavingCheck,
            gridLayout->rowCount(),
            0,
            1,
            gridLayout->columnCount()
            );
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedFiles =
        dialog.selectedFiles();

    if (selectedFiles.isEmpty())
    {
        return;
    }

    QString targetPath =
        selectedFiles.first();

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
        openAfterSavingCheck->isChecked()
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

