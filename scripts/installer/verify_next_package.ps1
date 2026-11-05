[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("x64", "arm64")]
    [string]$Architecture,

    [Parameter(Mandatory = $true)]
    [string]$ProjectVersion,

    [Parameter(Mandatory = $true)]
    [string]$QtVersion,

    [Parameter(Mandatory = $true)]
    [string]$InnoExtractPath
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$releaseRoot = Join-Path $repoRoot "dist"
$buildRoot = Join-Path $repoRoot "build\windows-$Architecture-release"
$reportPath = Join-Path $buildRoot "reports\windows-$Architecture-final-package.json"
$redistributableName = "vc_redist.$Architecture.exe"
$stagedRedistributable = Join-Path $buildRoot "installer-prerequisites\$redistributableName"
$installerPattern = "ClassMngr-*-win-$Architecture.exe"
$installerMatches = @(Get-ChildItem -LiteralPath $releaseRoot -Filter $installerPattern -File)
if ($installerMatches.Count -ne 1) {
    throw "Expected exactly one final Windows $Architecture installer; found $($installerMatches.Count)."
}
$installer = $installerMatches[0]

$checksumPath = Join-Path $releaseRoot "checksums-windows-$Architecture.txt"
$checksumLine = (Get-Content -LiteralPath $checksumPath -Raw).Trim()
if ($checksumLine -notmatch '^([0-9a-fA-F]{64})\s{2}(.+)$') {
    throw "The Windows $Architecture checksum sidecar is malformed."
}
$expectedHash = $Matches[1].ToLowerInvariant()
$expectedName = $Matches[2].Trim()
if ($expectedName -ne $installer.Name) {
    throw "Checksum sidecar names '$expectedName', expected '$($installer.Name)'."
}
$actualHash = (Get-FileHash -LiteralPath $installer.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualHash -ne $expectedHash) {
    throw "Final installer SHA-256 $actualHash does not match sidecar $expectedHash."
}

$expectedInnoExtractCommit = "376a13e7c41cc5528b6088d0dd16ec1b323a8d37"
$expectedInnoExtractVersion = "1.10-dev + $($expectedInnoExtractCommit.Substring(0, 7))"
if (-not (Test-Path -LiteralPath $InnoExtractPath -PathType Leaf)) {
    throw "The pinned innoextract executable was not found at $InnoExtractPath."
}
$InnoExtractPath = (Resolve-Path -LiteralPath $InnoExtractPath).Path
$innoExtractVersionOutput = (& $InnoExtractPath --version --silent 2>&1 | Out-String).Trim()
$innoExtractVersionExitCode = $LASTEXITCODE
if ($innoExtractVersionExitCode -ne 0) {
    throw "Pinned innoextract returned exit code $innoExtractVersionExitCode while reporting its version: $innoExtractVersionOutput"
}
if ($innoExtractVersionOutput -ne $expectedInnoExtractVersion) {
    throw "Expected innoextract built from $expectedInnoExtractCommit ($expectedInnoExtractVersion); found '$innoExtractVersionOutput'."
}

$verifierPath = Join-Path $repoRoot "scripts\verify_next_package.py"
function Invoke-PackageVerifier {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackageRoot,
        [string]$Report,
        [switch]$AllowInnoUninstaller
    )

    $arguments = @(
        $verifierPath,
        "verify",
        "--package-root", $PackageRoot,
        "--platform", "windows",
        "--architecture", $Architecture,
        "--version", $ProjectVersion,
        "--qt-version", $QtVersion
    )
    if ($AllowInnoUninstaller) {
        $arguments += "--allow-inno-uninstaller"
    }
    if ($Report) {
        $arguments += @("--report", $Report)
    }
    & python @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Final Windows package verification failed for $PackageRoot."
    }
}

$runSuffix = "$env:GITHUB_RUN_ID-$env:GITHUB_RUN_ATTEMPT"
$extractRoot = Join-Path $env:RUNNER_TEMP "ClassMngr-$Architecture-inno-payload-$runSuffix"
$extractLog = Join-Path $env:RUNNER_TEMP "ClassMngr-$Architecture-innoextract-$runSuffix.log"
if (Test-Path -LiteralPath $extractRoot) {
    throw "The run-specific Inno payload directory already exists: $extractRoot."
}
New-Item -ItemType Directory -Path $extractRoot | Out-Null
$installRoot = $null
$smokeRoot = $null
$installerProcess = $null
$startupProcess = $null
$oldSettingsRoot = $null
$oldQpaPlatform = $null
$oldForceStderr = $null
$environmentChanged = $false
$startupProcessStarted = $false

try {
    Write-Host "Streaming innoextract file progress; full output log: $extractLog"
    # innoextract supports --list with --test/--extract; --list-sizes emits each
    # processed filename and byte count even with --silent. Explicit progress
    # keeps the log active during lengthy processing while silent suppresses
    # unrelated output.
    & $InnoExtractPath --no-extract-unknown --list --list-sizes --test --extract --silent --progress=1 `
        --output-dir $extractRoot -- $installer.FullName 2>&1 |
        Tee-Object -FilePath $extractLog
    $extractExitCode = $LASTEXITCODE
    if ($extractExitCode -ne 0) {
        $details = (Get-Content -LiteralPath $extractLog -Tail 80 -ErrorAction SilentlyContinue) -join "`n"
        throw "Pinned innoextract failed to extract the Inno Setup payload (exit code $extractExitCode). Full output log: $extractLog`nLast output lines:`n$details"
    }

    if (-not (Test-Path -LiteralPath $stagedRedistributable -PathType Leaf)) {
        throw "The staged $redistributableName was not found at $stagedRedistributable."
    }
    $embeddedRedistributables = @(
        Get-ChildItem -Path $extractRoot -Recurse -File -Filter "vc_redist.*.exe"
    )
    $matchingRedistributables = @(
        $embeddedRedistributables | Where-Object { $_.Name -ieq $redistributableName }
    )
    if ($matchingRedistributables.Count -ne 1) {
        throw "Expected exactly one embedded $redistributableName; found $($matchingRedistributables.Count)."
    }
    $otherRedistributables = @(
        $embeddedRedistributables | Where-Object { $_.Name -ine $redistributableName }
    )
    if ($otherRedistributables.Count -ne 0) {
        $otherNames = ($otherRedistributables | ForEach-Object { $_.FullName }) -join ", "
        throw "The $Architecture Inno payload contains unexpected redistributables: $otherNames."
    }
    $stagedRedistributableHash = (Get-FileHash -LiteralPath $stagedRedistributable -Algorithm SHA256).Hash
    $embeddedRedistributableHash = (Get-FileHash -LiteralPath $matchingRedistributables[0].FullName -Algorithm SHA256).Hash
    if ($embeddedRedistributableHash -ne $stagedRedistributableHash) {
        throw "The embedded $redistributableName does not match the validated staged file."
    }
    Write-Host "Verified exactly one embedded $redistributableName matching the staged VS 2022 redist."

    $payloadExecutables = @(
        Get-ChildItem -Path $extractRoot -Recurse -File -Filter "ClassMngr.exe" |
            Where-Object {
                Test-Path -LiteralPath (Join-Path $_.DirectoryName "package-manifest.json")
            }
    )
    if ($payloadExecutables.Count -ne 1) {
        throw "Expected one extracted ClassMngr.exe beside its package manifest; found $($payloadExecutables.Count)."
    }
    $payloadRoot = $payloadExecutables[0].DirectoryName
    Invoke-PackageVerifier -PackageRoot $payloadRoot

    if ($Architecture -eq "x64") {
        $installRoot = Join-Path $env:RUNNER_TEMP "ClassMngr-x64-installed-$runSuffix"
        $installArguments = @(
            "/VERYSILENT",
            "/SUPPRESSMSGBOXES",
            "/NORESTART",
            "/SP-",
            "/DIR=`"$installRoot`""
        )
        $installerProcess = Start-Process -FilePath $installer.FullName `
            -ArgumentList $installArguments -PassThru
        if (-not $installerProcess.WaitForExit(120000)) {
            $installerProcess.Kill()
            throw "Silent Inno installation did not finish within 120 seconds."
        }
        if ($installerProcess.ExitCode -ne 0) {
            throw "Silent Inno installation failed with exit code $($installerProcess.ExitCode)."
        }

        $installedExecutable = Join-Path $installRoot "ClassMngr.exe"
        if (-not (Test-Path -LiteralPath $installedExecutable -PathType Leaf)) {
            throw "Silent installation did not create $installedExecutable."
        }
        Invoke-PackageVerifier `
            -PackageRoot $installRoot `
            -Report $reportPath `
            -AllowInnoUninstaller

        $smokeRoot = Join-Path $env:RUNNER_TEMP "ClassMngr-Next-startup-smoke-$runSuffix"
        New-Item -ItemType Directory -Path $smokeRoot | Out-Null
        $settingsRoot = Join-Path $smokeRoot "settings"
        New-Item -ItemType Directory -Path $settingsRoot | Out-Null
        $oldSettingsRoot = [Environment]::GetEnvironmentVariable(
            "CLASSMNGR_SETTINGS_ROOT", [EnvironmentVariableTarget]::Process
        )
        $oldQpaPlatform = [Environment]::GetEnvironmentVariable(
            "QT_QPA_PLATFORM", [EnvironmentVariableTarget]::Process
        )
        $oldForceStderr = [Environment]::GetEnvironmentVariable(
            "QT_FORCE_STDERR_LOGGING", [EnvironmentVariableTarget]::Process
        )
        $environmentChanged = $true
        $env:CLASSMNGR_SETTINGS_ROOT = $settingsRoot
        $env:QT_QPA_PLATFORM = "offscreen"
        $env:QT_FORCE_STDERR_LOGGING = "1"

        $startupInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startupInfo.FileName = $installedExecutable
        $startupInfo.WorkingDirectory = $installRoot
        $startupInfo.UseShellExecute = $false
        $startupInfo.CreateNoWindow = $true
        $startupInfo.RedirectStandardOutput = $true
        $startupInfo.RedirectStandardError = $true
        $startupProcess = [System.Diagnostics.Process]::new()
        $startupProcess.StartInfo = $startupInfo
        if (-not $startupProcess.Start()) {
            throw "Unable to start the installed Next app for startup verification."
        }
        $startupProcessStarted = $true
        $startupReaders = @(
            [pscustomobject]@{
                Reader = $startupProcess.StandardOutput
                ReadTask = $startupProcess.StandardOutput.ReadLineAsync()
            },
            [pscustomobject]@{
                Reader = $startupProcess.StandardError
                ReadTask = $startupProcess.StandardError.ReadLineAsync()
            }
        )
        $deadline = [DateTime]::UtcNow.AddSeconds(120)
        $startupReady = $false
        $combinedLog = ""
        $startupLogLines = [System.Collections.Generic.List[string]]::new()
        while ([DateTime]::UtcNow -lt $deadline) {
            $startupProcess.Refresh()
            foreach ($reader in $startupReaders) {
                while ($null -ne $reader.ReadTask -and $reader.ReadTask.IsCompleted) {
                    $outputLine = $reader.ReadTask.GetAwaiter().GetResult()
                    if ($null -eq $outputLine) {
                        $reader.ReadTask = $null
                        break
                    }
                    $startupLogLines.Add([string]$outputLine)
                    $reader.ReadTask = $reader.Reader.ReadLineAsync()
                }
            }
            $combinedLog = $startupLogLines -join "`n"
            if ($combinedLog -match 'ClassMngrNext startup stage=startup-ready outcome=completed') {
                $startupReady = $true
                Start-Sleep -Milliseconds 250
                foreach ($reader in $startupReaders) {
                    while ($null -ne $reader.ReadTask -and $reader.ReadTask.IsCompleted) {
                        $outputLine = $reader.ReadTask.GetAwaiter().GetResult()
                        if ($null -eq $outputLine) {
                            $reader.ReadTask = $null
                            break
                        }
                        $startupLogLines.Add([string]$outputLine)
                        $reader.ReadTask = $reader.Reader.ReadLineAsync()
                    }
                }
                $combinedLog = $startupLogLines -join "`n"
                break
            }
            if ($startupProcess.HasExited) {
                $startupProcess.WaitForExit()
                $readerDrainDeadline = [DateTime]::UtcNow.AddSeconds(10)
                while ([DateTime]::UtcNow -lt $readerDrainDeadline -and
                    @($startupReaders | Where-Object { $null -ne $_.ReadTask -and -not $_.ReadTask.IsCompleted }).Count -gt 0) {
                    Start-Sleep -Milliseconds 10
                }
                foreach ($reader in $startupReaders) {
                    while ($null -ne $reader.ReadTask -and $reader.ReadTask.IsCompleted) {
                        $outputLine = $reader.ReadTask.GetAwaiter().GetResult()
                        if ($null -eq $outputLine) {
                            $reader.ReadTask = $null
                            break
                        }
                        $startupLogLines.Add([string]$outputLine)
                        $reader.ReadTask = $reader.Reader.ReadLineAsync()
                    }
                }
                $combinedLog = $startupLogLines -join "`n"
                break
            }
            Start-Sleep -Milliseconds 250
        }

        if (-not $startupReady) {
            $startupProcess.Refresh()
            if ($startupProcess.HasExited) {
                throw "Installed Next app exited with code $($startupProcess.ExitCode) before startup-ready.`n$combinedLog"
            }
            throw "Installed Next app did not reach startup-ready within 120 seconds.`n$combinedLog"
        }

        $stageMatches = [regex]::Matches(
            $combinedLog,
            'ClassMngrNext startup stage=(?<stage>[a-z-]+) outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+)'
        )
        $expectedStages = @(
            "process-created",
            "core-services-created",
            "settings-loaded",
            "language-loaded",
            "theme-loaded",
            "required-fonts-loaded",
            "workspace-opened",
            "shell-created",
            "initial-page-ready",
            "startup-ready"
        )
        if ($stageMatches.Count -ne $expectedStages.Count) {
            throw "Expected $($expectedStages.Count) ordered Next startup stage records, found $($stageMatches.Count).`n$combinedLog"
        }
        for ($index = 0; $index -lt $expectedStages.Count; $index++) {
            $record = $stageMatches[$index]
            $stageName = $record.Groups["stage"].Value
            if ($stageName -ne $expectedStages[$index]) {
                throw "Next startup stage order mismatch at ${index}: expected '$($expectedStages[$index])', found '$stageName'."
            }
            $expectedOutcome = if ($stageName -eq "workspace-opened") { "warning" } else { "completed" }
            if ($record.Groups["outcome"].Value -ne $expectedOutcome) {
                throw "Next startup stage '$stageName' had outcome '$($record.Groups['outcome'].Value)', expected '$expectedOutcome'."
            }
            $expectedDiagnostic = if ($stageName -eq "workspace-opened") { "stage-warning" } else { "none" }
            $expectedDiagnosticStage = if ($stageName -eq "workspace-opened") { "workspace-opened" } else { "none" }
            if ($record.Groups["diagnostic"].Value -ne $expectedDiagnostic `
                -or $record.Groups["diagnosticStage"].Value -ne $expectedDiagnosticStage) {
                throw "Next startup stage '$stageName' reported an unexpected diagnostic."
            }
        }
        Write-Host "Verified installed Next startup stages in order: $($expectedStages -join ', ')."
    }
    else {
        Invoke-PackageVerifier -PackageRoot $payloadRoot -Report $reportPath
    }
}
finally {
    if ($startupProcessStarted) {
        $startupProcess.Refresh()
        if (-not $startupProcess.HasExited) {
            Stop-Process -Id $startupProcess.Id -Force -ErrorAction SilentlyContinue
            [void]$startupProcess.WaitForExit(10000)
        }
        $startupProcess.Dispose()
    }
    if ($environmentChanged) {
        [Environment]::SetEnvironmentVariable(
            "CLASSMNGR_SETTINGS_ROOT", $oldSettingsRoot, [EnvironmentVariableTarget]::Process
        )
        [Environment]::SetEnvironmentVariable(
            "QT_QPA_PLATFORM", $oldQpaPlatform, [EnvironmentVariableTarget]::Process
        )
        [Environment]::SetEnvironmentVariable(
            "QT_FORCE_STDERR_LOGGING", $oldForceStderr, [EnvironmentVariableTarget]::Process
        )
    }
    if ($installRoot -and (Test-Path -LiteralPath (Join-Path $installRoot "unins000.exe"))) {
        try {
            $uninstaller = Start-Process `
                -FilePath (Join-Path $installRoot "unins000.exe") `
                -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART") `
                -PassThru
            if ($uninstaller.WaitForExit(30000)) {
                if ($uninstaller.ExitCode -ne 0) {
                    Write-Warning "Temporary Inno uninstall returned $($uninstaller.ExitCode)."
                }
            }
            else {
                $uninstaller.Kill()
                Write-Warning "Temporary Inno uninstall exceeded 30 seconds."
            }
        }
        catch {
            Write-Warning "Unable to clean up the temporary Inno installation: $_"
        }
    }
    Remove-Item -LiteralPath $extractRoot -Recurse -Force -ErrorAction SilentlyContinue
    if ($smokeRoot) {
        Remove-Item -LiteralPath $smokeRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    if ($installRoot) {
        Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Output "WINDOWS_PACKAGE_VERIFICATION_SUCCESS:$Architecture"
