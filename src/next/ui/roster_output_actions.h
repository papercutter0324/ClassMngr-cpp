#pragma once
#include "next/application/file_dialog_port.h"
#include "next/output/roster_output.h"
#include "next/platform/pdf_print_operation.h"
#include "next/platform/roster_pdf_preview_session.h"
#include "shared/roster/roster_template_print_truncation.h"
#include <QPageLayout>
#include <functional>
class QWidget;
namespace ClassMngr::Next::Ui
{
struct RosterOutputActions final
{
    std::function<Output::RosterOutputChoicesResult(const QString&, const Domain::ClassId&, bool)> readChoices;
    std::function<std::expected<RosterTemplatePrintTruncation::Summary, QString>(
        const QString&, const Output::RosterOutputRequest&)> inspectTruncation;
    std::function<DocumentOutputResult(const QString&, const Output::RosterOutputRequest&, const QString&,
        const RosterTemplateRenderer::Fonts&)> savePdf;
    using SaveFileDialogResponse = Application::FileDialogResponse<
        Application::SavedFileDialogSelection>;
    std::function<Domain::Result<SaveFileDialogResponse>(
        QWidget*, const Application::SaveFileDialogRequest&)> saveFileDialog;
    Platform::PdfPrintOperationFactory printOperationFactory;
    Platform::RosterPdfPreviewSessionFactory pdfPreviewSessionFactory;
    RosterTemplateRenderer::Fonts fonts;
};
}
