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

namespace SpeakingEvalBatchReportService
{
namespace
{

constexpr QSizeF ReportPageSizeInches(7.5, 10.833333);
constexpr int ReportPdfResolution = 144;
constexpr int PowerPointTimeoutMs = 120000;

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

on setCellFill(tableShape, rowIndex, columnIndex, fillColor)
    tell application "Microsoft PowerPoint"
        set cellShape to shape of cell columnIndex of row rowIndex of table of tableShape
        set fore color of fill format of cellShape to fillColor
    end tell
end setCellFill

on run argv
    set pptxPath to item 1 of argv
    set pdfPath to item 2 of argv
    set englishName to item 3 of argv
    set koreanName to item 4 of argv
    set classLabel to item 5 of argv
    set nativeTeacher to item 6 of argv
    set koreanTeacher to item 7 of argv
    set evaluationDate to item 8 of argv
    set commentsText to item 9 of argv
    set overallGrade to item 10 of argv
    set isAdvanced to (item 11 of argv is "true")
    set scoreValues to items 12 thru 17 of argv
    set greyFill to {217, 217, 217}
    set yellowFill to {255, 255, 0}
    set reportPresentation to missing value

    tell application "Microsoft PowerPoint"
        try
            set reportPresentation to open (POSIX file pptxPath)
            set reportSlide to slide 1 of reportPresentation
            tell reportSlide
                set content of text range of text frame of shape "English_Name" to englishName
                set content of text range of text frame of shape "Korean_Name" to koreanName
                set content of text range of text frame of shape "Grade_Level" to classLabel
                set content of text range of text frame of shape "Native_Teacher" to nativeTeacher
                set content of text range of text frame of shape "Korean_Teacher" to koreanTeacher
                set content of text range of text frame of shape "Eval_Date" to evaluationDate
                set content of text range of text frame of shape "Comments" to commentsText
                set content of text range of text frame of shape "Overall_Grade" to overallGrade
            end tell

            if isAdvanced then
                set reportTableShape to shape "Report_Table" of reportSlide
                set firstGradeColumn to 3
            else
                -- The score-label shapes on the standard slide are
                -- transparent overlays.  Fill the bordered master table.
                set reportTableShape to shape "Grades & Scores" of master of reportSlide
                set firstGradeColumn to 2
            end if

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

            save reportPresentation in (POSIX file pdfPath) as save as PDF
            close reportPresentation saving no
        on error errorMessage number errorNumber
            if reportPresentation is not missing value then
                try
                    close reportPresentation saving no
                end try
            end if
            error errorMessage number errorNumber
        end try
    end tell
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
            documentPath,
            data.englishName,
            data.koreanName,
            data.classLabel,
            data.nativeTeacher,
            data.koreanTeacher,
            data.date,
            data.comments,
            overallGrade(data.scores),
            data.useAdvancedTemplate
                ? QStringLiteral("true")
                : QStringLiteral("false"),
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
                request.reports.at(index).displayName,
                index + 1
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
    const QString& displayName,
    int sequenceNumber
    )
{
    QString baseName = displayName.trimmed();
    baseName.replace(
        QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
        QStringLiteral("-")
        );
    baseName = baseName.simplified();
    if (baseName.isEmpty())
    {
        baseName = QObject::tr("Student");
    }

    return QStringLiteral("%1 - %2.pdf")
        .arg(sequenceNumber, 3, 10, QLatin1Char('0'))
        .arg(baseName);
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
                safeFileName(student.displayName, index + 1)
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

    if (request.progressCallback
        && !request.progressCallback(
            request.reports.size(),
            request.reports.size(),
            QString()
            ))
    {
        return canceled();
    }

    if (request.savePdf
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
