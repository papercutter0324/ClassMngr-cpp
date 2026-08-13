#pragma once

#include "domain/models/document_output_result.h"
#include "features/sub_prep/services/sub_prep_document_model.h"

#include <QString>

namespace SubPrepPdfRenderer
{
using Status = DocumentOutputStatus;
using Result = DocumentOutputResult;

[[nodiscard]] Result renderPdf(
    const SubPrepDocumentModel::Document& document,
    const QString& documentPath
    );
}
