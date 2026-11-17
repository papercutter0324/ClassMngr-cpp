#pragma once
#include "core/pdf_generation_port.h"

class ProcessMemoryDiagnostics;

[[nodiscard]] std::shared_ptr<PdfGenerationPort> makeQtPdfGenerationPort(
    std::shared_ptr<ProcessMemoryDiagnostics> diagnostics = {});
