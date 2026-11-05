[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot
)

$ErrorActionPreference = "Stop"
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path

function Replace-UniqueSourceBlock {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Pattern,

        [Parameter(Mandatory = $true)]
        [string[]]$ReplacementLines
    )

    $content = [System.IO.File]::ReadAllText($Path)
    $matches = [regex]::Matches($content, $Pattern)
    if ($matches.Count -ne 1) {
        throw "Expected exactly one innoextract source block in $Path; found $($matches.Count)."
    }

    $lineEnding = if ($content.Contains("`r`n")) { "`r`n" } else { "`n" }
    $replacement = $ReplacementLines -join $lineEnding
    $updated = ([regex]::new($Pattern)).Replace($content, $replacement, 1)
    [System.IO.File]::WriteAllText(
        $Path,
        $updated,
        [System.Text.UTF8Encoding]::new($false)
    )
}

$fileEntryPath = Join-Path $SourceRoot "src\setup\file.cpp"
$fileEntryPattern = '(?m)^\t\tis\.read\(checksum\.sha256, std::streamsize\(sizeof\(checksum\.sha256\)\)\);\r?\n\t\tchecksum\.type = crypto::SHA256;\r?\n\t\tverification = stored_enum<stored_file_verification_type>\(is\)\.get\(\);'
Replace-UniqueSourceBlock `
    -Path $fileEntryPath `
    -Pattern $fileEntryPattern `
    -ReplacementLines @(
        "`t`tis.read(checksum.sha256, std::streamsize(sizeof(checksum.sha256)));",
        "`t`tverification = stored_enum<stored_file_verification_type>(is).get();",
        "`t`tchecksum.type = verification == FileVerificationNone ? crypto::None : crypto::SHA256;"
    )

$extractPath = Join-Path $SourceRoot "src\cli\extract.cpp"
$hasChecksumPattern = '(?m)^\tbool has_checksum\(\) const \{\r?\n\t\treturn checksum_position_ == file_->entry\(\)\.size;\r?\n\t\}'
Replace-UniqueSourceBlock `
    -Path $extractPath `
    -Pattern $hasChecksumPattern `
    -ReplacementLines @(
        "`tbool has_checksum() const {",
        "`t`tconst boost::uint64_t expected_size = file_->entry().size != 0",
        "`t`t                                      ? file_->entry().size : total_written_;",
        "`t`treturn checksum_position_ == expected_size;",
        "`t}"
    )

$readbackPattern = '(?m)^\t\twhile\(!stream_\.eof\(\)\) \{\r?\n\t\t\tchar buffer\[8192\];\r?\n\t\t\tstd::streamsize n = stream_\.read\(buffer, sizeof\(buffer\)\)\.gcount\(\);\r?\n\t\t\tchecksum_\.update\(buffer, size_t\(n\)\);\r?\n\t\t\tchecksum_position_ \+= boost::uint64_t\(n\);\r?\n\t\t\}'
Replace-UniqueSourceBlock `
    -Path $extractPath `
    -Pattern $readbackPattern `
    -ReplacementLines @(
        "`t`twhile(!stream_.eof()) {",
        "`t`t`tchar buffer[8192];",
        "`t`t`tstd::streamsize n = stream_.read(buffer, sizeof(buffer)).gcount();",
        "`t`t`tchecksum_.update(buffer, size_t(n));",
        "`t`t`tchecksum_position_ += boost::uint64_t(n);",
        "`t`t`tif(stream_.bad() || (stream_.fail() && !stream_.eof())) {",
        "`t`t`t`tlog_warning << `"Could not read back `" << path_ << `" to calculate output checksum`";",
        "`t`t`t`treturn false;",
        "`t`t`t}",
        "`t`t}"
    )
