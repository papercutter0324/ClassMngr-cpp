#include "pdf_viewer_page_p.h"

void PdfViewerPage::showStatusMessage(
    const QString& message
    )
{
    m_statusLabel->setText(
        message
        );
    m_statusLabel->show();
}

void PdfViewerPage::clearStatusMessage()
{
    m_statusLabel->clear();
    m_statusLabel->hide();
}

QString PdfViewerPage::documentErrorText() const
{
    return documentErrorText(
        static_cast<int>(
            m_document->error()
            )
        );
}

QString PdfViewerPage::documentErrorText(
    int error
    ) const
{
    switch (static_cast<QPdfDocument::Error>(error))
    {
    case QPdfDocument::Error::None:
        return tr("No error.");

    case QPdfDocument::Error::Unknown:
        return tr("Unknown error.");

    case QPdfDocument::Error::DataNotYetAvailable:
        return tr("The PDF data is not available yet.");

    case QPdfDocument::Error::FileNotFound:
        return tr("The file was not found.");

    case QPdfDocument::Error::InvalidFileFormat:
        return tr("The file is not a valid PDF.");

    case QPdfDocument::Error::IncorrectPassword:
        return tr("The PDF password is incorrect.");

    case QPdfDocument::Error::UnsupportedSecurityScheme:
        return tr("The PDF uses an unsupported security scheme.");
    }

    return tr("Unknown error.");
}
