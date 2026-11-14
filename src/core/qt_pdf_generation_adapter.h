#pragma once
#include "core/pdf_generation_port.h"

[[nodiscard]] std::shared_ptr<PdfGenerationPort> makeQtPdfGenerationPort();
