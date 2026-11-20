<#
.SYNOPSIS
Runs the P9-15 packaged Windows x64 Release Schedule locale/theme lifecycle.

.DESCRIPTION
Reuses a packaged Release only after checking current product-source,
package-tree, executable, resource, build-receipt, and CMake-cache fingerprints.
The route uses PID/HWND-bound UIA controls, isolated QSettings, and an exact
SQLite fixture row snapshot. Failed attempts retain their run artifacts.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = '',
    [string]$EvidenceRoot = 'artifacts/phase9/packaged-next-p9-15-schedule-locale-theme',
    [string]$PriorRunManifestPath = 'artifacts/phase9/packaged-next-p9-14-staff-directory/20261120T210244662Z-e03ad18018b742949cd997ccdc9ef8c1/manifest.json',
    [string]$BuildDirectory = 'build/windows-x64-release',
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(30, 180)][int]$ExitTimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$TaskId = 'P9_15_PACKAGED_NEXT_SCHEDULE_LOCALE_THEME'
$Schema = 'classmngr-phase9-p9-15-packaged-schedule-locale-theme-v1'
$StrictWorkingSetLimit = [Int64]262144000
$OperationPeakAdvisoryLimit = [Int64]536870912
$P915OwnedSources = @(
    'scripts/phase9/prepare_p9_15_schedule_workspace.py',
    'scripts/phase9/run_packaged_next_p9_15_schedule_locale_theme.ps1',
    'scripts/phase9/validate_packaged_next_p9_15_schedule_locale_theme.py',
    'scripts/phase9/test_validate_packaged_next_p9_15_schedule_locale_theme.py'
)
$P914Sources = @(
    'scripts/phase9/p9_14_packaged_lifecycle_common.ps1',
    'scripts/phase9/prepare_p9_14_staff_ai_workspaces.py',
    'scripts/phase9/run_packaged_next_p9_14_staff_directory_lifecycle.ps1',
    'scripts/phase9/run_packaged_next_p9_14_ai_batch_review_lifecycle.ps1',
    'scripts/phase9/validate_packaged_next_p9_14_lifecycle.py',
    'scripts/phase9/test_validate_packaged_next_p9_14_lifecycle.py'
)
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot '../..' }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$PriorRunManifestPath = if ([IO.Path]::IsPathRooted($PriorRunManifestPath)) { [IO.Path]::GetFullPath($PriorRunManifestPath) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PriorRunManifestPath)) }
$BuildDirectory = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory)) }
if ($env:OS -cne 'Windows_NT') { throw 'P9-15 packaged UIA lifecycle must run locally on Windows.' }
. (Join-Path $PSScriptRoot 'p9_14_packaged_lifecycle_common.ps1')

function Get-P915InventoryRows([object[]]$Rows) { Get-P914InventoryFingerprint $Rows }

function Get-P915CurrentSource([string]$Repository, [string[]]$IncludedRoots) {
    Get-P914SourceInventory $Repository $IncludedRoots
}

function Assert-P915OldFilesUnchanged([object[]]$OldRows, [object[]]$CurrentRows) {
    $old = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    $now = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $OldRows) { $old.Add([string]$row.path, $row) }
    foreach ($row in $CurrentRows) { $now.Add([string]$row.path, $row) }
    foreach ($path in $old.Keys) {
        if (-not $now.ContainsKey($path)) { throw "Previously captured source is missing: $path" }
        if ([Int64]$old[$path].sizeBytes -ne [Int64]$now[$path].sizeBytes -or [string]$old[$path].sha256 -cne [string]$now[$path].sha256) { throw "Previously captured source changed: $path" }
    }
    $added = @($now.Keys | Where-Object { -not $old.ContainsKey($_) } | ForEach-Object { ([string]$_).Replace('\', '/') } | Sort-Object)
    if (($added -join "`n") -cne (($P915OwnedSources | Sort-Object) -join "`n")) { throw "Unexpected source additions after accepted P9-14: $($added -join ', ')" }
}

function Initialize-P915ReusedPackage([string]$Repository, [string]$EvidenceDirectory, [string]$AcceptedManifestPath) {
    if (-not (Test-Path -LiteralPath $AcceptedManifestPath -PathType Leaf)) { throw "Accepted P9-14 manifest is missing: $AcceptedManifestPath" }
    $acceptedHash = Get-P914FileSha256 $AcceptedManifestPath
    $accepted = Read-P914Json $AcceptedManifestPath
    if ([string]$accepted.taskId -cne 'P9_14_STAFF_DIRECTORY_PACKAGED_LIFECYCLE' -or [string]$accepted.status -cne 'passed') { throw 'P9-15 package ancestry is not the passed P9-14 Staff route.' }
    $acceptedRoot = Split-Path -Parent $AcceptedManifestPath
    $acceptedProof = $accepted.packageProvenance
    $acceptedSourcePath = Join-Path $acceptedRoot ([string]$acceptedProof.currentSourceInventoryPath)
    if ((Get-P914FileSha256 $acceptedSourcePath) -cne [string]$acceptedProof.currentSourceInventorySha256) { throw 'Accepted P9-14 source inventory changed.' }
    $acceptedSource = Read-P914Json $acceptedSourcePath

    $p913ManifestPath = [IO.Path]::GetFullPath([string]$acceptedProof.priorRunManifestPath)
    if (-not (Test-Path -LiteralPath $p913ManifestPath -PathType Leaf) -or (Get-P914FileSha256 $p913ManifestPath) -cne [string]$acceptedProof.priorRunManifestSha256) { throw 'Accepted P9-13 package ancestry manifest changed or is missing.' }
    $p913 = Read-P914Json $p913ManifestPath
    if ([string]$p913.taskId -cne 'P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE' -or [string]$p913.status -cne 'passed') { throw 'Package source derivation does not come from the passed P9-13 route.' }
    $p913Root = Split-Path -Parent $p913ManifestPath
    $derivedPath = Join-Path $p913Root ([string]$p913.source.derivedReceiptPath)
    if ((Get-P914FileSha256 $derivedPath) -cne [string]$p913.source.derivedReceiptSha256) { throw 'Accepted P9-13 product-source derivation receipt changed.' }
    $derived = Read-P914Json $derivedPath
    if ([string]$derived.status -cne 'verified-script-only-derivation' -or [string]$derived.source.productSourceSha256 -cne [string]$p913.source.productSourceSha256) { throw 'P9-13 product-source derivation receipt is inconsistent.' }

    $currentSource = Get-P915CurrentSource $Repository @($acceptedSource.includedRoots)
    Assert-P915OldFilesUnchanged @($acceptedSource.files) @($currentSource.files)
    $ignored = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $derived.source.allowedPaths) { [void]$ignored.Add([string]$path) }
    foreach ($path in $P914Sources) { [void]$ignored.Add([string]$path) }
    foreach ($path in $P915OwnedSources) { [void]$ignored.Add([string]$path) }
    $productRows = @($currentSource.files | Where-Object { -not $ignored.Contains([string]$_.path) })
    $productSha = Get-P915InventoryRows $productRows
    if ($productSha -cne [string]$acceptedProof.productSourceSha256 -or $productSha -cne [string]$p913.source.productSourceSha256) { throw 'Current product sources no longer match the accepted packaged Release fingerprint.' }

    $packageRoot = [IO.Path]::GetFullPath([string]$acceptedProof.packageRoot)
    $package = Get-P914CanonicalInventory $packageRoot
    if ([string]$package.sha256 -cne [string]$acceptedProof.packageSha256 -or [int]$package.fileCount -ne [int]$acceptedProof.packageFileCount) { throw 'Accepted P9-14 package tree no longer matches its recorded complete inventory.' }
    $acceptedPackagePath = Join-Path $acceptedRoot ([string]$acceptedProof.packageInventoryPath)
    if ((Get-P914FileSha256 $acceptedPackagePath) -cne [string]$acceptedProof.packageInventorySha256) { throw 'Accepted P9-14 package inventory file changed.' }
    $acceptedPackage = Read-P914Json $acceptedPackagePath
    if ((Get-P915InventoryRows @($acceptedPackage.files)) -cne [string]$package.sha256 -or [int]$acceptedPackage.fileCount -ne [int]$package.fileCount) { throw 'Current package files differ from the accepted full package inventory.' }
    $appPath = Join-Path $packageRoot 'ClassMngrNext.exe'
    if ((Get-P914FileSha256 $appPath) -cne [string]$acceptedProof.applicationSha256 -or (Get-P914PeMachine $appPath) -ne 0x8664) { throw 'Accepted packaged executable hash or architecture changed.' }
    $releasePath = [IO.Path]::GetFullPath([string]$acceptedProof.acceptedReleaseReceiptPath)
    if ((Get-P914FileSha256 $releasePath) -cne [string]$acceptedProof.acceptedReleaseReceiptSha256) { throw 'Accepted Release build receipt changed.' }
    $release = Read-P914Json $releasePath
    if ([string]$release.status -cne 'succeeded' -or [string]$release.configuration -cne 'Release' -or [string]$release.platform -cne 'x64' -or [int]$release.build.exitCode -ne 0 -or [string]$release.releaseExecutableSha256 -cne (Get-P914FileSha256 $appPath)) { throw 'Accepted Release receipt does not prove the package executable.' }
    $cachePath = [IO.Path]::GetFullPath([string]$acceptedProof.configuredCachePath)
    if ((Get-P914FileSha256 $cachePath) -cne [string]$acceptedProof.configuredCacheSha256 -or (Get-P914FileSha256 $cachePath) -cne [string]$release.cacheSha256After) { throw 'Configured Release CMake cache differs from the accepted build receipt.' }
    $resourceRoot = [IO.Path]::GetFullPath([string]$acceptedProof.resourceRoot)
    $resource = Get-P914CanonicalInventory $resourceRoot
    if ([string]$resource.sha256 -cne [string]$acceptedProof.resourceInventorySha256 -or [string]$resource.sha256 -cne [string]$release.generatedResources.stagedResourceInputs.sha256) { throw 'Staged resource inputs differ from accepted Release receipt.' }

    $sourcePath = Join-Path $EvidenceDirectory 'source-files.json'; Write-P914Json $sourcePath $currentSource
    $packagePath = Join-Path $EvidenceDirectory 'package-files.json'; Write-P914Json $packagePath $package
    $resourcePath = Join-Path $EvidenceDirectory 'resource-files.json'; Write-P914Json $resourcePath $resource
    [ordered]@{
        reusedPackage = $true; reuseBasis = 'P9-14 accepted package reverified against current product-source fingerprint, complete package inventory, x64 executable, staged resources, successful Release receipt, and configured cache'
        priorRunManifestPath = [IO.Path]::GetFullPath($AcceptedManifestPath); priorRunManifestSha256 = $acceptedHash; priorRunId = [string]$accepted.runId
        priorP913ManifestPath = $p913ManifestPath; priorP913ManifestSha256 = Get-P914FileSha256 $p913ManifestPath
        packageRoot = $packageRoot; packageSha256 = [string]$package.sha256; packageFileCount = [int]$package.fileCount
        packageInventoryPath = 'package-files.json'; packageInventoryFileSha256 = Get-P914FileSha256 $packagePath
        applicationPath = [IO.Path]::GetFullPath($appPath); applicationSha256 = Get-P914FileSha256 $appPath; architecture = 'x64'
        productSourceSha256 = $productSha; currentSourceSha256 = [string]$currentSource.sha256
        sourceInventoryPath = 'source-files.json'; sourceInventoryFileSha256 = Get-P914FileSha256 $sourcePath
        acceptedReleaseReceiptPath = $releasePath; acceptedReleaseReceiptSha256 = Get-P914FileSha256 $releasePath
        configuration = 'Release'; platform = 'x64'; generator = [string]$release.generator
        configuredCachePath = $cachePath; configuredCacheSha256 = Get-P914FileSha256 $cachePath
        resourceRoot = $resourceRoot; resourceInventoryPath = 'resource-files.json'
        resourceInventorySha256 = [string]$resource.sha256; resourceInventoryFileSha256 = Get-P914FileSha256 $resourcePath
        freshExactTreeRebuild = $false
    }
}

function Get-P915TopRoots([object]$Context) {
    $roots = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($handle in [P914Native]::GetTopLevelWindows([uint32]$Context.process.Id)) {
        if (-not [P914Native]::IsVisible($handle)) { continue }
        if ([uint32][P914Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id) { throw 'An enumerated P9-15 top-level HWND belongs to another PID.' }
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
        $current = $root.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window') { continue }
        $Context.ownerHandles[(Get-P914RuntimeKey $root)] = [Int64]$handle.ToInt64()
        $roots.Add($root)
    }
    return $roots.ToArray()
}

function Find-P915Suffix([object]$Context, [string]$Suffix, [string]$ControlType = '', [string]$ContainerRootKey = '') {
    $found = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    if ($ContainerRootKey -ceq 'main') { $roots = @($Context.mainWindowRoot) }
    elseif ($ContainerRootKey -ceq 'nextPreferencesDialog') {
        $dialog = Get-P915PreferencesDialogInfo $Context
        if ($null -eq $dialog) { $roots = @() }
        else { $Context.preferencesDialogRoot = $dialog.root; $Context.preferencesDialogHandle = [Int64]$dialog.windowHandle; $roots = @($dialog.root) }
    }
    elseif ($ContainerRootKey) { throw "Unsupported P9-15 UIA container root key: $ContainerRootKey" }
    else { $roots = @(Get-P915TopRoots $Context) }
    foreach ($root in $roots) {
        if ($null -eq $root) { continue }
        $nodes = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new(); $nodes.Add($root)
        foreach ($node in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) { $nodes.Add($node) }
        foreach ($node in $nodes) {
            $current = $node.Current
            if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { continue }
            $id = [string]$current.AutomationId
            if ($id -cne $Suffix -and -not $id.EndsWith('.' + $Suffix, [StringComparison]::Ordinal)) { continue }
            if ($ControlType -and [string]$current.ControlType.ProgrammaticName -cne $ControlType) { continue }
            $rootKey = Get-P914RuntimeKey $root
            $Context.ownerHandles[(Get-P914RuntimeKey $node)] = [Int64]$Context.ownerHandles[$rootKey]
            $found.Add($node)
        }
    }
    if ($found.Count -gt 1) { throw "UIA suffix '$Suffix' resolved to $($found.Count) PID-bound controls instead of one." }
    if ($found.Count -eq 0) { return $null }
    return $found[0]
}

function Find-P915ExactName([object]$Context, [string]$Name, [string]$ControlType, [string]$ContainerRootKey) {
    $found = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    $roots = if ($ContainerRootKey -ceq 'main') { @($Context.mainWindowRoot) }
        elseif ($ContainerRootKey -ceq 'nextPreferencesDialog') {
            $dialog = Get-P915PreferencesDialogInfo $Context
            if ($null -eq $dialog) { @() }
            else { $Context.preferencesDialogRoot = $dialog.root; $Context.preferencesDialogHandle = [Int64]$dialog.windowHandle; @($dialog.root) }
        }
        else { throw "Unsupported P9-15 exact-name container: $ContainerRootKey" }
    foreach ($root in $roots) {
        if ($null -eq $root) { continue }
        $nodes = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new(); $nodes.Add($root)
        foreach ($node in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) { $nodes.Add($node) }
        foreach ($node in $nodes) {
            $current = $node.Current
            if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.Name -cne $Name) { continue }
            if ($ControlType -and [string]$current.ControlType.ProgrammaticName -cne $ControlType) { continue }
            $Context.ownerHandles[(Get-P914RuntimeKey $node)] = [Int64]$Context.ownerHandles[(Get-P914RuntimeKey $root)]
            $found.Add($node)
        }
    }
    if ($found.Count -gt 1) { throw "UIA exact name '$Name' resolved to $($found.Count) PID-bound controls." }
    if ($found.Count -eq 0) { return $null }
    return $found[0]
}

function Wait-P915ExactName([object]$Context, [string]$EvidenceDirectory, [string]$Name, [string]$ControlType, [int]$TimeoutSeconds, [string]$ContainerRootKey) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured app exited while waiting for exact UIA name '$Name'." }
        $element = Find-P915ExactName $Context $Name $ControlType $ContainerRootKey
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for exact PID-bound UIA name '$Name'."
}

function Wait-P915Suffix([object]$Context, [string]$EvidenceDirectory, [string]$Suffix, [string]$ControlType, [int]$TimeoutSeconds, [string]$ContainerRootKey = '') {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured app exited while waiting for UIA suffix $Suffix." }
        $element = Find-P915Suffix $Context $Suffix $ControlType $ContainerRootKey
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for exact PID-bound UIA suffix $Suffix."
}

function Add-P915PatternAction([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Kind, [System.Windows.Automation.AutomationElement]$Element, [string]$Method) {
    Add-P914Action $Context $Manifest $EvidenceDirectory $Kind $Element $Method
}

function Get-P915PreferencesDialogInfo([object]$Context) {
    $expectedKoreanTitle = [string]::Concat([char]0xD658, [char]0xACBD, ' ', [char]0xC124, [char]0xC815)
    $matches = [Collections.Generic.List[object]]::new()
    foreach ($handle in [P914Native]::GetTopLevelWindows([uint32]$Context.process.Id)) {
        if ([Int64]$handle.ToInt64() -eq [Int64]$Context.mainWindowHandle -or -not [P914Native]::IsVisible($handle)) { continue }
        if ([uint32][P914Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id) { throw 'Preferences dialog candidate HWND belongs to another process.' }
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
        $current = $root.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window') { continue }
        $automationId = [string]$current.AutomationId
        $matchesId = $automationId -ceq 'nextPreferencesDialog' -or $automationId.EndsWith('.nextPreferencesDialog', [StringComparison]::Ordinal)
        $matchesTitle = [string]$current.Name -cin @('Preferences', $expectedKoreanTitle)
        if (-not $matchesId -and -not $matchesTitle) { continue }
        if ([string]::IsNullOrWhiteSpace([string]$current.ClassName) -and -not $matchesId) { throw "Preferences dialog UIA root lacks both its automation ID and native class identity (name='$($current.Name)', hwnd=$($handle.ToInt64()))." }
        $Context.ownerHandles[(Get-P914RuntimeKey $root)] = [Int64]$handle.ToInt64()
        $matches.Add([ordered]@{ root = $root; windowHandle = [Int64]$handle.ToInt64(); automationId = $automationId; name = [string]$current.Name; className = [string]$current.ClassName; controlType = [string]$current.ControlType.ProgrammaticName; matchedBy = if ($matchesId) { 'AutomationId nextPreferencesDialog' } else { 'localized Preferences window title plus measured PID/HWND/ControlType.Window' } })
    }
    if ($matches.Count -gt 1) { throw "Preferences dialog UIA identity matched $($matches.Count) visible top-level windows." }
    if ($matches.Count -eq 0) { return $null }
    return $matches[0]
}

function Wait-P915PreferencesDialog([object]$Context, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw 'Measured app exited while waiting for its Preferences dialog.' }
        $dialog = Get-P915PreferencesDialogInfo $Context
        if ($null -ne $dialog) { $Context.preferencesDialogRoot = $dialog.root; $Context.preferencesDialogHandle = [Int64]$dialog.windowHandle; return $dialog }
        Start-Sleep -Milliseconds 100
    }
    throw 'Timed out waiting for the visible PID-bound Preferences dialog window.'
}

function Invoke-P915BoundedClick([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [System.Windows.Automation.AutomationElement]$Element, [Int64]$ExpectedHandle) {
    $current = $Element.Current
    $snapshot = Get-P914ControlSnapshot $Context $Element
    $ownerHandle = [Int64]$snapshot.ownerWindowHandle
    $measuredPid = [uint32]$Context.process.Id
    if ([uint32]$current.ProcessId -ne $measuredPid -or [Int64]$snapshot.ownerWindowProcessId -ne [Int64]$measuredPid -or
        $ownerHandle -ne $ExpectedHandle -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$ownerHandle) -ne $measuredPid -or
        -not [bool]$current.IsEnabled -or [bool]$current.IsOffscreen) { throw "UIA click target failed PID/HWND/enabled/visible checks: $ActionKind" }
    $rectangle = $current.BoundingRectangle
    if ([double]::IsInfinity($rectangle.X) -or [double]::IsInfinity($rectangle.Y) -or [double]::IsNaN($rectangle.X) -or [double]::IsNaN($rectangle.Y) -or $rectangle.Width -le 0 -or $rectangle.Height -le 0) { throw "UIA click target has no nonempty measured rectangle: $ActionKind" }
    $bounds = [P914Native]::GetBounds([IntPtr]$ownerHandle, $measuredPid)
    if ($rectangle.X -lt $bounds[0] -or $rectangle.Y -lt $bounds[1] -or ($rectangle.X + $rectangle.Width) -gt ($bounds[0] + $bounds[2]) -or ($rectangle.Y + $rectangle.Height) -gt ($bounds[1] + $bounds[3])) { throw "UIA click target bounds leave its measured owner HWND: $ActionKind" }
    $x = [int][Math]::Round($rectangle.X + $rectangle.Width / 2)
    $y = [int][Math]::Round($rectangle.Y + $rectangle.Height / 2)
    $action = Add-P915PatternAction $Context $Manifest $EvidenceDirectory $ActionKind $Element 'UIA-targeted window click'
    $action.pointerAction = [ordered]@{ kind = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at the center of the reacquired UIA BoundingRectangle'; actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'; screenX = $x; screenY = $y; targetProcessId = [Int64]$measuredPid; windowHandle = $ownerHandle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$ownerHandle); measuredWindowBounds = [ordered]@{ x = [int]$bounds[0]; y = [int]$bounds[1]; width = [int]$bounds[2]; height = [int]$bounds[3] } }
    [P914Native]::ClickScreenPoint([IntPtr]$ownerHandle, $measuredPid, $x, $y)
    $action.pointerInputReturnedAtUtc = Get-P914Utc; $action.completedAtUtc = Get-P914Utc
    return $action
}

function Get-P915EditMenuName([string]$Locale) {
    if ($Locale -ceq 'Korean') { return [string]::Concat([char]0xD3B8, [char]0xC9D1) }
    return 'Edit'
}

function Open-P915Preferences([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Locale, [int]$TimeoutSeconds) {
    $Context.preferencesExpectedLocale = $Locale; $Context.preferencesDialogRoot = $null; $Context.preferencesDialogHandle = 0L
    $edit = Wait-P915ExactName $Context $EvidenceDirectory (Get-P915EditMenuName $Locale) 'ControlType.MenuItem' $TimeoutSeconds 'main'
    $expanded = $false
    try {
        $pattern = [System.Windows.Automation.ExpandCollapsePattern]$edit.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
        if ($pattern.Current.ExpandCollapseState -ne [System.Windows.Automation.ExpandCollapseState]::Expanded) {
            $action = Add-P915PatternAction $Context $Manifest $EvidenceDirectory 'open-edit-menu' $edit 'ExpandCollapsePattern.Expand'
            $pattern.Expand(); $action.invocationReturnedAtUtc = Get-P914Utc
        }
        $expanded = $true
    } catch {
        $action = Add-P915PatternAction $Context $Manifest $EvidenceDirectory 'open-edit-menu' $edit 'InvokePattern.Invoke'
        ([System.Windows.Automation.InvokePattern]$edit.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)).Invoke()
        $action.invocationReturnedAtUtc = Get-P914Utc; $expanded = $true
    }
    if (-not $expanded) { throw 'Edit menu was not opened through its UIA menu control.' }
    $preferences = Wait-P915Suffix $Context $EvidenceDirectory 'classMngrNextPreferences' '' $TimeoutSeconds 'main'
    $current = $preferences.Current
    if ([string]$current.AutomationId -cne 'classMngrNextPreferences' -and -not [string]$current.AutomationId.EndsWith('.classMngrNextPreferences', [StringComparison]::Ordinal)) { throw 'Preferences QAction automation identity differs from the exact Next action.' }
    $null = Invoke-P915BoundedClick $Context $Manifest $EvidenceDirectory 'open-edit-preferences' $preferences ([Int64]$Context.mainWindowHandle)
    return Wait-P915PreferencesDialog $Context $EvidenceDirectory $TimeoutSeconds
}

function Get-P915SelectedRaw([System.Windows.Automation.AutomationElement]$Element) {
    try {
        $selection = [System.Windows.Automation.SelectionPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionPattern]::Pattern)
        $selected = @($selection.Current.GetSelection())
        if ($selected.Count -eq 1) { return [string]$selected[0].Current.Name }
    } catch { }
    try {
        $value = [System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
        if (-not [string]::IsNullOrWhiteSpace([string]$value.Current.Value)) { return [string]$value.Current.Value }
    } catch { }
    return Get-P914ControlText $Element
}

function Get-P915AppearanceTabName([string]$Locale) {
    if ($Locale -ceq 'Korean') { return [string]::Concat([char]0xBAA8, [char]0xC591) }
    return 'Appearance'
}

function Ensure-P915AppearanceTab([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Locale, [int]$TimeoutSeconds) {
    $name = Get-P915AppearanceTabName $Locale
    $tab = Wait-P915ExactName $Context $EvidenceDirectory $name 'ControlType.TabItem' ([Math]::Min($TimeoutSeconds, 20)) 'nextPreferencesDialog'
    if ([string]$tab.Current.ControlType.ProgrammaticName -cne 'ControlType.TabItem') { throw "Preferences Appearance tab is not exposed as a UIA TabItem: $([string]$tab.Current.ControlType.ProgrammaticName)" }
    $selected = $false
    try { $selected = [bool]([System.Windows.Automation.SelectionItemPattern]$tab.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Current.IsSelected } catch { $selected = $false }
    if ($selected) { return }
    $null = Invoke-P915BoundedClick $Context $Manifest $EvidenceDirectory 'open-preferences-appearance-tab' $tab ([Int64]$Context.preferencesDialogHandle)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt [Math]::Min($TimeoutSeconds, 20)) {
        Drain-P914Output $Context $EvidenceDirectory
        $tab = Find-P915ExactName $Context $name 'ControlType.TabItem' 'nextPreferencesDialog'
        if ($null -ne $tab) {
            try { if ([bool]([System.Windows.Automation.SelectionItemPattern]$tab.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Current.IsSelected) { return } } catch { }
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'UIA-targeted Appearance tab click did not select the Preferences Appearance tab.'
}

function ConvertTo-P915CanonicalPreference([string]$Kind, [string]$Raw) {
    $value = $Raw.Trim()
    $englishKoreanLabel = [string]::Concat([char]0xC601, [char]0xC5B4)
    $koreanKoreanLabel = [string]::Concat([char]0xD55C, [char]0xAD6D, [char]0xC5B4)
    $lightKoreanLabel = [string]::Concat([char]0xBC1D, [char]0xC740, ' ', [char]0xD14C, [char]0xB9C8)
    $darkKoreanLabel = [string]::Concat([char]0xC5B4, [char]0xB450, [char]0xC6B4, ' ', [char]0xD14C, [char]0xB9C8)
    if ($Kind -ceq 'language') {
        if ($value -in @('English', $englishKoreanLabel)) { return 'English' }
        if ($value -in @('Korean', $koreanKoreanLabel)) { return 'Korean' }
    } else {
        if ($value -in @('Light Theme', $lightKoreanLabel)) { return 'Light' }
        if ($value -in @('Dark Theme', $darkKoreanLabel)) { return 'Dark' }
    }
    throw "Preferences $Kind UIA selection is not one of the supported English/Korean or Light/Dark choices: '$Raw'."
}

function Get-P915VisibleChoice([string]$Kind, [string]$Canonical, [string]$CurrentLocale) {
    if ($CurrentLocale -ceq 'Korean') {
        if ($Kind -ceq 'language') { if ($Canonical -ceq 'English') { return [string]::Concat([char]0xC601, [char]0xC5B4) }; return [string]::Concat([char]0xD55C, [char]0xAD6D, [char]0xC5B4) }
        if ($Canonical -ceq 'Light') { return [string]::Concat([char]0xBC1D, [char]0xC740, ' ', [char]0xD14C, [char]0xB9C8) }
        return [string]::Concat([char]0xC5B4, [char]0xB450, [char]0xC6B4, ' ', [char]0xD14C, [char]0xB9C8)
    }
    if ($Kind -ceq 'language') { return $Canonical }
    if ($Canonical -ceq 'Light') { return 'Light Theme' }
    return 'Dark Theme'
}

function Get-P915Preferences([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$TimeoutSeconds, [string]$ExpectedLocale, [string]$ExpectedTheme) {
    $Context.preferencesExpectedLocale = $ExpectedLocale
    $dialog = Get-P915PreferencesDialogInfo $Context
    if ($null -eq $dialog) { throw 'Expected visible PID-bound Preferences dialog disappeared before selection capture.' }
    $Context.preferencesDialogRoot = $dialog.root; $Context.preferencesDialogHandle = [Int64]$dialog.windowHandle
    Ensure-P915AppearanceTab $Context $Manifest $EvidenceDirectory $ExpectedLocale $TimeoutSeconds
    $languageElement = Wait-P915Suffix $Context $EvidenceDirectory 'preferencesLanguage' 'ControlType.ComboBox' $TimeoutSeconds 'nextPreferencesDialog'
    $themeElement = Wait-P915Suffix $Context $EvidenceDirectory 'preferencesTheme' 'ControlType.ComboBox' $TimeoutSeconds 'nextPreferencesDialog'
    if ($languageElement -isnot [System.Windows.Automation.AutomationElement] -or $themeElement -isnot [System.Windows.Automation.AutomationElement]) {
        throw "Preferences UIA search returned unexpected control types: language=$($languageElement.GetType().FullName), theme=$($themeElement.GetType().FullName)."
    }
    $languageRaw = Get-P915SelectedRaw $languageElement; $themeRaw = Get-P915SelectedRaw $themeElement
    $languageCanonical = ConvertTo-P915CanonicalPreference 'language' $languageRaw
    $themeCanonical = ConvertTo-P915CanonicalPreference 'theme' $themeRaw
    if ($languageCanonical -cne $ExpectedLocale -or $themeCanonical -cne $ExpectedTheme) { throw "Preferences UIA selections differ from expected state: locale=$languageCanonical theme=$themeCanonical expected=$ExpectedLocale/$ExpectedTheme." }
    $dialog = Get-P915PreferencesDialogInfo $Context
    $dialogHandle = [Int64]$dialog.windowHandle
    if ($dialogHandle -le 0 -or $dialogHandle -eq [Int64]$Context.mainWindowHandle -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$dialogHandle) -ne [uint32]$Context.process.Id) { throw 'Preferences dialog HWND is missing or belongs to another process.' }
    $languageSnapshot = Get-P914ControlSnapshot $Context $languageElement
    $themeSnapshot = Get-P914ControlSnapshot $Context $themeElement
    $buttons = Find-P915Suffix $Context 'preferencesButtons' '' 'nextPreferencesDialog'
    if ($null -eq $buttons -or [uint32]$buttons.Current.ProcessId -ne [uint32]$Context.process.Id) { throw 'Exact Preferences buttons control is missing from the PID-bound dialog.' }
    $buttonSnapshot = Get-P914ControlSnapshot $Context $buttons
    [ordered]@{ dialogAutomationId = [string]$dialog.automationId; dialogName = [string]$dialog.name; dialogClassName = [string]$dialog.className; dialogControlType = [string]$dialog.controlType; dialogIdentityMatchedBy = [string]$dialog.matchedBy; dialogProcessId = [Int64]$Context.process.Id; dialogWindowHandle = $dialogHandle
        language = [ordered]@{ selectedItem = $languageCanonical; selectedVisibleText = $languageRaw; control = $languageSnapshot }
        theme = [ordered]@{ selectedItem = $themeCanonical; selectedVisibleText = $themeRaw; control = $themeSnapshot }
        buttons = $buttonSnapshot }
}

function Close-P915Preferences([object]$Context, [object]$Manifest, [string]$EvidenceDirectory) {
    $buttons = Find-P915Suffix $Context 'preferencesButtons' '' 'nextPreferencesDialog'
    if ($null -eq $buttons) { throw 'Preferences button box disappeared before close.' }
    $buttonType = [System.Windows.Automation.ControlType]::Button
    $buttonCondition = [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, $buttonType)
    $children = @($buttons.FindAll([System.Windows.Automation.TreeScope]::Descendants, $buttonCondition))
    if ($children.Count -ne 1) { throw "Expected exactly one Close button under preferencesButtons, found $($children.Count)." }
    $button = $children[0]; $current = $button.Current
    if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or -not [bool]$current.IsEnabled -or [bool]$current.IsOffscreen) { throw 'Preferences Close button failed PID/enabled/visible checks.' }
    $dialog = Get-P915PreferencesDialogInfo $Context
    if ($null -eq $dialog) { throw 'Preferences dialog identity disappeared before its Close button action.' }
    $Context.preferencesDialogRoot = $dialog.root; $Context.preferencesDialogHandle = [Int64]$dialog.windowHandle
    $Context.ownerHandles[(Get-P914RuntimeKey $button)] = [Int64]$dialog.windowHandle
    $null = Invoke-P915BoundedClick $Context $Manifest $EvidenceDirectory 'close-preferences-dialog' $button ([Int64]$dialog.windowHandle)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 10) {
        Drain-P914Output $Context $EvidenceDirectory
        if ($null -eq (Get-P915PreferencesDialogInfo $Context)) { $Context.preferencesDialogRoot = $null; $Context.preferencesDialogHandle = 0L; return }
        Start-Sleep -Milliseconds 100
    }
    throw 'Preferences Close button action did not close the measured dialog window.'
}

function Find-P915PreferenceChoice([object]$Context, [string]$Name) {
    $dialog = Get-P915PreferencesDialogInfo $Context
    if ($null -eq $dialog) { return $null }
    $Context.preferencesDialogRoot = $dialog.root; $Context.preferencesDialogHandle = [Int64]$dialog.windowHandle
    $matches = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($candidate in $dialog.root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
        $current = $candidate.Current
        if ([uint32]$current.ProcessId -eq [uint32]$Context.process.Id -and
            [string]$current.ControlType.ProgrammaticName -ceq 'ControlType.ListItem' -and [string]$current.Name -ceq $Name) { $matches.Add($candidate) }
    }
    if ($matches.Count -gt 1) { throw "Preferences dialog exposes $($matches.Count) exact ListItem options named '$Name'." }
    if ($matches.Count -eq 0) { return $null }
    $Context.ownerHandles[(Get-P914RuntimeKey $matches[0])] = [Int64]$dialog.windowHandle
    return $matches[0]
}

function Set-P915Preference([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Kind, [string]$Canonical, [string]$CurrentLocale, [string]$AfterLocale, [int]$TimeoutSeconds) {
    $suffix = if ($Kind -ceq 'language') { 'preferencesLanguage' } else { 'preferencesTheme' }
    $element = Wait-P915Suffix $Context $EvidenceDirectory $suffix 'ControlType.ComboBox' $TimeoutSeconds 'nextPreferencesDialog'
    $rawBefore = Get-P915SelectedRaw $element
    $canonicalBefore = ConvertTo-P915CanonicalPreference $Kind $rawBefore
    if ($Kind -ceq 'language' -and $canonicalBefore -cne $CurrentLocale) { throw "Language control changed before UIA set action: $canonicalBefore != $CurrentLocale." }
    if ($Kind -ceq 'theme' -and $canonicalBefore -ceq $Canonical) { throw "Theme change requested the already selected state: $Canonical." }
    $targetText = Get-P915VisibleChoice $Kind $Canonical $CurrentLocale
    $kindName = 'set-' + $Kind + '-' + $Canonical.ToLowerInvariant()
    $selectedAction = $null; $valueAction = $null
    try {
        $set = [System.Windows.Automation.ValuePattern]$element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
        $valueAction = Add-P915PatternAction $Context $Manifest $EvidenceDirectory $kindName $element 'ValuePattern.SetValue'
        $set.SetValue($targetText)
        $valueAction.invocationReturnedAtUtc = Get-P914Utc
        $watch = [Diagnostics.Stopwatch]::StartNew(); $valueRaw = Get-P915SelectedRaw $element
        while ($watch.Elapsed.TotalSeconds -lt 1.5) {
            try { if ((ConvertTo-P915CanonicalPreference $Kind $valueRaw) -ceq $Canonical) { break } } catch { }
            Start-Sleep -Milliseconds 100
            $valueRaw = Get-P915SelectedRaw $element
        }
        $valueAction.valueAfter = $valueRaw
        try { $valueCanonical = ConvertTo-P915CanonicalPreference $Kind $valueRaw } catch { $valueCanonical = $null }
        if ($valueCanonical -ceq $Canonical) {
            $valueAction.result = 'succeeded'; $valueAction.selectedValue = $Canonical; $selectedAction = $valueAction
        } else {
            $valueAction.actionKind = $kindName + '-value-pattern-no-selection-change'
            $valueAction.result = 'no-selection-change'
        }
    } catch {
        if ($null -ne $valueAction) { $valueAction.actionKind = $kindName + '-value-pattern-failed'; $valueAction.patternFailure = $_.Exception.Message; $valueAction.result = 'failed'; $valueAction.failedAtUtc = Get-P914Utc }
    }

    if ($null -eq $selectedAction) {
        $element = Wait-P915Suffix $Context $EvidenceDirectory $suffix 'ControlType.ComboBox' 10 'nextPreferencesDialog'
        $expand = [System.Windows.Automation.ExpandCollapsePattern]$element.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
        if ($expand.Current.ExpandCollapseState -ne [System.Windows.Automation.ExpandCollapseState]::Expanded) {
            $expandAction = Add-P915PatternAction $Context $Manifest $EvidenceDirectory ($kindName + '-expand-options') $element 'ExpandCollapsePattern.Expand'
            $expand.Expand(); $expandAction.invocationReturnedAtUtc = Get-P914Utc; $expandAction.result = 'succeeded'
        }
        $choice = $null; $timer = [Diagnostics.Stopwatch]::StartNew()
        while ($timer.Elapsed.TotalSeconds -lt 10 -and $null -eq $choice) {
            Drain-P914Output $Context $EvidenceDirectory
            $choice = Find-P915PreferenceChoice $Context $targetText
            if ($null -eq $choice) { Start-Sleep -Milliseconds 100 }
        }
        if ($null -eq $choice) { throw "Expanded Preferences UIA combo has no exact ListItem '$targetText' beneath its measured dialog." }
        $selectAction = Add-P915PatternAction $Context $Manifest $EvidenceDirectory ($kindName + '-select-option') $choice 'SelectionItemPattern.Select'
        try { ([System.Windows.Automation.SelectionItemPattern]$choice.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Select(); $selectAction.selectionReturnedAtUtc = Get-P914Utc; $selectAction.result = 'invoked' }
        catch { $selectAction.selectionFailure = $_.Exception.Message; $selectAction.result = 'failed' }
        $watch = [Diagnostics.Stopwatch]::StartNew(); $confirmedRaw = ''; $confirmed = $false
        while ($watch.Elapsed.TotalSeconds -lt 1.5) {
            $element = Wait-P915Suffix $Context $EvidenceDirectory $suffix 'ControlType.ComboBox' 10 'nextPreferencesDialog'
            $confirmedRaw = Get-P915SelectedRaw $element
            try { if ((ConvertTo-P915CanonicalPreference $Kind $confirmedRaw) -ceq $Canonical) { $confirmed = $true; break } } catch { }
            Start-Sleep -Milliseconds 100
        }
        if ($confirmed) {
            $selectAction.result = 'succeeded'; $selectAction.selectedValue = $Canonical; $selectAction.selectedVisibleText = $confirmedRaw; $selectedAction = $selectAction
        } else {
            $clickAction = Invoke-P915BoundedClick $Context $Manifest $EvidenceDirectory ($kindName + '-activate-option') $choice ([Int64]$Context.preferencesDialogHandle)
            $watch.Restart()
            while ($watch.Elapsed.TotalSeconds -lt 5) {
                Drain-P914Output $Context $EvidenceDirectory
                $element = Wait-P915Suffix $Context $EvidenceDirectory $suffix 'ControlType.ComboBox' 10 'nextPreferencesDialog'
                $confirmedRaw = Get-P915SelectedRaw $element
                try { if ((ConvertTo-P915CanonicalPreference $Kind $confirmedRaw) -ceq $Canonical) { $confirmed = $true; break } } catch { }
                Start-Sleep -Milliseconds 100
            }
            if (-not $confirmed) { $clickAction.result = 'no-selection-change'; throw "UIA option activation did not change $Kind to $Canonical (selected control value='$confirmedRaw')." }
            $clickAction.result = 'succeeded'; $clickAction.selectedValue = $Canonical; $clickAction.selectedVisibleText = $confirmedRaw; $selectedAction = $clickAction
        }
    }

    if ($Kind -ceq 'language') { $Context.preferencesExpectedLocale = $AfterLocale }
    $element = Wait-P915Suffix $Context $EvidenceDirectory $suffix 'ControlType.ComboBox' 10 'nextPreferencesDialog'
    $observed = Get-P915SelectedRaw $element
    $canonicalObserved = ConvertTo-P915CanonicalPreference $Kind $observed
    if ($canonicalObserved -cne $Canonical -or [string]$selectedAction.selectedValue -cne $Canonical) { throw "UIA did not confirm requested $Kind value $Canonical after its selected action." }
    return [ordered]@{ kind = $Kind; value = $Canonical; visibleText = $observed; before = $rawBefore; expectedVisibleText = (Get-P915VisibleChoice $Kind $Canonical $AfterLocale); actionKind = [string]$selectedAction.actionKind }
}

function Find-P915SchedulePageName([object]$Context, [string]$Locale) {
    $koreanTitle = [string]::Concat([char]0xC8FC, [char]0xAC04, ' ', [char]0xC218, [char]0xC5C5, ' ', [char]0xC77C, [char]0xC815)
    $expected = if ($Locale -ceq 'Korean') { $koreanTitle } else { 'Weekly Class Schedule' }
    $matches = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($node in $Context.mainWindowRoot.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
        $current = $node.Current
        if ([uint32]$current.ProcessId -eq [uint32]$Context.process.Id -and [string]$current.Name -ceq $expected) { $matches.Add($node) }
    }
    if ($matches.Count -eq 0) { throw "Schedule page accessible name for locale $Locale is not exposed by PID-bound UIA." }
    $element = $matches[0]; $Context.ownerHandles[(Get-P914RuntimeKey $element)] = [Int64]$Context.mainWindowHandle
    return [ordered]@{ accessibleName = [string]$element.Current.Name; automationId = [string]$element.Current.AutomationId; controlType = [string]$element.Current.ControlType.ProgrammaticName; processId = [Int64]$element.Current.ProcessId; ownerWindowHandle = [Int64]$Context.mainWindowHandle }
}

function Get-P915VisualMetrics([string]$Path) {
    $bitmap = [Drawing.Bitmap]::new($Path)
    try {
        $stepX = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 100))
        $stepY = [Math]::Max(1, [int][Math]::Floor($bitmap.Height / 75))
        $sum = [Double]0; $sampled = [Int64]0; $dark = [Int64]0; $bright = [Int64]0
        $colors = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        for ($y = 0; $y -lt $bitmap.Height; $y += $stepY) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $stepX) {
                $color = $bitmap.GetPixel($x, $y)
                $luminance = 0.2126 * [Double]$color.R + 0.7152 * [Double]$color.G + 0.0722 * [Double]$color.B
                $sum += $luminance; $sampled++
                if ($luminance -lt 100) { $dark++ }; if ($luminance -gt 210) { $bright++ }
                [void]$colors.Add(([Math]::Floor($color.R / 32)).ToString() + ',' + ([Math]::Floor($color.G / 32)).ToString() + ',' + ([Math]::Floor($color.B / 32)).ToString())
            }
        }
        if ($sampled -le 0) { throw 'Screenshot visual sampler produced no pixels.' }
        [ordered]@{ sampleCount = $sampled; meanLuminance = [Math]::Round($sum / $sampled, 2); darkPixelRatio = [Math]::Round($dark / [Double]$sampled, 4); brightPixelRatio = [Math]::Round($bright / [Double]$sampled, 4); quantizedColors = $colors.Count }
    } finally { $bitmap.Dispose() }
}

function Save-P915ScheduleState([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Name, [string]$Locale, [string]$Theme, [object]$Preferences, [object]$Activation) {
    $table = $null
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 30) {
        Drain-P914Output $Context $EvidenceDirectory
        # Qt exposes the object-name suffix on multiple descendants in some
        # builds. Bind to the actual table control first, then verify its
        # GridPattern/TablePattern below; a generic suffix match is ambiguous.
        $table = Find-P915Suffix $Context 'nextScheduleTable' 'ControlType.Table' 'main'
        if ($null -eq $table) { $table = Find-P915Suffix $Context 'nextScheduleTable' 'ControlType.DataGrid' 'main' }
        if ($null -ne $table) { break }
        Start-Sleep -Milliseconds 100
    }
    if ($null -eq $table) { throw 'Current Schedule page did not expose nextScheduleTable.' }
    if ([string]$table.Current.ControlType.ProgrammaticName -notin @('ControlType.Table', 'ControlType.DataGrid') -or [uint32]$table.Current.ProcessId -ne [uint32]$Context.process.Id) { throw 'Schedule table UIA control identity is not the expected PID-bound Table/DataGrid.' }
    $tableSnapshot = Get-P914ControlSnapshot $Context $table
    $grid = Get-P914Grid $table
    $gridCurrent = $grid.Current
    $expectedGrid = $Context.expectedScheduleGrid
    if ($null -eq $expectedGrid -or [int]$gridCurrent.RowCount -ne [int]$expectedGrid.rowCount -or [int]$gridCurrent.ColumnCount -ne [int]$expectedGrid.columnCount) { throw "Schedule UIA GridPattern dimensions differ from the fixture-derived display grid ($($gridCurrent.RowCount)x$($gridCurrent.ColumnCount))." }
    $tableKey = Get-P914RuntimeKey $table
    if (-not $Context.ownerHandles.ContainsKey($tableKey)) { throw 'Schedule table is missing its current measured owner HWND mapping.' }
    $ownerHandle = [Int64]$Context.ownerHandles[$tableKey]
    if ($ownerHandle -ne [Int64]$Context.mainWindowHandle -or [Int64][P914Native]::GetWindowProcessId([IntPtr]$ownerHandle) -ne [Int64]$Context.process.Id) { throw 'Schedule table owner HWND differs from the measured main window.' }

    $tablePattern = [System.Windows.Automation.TablePattern]$table.GetCurrentPattern([System.Windows.Automation.TablePattern]::Pattern)
    $headerElements = @($tablePattern.Current.GetColumnHeaders())
    if ($headerElements.Count -ne [int]$gridCurrent.ColumnCount) { throw 'Schedule UIA TablePattern header count differs from its GridPattern columns.' }
    $headers = [Collections.Generic.List[object]]::new()
    for ($column = 0; $column -lt $headerElements.Count; $column++) {
        $header = $headerElements[$column]; $current = $header.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { throw "Schedule header $column belongs to another process." }
        $Context.ownerHandles[(Get-P914RuntimeKey $header)] = $ownerHandle
        $headers.Add([ordered]@{ column = $column; text = Get-P914ControlText $header; processId = [Int64]$current.ProcessId; ownerWindowHandle = $ownerHandle; ownerWindowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$ownerHandle); automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; runtimeId = Get-P914RuntimeKey $header })
    }
    $cells = [Collections.Generic.List[object]]::new()
    for ($row = 0; $row -lt [int]$gridCurrent.RowCount; $row++) {
        for ($column = 0; $column -lt [int]$gridCurrent.ColumnCount; $column++) {
            $cell = $grid.GetItem($row, $column); $cellCurrent = $cell.Current
            if ([uint32]$cellCurrent.ProcessId -ne [uint32]$Context.process.Id) { throw "Schedule UIA cell $row,$column belongs to another process." }
            $Context.ownerHandles[(Get-P914RuntimeKey $cell)] = $ownerHandle
            $cells.Add([ordered]@{ row = $row; column = $column; text = Get-P914ControlText $cell; control = Get-P914ControlSnapshot $Context $cell; runtimeId = Get-P914RuntimeKey $cell })
        }
    }
    $pageName = Find-P915SchedulePageName $Context $Locale
    $generic = [ordered]@{ event = 'page-lifecycle'; page_key = 'schedule'; state = 'activated'; sample_available = [bool]$Activation.record.sample_available; page_instance_available = [bool]$Activation.record.page_instance_available; page = $Activation.record.page }
    $screenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory ('schedule-' + $Name) $Context.mainWindowRoot
    $visual = Get-P915VisualMetrics (Join-Path $EvidenceDirectory ([string]$screenshot.relativePath))
    $record = [ordered]@{ name = $Name; locale = $Locale; theme = $Theme; capturedAtUtc = Get-P914Utc
        processId = [Int64]$Context.process.Id; mainWindow = [ordered]@{ processId = [Int64]$Context.process.Id; windowHandle = [Int64]$Context.mainWindowHandle; ownerProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle) }
        schedulePage = $pageName; table = [ordered]@{ control = $tableSnapshot; gridPattern = [ordered]@{ available = $true; rowCount = [int]$gridCurrent.RowCount; columnCount = [int]$gridCurrent.ColumnCount; pattern = 'GridPattern' }; automationIdSuffix = 'nextScheduleTable' }
        headers = $headers.ToArray(); cells = $cells.ToArray(); preferences = $Preferences; genericLifecycle = $generic }
    $snapshotPath = Join-Path (Join-Path $EvidenceDirectory 'uia') ('schedule-state-' + $Name + '.json')
    Write-P914Json $snapshotPath $record
    $snapshotRef = [ordered]@{ relativePath = 'uia/' + (Split-Path -Leaf $snapshotPath); sha256 = Get-P914FileSha256 $snapshotPath; processId = [Int64]$Context.process.Id; windowHandle = [Int64]$Context.mainWindowHandle; capturedAtUtc = $record.capturedAtUtc }
    $Manifest.route.uiaSnapshots.Add($snapshotRef)
    $stable = Add-P914StableCheckpoint $Context $Manifest ('schedule-' + $Name)
    $state = [ordered]@{ name = $Name; locale = $Locale; theme = $Theme; capturedAtUtc = Get-P914Utc; preferences = $Preferences
        uiaSnapshot = $snapshotRef; screenshot = $screenshot; visualMetrics = $visual; stableCheckpoint = $stable }
    $Manifest.route.scheduleStates.Add($state)
    return $state
}

function Get-P915PageActivation([object]$Context, [string]$EvidenceDirectory, [Int64]$AfterArrival, [int]$TimeoutSeconds) {
    $event = Wait-P914Lifecycle $Context $EvidenceDirectory 'schedule' 'activated' $AfterArrival $TimeoutSeconds
    if (-not [bool]$event.record.sample_available -or -not [bool]$event.record.page_instance_available) { throw 'Schedule page lifecycle activation lacks generic page/widget metrics.' }
    return $event
}

function Invoke-P915Route([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $route = $Manifest.route
    $firstWorkspace = $false
    $wait = [Diagnostics.Stopwatch]::StartNew()
    while ($wait.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory
        if (@($Context.routeEvents | Where-Object { $_.targetPageId -ceq 'my_workspace' }).Count -gt 0) { $firstWorkspace = $true; break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $firstWorkspace) { throw 'Initial Next route did not reach My Workspace.' }
    Set-P914SamplerContext $Context 'normal-use' 'startup-ready-my-workspace'
    $null = Add-P914StableCheckpoint $Context $Manifest 'startup-ready-my-workspace'

    $prefsDialog = Open-P915Preferences $Context $Manifest $EvidenceDirectory 'English' $TimeoutSeconds
    $initialPreferences = Get-P915Preferences $Context $Manifest $EvidenceDirectory $TimeoutSeconds 'English' 'Light'
    $initialPreferencePath = Join-Path (Join-Path $EvidenceDirectory 'uia') 'preferences-initial-english-light.json'
    Write-P914Json $initialPreferencePath $initialPreferences
    Close-P915Preferences $Context $Manifest $EvidenceDirectory

    $priorArrival = [Int64]$Context.arrival; Set-P914SamplerContext $Context 'operation' 'navigate-schedule-initial'
    $null = Select-P914Route $Context $Manifest $EvidenceDirectory 'Schedule' 'schedule'
    $activation = Get-P915PageActivation $Context $EvidenceDirectory $priorArrival $TimeoutSeconds
    Set-P914SamplerContext $Context 'normal-use' 'schedule-initial-english-light'
    $null = Save-P915ScheduleState $Context $Manifest $EvidenceDirectory 'initial' 'English' 'Light' $initialPreferences $activation

    $transitionSteps = @(
        @{ name = 'ko_light'; kind = 'language'; desired = 'Korean'; localeBefore = 'English'; localeAfter = 'Korean'; theme = 'Light' },
        @{ name = 'ko_dark'; kind = 'theme'; desired = 'Dark'; localeBefore = 'Korean'; localeAfter = 'Korean'; theme = 'Dark' },
        @{ name = 'en_dark'; kind = 'language'; desired = 'English'; localeBefore = 'Korean'; localeAfter = 'English'; theme = 'Dark' },
        @{ name = 'en_light'; kind = 'theme'; desired = 'Light'; localeBefore = 'English'; localeAfter = 'English'; theme = 'Light' }
    )
    $locale = 'English'; $theme = 'Light'
    foreach ($step in $transitionSteps) {
        Set-P914SamplerContext $Context 'operation' ('preferences-' + [string]$step.name)
        $dialog = Open-P915Preferences $Context $Manifest $EvidenceDirectory $locale $TimeoutSeconds
        $null = Get-P915Preferences $Context $Manifest $EvidenceDirectory $TimeoutSeconds $locale $theme
        $null = Set-P915Preference $Context $Manifest $EvidenceDirectory ([string]$step.kind) ([string]$step.desired) $locale ([string]$step.localeAfter) $TimeoutSeconds
        $locale = [string]$step.localeAfter; $theme = [string]$step.theme
        Start-Sleep -Milliseconds 500
        $currentPreferences = Get-P915Preferences $Context $Manifest $EvidenceDirectory $TimeoutSeconds $locale $theme
        $preferencesPath = Join-Path (Join-Path $EvidenceDirectory 'uia') ('preferences-' + [string]$step.name + '.json')
        Write-P914Json $preferencesPath $currentPreferences
        Close-P915Preferences $Context $Manifest $EvidenceDirectory
        Start-Sleep -Milliseconds 500
        Set-P914SamplerContext $Context 'normal-use' ('schedule-' + [string]$step.name)
        $null = Save-P915ScheduleState $Context $Manifest $EvidenceDirectory ([string]$step.name) $locale $theme $currentPreferences $activation
    }

    for ($reentry = 1; $reentry -le 2; $reentry++) {
        Set-P914SamplerContext $Context 'operation' ('schedule-leave-reentry-' + $reentry)
        $priorArrival = [Int64]$Context.arrival
        $null = Select-P914Route $Context $Manifest $EvidenceDirectory 'My Workspace' 'my_workspace'
        $null = Wait-P914Lifecycle $Context $EvidenceDirectory 'schedule' 'suspended' $priorArrival $TimeoutSeconds
        $returnArrival = [Int64]$Context.arrival
        $null = Select-P914Route $Context $Manifest $EvidenceDirectory 'Schedule' 'schedule'
        $activation = Get-P915PageActivation $Context $EvidenceDirectory $returnArrival $TimeoutSeconds
        Start-Sleep -Milliseconds 250
        Set-P914SamplerContext $Context 'normal-use' ('schedule-reentry-' + $reentry)
        $null = Save-P915ScheduleState $Context $Manifest $EvidenceDirectory ('reentry_' + $reentry) 'English' 'Light' $currentPreferences $activation
    }
    $route.refreshReentryCount = 2
    $route.refreshMechanism = 'Schedule page activation reload after My Workspace leave/re-entry'
    $route.workflowCompletedAtUtc = Get-P914Utc
    Set-P914SamplerContext $Context 'normal-use' 'schedule-route-ready-for-natural-close'
}

function Get-P915StableSettings([string]$EvidenceDirectory) {
    $settingsRoot = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings'
    $profile = Join-Path (Join-Path $settingsRoot 'PaperCloud') 'ClassMngr.ini'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $profile))
    [IO.File]::WriteAllText($profile, "[options]`r`nlanguage=1`r`ntheme=1`r`n", [Text.UTF8Encoding]::new($false))
    $inventory = Get-P914CanonicalInventory $settingsRoot
    $path = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings-before-launch-inventory.json'
    Write-P914Json $path $inventory
    if ([int]$inventory.fileCount -ne 1 -or [IO.File]::ReadAllText($profile, [Text.Encoding]::UTF8) -cne "[options]`r`nlanguage=1`r`ntheme=1`r`n") { throw 'Fresh isolated settings profile did not encode English/Light exactly.' }
    [ordered]@{ root = [IO.Path]::GetFullPath($settingsRoot); settingsRootFromEnvironment = $true; globalDefaultChanged = $false
        beforeLaunchInventoryPath = 'runtime/settings-before-launch-inventory.json'; beforeLaunchInventorySha256 = Get-P914FileSha256 $path; beforeLaunchInventoryContentSha256 = [string]$inventory.sha256
        initialLanguagePreference = 'English'; initialThemePreference = 'Light'; initialPreferenceFile = 'runtime/settings/PaperCloud/ClassMngr.ini'; preferenceFileSha256 = Get-P914FileSha256 $profile }
}

function Get-P915Workspace([string]$Repository, [string]$EvidenceDirectory) {
    $base = Join-Path $Repository 'artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1/workspace-snapshots/master-blank.tps'
    $seed = Join-Path $Repository 'tests/fixtures/workspaces/large_startup.sql'
    $preparer = Join-Path $PSScriptRoot 'prepare_p9_15_schedule_workspace.py'
    $workspace = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'schedule-locale-theme.tps'
    $receipt = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'preparation-receipt.json'
    $prepOutput = Join-Path (Join-Path $EvidenceDirectory 'logs') 'workspace-preparation-output.txt'
    $lines = @(& python $preparer prepare --base-workspace $base --seed-sql $seed --output $workspace --receipt $receipt 2>&1 | ForEach-Object { [string]$_ }); $code = $LASTEXITCODE
    [IO.File]::WriteAllLines($prepOutput, $lines, [Text.UTF8Encoding]::new($false))
    if ($code -ne 0) { throw "P9-15 fixture preparation failed with exit code $code." }
    $preparation = Read-P914Json $receipt
    if ([string]$preparation.status -cne 'passed' -or [int]$preparation.schedule.classCount -ne 96 -or [int]$preparation.schedule.scheduleEntryCount -ne 768) { throw 'Prepared P9-15 fixture does not prove 96 source classes and 768 regular schedule entries.' }
    $before = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'before-snapshot.json'
    $beforeOutput = Join-Path (Join-Path $EvidenceDirectory 'logs') 'before-snapshot-output.txt'
    $beforeLines = @(& python $preparer snapshot --phase before --workspace $workspace --preparation-receipt $receipt --output $before 2>&1 | ForEach-Object { [string]$_ }); $beforeCode = $LASTEXITCODE
    [IO.File]::WriteAllLines($beforeOutput, $beforeLines, [Text.UTF8Encoding]::new($false))
    if ($beforeCode -ne 0) { throw "P9-15 before snapshot failed with exit code $beforeCode." }
    [ordered]@{ workspacePath = [IO.Path]::GetFullPath($workspace); workspaceSha256 = Get-P914FileSha256 $workspace
        preparationPath = [IO.Path]::GetFullPath($receipt); preparationSha256 = Get-P914FileSha256 $receipt
        beforePath = [IO.Path]::GetFullPath($before); beforeSha256 = Get-P914FileSha256 $before
        preparation = $preparation; beforeSnapshot = Read-P914Json $before }
}

function Get-P915Package([string]$Repository, [string]$EvidenceDirectory, [string]$AcceptedManifest) {
    try { return Initialize-P915ReusedPackage $Repository $EvidenceDirectory $AcceptedManifest }
    catch {
        $reuseFailure = $_.Exception.Message
        $cachePath = Join-Path $BuildDirectory 'CMakeCache.txt'
        if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "Package equivalence failed ('$reuseFailure') and configured Release CMake cache is missing: $cachePath" }
        $cache = @(Get-Content -LiteralPath $cachePath)
        $getValue = { param($key) $matches = @($cache | Where-Object { $_ -match "^$([regex]::Escape($key)):[^=]+=" }); if ($matches.Count -ne 1) { throw "Expected exactly one configured CMake value '$key'." }; ($matches[0] -split '=', 2)[1] }
        if ([IO.Path]::GetFullPath((& $getValue 'CMAKE_HOME_DIRECTORY')) -ine $Repository -or (& $getValue 'CLASSMNGR_NEXT_RELEASE_PACKAGE') -cne 'ON' -or (& $getValue 'CMAKE_GENERATOR') -notmatch '^Visual Studio' -or (& $getValue 'CMAKE_GENERATOR_PLATFORM') -notin @('x64','')) { throw 'Cannot build fallback package: standard configured build is not a Windows x64 Release package tree.' }
        $cmake = [IO.Path]::GetFullPath((& $getValue 'CMAKE_COMMAND'))
        $qt6Dir = [IO.Path]::GetFullPath((& $getValue 'Qt6_DIR'))
        $qtBin = [IO.Path]::GetFullPath((Join-Path $qt6Dir '../../../bin'))
        $windeploy = Join-Path $qtBin 'windeployqt.exe'
        $packageRoot = Join-Path $EvidenceDirectory 'package'
        [void][IO.Directory]::CreateDirectory($packageRoot)
        $buildLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'local-release-build.log'
        $oldPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
        try {
            [Environment]::SetEnvironmentVariable('PATH', "$qtBin;$oldPath", 'Process')
            $buildLines = @(& $cmake --build $BuildDirectory --config Release --verbose --target ClassMngrNext ClassMngrNextResourcePackage 2>&1 | ForEach-Object { [string]$_ }); $buildCode = $LASTEXITCODE
        } finally { [Environment]::SetEnvironmentVariable('PATH', $oldPath, 'Process') }
        [IO.File]::WriteAllLines($buildLog, @("cmake --build $BuildDirectory --config Release --verbose --target ClassMngrNext ClassMngrNextResourcePackage", $buildLines, "exitCode=$buildCode"), [Text.UTF8Encoding]::new($false))
        if ($buildCode -ne 0) { throw "Package equivalence failed ('$reuseFailure') and local Release build failed with exit code $buildCode." }
        $installLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'resource-install.log'
        $installLines = @(& $cmake --install $BuildDirectory --config Release --prefix $packageRoot --component ClassMngrNextResources 2>&1 | ForEach-Object { [string]$_ }); $installCode = $LASTEXITCODE
        [IO.File]::WriteAllLines($installLog, $installLines, [Text.UTF8Encoding]::new($false))
        if ($installCode -ne 0) { throw "Configured Release resource package install failed with exit code $installCode." }
        $releaseExe = Join-Path $BuildDirectory 'next-package-build/Release/ClassMngr.exe'
        $appPath = Join-Path $packageRoot 'ClassMngrNext.exe'
        Copy-Item -LiteralPath $releaseExe -Destination $appPath
        if (-not (Test-Path -LiteralPath $windeploy -PathType Leaf)) { throw "Cache-bound windeployqt is missing: $windeploy" }
        $oldPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
        try {
            [Environment]::SetEnvironmentVariable('PATH', "$qtBin;$oldPath", 'Process')
            $deploy = @(& $windeploy --qmldir (Join-Path $Repository 'src/features/calendar/ui/qml') --skip-plugin-types qmltooling --include-plugins qoffscreen,qsqlite --exclude-plugins qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql --translations en,ko --compiler-runtime --dir $packageRoot $appPath 2>&1 | ForEach-Object { [string]$_ }); $deployCode = $LASTEXITCODE
        } finally { [Environment]::SetEnvironmentVariable('PATH', $oldPath, 'Process') }
        [IO.File]::WriteAllLines((Join-Path (Join-Path $EvidenceDirectory 'logs') 'windeployqt.log'), $deploy, [Text.UTF8Encoding]::new($false))
        if ($deployCode -ne 0) { throw "Cache-bound windeployqt failed with exit code $deployCode." }
        $inventory = Get-P914CanonicalInventory $packageRoot
        $packagePath = Join-Path $EvidenceDirectory 'package-files.json'; Write-P914Json $packagePath $inventory
        $source = Get-P914SourceInventory $Repository @('CMakeLists.txt','CMakePresets.json','cmake','resources','scripts/phase9','src','tests','docs/qt-rewrite/visual-baseline/release/large-class-transfer-boundary')
        $sourcePath = Join-Path $EvidenceDirectory 'source-files.json'; Write-P914Json $sourcePath $source
        $resourceRoot = Join-Path $packageRoot 'resources'
        $resource = Get-P914CanonicalInventory $resourceRoot
        $resourcePath = Join-Path $EvidenceDirectory 'resource-files.json'; Write-P914Json $resourcePath $resource
        [ordered]@{ reusedPackage = $false; reuseFailureReason = $reuseFailure; freshExactTreeRebuild = $true
            packageRoot = [IO.Path]::GetFullPath($packageRoot); packageSha256 = [string]$inventory.sha256; packageFileCount = [int]$inventory.fileCount
            packageInventoryPath = 'package-files.json'; packageInventoryFileSha256 = Get-P914FileSha256 $packagePath
            applicationPath = [IO.Path]::GetFullPath($appPath); applicationSha256 = Get-P914FileSha256 $appPath; architecture = 'x64'
            productSourceSha256 = [string]$source.sha256; currentSourceSha256 = [string]$source.sha256; sourceInventoryPath = 'source-files.json'; sourceInventoryFileSha256 = Get-P914FileSha256 $sourcePath
            acceptedReleaseReceiptPath = $null; acceptedReleaseReceiptSha256 = $null; configuration = 'Release'; platform = 'x64'; generator = (& $getValue 'CMAKE_GENERATOR')
            configuredCachePath = [IO.Path]::GetFullPath($cachePath); configuredCacheSha256 = Get-P914FileSha256 $cachePath
            resourceRoot = [IO.Path]::GetFullPath($resourceRoot); resourceInventoryPath = 'resource-files.json'; resourceInventorySha256 = [string]$resource.sha256; resourceInventoryFileSha256 = Get-P914FileSha256 $resourcePath
            localBuild = [ordered]@{ buildLogPath = 'logs/local-release-build.log'; buildLogSha256 = Get-P914FileSha256 $buildLog; buildExitCode = $buildCode; resourceInstallLogPath = 'logs/resource-install.log'; resourceInstallExitCode = $installCode; windeployqtPath = [IO.Path]::GetFullPath($windeploy); windeployqtSha256 = Get-P914FileSha256 $windeploy; deployExitCode = $deployCode }
        }
    }
}

function Save-P915FinalWorkspaceSnapshot([string]$EvidenceDirectory, [object]$Fixture) {
    $preparer = Join-Path $PSScriptRoot 'prepare_p9_15_schedule_workspace.py'
    $after = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'after-snapshot.json'
    $log = Join-Path (Join-Path $EvidenceDirectory 'logs') 'after-snapshot-output.txt'
    $lines = @(& python $preparer snapshot --phase after --workspace $Fixture.workspacePath --preparation-receipt $Fixture.preparationPath --output $after 2>&1 | ForEach-Object { [string]$_ }); $code = $LASTEXITCODE
    [IO.File]::WriteAllLines($log, $lines, [Text.UTF8Encoding]::new($false))
    if ($code -ne 0) { throw "P9-15 after snapshot failed with exit code $code." }
    [ordered]@{ relativePath = 'workspace/after-snapshot.json'; sha256 = Get-P914FileSha256 $after; phase = 'after'; workspaceSha256 = [string](Read-P914Json $after).workspaceSha256 }
}

function Invoke-P915PackagedLifecycle {
    $runId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
    $evidenceDirectory = Join-Path $EvidenceRoot $runId
    if (Test-Path -LiteralPath $evidenceDirectory) { throw "Refusing to overwrite P9-15 evidence: $evidenceDirectory" }
    foreach ($name in @('', 'logs', 'runtime', 'runtime/settings', 'runtime/local-app-data', 'runtime/app-data', 'runtime/temp', 'runtime/diagnostics', 'workspace', 'screenshots', 'uia', 'output')) { [void][IO.Directory]::CreateDirectory((Join-Path $evidenceDirectory $name)) }
    $manifestPath = Join-Path $evidenceDirectory 'manifest.json'
    $manifest = [ordered]@{ schema = $Schema; taskId = $TaskId; runId = $runId; status = 'running'; startedAtUtc = Get-P914Utc; completedAtUtc = $null; repositoryRoot = $RepositoryRoot
        packageProvenance = $null; fixture = $null; settings = $null; process = $null; processExit = $null
        route = [ordered]@{ workflow = 'Schedule locale/theme packaged Windows x64 Release'; actions = [Collections.Generic.List[object]]::new(); observations = [Collections.Generic.List[object]]::new(); routeEvents = @(); lifecycleEvents = @(); screenshots = [Collections.Generic.List[object]]::new(); uiaSnapshots = [Collections.Generic.List[object]]::new(); scheduleStates = [Collections.Generic.List[object]]::new(); refreshReentryCount = 0; refreshMechanism = $null }
        memory = [ordered]@{ stableCheckpoints = [Collections.Generic.List[object]]::new(); processMetrics = $null; thresholds = [ordered]@{ stableWorkingSetExclusiveBytes = $StrictWorkingSetLimit; strictAtOrAboveFails = $true; operationPeakWorkingSetExclusiveBytesAdvisory = $OperationPeakAdvisoryLimit; operationPeakAdvisoryOnly = $true; returnToBaselineAllowance = 'none defined' } }
        cleanup = $null; failure = $null; validation = $null }
    $failure = $null; $context = $null; $fixture = $null; $package = $null; $settings = $null; $completed = $false
    try {
        $package = Get-P915Package $RepositoryRoot $evidenceDirectory $PriorRunManifestPath
        $manifest.packageProvenance = $package
        $fixture = Get-P915Workspace $RepositoryRoot $evidenceDirectory
        $manifest.fixture = [ordered]@{ kind = 'schedule-locale-theme'; workspacePath = $fixture.workspacePath; preparationReceipt = [ordered]@{ relativePath = 'workspace/preparation-receipt.json'; sha256 = $fixture.preparationSha256; workspaceSha256 = [string]$fixture.preparation.workspaceSha256; classCount = 96; scheduleEntryCount = 768 }; beforeSnapshot = [ordered]@{ relativePath = 'workspace/before-snapshot.json'; sha256 = $fixture.beforeSha256; phase = 'before' }; afterSnapshot = $null; sourceBaseWorkspacePath = [string]$fixture.preparation.baseWorkspace; sourceBaseWorkspaceSha256 = [string]$fixture.preparation.baseWorkspaceSha256; sourceSeedSqlPath = [string]$fixture.preparation.seedSql; sourceSeedSqlSha256 = [string]$fixture.preparation.seedSqlSha256 }
        $settings = Get-P915StableSettings $evidenceDirectory; $manifest.settings = $settings
        Write-P914Json $manifestPath $manifest
        $context = Start-P914App 'schedule-locale-theme' $evidenceDirectory ([string]$fixture.workspacePath) ([string]$package.applicationPath)
        $context.preferencesDialogRoot = $null; $context.preferencesDialogHandle = 0L; $context.preferencesExpectedLocale = 'English'
        $context.sampler = [P914ProcessSampler]::new($context.process)
        $null = Wait-P914MainRoot $context $evidenceDirectory $ReadinessTimeoutSeconds
        Drain-P914Output $context $evidenceDirectory
        $context.process.Refresh()
        $manifest.process = [ordered]@{ processId = [Int64]$context.process.Id; processImagePath = [string]$context.processImagePath; processImageSha256 = [string]$context.processImageSha256; architecture = 'x64'; startedAtUtc = $context.process.StartTime.ToUniversalTime().ToString('o'); workspacePath = [string]$context.workspacePath; commandLine = [ordered]@{ executable = [string]$package.applicationPath; workspaceArgument = [string]$fixture.workspacePath }; stdoutPath = 'logs/process-stdout.log'; stderrPath = 'logs/process-stderr.log'; mainWindowHandle = [Int64]$context.mainWindowHandle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle); hwndProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle); closeMethod = $null; forcedTermination = $false }
        $context.expectedScheduleGrid = $fixture.preparation.schedule.uiGrid
        if ([Int64]$manifest.process.windowProcessId -ne [Int64]$context.process.Id -or [Int64]$manifest.process.hwndProcessId -ne [Int64]$context.process.Id) { throw 'Main Window HWND PID differs from the measured process.' }
        Set-P914SamplerContext $context 'normal-use' 'startup-ready'
        Invoke-P915Route $context $manifest $evidenceDirectory $ReadinessTimeoutSeconds
        Set-P914SamplerContext $context 'normal-use' 'ready-for-natural-process-close'
        $exit = Invoke-P914NaturalClose $context $manifest $evidenceDirectory $ExitTimeoutSeconds
        $manifest.processExit = $exit; $manifest.process.closeMethod = [string]$exit.closeMethod; $manifest.process.forcedTermination = $false
        $manifest.cleanup = Get-P914RuntimeCleanup $context $evidenceDirectory
        if (@($manifest.cleanup.workspaceSidecarFilesAfterExit).Count -ne 0) { throw 'Workspace sidecar files remain after the natural process exit.' }
        $manifest.fixture.afterSnapshot = Save-P915FinalWorkspaceSnapshot $evidenceDirectory $fixture
        $completed = $true
    } catch {
        $failure = [ordered]@{ message = $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = [string]$_.CategoryInfo.Category; fullyQualifiedErrorId = [string]$_.FullyQualifiedErrorId; position = [string]$_.InvocationInfo.PositionMessage; scriptStackTrace = [string]$_.ScriptStackTrace }
    } finally {
        if ($null -ne $context) {
            try { Drain-P914Output $context $evidenceDirectory } catch { }
            if ($null -ne $context.sampler) {
                try {
                    $context.process.Refresh()
                    if (-not $context.process.HasExited) {
                        try { $null = Invoke-P914NaturalClose $context $manifest $evidenceDirectory $ExitTimeoutSeconds; $manifest.processExit = $context.processExit; if ($null -ne $manifest.process) { $manifest.process.closeMethod = [string]$context.processExit.closeMethod } }
                        catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Natural close during cleanup failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
                    }
                    $context.sampler.Dispose(); $manifest.memory.processMetrics = Get-P914SamplerSummary $context $evidenceDirectory
                    if ($null -ne $manifest.cleanup) { $manifest.cleanup = Get-P914RuntimeCleanup $context $evidenceDirectory }
                } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Final process evidence collection failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
            }
            $manifest.route.routeEvents = Get-P914RouteSummary $context
            $manifest.route.lifecycleEvents = Get-P914LifecycleSummary $context
            $manifest.route.completedAtUtc = Get-P914Utc
            try { Save-P914RouteManifest $manifest $evidenceDirectory } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Route evidence serialization failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc } } }
        }
        $manifest.failure = $failure
        $manifest.status = if ($completed -and $null -eq $failure -and $null -ne $manifest.processExit -and [bool]$manifest.processExit.normalExit -and [int]$manifest.processExit.exitCode -eq 0) { 'evidence-collected' } else { 'failed' }
        $manifest.completedAtUtc = Get-P914Utc
        try { Write-P914Json $manifestPath $manifest } catch { }
        $validatorPath = Join-Path $PSScriptRoot 'validate_packaged_next_p9_15_schedule_locale_theme.py'
        $validationLog = Join-Path (Join-Path $evidenceDirectory 'logs') 'validator-output.txt'
        try {
            $validationLines = @(& python $validatorPath --evidence-dir $evidenceDirectory 2>&1 | ForEach-Object { [string]$_ }); $validationExit = $LASTEXITCODE
            [IO.File]::WriteAllLines($validationLog, $validationLines, [Text.UTF8Encoding]::new($false))
            $jsonLine = @($validationLines | Where-Object { $_.TrimStart().StartsWith('{') } | Select-Object -Last 1)
            $validationResult = if ($jsonLine.Count -eq 1) { $jsonLine[0] | ConvertFrom-Json -ErrorAction Stop } else { $null }
            $manifest.validation = [ordered]@{ status = if ($validationExit -eq 0) { 'passed' } else { 'failed' }; exitCode = $validationExit; validatorPath = [IO.Path]::GetFullPath($validatorPath); validatorSha256 = Get-P914FileSha256 $validatorPath; result = $validationResult; logPath = 'logs/validator-output.txt'; logSha256 = Get-P914FileSha256 $validationLog }
        } catch { $manifest.validation = [ordered]@{ status = 'failed'; error = $_.Exception.Message } }
        $manifest.status = if ([string]$manifest.status -ceq 'evidence-collected' -and [string]$manifest.validation.status -ceq 'passed') { 'passed' } else { 'failed' }
        $manifest.completedAtUtc = Get-P914Utc
        try { Write-P914Json $manifestPath $manifest } catch { }
    }
    if ([string]$manifest.status -cne 'passed') { [Console]::Error.WriteLine("P9-15 packaged Schedule locale/theme lifecycle failed. Evidence preserved at $evidenceDirectory."); exit 1 }
    Write-Output "P9-15 packaged Schedule locale/theme lifecycle validated: $evidenceDirectory"
}

Invoke-P915PackagedLifecycle
