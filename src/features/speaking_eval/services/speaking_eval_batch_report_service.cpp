#include "speaking_eval_batch_report_service.h"
#include "speaking_eval_report_output_policy.h"
#include "speaking_eval_report_output.h"
#include "speaking_eval_report_data_assembler.h"
#include "speaking_eval_report_asset_resolver.h"
#include "speaking_eval_internal_pdf_renderer.h"
#include "speaking_eval_powerpoint_automation.h"
#include "speaking_eval_powerpoint_workspace.h"

#include "core/resource_paths.h"
#include "core/utils/file_name_utils.h"
#include "core/zip_archive_writer.h"
#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTime>

#include <algorithm>

namespace SpeakingEvalBatchReportService
{
namespace
{

constexpr int PowerPointTimeoutMs = 5 * 60 * 1000;
constexpr auto PowerPointCommentFontName = "Segoe UI Semibold";

QString powerPointText(const QString& value)
{
    // macOS input methods can supply Hangul as decomposed Jamo. PowerPoint
    // preserves that representation when text arrives through AppleScript,
    // causing names such as 박지혜 to be displayed as separate characters.
    return value.normalized(QString::NormalizationForm_C);
}

#ifdef Q_OS_MACOS
QString macPowerPointTextArgument(const QString& value)
{
    return QString::fromLatin1(
        powerPointText(value).toUtf8().toBase64()
        );
}
#endif

Result completed(
    const QStringList& savedPdfPaths = {},
    const QString& savedArchivePath = {}
    )
{
    return {
        Status::Completed,
        QObject::tr("Reports created successfully."),
        savedPdfPaths,
        savedArchivePath
    };
}

Result canceled()
{
    return {
        Status::Canceled,
        QObject::tr("Report export was canceled."),
        {},
        {}
    };
}

Result failed(
    const QString& message,
    bool internalRendererFailed = false
    )
{
    return {
        internalRendererFailed
            ? Status::InternalRendererFailed
            : Status::Failed,
        message,
        {},
        {}
    };
}

struct PowerPointTemplateProfile
{
    SpeakingEvalReportTemplate reportTemplate =
        SpeakingEvalReportTemplate::Standard;
    QString resourcePath;
    QRectF signatureBounds;
    bool signatureAlignsBottomLeft = false;
    bool scoreTableOnMaster = true;
    QString scoreTableName;
    int minimumTableRows = 12;
    int minimumTableColumns = 6;
    int firstGradeColumn = 2;
    int neutralFillRed = 217;
    int neutralFillGreen = 217;
    int neutralFillBlue = 217;
};

struct PowerPointStudentJob
{
    QString displayName;
    QString pdfPath;
    QString completionPath;
    QString englishName;
    QString koreanName;
    QString classLabel;
    QString nativeTeacher;
    QString koreanTeacher;
    QString date;
    QString comments;
    qreal commentsFontSizePoints = 0.0;
    QString overallGrade;
    std::array<QString, 6> scores;
};

struct PowerPointBatchJob
{
    PowerPointTemplateProfile templateProfile;
    QByteArray signatureImage;
    QList<PowerPointStudentJob> students;
};

enum class PowerPointBatchStatus
{
    Completed,
    Canceled,
    Failed
};

PowerPointTemplateProfile powerPointTemplateProfile(
    SpeakingEvalReportTemplate reportTemplate
    )
{
    const SpeakingEvalReportTemplateLayout& layout =
        speakingEvalReportTemplateLayout(reportTemplate);

    PowerPointTemplateProfile profile;
    profile.reportTemplate = reportTemplate;
    profile.resourcePath =
        ResourcePaths::Documents::filePath(
            layout.powerPointResourcePath
            );
    profile.signatureBounds = layout.signatureBounds;
    profile.signatureAlignsBottomLeft =
        layout.signatureAlignsBottomLeft;

    if (layout.usesAdvancedScoreTable)
    {
        profile.scoreTableOnMaster = false;
        profile.scoreTableName = QStringLiteral("Report_Table");
        profile.minimumTableColumns = 7;
        profile.firstGradeColumn = 3;
        profile.neutralFillRed = 229;
        profile.neutralFillGreen = 229;
        profile.neutralFillBlue = 231;
    }
    else
    {
        profile.scoreTableName = QStringLiteral("Grades & Scores");
    }

    return profile;
}

PowerPointBatchJob powerPointBatchJob(
    const QList<StudentReport>& reports,
    const QStringList& pdfPaths,
    const QString& workingDirectory
    )
{
    PowerPointBatchJob batch;
    if (reports.isEmpty() || reports.size() != pdfPaths.size())
    {
        return batch;
    }

    batch.templateProfile =
        powerPointTemplateProfile(
            reports.constFirst().report.reportTemplate
            );
    batch.signatureImage =
        reports.constFirst().report.signatureImage;
    batch.students.reserve(reports.size());

    for (int index = 0; index < reports.size(); ++index)
    {
        const StudentReport& student = reports.at(index);
        const SpeakingEvalReportData& data = student.report;

        PowerPointStudentJob job;
        job.displayName = powerPointText(student.displayName);
        job.pdfPath = pdfPaths.at(index);
        job.completionPath =
            QDir(workingDirectory).filePath(
                QStringLiteral("completed-%1")
                    .arg(index, 6, 10, QLatin1Char('0'))
                );
        job.englishName = powerPointText(data.englishName);
        job.koreanName = powerPointText(data.koreanName);
        job.classLabel = powerPointText(data.classLabel);
        job.nativeTeacher = powerPointText(data.nativeTeacher);
        job.koreanTeacher = powerPointText(data.koreanTeacher);
        job.date = powerPointText(data.date);
        job.comments = powerPointText(data.comments);
        const SpeakingEvalFieldAsset* commentsField =
            speakingEvalFieldAsset(
                data.reportTemplate,
                QStringLiteral("comments")
                );
        if (commentsField)
        {
            job.commentsFontSizePoints =
                speakingEvalFittedFieldFontSize(
                    *commentsField,
                    job.comments,
                    1.0
                    );
        }
        job.overallGrade = powerPointText(
            SpeakingEvalReportDataAssembler::overallGrade(data.scores)
            );
        for (
            std::size_t scoreIndex = 0;
            scoreIndex < job.scores.size();
            ++scoreIndex
            )
        {
            job.scores[scoreIndex] =
                powerPointText(data.scores[scoreIndex]);
        }
        batch.students.append(job);
    }

    return batch;
}

QJsonObject powerPointBatchJson(
    const PowerPointBatchJob& batch,
    const QString& pptxPath,
    const QString& signaturePath,
    const QString& cancelPath
    )
{
    QJsonArray students;
    for (const PowerPointStudentJob& job : batch.students)
    {
        QJsonArray scores;
        for (const QString& score : job.scores)
        {
            scores.append(score);
        }

        students.append(
            QJsonObject{
                {
                    QStringLiteral("displayName"),
                    job.displayName
                },
                {
                    QStringLiteral("pdfPath"),
                    QDir::toNativeSeparators(job.pdfPath)
                },
                {
                    QStringLiteral("completionPath"),
                    QDir::toNativeSeparators(job.completionPath)
                },
                { QStringLiteral("englishName"), job.englishName },
                { QStringLiteral("koreanName"), job.koreanName },
                { QStringLiteral("classLabel"), job.classLabel },
                { QStringLiteral("nativeTeacher"), job.nativeTeacher },
                { QStringLiteral("koreanTeacher"), job.koreanTeacher },
                { QStringLiteral("date"), job.date },
                { QStringLiteral("comments"), job.comments },
                {
                    QStringLiteral("commentsFontName"),
                    QString::fromLatin1(PowerPointCommentFontName)
                },
                {
                    QStringLiteral("commentsFontSizePoints"),
                    job.commentsFontSizePoints
                },
                { QStringLiteral("overallGrade"), job.overallGrade },
                { QStringLiteral("scores"), scores }
            }
            );
    }

    const PowerPointTemplateProfile& profile =
        batch.templateProfile;
    return {
        {
            QStringLiteral("pptxPath"),
            QDir::toNativeSeparators(pptxPath)
        },
        {
            QStringLiteral("cancelPath"),
            QDir::toNativeSeparators(cancelPath)
        },
        {
            QStringLiteral("scoreTableOnMaster"),
            profile.scoreTableOnMaster
        },
        {
            QStringLiteral("scoreTableName"),
            profile.scoreTableName
        },
        {
            QStringLiteral("minimumTableRows"),
            profile.minimumTableRows
        },
        {
            QStringLiteral("minimumTableColumns"),
            profile.minimumTableColumns
        },
        {
            QStringLiteral("firstGradeColumn"),
            profile.firstGradeColumn
        },
        {
            QStringLiteral("neutralFill"),
            profile.neutralFillRed
                + (profile.neutralFillGreen << 8)
                + (profile.neutralFillBlue << 16)
        },
        {
            QStringLiteral("signaturePath"),
            QDir::toNativeSeparators(signaturePath)
        },
        {
            QStringLiteral("signatureLeft"),
            profile.signatureBounds.left()
        },
        {
            QStringLiteral("signatureTop"),
            profile.signatureBounds.top()
        },
        {
            QStringLiteral("signatureWidth"),
            profile.signatureBounds.width()
        },
        {
            QStringLiteral("signatureHeight"),
            profile.signatureBounds.height()
        },
        {
            QStringLiteral("signatureAlignsBottomLeft"),
            profile.signatureAlignsBottomLeft
        },
        { QStringLiteral("students"), students }
    };
}

bool writeUtf8File(
    const QString& path,
    const QByteArray& data,
    QString* errorMessage
    )
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size()
        || !file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Unable to prepare PowerPoint automation.");
        }
        return false;
    }

    return true;
}

#ifdef Q_OS_WIN
QString windowsPowerPointScript()
{
    return QString::fromUtf8(R"PS(
$ErrorActionPreference = 'Stop'

function Find-Shape($shapes, [string]$name) {
    for ($index = 1; $index -le $shapes.Count; ++$index) {
        $shape = $shapes.Item($index)
        if ($shape.Name -eq $name) {
            return $shape
        }
        if ($shape.Type -eq 6) {
            $nested = Find-Shape $shape.GroupItems $name
            if ($null -ne $nested) {
                return $nested
            }
        }
    }
    return $null
}

function Require-Shape($shapes, [string]$name) {
    $shape = Find-Shape $shapes $name
    if ($null -eq $shape) {
        throw "The PowerPoint template is missing '$name'."
    }
    return $shape
}

function Set-Text($shape, [string]$value) {
    $shape.TextFrame.TextRange.Text = $value
}

function Set-Comments(
    $shape,
    [string]$value,
    [string]$fontName,
    [double]$fontSizePoints
) {
    if ($fontSizePoints -le 0) {
        $shape.TextFrame.TextRange.Text = $value
        return
    }

    $textFrame = $shape.TextFrame
    $maximumTextHeight = $shape.Height `
        - $textFrame.MarginTop `
        - $textFrame.MarginBottom
    $textFrame.AutoSize = 0 # ppAutoSizeNone
    $textFrame.TextRange.Text = $value
    $textFrame.TextRange.Font.Name = $fontName
    $textFrame.TextRange.Font.Size = $fontSizePoints
    while (
        $textFrame.TextRange.BoundHeight -gt $maximumTextHeight `
        -and $fontSizePoints -gt 1
    ) {
        $fontSizePoints -= 1
        $textFrame.TextRange.Font.Size = $fontSizePoints
    }
}

function Set-UnderlinedText($shape, [string]$value) {
    $shape.TextFrame.TextRange.Text = $value
    # Keep the text on the underline supplied by the template.  The text-box
    # bounds intentionally extend slightly below that line to leave room for
    # descenders, so do not resize or reposition the shape here.
    $shape.TextFrame.VerticalAnchor = 4 # msoAnchorBottom
}

function Require-Table($shapes, [string]$name, [int]$rows, [int]$columns) {
    $shape = Require-Shape $shapes $name
    if ($shape.HasTable -ne -1) {
        throw "The PowerPoint template shape '$name' is not a table."
    }
    if ($shape.Table.Rows.Count -lt $rows -or $shape.Table.Columns.Count -lt $columns) {
        throw "The PowerPoint template table '$name' has an unexpected size."
    }
    return $shape.Table
}

function Set-Fill($shape, [int]$color) {
    $shape.Fill.Solid()
    $shape.Fill.ForeColor.RGB = $color
}

function Add-Signature(
    $shapes,
    [string]$path,
    [double]$left,
    [double]$top,
    [double]$maximumWidth,
    [double]$maximumHeight,
    [bool]$alignsBottomLeft
) {
    if ([string]::IsNullOrWhiteSpace($path)) {
        return
    }

    $picture = $shapes.AddPicture($path, 0, -1, 0, 0, -1, -1)
    $picture.Name = 'Signature_Image'
    $picture.LockAspectRatio = -1
    $scale = [Math]::Min(
        $maximumWidth / $picture.Width,
        $maximumHeight / $picture.Height
    )
    $picture.Width = $picture.Width * $scale
    $picture.Height = $picture.Height * $scale
    if ($alignsBottomLeft) {
        $picture.Left = $left
    }
    else {
        $picture.Left = $left + $maximumWidth - $picture.Width
    }
    $picture.Top = $top + $maximumHeight - $picture.Height
}

$data = Get-Content -LiteralPath $args[0] -Raw | ConvertFrom-Json
$pptxPath = [string]$data.pptxPath
Write-Output ("ClassMngr PowerPoint batch export: $pptxPath")
$ppt = $null
$presentation = $null
$canceled = $false

try {
    $ppt = New-Object -ComObject PowerPoint.Application
    $firstGradeColumn = [int]$data.firstGradeColumn
    $grey = [int]$data.neutralFill
    $yellow = 65535
    $gradeOffsets = @{
        'A+' = 0
        'A' = 1
        'B+' = 2
        'B' = 3
        'C' = 4
    }

    foreach ($student in $data.students) {
        if (Test-Path -LiteralPath ([string]$data.cancelPath)) {
            $canceled = $true
            break
        }

        # SaveAs with ppSaveAsPDF is reliable through Windows PowerShell's
        # PowerPoint COM adapter, while ExportAsFixedFormat is exposed as a
        # parameterized property by some Office versions and cannot be called
        # with its enum argument. Reopen the template for each student so
        # SaveAs never changes the presentation used by the next report.
        $presentation = $ppt.Presentations.Open(
            $pptxPath,
            $false,
            $false,
            $false
        )
        $slide = $presentation.Slides.Item(1)
        $shapes = $slide.Shapes

        $englishNameShape = Require-Shape $shapes 'English_Name'
        $koreanNameShape = Require-Shape $shapes 'Korean_Name'
        $gradeLevelShape = Require-Shape $shapes 'Grade_Level'
        $nativeTeacherShape = Require-Shape $shapes 'Native_Teacher'
        $koreanTeacherShape = Require-Shape $shapes 'Korean_Teacher'
        $evaluationDateShape = Require-Shape $shapes 'Eval_Date'
        $commentsShape = Require-Shape $shapes 'Comments'
        $overallGradeShape = Require-Shape $shapes 'Overall_Grade'

        if ([bool]$data.scoreTableOnMaster) {
            $tableShapes = $slide.Master.Shapes
        }
        else {
            $tableShapes = $shapes
        }
        $table = Require-Table `
            $tableShapes `
            ([string]$data.scoreTableName) `
            ([int]$data.minimumTableRows) `
            ([int]$data.minimumTableColumns)

        Add-Signature `
            $shapes `
            ([string]$data.signaturePath) `
            ([double]$data.signatureLeft) `
            ([double]$data.signatureTop) `
            ([double]$data.signatureWidth) `
            ([double]$data.signatureHeight) `
            ([bool]$data.signatureAlignsBottomLeft)

        Set-UnderlinedText $englishNameShape ([string]$student.englishName)
        Set-UnderlinedText $koreanNameShape ([string]$student.koreanName)
        Set-UnderlinedText $gradeLevelShape ([string]$student.classLabel)
        Set-UnderlinedText $nativeTeacherShape ([string]$student.nativeTeacher)
        Set-UnderlinedText $koreanTeacherShape ([string]$student.koreanTeacher)
        Set-UnderlinedText $evaluationDateShape ([string]$student.date)
        Set-Comments `
            $commentsShape `
            ([string]$student.comments) `
            ([string]$student.commentsFontName) `
            ([double]$student.commentsFontSizePoints)
        Set-Text $overallGradeShape ([string]$student.overallGrade)

        for ($scoreIndex = 0; $scoreIndex -lt 6; ++$scoreIndex) {
            $rowIndex = $scoreIndex * 2 + 1
            foreach ($column in $firstGradeColumn..($firstGradeColumn + 4)) {
                Set-Fill $table.Cell($rowIndex, $column).Shape $grey
            }

            $score = [string]$student.scores[$scoreIndex]
            if ($gradeOffsets.ContainsKey($score)) {
                $selectedCell = $table.Cell(
                    $rowIndex,
                    $firstGradeColumn + [int]$gradeOffsets[$score]
                )
                Set-Fill $selectedCell.Shape $yellow
            }
        }

        $pdfPath = [string]$student.pdfPath
        Write-Output (
            "ClassMngr PowerPoint report: {0} -> {1}" `
                -f ([string]$student.displayName), $pdfPath
        )
        $presentation.SaveAs($pdfPath, [int]32)
        $presentation.Saved = $true
        $presentation.Close()
        $presentation = $null
        [System.IO.File]::WriteAllText(
            ([string]$student.completionPath),
            [string]::Empty
        )
    }
}
finally {
    if ($null -ne $presentation) {
        $presentation.Saved = $true
        $presentation.Close()
    }
    if ($null -ne $ppt) {
        $ppt.Quit()
    }
}

if ($canceled) {
    exit 2
}
)PS");
}
#endif

#ifdef Q_OS_MACOS
QString macPowerPointScript()
{
    return QString::fromUtf8(R"APPLESCRIPT(
on gradeColumn(scoreValue, firstGradeColumn)
    if scoreValue is "A+" then return firstGradeColumn
    if scoreValue is "A" then return firstGradeColumn + 1
    if scoreValue is "B+" then return firstGradeColumn + 2
    if scoreValue is "B" then return firstGradeColumn + 3
    if scoreValue is "C" then return firstGradeColumn + 4
    return 0
end gradeColumn

on findShape(shapeContainer, targetName)
    tell application "Microsoft PowerPoint"
        try
            return shape (targetName) of shapeContainer
        end try
        try
            set candidateShapes to shapes of shapeContainer
        on error
            return missing value
        end try
        repeat with shapeReference in candidateShapes
            set candidateShape to contents of shapeReference
            if (name of candidateShape as text) is targetName then
                return candidateShape
            end if
            set nestedShape to my findShape(candidateShape, targetName)
            if nestedShape is not missing value then return nestedShape
        end repeat
    end tell
    return missing value
end findShape

on requireShape(shapeContainer, targetName)
    set requiredShape to my findShape(shapeContainer, targetName)
    if requiredShape is missing value then
        error "The PowerPoint template is missing '" & targetName & "'."
    end if
    return requiredShape
end requireShape

on setShapeText(targetShape, textValue, alignWithUnderline)
    tell application "Microsoft PowerPoint"
        set content of text range of text frame of targetShape to textValue
        if alignWithUnderline then
            set «property TfVA» of text frame of targetShape to 4
        end if
    end tell
end setShapeText

on setCommentsText(targetShape, textValue, fontName, fontSizePoints)
    tell application "Microsoft PowerPoint"
        set commentTextRange to text range of text frame of targetShape
        set content of commentTextRange to textValue
        if fontSizePoints is greater than 0 then
            set font name of font of commentTextRange to fontName
            set font size of font of commentTextRange to fontSizePoints
        end if
    end tell
end setCommentsText

on requireTable(shapeContainer, targetName, minimumRows, minimumColumns)
    set tableShape to my requireShape(shapeContainer, targetName)
    tell application "Microsoft PowerPoint"
        if not («property sHTb» of tableShape) then
            error "The PowerPoint template shape '" & targetName & "' is not a table."
        end if
        if «property NRws» of tableShape is less than minimumRows or «property NCms» of tableShape is less than minimumColumns then
            error "The PowerPoint template table '" & targetName & "' has an unexpected size."
        end if
    end tell
    return tableShape
end requireTable

on setCellFill(tableShape, rowIndex, columnIndex, rgbValue)
    tell application "Microsoft PowerPoint"
        set reportTable to «property PTbO» of tableShape
        set reportCell to cell (columnIndex) of row (rowIndex) of reportTable
        set cellShape to shape of reportCell
        set «property fClr» of «property pFFm» of cellShape to contents of rgbValue
    end tell
end setCellFill

on addSignature(reportSlide, signaturePath, signatureLeft, signatureTop, maximumWidth, maximumHeight, alignsBottomLeft)
    if signaturePath is "" then return

    tell application "Microsoft PowerPoint"
        set signaturePicture to make new picture at end of reportSlide with properties {file name:signaturePath, link to file:false, save with document:true}
        set pictureWidth to width of signaturePicture
        set pictureHeight to height of signaturePicture
        set scaleFactor to maximumWidth / pictureWidth
        if (maximumHeight / pictureHeight) is less than scaleFactor then
            set scaleFactor to maximumHeight / pictureHeight
        end if
        set width of signaturePicture to pictureWidth * scaleFactor
        set height of signaturePicture to pictureHeight * scaleFactor
        if alignsBottomLeft then
            set left position of signaturePicture to signatureLeft
        else
            set left position of signaturePicture to signatureLeft + maximumWidth - (width of signaturePicture)
        end if
        set top of signaturePicture to signatureTop + maximumHeight - (height of signaturePicture)
        set name of signaturePicture to "Signature_Image"
    end tell
end addSignature

on openPresentationCount()
    tell application "Microsoft PowerPoint"
        set openPresentations to presentations
    end tell
    if openPresentations is missing value then return 0
    return count of openPresentations
end openPresentationCount

on waitForOpenedPresentation(previousPresentationCount)
    repeat with attemptIndex from 1 to 100
        tell application "Microsoft PowerPoint"
            set openPresentations to presentations
        end tell
        if openPresentations is not missing value and (count of openPresentations) is greater than previousPresentationCount then
            return contents of last item of openPresentations
        end if
        delay 0.1
    end repeat
    return missing value
end waitForOpenedPresentation

on fileExists(filePath)
    try
        do shell script "/usr/bin/test -e " & quoted form of filePath
        return true
    on error
        return false
    end try
end fileExists

on touchFile(filePath)
    do shell script "/usr/bin/touch " & quoted form of filePath
end touchFile

on decodedText(encodedText)
    return do shell script "/bin/echo " & quoted form of encodedText & " | /usr/bin/base64 -D"
end decodedText

on run argv
    set pptxPath to item 1 of argv
    set scoreTableOnMaster to (item 2 of argv is "true")
    set scoreTableName to item 3 of argv
    set minimumTableRows to item 4 of argv as integer
    set minimumTableColumns to item 5 of argv as integer
    set firstGradeColumn to item 6 of argv as integer
    set greyFill to {(item 7 of argv as integer), (item 8 of argv as integer), (item 9 of argv as integer)}
    set signaturePath to item 10 of argv
    set signatureLeft to item 11 of argv as real
    set signatureTop to item 12 of argv as real
    set signatureWidth to item 13 of argv as real
    set signatureHeight to item 14 of argv as real
    set signatureAlignsBottomLeft to (item 15 of argv is "true")
    set cancelPath to item 16 of argv
    set jobCount to item 17 of argv as integer
    set argumentIndex to 18
    set yellowFill to {255, 255, 0}
    set reportPresentation to missing value
    set wasCanceled to false
    set exportStep to "opening the presentation"

    try
        tell application "Microsoft PowerPoint"
            -- PowerPoint opens the file but does not return a reliable
            -- presentation object from its open command.  Resolve the newly
            -- added presentation from the application collection instead.
            set originalPresentationCount to my openPresentationCount()
            open (POSIX file pptxPath)
            set reportPresentation to my waitForOpenedPresentation(originalPresentationCount)
            if reportPresentation is missing value then
                error "PowerPoint did not finish opening the template."
            end if
            set exportStep to "accessing slide 1"
            set reportSlide to slide (1) of reportPresentation
            set exportStep to "adding the signature image"
            my addSignature(reportSlide, signaturePath, signatureLeft, signatureTop, signatureWidth, signatureHeight, signatureAlignsBottomLeft)
            set exportStep to "resolving report text shapes"
            set englishNameShape to my requireShape(reportSlide, "English_Name")
            set koreanNameShape to my requireShape(reportSlide, "Korean_Name")
            set gradeLevelShape to my requireShape(reportSlide, "Grade_Level")
            set nativeTeacherShape to my requireShape(reportSlide, "Native_Teacher")
            set koreanTeacherShape to my requireShape(reportSlide, "Korean_Teacher")
            set evaluationDateShape to my requireShape(reportSlide, "Eval_Date")
            set commentsShape to my requireShape(reportSlide, "Comments")
            set overallGradeShape to my requireShape(reportSlide, "Overall_Grade")

            set exportStep to "resolving the score table"
            if scoreTableOnMaster then
                -- The standard template's visible bordered table lives on
                -- the slide master; the slide shapes are transparent labels.
                set scoreTableContainer to «property SlMr» of reportSlide
            else
                set scoreTableContainer to reportSlide
            end if
            set reportTableShape to my requireTable(scoreTableContainer, scoreTableName, minimumTableRows, minimumTableColumns)

            repeat with jobIndex from 1 to jobCount
                if my fileExists(cancelPath) then
                    set wasCanceled to true
                    exit repeat
                end if

                set studentName to my decodedText(item argumentIndex of argv)
                set pdfPath to item (argumentIndex + 1) of argv
                set completionPath to item (argumentIndex + 2) of argv
                set englishName to my decodedText(item (argumentIndex + 3) of argv)
                set koreanName to my decodedText(item (argumentIndex + 4) of argv)
                set classLabel to my decodedText(item (argumentIndex + 5) of argv)
                set nativeTeacher to my decodedText(item (argumentIndex + 6) of argv)
                set koreanTeacher to my decodedText(item (argumentIndex + 7) of argv)
                set evaluationDate to my decodedText(item (argumentIndex + 8) of argv)
                set commentsText to my decodedText(item (argumentIndex + 9) of argv)
                set commentsFontName to my decodedText(item (argumentIndex + 10) of argv)
                set commentsFontSizePoints to item (argumentIndex + 11) of argv as real
                set overallGrade to my decodedText(item (argumentIndex + 12) of argv)
                set scoreValues to items (argumentIndex + 13) thru (argumentIndex + 18) of argv
                set argumentIndex to argumentIndex + 19

                set exportStep to "updating report text for " & studentName
                my setShapeText(englishNameShape, englishName, true)
                my setShapeText(koreanNameShape, koreanName, true)
                my setShapeText(gradeLevelShape, classLabel, true)
                my setShapeText(nativeTeacherShape, nativeTeacher, true)
                my setShapeText(koreanTeacherShape, koreanTeacher, true)
                my setShapeText(evaluationDateShape, evaluationDate, true)
                my setCommentsText(commentsShape, commentsText, commentsFontName, commentsFontSizePoints)
                my setShapeText(overallGradeShape, overallGrade, false)

                set exportStep to "updating score-table cells for " & studentName
                repeat with scoreIndex from 1 to 6
                    set rowIndex to (scoreIndex - 1) * 2 + 1
                    repeat with columnIndex from firstGradeColumn to firstGradeColumn + 4
                        my setCellFill(reportTableShape, rowIndex, columnIndex, greyFill)
                    end repeat
                    set selectedColumn to my gradeColumn(item scoreIndex of scoreValues, firstGradeColumn)
                    if selectedColumn is greater than 0 then
                        my setCellFill(reportTableShape, rowIndex, selectedColumn, yellowFill)
                    end if
                end repeat

                set exportStep to "saving the PDF for " & studentName
                save reportPresentation in (POSIX file pdfPath) as save as PDF
                my touchFile(completionPath)
            end repeat

            close reportPresentation saving no
            set reportPresentation to missing value
            if wasCanceled then
                error "ClassMngr PowerPoint export canceled." number 2
            end if
        end tell
    on error errorMessage number errorNumber
        -- Keep cleanup outside PowerPoint's tell scope so AppleScript resolves
        -- reportPresentation as this handler's local variable.  The previous
        -- cleanup masked the original export failure with error -2753.
        if reportPresentation is not missing value then
            tell application "Microsoft PowerPoint"
                try
                    close reportPresentation saving no
                end try
            end tell
        end if
        error exportStep & ": " & errorMessage number errorNumber
    end try
end run
)APPLESCRIPT");
}
#endif

PowerPointBatchStatus renderPowerPointBatch(
    const PowerPointBatchJob& batch,
    const QString& automationDirectory,
    const QString& presentationDirectory,
    const std::function<
        bool(int completed, int total, const QString& studentName)
        >& progressCallback,
    QString* errorMessage
    )
{
    if (batch.students.isEmpty()
        || automationDirectory.trimmed().isEmpty()
        || presentationDirectory.trimmed().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "The temporary PowerPoint batch is unavailable."
                );
        }
        return PowerPointBatchStatus::Failed;
    }

    const QDir automation(automationDirectory);
    const QDir presentation(presentationDirectory);
    const QString preparedPptxPath =
        automation.filePath(QStringLiteral("report-template.pptx"));
    const QString pptxPath =
        presentation.filePath(QStringLiteral("report-template.pptx"));
    const QString jobPath =
        automation.filePath(QStringLiteral("batch.json"));
    const QString cancelPath =
        automation.filePath(QStringLiteral("cancel-requested"));
    const QString scriptPath =
        automation.filePath(
#ifdef Q_OS_WIN
            QStringLiteral("export-batch.ps1")
#else
            QStringLiteral("export-batch.applescript")
#endif
            );

    QString preparedSignaturePath;
    if (!SpeakingEvalReportAssetResolver::copyResourceToFile(
            batch.templateProfile.resourcePath,
            preparedPptxPath,
            errorMessage
            )
        || !SpeakingEvalReportAssetResolver::prepareSignatureImage(
            batch.signatureImage,
            automationDirectory,
            &preparedSignaturePath,
            errorMessage
            ))
    {
        return PowerPointBatchStatus::Failed;
    }

    QString signaturePath = preparedSignaturePath;
    if (automationDirectory != presentationDirectory)
    {
        if (!SpeakingEvalReportAssetResolver::copyFileReplacing(
                preparedPptxPath,
                pptxPath,
                QObject::tr(
                    "The PowerPoint template could not be copied into PowerPoint's private workspace."
                    ),
                errorMessage
                ))
        {
            return PowerPointBatchStatus::Failed;
        }

        signaturePath.clear();
        if (!preparedSignaturePath.isEmpty())
        {
            signaturePath =
                presentation.filePath(
                    QStringLiteral("signature.png")
                    );
            if (!SpeakingEvalReportAssetResolver::copyFileReplacing(
                    preparedSignaturePath,
                    signaturePath,
                    QObject::tr(
                        "The signature could not be copied into PowerPoint's private workspace."
                        ),
                    errorMessage
                    ))
            {
                return PowerPointBatchStatus::Failed;
            }
        }
    }

    QString executable;
    QStringList arguments;

#ifdef Q_OS_WIN
    executable = SpeakingEvalPowerPointAutomation::executable();
    if (executable.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = powerPointRendererAvailabilityMessage();
        }
        return PowerPointBatchStatus::Failed;
    }

    const QByteArray jobJson =
        QByteArrayLiteral("\xEF\xBB\xBF")
        + QJsonDocument(
            powerPointBatchJson(
                batch,
                pptxPath,
                signaturePath,
                cancelPath
                )
            ).toJson(QJsonDocument::Compact);
    if (!writeUtf8File(jobPath, jobJson, errorMessage)
        || !writeUtf8File(
            scriptPath,
            windowsPowerPointScript().toUtf8(),
            errorMessage
            ))
    {
        return PowerPointBatchStatus::Failed;
    }

    arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-File"),
        scriptPath,
        jobPath
    };
#elif defined(Q_OS_MACOS)
    executable = SpeakingEvalPowerPointAutomation::executable();
    if (executable.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = powerPointRendererAvailabilityMessage();
        }
        return PowerPointBatchStatus::Failed;
    }

    if (!writeUtf8File(
            scriptPath,
            macPowerPointScript().toUtf8(),
            errorMessage
            ))
    {
        return PowerPointBatchStatus::Failed;
    }

    const PowerPointTemplateProfile& profile =
        batch.templateProfile;
    arguments = {
        scriptPath,
        pptxPath,
        profile.scoreTableOnMaster
            ? QStringLiteral("true")
            : QStringLiteral("false"),
        profile.scoreTableName,
        QString::number(profile.minimumTableRows),
        QString::number(profile.minimumTableColumns),
        QString::number(profile.firstGradeColumn),
        QString::number(profile.neutralFillRed),
        QString::number(profile.neutralFillGreen),
        QString::number(profile.neutralFillBlue),
        signaturePath,
        QString::number(profile.signatureBounds.left()),
        QString::number(profile.signatureBounds.top()),
        QString::number(profile.signatureBounds.width()),
        QString::number(profile.signatureBounds.height()),
        profile.signatureAlignsBottomLeft
            ? QStringLiteral("true")
            : QStringLiteral("false"),
        cancelPath,
        QString::number(batch.students.size())
    };
    for (const PowerPointStudentJob& job : batch.students)
    {
        arguments.append(
            QStringList{
                macPowerPointTextArgument(job.displayName),
                job.pdfPath,
                job.completionPath,
                macPowerPointTextArgument(job.englishName),
                macPowerPointTextArgument(job.koreanName),
                macPowerPointTextArgument(job.classLabel),
                macPowerPointTextArgument(job.nativeTeacher),
                macPowerPointTextArgument(job.koreanTeacher),
                macPowerPointTextArgument(job.date),
                macPowerPointTextArgument(job.comments),
                macPowerPointTextArgument(
                    QString::fromLatin1(
                        PowerPointCommentFontName
                        )
                    ),
                QString::number(job.commentsFontSizePoints),
                macPowerPointTextArgument(job.overallGrade),
                job.scores[0],
                job.scores[1],
                job.scores[2],
                job.scores[3],
                job.scores[4],
                job.scores[5]
            }
            );
    }
#else
    Q_UNUSED(jobPath);
    Q_UNUSED(scriptPath);
    Q_UNUSED(cancelPath);
    if (errorMessage)
    {
        *errorMessage = powerPointRendererAvailabilityMessage();
    }
    return PowerPointBatchStatus::Failed;
#endif

    QList<SpeakingEvalPowerPointAutomation::ReportMarker> reports;
    reports.reserve(batch.students.size());
    for (const PowerPointStudentJob& job : batch.students)
    {
        reports.append({
            job.displayName,
            job.pdfPath,
            job.completionPath
        });
    }

    const SpeakingEvalPowerPointAutomation::Status status =
        SpeakingEvalPowerPointAutomation::run(
            {
                executable,
                arguments,
                automationDirectory,
                cancelPath,
                reports,
                PowerPointTimeoutMs,
                progressCallback
            },
            errorMessage
            );
    switch (status)
    {
    case SpeakingEvalPowerPointAutomation::Status::Completed:
        return PowerPointBatchStatus::Completed;
    case SpeakingEvalPowerPointAutomation::Status::Canceled:
        return PowerPointBatchStatus::Canceled;
    case SpeakingEvalPowerPointAutomation::Status::Failed:
    default:
        return PowerPointBatchStatus::Failed;
    }
}

bool powerPointReportsUseSingleTemplate(
    const QList<StudentReport>& reports
    )
{
    if (reports.isEmpty())
    {
        return true;
    }

    const SpeakingEvalReportTemplate reportTemplate =
        reports.constFirst().report.reportTemplate;
    return std::all_of(
        reports.cbegin(),
        reports.cend(),
        [reportTemplate](const StudentReport& report)
        {
            return report.report.reportTemplate == reportTemplate;
        }
        );
}

} // namespace

QString rendererDisplayName(
    Renderer renderer
    )
{
    switch (renderer)
    {
    case Renderer::Internal:
        return QObject::tr("Internal Template (Default)");
    case Renderer::PowerPoint:
        return QObject::tr("PowerPoint (Fallback)");
    }

    return {};
}

QString defaultOutputDirectory(
    const ClassInfo& classInfo,
    const QString& evaluationName,
    const QString& documentsDirectory
    )
{
    return SpeakingEvalReportOutputPolicy::defaultDirectory(
        classInfo,
        evaluationName,
        documentsDirectory
        );
}

QString batchArchivePath(
    const QString& outputDirectory
    )
{
    return SpeakingEvalReportOutputPolicy::batchArchivePath(outputDirectory);
}

QString safeFileName(
    const QString& englishName,
    const QString& koreanName
    )
{
    return SpeakingEvalReportOutputPolicy::studentFileName(
        englishName,
        koreanName
        );
}

bool isPowerPointRendererAvailable()
{
    return SpeakingEvalPowerPointAutomation::isAvailable();
}

QString powerPointRendererAvailabilityMessage()
{
    return SpeakingEvalPowerPointAutomation::availabilityMessage();
}

Result exportReports(
    const Request& request
    )
{
    if (request.reports.isEmpty())
    {
        return failed(QObject::tr("There are no student reports to export."));
    }

    if (!request.savePdf && !request.printReports)
    {
        return failed(QObject::tr("Choose PDF saving, printing, or both."));
    }

    if (request.savePdf
        && request.outputDirectory.trimmed().isEmpty()
        && request.outputFilePath.trimmed().isEmpty())
    {
        return failed(QObject::tr("Choose a destination for the PDF reports."));
    }

    if (request.renderer == Renderer::PowerPoint
        && !powerPointReportsUseSingleTemplate(request.reports))
    {
        return failed(
            QObject::tr(
                "All reports in a PowerPoint batch must use the same template."
                )
            );
    }

    if (request.renderer == Renderer::PowerPoint
        && !isPowerPointRendererAvailable())
    {
        return failed(powerPointRendererAvailabilityMessage());
    }

    const bool creatingBatchArchive =
        request.savePdf && request.reports.size() > 1;
    const bool savingIndividualPdfFiles =
        request.savePdf
        && (!creatingBatchArchive
            || request.keepIndividualPdfFiles);

    QStringList targetPaths;
    QString targetArchivePath;
    QString errorMessage;
    if (request.savePdf
        && !SpeakingEvalReportOutput::targetFilePaths(
            request,
            savingIndividualPdfFiles,
            &targetPaths,
            &errorMessage
            ))
    {
        return failed(errorMessage);
    }
    if (creatingBatchArchive)
    {
        targetArchivePath =
            batchArchivePath(request.outputDirectory);
        if (!request.overwriteExisting
            && QFileInfo::exists(targetArchivePath))
        {
            return failed(
                QObject::tr("A ZIP archive named \"%1\" already exists.")
                    .arg(QFileInfo(targetArchivePath).fileName())
                );
        }
    }

    QTemporaryDir stagingDirectory(
        QDir::temp().filePath(
            QStringLiteral("ClassMngr-speaking-evaluations-XXXXXX")
            )
        );
    if (!stagingDirectory.isValid())
    {
        return failed(QObject::tr("A temporary report folder could not be created."));
    }

    QStringList stagedPdfPaths;
    stagedPdfPaths.reserve(request.reports.size());
    SpeakingEvalPowerPointWorkspace powerPointWorkspace;
    if (request.renderer == Renderer::PowerPoint)
    {
        if (!powerPointWorkspace.prepare(
                stagingDirectory.path(),
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }

        QStringList powerPointPdfPaths;
        powerPointPdfPaths.reserve(request.reports.size());
        for (int index = 0; index < request.reports.size(); ++index)
        {
            const QString fileName =
                QStringLiteral("report-%1.pdf")
                    .arg(index, 6, 10, QLatin1Char('0'));
            stagedPdfPaths.append(
                QDir(
                    powerPointWorkspace.automationDirectory()
                    ).filePath(fileName)
                );
            powerPointPdfPaths.append(
                QDir(
                    powerPointWorkspace.presentationDirectory()
                    ).filePath(fileName)
                );
        }

        const PowerPointBatchJob batch =
            powerPointBatchJob(
                request.reports,
                powerPointPdfPaths,
                powerPointWorkspace.automationDirectory()
                );
        const PowerPointBatchStatus powerPointStatus =
            renderPowerPointBatch(
                batch,
                powerPointWorkspace.automationDirectory(),
                powerPointWorkspace.presentationDirectory(),
                request.progressCallback,
                &errorMessage
                );
        if (powerPointStatus == PowerPointBatchStatus::Canceled)
        {
            return canceled();
        }
        if (powerPointStatus == PowerPointBatchStatus::Failed)
        {
            return failed(errorMessage);
        }

        QStringList displayNames;
        displayNames.reserve(batch.students.size());
        for (const PowerPointStudentJob& job : batch.students)
        {
            displayNames.append(job.displayName);
        }

        if (powerPointWorkspace.usesSeparatePresentationDirectory()
            && !powerPointWorkspace.copyOutputFiles(
                powerPointPdfPaths,
                stagedPdfPaths,
                displayNames,
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }

        if (!powerPointWorkspace.removePresentationDirectory(
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }
    }
    else
    {
        for (int index = 0; index < request.reports.size(); ++index)
        {
            const StudentReport& student = request.reports.at(index);
            if (request.progressCallback
                && !request.progressCallback(
                    index,
                    request.reports.size(),
                    student.displayName
                    ))
            {
                return canceled();
            }

            const QString stagedPath =
                QDir(stagingDirectory.path()).filePath(
                    safeFileName(
                        student.report.englishName,
                        student.report.koreanName
                        )
                    );

            const bool rendered =
                SpeakingEvalInternalPdfRenderer::render(
                    student.report,
                    stagedPath,
                    &errorMessage
                    );

            if (!rendered)
            {
                return failed(
                    QObject::tr("%1: %2")
                        .arg(student.displayName, errorMessage),
                    true
                    );
            }

            stagedPdfPaths.append(stagedPath);
        }

        if (request.progressCallback
            && !request.progressCallback(
                request.reports.size(),
                request.reports.size(),
                QString()
                ))
        {
            return canceled();
        }
    }

    QString stagedArchivePath;
    if (creatingBatchArchive)
    {
        stagedArchivePath =
            QDir(stagingDirectory.path()).filePath(
                QStringLiteral("reports.zip")
                );
        QList<ZipArchiveWriter::Entry> archiveEntries;
        archiveEntries.reserve(stagedPdfPaths.size());
        for (int index = 0; index < stagedPdfPaths.size(); ++index)
        {
            archiveEntries.append({
                stagedPdfPaths.at(index),
                QFileInfo(targetPaths.at(index)).fileName()
            });
        }

        if (!ZipArchiveWriter::writeArchive(
                stagedArchivePath,
                archiveEntries,
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }
    }

    if (request.savePdf)
    {
        QStringList stagedOutputPaths;
        QStringList targetOutputPaths;
        if (creatingBatchArchive)
        {
            stagedOutputPaths.append(stagedArchivePath);
            targetOutputPaths.append(targetArchivePath);
        }
        if (savingIndividualPdfFiles)
        {
            stagedOutputPaths.append(stagedPdfPaths);
            targetOutputPaths.append(targetPaths);
        }

        if (!SpeakingEvalReportOutput::commitFiles(
                stagedOutputPaths,
                targetOutputPaths,
                request.overwriteExisting,
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }
    }

    if (request.printReports)
    {
        const bool printingOneReport =
            request.reports.size() == 1;
        const PdfPrintService::Result printResult =
            PdfPrintService::printPdfDocuments(
                {
                    request.parent,
                    stagedPdfPaths,
                    printingOneReport
                        ? QObject::tr("Print Speaking Evaluation Report")
                        : QObject::tr("Print Speaking Evaluation Reports"),
                    QPageLayout::Portrait,
                    QPageSize::A4,
                    true
                }
                );

        if (printResult.status == PdfPrintService::Status::Canceled)
        {
            return canceled();
        }

        if (printResult.status == PdfPrintService::Status::Failed)
        {
            return failed(printResult.message);
        }
    }

    return completed(
        savingIndividualPdfFiles
            ? targetPaths
            : QStringList(),
        targetArchivePath
        );
}

} // namespace SpeakingEvalBatchReportService
