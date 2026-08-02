param(
    [switch]$Check,
    [switch]$BootstrapPlaceholders
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'Speaking-evaluation PNG assets can currently be generated only on Windows.'
}

Add-Type -AssemblyName System.Drawing

$repositoryRoot = [IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '..\..')
)
$resourceOutput = Join-Path `
    $repositoryRoot `
    'resources\assets\templates\speaking-eval'
$sourceRoot = Join-Path $resourceOutput 'sources'
$assetSpecPath = Join-Path $sourceRoot 'asset-spec.json'
$workRoot = Join-Path `
    ([IO.Path]::GetTempPath()) `
    ('classmngr-speaking-eval-svg-' + [guid]::NewGuid().ToString('N'))

$logicalWidth = 540.0
$logicalHeight = 780.0
$recommendedDpi = 300
$selectedYellow = '#FFFF00'
$neutralGrey = [Drawing.Color]::FromArgb(255, 217, 217, 217)
$grades = [ordered]@{
    'A+' = 'aplus'
    'A' = 'a'
    'B+' = 'bplus'
    'B' = 'b'
    'C' = 'c'
}
$overallGrades = [ordered]@{
    'A+' = 'aplus'
    'A' = 'a'
    'B+' = 'bplus'
    'B' = 'b'
    'C' = 'c'
    'N/A' = 'na'
}
$metricKeys = @(
    'grammar',
    'pronunciation',
    'fluency',
    'manner',
    'content',
    'overall-effort'
)
$fieldIds = [ordered]@{
    englishName = 'field-english-name'
    koreanName = 'field-korean-name'
    classLabel = 'field-class-label'
    nativeTeacher = 'field-native-teacher'
    koreanTeacher = 'field-korean-teacher'
    date = 'field-date'
    comments = 'field-comments'
}

function Number-Text {
    param([Parameter(Mandatory)][double]$Value)

    return $Value.ToString(
        '0.########',
        [Globalization.CultureInfo]::InvariantCulture
    )
}

function Number-Array {
    param(
        [Parameter(Mandatory)]$Value,
        [Parameter(Mandatory)][int]$Count,
        [Parameter(Mandatory)][string]$Context
    )

    $items = @($Value)
    if ($items.Count -ne $Count) {
        throw "$Context must contain $Count numbers."
    }

    return @(
        $items |
            ForEach-Object {
                $number = [double]$_
                if ([double]::IsNaN($number) -or [double]::IsInfinity($number)) {
                    throw "$Context contains a non-finite number."
                }
                $number
            }
    )
}

function Read-Json {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required JSON file is missing: $Path"
    }
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Write-Json {
    param(
        [Parameter(Mandatory)]$Value,
        [Parameter(Mandatory)][string]$Path
    )

    $json = $Value | ConvertTo-Json -Depth 30
    [IO.File]::WriteAllText(
        $Path,
        $json + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false)
    )
}

function Load-Bitmap {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required image is missing: $Path"
    }
    $loaded = [Drawing.Bitmap]::FromFile($Path)
    try {
        return [Drawing.Bitmap]::new($loaded)
    }
    finally {
        $loaded.Dispose()
    }
}

function Save-Png {
    param(
        [Parameter(Mandatory)][Drawing.Bitmap]$Bitmap,
        [Parameter(Mandatory)][string]$Path
    )

    $directory = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $Bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
}

function Crop-Bitmap {
    param(
        [Parameter(Mandatory)][Drawing.Bitmap]$Bitmap,
        [Parameter(Mandatory)][Drawing.Rectangle]$Rectangle
    )

    if (
        $Rectangle.Width -le 0 -or
        $Rectangle.Height -le 0 -or
        $Rectangle.Left -lt 0 -or
        $Rectangle.Top -lt 0 -or
        $Rectangle.Right -gt $Bitmap.Width -or
        $Rectangle.Bottom -gt $Bitmap.Height
    ) {
        throw "Invalid image crop: $Rectangle"
    }

    $result = [Drawing.Bitmap]::new(
        $Rectangle.Width,
        $Rectangle.Height,
        [Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    $graphics = [Drawing.Graphics]::FromImage($result)
    try {
        $graphics.CompositingMode =
            [Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.DrawImage(
            $Bitmap,
            [Drawing.Rectangle]::new(
                0,
                0,
                $Rectangle.Width,
                $Rectangle.Height
            ),
            $Rectangle,
            [Drawing.GraphicsUnit]::Pixel
        )
    }
    finally {
        $graphics.Dispose()
    }
    return $result
}

function Crop-Transparent-Bounds {
    param([Parameter(Mandatory)][Drawing.Bitmap]$Bitmap)

    $left = $Bitmap.Width
    $top = $Bitmap.Height
    $right = -1
    $bottom = -1
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            if ($Bitmap.GetPixel($x, $y).A -le 2) {
                continue
            }
            $left = [Math]::Min($left, $x)
            $top = [Math]::Min($top, $y)
            $right = [Math]::Max($right, $x)
            $bottom = [Math]::Max($bottom, $y)
        }
    }

    if ($right -lt $left -or $bottom -lt $top) {
        throw 'The transparent label image contains no visible pixels.'
    }

    return Crop-Bitmap `
        -Bitmap $Bitmap `
        -Rectangle ([Drawing.Rectangle]::FromLTRB(
            $left,
            $top,
            $right + 1,
            $bottom + 1
        ))
}

function Extract-Score-Label {
    param(
        [Parameter(Mandatory)][Drawing.Bitmap]$SpriteSheet,
        [Parameter(Mandatory)][Drawing.Rectangle]$Source
    )

    $inset = [Math]::Min(
        3,
        [Math]::Floor([Math]::Min($Source.Width, $Source.Height) / 6)
    )
    $interior = [Drawing.Rectangle]::new(
        $Source.X + $inset,
        $Source.Y + $inset,
        $Source.Width - (2 * $inset),
        $Source.Height - (2 * $inset)
    )
    $label = [Drawing.Bitmap]::new(
        $interior.Width,
        $interior.Height,
        [Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    for ($y = 0; $y -lt $interior.Height; ++$y) {
        for ($x = 0; $x -lt $interior.Width; ++$x) {
            $color = $SpriteSheet.GetPixel(
                $interior.X + $x,
                $interior.Y + $y
            )
            $alpha = 255 - [Math]::Max($color.R, $color.G)
            if ($alpha -lt 3) {
                $alpha = 0
            }
            $label.SetPixel(
                $x,
                $y,
                [Drawing.Color]::FromArgb($alpha, 0, 0, 0)
            )
        }
    }

    try {
        return Crop-Transparent-Bounds -Bitmap $label
    }
    finally {
        $label.Dispose()
    }
}

function Image-Has-Transparency {
    param([Parameter(Mandatory)][Drawing.Bitmap]$Bitmap)

    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            if ($Bitmap.GetPixel($x, $y).A -lt 255) {
                return $true
            }
        }
    }
    return $false
}

function Image-Has-Visible-Pixel {
    param([Parameter(Mandatory)][Drawing.Bitmap]$Bitmap)

    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            if ($Bitmap.GetPixel($x, $y).A -gt 2) {
                return $true
            }
        }
    }
    return $false
}

function Image-Rectangle {
    param(
        [Parameter(Mandatory)]$Values,
        [Parameter(Mandatory)][string]$Context
    )

    $numbers = Number-Array -Value $Values -Count 4 -Context $Context
    return [Drawing.Rectangle]::new(
        [int][Math]::Round($numbers[0]),
        [int][Math]::Round($numbers[1]),
        [int][Math]::Round($numbers[2]),
        [int][Math]::Round($numbers[3])
    )
}

function Rounded-Array {
    param(
        [Parameter(Mandatory)][double[]]$Values,
        [int]$Digits = 6
    )

    return @(
        $Values |
            ForEach-Object { [Math]::Round($_, $Digits) }
    )
}

function Minimum-Pixels {
    param([Parameter(Mandatory)][double[]]$PointSize)

    return @(
        [int][Math]::Ceiling($PointSize[0] * $recommendedDpi / 72.0),
        [int][Math]::Ceiling($PointSize[1] * $recommendedDpi / 72.0)
    )
}

function Write-Authoring-Map {
    param(
        [Parameter(Mandatory)][string]$Kind,
        [Parameter(Mandatory)]$Manifest,
        [Parameter(Mandatory)][string]$Path
    )

    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('<?xml version="1.0" encoding="UTF-8"?>')
    $lines.Add(
        '<svg xmlns="http://www.w3.org/2000/svg" ' +
        'xmlns:xlink="http://www.w3.org/1999/xlink" ' +
        'width="7.5in" height="10.833333in" viewBox="0 0 540 780">'
    )
    $lines.Add(
        '  <image id="template-background" href="background-clean.png" ' +
        'x="0" y="0" width="540" height="780" preserveAspectRatio="none"/>'
    )
    $lines.Add('  <g id="dynamic-map" fill="none" pointer-events="none">')

    foreach ($property in $fieldIds.GetEnumerator()) {
        $field = $Manifest.fields.($property.Key)
        $rect = Number-Array `
            -Value $field.rect `
            -Count 4 `
            -Context "$Kind $($property.Key) rectangle"
        $margins = Number-Array `
            -Value $field.margins `
            -Count 4 `
            -Context "$Kind $($property.Key) margins"
        $marginText = (
            $margins |
                ForEach-Object { Number-Text $_ }
        ) -join ' '
        $lines.Add(
            ('    <rect id="{0}" data-role="field" data-field="{1}" ' +
             'x="{2}" y="{3}" width="{4}" height="{5}" ' +
             'data-margins="{6}" data-font-role="{7}" ' +
             'data-font-size-pt="{8}" data-align-h="{9}" ' +
             'data-align-v="{10}" data-fit="{11}" ' +
             'data-minimum-scale="0.7" data-letter-spacing="0" ' +
             'data-word-spacing="0" data-line-height="1" ' +
             'data-baseline-offset="0"/>') -f @(
                $property.Value,
                $property.Key,
                (Number-Text $rect[0]),
                (Number-Text $rect[1]),
                (Number-Text $rect[2]),
                (Number-Text $rect[3]),
                $marginText,
                [string]$field.fontRole,
                (Number-Text ([double]$field.pixelSize)),
                [string]$field.horizontalAlignment,
                [string]$field.verticalAlignment,
                [string]$field.fit
            )
        )
    }

    $rows = @($Manifest.scoreTable.rows)
    if ($rows.Count -ne $metricKeys.Count) {
        throw "$Kind must contain six score rows."
    }
    foreach ($metricIndex in 0..($metricKeys.Count - 1)) {
        $row = Number-Array `
            -Value $rows[$metricIndex] `
            -Count 2 `
            -Context "$Kind score row $metricIndex"
        foreach ($gradeEntry in $grades.GetEnumerator()) {
            $column = Number-Array `
                -Value $Manifest.scoreTable.columns.($gradeEntry.Key) `
                -Count 2 `
                -Context "$Kind $($gradeEntry.Key) score column"
            $lines.Add(
                ('    <rect id="score-{0}-{1}" data-role="score-cell" ' +
                 'data-metric="{0}" data-grade="{2}" x="{3}" y="{4}" ' +
                 'width="{5}" height="{6}"/>') -f @(
                    $metricKeys[$metricIndex],
                    $gradeEntry.Value,
                    $gradeEntry.Key,
                    (Number-Text $column[0]),
                    (Number-Text $row[0]),
                    (Number-Text $column[1]),
                    (Number-Text $row[1])
                )
            )
        }
    }

    $overallRect = Number-Array `
        -Value $Manifest.overallGrades.'A+'.destination `
        -Count 4 `
        -Context "$Kind overall grade rectangle"
    $signatureRect = Number-Array `
        -Value $Manifest.signatureBounds `
        -Count 4 `
        -Context "$Kind signature rectangle"
    $lines.Add(
        ('    <rect id="field-overall-grade" data-role="overall-grade" ' +
         'x="{0}" y="{1}" width="{2}" height="{3}"/>') -f @(
            (Number-Text $overallRect[0]),
            (Number-Text $overallRect[1]),
            (Number-Text $overallRect[2]),
            (Number-Text $overallRect[3])
        )
    )
    $lines.Add(
        ('    <rect id="field-signature" data-role="signature" ' +
         'x="{0}" y="{1}" width="{2}" height="{3}"/>') -f @(
            (Number-Text $signatureRect[0]),
            (Number-Text $signatureRect[1]),
            (Number-Text $signatureRect[2]),
            (Number-Text $signatureRect[3])
        )
    )
    $lines.Add('  </g>')
    $lines.Add('</svg>')

    [IO.File]::WriteAllText(
        $Path,
        ($lines -join [Environment]::NewLine) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false)
    )
}

function Bootstrap-Template {
    param(
        [Parameter(Mandatory)][string]$Kind,
        [Parameter(Mandatory)][Collections.IDictionary]$SpecTemplates
    )

    $manifestPath = Join-Path $resourceOutput "$Kind-manifest.json"
    $backgroundPath = Join-Path $resourceOutput "$Kind-background.png"
    $spritePath = Join-Path $resourceOutput "$Kind-sprites.png"
    $manifest = Read-Json -Path $manifestPath
    if ($manifest.version -ne 2) {
        throw "Placeholder bootstrap requires the existing version 2 $Kind manifest."
    }

    $templateSource = Join-Path $sourceRoot $Kind
    New-Item -ItemType Directory -Path $templateSource -Force | Out-Null
    $backgroundOutput = Join-Path $templateSource 'background-clean.png'
    $mapOutput = Join-Path $templateSource 'map.svg'
    $background = Load-Bitmap -Path $backgroundPath
    $sprites = Load-Bitmap -Path $spritePath
    try {
        $logicalSize = Number-Array `
            -Value $manifest.logicalSize `
            -Count 2 `
            -Context "$Kind logical size"
        $scaleX = $background.Width / $logicalSize[0]
        $scaleY = $background.Height / $logicalSize[1]
        $graphics = [Drawing.Graphics]::FromImage($background)
        $brush = [Drawing.SolidBrush]::new($neutralGrey)
        try {
            foreach ($rowValue in @($manifest.scoreTable.rows)) {
                $row = Number-Array `
                    -Value $rowValue `
                    -Count 2 `
                    -Context "$Kind score row"
                foreach ($grade in $grades.Keys) {
                    $column = Number-Array `
                        -Value $manifest.scoreTable.columns.$grade `
                        -Count 2 `
                        -Context "$Kind $grade score column"
                    $rect = [Drawing.Rectangle]::new(
                        [int][Math]::Floor($column[0] * $scaleX) + 3,
                        [int][Math]::Floor($row[0] * $scaleY) + 3,
                        [Math]::Max(
                            1,
                            [int][Math]::Ceiling($column[1] * $scaleX) - 6
                        ),
                        [Math]::Max(
                            1,
                            [int][Math]::Ceiling($row[1] * $scaleY) - 6
                        )
                    )
                    $graphics.FillRectangle($brush, $rect)
                }
            }
        }
        finally {
            $brush.Dispose()
            $graphics.Dispose()
        }
        Save-Png -Bitmap $background -Path $backgroundOutput

        $spriteScale = Number-Array `
            -Value $manifest.spriteRasterScale `
            -Count 2 `
            -Context "$Kind sprite scale"
        $scoreLabelSpec = [ordered]@{}
        foreach ($entry in $grades.GetEnumerator()) {
            $source = Image-Rectangle `
                -Values $manifest.scoreTable.highlights.($entry.Key).source `
                -Context "$Kind $($entry.Key) highlight source"
            $label = Extract-Score-Label `
                -SpriteSheet $sprites `
                -Source $source
            try {
                $fileName = "score-label-$($entry.Value).png"
                Save-Png `
                    -Bitmap $label `
                    -Path (Join-Path $templateSource $fileName)
                $pointSize = Rounded-Array -Values @(
                    ([double]$label.Width / [double]$spriteScale[0]),
                    ([double]$label.Height / [double]$spriteScale[1])
                )
                $scoreLabelSpec[$entry.Key] = [ordered]@{
                    file = "$Kind/$fileName"
                    pointSize = $pointSize
                    minimumRecommendedPixels = Minimum-Pixels $pointSize
                }
            }
            finally {
                $label.Dispose()
            }
        }

        $overallLabelSpec = [ordered]@{}
        foreach ($entry in $overallGrades.GetEnumerator()) {
            $source = Image-Rectangle `
                -Values $manifest.overallGrades.($entry.Key).source `
                -Context "$Kind $($entry.Key) overall source"
            $crop = Crop-Bitmap -Bitmap $sprites -Rectangle $source
            try {
                $label = Crop-Transparent-Bounds -Bitmap $crop
                try {
                    $fileName = "overall-grade-$($entry.Value).png"
                    Save-Png `
                        -Bitmap $label `
                        -Path (Join-Path $templateSource $fileName)
                    $pointSize = Rounded-Array -Values @(
                        ([double]$label.Width / [double]$spriteScale[0]),
                        ([double]$label.Height / [double]$spriteScale[1])
                    )
                    $overallLabelSpec[$entry.Key] = [ordered]@{
                        file = "$Kind/$fileName"
                        pointSize = $pointSize
                        minimumRecommendedPixels = Minimum-Pixels $pointSize
                    }
                }
                finally {
                    $label.Dispose()
                }
            }
            finally {
                $crop.Dispose()
            }
        }

        Write-Authoring-Map `
            -Kind $Kind `
            -Manifest $manifest `
            -Path $mapOutput
        $SpecTemplates[$Kind] = [ordered]@{
            map = "$Kind/map.svg"
            background = "$Kind/background-clean.png"
            minimumRecommendedBackgroundPixels = @(2250, 3250)
            scoreLabels = $scoreLabelSpec
            overallGrades = $overallLabelSpec
        }
    }
    finally {
        $background.Dispose()
        $sprites.Dispose()
    }
}

function Bootstrap-Placeholder-Sources {
    New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null
    $templates = [ordered]@{}
    Bootstrap-Template -Kind 'standard' -SpecTemplates $templates
    Bootstrap-Template -Kind 'advanced' -SpecTemplates $templates
    $spec = [ordered]@{
        version = 1
        placeholderArtwork = $true
        units = 'pt'
        pageSizePoints = @($logicalWidth, $logicalHeight)
        physicalSizeInches = @(7.5, 10.833333)
        outputDpi = $recommendedDpi
        highlightColor = $selectedYellow
        highlightInsetPoints = 1.0
        templates = $templates
    }
    Write-Json -Value $spec -Path $assetSpecPath
    Write-Output "Bootstrapped replaceable placeholder artwork in '$sourceRoot'."
}

function Xml-Attribute {
    param(
        [Parameter(Mandatory)][Xml.XmlElement]$Element,
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Context
    )

    $value = $Element.GetAttribute($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "$Context is missing '$Name'."
    }
    return $value
}

function Xml-Number {
    param(
        [Parameter(Mandatory)][Xml.XmlElement]$Element,
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Context
    )

    $text = Xml-Attribute -Element $Element -Name $Name -Context $Context
    $value = 0.0
    if (-not [double]::TryParse(
            $text,
            [Globalization.NumberStyles]::Float,
            [Globalization.CultureInfo]::InvariantCulture,
            [ref]$value
        ) -or [double]::IsNaN($value) -or [double]::IsInfinity($value)
    ) {
        throw "$Context has an invalid '$Name' value."
    }
    return $value
}

function Xml-Optional-Number {
    param(
        [Parameter(Mandatory)][Xml.XmlElement]$Element,
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][double]$Default,
        [Parameter(Mandatory)][string]$Context
    )

    if (-not $Element.HasAttribute($Name)) {
        return $Default
    }
    return Xml-Number -Element $Element -Name $Name -Context $Context
}

function Map-Rectangle {
    param(
        [Parameter(Mandatory)][Xml.XmlElement]$Element,
        [Parameter(Mandatory)][string]$Context
    )

    $rect = @(
        (Xml-Number $Element 'x' $Context),
        (Xml-Number $Element 'y' $Context),
        (Xml-Number $Element 'width' $Context),
        (Xml-Number $Element 'height' $Context)
    )
    if (
        $rect[2] -le 0.0 -or $rect[3] -le 0.0 -or
        $rect[0] -lt 0.0 -or $rect[1] -lt 0.0 -or
        $rect[0] + $rect[2] -gt $logicalWidth + 0.001 -or
        $rect[1] + $rect[3] -gt $logicalHeight + 0.001
    ) {
        throw "$Context lies outside the 540 by 780 point page."
    }
    return Rounded-Array $rect
}

function Read-Authoring-Map {
    param(
        [Parameter(Mandatory)][string]$Kind,
        [Parameter(Mandatory)][string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "The $Kind SVG map is missing: $Path"
    }
    [xml]$document = Get-Content -LiteralPath $Path -Raw
    $svg = $document.DocumentElement
    if (
        $null -eq $svg -or
        $svg.LocalName -ne 'svg' -or
        $svg.GetAttribute('width') -ne '7.5in' -or
        $svg.GetAttribute('height') -ne '10.833333in' -or
        $svg.GetAttribute('viewBox') -ne '0 0 540 780'
    ) {
        throw "The $Kind SVG map must use a 7.5in by 10.833333in 540 by 780 point viewBox."
    }

    $backgroundElements = @(
        $document.GetElementsByTagName('image') |
            Where-Object {
                $_ -is [Xml.XmlElement] `
                -and $_.GetAttribute('id') -eq 'template-background'
            }
    )
    if ($backgroundElements.Count -ne 1) {
        throw "The $Kind SVG map must contain one 'template-background' image."
    }
    $backgroundElement = [Xml.XmlElement]$backgroundElements[0]
    $backgroundX = Xml-Number $backgroundElement 'x' "$Kind background"
    $backgroundY = Xml-Number $backgroundElement 'y' "$Kind background"
    $backgroundWidth =
        Xml-Number $backgroundElement 'width' "$Kind background"
    $backgroundHeight =
        Xml-Number $backgroundElement 'height' "$Kind background"
    if (
        $backgroundElement.GetAttribute('href') -ne 'background-clean.png' -or
        $backgroundX -ne 0.0 -or
        $backgroundY -ne 0.0 -or
        $backgroundWidth -ne $logicalWidth -or
        $backgroundHeight -ne $logicalHeight
    ) {
        throw "The $Kind SVG background image must cover the complete page."
    }

    $fields = [ordered]@{}
    $metrics = [ordered]@{}
    foreach ($metric in $metricKeys) {
        $metrics[$metric] = [ordered]@{}
    }
    $overallBounds = $null
    $signatureBounds = $null
    $seenIds = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::Ordinal
    )

    foreach ($node in $document.GetElementsByTagName('rect')) {
        if ($node -isnot [Xml.XmlElement]) {
            continue
        }
        $id = Xml-Attribute $node 'id' "$Kind map rectangle"
        if (-not $seenIds.Add($id)) {
            throw "The $Kind SVG map contains duplicate id '$id'."
        }
        $role = Xml-Attribute $node 'data-role' "$Kind map rectangle '$id'"
        $rect = Map-Rectangle $node "$Kind map rectangle '$id'"
        switch ($role) {
        'field' {
            $fieldName = Xml-Attribute $node 'data-field' "$Kind field '$id'"
            if (-not $fieldIds.Contains($fieldName)) {
                throw "The $Kind SVG map contains unsupported field '$fieldName'."
            }
            $marginText = Xml-Attribute $node 'data-margins' "$Kind field '$id'"
            $margins = @(
                ($marginText -split '[,\s]+') |
                    Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
                    ForEach-Object {
                        [double]::Parse(
                            $_,
                            [Globalization.CultureInfo]::InvariantCulture
                        )
                    }
            )
            if ($margins.Count -ne 4) {
                throw "The $Kind field '$id' must contain four margins."
            }
            $fields[$fieldName] = [ordered]@{
                rect = $rect
                margins = Rounded-Array $margins
                fontRole = Xml-Attribute $node 'data-font-role' "$Kind field '$id'"
                fontSizePt = Xml-Number $node 'data-font-size-pt' "$Kind field '$id'"
                horizontalAlignment = Xml-Attribute $node 'data-align-h' "$Kind field '$id'"
                verticalAlignment = Xml-Attribute $node 'data-align-v' "$Kind field '$id'"
                fit = Xml-Attribute $node 'data-fit' "$Kind field '$id'"
                minimumScale = Xml-Optional-Number $node 'data-minimum-scale' 0.7 "$Kind field '$id'"
                letterSpacing = Xml-Optional-Number $node 'data-letter-spacing' 0.0 "$Kind field '$id'"
                wordSpacing = Xml-Optional-Number $node 'data-word-spacing' 0.0 "$Kind field '$id'"
                lineHeight = Xml-Optional-Number $node 'data-line-height' 1.0 "$Kind field '$id'"
                baselineOffset = Xml-Optional-Number $node 'data-baseline-offset' 0.0 "$Kind field '$id'"
            }
            break
        }
        'score-cell' {
            $metric = Xml-Attribute $node 'data-metric' "$Kind score cell '$id'"
            $grade = Xml-Attribute $node 'data-grade' "$Kind score cell '$id'"
            if (-not $metrics.Contains($metric) -or -not $grades.Contains($grade)) {
                throw "The $Kind SVG map contains unsupported score cell '$id'."
            }
            if ($metrics[$metric].Contains($grade)) {
                throw "The $Kind SVG map repeats the $metric $grade score cell."
            }
            $metrics[$metric][$grade] = $rect
            break
        }
        'overall-grade' {
            if ($null -ne $overallBounds) {
                throw "The $Kind SVG map repeats the overall-grade rectangle."
            }
            $overallBounds = $rect
            break
        }
        'signature' {
            if ($null -ne $signatureBounds) {
                throw "The $Kind SVG map repeats the signature rectangle."
            }
            $signatureBounds = $rect
            break
        }
        default {
            throw "The $Kind SVG map contains unsupported role '$role'."
        }
        }
    }

    foreach ($fieldName in $fieldIds.Keys) {
        if (-not $fields.Contains($fieldName)) {
            throw "The $Kind SVG map is missing field '$fieldName'."
        }
    }
    foreach ($metric in $metricKeys) {
        foreach ($grade in $grades.Keys) {
            if (-not $metrics[$metric].Contains($grade)) {
                throw "The $Kind SVG map is missing the $metric $grade score cell."
            }
        }
    }
    if ($null -eq $overallBounds -or $null -eq $signatureBounds) {
        throw "The $Kind SVG map is missing the overall-grade or signature rectangle."
    }

    return [ordered]@{
        fields = $fields
        metrics = $metrics
        overallGradeBounds = $overallBounds
        signatureBounds = $signatureBounds
    }
}

function Spec-Property {
    param(
        [Parameter(Mandatory)]$Object,
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Context
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "$Context is missing '$Name'."
    }
    return $property.Value
}

function Sprite-Manifest {
    param(
        [Parameter(Mandatory)][Drawing.Rectangle]$Source,
        [Parameter(Mandatory)][double[]]$PointSize
    )

    return [ordered]@{
        source = @($Source.X, $Source.Y, $Source.Width, $Source.Height)
        width = $Source.Width
        height = $Source.Height
        pointSize = Rounded-Array $PointSize
    }
}

function Compile-Template {
    param(
        [Parameter(Mandatory)][string]$Kind,
        [Parameter(Mandatory)]$Spec,
        [Parameter(Mandatory)][string]$OutputDirectory
    )

    $templateSpec = Spec-Property $Spec.templates $Kind 'asset specification'
    $mapPath = Join-Path $sourceRoot ([string]$templateSpec.map)
    $backgroundPath = Join-Path $sourceRoot ([string]$templateSpec.background)
    $map = Read-Authoring-Map -Kind $Kind -Path $mapPath
    $background = Load-Bitmap -Path $backgroundPath
    try {
        if ($background.Width -le 0 -or $background.Height -le 0) {
            throw "The $Kind background has invalid dimensions."
        }

        $items = [Collections.Generic.List[object]]::new()
        foreach ($entry in $grades.GetEnumerator()) {
            $labelSpec = Spec-Property `
                $templateSpec.scoreLabels `
                $entry.Key `
                "$Kind score labels"
            $path = Join-Path $sourceRoot ([string]$labelSpec.file)
            $bitmap = Load-Bitmap -Path $path
            if (
                -not (Image-Has-Transparency $bitmap) -or
                -not (Image-Has-Visible-Pixel $bitmap)
            ) {
                $bitmap.Dispose()
                throw "The $Kind $($entry.Key) score label must contain visible pixels and transparency."
            }
            $items.Add([ordered]@{
                category = 'score'
                grade = $entry.Key
                bitmap = $bitmap
                pointSize = Number-Array `
                    $labelSpec.pointSize `
                    2 `
                    "$Kind $($entry.Key) score-label point size"
            })
        }
        foreach ($entry in $overallGrades.GetEnumerator()) {
            $labelSpec = Spec-Property `
                $templateSpec.overallGrades `
                $entry.Key `
                "$Kind overall grades"
            $path = Join-Path $sourceRoot ([string]$labelSpec.file)
            $bitmap = Load-Bitmap -Path $path
            if (
                -not (Image-Has-Transparency $bitmap) -or
                -not (Image-Has-Visible-Pixel $bitmap)
            ) {
                $bitmap.Dispose()
                throw "The $Kind $($entry.Key) overall-grade label must contain visible pixels and transparency."
            }
            $items.Add([ordered]@{
                category = 'overall'
                grade = $entry.Key
                bitmap = $bitmap
                pointSize = Number-Array `
                    $labelSpec.pointSize `
                    2 `
                    "$Kind $($entry.Key) overall-grade point size"
            })
        }

        try {
            $spriteWidth = (
                $items |
                    ForEach-Object { $_.bitmap.Width } |
                    Measure-Object -Maximum
            ).Maximum
            $spriteHeight = (
                $items |
                    ForEach-Object { $_.bitmap.Height } |
                    Measure-Object -Sum
            ).Sum
            $spriteSheet = [Drawing.Bitmap]::new(
                $spriteWidth,
                $spriteHeight,
                [Drawing.Imaging.PixelFormat]::Format32bppArgb
            )
            $graphics = [Drawing.Graphics]::FromImage($spriteSheet)
            try {
                $graphics.Clear([Drawing.Color]::Transparent)
                $graphics.CompositingMode =
                    [Drawing.Drawing2D.CompositingMode]::SourceCopy
                $scoreLabels = [ordered]@{}
                $overallLabels = [ordered]@{}
                $top = 0
                foreach ($item in $items) {
                    $source = [Drawing.Rectangle]::new(
                        0,
                        $top,
                        $item.bitmap.Width,
                        $item.bitmap.Height
                    )
                    $graphics.DrawImageUnscaled($item.bitmap, 0, $top)
                    $sprite = Sprite-Manifest `
                        -Source $source `
                        -PointSize $item.pointSize
                    if ($item.category -eq 'score') {
                        $scoreLabels[$item.grade] = $sprite
                    }
                    else {
                        $bounds = $map.overallGradeBounds
                        $destinationX =
                            [double]$bounds[0] `
                            + (
                                (
                                    [double]$bounds[2] `
                                    - [double]$item.pointSize[0]
                                ) / 2.0
                            )
                        $destinationY =
                            [double]$bounds[1] `
                            + (
                                (
                                    [double]$bounds[3] `
                                    - [double]$item.pointSize[1]
                                ) / 2.0
                            )
                        $sprite.destination = Rounded-Array -Values @(
                            $destinationX,
                            $destinationY,
                            ([double]$item.pointSize[0]),
                            ([double]$item.pointSize[1])
                        )
                        $overallLabels[$item.grade] = $sprite
                    }
                    $top += $item.bitmap.Height
                }
            }
            finally {
                $graphics.Dispose()
            }

            $metrics = @(
                foreach ($metric in $metricKeys) {
                    [ordered]@{
                        name = $metric
                        cells = $map.metrics[$metric]
                    }
                }
            )
            $manifest = [ordered]@{
                version = 3
                template = $Kind
                units = 'pt'
                placeholderArtwork = [bool]$Spec.placeholderArtwork
                logicalSize = @($logicalWidth, $logicalHeight)
                physicalSizeInches = @(7.5, 10.833333)
                outputDpi = $recommendedDpi
                backgroundPixelSize = @($background.Width, $background.Height)
                background = "$Kind-background.png"
                sprites = "$Kind-sprites.png"
                fields = $map.fields
                scoreTable = [ordered]@{
                    fillColor = [string]$Spec.highlightColor
                    fillInset = [double]$Spec.highlightInsetPoints
                    metrics = $metrics
                    labels = $scoreLabels
                }
                overallGradeBounds = $map.overallGradeBounds
                overallGrades = $overallLabels
                signatureBounds = $map.signatureBounds
            }

            New-Item -ItemType Directory -Path $OutputDirectory -Force |
                Out-Null
            Copy-Item `
                -LiteralPath $backgroundPath `
                -Destination (
                    Join-Path $OutputDirectory "$Kind-background.png"
                ) `
                -Force
            Save-Png `
                -Bitmap $spriteSheet `
                -Path (Join-Path $OutputDirectory "$Kind-sprites.png")
            Write-Json `
                -Value $manifest `
                -Path (Join-Path $OutputDirectory "$Kind-manifest.json")
        }
        finally {
            if ($null -ne $spriteSheet) {
                $spriteSheet.Dispose()
            }
        }
    }
    finally {
        if ($null -ne $items) {
            foreach ($item in $items) {
                $item.bitmap.Dispose()
            }
        }
        $background.Dispose()
    }
}

function Compare-GeneratedFiles {
    param(
        [Parameter(Mandatory)][string]$GeneratedDirectory,
        [Parameter(Mandatory)][string]$TrackedDirectory
    )

    $names = @(
        'standard-background.png',
        'standard-manifest.json',
        'standard-sprites.png',
        'advanced-background.png',
        'advanced-manifest.json',
        'advanced-sprites.png'
    )
    foreach ($name in $names) {
        $generatedPath = Join-Path $GeneratedDirectory $name
        $trackedPath = Join-Path $TrackedDirectory $name
        if (
            -not (Test-Path -LiteralPath $generatedPath) -or
            -not (Test-Path -LiteralPath $trackedPath)
        ) {
            return $false
        }
        $generatedHash =
            (Get-FileHash -LiteralPath $generatedPath -Algorithm SHA256).Hash
        $trackedHash =
            (Get-FileHash -LiteralPath $trackedPath -Algorithm SHA256).Hash
        if ($generatedHash -ne $trackedHash) {
            return $false
        }
    }
    return $true
}

if ($BootstrapPlaceholders.IsPresent) {
    Bootstrap-Placeholder-Sources
}
if (-not (Test-Path -LiteralPath $assetSpecPath)) {
    throw (
        "The SVG/PNG authoring sources are missing. Run " +
        "generate_internal_template_assets.ps1 --bootstrap-placeholders once."
    )
}

New-Item -ItemType Directory -Path $workRoot -Force | Out-Null
try {
    $spec = Read-Json -Path $assetSpecPath
    $pageSize = Number-Array `
        $spec.pageSizePoints `
        2 `
        'asset specification page size'
    if (
        $spec.version -ne 1 -or
        $spec.units -ne 'pt' -or
        [Math]::Abs($pageSize[0] - $logicalWidth) -gt 0.001 -or
        [Math]::Abs($pageSize[1] - $logicalHeight) -gt 0.001 -or
        $spec.outputDpi -ne $recommendedDpi
    ) {
        throw 'The speaking-evaluation asset specification is unsupported.'
    }

    Compile-Template `
        -Kind 'standard' `
        -Spec $spec `
        -OutputDirectory $workRoot
    Compile-Template `
        -Kind 'advanced' `
        -Spec $spec `
        -OutputDirectory $workRoot

    if ($Check.IsPresent) {
        if (-not (Compare-GeneratedFiles $workRoot $resourceOutput)) {
            throw 'Speaking-evaluation SVG template assets are out of date.'
        }
        Write-Output 'Speaking-evaluation SVG template assets are current.'
    }
    else {
        foreach ($file in Get-ChildItem -LiteralPath $workRoot -File) {
            Copy-Item `
                -LiteralPath $file.FullName `
                -Destination $resourceOutput `
                -Force
        }
        Write-Output "Generated SVG-mapped template assets in '$resourceOutput'."
    }
}
finally {
    if (Test-Path -LiteralPath $workRoot) {
        $resolvedTempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        $resolvedWorkRoot = [IO.Path]::GetFullPath($workRoot)
        if (
            -not $resolvedWorkRoot.StartsWith(
                $resolvedTempRoot,
                [StringComparison]::OrdinalIgnoreCase
            )
        ) {
            throw "Refusing to remove work directory outside the temporary root: $resolvedWorkRoot"
        }
        Remove-Item -LiteralPath $workRoot -Recurse -Force
    }
}
