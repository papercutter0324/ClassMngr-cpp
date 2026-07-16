#include "speaking_eval_batch_report_service.h"

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
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTime>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace SpeakingEvalBatchReportService
{
namespace
{

constexpr QSizeF ReportPageSizeInches(7.5, 10.833333);
constexpr int ReportPdfResolution = 144;
constexpr int PowerPointTimeoutMs = 5 * 60 * 1000;

#ifdef Q_OS_MACOS
QString powerPointWorkingDirectory()
{
    const QString homePath = QStandardPaths::writableLocation(
        QStandardPaths::HomeLocation
        );
    QDir powerPointDocuments(
        homePath + QStringLiteral(
                       "/Library/Containers/com.microsoft.Powerpoint/Data/Documents/ClassMngr"
                       )
        );
    if (!powerPointDocuments.exists() && !powerPointDocuments.mkpath(QStringLiteral(".")))
    {
        return {};
    }

    return powerPointDocuments.absolutePath();
}
#endif

QString safeFolderName(const QString& value, const QString& fallback)
{
    QString name = value.trimmed();
    name.replace(
        QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
        QStringLiteral("-")
        );
    name = name.simplified();
    return name.isEmpty() ? fallback : name;
}

QString shortDay(const QString& day)
{
    const QString normalized = day.trimmed().toLower();
    if (normalized.startsWith(QStringLiteral("mon"))) return QStringLiteral("M");
    if (normalized.startsWith(QStringLiteral("tue"))) return QStringLiteral("T");
    if (normalized.startsWith(QStringLiteral("wed"))) return QStringLiteral("W");
    if (normalized.startsWith(QStringLiteral("thu"))) return QStringLiteral("Th");
    if (normalized.startsWith(QStringLiteral("fri"))) return QStringLiteral("F");
    if (normalized.startsWith(QStringLiteral("sat"))) return QStringLiteral("Sa");
    if (normalized.startsWith(QStringLiteral("sun"))) return QStringLiteral("Su");
    return day.trimmed().left(2);
}

QString shortTime(const QString& value)
{
    const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("hh:mm AP"),
        QStringLiteral("hh:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm")
    };
    for (const QString& format : formats)
    {
        const QTime time = QTime::fromString(value.trimmed(), format);
        if (time.isValid())
        {
            QString result = time.toString(
                time.minute() == 0
                    ? QStringLiteral("hap")
                    : QStringLiteral("h:mmap")
                );
            return result.toLower();
        }
    }
    return value.trimmed().remove(QLatin1Char(' ')).toLower();
}

Result completed(
    const QStringList& savedPdfPaths = {}
    )
{
    return {
        Status::Completed,
        QObject::tr("Reports created successfully."),
        savedPdfPaths
    };
}

Result canceled()
{
    return {
        Status::Canceled,
        QObject::tr("Report export was canceled."),
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
        {}
    };
}

QPageLayout reportPageLayout()
{
    return QPageLayout(
        QPageSize(
            ReportPageSizeInches,
            QPageSize::Inch,
            QStringLiteral("SpeakingEvaluation")
            ),
        QPageLayout::Portrait,
        QMarginsF(),
        QPageLayout::Inch
        );
}

QRectF reportPageRect(
    const QPdfWriter& writer
    )
{
    const QRect pageRect =
        writer.pageLayout().fullRectPixels(
            std::max(1, writer.resolution())
            );

    if (!pageRect.isEmpty())
    {
        return QRectF(pageRect);
    }

    return QRectF(
        0.0,
        0.0,
        writer.width(),
        writer.height()
        );
}

bool renderInternalPdf(
    const SpeakingEvalReportData& data,
    const QString& documentPath,
    QString* errorMessage
    )
{
    QPdfWriter writer(documentPath);
    writer.setCreator(QStringLiteral("ClassMngr"));
    writer.setTitle(QObject::tr("Speaking Evaluation"));
    writer.setResolution(ReportPdfResolution);

    if (!writer.setPageLayout(reportPageLayout()))
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("Unable to configure the report PDF page.");
        }
        return false;
    }

    QPainter painter;
    if (!painter.begin(&writer))
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("Unable to create the report PDF.");
        }
        return false;
    }

    SpeakingEvalReportWidget report;
    report.setReportData(data);
    report.paintReport(
        &painter,
        reportPageRect(writer)
        );

    if (!painter.end())
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("The report PDF could not be completed.");
        }
        return false;
    }

    if (!QFileInfo::exists(documentPath)
        || QFileInfo(documentPath).size() <= 0)
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("The report PDF was not created.");
        }
        return false;
    }

    return true;
}

QString overallGrade(
    const std::array<QString, 6>& scores
    )
{
    const QHash<QString, int> gradeValues{
        { QStringLiteral("C"), 1 },
        { QStringLiteral("B"), 2 },
        { QStringLiteral("B+"), 3 },
        { QStringLiteral("A"), 4 },
        { QStringLiteral("A+"), 5 }
    };
    const QStringList grades{
        QStringLiteral("C"),
        QStringLiteral("B"),
        QStringLiteral("B+"),
        QStringLiteral("A"),
        QStringLiteral("A+")
    };

    int sum = 0;
    for (const QString& score : scores)
    {
        if (!gradeValues.contains(score))
        {
            return QStringLiteral("N/A");
        }

        sum += gradeValues.value(score);
    }

    const double average =
        static_cast<double>(sum) / scores.size();
    int rounded = static_cast<int>(average);
    if (average - rounded >= 0.4)
    {
        ++rounded;
    }

    return grades.value(
        qBound(1, rounded, 5) - 1,
        QStringLiteral("N/A")
        );
}

bool copyResourceToFile(
    const QString& resourcePath,
    const QString& targetPath,
    QString* errorMessage
    )
{
    QFile source(resourcePath);
    if (!source.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The PowerPoint template could not be opened.");
        }
        return false;
    }

    QSaveFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly)
        || target.write(source.readAll()) < 0
        || !target.commit())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("A temporary PowerPoint template could not be created.");
        }
        return false;
    }

    return true;
}

QJsonObject powerPointJob(
    const SpeakingEvalReportData& data,
    const QString& pptxPath,
    const QString& pdfPath
    )
{
    QJsonArray scores;
    for (const QString& score : data.scores)
    {
        scores.append(score);
    }

    return {
        { QStringLiteral("pptxPath"), QDir::toNativeSeparators(pptxPath) },
        { QStringLiteral("pdfPath"), QDir::toNativeSeparators(pdfPath) },
        { QStringLiteral("advanced"), data.useAdvancedTemplate },
        { QStringLiteral("englishName"), data.englishName },
        { QStringLiteral("koreanName"), data.koreanName },
        { QStringLiteral("classLabel"), data.classLabel },
        { QStringLiteral("nativeTeacher"), data.nativeTeacher },
        { QStringLiteral("koreanTeacher"), data.koreanTeacher },
        { QStringLiteral("date"), data.date },
        { QStringLiteral("comments"), data.comments },
        { QStringLiteral("overallGrade"), overallGrade(data.scores) },
        { QStringLiteral("scores"), scores }
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
QString powerShellExecutable()
{
    for (const QString& candidate : {
             QStringLiteral("powershell.exe"),
             QStringLiteral("powershell"),
             QStringLiteral("pwsh.exe"),
             QStringLiteral("pwsh")
             })
    {
        const QString executable =
            QStandardPaths::findExecutable(candidate);
        if (!executable.isEmpty())
        {
            return executable;
        }
    }

    return {};
}

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

function Set-Text($shapes, [string]$name, [string]$value) {
    $shape = Require-Shape $shapes $name
    $shape.TextFrame.TextRange.Text = $value
}

function Set-UnderlinedText($shapes, [string]$name, [string]$value) {
    $shape = Require-Shape $shapes $name
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

$data = Get-Content -LiteralPath $args[0] -Raw | ConvertFrom-Json
$pptxPath = [string]$data.pptxPath
$pdfPath = [string]$data.pdfPath
Write-Output ("ClassMngr PowerPoint export: $pptxPath -> $pdfPath")
$ppt = $null
$presentation = $null

try {
    $ppt = New-Object -ComObject PowerPoint.Application
    $presentation = $ppt.Presentations.Open($pptxPath, $false, $false, $false)
    $slide = $presentation.Slides.Item(1)
    $shapes = $slide.Shapes

    Set-UnderlinedText $shapes 'English_Name' ([string]$data.englishName)
    Set-UnderlinedText $shapes 'Korean_Name' ([string]$data.koreanName)
    Set-UnderlinedText $shapes 'Grade_Level' ([string]$data.classLabel)
    Set-UnderlinedText $shapes 'Native_Teacher' ([string]$data.nativeTeacher)
    Set-UnderlinedText $shapes 'Korean_Teacher' ([string]$data.koreanTeacher)
    Set-UnderlinedText $shapes 'Eval_Date' ([string]$data.date)
    Set-Text $shapes 'Comments' ([string]$data.comments)
    Set-Text $shapes 'Overall_Grade' ([string]$data.overallGrade)

    $grey = 14277081
    $yellow = 65535

    if ([bool]$data.advanced) {
        $table = Require-Table $shapes 'Report_Table' 12 7
        $headerRows = @(1, 3, 5, 7, 9, 11)
        $gradeColumns = @{ 'A+' = 3; 'A' = 4; 'B+' = 5; 'B' = 6; 'C' = 7 }

        for ($index = 0; $index -lt $headerRows.Count; ++$index) {
            foreach ($column in 3..7) {
                Set-Fill $table.Cell($headerRows[$index], $column).Shape $grey
            }
            $score = [string]$data.scores[$index]
            if ($gradeColumns.ContainsKey($score)) {
                Set-Fill $table.Cell($headerRows[$index], $gradeColumns[$score]).Shape $yellow
            }
        }
    }
    else {
        # In the standard template the visible table and its borders live on
        # the slide master.  The named score shapes on the slide are only
        # transparent text overlays.  Filling those overlays hides the table
        # borders, so apply the shading directly to the master-table cells.
        $table = Require-Table $slide.Master.Shapes 'Grades & Scores' 12 6
        $headerRows = @(1, 3, 5, 7, 9, 11)
        $gradeColumns = @{ 'A+' = 2; 'A' = 3; 'B+' = 4; 'B' = 5; 'C' = 6 }

        for ($index = 0; $index -lt $headerRows.Count; ++$index) {
            foreach ($column in 2..6) {
                Set-Fill $table.Cell($headerRows[$index], $column).Shape $grey
            }
            $score = [string]$data.scores[$index]
            if ($gradeColumns.ContainsKey($score)) {
                Set-Fill $table.Cell($headerRows[$index], $gradeColumns[$score]).Shape $yellow
            }
        }
    }

    $presentation.SaveAs($pdfPath, [int]32)
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

on run argv
    set pptxPath to item 1 of argv
    set isAdvanced to (item 2 of argv is "true")
    set jobCount to item 3 of argv as integer
    set argumentIndex to 4
    set greyFill to {217, 217, 217}
    set yellowFill to {255, 255, 0}
    set reportPresentation to missing value
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
            if isAdvanced then
                set greyFill to {229, 229, 231}
                set reportTableShape to my requireTable(reportSlide, "Report_Table", 12, 7)
                set firstGradeColumn to 3
            else
                -- The score-label shapes on the standard slide are
                -- transparent overlays. Fill the cells in the bordered table
                -- on the slide master directly.
                set reportMaster to «property SlMr» of reportSlide
                set reportTableShape to my requireTable(reportMaster, "Grades & Scores", 12, 6)
                set firstGradeColumn to 2
            end if

            repeat with jobIndex from 1 to jobCount
                set studentName to item argumentIndex of argv
                set pdfPath to item (argumentIndex + 1) of argv
                set englishName to item (argumentIndex + 2) of argv
                set koreanName to item (argumentIndex + 3) of argv
                set classLabel to item (argumentIndex + 4) of argv
                set nativeTeacher to item (argumentIndex + 5) of argv
                set koreanTeacher to item (argumentIndex + 6) of argv
                set evaluationDate to item (argumentIndex + 7) of argv
                set commentsText to item (argumentIndex + 8) of argv
                set overallGrade to item (argumentIndex + 9) of argv
                set scoreValues to items (argumentIndex + 10) thru (argumentIndex + 15) of argv
                set argumentIndex to argumentIndex + 16

                set exportStep to "updating report text for " & studentName
                my setShapeText(englishNameShape, englishName, true)
                my setShapeText(koreanNameShape, koreanName, true)
                my setShapeText(gradeLevelShape, classLabel, true)
                my setShapeText(nativeTeacherShape, nativeTeacher, true)
                my setShapeText(koreanTeacherShape, koreanTeacher, true)
                my setShapeText(evaluationDateShape, evaluationDate, true)
                my setShapeText(commentsShape, commentsText, false)
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
            end repeat

            close reportPresentation saving no
            set reportPresentation to missing value
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

bool renderPowerPointPdf(
    const SpeakingEvalReportData& data,
    const QString& documentPath,
    QTemporaryDir* stagingDirectory,
    QString* errorMessage
    )
{
    if (!stagingDirectory || !stagingDirectory->isValid())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The temporary report directory is unavailable.");
        }
        return false;
    }

    const QString workingDirectory =
        QDir(stagingDirectory->path()).filePath(
            QStringLiteral("powerpoint-%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
            );
    if (!QDir().mkpath(workingDirectory))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("A temporary PowerPoint directory could not be created.");
        }
        return false;
    }

    const QString pptxPath =
        QDir(workingDirectory).filePath(QStringLiteral("report.pptx"));
    const QString jobPath =
        QDir(workingDirectory).filePath(QStringLiteral("report.json"));
    const QString scriptPath =
        QDir(workingDirectory).filePath(
#ifdef Q_OS_WIN
            QStringLiteral("export.ps1")
#else
            QStringLiteral("export.applescript")
#endif
            );
    const QString resourcePath =
        data.useAdvancedTemplate
            ? QStringLiteral(":/assets/files/evaluations/SpeakingEvaluationTemplate_Advanced-Full.pptx")
            : QStringLiteral(":/assets/files/evaluations/SpeakingEvaluationTemplate-Full.pptx");

    if (!copyResourceToFile(resourcePath, pptxPath, errorMessage))
    {
        return false;
    }

    const QByteArray jobJson =
        QByteArrayLiteral("\xEF\xBB\xBF")
        + QJsonDocument(
            powerPointJob(data, pptxPath, documentPath)
            ).toJson(QJsonDocument::Compact);

    if (!writeUtf8File(jobPath, jobJson, errorMessage))
    {
        return false;
    }

    QProcess process;
    process.setWorkingDirectory(workingDirectory);

#ifdef Q_OS_WIN
    const QString executable = powerShellExecutable();
    if (executable.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = powerPointRendererAvailabilityMessage();
        }
        return false;
    }

    if (!writeUtf8File(
            scriptPath,
            windowsPowerPointScript().toUtf8(),
            errorMessage
            ))
    {
        return false;
    }

    process.start(
        executable,
        {
            QStringLiteral("-NoProfile"),
            QStringLiteral("-NonInteractive"),
            QStringLiteral("-ExecutionPolicy"),
            QStringLiteral("Bypass"),
            QStringLiteral("-File"),
            scriptPath,
            jobPath
        }
        );
#elif defined(Q_OS_MACOS)
    const QString executable =
        QStandardPaths::findExecutable(QStringLiteral("osascript"));
    if (executable.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = powerPointRendererAvailabilityMessage();
        }
        return false;
    }

    if (!writeUtf8File(
            scriptPath,
            macPowerPointScript().toUtf8(),
            errorMessage
            ))
    {
        return false;
    }

    process.start(
        executable,
        {
            scriptPath,
            pptxPath,
            data.useAdvancedTemplate
                ? QStringLiteral("true")
                : QStringLiteral("false"),
            QStringLiteral("1"),
            data.englishName,
            documentPath,
            data.englishName,
            data.koreanName,
            data.classLabel,
            data.nativeTeacher,
            data.koreanTeacher,
            data.date,
            data.comments,
            overallGrade(data.scores),
            data.scores[0],
            data.scores[1],
            data.scores[2],
            data.scores[3],
            data.scores[4],
            data.scores[5]
        }
        );
#else
    Q_UNUSED(scriptPath);
    Q_UNUSED(jobPath);
    if (errorMessage)
    {
        *errorMessage = powerPointRendererAvailabilityMessage();
    }
    return false;
#endif

    if (!process.waitForStarted())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("PowerPoint could not be started.");
        }
        return false;
    }

    if (!process.waitForFinished(PowerPointTimeoutMs))
    {
        process.kill();
        process.waitForFinished();
        if (errorMessage)
        {
            *errorMessage = QObject::tr("PowerPoint did not finish exporting the report.");
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        if (errorMessage)
        {
            const QString details =
                (QString::fromUtf8(process.readAllStandardError())
                 + QLatin1Char('\n')
                 + QString::fromUtf8(process.readAllStandardOutput()))
                    .trimmed();
            *errorMessage = details.isEmpty()
                ? QObject::tr("PowerPoint could not export the report.")
                : details;
        }
        return false;
    }

    if (!QFileInfo::exists(documentPath)
        || QFileInfo(documentPath).size() <= 0)
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("PowerPoint did not create a PDF report.");
        }
        return false;
    }

    return true;
}

#ifdef Q_OS_MACOS
bool renderMacPowerPointTemplateBatch(
    const QList<StudentReport>& reports,
    const QStringList& documentPaths,
    const QList<int>& reportIndexes,
    bool useAdvancedTemplate,
    QTemporaryDir* stagingDirectory,
    QString* errorMessage
    )
{
    if (reportIndexes.isEmpty())
    {
        return true;
    }

    const QString workingDirectory = powerPointWorkingDirectory();
    if (workingDirectory.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("PowerPoint's ClassMngr Documents folder could not be created.");
        }
        return false;
    }

    const QString exportId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString pptxPath =
        QDir(workingDirectory).filePath(
            QStringLiteral("report-%1.pptx").arg(exportId)
            );
    const QString scriptPath =
        QDir(workingDirectory).filePath(
            QStringLiteral("export-%1.applescript").arg(exportId)
            );
    const auto removeWorkingFiles = [&pptxPath, &scriptPath]()
    {
        QFile::remove(pptxPath);
        QFile::remove(scriptPath);
    };
    const QString resourcePath =
        useAdvancedTemplate
            ? QStringLiteral(":/assets/files/evaluations/SpeakingEvaluationTemplate_Advanced-Full.pptx")
            : QStringLiteral(":/assets/files/evaluations/SpeakingEvaluationTemplate-Full.pptx");

    if (!copyResourceToFile(resourcePath, pptxPath, errorMessage)
        || !writeUtf8File(
            scriptPath,
            macPowerPointScript().toUtf8(),
            errorMessage
            ))
    {
        removeWorkingFiles();
        return false;
    }

    const QString executable =
        QStandardPaths::findExecutable(QStringLiteral("osascript"));
    if (executable.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = powerPointRendererAvailabilityMessage();
        }
        removeWorkingFiles();
        return false;
    }

    QStringList arguments{
        scriptPath,
        pptxPath,
        useAdvancedTemplate
            ? QStringLiteral("true")
            : QStringLiteral("false"),
        QString::number(reportIndexes.size())
    };
    for (const int reportIndex : reportIndexes)
    {
        const StudentReport& student = reports.at(reportIndex);
        const SpeakingEvalReportData& data = student.report;
        arguments.append(QStringList{
            student.displayName,
            documentPaths.at(reportIndex),
            data.englishName,
            data.koreanName,
            data.classLabel,
            data.nativeTeacher,
            data.koreanTeacher,
            data.date,
            data.comments,
            overallGrade(data.scores),
            data.scores[0],
            data.scores[1],
            data.scores[2],
            data.scores[3],
            data.scores[4],
            data.scores[5]
        });
    }

    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    process.start(executable, arguments);
    if (!process.waitForStarted())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("PowerPoint could not be started.");
        }
        removeWorkingFiles();
        return false;
    }

    const qint64 timeout =
        static_cast<qint64>(PowerPointTimeoutMs)
        * std::max<qsizetype>(1, reportIndexes.size());
    if (!process.waitForFinished(
            static_cast<int>(
                std::min<qint64>(timeout, std::numeric_limits<int>::max())
                )
            ))
    {
        process.kill();
        process.waitForFinished();
        if (errorMessage)
        {
            *errorMessage = QObject::tr("PowerPoint did not finish exporting the reports.");
        }
        removeWorkingFiles();
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        if (errorMessage)
        {
            const QString details =
                (QString::fromUtf8(process.readAllStandardError())
                 + QLatin1Char('\n')
                 + QString::fromUtf8(process.readAllStandardOutput()))
                    .trimmed();
            *errorMessage = details.isEmpty()
                ? QObject::tr("PowerPoint could not export the reports.")
                : details;
        }
        removeWorkingFiles();
        return false;
    }

    for (const int reportIndex : reportIndexes)
    {
        const QString& documentPath = documentPaths.at(reportIndex);
        if (!QFileInfo::exists(documentPath)
            || QFileInfo(documentPath).size() <= 0)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr("PowerPoint did not create a PDF report for %1.")
                                    .arg(reports.at(reportIndex).displayName);
            }
            removeWorkingFiles();
            return false;
        }
    }

    removeWorkingFiles();
    return true;
}

bool renderMacPowerPointPdfs(
    const QList<StudentReport>& reports,
    const QStringList& documentPaths,
    QTemporaryDir* stagingDirectory,
    QString* errorMessage
    )
{
    if (!stagingDirectory || !stagingDirectory->isValid())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The temporary report directory is unavailable.");
        }
        return false;
    }

    QList<int> standardReportIndexes;
    QList<int> advancedReportIndexes;
    for (int index = 0; index < reports.size(); ++index)
    {
        (reports.at(index).report.useAdvancedTemplate
             ? advancedReportIndexes
             : standardReportIndexes)
            .append(index);
    }

    return renderMacPowerPointTemplateBatch(
               reports,
               documentPaths,
               standardReportIndexes,
               false,
               stagingDirectory,
               errorMessage
               )
        && renderMacPowerPointTemplateBatch(
            reports,
            documentPaths,
            advancedReportIndexes,
            true,
            stagingDirectory,
            errorMessage
            );
}
#endif

bool targetFilePaths(
    const Request& request,
    QStringList* targetPaths,
    QString* errorMessage
    )
{
    if (!targetPaths)
    {
        return false;
    }

    QDir outputDirectory(request.outputDirectory);
    if (!outputDirectory.exists() && !QDir().mkpath(outputDirectory.path()))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The selected PDF folder could not be created.");
        }
        return false;
    }

    targetPaths->clear();
    targetPaths->reserve(request.reports.size());
    QSet<QString> seenPaths;
    for (int index = 0; index < request.reports.size(); ++index)
    {
        const QString path = outputDirectory.filePath(
            safeFileName(
                request.reports.at(index).report.englishName,
                request.reports.at(index).report.koreanName
                )
            );

        if ((!request.overwriteExisting && QFileInfo::exists(path))
            || seenPaths.contains(path))
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr("A PDF named \"%1\" already exists.")
                    .arg(QFileInfo(path).fileName());
            }
            return false;
        }

        seenPaths.insert(path);
        targetPaths->append(path);
    }

    return true;
}

bool commitPdfFiles(
    const QStringList& stagedPaths,
    const QStringList& targetPaths,
    bool overwriteExisting,
    QString* errorMessage
    )
{
    if (stagedPaths.size() != targetPaths.size())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The staged report files are incomplete.");
        }
        return false;
    }

    QStringList backupTargets;
    backupTargets.reserve(targetPaths.size());
    for (const QString& targetPath : targetPaths)
    {
        if (!overwriteExisting || !QFileInfo::exists(targetPath))
        {
            backupTargets.append(QString());
            continue;
        }

        const QString backupPath = targetPath + QStringLiteral(".classmngr-backup");
        QFile::remove(backupPath);
        if (!QFile::rename(targetPath, backupPath))
        {
            for (int index = 0; index < backupTargets.size(); ++index)
            {
                if (!backupTargets.at(index).isEmpty())
                {
                    QFile::rename(backupTargets.at(index), targetPaths.at(index));
                }
            }
            if (errorMessage)
            {
                *errorMessage = QObject::tr("An existing PDF could not be prepared for replacement.");
            }
            return false;
        }
        backupTargets.append(backupPath);
    }

    QStringList temporaryTargets;
    for (int index = 0; index < stagedPaths.size(); ++index)
    {
        const QString temporaryTarget =
            targetPaths.at(index) + QStringLiteral(".classmngr-part");
        QFile::remove(temporaryTarget);

        if (!QFile::copy(stagedPaths.at(index), temporaryTarget))
        {
            for (const QString& createdPath : temporaryTargets)
            {
                QFile::remove(createdPath);
            }
            for (int backupIndex = 0; backupIndex < backupTargets.size(); ++backupIndex)
            {
                if (!backupTargets.at(backupIndex).isEmpty())
                {
                    QFile::rename(backupTargets.at(backupIndex), targetPaths.at(backupIndex));
                }
            }
            if (errorMessage)
            {
                *errorMessage = QObject::tr("A PDF could not be copied to the selected folder.");
            }
            return false;
        }

        temporaryTargets.append(temporaryTarget);
    }

    QStringList committedTargets;
    for (int index = 0; index < temporaryTargets.size(); ++index)
    {
        if (!QFile::rename(temporaryTargets.at(index), targetPaths.at(index)))
        {
            for (const QString& createdPath : temporaryTargets)
            {
                QFile::remove(createdPath);
            }
            for (const QString& committedPath : committedTargets)
            {
                QFile::remove(committedPath);
            }
            for (int backupIndex = 0; backupIndex < backupTargets.size(); ++backupIndex)
            {
                if (!backupTargets.at(backupIndex).isEmpty())
                {
                    QFile::rename(backupTargets.at(backupIndex), targetPaths.at(backupIndex));
                }
            }
            if (errorMessage)
            {
                *errorMessage = QObject::tr("A PDF could not be finalized in the selected folder.");
            }
            return false;
        }

        committedTargets.append(targetPaths.at(index));
    }

    for (const QString& backupPath : backupTargets)
    {
        if (!backupPath.isEmpty())
        {
            QFile::remove(backupPath);
        }
    }

    return true;
}

} // namespace

QString rendererDisplayName(
    Renderer renderer
    )
{
    switch (renderer)
    {
    case Renderer::Internal:
        return QObject::tr("Internal Template");
    case Renderer::PowerPoint:
        return QObject::tr("PowerPoint Template (Recommended)");
    }

    return {};
}

QString defaultOutputDirectory(
    const ClassInfo& classInfo,
    const QString& evaluationName,
    const QString& documentsDirectory
    )
{
    QString className = QStringList{
        classInfo.classGrade.trimmed(),
        classInfo.classLevel.trimmed()
    }.filter(QRegularExpression(QStringLiteral(".+"))).join(QLatin1Char(' '));
    if (className.isEmpty())
    {
        className = QObject::tr("Speaking Evaluation");
    }

    QStringList days;
    for (const ClassTime& classTime : classInfo.classTimes)
    {
        const QString day = shortDay(classTime.day);
        if (!day.isEmpty() && !days.contains(day))
        {
            days.append(day);
        }
    }

    QString schedule;
    if (!classInfo.classTimes.isEmpty())
    {
        const QString time = shortTime(classInfo.classTimes.first().startTime);
        if (!days.isEmpty() && !time.isEmpty())
        {
            schedule = QStringLiteral("%1 - %2")
                .arg(days.join(QString()), time);
        }
        else
        {
            schedule = !days.isEmpty() ? days.join(QString()) : time;
        }
    }
    if (!schedule.trimmed().isEmpty())
    {
        className += QStringLiteral(" (%1)").arg(schedule);
    }

    const QString root = documentsDirectory.trimmed().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : documentsDirectory;
    QDir directory(root);
    return QDir::cleanPath(
        directory.filePath(
            QStringLiteral("DYB/SpeakingEvals/%1/%2")
                .arg(
                    safeFolderName(className, QObject::tr("Speaking Evaluation")),
                    safeFolderName(evaluationName, QObject::tr("Evaluation"))
                    )
            )
        );
}

QString safeFileName(
    const QString& englishName,
    const QString& koreanName
    )
{
    QString baseName;
    const QString english = englishName.trimmed();
    const QString korean = koreanName.trimmed();
    if (!english.isEmpty() && !korean.isEmpty())
    {
        baseName = QStringLiteral("%1 (%2)").arg(english, korean);
    }
    else
    {
        baseName = !english.isEmpty() ? english : korean;
    }
    baseName.replace(
        QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
        QStringLiteral("-")
        );
    baseName = baseName.simplified();
    if (baseName.isEmpty())
    {
        baseName = QObject::tr("Student");
    }

    return QStringLiteral("%1.pdf").arg(baseName);
}

bool isPowerPointRendererAvailable()
{
#ifdef Q_OS_WIN
    QSettings powerPointRegistry(
        QStringLiteral("HKEY_CLASSES_ROOT\\PowerPoint.Application"),
        QSettings::NativeFormat
        );
    return !powerShellExecutable().isEmpty()
        && (!powerPointRegistry.allKeys().isEmpty()
            || !powerPointRegistry.childGroups().isEmpty());
#elif defined(Q_OS_MACOS)
    return QFileInfo::exists(
        QStringLiteral("/Applications/Microsoft PowerPoint.app")
        ) && !QStandardPaths::findExecutable(
            QStringLiteral("osascript")
            ).isEmpty();
#else
    return false;
#endif
}

QString powerPointRendererAvailabilityMessage()
{
#ifdef Q_OS_WIN
    return QObject::tr(
        "PowerPoint export requires the installed desktop Microsoft PowerPoint application."
        );
#elif defined(Q_OS_MACOS)
    return QObject::tr(
        "PowerPoint export requires Microsoft PowerPoint in /Applications and macOS Automation permission."
        );
#else
    return QObject::tr(
        "PowerPoint template export is available only on Windows and macOS. Use the internal report on this platform."
        );
#endif
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

    if (request.savePdf && request.outputDirectory.trimmed().isEmpty())
    {
        return failed(QObject::tr("Choose a folder for the PDF reports."));
    }

    if (request.renderer == Renderer::PowerPoint
        && !isPowerPointRendererAvailable())
    {
        return failed(powerPointRendererAvailabilityMessage());
    }

    QStringList targetPaths;
    QString errorMessage;
    if (request.savePdf
        && !targetFilePaths(request, &targetPaths, &errorMessage))
    {
        return failed(errorMessage);
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
    bool pdfsSavedDirectlyToTarget = false;
#ifdef Q_OS_MACOS
    if (request.renderer == Renderer::PowerPoint && request.savePdf)
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

            stagedPdfPaths.append(
                targetPaths.at(index)
                );
        }

        if (!renderMacPowerPointPdfs(
                request.reports,
                stagedPdfPaths,
                &stagingDirectory,
                &errorMessage
                ))
        {
            return failed(errorMessage);
        }
        pdfsSavedDirectlyToTarget = true;
    }
    else
#endif
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
                request.renderer == Renderer::Internal
                    ? renderInternalPdf(student.report, stagedPath, &errorMessage)
                    : renderPowerPointPdf(
                        student.report,
                        stagedPath,
                        &stagingDirectory,
                        &errorMessage
                        );

            if (!rendered)
            {
                return failed(
                    QObject::tr("%1: %2")
                        .arg(student.displayName, errorMessage),
                    request.renderer == Renderer::Internal
                    );
            }

            stagedPdfPaths.append(stagedPath);
        }
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

    if (request.savePdf && !pdfsSavedDirectlyToTarget
        && !commitPdfFiles(
            stagedPdfPaths,
            targetPaths,
            request.overwriteExisting,
            &errorMessage
            ))
    {
        return failed(errorMessage);
    }

    if (request.printReports)
    {
        const PdfPrintService::Result printResult =
            PdfPrintService::printPdfDocuments(
                {
                    request.parent,
                    stagedPdfPaths,
                    QObject::tr("Print Speaking Evaluation Reports"),
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

    return completed(targetPaths);
}

} // namespace SpeakingEvalBatchReportService
