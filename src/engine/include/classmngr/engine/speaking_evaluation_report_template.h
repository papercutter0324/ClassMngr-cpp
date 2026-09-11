#pragma once

#include <string_view>

namespace classmngr::engine
{

enum class SpeakingEvaluationReportTemplate
{
    Standard,
    Advanced
};

struct SpeakingEvaluationReportRectangle
{
    double left = 0.0;
    double top = 0.0;
    double width = 0.0;
    double height = 0.0;
};

// Renderer-neutral policy for the built-in speaking-evaluation templates.
// Resource identifiers are relative to the presentation adapter's documents
// pack; the engine does not resolve platform-specific paths.
struct SpeakingEvaluationReportTemplatePolicy
{
    SpeakingEvaluationReportTemplate reportTemplate =
        SpeakingEvaluationReportTemplate::Standard;
    double pageWidth = 540.0;
    double pageHeight = 780.0;
    SpeakingEvaluationReportRectangle signatureBounds;
    std::string_view powerPointResourcePath;
    bool usesAdvancedScoreTable = false;
    bool signatureAlignsBottomLeft = false;
    bool scoreTableOnMaster = true;
    std::string_view scoreTableName;
    int minimumTableRows = 12;
    int minimumTableColumns = 6;
    int firstGradeColumn = 2;
    int neutralFillRed = 217;
    int neutralFillGreen = 217;
    int neutralFillBlue = 217;
};

class SpeakingEvaluationReportTemplateService final
{
public:
    [[nodiscard]] static const SpeakingEvaluationReportTemplatePolicy&
    policy(
        SpeakingEvaluationReportTemplate reportTemplate
        );
};

} // namespace classmngr::engine
