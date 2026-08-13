#include "speaking_eval_powerpoint_scripts.h"

#include "speaking_eval_powerpoint_job_model.h"

namespace SpeakingEvalPowerPointScripts
{
QString macTextArgument(
    const QString& value
    )
{
    return QString::fromLatin1(
        SpeakingEvalPowerPointJobModel::normalizedText(value)
            .toUtf8()
            .toBase64()
        );
}

#ifdef Q_OS_WIN
QString windowsScript()
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
QString macScript()
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
}
