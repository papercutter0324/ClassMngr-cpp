[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $TranslationDirectory,

    [Parameter(Mandatory = $true)]
    [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-ResourceKey {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Context,

        [Parameter(Mandatory = $true)]
        [string] $Source
    )

    $null = $hashInput = $Context + "`n" + $Source
    $null = $bytes = [System.Text.Encoding]::UTF8.GetBytes($hashInput)
    $null = $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $null = $digest = $sha256.ComputeHash($bytes)
    }
    finally {
        $sha256.Dispose()
    }

    $null = $hash = ([BitConverter]::ToString($digest)).Replace('-', '').ToLowerInvariant()
    $null = $contextKey = [regex]::Replace($Context, '[^A-Za-z0-9_]', '_')
    if ([string]::IsNullOrEmpty($contextKey)) {
        $null = $contextKey = 'Context'
    }
    if ($contextKey[0] -match '[0-9]') {
        $null = $contextKey = '_' + $contextKey
    }

    return "ClassMngr.Strings.$contextKey.$hash"
}

function Get-LocaleTag {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.FileInfo] $File
    )

    $null = $match = [regex]::Match(
        $File.BaseName,
        '^ClassMngr_(?<language>[A-Za-z]{2,3})_(?<region>[A-Za-z]{2,3})$'
    )
    if (-not $match.Success) {
        throw "Translation file has an unsupported name: $($File.Name)"
    }

    return "{0}-{1}" -f `
        $match.Groups['language'].Value.ToLowerInvariant(), `
        $match.Groups['region'].Value.ToUpperInvariant()
}

function Get-MessageValue {
    param(
        [Parameter(Mandatory = $true)]
        [System.Xml.XmlElement] $Message
    )

    $null = $sourceNode = $Message.SelectSingleNode('source')
    if ($null -eq $sourceNode) {
        throw 'Translation message is missing its source text.'
    }
    $null = $source = [string] $sourceNode.InnerText
    $null = $translation = $Message.SelectSingleNode('translation')
    if ($null -eq $translation) {
        return $null
    }

    if ($translation.GetAttribute('type') -in @(
            'unfinished'
            'vanished'
            'obsolete'
        )) {
        return $null
    }

    $null = $pluralForms = @($translation.SelectNodes('numerusform'))
    if ($pluralForms.Count -gt 0) {
        throw "Plural translation is not supported by the WinUI string-resource bridge: '$source'"
    }

    $null = $translated = [string] $translation.InnerText
    if ([string]::IsNullOrWhiteSpace($translated)) {
        return $null
    }

    return $translated
}

function Read-TranslationCatalog {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.FileInfo] $TranslationFile
    )

    $null = $translationDocument = [xml] (Get-Content `
        -LiteralPath $TranslationFile.FullName `
        -Encoding UTF8 `
        -Raw)

    $null = $entries = [ordered]@{}
    foreach ($context in @($translationDocument.SelectNodes('/TS/context'))) {
        $null = $nameNode = $context.SelectSingleNode('name')
        if ($null -eq $nameNode) {
            throw "Translation context is missing its name in $($TranslationFile.Name)."
        }
        $null = $contextName = [string] $nameNode.InnerText
        foreach ($message in @($context.SelectNodes('message'))) {
            $null = $sourceNode = $message.SelectSingleNode('source')
            if ($null -eq $sourceNode) {
                throw "Translation message is missing its source in $($TranslationFile.Name)."
            }
            $null = $source = [string] $sourceNode.InnerText
            $null = $key = Get-ResourceKey -Context $contextName -Source $source

            $translation = $message.SelectSingleNode('translation')
            if ($null -ne $translation -and $translation.GetAttribute('type') -in @(
                    'vanished'
                    'obsolete'
                )) {
                continue
            }

            $null = $value = Get-MessageValue -Message $message

            if ($entries.Contains($key)) {
                $existing = $entries[$key]
                if ($existing.Source -ne $source) {
                    throw "Conflicting translation sources generated the same resource key: $key"
                }
                if ($null -eq $existing.Value -and $null -ne $value) {
                    $existing.Value = $value
                }
                elseif ($null -ne $existing.Value -and $null -ne $value `
                    -and $existing.Value -ne $value) {
                    throw "Conflicting translations generated the same resource key: $key"
                }
                continue
            }

            $entries[$key] = [pscustomobject]@{
                Key = $key
                Context = $contextName
                Source = $source
                Value = $value
            }
        }
    }

    return [pscustomobject]@{
        File = $TranslationFile
        Entries = $entries
    }
}

function New-ResourceDocument {
    param(
        [Parameter(Mandatory = $true)]
        [System.Collections.IDictionary] $TranslationEntries,

        [Parameter(Mandatory = $true)]
        [System.Collections.IDictionary] $FallbackEntries
    )

    $null = $resourceDocument = New-Object System.Xml.XmlDocument
    $null = $resourceDocument.AppendChild(
        $resourceDocument.CreateXmlDeclaration('1.0', 'utf-8', $null)
    )
    $null = $root = $resourceDocument.CreateElement('root')
    $null = $resourceDocument.AppendChild($root)

    $null = $entries = New-Object 'System.Collections.Generic.Dictionary[string,string]'
    foreach ($fallbackEntry in $FallbackEntries.Values) {
        $null = $key = $fallbackEntry.Key
        $null = $value = $fallbackEntry.Value

        if ($TranslationEntries.Contains($key)) {
            $localizedValue = $TranslationEntries[$key].Value
            if ($null -ne $localizedValue) {
                $value = $localizedValue
            }
        }

        if ($null -eq $value) {
            throw "No fallback text exists for resource key: $key"
        }

        if ($entries.ContainsKey($key)) {
            if ($entries[$key] -ne $value) {
                throw "Conflicting translations generated the same resource key: $key"
            }
            continue
        }
        $null = $entries.Add($key, $value)

        $null = $data = $resourceDocument.CreateElement('data')
        $null = $data.SetAttribute('name', $key)
        $null = $data.SetAttribute(
            'space',
            'http://www.w3.org/XML/1998/namespace',
            'preserve'
        )
        $null = $valueElement = $resourceDocument.CreateElement('value')
        $null = $valueElement.InnerText = $value
        $null = $data.AppendChild($valueElement)
        $null = $root.AppendChild($data)
    }

    return [pscustomobject]@{
        Document = $resourceDocument
        EntryCount = $entries.Count
    }
}

$null = $translationPath = [System.IO.Path]::GetFullPath($TranslationDirectory)
if (-not (Test-Path -LiteralPath $translationPath -PathType Container)) {
    throw "Translation directory was not found: $translationPath"
}

$null = $outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$null = New-Item -ItemType Directory -Path $outputPath -Force
$null = $translationFiles = @(Get-ChildItem `
    -LiteralPath $translationPath `
    -Filter 'ClassMngr_*.ts' `
    -File |
    Sort-Object -Property Name)
if ($translationFiles.Count -eq 0) {
    throw "No shared translation catalogs were found in: $translationPath"
}

$null = $defaultFiles = @($translationFiles | Where-Object {
    $_.BaseName -eq 'ClassMngr_en_US'
})
if ($defaultFiles.Count -ne 1) {
    throw "Exactly one ClassMngr_en_US.ts catalog is required in: $translationPath"
}

$defaultCatalog = Read-TranslationCatalog -TranslationFile $defaultFiles[0]
$null = $defaultEntries = [ordered]@{}
foreach ($entry in $defaultCatalog.Entries.Values) {
    $null = $fallbackValue = $entry.Value
    if ($null -eq $fallbackValue) {
        $fallbackValue = $entry.Source
    }
    $defaultEntries[$entry.Key] = [pscustomobject]@{
        Key = $entry.Key
        Context = $entry.Context
        Source = $entry.Source
        Value = $fallbackValue
    }
}
if ($defaultEntries.Count -eq 0) {
    throw "The ClassMngr_en_US.ts catalog contains no active messages."
}

$null = $totalEntries = 0
foreach ($translationFile in $translationFiles) {
    $null = $locale = Get-LocaleTag -File $translationFile
    $catalog = Read-TranslationCatalog -TranslationFile $translationFile
    $null = $generated = New-ResourceDocument `
        -TranslationEntries $catalog.Entries `
        -FallbackEntries $defaultEntries
    $null = $localeDirectory = Join-Path $outputPath (Join-Path 'Strings' $locale)
    $null = New-Item -ItemType Directory -Path $localeDirectory -Force
    $null = $outputFile = Join-Path $localeDirectory 'Resources.resw'

    $null = $settings = New-Object System.Xml.XmlWriterSettings
    $null = $settings.Indent = $true
    $null = $settings.Encoding = New-Object System.Text.UTF8Encoding($false)
    $null = $writer = [System.Xml.XmlWriter]::Create($outputFile, $settings)
    try {
        $generated.Document.Save($writer)
    }
    finally {
        $writer.Dispose()
    }

    $null = $totalEntries += [int] $generated.EntryCount
    Write-Output "Generated $locale resource catalog with $($generated.EntryCount) entries."
}

Write-Output "Generated $($translationFiles.Count) WinUI resource catalogs ($totalEntries total entries)."
