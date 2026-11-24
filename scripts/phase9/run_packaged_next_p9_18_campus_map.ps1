param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$BuildReceiptPath,
    [string]$EvidenceRoot = 'artifacts/phase9/packaged-next-p9-18-campus-map',
    [ValidateRange(30,300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(10,180)][int]$ExitTimeoutSeconds = 60
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$scriptRepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ($RepositoryRoot -ine $scriptRepositoryRoot) { throw "P9-18 repository root does not match the runner's actual repository: $RepositoryRoot" }
. (Join-Path $RepositoryRoot 'scripts/phase9/p9_14_packaged_lifecycle_common.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing

$TaskId = 'P9_18_PACKAGED_NEXT_CAMPUS_MAP_OPEN_CLOSE'
$Schema = 'classmngr-phase9-p9-18-packaged-campus-map-v1'
$CallPhaseDiagnosticTaskId = 'P9-18_CALL_PHASE_DIAGNOSTIC'
$CallPhaseDiagnosticSchema = 'classmngr-phase9-p9-18-call-phase-memory-diagnostic-v1'
$CallPhaseDiagnosticTestPath = 'scripts/phase9/test_p9_18_call_phase_diagnostic.py'
$CallPhaseRunnerPath = 'scripts/phase9/run_packaged_next_p9_18_campus_map.ps1'
$MainWindowAutomationId = 'QApplication.classMngrNextApplicationShell'
$CampusDirectoryStackAutomationId = 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget'
$InformationPageAutomationId = $CampusDirectoryStackAutomationId + '.nextCampusDirectoryPage_0'
$MapPageAutomationId = $CampusDirectoryStackAutomationId + '.nextCampusDirectoryPage_4'
$MapImageCardAutomationId = $MapPageAutomationId + '.campusDirectoryScrollArea.qt_scrollarea_viewport.campusDirectoryForm.campusMapImageCard'
$MapImageAutomationId = $MapImageCardAutomationId + '.campusMapImage'
$TargetedUiaSchema = 'classmngr-phase9-p9-18-targeted-uia-v1'
$TargetedUiaCachedProperties = @('AutomationId','Name','ControlType','ProcessId','NativeWindowHandle','ClassName','IsEnabled','IsOffscreen','BoundingRectangle')
$WorkingSetTargetBytes = [Int64]268435456
$WorkingSetCeilingBytes = [Int64]471859200
$OperationPeakAdvisoryBytes = [Int64]536870912
$ProductRoots = @('CMakeLists.txt','CMakePresets.json','cmake','docs/qt-rewrite/phase-4-resource-inventory.json','licenses','resources','scripts/phase4','src','tests','third_party')
$BuildReceiptPath = [IO.Path]::GetFullPath($BuildReceiptPath)
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
if ($env:OS -cne 'Windows_NT') { throw 'P9-18 packaged UIA lifecycle must run locally on Windows.' }
$buildEvidenceRoot = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'artifacts/phase9/p9-18-exact-release-builds')).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
if (-not $BuildReceiptPath.StartsWith($buildEvidenceRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Build receipt must come from a P9-18 exact-source Release build under artifacts/phase9/p9-18-exact-release-builds.' }
if (-not (Test-Path -LiteralPath $BuildReceiptPath -PathType Leaf)) { throw "P9-18 exact-source Release build receipt is missing: $BuildReceiptPath" }
$runId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
$EvidenceDirectory = Join-Path $EvidenceRoot $runId
if (Test-Path -LiteralPath $EvidenceDirectory) { throw "Refusing to overwrite P9-18 route evidence: $EvidenceDirectory" }
foreach ($name in @('','logs','runtime','runtime/settings','runtime/local-app-data','runtime/app-data','runtime/temp','runtime/diagnostics','fixture','package','package/bin','screenshots','uia','output')) {
    $directory = if ([string]::IsNullOrEmpty($name)) { $EvidenceDirectory } else { Join-Path $EvidenceDirectory $name }
    [void][IO.Directory]::CreateDirectory($directory)
}

function Get-P918RepositoryRelativePath([string]$Root, [string]$Path) {
    $rootPrefix = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not $fullPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw "P9-18 receipt path is outside the repository: $fullPath" }
    return $fullPath.Substring($rootPrefix.Length).Replace('\','/')
}

function Invoke-P918Preparer([string]$Directory, [string[]]$Arguments, [string]$LogName) {
    $scriptPath = Join-Path $PSScriptRoot 'prepare_p9_18_bundang_fixture.py'
    $logPath = Join-Path (Join-Path $Directory 'logs') $LogName
    $lines = @(& python $scriptPath @Arguments 2>&1 | ForEach-Object { [string]$_ })
    $code = [int]$LASTEXITCODE
    [IO.File]::WriteAllLines($logPath, $lines, [Text.UTF8Encoding]::new($false))
    if ($code -ne 0) { throw "P9-18 fixture helper failed with exit code ${code}: $($lines -join ' ')" }
    $jsonLine = @($lines | Where-Object { $_.TrimStart().StartsWith('{') } | Select-Object -Last 1)
    if ($jsonLine.Count -ne 1) { throw 'P9-18 fixture helper emitted no single JSON summary.' }
    return ($jsonLine[0] | ConvertFrom-Json -ErrorAction Stop)
}

function Get-P918HarnessSources {
    $relative = @(
        'scripts/phase9/p9_14_packaged_lifecycle_common.ps1',
        'scripts/phase9/p9_18_build_current_source_release.ps1',
        'scripts/phase9/prepare_p9_18_bundang_fixture.py',
        'scripts/phase9/run_packaged_next_p9_18_campus_map.ps1',
        'scripts/phase9/validate_packaged_next_p9_18_campus_map.py',
        'scripts/phase9/test_prepare_p9_18_bundang_fixture.py',
        'scripts/phase9/test_validate_packaged_next_p9_18_campus_map.py'
    )
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in $relative) {
        $fullPath = Join-Path $RepositoryRoot $path
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) { throw "Required P9-18 harness source is missing: $path" }
        $rows.Add([ordered]@{ path = $path; sizeBytes = [Int64](Get-Item -LiteralPath $fullPath).Length; sha256 = Get-P914FileSha256 $fullPath })
    }
    $files = @($rows.ToArray())
    [ordered]@{ fileCount = $files.Count; sha256 = Get-P914InventoryFingerprint $files; files = $files }
}

function Assert-P918BuildReceipt([string]$Directory, [string]$ReceiptPath) {
    $receiptHash = Get-P914FileSha256 $ReceiptPath
    $receipt = Read-P914Json $ReceiptPath
    if ([string]$receipt.schema -cne 'classmngr-phase9-p9-18-exact-release-build-v1' -or [string]$receipt.taskId -cne $TaskId -or [string]$receipt.status -cne 'succeeded') { throw 'Build receipt is not a successful P9-18 exact-source VS2022 Release receipt.' }
    if ([string]$receipt.configuration -cne 'Release' -or [string]$receipt.platform -cne 'x64' -or [string]$receipt.generator -cne 'Visual Studio 17 2022' -or [string]$receipt.generatorPlatform -cne 'x64' -or [string]$receipt.generatorToolset -cne 'v143' -or [string]$receipt.windowsSdkVersion -cne '10.0.26100.0') { throw 'P9-18 build receipt does not prove the pinned Windows x64 VS2022/v143 Release toolchain.' }
    if ([string]$receipt.compiler.fileVersion -cne '19.44.35230.0' -or [string]$receipt.compiler.toolsetVersion -cne '14.44.35207') { throw 'P9-18 build receipt compiler identity differs from the pinned VS2022 v143 toolchain.' }
    $expectedRedistArgument = '-DCLASSMNGR_INSTALLER_VC_REDIST_FILE=' + [string]$receipt.vcRedist.path
    $expectedInstalledRedist = [IO.Path]::GetFullPath((Join-Path ([string]$receipt.generatorInstance) 'VC/Redist/MSVC/v143/vc_redist.x64.exe'))
    if ([IO.Path]::GetFullPath([string]$receipt.vcRedist.path) -ine $expectedInstalledRedist -or [string]$receipt.vcRedist.signatureStatus -cne 'Valid' -or [string]$receipt.vcRedist.signer -notmatch '^CN=Microsoft Corporation,' -or [string]::IsNullOrWhiteSpace([string]$receipt.vcRedist.signerThumbprint) -or (Get-P914FileSha256 ([string]$receipt.vcRedist.path)) -cne [string]$receipt.vcRedist.sha256 -or (Get-P914FileSha256 ([string]$receipt.vcRedist.cacheReferenceValuePath)) -cne [string]$receipt.vcRedist.cacheReferenceValueSha256 -or [string]$receipt.vcRedist.cacheReferenceValueSha256 -cne [string]$receipt.vcRedist.sha256 -or @($receipt.configure.arguments) -notcontains $expectedRedistArgument) { throw 'P9-18 build receipt does not bind the signed installed VS2022 v143 x64 redistributable used by the clean package configuration.' }
    if ([string]$receipt.buildDirectory -notlike '*artifacts*' -or [IO.Path]::GetFullPath([string]$receipt.buildDirectory) -ieq [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'build/windows-x64-release'))) { throw 'P9-18 build receipt points into the preserved standard Release build tree.' }
    if ([int]$receipt.build.exitCode -ne 0 -or [string]$receipt.releaseExecutableSha256 -cne (Get-P914FileSha256 ([string]$receipt.releaseExecutablePath)) -or (Get-P914PeMachine ([string]$receipt.releaseExecutablePath)) -ne 0x8664) { throw 'P9-18 build receipt does not prove the exact x64 Release executable bytes.' }
    $helper = Join-Path $RepositoryRoot 'scripts/phase9/p9_18_build_current_source_release.ps1'
    if ((Get-P914FileSha256 $helper) -cne [string]$receipt.buildHelperSha256) { throw 'P9-18 exact-build helper changed after its receipt was created.' }
    $harnessInventoryPath = [IO.Path]::GetFullPath([string]$receipt.harnessInventoryPath)
    if ((Get-P914FileSha256 $harnessInventoryPath) -cne [string]$receipt.harnessInventoryFileSha256) { throw 'P9-18 harness inventory file hash differs from the exact-source build receipt.' }
    $recordedHarness = Read-P914Json $harnessInventoryPath
    $currentHarness = Get-P918HarnessSources
    if ([string]$recordedHarness.sha256 -cne [string]$receipt.harnessSourceSha256 -or [int]$recordedHarness.fileCount -ne [int]$receipt.harnessSourceFileCount) { throw 'P9-18 recorded harness inventory differs from the exact-source build receipt.' }
    $recordedRows = @{}; foreach ($row in @($recordedHarness.files)) { $recordedRows[[string]$row.path] = $row }
    $currentRows = @{}; foreach ($row in @($currentHarness.files)) { $currentRows[[string]$row.path] = $row }
    if ($recordedRows.Count -ne $currentRows.Count -or [int]$currentHarness.fileCount -ne [int]$receipt.harnessSourceFileCount) { throw 'P9-18 harness inventory file set differs from the exact-source receipt.' }
    $changedHarnessPaths = [Collections.Generic.List[string]]::new()
    foreach ($path in $recordedRows.Keys) {
        if (-not $currentRows.ContainsKey($path)) { throw "P9-18 harness source disappeared after the exact-source build: $path" }
        $oldRow = $recordedRows[$path]; $newRow = $currentRows[$path]
        if ([Int64]$oldRow.sizeBytes -ne [Int64]$newRow.sizeBytes -or [string]$oldRow.sha256 -cne [string]$newRow.sha256) {
            if ([string]$path -cne $CallPhaseRunnerPath) { throw "P9-18 harness source differs from the exact-source receipt outside the call-phase diagnostic runner overlay: $path" }
            $changedHarnessPaths.Add([string]$path)
        }
    }
    if ($changedHarnessPaths.Count -gt 1) { throw 'P9-18 permits only a runner call-phase diagnostic overlay after the pinned RETRY13 build.' }
    $currentSource = Get-P914SourceInventory $RepositoryRoot @($receipt.sourceIncludedRoots)
    if ([string]$currentSource.sha256 -cne [string]$receipt.sourceSha256 -or [int]$currentSource.fileCount -ne [int]$receipt.sourceFileCount) { throw 'Current product/test/resource source differs from the exact-source Release build receipt.' }
    $sourcePath = [IO.Path]::GetFullPath([string]$receipt.sourceInventoryPath)
    if ((Get-P914FileSha256 $sourcePath) -cne [string]$receipt.sourceInventoryFileSha256) { throw 'P9-18 build source inventory file hash differs from its receipt.' }
    $recordedSource = Read-P914Json $sourcePath
    if ([string]$recordedSource.sha256 -cne [string]$currentSource.sha256 -or [int]$recordedSource.fileCount -ne [int]$currentSource.fileCount) { throw 'P9-18 build source inventory does not reproduce the current source fingerprint.' }
    $generated = $receipt.generatedResources
    foreach ($path in @([string]$generated.assetsRoot,[string]$generated.translationsRoot,[string]$generated.catalogPath)) {
        if (-not (Test-Path -LiteralPath $path)) { throw "P9-18 Release build generated output is missing: $path" }
    }
    $assets = Get-P914CanonicalInventory ([string]$generated.assetsRoot)
    $translations = Get-P914CanonicalInventory ([string]$generated.translationsRoot)
    if ([string]$assets.sha256 -cne [string]$generated.assetsSha256 -or [string]$translations.sha256 -cne [string]$generated.translationsSha256 -or (Get-P914FileSha256 ([string]$generated.catalogPath)) -cne [string]$generated.catalogSha256) { throw 'Generated resource outputs differ from the P9-18 Release build receipt.' }
    $diagnosticTestFullPath = Join-Path $RepositoryRoot $CallPhaseDiagnosticTestPath
    if (-not (Test-Path -LiteralPath $diagnosticTestFullPath -PathType Leaf)) { throw "Focused P9-18 call-phase diagnostic test is missing: $CallPhaseDiagnosticTestPath" }
    [ordered]@{
        receipt = $receipt; receiptPath = $ReceiptPath; receiptSha256 = $receiptHash
        currentSourceSha256 = [string]$currentSource.sha256; sourceFileCount = [int]$currentSource.fileCount
        releaseExecutableSha256 = [string]$receipt.releaseExecutableSha256
        recordedHarnessSourceSha256 = [string]$recordedHarness.sha256
        currentHarnessSourceSha256 = [string]$currentHarness.sha256
        runnerSourceSha256 = [string]$currentRows[$CallPhaseRunnerPath].sha256
        diagnosticTestPath = $diagnosticTestFullPath
        diagnosticTestSha256 = Get-P914FileSha256 $diagnosticTestFullPath
    }
}

function Get-P918StopwatchNanoseconds([Diagnostics.Stopwatch]$Watch) {
    $frequency = [decimal][Diagnostics.Stopwatch]::Frequency
    $nanoseconds = ([decimal]$Watch.ElapsedTicks * [decimal]1000000000) / $frequency
    return [Int64][decimal]::Round($nanoseconds, 0, [MidpointRounding]::AwayFromZero)
}

function Get-P918DiagnosticProcessCounterSnapshot([object]$Context) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $errors = [Collections.Generic.List[string]]::new()
    $sample = [ordered]@{
        sampleStartedAtUtc = Get-P914Utc; sampleCompletedAtUtc = $null
        processId = [Int64]$Context.process.Id; processState = 'unknown'
        workingSetBytes = $null; privateUsageBytes = $null; peakWorkingSetBytes = $null
        counterReadDurationNanoseconds = $null; metricErrors = @()
    }
    try {
        $Context.process.Refresh()
        if ($Context.process.HasExited) {
            $sample.processState = 'exited'
            $errors.Add('measured process exited at the call-phase diagnostic point')
        } else {
            $sample.processState = 'running'
            $sample.workingSetBytes = [Int64]$Context.process.WorkingSet64
            $sample.privateUsageBytes = [Int64]$Context.process.PrivateMemorySize64
            $sample.peakWorkingSetBytes = [Int64]$Context.process.PeakWorkingSet64
        }
    } catch { $errors.Add($_.Exception.Message) }
    finally {
        $watch.Stop()
        $sample.counterReadDurationNanoseconds = Get-P918StopwatchNanoseconds $watch
        $sample.sampleCompletedAtUtc = Get-P914Utc
        $sample.metricErrors = $errors.ToArray()
    }
    return $sample
}

function Get-P918CallPhaseContext(
    [object]$Context,
    [string]$WindowId,
    [string]$PageTitle,
    [string]$Phase = '',
    [object]$CycleNumber = $null
) {
    $resolvedPhase = $Phase
    $resolvedCycleNumber = $CycleNumber
    if ([string]::IsNullOrWhiteSpace($resolvedPhase)) {
        if ($WindowId -ceq 'cold-baseline') { $resolvedPhase = 'cold-baseline' }
        elseif ($WindowId.StartsWith('plateau-baseline-', [StringComparison]::Ordinal)) { $resolvedPhase = 'baseline' }
        elseif ($WindowId -ceq 'warmup-map-open') { $resolvedPhase = 'warmup-open' }
        elseif ($WindowId -ceq 'warmup-map-suspended') { $resolvedPhase = 'warmup-return' }
        elseif ($WindowId -match '^cycle-(\d+)-map-open$') { $resolvedPhase = 'cycle-open'; $resolvedCycleNumber = [int]$Matches[1] }
        elseif ($WindowId -match '^cycle-(\d+)-return$') { $resolvedPhase = 'cycle-return'; $resolvedCycleNumber = [int]$Matches[1] }
        else { $resolvedPhase = 'navigation-setup' }
    }
    [ordered]@{
        windowId = $WindowId; phase = $resolvedPhase
        cycleNumber = if ($null -ne $resolvedCycleNumber -and [int]$resolvedCycleNumber -gt 0) { [int]$resolvedCycleNumber } else { $null }
        pageTitle = if ([string]::IsNullOrEmpty($PageTitle)) { $null } else { $PageTitle }
        processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle
    }
}

function Get-P918DiagnosticCounterDelta([object]$Before, [object]$After, [string]$MetricName) {
    if ($null -eq $Before[$MetricName] -or $null -eq $After[$MetricName]) { return $null }
    return ([decimal]$After[$MetricName] - [decimal]$Before[$MetricName])
}

function Add-P918CallPhaseDiagnosticSample([object]$Manifest, [object]$Sample) {
    try { $Manifest.callPhaseDiagnostic.samples.Add($Sample) }
    catch {
        try { $Manifest.callPhaseDiagnostic.recordingErrors.Add($_.Exception.Message) }
        catch { }
    }
}

function Add-P918DiagnosticCounterPoint(
    [object]$Manifest,
    [object]$Context,
    [string]$WindowId,
    [string]$PageTitle,
    [string]$Phase,
    [object]$CycleNumber,
    [string]$PointName,
    [object]$CounterSnapshot,
    [DateTimeOffset]$ObservedAtUtc,
    [decimal]$SettleDurationMilliseconds = 0
) {
    $sample = [ordered]@{
        sequence = [int]$Manifest.callPhaseDiagnostic.samples.Count
        recordType = 'counter-point'; point = $PointName
        observedAtUtc = $ObservedAtUtc.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'")
        context = Get-P918CallPhaseContext $Context $WindowId $PageTitle $Phase $CycleNumber
        settleDurationMilliseconds = $SettleDurationMilliseconds
        counterSnapshot = $CounterSnapshot
    }
    Add-P918CallPhaseDiagnosticSample $Manifest $sample
}

function Invoke-P918DiagnosticCall(
    [object]$Context,
    [object]$Manifest,
    [string]$WindowId,
    [string]$PageTitle,
    [string]$CallKind,
    [scriptblock]$Operation,
    [string]$Phase = '',
    [object]$CycleNumber = $null
) {
    $callContext = Get-P918CallPhaseContext $Context $WindowId $PageTitle $Phase $CycleNumber
    $before = Get-P918DiagnosticProcessCounterSnapshot $Context
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $outcome = 'succeeded'; $operationError = $null; $result = $null
    try { $result = & $Operation }
    catch { $outcome = 'failed'; $operationError = $_.Exception.Message; throw }
    finally {
        $watch.Stop()
        $after = Get-P918DiagnosticProcessCounterSnapshot $Context
        $reference = $null
        if ($null -ne $result -and $result -is [Collections.IDictionary]) {
            if ($result.Contains('manifestRecord')) { $reference = $result['manifestRecord'] }
            elseif ($result.Contains('relativePath')) { $reference = $result }
        }
        $metricErrors = @(@($before.metricErrors) + @($after.metricErrors))
        $sample = [ordered]@{
            sequence = [int]$Manifest.callPhaseDiagnostic.samples.Count
            recordType = 'call-bracket'; callKind = $CallKind; context = $callContext
            operationOutcome = $outcome; operationError = $operationError
            callDurationNanoseconds = Get-P918StopwatchNanoseconds $watch
            before = $before; after = $after
            delta = [ordered]@{
                workingSetBytes = Get-P918DiagnosticCounterDelta $before $after 'workingSetBytes'
                privateUsageBytes = Get-P918DiagnosticCounterDelta $before $after 'privateUsageBytes'
                peakWorkingSetBytes = Get-P918DiagnosticCounterDelta $before $after 'peakWorkingSetBytes'
            }
            metricErrors = $metricErrors; evidenceReference = $reference
        }
        Add-P918CallPhaseDiagnosticSample $Manifest $sample
    }
    return $result
}

function Stage-P918Package([string]$Directory, [object]$BuildProof) {
    $receipt = $BuildProof.receipt
    $packageRoot = Join-Path (Join-Path $Directory 'package') 'bin'
    $applicationPath = Join-Path $packageRoot 'ClassMngrNext.exe'
    Copy-Item -LiteralPath ([string]$receipt.releaseExecutablePath) -Destination $applicationPath
    $resourceRoot = Join-Path $packageRoot 'resources'
    $assetRoot = Join-Path $resourceRoot 'assets'
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
    $oldPath = [Environment]::GetEnvironmentVariable('PATH','Process')
    $savedErrorActionPreference = $ErrorActionPreference
    try {
        [Environment]::SetEnvironmentVariable('PATH', "$qtBin;$oldPath", 'Process')
        $ErrorActionPreference = 'Continue'
        $lines = @(& $windeployqt --qmldir (Join-Path $RepositoryRoot 'src/features/calendar/ui/qml') --skip-plugin-types qmltooling --include-plugins qoffscreen,qsqlite --exclude-plugins qsqlibase,qsqlmimer,qsqlodbc,qsqlpsql --translations en,ko --compiler-runtime --dir $packageRoot $applicationPath 2>&1 | ForEach-Object { [string]$_ })
        $exitCode = [int]$LASTEXITCODE
    } finally {
        [Environment]::SetEnvironmentVariable('PATH', $oldPath, 'Process')
        $ErrorActionPreference = $savedErrorActionPreference
    }
    [IO.File]::WriteAllLines($deployLog, $lines, [Text.UTF8Encoding]::new($false))
    if ($exitCode -ne 0) { throw "Cache-bound windeployqt failed with exit code $exitCode. See $deployLog" }
    foreach ($relative in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Sql.dll','platforms/qwindows.dll','sqldrivers/qsqlite.dll','resources/resource-catalog.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $packageRoot $relative) -PathType Leaf)) { throw "P9-18 packaged Release application lacks $relative" }
    }
    $package = Get-P914CanonicalInventory $packageRoot
    $packageInventoryPath = Join-Path $Directory 'package-files.json'
    Write-P914Json $packageInventoryPath $package
    $resource = Get-P914CanonicalInventory $resourceRoot
    $resourceInventoryPath = Join-Path $Directory 'resource-files.json'
    Write-P914Json $resourceInventoryPath $resource
    [ordered]@{
        packageRoot = [IO.Path]::GetFullPath($packageRoot); packageSha256 = [string]$package.sha256; packageFileCount = [int]$package.fileCount
        packageInventoryPath = 'package-files.json'; packageInventoryFileSha256 = Get-P914FileSha256 $packageInventoryPath
        applicationPath = [IO.Path]::GetFullPath($applicationPath); applicationSha256 = Get-P914FileSha256 $applicationPath; architecture = 'x64'
        resourceRoot = [IO.Path]::GetFullPath($resourceRoot); resourceSha256 = [string]$resource.sha256
        resourceInventoryPath = 'resource-files.json'; resourceInventoryFileSha256 = Get-P914FileSha256 $resourceInventoryPath
        windeployqtPath = [IO.Path]::GetFullPath($windeployqt); windeployqtSha256 = Get-P914FileSha256 $windeployqt; deployLogPath = 'logs/windeployqt.log'; deployLogSha256 = Get-P914FileSha256 $deployLog; deployExitCode = $exitCode
    }
}

function Initialize-P918Settings([string]$Directory) {
    $settingsRoot = Join-Path (Join-Path $Directory 'runtime') 'settings'
    $profilePath = Join-Path (Join-Path $settingsRoot 'PaperCloud') 'ClassMngr.ini'
    [IO.Directory]::CreateDirectory((Split-Path -Parent $profilePath)) | Out-Null
    $profileText = "[options]`r`ntheme=1`r`nlanguage=1`r`n`r`n[campus]`r`nlastSelectedJsonId=bundang`r`n`r`n[ui]`r`nwindowGeometry=@ByteArray(\x1\xd9\xd0\xcb\0\x3\0\0\0\0\x4\0\0\0\x1\xc2\0\0\a\xff\0\0\x4\xaf\0\0\x4\0\0\0\x1\xe0\0\0\a\xff\0\0\x4\xaf\0\0\0\0\0\0\0\0\f\0\0\0\x4\0\0\0\x1\xe0\0\0\a\xff\0\0\x4\xaf)`r`n"
    [IO.File]::WriteAllText($profilePath, $profileText, [Text.UTF8Encoding]::new($false))
    $inventory = Get-P914CanonicalInventory $settingsRoot
    if ([int]$inventory.fileCount -ne 1 -or [IO.File]::ReadAllText($profilePath,[Text.Encoding]::UTF8) -cne $profileText) { throw 'Isolated P9-18 settings profile does not contain exactly the pinned English/Light/Bundang preferences.' }
    $path = Join-Path (Join-Path $Directory 'runtime') 'settings-before-launch-inventory.json'
    Write-P914Json $path $inventory
    [ordered]@{
        root = [IO.Path]::GetFullPath($settingsRoot); profileRelativePath = 'runtime/settings/PaperCloud/ClassMngr.ini'
        profileSha256 = Get-P914FileSha256 $profilePath; inventoryPath = 'runtime/settings-before-launch-inventory.json'
        inventoryFileSha256 = Get-P914FileSha256 $path; inventorySha256 = [string]$inventory.sha256
        language = 'English'; theme = 'Light'; lastSelectedCampusId = 'bundang'
        readOnlyByHarness = $true; noWorkspaceDatabaseOpened = $true
    }
}

function Get-P918TreeId { 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree' }
function Expand-P918CampusDirectory([object]$Context, [object]$Manifest, [string]$Directory) {
    $treeId = Get-P918TreeId
    $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Directory' $treeId
    if ($null -eq $group) { throw 'Exact Campus Directory navigation group is not exposed by UIA.' }
    $pattern = $null
    try { $pattern = [System.Windows.Automation.ExpandCollapsePattern]$group.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern) } catch { throw 'Campus Directory navigation group lacks UIA ExpandCollapsePattern.' }
    if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Expanded) { return }
    if ($pattern.Current.ExpandCollapseState -ne [System.Windows.Automation.ExpandCollapseState]::Collapsed) { throw "Campus Directory has unsupported expansion state $($pattern.Current.ExpandCollapseState)." }
    $action = Add-P914Action $Context $Manifest $Directory 'expand-campus-directory-navigation-group' $group 'ExpandCollapsePattern.Expand'
    $pattern.Expand(); $action.invocationReturnedAtUtc = Get-P914Utc
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt 2) {
        $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Directory' $treeId
        if ($null -eq $group) { throw 'Campus Directory group disappeared after UIA Expand.' }
        $pattern = [System.Windows.Automation.ExpandCollapsePattern]$group.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
        if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Expanded) { $action.expansionStateAfter = 'Expanded'; $action.completedAtUtc = Get-P914Utc; return }
        Start-Sleep -Milliseconds 100
    }
    $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Directory' $treeId
    if ($null -eq $group) { throw 'Campus Directory UIA TreeItem disappeared before disclosure inspection.' }
    $current = $group.Current; $snapshot = Get-P914ControlSnapshot $Context $group; $rectangle = $current.BoundingRectangle
    $handle = [Int64]$Context.mainWindowHandle; $measuredProcessId = [uint32]$Context.process.Id
    if ([uint32]$current.ProcessId -ne $measuredProcessId -or [bool]$current.IsOffscreen -or [double]$rectangle.Width -le 0 -or [double]$rectangle.Height -le 0 -or [Int64]$snapshot.ownerWindowHandle -ne $handle -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $measuredProcessId) { throw 'Campus Directory UIA disclosure inspection failed PID/HWND/visibility/bounds checks.' }
    $bounds = [P914Native]::GetBounds([IntPtr]$handle,$measuredProcessId)
    if ($rectangle.X -lt $bounds[0] -or $rectangle.Y -lt $bounds[1] -or ($rectangle.X+$rectangle.Width) -gt ($bounds[0]+$bounds[2]) -or ($rectangle.Y+$rectangle.Height) -gt ($bounds[1]+$bounds[3])) { throw 'Campus Directory UIA row leaves the measured main HWND.' }
    $windowBoundsRecord = [ordered]@{ x = [int]$bounds[0]; y = [int]$bounds[1]; width = [int]$bounds[2]; height = [int]$bounds[3] }
    $tree = Get-P914Control $Context 'main' $treeId 'ControlType.Tree' ''
    if ($null -eq $tree) { throw 'Measured navigation parent Tree is not exposed by UIA before Campus Directory disclosure inspection.' }
    $treeSnapshot = Get-P914ControlSnapshot $Context $tree
    $treeCurrent = $tree.Current; $treeRectangle = $treeCurrent.BoundingRectangle
    if ([uint32]$treeCurrent.ProcessId -ne $measuredProcessId -or [bool]$treeCurrent.IsOffscreen -or [string]$treeCurrent.ControlType.ProgrammaticName -cne 'ControlType.Tree' -or [string]$treeCurrent.AutomationId -cne $treeId -or [Int64]$treeSnapshot.ownerWindowHandle -ne $handle -or $treeRectangle.Width -le 0 -or $treeRectangle.Height -le 0) { throw 'Navigation parent Tree failed its measured PID/HWND/type/bounds checks.' }
    if ($treeRectangle.X -lt $bounds[0] -or $treeRectangle.Y -lt $bounds[1] -or ($treeRectangle.X+$treeRectangle.Width) -gt ($bounds[0]+$bounds[2]) -or ($treeRectangle.Y+$treeRectangle.Height) -gt ($bounds[1]+$bounds[3])) { throw 'Campus Directory parent Tree bounds leave the measured main HWND.' }
    $disclosureScreenshot = Invoke-P918DiagnosticCall -Context $Context -Manifest $Manifest `
        -WindowId 'campus-directory-navigation-before-disclosure-click' -PageTitle 'Campus Directory Navigation' `
        -CallKind 'screenshot' -Phase 'navigation-setup' -Operation {
            Capture-P914Screenshot $Context $Manifest $Directory 'campus-directory-navigation-before-disclosure-click' $Context.mainWindowRoot
        }
    if ([Int64]$disclosureScreenshot.processId -ne [Int64]$measuredProcessId -or [Int64]$disclosureScreenshot.windowHandle -ne $handle -or [Int64]$disclosureScreenshot.windowProcessId -ne [Int64]$measuredProcessId -or [int]$disclosureScreenshot.width -ne [int]$bounds[2] -or [int]$disclosureScreenshot.height -ne [int]$bounds[3] -or [int]$disclosureScreenshot.screenBounds.x -ne [int]$bounds[0] -or [int]$disclosureScreenshot.screenBounds.y -ne [int]$bounds[1]) { throw 'Campus Directory disclosure screenshot is not bound to the measured PID/HWND/bounds.' }
    $treeImageBounds = [ordered]@{ x = [double]$treeRectangle.X - [double]$windowBoundsRecord.x; y = [double]$treeRectangle.Y - [double]$windowBoundsRecord.y; width = [double]$treeRectangle.Width; height = [double]$treeRectangle.Height }
    $itemImageBounds = [ordered]@{ x = [double]$rectangle.X - [double]$windowBoundsRecord.x; y = [double]$rectangle.Y - [double]$windowBoundsRecord.y; width = [double]$rectangle.Width; height = [double]$rectangle.Height }
    Write-Host ('Inspect ' + $disclosureScreenshot.relativePath + '. Current image=' + $disclosureScreenshot.width + 'x' + $disclosureScreenshot.height + ', screen origin=(' + $windowBoundsRecord.x + ',' + $windowBoundsRecord.y + '), UIA Tree image bounds=' + ($treeImageBounds | ConvertTo-Json -Compress) + ', Campus Directory TreeItem image bounds=' + ($itemImageBounds | ConvertTo-Json -Compress) + '. Enter the visible disclosure glyph pixel x,y; blank input stops without a click.')
    $enteredPoint = Read-Host 'Measured disclosure pixel x,y'
    if ([string]::IsNullOrWhiteSpace($enteredPoint) -or $enteredPoint -notmatch '^\s*(\d+)\s*,\s*(\d+)\s*$') { throw 'No valid screenshot-derived Campus Directory disclosure point was supplied; navigation stopped without a click.' }
    $imageX = [int]$Matches[1]; $imageY = [int]$Matches[2]
    if ($imageX -lt 0 -or $imageY -lt 0 -or $imageX -ge [int]$disclosureScreenshot.width -or $imageY -ge [int]$disclosureScreenshot.height) { throw 'Campus Directory disclosure point is outside the captured PrintWindow image.' }
    $x = [int]$bounds[0] + $imageX; $y = [int]$bounds[1] + $imageY
    $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Directory' $treeId
    if ($null -eq $group) { throw 'Campus Directory UIA TreeItem disappeared after screenshot inspection.' }
    $current = $group.Current; $rectangle = $current.BoundingRectangle; $freshSnapshot = Get-P914ControlSnapshot $Context $group
    if ([uint32]$current.ProcessId -ne $measuredProcessId -or [bool]$current.IsOffscreen -or [Int64]$freshSnapshot.ownerWindowHandle -ne $handle -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $measuredProcessId) { throw 'Campus Directory UIA target or measured HWND identity changed after screenshot inspection.' }
    if ([double]$rectangle.X -ne [double]$snapshot.rectangle.x -or [double]$rectangle.Y -ne [double]$snapshot.rectangle.y -or [double]$rectangle.Width -ne [double]$snapshot.rectangle.width -or [double]$rectangle.Height -ne [double]$snapshot.rectangle.height) { throw 'Campus Directory UIA bounds changed after screenshot inspection; the inspected point is stale.' }
    $tree = Get-P914Control $Context 'main' $treeId 'ControlType.Tree' ''
    if ($null -eq $tree) { throw 'Navigation parent Tree disappeared after screenshot inspection.' }
    $freshTreeSnapshot = Get-P914ControlSnapshot $Context $tree; $treeCurrent = $tree.Current; $treeRectangle = $treeCurrent.BoundingRectangle
    if ([uint32]$treeCurrent.ProcessId -ne $measuredProcessId -or [bool]$treeCurrent.IsOffscreen -or [string]$treeCurrent.ControlType.ProgrammaticName -cne 'ControlType.Tree' -or [string]$treeCurrent.AutomationId -cne $treeId -or [Int64]$freshTreeSnapshot.ownerWindowHandle -ne $handle) { throw 'Navigation parent Tree identity changed after screenshot inspection.' }
    if ([double]$treeRectangle.X -ne [double]$treeSnapshot.rectangle.x -or [double]$treeRectangle.Y -ne [double]$treeSnapshot.rectangle.y -or [double]$treeRectangle.Width -ne [double]$treeSnapshot.rectangle.width -or [double]$treeRectangle.Height -ne [double]$treeSnapshot.rectangle.height) { throw 'Navigation parent Tree bounds changed after screenshot inspection; the inspected point is stale.' }
    $freshBounds = [P914Native]::GetBounds([IntPtr]$handle,$measuredProcessId)
    if (($freshBounds -join ',') -cne ($bounds -join ',')) { throw 'Measured main window bounds changed after screenshot inspection; the inspected point is stale.' }
    if ($x -lt $treeRectangle.X -or $x -ge ($treeRectangle.X+$treeRectangle.Width) -or $y -lt $treeRectangle.Y -or $y -ge ($treeRectangle.Y+$treeRectangle.Height)) { throw 'Screenshot-derived Campus Directory disclosure point is outside the current UIA parent Tree bounds.' }
    if ($x -ge $rectangle.X -or $y -lt $rectangle.Y -or $y -ge ($rectangle.Y+$rectangle.Height)) { throw 'Screenshot-derived point is not in the navigation Tree disclosure region adjacent to the Campus Directory row.' }
    if ($x -lt $bounds[0] -or $x -ge ($bounds[0]+$bounds[2]) -or $y -lt $bounds[1] -or $y -ge ($bounds[1]+$bounds[3])) { throw 'Screenshot-derived Campus Directory disclosure point is outside the measured main HWND.' }
    $fallback = [ordered]@{ actionKind = 'expand-campus-directory-navigation-group-uia-click-fallback'; method = 'fresh PrintWindow-inspected disclosure indicator with screenshot-derived point bounded by UIA parent Tree and measured main HWND; bounded WM mouse click'; targetPageId = $null; target = $freshSnapshot; pointerAction = [ordered]@{ actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'; screenX = $x; screenY = $y; sourceTarget = [ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; rectangle = [ordered]@{ x = [double]$rectangle.X; y = [double]$rectangle.Y; width = [double]$rectangle.Width; height = [double]$rectangle.Height } }; parentTreeTarget = $freshTreeSnapshot; targetProcessId = [Int64]$measuredProcessId; windowHandle = $handle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$handle); measuredMainWindowBounds = $windowBoundsRecord; disclosurePointDerivation = [ordered]@{ method = 'operator-inspected fresh PrintWindow pixel converted through captured main-window screen origin and checked against fresh UIA parent Tree and TreeItem bounds'; screenshotRelativePath = [string]$disclosureScreenshot.relativePath; screenshotSha256 = [string]$disclosureScreenshot.sha256; screenshotCapturedAtUtc = [string]$disclosureScreenshot.capturedAtUtc; screenshotProcessId = [Int64]$disclosureScreenshot.processId; screenshotWindowHandle = [Int64]$disclosureScreenshot.windowHandle; screenshotWindowProcessId = [Int64]$disclosureScreenshot.windowProcessId; screenshotWidth = [int]$disclosureScreenshot.width; screenshotHeight = [int]$disclosureScreenshot.height; screenshotScreenBounds = $windowBoundsRecord; selectedImagePixel = [ordered]@{ x = $imageX; y = $imageY }; uiaTreeBoundsAtInspection = $freshTreeSnapshot; uiaTreeItemBoundsAtInspection = $freshSnapshot } }; issuedAtUtc = Get-P914Utc }
    $Manifest.route.campusDirectoryDisclosureInspection = [ordered]@{ screenshotRelativePath = [string]$disclosureScreenshot.relativePath; screenshotSha256 = [string]$disclosureScreenshot.sha256; screenshotProcessId = [Int64]$disclosureScreenshot.processId; screenshotWindowHandle = [Int64]$disclosureScreenshot.windowHandle; parentTreeTarget = $freshTreeSnapshot; treeItemTarget = $freshSnapshot; selectedImagePixel = [ordered]@{ x = $imageX; y = $imageY }; screenPoint = [ordered]@{ x = $x; y = $y }; actionKind = [string]$fallback.actionKind; method = [string]$fallback.method }
    $Manifest.route.actions.Add($fallback); Add-P914JsonLine (Join-Path $Directory 'uia-actions.jsonl') $fallback
    [P914Native]::ClickScreenPoint([IntPtr]$handle,$measuredProcessId,$x,$y)
    $watch.Restart()
    while ($watch.Elapsed.TotalSeconds -lt 2) {
        $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Directory' $treeId
        $pattern = [System.Windows.Automation.ExpandCollapsePattern]$group.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
        if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Expanded) { $fallback.expansionStateAfter = 'Expanded'; $fallback.completedAtUtc = Get-P914Utc; return }
        Start-Sleep -Milliseconds 100
    }
    throw 'Campus Directory did not expand after the UIA-bounded fallback click.'
}

function Get-P918LifecycleInfo([object[]]$Events) {
    @($Events | ForEach-Object { [ordered]@{ arrivalIndex = [Int64]$_.arrivalIndex; pageKey = [string]$_.record.page_key; state = [string]$_.record.state; capturedAtUtc = [string]$_.record.captured_at; pageInstanceAvailable = [bool]$_.record.page_instance_available; widgetCount = [Int64]$_.record.page.widget_count; sampleAvailable = [bool]$_.record.sample_available } })
}
function Assert-P918RouteTarget([object]$RouteEvent, [string]$TargetPageId) {
    if ($null -eq $RouteEvent -or [string]$RouteEvent.targetPageId -cne $TargetPageId) { throw "P9-18 route did not confirm exact target page $TargetPageId." }
}
function Assert-P918LifecycleEvent([object]$LifecycleEvent, [string]$PageKey, [string]$State) {
    $instanceAvailableProperty = if ($null -ne $LifecycleEvent -and $null -ne $LifecycleEvent.record) { $LifecycleEvent.record.PSObject.Properties['page_instance_available'] } else { $null }
    $sampleAvailableProperty = if ($null -ne $LifecycleEvent -and $null -ne $LifecycleEvent.record) { $LifecycleEvent.record.PSObject.Properties['sample_available'] } else { $null }
    $expectedInstanceAvailable = $State -cne 'created'
    if ($null -eq $LifecycleEvent -or [string]$LifecycleEvent.record.event -cne 'page-lifecycle' -or [string]$LifecycleEvent.record.page_key -cne $PageKey -or [string]$LifecycleEvent.record.state -cne $State -or $null -eq $instanceAvailableProperty -or $instanceAvailableProperty.Value -isnot [bool] -or $null -eq $sampleAvailableProperty -or $sampleAvailableProperty.Value -isnot [bool] -or $instanceAvailableProperty.Value -ne $expectedInstanceAvailable -or $sampleAvailableProperty.Value -ne $true) { throw "P9-18 lifecycle did not confirm typed page-instance and sample state for $PageKey/$State." }
}
function Assert-P918LifecycleSequence([object[]]$Events, [string[]]$Expected, [string]$Label) {
    $actual = @($Events | ForEach-Object { [string]$_.record.page_key + '/' + [string]$_.record.state })
    if ($actual.Count -ne $Expected.Count -or ($actual -join '|') -cne ($Expected -join '|')) { throw "P9-18 lifecycle sequence differs for ${Label}: $($actual -join ' -> ')." }
    $arrivalIndexes = @($Events | ForEach-Object { [Int64]$_.arrivalIndex })
    if ($arrivalIndexes.Count -ne $Expected.Count) { throw "P9-18 lifecycle sequence lacks arrival indexes for $Label." }
    for ($index = 0; $index -lt $arrivalIndexes.Count; $index++) {
        Assert-P918LifecycleEvent $Events[$index] ([string]$Expected[$index].Split('/')[0]) ([string]$Expected[$index].Split('/')[1])
        if ($index -gt 0 -and $arrivalIndexes[$index] -le $arrivalIndexes[$index - 1]) { throw "P9-18 lifecycle sequence arrival order differs for $Label." }
    }
}
function New-P918TargetedUiaCacheRequest {
    $cache = [System.Windows.Automation.CacheRequest]::new()
    $cache.TreeScope = [System.Windows.Automation.TreeScope]::Element
    $cache.AutomationElementMode = [System.Windows.Automation.AutomationElementMode]::Full
    foreach ($property in @(
        [System.Windows.Automation.AutomationElement]::AutomationIdProperty,
        [System.Windows.Automation.AutomationElement]::NameProperty,
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.AutomationElement]::ProcessIdProperty,
        [System.Windows.Automation.AutomationElement]::NativeWindowHandleProperty,
        [System.Windows.Automation.AutomationElement]::ClassNameProperty,
        [System.Windows.Automation.AutomationElement]::IsEnabledProperty,
        [System.Windows.Automation.AutomationElement]::IsOffscreenProperty,
        [System.Windows.Automation.AutomationElement]::BoundingRectangleProperty
    )) { $cache.Add($property) }
    return $cache
}

function Get-P918TargetedCondition([string]$AutomationId, [string]$ControlType, [object]$Name = $null) {
    if ([string]::IsNullOrWhiteSpace($AutomationId) -or [string]::IsNullOrWhiteSpace($ControlType)) { throw 'P9-18 targeted UIA query requires an exact AutomationId and ControlType.' }
    $control = switch ($ControlType) {
        'ControlType.Text' { [System.Windows.Automation.ControlType]::Text }
        'ControlType.Custom' { [System.Windows.Automation.ControlType]::Custom }
        'ControlType.Image' { [System.Windows.Automation.ControlType]::Image }
        default { throw "Unsupported P9-18 targeted UIA ControlType: $ControlType" }
    }
    $conditions = [Collections.Generic.List[System.Windows.Automation.Condition]]::new()
    $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::AutomationIdProperty, $AutomationId))
    $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, $control))
    if ($null -ne $Name) { $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, $Name)) }
    return [System.Windows.Automation.AndCondition]::new([System.Windows.Automation.Condition[]]$conditions.ToArray())
}

function Get-P918CachedTargetControl([System.Windows.Automation.AutomationElement]$Element, [object]$Context, [string]$Label) {
    $automationId = [string]$Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::AutomationIdProperty)
    $name = [string]$Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::NameProperty)
    $controlType = [string]$Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::ControlTypeProperty).ProgrammaticName
    $processId = [Int64]$Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::ProcessIdProperty)
    $nativeWindowHandle = [Int64]$Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::NativeWindowHandleProperty)
    $className = [string]$Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::ClassNameProperty)
    $rectangle = $Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::BoundingRectangleProperty)
    if ($processId -ne [Int64]$Context.process.Id -or $automationId -notin @($MapImageCardAutomationId, $MapImageAutomationId) -and $Label -ne 'page-title') { throw "P9-18 targeted UIA query returned an element outside the measured PID or exact target IDs in $Label." }
    [ordered]@{
        name = $name; automationId = $automationId; controlType = $controlType
        processId = $processId; nativeWindowHandle = $nativeWindowHandle
        ownerWindowHandle = [Int64]$Context.mainWindowHandle
        ownerWindowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle)
        className = $className
        isEnabled = [bool]$Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::IsEnabledProperty)
        isOffscreen = [bool]$Element.GetCachedPropertyValue([System.Windows.Automation.AutomationElement]::IsOffscreenProperty)
        rectangle = [ordered]@{ x = [double]$rectangle.X; y = [double]$rectangle.Y; width = [double]$rectangle.Width; height = [double]$rectangle.Height }
    }
}

function Get-P918TargetedUiaElements([System.Windows.Automation.AutomationElement]$Root, [object]$Context, [System.Windows.Automation.Condition]$Condition, [string]$Label) {
    $items = [Collections.Generic.List[object]]::new()
    $matches = $Root.FindAll([System.Windows.Automation.TreeScope]::Descendants, $Condition)
    foreach ($element in $matches) { $items.Add((Get-P918CachedTargetControl $element $Context $Label)) }
    return $items.ToArray()
}

function Save-P918TargetedUiaArtifact([object]$Context, [object]$Manifest, [string]$Directory, [string]$Name, [string]$ExpectedPage) {
    $root = Get-P914MainRoot $Context
    $measuredProcessId = [Int64]$Context.process.Id
    $measuredHandle = [Int64]$Context.mainWindowHandle
    if ($null -eq $root -or $measuredHandle -le 0 -or [Int64][P914Native]::GetWindowProcessId([IntPtr]$measuredHandle) -ne $measuredProcessId) { throw 'P9-18 UIA confirmation lost the original measured main window.' }
    $rootSnapshot = Get-P914ControlSnapshot $Context $root
    if ([Int64]$rootSnapshot.processId -ne $measuredProcessId -or [Int64]$rootSnapshot.ownerWindowHandle -ne $measuredHandle -or [string]$rootSnapshot.automationId -cne $MainWindowAutomationId -or [string]$rootSnapshot.controlType -cne 'ControlType.Window' -or [bool]$rootSnapshot.isOffscreen) { throw 'P9-18 targeted UIA root identity is not the visible measured Next main HWND.' }
    $titlePageId = if ($ExpectedPage -ceq 'Maps') { $MapPageAutomationId } elseif ($ExpectedPage -ceq 'Office Information') { $InformationPageAutomationId } else { throw "Unsupported P9-18 page title: $ExpectedPage" }
    $titleId = $titlePageId + '.campusDirectoryTitle'
    $titleCondition = Get-P918TargetedCondition $titleId 'ControlType.Text' $ExpectedPage
    $cardCondition = Get-P918TargetedCondition $MapImageCardAutomationId 'ControlType.Custom' $null
    $imageCondition = Get-P918TargetedCondition $MapImageAutomationId 'ControlType.Image' $null
    $cache = New-P918TargetedUiaCacheRequest
    $activation = $cache.Activate()
    try {
        $titleElements = @(Get-P918TargetedUiaElements $root $Context $titleCondition 'page-title')
        $mapImageCards = @(Get-P918TargetedUiaElements $root $Context $cardCondition 'map-image-card')
        $mapImageNodes = @(Get-P918TargetedUiaElements $root $Context $imageCondition 'map-image')
    } finally { $activation.Dispose() }
    if ($titleElements.Count -ne 1 -or [string]$titleElements[0].name -cne $ExpectedPage -or [string]$titleElements[0].automationId -cne $titleId -or [string]$titleElements[0].controlType -cne 'ControlType.Text' -or [bool]$titleElements[0].isOffscreen) { throw "P9-18 targeted UIA current page title is not the exact visible '$ExpectedPage' title for $Name." }
    if ($ExpectedPage -ceq 'Maps') {
        foreach ($controlSet in @(@{ label = 'map image card'; items = $mapImageCards }, @{ label = 'map image'; items = $mapImageNodes })) {
            if ($controlSet.items.Count -notin @(0,2) -or ($controlSet.items.Count -eq 2 -and @($controlSet.items | Where-Object { [bool]$_.isOffscreen }).Count -ne 0)) { throw "P9-18 UIA exposed an incomplete or hidden $($controlSet.label) set in $Name." }
        }
    } elseif ($mapImageNodes.Count -ne 0 -or $mapImageCards.Count -ne 0) {
        throw "P9-18 map image UIA controls remain after page suspension in $Name."
    }
    $artifact = [ordered]@{
        schema = $TargetedUiaSchema; taskId = $TaskId; name = 'state-' + $Name; capturedAtUtc = Get-P914Utc
        query = [ordered]@{
            method = 'AutomationElement.FindAll with property-constrained AndCondition and CacheRequest.Activate'
            treeScope = 'Descendants'; cacheRequestUsed = $true; cacheTreeScope = 'Element'
            cachedProperties = $TargetedUiaCachedProperties
            targetPageObjectAutomationId = $titlePageId
            targets = [ordered]@{
                pageTitle = [ordered]@{ automationId = $titleId; controlType = 'ControlType.Text'; name = $ExpectedPage }
                mapImageCard = [ordered]@{ automationId = $MapImageCardAutomationId; controlType = 'ControlType.Custom' }
                mapImage = [ordered]@{ automationId = $MapImageAutomationId; controlType = 'ControlType.Image' }
            }
        }
        processId = $measuredProcessId; mainWindowHandle = $measuredHandle
        windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$measuredHandle)
        rootNativeWindowHandle = [Int64]$root.Current.NativeWindowHandle
        root = $rootSnapshot; expectedPageTitle = $ExpectedPage
        controls = [ordered]@{
            pageTitle = [ordered]@{ automationId = $titleId; controlType = 'ControlType.Text'; name = $ExpectedPage; count = $titleElements.Count; elements = $titleElements }
            mapImageCard = [ordered]@{ automationId = $MapImageCardAutomationId; controlType = 'ControlType.Custom'; name = ''; count = $mapImageCards.Count; elements = $mapImageCards }
            mapImage = [ordered]@{ automationId = $MapImageAutomationId; controlType = 'ControlType.Image'; name = ''; count = $mapImageNodes.Count; elements = $mapImageNodes }
        }
    }
    $artifactPath = Join-Path (Join-Path $Directory 'uia') ('target-controls-' + $Name + '.json')
    Write-P914Json $artifactPath $artifact
    $artifactFile = Get-Item -LiteralPath $artifactPath
    $artifactManifestRecord = [ordered]@{ relativePath = 'uia/target-controls-' + $Name + '.json'; sha256 = Get-P914FileSha256 $artifactPath; sizeBytes = [Int64]$artifactFile.Length; capturedAtUtc = [string]$artifact.capturedAtUtc; processId = $measuredProcessId; windowHandle = $measuredHandle }
    $Manifest.route.targetedUiaArtifacts.Add($artifactManifestRecord)
    return [ordered]@{ root = $root; record = $artifact; path = $artifactPath; manifestRecord = $artifactManifestRecord }
}

function Get-P918Confirmation([object]$Context, [object]$Manifest, [string]$Directory, [string]$Name, [string]$ExpectedPage, [object[]]$LifecycleEvents) {
    $targetedUia = Invoke-P918DiagnosticCall -Context $Context -Manifest $Manifest `
        -WindowId $Name -PageTitle $ExpectedPage -CallKind 'uia-targeted-controls' -Operation {
            Save-P918TargetedUiaArtifact $Context $Manifest $Directory $Name $ExpectedPage
        }
    $root = $targetedUia.root
    $artifact = $targetedUia.record
    $artifactManifestRecord = $targetedUia.manifestRecord
    $screenshot = Invoke-P918DiagnosticCall -Context $Context -Manifest $Manifest `
        -WindowId $Name -PageTitle $ExpectedPage -CallKind 'screenshot' -Operation {
            Capture-P914Screenshot $Context $Manifest $Directory ('state-' + $Name) $root
        }
    [ordered]@{
        state = if ($ExpectedPage -ceq 'Maps') { 'campus-map' } else { 'campus-information' }
        pageTitle = $ExpectedPage; processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle
        confirmedAtUtc = Get-P914Utc; targetedUiaArtifact = $artifactManifestRecord; screenshot = $screenshot
        lifecycleEvents = Get-P918LifecycleInfo $LifecycleEvents
        pageLifecycleMapWidgetCount = if ($ExpectedPage -ceq 'Maps') { [Int64](@($LifecycleEvents | Where-Object { [string]$_.record.page_key -ceq 'campus_map' -and [string]$_.record.state -ceq 'activated' } | Select-Object -Last 1)[0].record.page.widget_count) } else { $null }
        uiaMapImageControlCount = [int]$artifact.controls.mapImage.count; uiaMapImageCardCount = [int]$artifact.controls.mapImageCard.count
    }
}

function Get-P918ResourceEvents([object]$Context, [Int64]$AfterArrival, [Int64]$BeforeArrival) {
    $prefix = 'ClassMngrNext resource checkpoint='
    $records = [Collections.Generic.List[object]]::new()
    foreach ($entry in $Context.outputLines) {
        if ([Int64]$entry.arrivalIndex -le $AfterArrival -or [Int64]$entry.arrivalIndex -gt $BeforeArrival -or -not ([string]$entry.line).StartsWith($prefix,[StringComparison]::Ordinal)) { continue }
        try { $payload = ([string]$entry.line).Substring($prefix.Length) | ConvertFrom-Json -ErrorAction Stop } catch { continue }
        $records.Add([ordered]@{ arrivalIndex = [Int64]$entry.arrivalIndex; observedAtUtc = [string]$entry.observedAtUtc; record = $payload })
    }
    $records.ToArray()
}
function Assert-P918MapResourceLoads([object[]]$Events, [object]$Fixture, [string]$ActivationId) {
    $requiredIds = @($Fixture.images | ForEach-Object { [string]$_.resourceId })
    $relevant = @($Events | Where-Object { [string]$_.record.resource_id -in $requiredIds })
    $actualIds = @($relevant | ForEach-Object { [string]$_.record.resource_id } | Sort-Object -CaseSensitive)
    $expectedIds = @($requiredIds | Sort-Object -CaseSensitive)
    if ($relevant.Count -ne 2 -or ($actualIds -join "`n") -cne ($expectedIds -join "`n")) {
        $actual = @($relevant | ForEach-Object { [string]$_.record.resource_id }) -join ', '
        throw "P9-18 map activation $ActivationId did not load exactly the two pinned Bundang images: $actual"
    }
    foreach ($event in $relevant) {
        $record = $event.record
        if ([string]$record.outcome -cne 'succeeded' -or [string]$record.request_scope.kind -cne 'page' -or [string]$record.request_scope.owner -cne 'campus_map') { throw "P9-18 image load was unsuccessful or outside the campus_map page scope in $ActivationId." }
        if ($null -eq $record.image.decoded_dimensions -or [Int64]$record.payload_accounting.decoded_payload_bytes -le 0) { throw "P9-18 image load lacks decoded image evidence in $ActivationId." }
        $fixtureImage = @($Fixture.images | Where-Object { [string]$_.resourceId -ceq [string]$record.resource_id })
        if ($fixtureImage.Count -ne 1 -or [string]$fixtureImage[0].catalogRecord.location -cne 'resource_file' -or [string]$fixtureImage[0].catalogRecord.resource_kind -cne 'campus_map_image' -or [string]$fixtureImage[0].catalogRecord.expected_type -cne 'png' -or [string]$record.catalog_resource.location -cne 'package_file' -or [string]$record.catalog_resource.kind -cne 'campus_map_image' -or [string]$record.catalog_resource.type -cne 'png') { throw "P9-18 source/package resource catalog metadata differs from its pinned Bundang fixture in $ActivationId." }
        $dimensions = $record.image.source_dimensions
        $decodedDimensions = $record.image.decoded_dimensions
        if ([Int64]$record.payload_accounting.source_catalog_bytes -ne [Int64]$fixtureImage[0].sizeBytes -or [int]$dimensions.width_pixels -ne [int]$fixtureImage[0].widthPixels -or [int]$dimensions.height_pixels -ne [int]$fixtureImage[0].heightPixels -or [int]$decodedDimensions.width_pixels -ne [int]$fixtureImage[0].widthPixels -or [int]$decodedDimensions.height_pixels -ne [int]$fixtureImage[0].heightPixels) { throw "P9-18 decoded source/payload dimensions differ from the pinned Bundang asset in $ActivationId." }
    }
    $relevant
}

function Invoke-P918MemoryWindow([object]$Context, [object]$Manifest, [string]$Name, [string]$Phase, [int]$CycleNumber, [object]$Confirmation) {
    $Context.process.Refresh(); if ($Context.process.HasExited) { throw "Measured process exited before settled P9-18 memory window $Name." }
    $confirmedAt = [DateTimeOffset]::Parse([string]$Confirmation.confirmedAtUtc).ToUniversalTime()
    Start-Sleep -Seconds 2
    $settleCompletedAt = [DateTimeOffset]::UtcNow
    $settleCounterSnapshot = Get-P918DiagnosticProcessCounterSnapshot $Context
    Add-P918DiagnosticCounterPoint -Manifest $Manifest -Context $Context -WindowId $Name `
        -PageTitle ([string]$Confirmation.pageTitle) -Phase $Phase -CycleNumber $CycleNumber `
        -PointName 'after-two-second-settle' -CounterSnapshot $settleCounterSnapshot `
        -ObservedAtUtc $settleCompletedAt -SettleDurationMilliseconds ([decimal]($settleCompletedAt - $confirmedAt).TotalMilliseconds)
    $samplingStarted = [DateTimeOffset]::UtcNow
    if (($samplingStarted - $confirmedAt).TotalSeconds -lt 1.9) { throw "P9-18 state settle interval was shorter than two seconds before $Name." }
    Set-P914SamplerContext $Context 'normal-use' ($Phase + '-' + $Name)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $samples = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt 10; $index++) {
        Start-Sleep -Milliseconds 100
        $errors = [Collections.Generic.List[string]]::new()
        $sample = [ordered]@{ sampleIndex = $index; capturedAtUtc = Get-P914Utc; elapsedMilliseconds = [int]$watch.ElapsedMilliseconds; processId = [Int64]$Context.process.Id; processState = 'running'; workingSetBytes = $null; privateBytes = $null; peakWorkingSetBytes = $null; handles = $null; threadCount = $null; metricErrors = @() }
        try {
            $Context.process.Refresh()
            if ($Context.process.HasExited) { $sample.processState = 'exited'; $errors.Add('measured process exited during settled memory sampling') }
            else {
                $sample.workingSetBytes = [Int64]$Context.process.WorkingSet64
                $sample.privateBytes = [Int64]$Context.process.PrivateMemorySize64
                $sample.peakWorkingSetBytes = [Int64]$Context.process.PeakWorkingSet64
                $sample.handles = [Int64]$Context.process.HandleCount
                $sample.threadCount = [Int64]$Context.process.Threads.Count
            }
        } catch { $errors.Add($_.Exception.Message) }
        $sample.metricErrors = $errors.ToArray()
        $samples.Add($sample)
        if ($sample.processState -eq 'exited') { break }
    }
    $workingSetValues = @($samples | Where-Object { $null -ne $_.workingSetBytes } | ForEach-Object { [Int64]$_.workingSetBytes })
    $targetMisses = @(); $ceilingMisses = @()
    for ($index = 0; $index -lt $samples.Count; $index++) {
        if ($null -eq $samples[$index].workingSetBytes) { continue }
        if ([Int64]$samples[$index].workingSetBytes -ge $WorkingSetTargetBytes) { $targetMisses += $index }
        if ([Int64]$samples[$index].workingSetBytes -gt $WorkingSetCeilingBytes) { $ceilingMisses += $index }
    }
    $window = [ordered]@{
        windowId = $Name; state = [string]$Confirmation.state; matchedState = [string]$Confirmation.state; phase = $Phase
        cycleNumber = if ($CycleNumber -gt 0) { $CycleNumber } else { $null }
        processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle
        sampleIntervalMilliseconds = 100; nominalDurationMilliseconds = 1000; settleDelaySeconds = 2
        stateConfirmedBeforeSampling = $true; stateConfirmation = $Confirmation
        samplingStartedAtUtc = $samplingStarted.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'"); samplingCompletedAtUtc = Get-P914Utc
        startedAtUtc = if ($samples.Count -gt 0) { [string]$samples[0].capturedAtUtc } else { Get-P914Utc }; completedAtUtc = Get-P914Utc
        sampleCount = [int]$samples.Count; samplingCompleted = ($samples.Count -eq 10); samplingErrorCount = [int](@($samples | Where-Object { @($_.metricErrors).Count -gt 0 }).Count); samplingErrors = @($samples | Where-Object { @($_.metricErrors).Count -gt 0 } | ForEach-Object { $_.metricErrors })
        workingSetTargetBytes = $WorkingSetTargetBytes; workingSetTargetExclusive = $true; workingSetCeilingBytes = $WorkingSetCeilingBytes; workingSetCeilingExclusive = $false
        workingSetTargetExceededSampleIndexes = $targetMisses; workingSetCeilingExceededSampleIndexes = $ceilingMisses
        workingSetTargetMet = ($targetMisses.Count -eq 0); workingSetCeilingPassed = ($ceilingMisses.Count -eq 0)
        maximumWorkingSetBytes = if ($workingSetValues.Count -gt 0) { [Int64](($workingSetValues | Measure-Object -Maximum).Maximum) } else { $null }
        samples = $samples.ToArray()
    }
    $Manifest.memory.windows.Add($window)
    return $window
}

function Start-P918App([string]$Directory, [string]$ApplicationPath) {
    $runtime = Join-Path $Directory 'runtime'
    $tracePath = Join-Path (Join-Path $runtime 'diagnostics') 'application-shutdown.jsonl'
    $stdoutPath = Join-Path (Join-Path $Directory 'logs') 'process-stdout.log'
    $stderrPath = Join-Path (Join-Path $Directory 'logs') 'process-stderr.log'
    foreach ($path in @($stdoutPath,$stderrPath,(Join-Path $Directory 'application-output.jsonl'),(Join-Path $Directory 'uia-actions.jsonl'))) { [IO.File]::WriteAllText($path,'',[Text.UTF8Encoding]::new($false)) }
    $envMap = [ordered]@{
        CLASSMNGR_SETTINGS_ROOT = (Join-Path $runtime 'settings')
        CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH = [IO.Path]::GetFullPath($tracePath)
        LOCALAPPDATA = (Join-Path $runtime 'local-app-data'); APPDATA = (Join-Path $runtime 'app-data')
        TEMP = (Join-Path $runtime 'temp'); TMP = (Join-Path $runtime 'temp')
        QT_FORCE_STDERR_LOGGING = '1'; PATH = "$((Split-Path -Parent $ApplicationPath));$env:PATH"
    }
    $old = [ordered]@{}
    foreach ($entry in $envMap.GetEnumerator()) { $old[[string]$entry.Key] = [Environment]::GetEnvironmentVariable([string]$entry.Key,'Process'); [Environment]::SetEnvironmentVariable([string]$entry.Key,[string]$entry.Value,'Process') }
    try { $process = Start-Process -FilePath $ApplicationPath -WorkingDirectory (Split-Path -Parent $ApplicationPath) -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru }
    finally { foreach ($entry in $old.GetEnumerator()) { [Environment]::SetEnvironmentVariable([string]$entry.Key,$entry.Value,'Process') } }
    if ($null -eq $process) { throw 'Unable to start the packaged P9-18 Release process.' }
    if ([int]$process.Id -eq 35864) { throw 'Refusing to use preserved unrelated PID 35864 as the measured application process.' }
    $observedPath = [IO.Path]::GetFullPath([string]$process.MainModule.FileName)
    if ($observedPath -ine [IO.Path]::GetFullPath($ApplicationPath) -or (Get-P914FileSha256 $observedPath) -cne (Get-P914FileSha256 $ApplicationPath) -or (Get-P914PeMachine $observedPath) -ne 0x8664) { throw 'Live process executable path/hash/architecture differs from the packaged P9-18 Release executable.' }
    [ordered]@{
        process = $process; stdoutPath = $stdoutPath; stderrPath = $stderrPath; seen = [ordered]@{ stdout = 0; stderr = 0 }; arrival = 0
        mainWindowHandle = 0L; mainWindowRoot = $null; ownerHandles = [Collections.Generic.Dictionary[string,Int64]]::new([StringComparer]::Ordinal)
        routeEvents = [Collections.Generic.List[object]]::new(); lifecycleEvents = [Collections.Generic.List[object]]::new(); outputLines = [Collections.Generic.List[object]]::new()
        forcedTermination = $false; closeAction = $null; processExit = $null; sampler = $null; workspacePath = ''
        processImagePath = $observedPath; processImageSha256 = Get-P914FileSha256 $observedPath; tempRoot = (Join-Path $runtime 'temp')
    }
}

function Get-P918AppDataInventory([string]$Directory, [string]$Name) {
    $root = Join-Path (Join-Path $Directory 'runtime') $Name
    $inventory = Get-P914CanonicalInventory $root
    $relative = "runtime/$Name-inventory.json"
    $path = Join-Path $Directory $relative
    Write-P914Json $path $inventory
    [ordered]@{ root = [IO.Path]::GetFullPath($root); relativePath = $relative; inventoryFileSha256 = Get-P914FileSha256 $path; fileCount = [int]$inventory.fileCount; sha256 = [string]$inventory.sha256; files = @($inventory.files) }
}
function Get-P918Cleanup([object]$Context, [string]$Directory, [object]$Before) {
    $afterSettings = Get-P918AppDataInventory $Directory 'settings'
    $afterLocal = Get-P918AppDataInventory $Directory 'local-app-data'
    $afterRoaming = Get-P918AppDataInventory $Directory 'app-data'
    $temp = Get-P914CanonicalInventory (Join-Path (Join-Path $Directory 'runtime') 'temp')
    $sidecars = @(Get-ChildItem -LiteralPath (Join-Path $Directory 'runtime') -File -Recurse | Where-Object { $_.Name -match '(?:-wal|-shm|-journal)$' } | ForEach-Object { [IO.Path]::GetFullPath($_.FullName) })
    $settingsSame = [string]$afterSettings.sha256 -ceq [string]$Before.settings.sha256
    $localSame = [string]$afterLocal.sha256 -ceq [string]$Before.localAppData.sha256
    $roamingSame = [string]$afterRoaming.sha256 -ceq [string]$Before.appData.sha256
    [ordered]@{
        capturedAtUtc = Get-P914Utc; processExited = [bool]$Context.process.HasExited; forcedTermination = [bool]$Context.forcedTermination
        settingsUnchanged = $settingsSame; settingsBefore = $Before.settings; settingsAfter = $afterSettings
        localAppDataUnchanged = $localSame; localAppDataBefore = $Before.localAppData; localAppDataAfter = $afterLocal
        appDataUnchanged = $roamingSame; appDataBefore = $Before.appData; appDataAfter = $afterRoaming
        isolatedTempFileCountAfterExit = [int]$temp.fileCount; isolatedTempSha256AfterExit = [string]$temp.sha256
        sqliteSidecarFilesAfterExit = $sidecars
        shutdownTraceRelativePath = if (Test-Path -LiteralPath (Join-Path (Join-Path (Join-Path $Directory 'runtime') 'diagnostics') 'application-shutdown.jsonl')) { 'runtime/diagnostics/application-shutdown.jsonl' } else { $null }
        readOnlyInvariantPassed = ($settingsSame -and $localSame -and $roamingSame -and $temp.fileCount -eq 0 -and $sidecars.Count -eq 0)
    }
}

function Invoke-P918Route([object]$Context, [object]$Manifest, [string]$Directory, [object]$Fixture, [int]$TimeoutSeconds) {
    $initialArrival = [Int64]$Context.arrival
    Expand-P918CampusDirectory $Context $Manifest $Directory
    Set-P914SamplerContext $Context 'operation' 'initial-navigation-campus-information'
    $startIndex = $Manifest.route.actions.Count
    $initialRoute = Select-P914Route $Context $Manifest $Directory 'Information' 'campus_information'
    $infoActivated = Wait-P914Lifecycle $Context $Directory 'campus_information' 'activated' $initialArrival $TimeoutSeconds
    Assert-P918RouteTarget $initialRoute 'campus_information'
    Assert-P918LifecycleEvent $infoActivated 'campus_information' 'activated'
    $initialLifecycle = @($infoActivated)
    $Manifest.route.initialInformationActionIndex = $startIndex
    $Manifest.route.initialInformationRouteEvent = $initialRoute
    $Manifest.route.initialInformationLifecycleEvents = Get-P918LifecycleInfo $initialLifecycle
    $Manifest.route.campusDirectoryGroupExpanded = $true
    $coldConfirmation = Get-P918Confirmation $Context $Manifest $Directory 'cold-baseline' 'Office Information' $initialLifecycle
    $coldWindow = Invoke-P918MemoryWindow $Context $Manifest 'cold-baseline' 'cold-baseline' 0 $coldConfirmation
    $Manifest.memory.coldBaselineWindowId = 'cold-baseline'

    $Manifest.route.warmupCount = 1
    $Manifest.route.measuredCycleCount = 10
    $Manifest.route.baselineWindowsPerState = 5
    $Manifest.route.memorySamplesPerWindow = 10
    $Manifest.route.memoryWindowIntervalMilliseconds = 100
    $Manifest.route.lifecycleProtocol = 'UIA-bound navigation action and exact page-route event, then both target/previous PageHost lifecycle states, then visible PID/HWND-bound UIA snapshot and screenshot, then two-second settle, then ten process samples at 100 ms intervals.'
    $routeStart = [Int64]$Context.arrival
    Set-P914SamplerContext $Context 'operation' 'warmup-activate-campus-map'
    $warmOpenActionIndex = $Manifest.route.actions.Count
    $warmOpenRoute = Select-P914Route $Context $Manifest $Directory 'Maps' 'campus_map'
    $warmMapCreated = Wait-P914Lifecycle $Context $Directory 'campus_map' 'created' $routeStart $TimeoutSeconds
    $warmMapActivated = Wait-P914Lifecycle $Context $Directory 'campus_map' 'activated' $routeStart $TimeoutSeconds
    $warmInfoSuspended = Wait-P914Lifecycle $Context $Directory 'campus_information' 'suspended' $routeStart $TimeoutSeconds
    Assert-P918RouteTarget $warmOpenRoute 'campus_map'
    Assert-P918LifecycleEvent $warmMapCreated 'campus_map' 'created'
    Assert-P918LifecycleEvent $warmMapActivated 'campus_map' 'activated'
    Assert-P918LifecycleEvent $warmInfoSuspended 'campus_information' 'suspended'
    $warmActivationLife = @($warmMapCreated,$warmInfoSuspended,$warmMapActivated)
    Assert-P918LifecycleSequence $warmActivationLife @('campus_map/created','campus_information/suspended','campus_map/activated') 'first map warm-up activation'
    $warmResourceEvents = Get-P918ResourceEvents $Context $routeStart ([Int64]$warmMapActivated.arrivalIndex)
    $warmLoads = Assert-P918MapResourceLoads $warmResourceEvents $Fixture 'warmup'
    $Manifest.route.resourceActivations.Add([ordered]@{ activationId = 'warmup'; cycleNumber = 0; kind = 'warmup'; routeEvent = $warmOpenRoute; lifecycleEvents = Get-P918LifecycleInfo $warmActivationLife; loads = $warmLoads })
    $warmOpenConfirmation = Get-P918Confirmation $Context $Manifest $Directory 'warmup-map-open' 'Maps' $warmActivationLife
    $warmOpenWindow = Invoke-P918MemoryWindow $Context $Manifest 'warmup-open' 'warmup-open' 0 $warmOpenConfirmation

    $routeStart = [Int64]$Context.arrival
    Set-P914SamplerContext $Context 'operation' 'warmup-suspend-campus-map'
    $warmCloseActionIndex = $Manifest.route.actions.Count
    $warmCloseRoute = Select-P914Route $Context $Manifest $Directory 'Information' 'campus_information'
    $warmMapSuspended = Wait-P914Lifecycle $Context $Directory 'campus_map' 'suspended' $routeStart $TimeoutSeconds
    $warmInfoActivated = Wait-P914Lifecycle $Context $Directory 'campus_information' 'activated' $routeStart $TimeoutSeconds
    Assert-P918RouteTarget $warmCloseRoute 'campus_information'
    Assert-P918LifecycleEvent $warmMapSuspended 'campus_map' 'suspended'
    Assert-P918LifecycleEvent $warmInfoActivated 'campus_information' 'activated'
    $warmReturnLife = @($warmMapSuspended,$warmInfoActivated)
    Assert-P918LifecycleSequence $warmReturnLife @('campus_map/suspended','campus_information/activated') 'warm-up return to Information'
    $warmMapWidgetCount = [Int64]$warmMapActivated.record.page.widget_count
    $warmMapSuspendedWidgetCount = [Int64]$warmMapSuspended.record.page.widget_count
    if ($warmMapActivated.record.page_instance_available -ne $true -or $warmMapSuspended.record.page_instance_available -ne $true -or ($warmMapWidgetCount - $warmMapSuspendedWidgetCount) -lt 6) { throw 'Warm-up suspension did not prove Campus Map page image widgets were removed after the map resources loaded.' }
    $warmReturnConfirmation = Get-P918Confirmation $Context $Manifest $Directory 'warmup-map-suspended' 'Office Information' $warmReturnLife
    $warmReturnConfirmation.mapPageWidgetCountBeforeSuspend = $warmMapWidgetCount
    $warmReturnConfirmation.mapPageWidgetCountAfterSuspend = $warmMapSuspendedWidgetCount
    $warmReturnConfirmation.mapPageWidgetCountReleased = $warmMapWidgetCount - $warmMapSuspendedWidgetCount
    $warmReturnWindow = Invoke-P918MemoryWindow $Context $Manifest 'warmup-return' 'warmup-return' 0 $warmReturnConfirmation
    $Manifest.route.warmup = [ordered]@{
        openActionIndex = $warmOpenActionIndex; openRouteEvent = $warmOpenRoute; openLifecycleEvents = Get-P918LifecycleInfo $warmActivationLife; openResourceActivationId = 'warmup'; openMemoryWindowId = [string]$warmOpenWindow.windowId
        suspendActionIndex = $warmCloseActionIndex; suspendRouteEvent = $warmCloseRoute; suspendLifecycleEvents = Get-P918LifecycleInfo $warmReturnLife; returnMemoryWindowId = [string]$warmReturnWindow.windowId
        mapPageWidgetCountBeforeSuspend = $warmMapWidgetCount; mapPageWidgetCountAfterSuspend = $warmMapSuspendedWidgetCount; mapPageWidgetCountReleased = $warmMapWidgetCount - $warmMapSuspendedWidgetCount
    }
    $Manifest.memory.coldReturnWindowId = 'warmup-return'

    for ($index = 1; $index -le 5; $index++) {
        $infoEvent = @($Context.lifecycleEvents | Where-Object { [string]$_.record.page_key -ceq 'campus_information' -and [string]$_.record.state -ceq 'activated' } | Select-Object -Last 1)
        if ($infoEvent.Count -ne 1) { throw 'P9-18 no-action baseline has no confirmed current Campus Information lifecycle event.' }
        $name = 'plateau-baseline-' + $index.ToString('00')
        $confirmation = Get-P918Confirmation $Context $Manifest $Directory $name 'Office Information' @($infoEvent[0])
        $window = Invoke-P918MemoryWindow $Context $Manifest $name 'baseline' 0 $confirmation
        $Manifest.memory.plateauBaselineWindowIds += [string]$window.windowId
    }

    for ($cycle = 1; $cycle -le 10; $cycle++) {
        $tag = $cycle.ToString('00')
        $routeStart = [Int64]$Context.arrival
        Set-P914SamplerContext $Context 'operation' ('cycle-' + $tag + '-activate-campus-map')
        $openActionIndex = $Manifest.route.actions.Count
        $openRoute = Select-P914Route $Context $Manifest $Directory 'Maps' 'campus_map'
        $mapActivated = Wait-P914Lifecycle $Context $Directory 'campus_map' 'activated' $routeStart $TimeoutSeconds
        $infoSuspended = Wait-P914Lifecycle $Context $Directory 'campus_information' 'suspended' $routeStart $TimeoutSeconds
        Assert-P918RouteTarget $openRoute 'campus_map'
        Assert-P918LifecycleEvent $mapActivated 'campus_map' 'activated'
        Assert-P918LifecycleEvent $infoSuspended 'campus_information' 'suspended'
        $activationLife = @($infoSuspended,$mapActivated)
        Assert-P918LifecycleSequence $activationLife @('campus_information/suspended','campus_map/activated') ("cycle $cycle map activation")
        $activationId = 'cycle-' + $tag
        $resourceEvents = Get-P918ResourceEvents $Context $routeStart ([Int64]$mapActivated.arrivalIndex)
        $loads = Assert-P918MapResourceLoads $resourceEvents $Fixture $activationId
        $Manifest.route.resourceActivations.Add([ordered]@{ activationId = $activationId; cycleNumber = $cycle; kind = 'measured'; routeEvent = $openRoute; lifecycleEvents = Get-P918LifecycleInfo $activationLife; loads = $loads })
        $openName = 'cycle-' + $tag + '-map-open'
        $openConfirmation = Get-P918Confirmation $Context $Manifest $Directory $openName 'Maps' $activationLife
        $openWindow = Invoke-P918MemoryWindow $Context $Manifest $openName 'cycle-open' $cycle $openConfirmation

        $routeStart = [Int64]$Context.arrival
        Set-P914SamplerContext $Context 'operation' ('cycle-' + $tag + '-suspend-campus-map')
        $closeActionIndex = $Manifest.route.actions.Count
        $closeRoute = Select-P914Route $Context $Manifest $Directory 'Information' 'campus_information'
        $mapSuspended = Wait-P914Lifecycle $Context $Directory 'campus_map' 'suspended' $routeStart $TimeoutSeconds
        $infoActivated = Wait-P914Lifecycle $Context $Directory 'campus_information' 'activated' $routeStart $TimeoutSeconds
        Assert-P918RouteTarget $closeRoute 'campus_information'
        Assert-P918LifecycleEvent $mapSuspended 'campus_map' 'suspended'
        Assert-P918LifecycleEvent $infoActivated 'campus_information' 'activated'
        $returnLife = @($mapSuspended,$infoActivated)
        Assert-P918LifecycleSequence $returnLife @('campus_map/suspended','campus_information/activated') ("cycle $cycle return to Information")
        $mapWidgetCount = [Int64]$mapActivated.record.page.widget_count
        $mapSuspendedWidgetCount = [Int64]$mapSuspended.record.page.widget_count
        $releasedCount = $mapWidgetCount - $mapSuspendedWidgetCount
        if ($mapActivated.record.page_instance_available -ne $true -or $mapSuspended.record.page_instance_available -ne $true -or $releasedCount -lt 6) { throw "P9-18 cycle $cycle did not prove suspension released both map image cards ($mapWidgetCount -> $mapSuspendedWidgetCount widgets)." }
        $returnName = 'cycle-' + $tag + '-return'
        $returnConfirmation = Get-P918Confirmation $Context $Manifest $Directory $returnName 'Office Information' $returnLife
        $returnConfirmation.mapPageWidgetCountBeforeSuspend = $mapWidgetCount
        $returnConfirmation.mapPageWidgetCountAfterSuspend = $mapSuspendedWidgetCount
        $returnConfirmation.mapPageWidgetCountReleased = $releasedCount
        $returnWindow = Invoke-P918MemoryWindow $Context $Manifest $returnName 'cycle-return' $cycle $returnConfirmation
        $Manifest.route.cycles.Add([ordered]@{
            schema = 'classmngr-phase9-p9-18-campus-map-cycle-v1'; cycleNumber = $cycle; activationId = $activationId
            openActionIndex = $openActionIndex; openRouteEvent = $openRoute; openLifecycleEvents = Get-P918LifecycleInfo $activationLife; openMemoryWindowId = [string]$openWindow.windowId
            suspendActionIndex = $closeActionIndex; suspendRouteEvent = $closeRoute; suspendLifecycleEvents = Get-P918LifecycleInfo $returnLife; returnMemoryWindowId = [string]$returnWindow.windowId
            mapPageWidgetCountBeforeSuspend = $mapWidgetCount; mapPageWidgetCountAfterSuspend = $mapSuspendedWidgetCount; mapPageWidgetCountReleased = $releasedCount
        })
    }
    $Manifest.route.workflowCompletedAtUtc = Get-P914Utc
}

$manifestPath = Join-Path $EvidenceDirectory 'manifest.json'
$manifest = [ordered]@{
    schema = $Schema; taskId = $TaskId; runId = $runId; status = 'running'; startedAtUtc = Get-P914Utc; completedAtUtc = $null; repositoryRoot = $RepositoryRoot
    buildProvenance = $null; package = $null; fixture = $null; settings = $null; process = $null; processExit = $null; appDataSnapshots = $null
    callPhaseDiagnostic = [ordered]@{
        schema = $CallPhaseDiagnosticSchema; taskId = $CallPhaseDiagnosticTaskId
        diagnosticOnly = $true; status = 'recording'
        metricSource = 'System.Diagnostics.Process.WorkingSet64, PrivateMemorySize64, and PeakWorkingSet64 sampled by the P9-18 coordinator.'
        purpose = 'Correlate target-process counters with the synchronous property-constrained target-control UIA query, PrintWindow screenshot, and existing two-second settle completion.'
        scope = 'Target-process counter observations only; diagnostic and not memory ownership attribution or an acceptance input.'
        samples = [Collections.Generic.List[object]]::new()
        recordingErrors = [Collections.Generic.List[string]]::new()
        provenance = $null
    }
    experiment = [ordered]@{ warmupCount = 1; baselineWindowsPerState = 5; measuredCycleCount = 10; memorySamplesPerWindow = 10; sampleIntervalMilliseconds = 100; target = $WorkingSetTargetBytes; ceiling = $WorkingSetCeilingBytes; operationPeakAdvisory = $OperationPeakAdvisoryBytes }
    route = [ordered]@{
        workflow = 'One Bundang Maps warm-up; five no-action Campus Information return-baseline windows; then ten measured Campus Maps activation/suspension cycles.'
        actions = [Collections.Generic.List[object]]::new(); cycles = [Collections.Generic.List[object]]::new(); resourceActivations = [Collections.Generic.List[object]]::new()
        targetedUiaArtifacts = [Collections.Generic.List[object]]::new(); screenshots = [Collections.Generic.List[object]]::new()
        baselineWindowsPerState = 5; measuredCycleCount = 10; warmupCount = 1; lifecycleProtocol = $null; workflowCompletedAtUtc = $null
    }
    memory = [ordered]@{
        windows = [Collections.Generic.List[object]]::new(); coldBaselineWindowId = $null; coldReturnWindowId = $null; plateauBaselineWindowIds = [Collections.Generic.List[string]]::new()
        operationPeakWorkingSetBytes = $null; operationPeakAdvisoryLimitBytes = $OperationPeakAdvisoryBytes; operationPeakAdvisoryOnly = $true
        workingSetTargetBytes = $WorkingSetTargetBytes; workingSetTargetExclusive = $true; workingSetCeilingBytes = $WorkingSetCeilingBytes; workingSetCeilingExclusive = $false
        analysis = $null
    }
    cleanup = $null; failure = $null; validation = $null; applicationOutput = $null; samplerMetrics = $null; harnessSources = $null
}
$failure = $null; $context = $null; $fixture = $null; $buildProof = $null; $settings = $null; $package = $null; $appDataBefore = $null; $routeCompleted = $false
try {
    if ([Environment]::UserInteractive -ne $true) { throw 'P9-18 UIA route requires a UserInteractive Windows session.' }
    $buildProof = Assert-P918BuildReceipt $EvidenceDirectory $BuildReceiptPath
    $manifest.buildProvenance = [ordered]@{
        receiptPath = Get-P918RepositoryRelativePath $RepositoryRoot $BuildReceiptPath; receiptSha256 = [string]$buildProof.receiptSha256
        schema = [string]$buildProof.receipt.schema; sourceSha256 = [string]$buildProof.currentSourceSha256; sourceFileCount = [int]$buildProof.sourceFileCount
        releaseExecutablePath = [string]$buildProof.receipt.releaseExecutablePath; releaseExecutableSha256 = [string]$buildProof.releaseExecutableSha256
        generator = [string]$buildProof.receipt.generator; platform = [string]$buildProof.receipt.generatorPlatform; toolset = [string]$buildProof.receipt.generatorToolset
        compilerPath = [string]$buildProof.receipt.compiler.path; compilerVersion = [string]$buildProof.receipt.compiler.fileVersion; compilerSha256 = [string]$buildProof.receipt.compiler.sha256
        windowsSdkVersion = [string]$buildProof.receipt.windowsSdkVersion; buildDirectory = [string]$buildProof.receipt.buildDirectory
        vcRedistPath = [string]$buildProof.receipt.vcRedist.path; vcRedistSha256 = [string]$buildProof.receipt.vcRedist.sha256; vcRedistSignatureStatus = [string]$buildProof.receipt.vcRedist.signatureStatus
        focusedCtests = $buildProof.receipt.focusedTests
    }
    $harnessSources = Get-P918HarnessSources
    $manifest.harnessSources = $harnessSources
    $manifest.callPhaseDiagnostic.provenance = [ordered]@{
        baseBuildReceiptPath = Get-P918RepositoryRelativePath $RepositoryRoot $BuildReceiptPath
        baseBuildReceiptSha256 = [string]$buildProof.receiptSha256
        productSourceSha256 = [string]$buildProof.currentSourceSha256
        productSourceFileCount = [int]$buildProof.sourceFileCount
        releaseExecutablePath = [string]$buildProof.receipt.releaseExecutablePath
        releaseExecutableSha256 = [string]$buildProof.releaseExecutableSha256
        buildHarnessSourceSha256 = [string]$buildProof.recordedHarnessSourceSha256
        currentHarnessSourceSha256 = [string]$buildProof.currentHarnessSourceSha256
        updatedRunnerPath = $CallPhaseRunnerPath
        updatedRunnerSha256 = [string]$buildProof.runnerSourceSha256
        focusedRegressionTestPath = Get-P918RepositoryRelativePath $RepositoryRoot $buildProof.diagnosticTestPath
        focusedRegressionTestSha256 = [string]$buildProof.diagnosticTestSha256
    }
    $package = Stage-P918Package $EvidenceDirectory $buildProof
    $manifest.package = $package
    $fixturePath = Join-Path (Join-Path $EvidenceDirectory 'fixture') 'fixture-receipt.json'
    $fixtureSummary = Invoke-P918Preparer $EvidenceDirectory @('--repository',$RepositoryRoot,'--package-root',[string]$package.packageRoot,'--output',$fixturePath) 'fixture-preparation-output.txt'
    $fixture = Read-P914Json $fixturePath
    if ([string]$fixture.schema -cne 'classmngr-phase9-p9-18-bundang-map-fixture-v1' -or [string]$fixture.status -cne 'passed' -or [string]$fixture.campusId -cne 'bundang' -or @($fixture.images).Count -ne 2) { throw 'P9-18 fixture receipt did not pin exactly the two current Bundang map images.' }
    $fixtureFile = Get-Item -LiteralPath $fixturePath
    $manifest.fixture = [ordered]@{ kind = 'read-only-bundang-campus-map'; campusId = 'bundang'; relativePath = 'fixture/fixture-receipt.json'; sha256 = Get-P914FileSha256 $fixturePath; sizeBytes = [Int64]$fixtureFile.Length; mapResourceIds = @($fixture.mapResourceIds); images = @($fixture.images) }
    $settings = Initialize-P918Settings $EvidenceDirectory
    $manifest.settings = $settings
    $appDataBefore = [ordered]@{
        settings = Get-P918AppDataInventory $EvidenceDirectory 'settings'
        localAppData = Get-P918AppDataInventory $EvidenceDirectory 'local-app-data'
        appData = Get-P918AppDataInventory $EvidenceDirectory 'app-data'
    }
    $manifest.appDataSnapshots = [ordered]@{ beforeLaunch = $appDataBefore; readOnly = $true; databaseOpened = $false }
    Write-P914Json $manifestPath $manifest
    $context = Start-P918App $EvidenceDirectory ([string]$package.applicationPath)
    $context.sampler = [P914ProcessSampler]::new($context.process)
    $manifest.process = [ordered]@{
        processId = [Int64]$context.process.Id; imagePath = [string]$context.processImagePath; imageSha256 = [string]$context.processImageSha256; architecture = 'x64'
        startedAtUtc = Get-P914Utc; commandLine = [ordered]@{ executable = [string]$context.processImagePath; workspaceArgument = $null }
        mainWindowHandle = $null; mainWindowAutomationId = 'QApplication.classMngrNextApplicationShell'; sessionId = [Int64]$context.process.SessionId
    }
    $root = Wait-P914MainRoot $context $EvidenceDirectory $ReadinessTimeoutSeconds
    $manifest.process.mainWindowHandle = [Int64]$context.mainWindowHandle
    $mainSnapshot = Get-P914ControlSnapshot $context $root
    $manifest.process.mainWindowSnapshot = $mainSnapshot
    Drain-P914Output $context $EvidenceDirectory
    Invoke-P918Route $context $manifest $EvidenceDirectory $fixture $ReadinessTimeoutSeconds
    $routeCompleted = $true
} catch {
    $failure = [ordered]@{ message = $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = if ($null -eq $context) { 'preflight-or-startup' } else { 'route-or-ui' }; fullyQualifiedErrorId = $_.FullyQualifiedErrorId }
} finally {
    if ($null -ne $context) {
        try {
            $context.process.Refresh()
            if (-not $context.process.HasExited) {
                try { $null = Invoke-P914NaturalClose $context $manifest $EvidenceDirectory $ExitTimeoutSeconds }
                catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Natural process close failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
            }
            $context.process.Refresh()
            if (-not $context.process.HasExited -and [int]$context.process.Id -ne 35864) {
                $context.forcedTermination = $true
                Stop-Process -Id $context.process.Id -Force -ErrorAction Stop
                $context.process.WaitForExit()
            }
        } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Final measured-process cleanup failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        try { if ($null -ne $context.sampler) { $context.sampler.Dispose() }; Drain-P914Output $context $EvidenceDirectory } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Final process output sampling failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        if ($null -ne $appDataBefore) {
            try { $manifest.cleanup = Get-P918Cleanup $context $EvidenceDirectory $appDataBefore; $manifest.appDataSnapshots.afterExit = [ordered]@{ settings = $manifest.cleanup.settingsAfter; localAppData = $manifest.cleanup.localAppDataAfter; appData = $manifest.cleanup.appDataAfter } }
            catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'P9-18 read-only app-data or cleanup snapshot failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        }
        try { $manifest.samplerMetrics = Get-P914SamplerSummary $context $EvidenceDirectory; $manifest.memory.operationPeakWorkingSetBytes = [Int64]$manifest.samplerMetrics.operationPeakWorkingSetBytes } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'P9-18 background process metrics could not be saved: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        try { $manifest.processExit = $context.processExit; $context.process.Refresh(); $manifest.process.finalState = if ($context.process.HasExited) { 'exited' } else { 'running' }; $manifest.process.exitCode = if ($context.process.HasExited) { [int]$context.process.ExitCode } else { $null }; $manifest.process.forcedTermination = [bool]$context.forcedTermination }
        catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'P9-18 process exit evidence failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        try { $manifest.route.routeEvents = Get-P914RouteSummary $context; $manifest.route.lifecycleEvents = Get-P914LifecycleSummary $context; Save-P914RouteManifest $manifest $EvidenceDirectory }
        catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'P9-18 route manifest could not be saved: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
        $manifest.applicationOutput = [ordered]@{ relativePath = 'application-output.jsonl'; sha256 = if (Test-Path -LiteralPath (Join-Path $EvidenceDirectory 'application-output.jsonl')) { Get-P914FileSha256 (Join-Path $EvidenceDirectory 'application-output.jsonl') } else { $null }; stdoutPath = 'logs/process-stdout.log'; stdoutSha256 = if (Test-Path -LiteralPath $context.stdoutPath) { Get-P914FileSha256 $context.stdoutPath } else { $null }; stderrPath = 'logs/process-stderr.log'; stderrSha256 = if (Test-Path -LiteralPath $context.stderrPath) { Get-P914FileSha256 $context.stderrPath } else { $null } }
    }
    $manifest.failure = $failure
    $manifest.completedAtUtc = Get-P914Utc
    $manifest.status = if ($null -eq $failure -and $routeCompleted) { 'completed' } else { 'failed' }
    $manifest.callPhaseDiagnostic.recordCount = [int]$manifest.callPhaseDiagnostic.samples.Count
    $manifest.callPhaseDiagnostic.status = if ($routeCompleted) { 'completed' } elseif ($manifest.callPhaseDiagnostic.samples.Count -gt 0) { 'partial' } else { 'not-started' }
    try { Write-P914Json $manifestPath $manifest } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'P9-18 manifest serialization failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'evidence' }; $manifest.failure = $failure; $manifest.status = 'failed' } }
    $validatorPath = Join-Path $PSScriptRoot 'validate_packaged_next_p9_18_campus_map.py'
    if (Test-Path -LiteralPath $validatorPath -PathType Leaf) {
        $validationPath = Join-Path $EvidenceDirectory 'validation.json'
        $validationLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'evidence-validation.log'
        $lines = @(& python $validatorPath --repository $RepositoryRoot --manifest $manifestPath --report-output $validationPath 2>&1 | ForEach-Object { [string]$_ })
        $validationCode = [int]$LASTEXITCODE
        [IO.File]::WriteAllLines($validationLog,$lines,[Text.UTF8Encoding]::new($false))
        $manifest.validation = [ordered]@{ relativePath = 'validation.json'; reportSha256 = if (Test-Path -LiteralPath $validationPath) { Get-P914FileSha256 $validationPath } else { $null }; validatorExitCode = $validationCode; logPath = 'logs/evidence-validation.log'; logSha256 = Get-P914FileSha256 $validationLog }
        if ($validationCode -eq 0) { $manifest.status = 'passed' } else { $manifest.status = 'failed'; if ($null -eq $failure) { $manifest.failure = [ordered]@{ message = 'P9-18 evidence validator reported one or more acceptance gate failures.'; capturedAtUtc = Get-P914Utc; category = 'acceptance-validation' } } }
        Write-P914Json $manifestPath $manifest
    }
}
$diagnosticReceiptPath = Join-Path $EvidenceDirectory 'call-phase-diagnostic-receipt.json'
$diagnosticProvenance = $manifest.callPhaseDiagnostic.provenance
$diagnosticReceipt = [ordered]@{
    schema = 'classmngr-phase9-p9-18-call-phase-diagnostic-receipt-v1'
    taskId = $CallPhaseDiagnosticTaskId; diagnosticOnly = $true
    status = [string]$manifest.callPhaseDiagnostic.status
    routeAcceptanceTaskId = $TaskId; routeAcceptanceStatus = [string]$manifest.status
    routeRunId = $runId; routeManifestPath = [IO.Path]::GetFullPath($manifestPath)
    routeManifestSha256 = if (Test-Path -LiteralPath $manifestPath -PathType Leaf) { Get-P914FileSha256 $manifestPath } else { $null }
    baseBuildReceiptPath = [IO.Path]::GetFullPath($BuildReceiptPath)
    baseBuildReceiptSha256 = if ($null -ne $diagnosticProvenance) { [string]$diagnosticProvenance.baseBuildReceiptSha256 } else { $null }
    productSourceSha256 = if ($null -ne $diagnosticProvenance) { [string]$diagnosticProvenance.productSourceSha256 } else { $null }
    releaseExecutablePath = if ($null -ne $diagnosticProvenance) { [string]$diagnosticProvenance.releaseExecutablePath } else { $null }
    releaseExecutableSha256 = if ($null -ne $diagnosticProvenance) { [string]$diagnosticProvenance.releaseExecutableSha256 } else { $null }
    updatedRunnerPath = if ($null -ne $diagnosticProvenance) { [string]$diagnosticProvenance.updatedRunnerPath } else { $null }
    updatedRunnerSha256 = if ($null -ne $diagnosticProvenance) { [string]$diagnosticProvenance.updatedRunnerSha256 } else { $null }
    focusedRegressionTestPath = if ($null -ne $diagnosticProvenance) { [string]$diagnosticProvenance.focusedRegressionTestPath } else { $null }
    focusedRegressionTestSha256 = if ($null -ne $diagnosticProvenance) { [string]$diagnosticProvenance.focusedRegressionTestSha256 } else { $null }
    traceSchema = $CallPhaseDiagnosticSchema
    traceRecordCount = [int]$manifest.callPhaseDiagnostic.samples.Count
    traceRecordsLocation = 'route manifest callPhaseDiagnostic.samples'
}
Write-P914Json $diagnosticReceiptPath $diagnosticReceipt
if ($manifest.status -cne 'passed') { Write-Error "P9-18 route did not pass. Evidence: $manifestPath"; exit 1 }
Write-Host (ConvertTo-Json -Compress ([ordered]@{ taskId = $TaskId; status = [string]$manifest.status; runId = $runId; manifest = [IO.Path]::GetFullPath($manifestPath); diagnosticReceipt = [IO.Path]::GetFullPath($diagnosticReceiptPath); diagnosticReceiptSha256 = Get-P914FileSha256 $diagnosticReceiptPath; buildReceipt = $BuildReceiptPath; measuredCycleCount = 10; memoryWindowCount = [int]$manifest.memory.windows.Count; validation = $manifest.validation }))
