param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [string]$BuildReceiptPath = '',
    [string]$EvidenceRoot = 'artifacts/phase9/packaged-next-p9-19-classes-lifecycle',
    [ValidateRange(30,300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(10,180)][int]$ExitTimeoutSeconds = 60
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$scriptRepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ($RepositoryRoot -ine $scriptRepositoryRoot) { throw "P9-19 repository root does not match the runner's checkout: $RepositoryRoot" }
. (Join-Path $RepositoryRoot 'scripts/phase9/p9_14_packaged_lifecycle_common.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing

$TaskId = 'P9_19_PACKAGED_NEXT_CLASSES_NAVIGATION'
$Schema = 'classmngr-phase9-p9-19-packaged-classes-lifecycle-v1'
$BuildSchema = 'classmngr-phase9-p9-19-exact-release-build-v1'
$FixtureSchema = 'classmngr-phase9-p9-19-large-startup-fixture-v1'
$WorkingSetTargetBytes = [Int64]268435456
$WorkingSetCeilingBytes = [Int64]471859200
$OperationPeakAdvisoryBytes = [Int64]536870912
$ProductRoots = @('CMakeLists.txt','CMakePresets.json','cmake','docs/qt-rewrite/phase-4-resource-inventory.json','licenses','resources','scripts/phase4','src','tests','third_party')
$HarnessPaths = @(
    'scripts/phase9/p9_14_packaged_lifecycle_common.ps1',
    'scripts/phase9/p9_19_build_current_source_release.ps1',
    'scripts/phase9/prepare_p9_19_large_startup_fixture.py',
    'scripts/phase9/run_packaged_next_p9_19_classes_lifecycle.ps1',
    'scripts/phase9/validate_packaged_next_p9_19_classes_lifecycle.py',
    'scripts/phase9/test_prepare_p9_19_large_startup_fixture.py',
    'scripts/phase9/test_validate_packaged_next_p9_19_classes_lifecycle.py'
)
$BaseWorkspaceSha256 = '85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9'
$SeedCanonicalSha256 = '1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3'
$BaseWorkspacePath = Join-Path $RepositoryRoot 'artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1/workspace-snapshots/master-blank.tps'
$SeedSqlPath = Join-Path $RepositoryRoot 'tests/fixtures/workspaces/large_startup.sql'
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
if ($env:OS -cne 'Windows_NT') { throw 'P9-19 packaged UIA lifecycle is supported only on local Windows.' }

function Get-P919HarnessInventory {
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($relative in $HarnessPaths) {
        $path = Join-Path $RepositoryRoot $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required P9-19 harness source is missing: $relative" }
        $rows.Add([ordered]@{ path = $relative; sizeBytes = [Int64](Get-Item -LiteralPath $path).Length; sha256 = Get-P914FileSha256 $path })
    }
    $files = @($rows.ToArray())
    [ordered]@{ fileCount = $files.Count; sha256 = Get-P914InventoryFingerprint $files; files = $files }
}
function Assert-P919BuildReceipt([string]$ReceiptPath, [string]$Directory) {
    if (-not (Test-Path -LiteralPath $ReceiptPath -PathType Leaf)) { throw "P9-19 exact-source Release receipt is missing: $ReceiptPath" }
    $receipt = Read-P914Json $ReceiptPath
    if ([string]$receipt.schema -cne $BuildSchema -or [string]$receipt.taskId -cne $TaskId -or [string]$receipt.status -cne 'succeeded') { throw 'Build receipt is not a successful P9-19 exact-source Release build.' }
    if ([string]$receipt.configuration -cne 'Release' -or [string]$receipt.platform -cne 'x64' -or [string]$receipt.generator -cne 'Visual Studio 17 2022' -or [string]$receipt.generatorPlatform -cne 'x64' -or [string]$receipt.generatorToolset -cne 'v143' -or [string]$receipt.windowsSdkVersion -cne '10.0.26100.0' -or [string]$receipt.compiler.fileVersion -cne '19.44.35230.0' -or [string]$receipt.compiler.toolsetVersion -cne '14.44.35207') { throw 'P9-19 build receipt does not prove the pinned VS2022/v143 x64 Release toolchain.' }
    if ([int]$receipt.build.exitCode -ne 0 -or [int]$receipt.focusedTestBuild.exitCode -ne 0 -or [int]$receipt.focusedTests.exitCode -ne 0) { throw 'P9-19 build receipt contains a failed Release build or focused test gate.' }
    $helper = Join-Path $RepositoryRoot 'scripts/phase9/p9_19_build_current_source_release.ps1'
    if ((Get-P914FileSha256 $helper) -cne [string]$receipt.buildHelperSha256) { throw 'P9-19 build helper changed after its exact-source receipt.' }
    $exe = [IO.Path]::GetFullPath([string]$receipt.releaseExecutablePath)
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf) -or (Get-P914FileSha256 $exe) -cne [string]$receipt.releaseExecutableSha256 -or (Get-P914PeMachine $exe) -ne 0x8664) { throw 'P9-19 release executable differs from its x64 receipt.' }
    $source = Get-P914SourceInventory $RepositoryRoot @($receipt.sourceIncludedRoots)
    if ([string]$source.sha256 -cne [string]$receipt.sourceSha256 -or [int]$source.fileCount -ne [int]$receipt.sourceFileCount) { throw 'Current product source differs from the exact-source P9-19 Release build.' }
    $buildRunRoot = [IO.Path]::GetFullPath((Split-Path -Parent $ReceiptPath)).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
    $buildSourceRoot = [IO.Path]::GetFullPath([string]$receipt.buildSourceRoot)
    $buildSourceInventoryPath = [IO.Path]::GetFullPath([string]$receipt.buildSourceInventoryPath)
    if (-not $buildSourceRoot.StartsWith($buildRunRoot,[StringComparison]::OrdinalIgnoreCase) -or -not $buildSourceInventoryPath.StartsWith($buildRunRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Canonical build-source snapshot escaped its exact-source build evidence directory.' }
    if (-not (Test-Path -LiteralPath $buildSourceRoot -PathType Container) -or -not (Test-Path -LiteralPath $buildSourceInventoryPath -PathType Leaf) -or (Get-P914FileSha256 $buildSourceInventoryPath) -cne [string]$receipt.buildSourceInventoryFileSha256) { throw 'Canonical build-source snapshot or its file inventory is missing or changed.' }
    $buildSource = Get-P914SourceInventory $buildSourceRoot @($receipt.sourceIncludedRoots)
    $recordedBuildSource = Read-P914Json $buildSourceInventoryPath
    if ([string]$buildSource.sha256 -cne [string]$receipt.buildSourceSha256 -or [int]$buildSource.fileCount -ne [int]$receipt.buildSourceFileCount -or [string]$recordedBuildSource.sha256 -cne [string]$buildSource.sha256 -or [int]$recordedBuildSource.fileCount -ne [int]$buildSource.fileCount -or (Get-P914InventoryFingerprint @($recordedBuildSource.files)) -cne [string]$buildSource.sha256) { throw 'Canonical build-source files differ from the exact-source P9-19 Release receipt.' }
    $harness = Get-P919HarnessInventory
    if ([string]$harness.sha256 -cne [string]$receipt.harnessSourceSha256 -or [int]$harness.fileCount -ne [int]$receipt.harnessSourceFileCount) { throw 'P9-19 harness source differs from the exact-source build receipt.' }
    $generated = $receipt.generatedResources
    foreach ($path in @([string]$generated.assetsRoot,[string]$generated.translationsRoot,[string]$generated.catalogPath)) { if (-not (Test-Path -LiteralPath $path)) { throw "P9-19 generated build output is missing: $path" } }
    $assets = Get-P914CanonicalInventory ([string]$generated.assetsRoot)
    $translations = Get-P914CanonicalInventory ([string]$generated.translationsRoot)
    if ([string]$assets.sha256 -cne [string]$generated.assetsSha256 -or [string]$translations.sha256 -cne [string]$generated.translationsSha256 -or (Get-P914FileSha256 ([string]$generated.catalogPath)) -cne [string]$generated.catalogSha256) { throw 'P9-19 generated resources differ from the exact-source build receipt.' }
    [ordered]@{ path = [IO.Path]::GetFullPath($ReceiptPath); sha256 = Get-P914FileSha256 $ReceiptPath; schema = [string]$receipt.schema; releaseExecutablePath = $exe; releaseExecutableSha256 = [string]$receipt.releaseExecutableSha256; sourceSha256 = [string]$source.sha256; harnessSha256 = [string]$harness.sha256; configuration = [string]$receipt.configuration; platform = [string]$receipt.platform; compilerVersion = [string]$receipt.compiler.fileVersion; buildHelperSha256 = [string]$receipt.buildHelperSha256; focusedTests = @($receipt.focusedTests.testNames) }
}
function Stage-P919Package([object]$BuildProof, [string]$Directory) {
    $receipt = Read-P914Json ([string]$BuildProof.path)
    $packageRoot = Join-Path (Join-Path $Directory 'package') 'bin'
    $applicationPath = Join-Path $packageRoot 'ClassMngrNext.exe'
    Copy-Item -LiteralPath ([string]$receipt.releaseExecutablePath) -Destination $applicationPath
    $resourceRoot = Join-Path $packageRoot 'resources'; $assetRoot = Join-Path $resourceRoot 'assets'
    [void][IO.Directory]::CreateDirectory($assetRoot)
    foreach ($item in @(Get-ChildItem -LiteralPath ([string]$receipt.generatedResources.assetsRoot) -Force)) { Copy-Item -LiteralPath $item.FullName -Destination $assetRoot -Recurse }
    $translationRoot = Join-Path $assetRoot 'translations'
    [void][IO.Directory]::CreateDirectory($translationRoot)
    foreach ($item in @(Get-ChildItem -LiteralPath ([string]$receipt.generatedResources.translationsRoot) -Force)) { Copy-Item -LiteralPath $item.FullName -Destination $translationRoot -Recurse }
    Copy-Item -LiteralPath ([string]$receipt.generatedResources.catalogPath) -Destination (Join-Path $resourceRoot 'resource-catalog.json')
    foreach ($license in @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'licenses/fonts') -Filter 'LICENSE.txt' -File -Recurse)) {
        $relative = $license.FullName.Substring((Join-Path $RepositoryRoot 'licenses').TrimEnd('\').Length + 1)
        $destination = Join-Path (Join-Path $packageRoot 'licenses') $relative
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
        Copy-Item -LiteralPath $license.FullName -Destination $destination
    }
    $qtBin = [IO.Path]::GetFullPath((Join-Path ([string]$receipt.qt6Directory) '../../../bin'))
    $windeployqt = Join-Path $qtBin 'windeployqt.exe'
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) { throw "Cache-bound Qt windeployqt is missing: $windeployqt" }
    $deployLog = Join-Path (Join-Path $Directory 'logs') 'windeployqt.log'
    $oldPath = [Environment]::GetEnvironmentVariable('PATH','Process'); $saved = $ErrorActionPreference
    try {
        [Environment]::SetEnvironmentVariable('PATH', "$qtBin;$oldPath", 'Process'); $ErrorActionPreference = 'Continue'
        $lines = @(& $windeployqt --qmldir (Join-Path $RepositoryRoot 'src/features/calendar/ui/qml') --skip-plugin-types qmltooling --include-plugins qoffscreen,qsqlite --exclude-plugins qsqlibase,qsqlmimer,qsqlodbc,qsqlpsql --translations en,ko --compiler-runtime --dir $packageRoot $applicationPath 2>&1 | ForEach-Object { [string]$_ })
        $exitCode = [int]$LASTEXITCODE
    } finally { [Environment]::SetEnvironmentVariable('PATH', $oldPath, 'Process'); $ErrorActionPreference = $saved }
    [IO.File]::WriteAllLines($deployLog, $lines, [Text.UTF8Encoding]::new($false))
    if ($exitCode -ne 0) { throw "Cache-bound windeployqt failed ($exitCode)." }
    foreach ($relative in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Sql.dll','platforms/qwindows.dll','sqldrivers/qsqlite.dll','resources/resource-catalog.json')) { if (-not (Test-Path -LiteralPath (Join-Path $packageRoot $relative) -PathType Leaf)) { throw "P9-19 package lacks $relative" } }
    $inventory = Get-P914CanonicalInventory $packageRoot; $inventoryPath = Join-Path $Directory 'package-files.json'; Write-P914Json $inventoryPath $inventory
    [ordered]@{ packageRoot = [IO.Path]::GetFullPath($packageRoot); packageSha256 = [string]$inventory.sha256; packageFileCount = [int]$inventory.fileCount; packageInventoryPath = 'package-files.json'; packageInventoryFileSha256 = Get-P914FileSha256 $inventoryPath; applicationPath = [IO.Path]::GetFullPath($applicationPath); applicationSha256 = Get-P914FileSha256 $applicationPath; architecture = 'x64'; windeployqtPath = [IO.Path]::GetFullPath($windeployqt); windeployqtSha256 = Get-P914FileSha256 $windeployqt; deployLogPath = 'logs/windeployqt.log'; deployLogSha256 = Get-P914FileSha256 $deployLog; deployExitCode = $exitCode }
}
function Invoke-P919Python([string]$ScriptPath, [string[]]$Arguments, [string]$LogPath) {
    $lines = @(& python $ScriptPath @Arguments 2>&1 | ForEach-Object { [string]$_ }); $code = [int]$LASTEXITCODE
    [IO.File]::WriteAllLines($LogPath, $lines, [Text.UTF8Encoding]::new($false))
    if ($code -ne 0) { throw "P9-19 Python helper failed with exit code $code. See $LogPath" }
    $jsonLines = @($lines | Where-Object { $_.TrimStart().StartsWith('{') } | Select-Object -Last 1)
    if ($jsonLines.Count -ne 1) { throw "P9-19 Python helper emitted no JSON summary. See $LogPath" }
    [ordered]@{ result = ($jsonLines[0] | ConvertFrom-Json -ErrorAction Stop); logPath = [IO.Path]::GetFullPath($LogPath); logSha256 = Get-P914FileSha256 $LogPath }
}
function Get-P919RuntimeInventory([string]$Directory, [string]$Phase) {
    $records = [ordered]@{}
    foreach ($name in @('settings','local-app-data','app-data','temp')) {
        $root = Join-Path (Join-Path $Directory 'runtime') $name
        $inventory = Get-P914CanonicalInventory $root
        $relative = "runtime/$name-$Phase-inventory.json"; $path = Join-Path $Directory $relative; Write-P914Json $path $inventory
        $records[$name] = [ordered]@{ root = [IO.Path]::GetFullPath($root); relativePath = $relative; inventoryFileSha256 = Get-P914FileSha256 $path; fileCount = [int]$inventory.fileCount; sha256 = [string]$inventory.sha256; files = @($inventory.files) }
    }
    $records
}
function Initialize-P919Settings([string]$Directory) {
    $root = Join-Path (Join-Path $Directory 'runtime') 'settings'
    $profile = Join-Path (Join-Path $root 'PaperCloud') 'ClassMngr.ini'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $profile))
    $text = "[options]`r`ntheme=1`r`nlanguage=1`r`n"
    [IO.File]::WriteAllText($profile, $text, [Text.UTF8Encoding]::new($false))
    $inventory = Get-P914CanonicalInventory $root
    if ([int]$inventory.fileCount -ne 1 -or [IO.File]::ReadAllText($profile,[Text.Encoding]::UTF8) -cne $text) { throw 'Fresh P9-19 English/Light settings profile does not match its contract.' }
    $path = Join-Path (Join-Path $Directory 'runtime') 'settings-before-launch-inventory.json'; Write-P914Json $path $inventory
    [ordered]@{ root = [IO.Path]::GetFullPath($root); profileRelativePath = 'runtime/settings/PaperCloud/ClassMngr.ini'; profileSha256 = Get-P914FileSha256 $profile; inventoryPath = 'runtime/settings-before-launch-inventory.json'; inventoryFileSha256 = Get-P914FileSha256 $path; inventorySha256 = [string]$inventory.sha256 }
}
function Find-P919Suffix([object]$Context, [string]$Suffix, [string]$ControlType = '') {
    $root = Get-P914MainRoot $Context
    if ($null -eq $root) { return $null }
    $found = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    $condition = if ($ControlType) { [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty,[System.Windows.Automation.ControlType]::$($ControlType.Replace('ControlType.',''))) } else { [System.Windows.Automation.Condition]::TrueCondition }
    foreach ($element in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants,$condition)) {
        $current = $element.Current; $id = [string]$current.AutomationId
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { continue }
        if ($id -ceq $Suffix -or $id.EndsWith('.' + $Suffix,[StringComparison]::Ordinal)) {
            if ($ControlType -and [string]$current.ControlType.ProgrammaticName -cne $ControlType) { continue }
            $found.Add($element)
        }
    }
    if ($found.Count -gt 1) { throw "P9-19 UIA suffix '$Suffix' resolved to multiple controls." }
    if ($found.Count -eq 0) { return $null }
    $rootKey = Get-P914RuntimeKey $root
    $Context.ownerHandles[(Get-P914RuntimeKey $found[0])] = [Int64]$Context.ownerHandles[$rootKey]
    return $found[0]
}
function Wait-P919Suffix([object]$Context, [string]$EvidenceDirectory, [string]$Suffix, [string]$ControlType, [int]$TimeoutSeconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured app exited while waiting for P9-19 UIA target $Suffix." }
        $element = Find-P919Suffix $Context $Suffix $ControlType
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for P9-19 UIA target $Suffix."
}
function Get-P919ClassItems([object]$Context, [System.Windows.Automation.AutomationElement]$ClassList) {
    $condition = [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty,[System.Windows.Automation.ControlType]::ListItem)
    $elements = $ClassList.FindAll([System.Windows.Automation.TreeScope]::Descendants,$condition)
    $listKey = Get-P914RuntimeKey $ClassList; $owner = [Int64]$Context.ownerHandles[$listKey]
    $items = [Collections.Generic.List[object]]::new(); $targets = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($element in $elements) {
        $current = $element.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.ListItem') { throw 'P9-19 class ListItem is outside the measured PID or exact ControlType.' }
        $Context.ownerHandles[(Get-P914RuntimeKey $element)] = $owner
        $selected = $false
        try { $pattern = [System.Windows.Automation.SelectionItemPattern]$element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern); $selected = [bool]$pattern.Current.IsSelected } catch { }
        $items.Add([ordered]@{ element = $element; name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; processId = [Int64]$current.ProcessId; isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen; isSelected = $selected; ownerWindowHandle = $owner; ownerWindowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$owner) })
        $targets.Add($element)
    }
    $scroll = $null
    try { $scroll = [System.Windows.Automation.ScrollPattern]$ClassList.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern) } catch { }
    [ordered]@{ elements = $targets.ToArray(); items = $items.ToArray(); scrollPattern = $scroll }
}
function Ensure-P919ClassItemVisible([object]$Context, [System.Windows.Automation.AutomationElement]$ClassList, [System.Windows.Automation.AutomationElement]$Item, [string]$ExpectedName) {
    for ($attempt = 0; $attempt -lt 24; $attempt++) {
        $current = $Item.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.Name -cne $ExpectedName -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.ListItem') { throw "P9-19 class item identity changed before selection: $ExpectedName" }
        if (-not [bool]$current.IsOffscreen -and $current.BoundingRectangle.Width -gt 0 -and $current.BoundingRectangle.Height -gt 0) { return }
        $scrollItem = $null
        try { $scrollItem = [System.Windows.Automation.ScrollItemPattern]$Item.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern) } catch { }
        if ($null -ne $scrollItem) { $scrollItem.ScrollIntoView() }
        else {
            $scroll = $null
            try { $scroll = [System.Windows.Automation.ScrollPattern]$ClassList.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern) } catch { }
            if ($null -eq $scroll -or -not [bool]$scroll.Current.VerticallyScrollable) { throw "Offscreen class item has no bounded UIA scroll method: $ExpectedName" }
            $scroll.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount,[System.Windows.Automation.ScrollAmount]::LargeIncrement)
        }
        Start-Sleep -Milliseconds 100
    }
    throw "P9-19 class item stayed offscreen after bounded scroll attempts: $ExpectedName"
}
function Select-P919ClassSweep([object]$Context, [object]$Manifest, [string]$Directory, [string]$SweepId, [string]$Phase, [int]$CycleNumber, [int]$TimeoutSeconds) {
    $sweepStartedAtUtc = Get-P914Utc
    $classList = Wait-P919Suffix $Context $Directory 'classesNavigatorClassList' 'ControlType.List' $TimeoutSeconds
    $items = Get-P919ClassItems $Context $classList
    if (@($items.items).Count -ne 96) { throw "P9-19 Classes List exposed $(@($items.items).Count) rows; expected 96." }
    $expectedNames = [Collections.Generic.List[string]]::new()
    for ($row = 0; $row -lt 96; $row++) { $expectedNames.Add(('Heavy Fixture Class {0:D3}' -f ($row + 1))) }
    $actualNames = @($items.items | ForEach-Object { [string]$_.name })
    $expectedNameSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in $expectedNames) { [void]$expectedNameSet.Add($name) }
    if (@($actualNames | Select-Object -Unique).Count -ne 96 -or $actualNames.Count -ne 96 -or @($actualNames | Where-Object { -not $expectedNameSet.Contains([string]$_) }).Count -ne 0) { throw 'P9-19 UIA Classes list does not expose the exact unique 96 fixture class names.' }
    if ([Int64]$items.items[0].ownerWindowHandle -ne [Int64]$Context.mainWindowHandle -or [Int64]$items.items[0].ownerWindowProcessId -ne [Int64]$Context.process.Id) { throw 'P9-19 class ListItems are not bound to the measured PID/HWND.' }
    $readbackPath = Join-Path (Join-Path $Directory 'output') ($SweepId + '.jsonl')
    [IO.File]::WriteAllText($readbackPath,'',[Text.UTF8Encoding]::new($false))
    $startAction = [int]$Manifest.route.actions.Count
    $arrivalBefore = [Int64]$Context.arrival
    Set-P914SamplerContext $Context 'operation' ($Phase + '-select-all-96-classes')
    for ($row = 0; $row -lt 96; $row++) {
        $expectedName = [string]$actualNames[$row]
        $classIdMatch = [regex]::Match($expectedName, '^Heavy Fixture Class (?<classId>[0-9]{3})$')
        if (-not $classIdMatch.Success) { throw "P9-19 class accessible name cannot be bound to a fixture class id: $expectedName" }
        $classId = [int]$classIdMatch.Groups['classId'].Value
        $item = $items.elements[$row]
        Ensure-P919ClassItemVisible $Context $classList $item $expectedName
        $current = $item.Current
        $target = Get-P914ControlSnapshot $Context $item
        if ([Int64]$target.ownerWindowHandle -ne [Int64]$Context.mainWindowHandle -or [Int64]$target.ownerWindowProcessId -ne [Int64]$Context.process.Id -or -not [bool]$target.isEnabled -or [bool]$target.isOffscreen) { throw "P9-19 UIA class target failed exact PID/HWND, enabled, or visible checks: $expectedName" }
        $selectionMethod = 'SelectionItemPattern.Select'
        $pattern = $null
        try { $pattern = [System.Windows.Automation.SelectionItemPattern]$item.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern) } catch { throw "P9-19 class ListItem lacks SelectionItemPattern: $expectedName" }
        $action = [ordered]@{ actionKind = 'select-fixture-class'; method = $selectionMethod; sweepId = $SweepId; cycleNumber = if ($CycleNumber -gt 0) { $CycleNumber } else { $null }; classId = $classId; className = $expectedName; target = [ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; processId = [Int64]$current.ProcessId; ownerWindowHandle = [Int64]$target.ownerWindowHandle; ownerWindowProcessId = [Int64]$target.ownerWindowProcessId }; issuedAtUtc = Get-P914Utc }
        $pattern.Select()
        $watch = [Diagnostics.Stopwatch]::StartNew(); $selected = $false
        while ($watch.Elapsed.TotalSeconds -lt 2.0) { Drain-P914Output $Context $Directory; $selected = [bool]$pattern.Current.IsSelected; if ($selected) { break }; Start-Sleep -Milliseconds 50 }
        if (-not $selected) {
            $selectionMethod = 'SelectionItemPattern.Select plus UIA-bounded ListItem click fallback'
            $rect = $item.Current.BoundingRectangle
            $listRect = $classList.Current.BoundingRectangle
            if ([bool]$item.Current.IsOffscreen -or $rect.Width -le 0 -or $rect.Height -le 0 -or $rect.X -lt $listRect.X -or $rect.Y -lt $listRect.Y -or ($rect.X + $rect.Width) -gt ($listRect.X + $listRect.Width) -or ($rect.Y + $rect.Height) -gt ($listRect.Y + $listRect.Height)) { throw "P9-19 selection fallback cannot be bounded inside the exact class list: $expectedName" }
            $x = [int][Math]::Round($rect.X + $rect.Width / 2); $y = [int][Math]::Round($rect.Y + $rect.Height / 2)
            [P914Native]::ClickScreenPoint([IntPtr]$Context.mainWindowHandle,[uint32]$Context.process.Id,$x,$y)
            $action.pointerAction = [ordered]@{ kind = 'UIA-derived ListItem center click clipped to the visible Classes List'; screenX = $x; screenY = $y; windowHandle = [Int64]$Context.mainWindowHandle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle); targetProcessId = [Int64]$Context.process.Id; classListAutomationId = [string]$classList.Current.AutomationId }
            $action.method = $selectionMethod
            $watch.Restart()
            while ($watch.Elapsed.TotalSeconds -lt 2.0) { Drain-P914Output $Context $Directory; $selected = [bool]$pattern.Current.IsSelected; if ($selected) { break }; Start-Sleep -Milliseconds 50 }
        }
        if (-not $selected) { throw "P9-19 SelectionItemPattern readback did not confirm class id $($row + 1)." }
        $action.selectedListItemName = [string]$item.Current.Name; $action.selectionItemPatternIsSelected = $selected; $action.readbackAtUtc = Get-P914Utc
        Add-P914JsonLine $readbackPath $action
    }
    $checkpointWatch = [Diagnostics.Stopwatch]::StartNew(); $checkpoint = $null
    while ($checkpointWatch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $Directory
        $matches = @($Context.lifecycleEvents | Where-Object { [Int64]$_.arrivalIndex -gt $arrivalBefore -and [string]$_.record.event -ceq 'page-action-checkpoint' -and [string]$_.record.page_key -ceq 'classes' -and [string]$_.record.checkpoint -ceq 'p9-19-class-selection-sweep' -and [string]$_.record.outcome -ceq 'completed' })
        if ($matches.Count -gt 0) { $checkpoint = $matches[$matches.Count - 1]; break }
        $Context.process.Refresh(); if ($Context.process.HasExited) { throw 'Measured app exited before the P9-19 class sweep checkpoint was observed.' }
        Start-Sleep -Milliseconds 50
    }
    if ($null -eq $checkpoint) { throw "P9-19 app-side checkpoint did not complete for sweep $SweepId." }
    $details = $checkpoint.record.details
    $checkpointContract = ([string]$details.schema -ceq 'classmngr-phase9-p9-19-class-selection-sweep-v1' -and [int]$details.class_count -eq 96 -and [int]$details.visible_class_count -eq 96 -and [int]$details.selected_unique_class_count -eq 96 -and [int]$details.selection_observation_count -eq 96 -and [int]$details.selected_class_editor_count_minimum -eq 1 -and [int]$details.selected_class_editor_count_maximum -eq 1 -and [int]$details.non_roster_widget_count_maximum -lt 250 -and [int]$details.class_id_child_widget_count_maximum -eq 0 -and [bool]$details.selected_class_editor_stable_within_sweep -and [bool]$details.selected_class_editor_stable_across_sweeps)
    if (-not $checkpointContract) { throw "P9-19 app-side class/editor checkpoint failed its contract for sweep $SweepId." }
    $file = Get-Item -LiteralPath $readbackPath
    $sweep = [ordered]@{ sweepId = $SweepId; phase = $Phase; cycleNumber = if ($CycleNumber -gt 0) { $CycleNumber } else { $null }; startedAtUtc = $sweepStartedAtUtc; completedAtUtc = Get-P914Utc; classCount = 96; listItemNameHash = Get-P914StringSha256 ($actualNames -join "`n"); selectionActionCount = 96; selectionReadbackPath = [IO.Path]::GetRelativePath($Directory,$readbackPath).Replace('\','/'); selectionReadbackSha256 = Get-P914FileSha256 $readbackPath; selectionReadbackBytes = [Int64]$file.Length; appCheckpointArrivalIndex = [Int64]$checkpoint.arrivalIndex; appCheckpointObservedAtUtc = [string]$checkpoint.observedAtUtc; appCheckpointDetails = $details; allSelectionReadbacksConfirmed = $true }
    $Manifest.route.selectionSweeps.Add($sweep)
    return $sweep
}
function Get-P919LifecycleSince([object]$Context, [Int64]$AfterArrival) {
    @($Context.lifecycleEvents | Where-Object { [Int64]$_.arrivalIndex -gt $AfterArrival } | ForEach-Object {
        $record = $_.record; $names = @($record.PSObject.Properties | ForEach-Object { [string]$_.Name })
        [ordered]@{
            arrivalIndex = [Int64]$_.arrivalIndex
            event = if ($names -contains 'event') { [string]$record.event } else { $null }
            pageKey = if ($names -contains 'page_key') { [string]$record.page_key } else { $null }
            state = if ($names -contains 'state') { [string]$record.state } else { $null }
            pageInstanceAvailable = if ($names -contains 'page_instance_available') { [bool]$record.page_instance_available } else { $null }
            rawLine = [string]$_.line
        }
    })
}

function Write-P919Manifest([string]$Path, [object]$Value) {
    $json = ConvertTo-Json -InputObject $Value -Depth 55
    [IO.File]::WriteAllText($Path, $json, [Text.UTF8Encoding]::new($false))
}
function New-P919Confirmation([object]$Context, [object]$Manifest, [string]$Directory, [string]$WindowId, [string]$PageId, [string]$PageTitle, [object[]]$LifecycleEvents) {
    $root = Get-P914MainRoot $Context
    if ($null -eq $root -or [Int64]$Context.mainWindowHandle -le 0 -or [Int64][P914Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle) -ne [Int64]$Context.process.Id) { throw "P9-19 state $WindowId is not bound to the measured main PID/HWND." }
    $screenshot = Capture-P914Screenshot $Context $Manifest $Directory ('state-' + $WindowId) $root
    [ordered]@{ state = $PageId; pageId = $PageId; pageTitle = $PageTitle; processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle; confirmedAtUtc = Get-P914Utc; screenshot = $screenshot; lifecycleEvents = @($LifecycleEvents) }
}
function Invoke-P919MemoryWindow([object]$Context, [object]$Manifest, [string]$Name, [string]$Phase, [int]$CycleNumber, [object]$Confirmation) {
    $Context.process.Refresh(); if ($Context.process.HasExited) { throw "Measured process exited before settled P9-19 memory window $Name." }
    $confirmedAt = [DateTimeOffset]::Parse([string]$Confirmation.confirmedAtUtc).ToUniversalTime()
    Start-Sleep -Seconds 2
    $samplingStarted = [DateTimeOffset]::UtcNow
    if (($samplingStarted - $confirmedAt).TotalSeconds -lt 1.9) { throw "P9-19 state settle interval was shorter than two seconds before $Name." }
    Set-P914SamplerContext $Context 'normal-use' ($Phase + '-' + $Name)
    $watch = [Diagnostics.Stopwatch]::StartNew(); $samples = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt 10; $index++) {
        Start-Sleep -Milliseconds 100; $errors = [Collections.Generic.List[string]]::new()
        $sample = [ordered]@{ sampleIndex = $index; capturedAtUtc = Get-P914Utc; elapsedMilliseconds = [int]$watch.ElapsedMilliseconds; processId = [Int64]$Context.process.Id; processState = 'running'; workingSetBytes = $null; privateBytes = $null; peakWorkingSetBytes = $null; handles = $null; threadCount = $null; metricErrors = @() }
        try { $Context.process.Refresh(); if ($Context.process.HasExited) { $sample.processState = 'exited'; $errors.Add('measured process exited during settled P9-19 memory sampling') } else { $sample.workingSetBytes = [Int64]$Context.process.WorkingSet64; $sample.privateBytes = [Int64]$Context.process.PrivateMemorySize64; $sample.peakWorkingSetBytes = [Int64]$Context.process.PeakWorkingSet64; $sample.handles = [Int64]$Context.process.HandleCount; $sample.threadCount = [Int64]$Context.process.Threads.Count } } catch { $errors.Add($_.Exception.Message) }
        $sample.metricErrors = $errors.ToArray(); $samples.Add($sample)
        if ($sample.processState -eq 'exited') { break }
    }
    $targetMisses = @(); $ceilingMisses = @(); $values = @($samples | Where-Object { $null -ne $_.workingSetBytes } | ForEach-Object { [Int64]$_.workingSetBytes })
    for ($index = 0; $index -lt $samples.Count; $index++) { if ($null -eq $samples[$index].workingSetBytes) { continue }; if ([Int64]$samples[$index].workingSetBytes -ge $WorkingSetTargetBytes) { $targetMisses += $index }; if ([Int64]$samples[$index].workingSetBytes -gt $WorkingSetCeilingBytes) { $ceilingMisses += $index } }
    $window = [ordered]@{ windowId = $Name; state = [string]$Confirmation.state; matchedState = [string]$Confirmation.state; phase = $Phase; cycleNumber = if ($CycleNumber -gt 0) { $CycleNumber } else { $null }; processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle; sampleIntervalMilliseconds = 100; nominalDurationMilliseconds = 1000; settleDelaySeconds = 2; stateConfirmedBeforeSampling = $true; stateConfirmation = $Confirmation; samplingStartedAtUtc = $samplingStarted.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'"); samplingCompletedAtUtc = Get-P914Utc; startedAtUtc = if ($samples.Count -gt 0) { [string]$samples[0].capturedAtUtc } else { Get-P914Utc }; completedAtUtc = Get-P914Utc; sampleCount = [int]$samples.Count; samplingCompleted = ($samples.Count -eq 10); samplingErrorCount = [int](@($samples | Where-Object { @($_.metricErrors).Count -gt 0 }).Count); samplingErrors = @($samples | Where-Object { @($_.metricErrors).Count -gt 0 } | ForEach-Object { $_.metricErrors }); workingSetTargetBytes = $WorkingSetTargetBytes; workingSetTargetExclusive = $true; workingSetCeilingBytes = $WorkingSetCeilingBytes; workingSetCeilingExclusive = $false; workingSetTargetExceededSampleIndexes = $targetMisses; workingSetCeilingExceededSampleIndexes = $ceilingMisses; workingSetTargetMet = ($targetMisses.Count -eq 0); workingSetCeilingPassed = ($ceilingMisses.Count -eq 0); maximumWorkingSetBytes = if ($values.Count -gt 0) { [Int64](($values | Measure-Object -Maximum).Maximum) } else { $null }; samples = $samples.ToArray() }
    $Manifest.memory.windows.Add($window)
    return $window
}
function Invoke-P919Transition([object]$Context, [object]$Manifest, [string]$Directory, [string]$TargetName, [string]$TargetPageId, [string]$SourcePageId, [int]$TimeoutSeconds) {
    $before = [Int64]$Context.arrival
    Set-P914SamplerContext $Context 'operation' ('navigate-' + $TargetPageId)
    $routeEvent = Select-P914Route $Context $Manifest $Directory $TargetName $TargetPageId
    $activated = Wait-P914Lifecycle $Context $Directory $TargetPageId 'activated' $before $TimeoutSeconds
    $suspended = Wait-P914Lifecycle $Context $Directory $SourcePageId 'suspended' $before $TimeoutSeconds
    $events = @(Get-P919LifecycleSince $Context $before)
    $transition = [ordered]@{ fromPageId = $SourcePageId; toPageId = $TargetPageId; targetAccessibleName = $TargetName; routeEvent = $routeEvent; activatedArrivalIndex = [Int64]$activated.arrivalIndex; suspendedArrivalIndex = [Int64]$suspended.arrivalIndex; lifecycleEvents = $events; processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle }
    $Manifest.route.transitions.Add($transition)
    return $transition
}
function Write-P919Snapshot([string]$Phase, [string]$Workspace, [string]$ReceiptPath, [string]$Directory) {
    $helper = Join-Path $PSScriptRoot 'prepare_p9_19_large_startup_fixture.py'
    $path = Join-Path (Join-Path $Directory 'workspace') ($Phase + '-snapshot.json')
    $log = Join-Path (Join-Path $Directory 'logs') ('sqlite-' + $Phase + '-snapshot.log')
    $call = Invoke-P919Python $helper @('snapshot','--phase',$Phase,'--workspace',$Workspace,'--preparation-receipt',$ReceiptPath,'--output',$path) $log
    $snapshot = Read-P914Json $path
    [ordered]@{ relativePath = 'workspace/' + $Phase + '-snapshot.json'; sha256 = Get-P914FileSha256 $path; helperLogPath = 'logs/sqlite-' + $Phase + '-snapshot.log'; helperLogSha256 = [string]$call.logSha256; workspaceSha256 = [string]$snapshot.workspaceSha256; integrityCheck = [string]$snapshot.integrityCheck; identity = $snapshot.identity; phase = $Phase }
}

$evidence = New-P914EvidenceDirectory $RepositoryRoot $EvidenceRoot
$EvidenceDirectory = [string]$evidence.evidenceDirectory
$RunId = [string]$evidence.runId
$manifestPath = Join-Path $EvidenceDirectory 'manifest.json'
$routeManifestPath = Join-Path $EvidenceDirectory 'route-manifest.json'
$manifest = [ordered]@{
    schema = $Schema; taskId = $TaskId; runId = $RunId; status = 'running'; routeMode = 'current-next-classes-navigation-editor-ui-packaged-windows-x64-release-local'
    startedAtUtc = Get-P914Utc; completedAtUtc = $null; repositoryRoot = $RepositoryRoot; evidenceDirectory = $EvidenceDirectory
    packageProvenance = $null; package = $null; settings = $null
    fixture = [ordered]@{ kind = 'fresh-p9-19-large-startup-workspace'; baseWorkspacePath = [IO.Path]::GetFullPath($BaseWorkspacePath); baseWorkspaceSha256Expected = $BaseWorkspaceSha256; baseWorkspaceSha256Observed = $null; baseWorkspacePinVerified = $false; seedSqlPath = [IO.Path]::GetFullPath($SeedSqlPath); seedSqlRawSha256 = $null; seedSqlCanonicalLfSha256Expected = $SeedCanonicalSha256; seedSqlCanonicalLfSha256Observed = $null; preparationReceipt = $null; beforeSnapshot = $null; afterSnapshot = $null }
    runtimeAppData = [ordered]@{ before = $null; after = $null; tempFilesAfterExit = @(); sqliteSidecarsAfterExit = @() }
    process = $null; processExit = $null
    route = [ordered]@{ workflow = 'classes-navigation-and-96-selection-sweeps'; startedAtUtc = Get-P914Utc; actions = [Collections.Generic.List[object]]::new(); transitions = [Collections.Generic.List[object]]::new(); selectionSweeps = [Collections.Generic.List[object]]::new(); routeEvents = @(); lifecycleEvents = @(); screenshots = [Collections.Generic.List[object]]::new(); uiaSnapshots = [Collections.Generic.List[object]]::new(); warmupCount = 1; baselineWindowsPerState = 5; measuredCycleCount = 10; classCount = 96; completedAtUtc = $null }
    memory = [ordered]@{ windows = [Collections.Generic.List[object]]::new(); processMetrics = $null; coldBaselineWindowId = 'cold-baseline'; coldReturnWindowId = 'warmup-return'; plateauBaselineWindowIds = @('plateau-baseline-01','plateau-baseline-02','plateau-baseline-03','plateau-baseline-04','plateau-baseline-05'); thresholds = [ordered]@{ workingSetTargetBytes = $WorkingSetTargetBytes; workingSetTargetExclusive = $true; workingSetCeilingBytes = $WorkingSetCeilingBytes; workingSetCeilingInclusive = $true; operationPeakAdvisoryBytes = $OperationPeakAdvisoryBytes; settledWindowSettleSeconds = 2; settledSamplesPerWindow = 10; settledSampleIntervalMilliseconds = 100; operationSampleIntervalMilliseconds = 50; privateBytesDiagnosticOnly = $true }; analysis = $null; operationPeakWorkingSetBytes = $null }
    failure = $null; validation = $null
}
$context = $null; $failure = $null; $fixtureReceiptPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'preparation-receipt.json'; $workspacePath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'p9-19-large-startup.tps'; $beforeSnapshotPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'before-snapshot.json'

try {
    $manifest.fixture.seedSqlRawSha256 = Get-P914FileSha256 $SeedSqlPath
    $manifest.fixture.baseWorkspaceSha256Observed = if (Test-Path -LiteralPath $BaseWorkspacePath -PathType Leaf) { Get-P914FileSha256 $BaseWorkspacePath } else { $null }
    if ([string]$manifest.fixture.baseWorkspaceSha256Observed -cne $BaseWorkspaceSha256) { throw "Pinned P9-19 master blank input is missing or differs from $BaseWorkspaceSha256; packaged route was not started." }
    $manifest.fixture.baseWorkspacePinVerified = $true
    $helper = Join-Path $PSScriptRoot 'prepare_p9_19_large_startup_fixture.py'
    $canonicalSeedPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'large_startup.canonical-lf.sql'
    $p916ReceiptPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'p9-16-preparation-receipt.json'
    $prepareLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'fixture-preparation.log'
    $prepared = Invoke-P919Python $helper @('prepare','--base-workspace',$BaseWorkspacePath,'--seed-sql',$SeedSqlPath,'--output',$workspacePath,'--receipt',$fixtureReceiptPath,'--p916-receipt',$p916ReceiptPath,'--canonical-seed-sql',$canonicalSeedPath) $prepareLog
    $fixtureReceipt = Read-P914Json $fixtureReceiptPath
    if ([string]$fixtureReceipt.schema -cne $FixtureSchema -or [string]$fixtureReceipt.status -cne 'passed' -or [string]$fixtureReceipt.baseWorkspaceSha256 -cne $BaseWorkspaceSha256 -or [string]$fixtureReceipt.seedSqlCanonicalLfSha256 -cne $SeedCanonicalSha256 -or [int]$fixtureReceipt.identity.classCount -ne 96 -or [int]$fixtureReceipt.identity.classInfoCount -ne 96 -or [int]$fixtureReceipt.identity.scheduleEntryCount -ne 768) { throw 'P9-19 fixture preparation receipt does not prove the exact base/SQL pins and 96/96/768 identity.' }
    $manifest.fixture.seedSqlCanonicalLfSha256Observed = [string]$fixtureReceipt.seedSqlCanonicalLfSha256
    $manifest.fixture.preparationReceipt = [ordered]@{ relativePath = 'workspace/preparation-receipt.json'; sha256 = Get-P914FileSha256 $fixtureReceiptPath; helperLogPath = 'logs/fixture-preparation.log'; helperLogSha256 = [string]$prepared.logSha256; workspaceSha256 = [string]$fixtureReceipt.workspaceSha256; baseWorkspaceSha256 = [string]$fixtureReceipt.baseWorkspaceSha256; seedSqlRawSha256 = [string]$fixtureReceipt.seedSqlRawSha256; seedSqlCanonicalLfSha256 = [string]$fixtureReceipt.seedSqlCanonicalLfSha256; canonicalSeedPath = [string]$fixtureReceipt.canonicalSeedSql; canonicalSeedSha256 = [string]$fixtureReceipt.canonicalSeedSqlSha256; identity = $fixtureReceipt.identity }
    $manifest.fixture.beforeSnapshot = Write-P919Snapshot 'before' $workspacePath $fixtureReceiptPath $EvidenceDirectory
    if ([string]$manifest.fixture.beforeSnapshot.integrityCheck -cne 'ok') { throw 'P9-19 pre-route SQLite integrity/read-only snapshot failed.' }

    if ([string]::IsNullOrWhiteSpace($BuildReceiptPath)) { throw 'P9-19 exact-source Release build receipt path was not supplied; no packaged process was started.' }
    $BuildReceiptPath = [IO.Path]::GetFullPath($BuildReceiptPath)
    $buildProof = Assert-P919BuildReceipt $BuildReceiptPath $EvidenceDirectory
    $manifest.packageProvenance = $buildProof
    $manifest.package = Stage-P919Package $buildProof $EvidenceDirectory
    $manifest.settings = Initialize-P919Settings $EvidenceDirectory
    $manifest.runtimeAppData.before = Get-P919RuntimeInventory $EvidenceDirectory 'before'

    $oldDiagnostics = [Environment]::GetEnvironmentVariable('CLASSMNGR_P9_19_CLASS_SELECTION_DIAGNOSTICS','Process')
    [Environment]::SetEnvironmentVariable('CLASSMNGR_P9_19_CLASS_SELECTION_DIAGNOSTICS','1','Process')
    try { $context = Start-P914App 'staff-directory' $EvidenceDirectory $workspacePath ([string]$manifest.package.applicationPath) }
    finally { [Environment]::SetEnvironmentVariable('CLASSMNGR_P9_19_CLASS_SELECTION_DIAGNOSTICS',$oldDiagnostics,'Process') }
    $context.sampler = [P914ProcessSampler]::new($context.process)
    $manifest.process = [ordered]@{ processId = [Int64]$context.process.Id; processImagePath = [string]$context.processImagePath; processImageSha256 = [string]$context.processImageSha256; architecture = 'x64'; applicationPath = [string]$manifest.package.applicationPath; applicationSha256 = [string]$manifest.package.applicationSha256; startedAtUtc = Get-P914Utc; mainWindowHandle = $null; mainWindowAutomationId = 'QApplication.classMngrNextApplicationShell' }
    $root = Wait-P914MainRoot $context $EvidenceDirectory $ReadinessTimeoutSeconds
    $context.sampler.SetContext('normal-use','pre-route-my-workspace')
    $initialArrival = [Int64]$context.arrival
    $initialActivation = Wait-P914Lifecycle $context $EvidenceDirectory 'my_workspace' 'activated' 0 $ReadinessTimeoutSeconds
    $manifest.process.mainWindowHandle = [Int64]$context.mainWindowHandle
    $startupEvents = Get-P919LifecycleSince $context 0
    $coldConfirmation = New-P919Confirmation $context $manifest $EvidenceDirectory 'cold-baseline' 'my_workspace' 'My Workspace' $startupEvents
    $coldWindow = Invoke-P919MemoryWindow $context $manifest 'cold-baseline' 'cold-baseline' 0 $coldConfirmation
    $manifest.memory.coldBaselineSamples = @($coldWindow.samples | ForEach-Object { [ordered]@{ sampleIndex = [int]$_.sampleIndex; capturedAtUtc = [string]$_.capturedAtUtc; workingSetBytes = [Int64]$_.workingSetBytes; processId = [Int64]$_.processId } })

    $warmEnter = Invoke-P919Transition $context $manifest $EvidenceDirectory 'Classes' 'classes' 'my_workspace' $ReadinessTimeoutSeconds
    $warmSweep = Select-P919ClassSweep $context $manifest $EvidenceDirectory 'class-selections-warmup' 'warmup' 0 $ReadinessTimeoutSeconds
    $warmClassesConfirmation = New-P919Confirmation $context $manifest $EvidenceDirectory 'warmup-classes' 'classes' 'Classes' @($warmEnter.lifecycleEvents)
    $null = Invoke-P919MemoryWindow $context $manifest 'warmup-classes' 'warmup' 0 $warmClassesConfirmation
    $warmReturn = Invoke-P919Transition $context $manifest $EvidenceDirectory 'My Workspace' 'my_workspace' 'classes' $ReadinessTimeoutSeconds
    $warmReturnConfirmation = New-P919Confirmation $context $manifest $EvidenceDirectory 'warmup-return' 'my_workspace' 'My Workspace' @($warmReturn.lifecycleEvents)
    $warmReturnWindow = Invoke-P919MemoryWindow $context $manifest 'warmup-return' 'warmup' 0 $warmReturnConfirmation
    $manifest.memory.firstReturnSamples = @($warmReturnWindow.samples | ForEach-Object { [ordered]@{ sampleIndex = [int]$_.sampleIndex; capturedAtUtc = [string]$_.capturedAtUtc; workingSetBytes = [Int64]$_.workingSetBytes; processId = [Int64]$_.processId } })

    for ($baseline = 1; $baseline -le 5; $baseline++) {
        $name = 'plateau-baseline-{0:D2}' -f $baseline
        $confirmation = New-P919Confirmation $context $manifest $EvidenceDirectory $name 'my_workspace' 'My Workspace' @()
        $null = Invoke-P919MemoryWindow $context $manifest $name 'no-action-baseline' 0 $confirmation
    }
    for ($cycle = 1; $cycle -le 10; $cycle++) {
        $classWindowName = 'cycle-{0:D2}-classes' -f $cycle
        $sweepId = 'class-selections-cycle-{0:D2}' -f $cycle
        $enter = Invoke-P919Transition $context $manifest $EvidenceDirectory 'Classes' 'classes' 'my_workspace' $ReadinessTimeoutSeconds
        $null = Select-P919ClassSweep $context $manifest $EvidenceDirectory $sweepId 'measured-cycle' $cycle $ReadinessTimeoutSeconds
        $classConfirmation = New-P919Confirmation $context $manifest $EvidenceDirectory $classWindowName 'classes' 'Classes' @($enter.lifecycleEvents)
        $null = Invoke-P919MemoryWindow $context $manifest $classWindowName 'measured-cycle-classes' $cycle $classConfirmation
        $returnWindowName = 'cycle-{0:D2}-return' -f $cycle
        $return = Invoke-P919Transition $context $manifest $EvidenceDirectory 'My Workspace' 'my_workspace' 'classes' $ReadinessTimeoutSeconds
        $returnConfirmation = New-P919Confirmation $context $manifest $EvidenceDirectory $returnWindowName 'my_workspace' 'My Workspace' @($return.lifecycleEvents)
        $null = Invoke-P919MemoryWindow $context $manifest $returnWindowName 'measured-cycle-return' $cycle $returnConfirmation
    }
    $windowIndex = @{}
    foreach ($window in $manifest.memory.windows) { $windowIndex[[string]$window.windowId] = $window }
    $manifest.memory.formulaInputs = [ordered]@{
        coldBaselineWindowId = 'cold-baseline'; coldBaselineWorkingSetSamples = @($windowIndex['cold-baseline'].samples | ForEach-Object { [Int64]$_.workingSetBytes })
        firstReturnWindowId = 'warmup-return'; firstReturnWorkingSetSamples = @($windowIndex['warmup-return'].samples | ForEach-Object { [Int64]$_.workingSetBytes })
        plateauBaselineWindowIds = @($manifest.memory.plateauBaselineWindowIds); plateauBaselineWorkingSetSamples = @($manifest.memory.plateauBaselineWindowIds | ForEach-Object { ,@($windowIndex[[string]$_].samples | ForEach-Object { [Int64]$_.workingSetBytes }) })
        measuredReturnWindowIds = @(1..10 | ForEach-Object { 'cycle-{0:D2}-return' -f $_ }); measuredReturnWorkingSetSamples = @(1..10 | ForEach-Object { ,@($windowIndex[('cycle-{0:D2}-return' -f $_)].samples | ForEach-Object { [Int64]$_.workingSetBytes }) })
        source = 'raw ten-sample settled memory windows recorded in manifest.memory.windows'
    }
    Set-P914SamplerContext $context 'normal-use' 'p9-19-route-ready-to-close'
    $manifest.route.completedAtUtc = Get-P914Utc
    $context.process.Refresh()
    if ($context.process.HasExited) { throw 'P9-19 app exited before its natural close action.' }
    $manifest.processExit = Invoke-P914NaturalClose $context $manifest $EvidenceDirectory $ExitTimeoutSeconds
    $manifest.process.exitCode = [int]$manifest.processExit.exitCode
    $manifest.process.normalExit = [bool]$manifest.processExit.normalExit
} catch {
    $failure = [ordered]@{ message = $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = $_.CategoryInfo.Category; fullyQualifiedErrorId = $_.FullyQualifiedErrorId; position = $_.InvocationInfo.PositionMessage; scriptStackTrace = $_.ScriptStackTrace }
} finally {
    if ($null -ne $context) {
        try {
            Drain-P914Output $context $EvidenceDirectory
            $context.process.Refresh()
            if (-not $context.process.HasExited) { $manifest.processExit = Invoke-P914NaturalClose $context $manifest $EvidenceDirectory $ExitTimeoutSeconds }
        } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Natural cleanup close failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        try {
            if ($null -ne $context.sampler) { $manifest.memory.processMetrics = Get-P914SamplerSummary $context $EvidenceDirectory; $manifest.memory.operationPeakWorkingSetBytes = [Int64]$manifest.memory.processMetrics.operationPeakWorkingSetBytes; $context.sampler.Dispose() }
        } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Final process metric collection failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        try {
            $context.process.Refresh()
            if (-not $context.process.HasExited) { throw 'Measured process remained live after natural close timeout.' }
            if ($null -eq $manifest.processExit) { $manifest.processExit = [ordered]@{ observed = $true; normalExit = ($context.process.ExitCode -eq 0); exitCode = [int]$context.process.ExitCode; closeMethod = 'observed-after-cleanup'; forcedTermination = [bool]$context.forcedTermination; exitedAtUtc = Get-P914Utc } }
            $manifest.process.exitCode = [int]$context.process.ExitCode; $manifest.process.normalExit = ([int]$context.process.ExitCode -eq 0)
            $manifest.runtimeAppData.after = Get-P919RuntimeInventory $EvidenceDirectory 'after'
            $tempFiles = @($manifest.runtimeAppData.after.temp.files | ForEach-Object { [string]$_.path })
            $sidecars = @(Get-ChildItem -LiteralPath (Join-Path $EvidenceDirectory 'runtime') -File -Recurse | Where-Object { $_.Name -match '(?:-wal|-shm|-journal)$' } | ForEach-Object { [IO.Path]::GetFullPath($_.FullName) })
            foreach ($suffix in @('-wal','-shm','-journal')) { $sidecar = [string]$workspacePath + $suffix; if (Test-Path -LiteralPath $sidecar) { $sidecars += [IO.Path]::GetFullPath($sidecar) } }
            $manifest.runtimeAppData.tempFilesAfterExit = $tempFiles; $manifest.runtimeAppData.sqliteSidecarsAfterExit = $sidecars
            $manifest.runtimeAppData.cleanup = Get-P914RuntimeCleanup $context $EvidenceDirectory
            if (Test-Path -LiteralPath $fixtureReceiptPath -PathType Leaf) { $manifest.fixture.afterSnapshot = Write-P919Snapshot 'after' $workspacePath $fixtureReceiptPath $EvidenceDirectory }
        } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Final cleanup or read-only database snapshot failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        $manifest.route.routeEvents = @(Get-P914RouteSummary $context)
        $manifest.route.lifecycleEvents = @(Get-P914LifecycleSummary $context)
        $manifest.route.completedAtUtc = Get-P914Utc
    }
}

$manifest.failure = $failure
$manifest.status = if ($null -eq $failure) { 'route-completed-validation-pending' } else { 'failed' }
$manifest.completedAtUtc = Get-P914Utc
try { if ($null -ne $context) { Save-P914RouteManifest $manifest $EvidenceDirectory } } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Route manifest serialization failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'evidence' } }; $manifest.failure = $failure; $manifest.status = 'failed' }
try { Write-P919Manifest $manifestPath $manifest } catch { $failure = [ordered]@{ message = 'P9-19 manifest serialization failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'evidence' }; $manifest.failure = $failure; $manifest.status = 'failed'; Write-P919Manifest $manifestPath $manifest }

$validationPath = Join-Path $EvidenceDirectory 'validation-report.json'
$validatorPath = Join-Path $PSScriptRoot 'validate_packaged_next_p9_19_classes_lifecycle.py'
$validationLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'validator-output.txt'
try {
    $validationCall = Invoke-P919Python $validatorPath @('--repository',$RepositoryRoot,'--manifest',$manifestPath,'--report-output',$validationPath) $validationLog
    $validation = Read-P914Json $validationPath
    $manifest.validation = [ordered]@{ status = [string]$validation.status; passed = [bool]$validation.passed; path = 'validation-report.json'; sha256 = Get-P914FileSha256 $validationPath; logPath = 'logs/validator-output.txt'; logSha256 = [string]$validationCall.logSha256; report = $validation }
    if ([bool]$validation.passed -and $null -eq $failure) { $manifest.status = 'passed' } else { $manifest.status = 'failed'; if ($null -eq $failure) { $failure = [ordered]@{ message = 'One or more P9-19 validator gates failed.'; capturedAtUtc = Get-P914Utc; category = 'validation' }; $manifest.failure = $failure } }
} catch {
    $manifest.status = 'failed'; $manifest.validation = [ordered]@{ status = 'failed'; passed = $false; path = if (Test-Path -LiteralPath $validationPath) { 'validation-report.json' } else { $null }; failure = $_.Exception.Message; logPath = if (Test-Path -LiteralPath $validationLog) { 'logs/validator-output.txt' } else { $null } }
    if ($null -eq $failure) { $failure = [ordered]@{ message = 'P9-19 validation command failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'validation' }; $manifest.failure = $failure }
}
$manifest.completedAtUtc = Get-P914Utc
try { Write-P919Manifest $manifestPath $manifest } catch { $manifest.status = 'failed'; $manifest.failure = [ordered]@{ message = 'Final P9-19 manifest serialization failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'evidence' }; Write-P919Manifest $manifestPath $manifest }
Write-Host (ConvertTo-Json -Compress ([ordered]@{ taskId = $TaskId; status = [string]$manifest.status; runId = $RunId; evidenceDirectory = $EvidenceDirectory; manifest = $manifestPath; failure = $manifest.failure }))
if ([string]$manifest.status -cne 'passed') { exit 1 }
