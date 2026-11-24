<#
.SYNOPSIS
Runs the P9-17 packaged Windows x64 Release large-roster editing route.

.DESCRIPTION
Verifies a source-bound Windows x64 Release build receipt, stages the current
executable and generated resources into a fresh package, prepares a pinned
100-row/156-column roster fixture, and drives one natural-exit application
process through workspace open, Classes navigation, roster edits, save,
suspension, re-entry, and persisted-value checks.
The opt-in -UiaObserverDiagnostic mode runs matched full-observer and
reduced-observer diagnostic arms. The separate
-ClientGcReferenceDiagnostic mode compares client-reference collection
boundaries between two full-observer arms. The separate -PayloadDiagnostic
mode compares filled and sparse custom-cell fixtures using two full-observer
ten-cycle arms. Diagnostic arms are never acceptance runs.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = '',
    [string]$EvidenceRoot = 'artifacts/phase9/packaged-next-p9-17-large-roster',
    [string]$BuildDirectory = 'build/windows-x64-release',
    [string]$ReleaseBuildReceiptPath = '',
    [string]$PostValidateManifestPath = '',
    [switch]$HeapSnapshotDiagnostic,
    [string]$HeapDiagnosticEvidenceRoot = 'artifacts/phase9/p9-17-heap-snapshot-diagnostic',
    [switch]$DefaultProcessHeapDiagnostic,
    [string]$DefaultProcessHeapDiagnosticEvidenceRoot = 'artifacts/p9h',
    [switch]$UiaObserverDiagnostic,
    [ValidateSet('FullThenReduced', 'ReducedThenFull', '')][string]$UiaObserverDiagnosticOrder = '',
    [string]$UiaObserverDiagnosticEvidenceRoot = 'artifacts/phase9/p9-17-uia-observer-diagnostic',
    [switch]$ClientGcReferenceDiagnostic,
    [ValidateSet('NoGcThenGc', 'GcThenNoGc', '')][string]$ClientGcReferenceDiagnosticOrder = '',
    [string]$ClientGcReferenceDiagnosticEvidenceRoot = 'artifacts/phase9/p9-17-client-gc-reference-diagnostic',
    [switch]$PayloadDiagnostic,
    [string]$PayloadDiagnosticEvidenceRoot = 'artifacts/phase9/p9-17-payload-diagnostic',
    [switch]$RosterRecordCountDiagnostic,
    [string]$RosterRecordCountDiagnosticEvidenceRoot = 'artifacts/phase9/p9-17-roster-record-count-diagnostic',
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(30, 180)][int]$ExitTimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Get-P917UiaObserverOrderPlan([string]$OrderSelection) {
    if ([string]::IsNullOrEmpty($OrderSelection) -or $OrderSelection -ieq 'FullThenReduced') {
        return [pscustomobject]@{
            selection = 'FullThenReduced'
            modes = [string[]]@('full-observer', 'reduced-observer')
        }
    }
    if ($OrderSelection -ieq 'ReducedThenFull') {
        return [pscustomobject]@{
            selection = 'ReducedThenFull'
            modes = [string[]]@('reduced-observer', 'full-observer')
        }
    }
    throw "Unsupported UIA observer diagnostic order: $OrderSelection"
}
function Get-P917ClientGcReferenceOrderPlan([string]$OrderSelection) {
    if ([string]::IsNullOrEmpty($OrderSelection) -or $OrderSelection -ieq 'NoGcThenGc') {
        return [pscustomobject]@{
            selection = 'NoGcThenGc'
            modes = [string[]]@('no-gc', 'client-gc')
        }
    }
    if ($OrderSelection -ieq 'GcThenNoGc') {
        return [pscustomobject]@{
            selection = 'GcThenNoGc'
            modes = [string[]]@('client-gc', 'no-gc')
        }
    }
    throw "Unsupported client GC reference diagnostic order: $OrderSelection"
}
$UiaObserverDiagnosticOrderPlan = $null
$ClientGcReferenceDiagnosticOrderPlan = $null
$PayloadDiagnosticOrderPlan = $null
$RosterRecordCountDiagnosticOrderPlan = $null
if ($UiaObserverDiagnostic -and $ClientGcReferenceDiagnostic) {
    throw 'UIA observer and client GC reference diagnostics must run as separate comparisons.'
}
if ($PayloadDiagnostic -and (
    $HeapSnapshotDiagnostic -or $DefaultProcessHeapDiagnostic -or
    $UiaObserverDiagnostic -or $ClientGcReferenceDiagnostic -or
    -not [string]::IsNullOrWhiteSpace($PostValidateManifestPath)
)) {
    throw 'Payload diagnostics must run separately from other diagnostic and post-validation modes.'
}
if (-not $PayloadDiagnostic -and $PSBoundParameters.ContainsKey('PayloadDiagnosticEvidenceRoot')) {
    throw '-PayloadDiagnosticEvidenceRoot can only be used with -PayloadDiagnostic.'
}
if ($RosterRecordCountDiagnostic) {
    $result = Invoke-P917RosterRecordCountDiagnosticComparison
} elseif ($PayloadDiagnostic) {
    $PayloadDiagnosticOrderPlan = [pscustomobject]@{
        selection = 'FilledThenSparse'
        modes = [string[]]@('filled', 'sparse')
    }
}
if ($RosterRecordCountDiagnostic -and (
    $PayloadDiagnostic -or $HeapSnapshotDiagnostic -or $DefaultProcessHeapDiagnostic -or
    $UiaObserverDiagnostic -or $ClientGcReferenceDiagnostic -or
    -not [string]::IsNullOrWhiteSpace($PostValidateManifestPath)
)) {
    throw 'Roster record-count diagnostics must run separately from other diagnostic and post-validation modes.'
}
if (-not $RosterRecordCountDiagnostic -and $PSBoundParameters.ContainsKey('RosterRecordCountDiagnosticEvidenceRoot')) {
    throw '-RosterRecordCountDiagnosticEvidenceRoot can only be used with -RosterRecordCountDiagnostic.'
}
if ($RosterRecordCountDiagnostic) {
    $RosterRecordCountDiagnosticOrderPlan = [pscustomobject]@{
        selection = 'FilledThenReducedRows'
        modes = [string[]]@('filled', 'reduced-rows')
    }
}
if ($DefaultProcessHeapDiagnostic -and (
    $HeapSnapshotDiagnostic -or $UiaObserverDiagnostic -or
    $ClientGcReferenceDiagnostic -or
    -not [string]::IsNullOrWhiteSpace($PostValidateManifestPath)
)) {
    throw 'Default process heap summary diagnostics must run separately from other diagnostic and post-validation modes.'
}
if (-not $DefaultProcessHeapDiagnostic -and
    $PSBoundParameters.ContainsKey('DefaultProcessHeapDiagnosticEvidenceRoot')) {
    throw '-DefaultProcessHeapDiagnosticEvidenceRoot can only be used with -DefaultProcessHeapDiagnostic.'
}
if ($UiaObserverDiagnostic) {
    $UiaObserverDiagnosticOrderPlan = Get-P917UiaObserverOrderPlan $UiaObserverDiagnosticOrder
    $UiaObserverDiagnosticOrder = [string]$UiaObserverDiagnosticOrderPlan.selection
} elseif (-not [string]::IsNullOrEmpty($UiaObserverDiagnosticOrder)) {
    throw '-UiaObserverDiagnosticOrder can only be used with -UiaObserverDiagnostic.'
}
if ($ClientGcReferenceDiagnostic) {
    if ($HeapSnapshotDiagnostic) { throw 'Client GC reference comparison cannot be combined with the heap snapshot diagnostic.' }
    if (-not [string]::IsNullOrWhiteSpace($PostValidateManifestPath)) { throw 'Client GC reference comparison cannot be combined with post-validation mode.' }
    $ClientGcReferenceDiagnosticOrderPlan = Get-P917ClientGcReferenceOrderPlan $ClientGcReferenceDiagnosticOrder
    $ClientGcReferenceDiagnosticOrder = [string]$ClientGcReferenceDiagnosticOrderPlan.selection
} elseif ($PSBoundParameters.ContainsKey('ClientGcReferenceDiagnosticOrder') -or
    $PSBoundParameters.ContainsKey('ClientGcReferenceDiagnosticEvidenceRoot')) {
    throw 'Client GC reference diagnostic parameters can only be used with -ClientGcReferenceDiagnostic.'
}
$TaskId = 'P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING'
$Schema = 'classmngr-phase9-p9-17-packaged-large-roster-v1'
$WorkingSetTarget = [Int64]268435456
$WorkingSetCeiling = [Int64]471859200
$OperationPeakAdvisoryLimit = [Int64]536870912
$BaseWorkspaceRelativePath = 'artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1/workspace-snapshots/master-blank.tps'
$OwnedSources = @(
    'scripts/phase9/prepare_p9_17_large_roster.py',
    'scripts/phase9/test_prepare_p9_17_large_roster.py',
    'scripts/phase9/p9_17_build_current_source_release.ps1',
    'scripts/phase9/run_packaged_next_p9_17_large_roster.ps1',
    'scripts/phase9/p9_17_heap_snapshot_elevated_worker.ps1',
    'scripts/phase9/validate_packaged_next_p9_17_large_roster.py',
    'scripts/phase9/test_validate_packaged_next_p9_17_large_roster.py'
)
$HarnessSources = @($OwnedSources + 'scripts/phase9/p9_14_packaged_lifecycle_common.ps1')
$P916ManifestRelativePath = 'artifacts/phase9/packaged-next-p9-16-repeated-workspace/20261121T023320451Z-9b576dcbfdc84dd0996f279db5ef67e9/manifest.json'
$P917TestOnlySourceBoundaryEvidence = @(
    [ordered]@{ path = 'cmake/next_app/CMakeLists.txt'; lines = '17-22,120-143'; finding = 'The standalone-test source change is absent from the ClassMngrNext production executable source and link closure.' },
    [ordered]@{ path = 'cmake/next_resources.cmake'; lines = '21-58,172,258-263,367-379'; finding = 'The standalone-test source change is absent from application resource inputs and installed package resources.' }
)
$P917AuthorizedTestOnlySourceChanges = @(
    [ordered]@{ path = 'tests/next/sub_prep_page_tests.cpp'; beforeSizeBytes = [Int64]20782; beforeSha256 = '70f5f2ff168432ea558729442cf727b15c8295ec439687c78ee51e4352943516'; afterSizeBytes = [Int64]20782; afterSha256 = 'af4f8325afdec691fa8c6130d4e31540b6f4d91919114ee395b575c5668862ae'; target = 'ClassMngrNextSubPrepPageTests'; cmakeLines = '4014-4023,4059-4070' },
    [ordered]@{ path = 'tests/next/shared_staff_directory_visual_capture.cpp'; beforeSizeBytes = [Int64]31510; beforeSha256 = 'b0a216be29787583e84093f0acf5e987bbca38f05c4515d057f3822ca41d0d78'; afterSizeBytes = [Int64]31518; afterSha256 = '916b828dba3b01ea7b3748ca6dc5f471e3265fa092b2f27bc228043ef68ad6a3'; target = 'ClassMngrNextSharedStaffDirectoryVisualCapture'; cmakeLines = '997-998,1035-1036' },
    [ordered]@{ path = 'tests/next/korean_teacher_directory_visual_capture.cpp'; beforeSizeBytes = [Int64]37762; beforeSha256 = 'b68062eef5318a151ed9d25527f9c2b0bf3ae22f0950bcc3144b5fb77369893d'; afterSizeBytes = [Int64]37778; afterSha256 = 'f69e1ca39365e50f111c3bfb81cbe56aa6ac665a75c3ba414f7651361fb56c89'; target = 'ClassMngrNextTeacherDirectoryVisualCapture'; cmakeLines = '1085-1086,1144-1145' },
    [ordered]@{ path = 'tests/next/upcoming_birthdays_visual_capture.cpp'; beforeSizeBytes = [Int64]20421; beforeSha256 = '65c42f543a550c27d763d80bdff914854647f175843f5b7df1b10f5ac2488221'; afterSizeBytes = [Int64]20429; afterSha256 = 'cc59fae5e2886aa48804acd0d0e50e4ba4cab7371cb44e26eb8244f97a40f3c9'; target = 'ClassMngrNextUpcomingBirthdaysVisualCapture'; cmakeLines = '5281-5282,5310-5311' },
    [ordered]@{ path = 'cmake/tests/next.cmake'; beforeSizeBytes = [Int64]296864; beforeSha256 = '675e7a1fc7c2877074108fc05ed4fbe6937bd058132d77aeba5c5059e289bc8e'; afterSizeBytes = [Int64]296959; afterSha256 = '6ba82e3c77e84b0ff425f2c071d9119d71a0b29895a7ee9579d6b17aa801c6dd'; target = 'ClassMngrNextRosterEditorTests'; cmakeLines = '4398-4406' },
    [ordered]@{ path = 'tests/next/page_host_tests.cpp'; beforeSizeBytes = [Int64]92258; beforeSha256 = '3bf6863e0a73bc96ef62f5aefefa3b493a3d54529cd82398a20eb2e16fd5353f'; afterSizeBytes = [Int64]105901; afterSha256 = '9dd0a582970000707ec805a25530d44f7049eea653f4c8b1d1b5875ca43a56c4'; target = 'ClassMngrNextPageHostTests'; cmakeLines = '843-894' },
    [ordered]@{ path = 'tests/next/roster_editor_tests.cpp'; beforeSizeBytes = [Int64]89727; beforeSha256 = '5f810ba8d2d3138e3a323ac5f791b00652772dd2c41993412b86255348357ad6'; afterSizeBytes = [Int64]126850; afterSha256 = 'b02cb7f7e7fc294643d94287df8a9ce6c146ecbd57a02279b6f22919ac98dc08'; target = 'ClassMngrNextRosterEditorTests'; cmakeLines = '4398-4446' },
    [ordered]@{ path = 'tests/next_application_roster_save_use_case_tests.cpp'; beforeSizeBytes = [Int64]26048; beforeSha256 = '70980d2f4529b7a9dfeea89a160e1429f63797c55935baf575a30465e836695c'; afterSizeBytes = [Int64]35484; afterSha256 = 'b5a3c4b93b11a46946567678e4e12ce0e1004652ecd4dfd7f18d15b311c8c426'; target = 'ClassMngrNextApplicationRosterSaveUseCaseTests'; cmakeLines = '5937-5944' },
    [ordered]@{ path = 'tests/next/classes_navigator_page_tests.cpp'; beforeSizeBytes = [Int64]111698; beforeSha256 = '88bcd4267dfa89b8b7b5fadeb43e6ab5af90833fddcb15cf8c0f02fc37ba4fc0'; afterSizeBytes = [Int64]118993; afterSha256 = '415c57ba38205959e438f2bfbe88baabcbd9b8d4406c0b3e8dc66e9202b6cdc2'; target = 'ClassMngrNextClassesNavigatorPageTests'; cmakeLines = '4264-4277' },
    [ordered]@{ path = 'tests/next_platform_application_services_roster_save_port_tests.cpp'; beforeSizeBytes = [Int64]38542; beforeSha256 = '471e010aac3912c2cdf140dbd18f8097cf69a3e146c107dbeb25ca76c7ac1dca'; afterSizeBytes = [Int64]46186; afterSha256 = '2694a4d2d77640481e5e53c20f98669d002069a3f01a758e23e8c30862e9cbbb'; target = 'ClassMngrNextPlatformApplicationServicesRosterSavePortTests'; cmakeLines = '7308-7316' }
)
$P917AuthorizedProductSourceChanges = @(
    [ordered]@{ path = 'src/core/process_memory_snapshot.cpp'; beforeSizeBytes = [Int64]8776; beforeSha256 = '3b5cf0737340dbb6b308c9df9e517d853944602bbe71f189ff7266b519d057c4'; afterSizeBytes = [Int64]11156; afterSha256 = '8600b0c07259ccee97da754a36a60fcfbec853e253eae5e53886e5e6471a1be7' },
    [ordered]@{ path = 'src/core/process_memory_snapshot.h'; beforeSizeBytes = [Int64]2397; beforeSha256 = '812976afbc797872c22087c39018302f0947eb0d490fcf844fec9a556d5226cc'; afterSizeBytes = [Int64]3025; afterSha256 = '8fd8731c3c4d52a370ea7ac308fc014b16ed5b18efc51fb1506efae76c30aca4' },
    [ordered]@{ path = 'src/data/repositories/roster_repository.cpp'; beforeSizeBytes = [Int64]146469; beforeSha256 = '41c594afab83cbe6635cadc7f4c800e125843fb2336ed66b63c8ae6d33bbf719'; afterSizeBytes = [Int64]154045; afterSha256 = 'b4805038b7589b18eaad8c7eee93b4424af187ce652521f44e24cbcc220b0fdb' },
    [ordered]@{ path = 'src/data/repositories/roster_repository.h'; beforeSizeBytes = [Int64]11811; beforeSha256 = '2081c277989f19cffcdbcf063efaaf0935ed1e285401263dcb5a9b746c5c4bd7'; afterSizeBytes = [Int64]13045; afterSha256 = '7e0185a49c19184099c581ff26b412c807a9a4a176f5043693e803a683c0fbce' },
    [ordered]@{ path = 'src/next/bootstrap/application_bootstrap.cpp'; beforeSizeBytes = [Int64]311829; beforeSha256 = 'd16a8768306778ca88aa6067066311827f8e0811fc79772282a19599ca812edd'; afterSizeBytes = [Int64]316874; afterSha256 = '7a166e8674496ad42333d28418d5c0508e2236445c27258d1665ad912e396d7b' },
    [ordered]@{ path = 'src/next/bootstrap/qt_page_lifecycle_diagnostics.cpp'; beforeSizeBytes = [Int64]28531; beforeSha256 = '32548730b42428addc4330b6c978b08a91d97d76cd9e169408f4b3cddcf45865'; afterSizeBytes = [Int64]38085; afterSha256 = '8488ccfddc6438f27597c20b6eba24a998a21e8ace2f91961e0261a2317dd354' },
    [ordered]@{ path = 'src/next/bootstrap/qt_page_lifecycle_diagnostics.h'; beforeSizeBytes = [Int64]4968; beforeSha256 = '7df91471cbef0a211db5d169cc6955c7bc8b5278f8a7f425318818ee134e07bc'; afterSizeBytes = [Int64]7140; afterSha256 = '05071ab23f76d38c45696414cfd56af840ef5936a0d3af0ee1b73ebed95fc046' },
    [ordered]@{ path = 'src/next/application/roster_save_preparation.h'; beforeSizeBytes = [Int64]19568; beforeSha256 = '196e28c3b42dd20906e6c6f84b1e54c6e0ececd2716f008c220cf51de64a889a'; afterSizeBytes = [Int64]24741; afterSha256 = '7ab0e34ddd5dfd44425226cf9558377d92aaf0dbba1e9e49fe5a3ed0d72afba5' },
    [ordered]@{ path = 'src/next/application/roster_save_use_case.h'; beforeSizeBytes = [Int64]5554; beforeSha256 = 'c3ce3b174398f27f2f77b943b9213dcc97a30af6ba2ea714a80157bbba245e66'; afterSizeBytes = [Int64]7537; afterSha256 = '1dad3866a1405ae8e1c493cb4c3009f862680c6bd789d7a7beda6eb046d23984' },
    [ordered]@{ path = 'src/next/platform/workspace_store_roster_actions.cpp'; beforeSizeBytes = [Int64]33903; beforeSha256 = '822f5415a5d3a7b7d1dab9ea54b53e0cdf662bd9e327bfe16505c0107ac50993'; afterSizeBytes = [Int64]38957; afterSha256 = '2637b3fb8a07a6b399fc19ab4bb2e9170131a6e05839f3d2ec94ae62f2e76443' },
    [ordered]@{ path = 'src/next/platform/workspace_store_roster_actions.h'; beforeSizeBytes = [Int64]1541; beforeSha256 = '9c5d440ce9459cca08ce92297cd54e7437803e49a24e2393617a8ab669c1521c'; afterSizeBytes = [Int64]1756; afterSha256 = 'e86ca771dcc5143466047a0c2c6740e8601cdaca38c93708e70e4f18195e1825' },
    [ordered]@{ path = 'src/next/ui/roster_editor_widget.cpp'; beforeSizeBytes = [Int64]53975; beforeSha256 = '83f3879273819377efd3994d6ff009d4343635b9471784132238d75a390138aa'; afterSizeBytes = [Int64]58395; afterSha256 = '58f09304b094cd4be93b4e3ed71def9b108304d59128ef8c77bc5aa954b53dae' },
    [ordered]@{ path = 'src/next/ui/roster_editor_widget.h'; beforeSizeBytes = [Int64]5292; beforeSha256 = 'c190060c12c6c9dcff8cec7f9b3f26b0229156fce41e079a0b844bccd1200b48'; afterSizeBytes = [Int64]6658; afterSha256 = 'b9852b472f9dde4d10be3b1b46b77a0c4546af6c216ba7522803b513e4a44639' },
    [ordered]@{ path = 'src/next/ui/roster_table_model.cpp'; beforeSizeBytes = [Int64]24480; beforeSha256 = '98678d18665d6cce5de53413f9c0ba9a145dc3537da6787cad2feb0480a13192'; afterSizeBytes = [Int64]28080; afterSha256 = '4f0d56113ec8113f4ec2a81e91d8279162fa46911b3c268bc1bd8ffe0a0d8ff4' },
    [ordered]@{ path = 'src/next/ui/roster_table_model.h'; beforeSizeBytes = [Int64]3633; beforeSha256 = '5087fb8af1d81b06fa726a8ea0b7759ea078c44649607b1ba94836b92f162f33'; afterSizeBytes = [Int64]3745; afterSha256 = '1184aa2156364f0af6adcfb7d14754c07029fda7a3da1a158b56f51c0e98ea64' },
    [ordered]@{ path = 'src/next/ui/selected_class_editor.cpp'; beforeSizeBytes = [Int64]82573; beforeSha256 = 'da5e2d80217c684b2b81a509ff72bb8f4162eb560a387e5bd9230772c8ee13f4'; afterSizeBytes = [Int64]83736; afterSha256 = '63f6e66755ba9c9ac33f6ce50ccbd7bfe7a67d9a292bafc0faea36786598fc52' },
    [ordered]@{ path = 'src/next/ui/selected_class_editor.h'; beforeSizeBytes = [Int64]10470; beforeSha256 = 'ea871923bc440ea7b1e0fa4bca699386eb09662c3765398e01df5ab57ba293ff'; afterSizeBytes = [Int64]10631; afterSha256 = '2bbdb7cdf6c9680817573be97fa4987e9b916710b0d3a6eca9294f9cc8f194c3' }
)
$P917AuthorizedHarnessSourceChanges = @(
    [ordered]@{ path = 'scripts/phase9/p9_14_packaged_lifecycle_common.ps1'; beforeSizeBytes = [Int64]176010; beforeSha256 = '868d92279cd14fea6eefdcac1cc35a554a28084e9932efb140581ed60dd7d263'; afterSizeBytes = [Int64]176293; afterSha256 = '24170ce41bcf03bdcc7c897d815dba716c729cede910b44a53e80913e829ae8c' }
)
$P914Sources = @(
    'scripts/phase9/p9_14_packaged_lifecycle_common.ps1',
    'scripts/phase9/prepare_p9_14_staff_ai_workspaces.py',
    'scripts/phase9/run_packaged_next_p9_14_staff_directory_lifecycle.ps1',
    'scripts/phase9/run_packaged_next_p9_14_ai_batch_review_lifecycle.ps1',
    'scripts/phase9/validate_packaged_next_p9_14_lifecycle.py',
    'scripts/phase9/test_validate_packaged_next_p9_14_lifecycle.py'
)
$P915Sources = @(
    'scripts/phase9/prepare_p9_15_schedule_workspace.py',
    'scripts/phase9/run_packaged_next_p9_15_schedule_locale_theme.ps1',
    'scripts/phase9/validate_packaged_next_p9_15_schedule_locale_theme.py',
    'scripts/phase9/test_validate_packaged_next_p9_15_schedule_locale_theme.py'
)
$P916Sources = @(
    'scripts/phase9/prepare_p9_16_workspace.py',
    'scripts/phase9/test_prepare_p9_16_workspace.py',
    'scripts/phase9/run_packaged_next_p9_16_repeated_workspace.ps1',
    'scripts/phase9/validate_packaged_next_p9_16_repeated_workspace.py',
    'scripts/phase9/test_validate_packaged_next_p9_16_repeated_workspace.py'
)
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot '../..' }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$BuildDirectory = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory)) }
$P917StandardBuildDirectory = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'build/windows-x64-release'))
$P917SaveStageDiagnosticBuildDirectory = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'build/p9-17-save-stage-diagnostic-release'))
$P917ExpectedReleaseGenerator = 'Visual Studio 17 2022'
$P917ExpectedReleasePlatform = 'x64'
$P917ExpectedReleaseToolset = 'v143'
$P917ExpectedReleaseGeneratorInstance = [IO.Path]::GetFullPath('C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools')
$P917ExpectedReleaseToolsetVersion = '14.44.35207'
$P917ExpectedReleaseCompilerVersion = '19.44.35230.0'
$P917ExpectedReleaseWindowsSdkVersion = '10.0.26100.0'
if ($env:OS -cne 'Windows_NT') { throw 'P9-17 packaged UIA route must run locally on Windows.' }
. (Join-Path $PSScriptRoot 'p9_14_packaged_lifecycle_common.ps1')

function Test-P917JsonInteger {
    param([object]$Value)
    if ($null -eq $Value -or $Value -is [bool]) { return $false }
    $valueType = $Value.GetType()
    return $valueType -eq [sbyte] -or $valueType -eq [byte] -or
        $valueType -eq [int16] -or $valueType -eq [uint16] -or
        $valueType -eq [int32] -or $valueType -eq [uint32] -or
        $valueType -eq [int64] -or $valueType -eq [uint64]
}

if (-not ('P917ExecutionContextNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Principal;

public sealed class P917ExecutionContextFacts
{
    public string Identity { get; set; }
    public string UserSid { get; set; }
    public bool IsElevated { get; set; }
    public bool IsAdministratorRole { get; set; }
    public int IntegrityLevelRid { get; set; }
    public string IntegrityLevel { get; set; }
    public bool UserInteractive { get; set; }
    public int ProcessSessionId { get; set; }
    public uint ActiveConsoleSessionId { get; set; }
}

public static class P917ExecutionContextNative
{
    private const uint TokenQuery = 0x0008;
    private const int TokenElevationInformation = 20;
    private const int TokenIntegrityLevelInformation = 25;
    [StructLayout(LayoutKind.Sequential)] private struct TokenMandatoryLabel { public IntPtr Sid; public uint Attributes; }

    [DllImport("kernel32.dll")] private static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll", SetLastError=true)] private static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern uint WTSGetActiveConsoleSessionId();
    [DllImport("advapi32.dll", SetLastError=true)] private static extern bool OpenProcessToken(IntPtr process, uint desiredAccess, out IntPtr token);
    [DllImport("advapi32.dll", SetLastError=true)] private static extern bool GetTokenInformation(IntPtr token, int informationClass, IntPtr information, int informationLength, out int returnLength);
    [DllImport("advapi32.dll", SetLastError=true)] private static extern IntPtr GetSidSubAuthorityCount(IntPtr sid);
    [DllImport("advapi32.dll", SetLastError=true)] private static extern IntPtr GetSidSubAuthority(IntPtr sid, uint subAuthorityIndex);

    private static IntPtr ReadTokenInformation(IntPtr token, int informationClass, out int length)
    {
        int required;
        GetTokenInformation(token, informationClass, IntPtr.Zero, 0, out required);
        if (required <= 0) throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows did not return the required token-information size.");
        IntPtr buffer = Marshal.AllocHGlobal(required);
        if (!GetTokenInformation(token, informationClass, buffer, required, out length))
        {
            int error = Marshal.GetLastWin32Error(); Marshal.FreeHGlobal(buffer);
            throw new Win32Exception(error, "Windows could not read the requested token information.");
        }
        return buffer;
    }

    private static int ReadElevation(IntPtr token)
    {
        int length; IntPtr buffer = ReadTokenInformation(token, TokenElevationInformation, out length);
        try { return Marshal.ReadInt32(buffer); } finally { Marshal.FreeHGlobal(buffer); }
    }

    private static int ReadIntegrityRid(IntPtr token)
    {
        int length; IntPtr buffer = ReadTokenInformation(token, TokenIntegrityLevelInformation, out length);
        try
        {
            TokenMandatoryLabel label = (TokenMandatoryLabel)Marshal.PtrToStructure(buffer, typeof(TokenMandatoryLabel));
            if (label.Sid == IntPtr.Zero) throw new InvalidOperationException("The token integrity SID is null.");
            IntPtr countAddress = GetSidSubAuthorityCount(label.Sid);
            if (countAddress == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not read the token integrity SID subauthority count.");
            byte count = Marshal.ReadByte(countAddress);
            if (count == 0) throw new InvalidOperationException("The token integrity SID has no subauthorities.");
            IntPtr ridAddress = GetSidSubAuthority(label.Sid, (uint)(count - 1));
            if (ridAddress == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not read the token integrity SID level.");
            return Marshal.ReadInt32(ridAddress);
        }
        finally { Marshal.FreeHGlobal(buffer); }
    }

    private static string GetIntegrityLevelName(int integrityRid)
    {
        switch (integrityRid & 0xF000)
        {
            case 0x5000: return "ProtectedProcess";
            case 0x4000: return "System";
            case 0x3000: return "High";
            case 0x2000: return "Medium";
            case 0x1000: return "Low";
            default: return "Untrusted";
        }
    }

    public static P917ExecutionContextFacts Read()
    {
        IntPtr token;
        if (!OpenProcessToken(GetCurrentProcess(), TokenQuery, out token))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not open the observer process token for query.");
        try
        {
            using (WindowsIdentity identity = WindowsIdentity.GetCurrent())
            using (Process process = Process.GetCurrentProcess())
            {
                if (identity.User == null) throw new InvalidOperationException("The observer Windows token has no user SID.");
                int integrityRid = ReadIntegrityRid(token);
                WindowsPrincipal principal = new WindowsPrincipal(identity);
                return new P917ExecutionContextFacts {
                    Identity = identity.Name,
                    UserSid = identity.User.Value,
                    IsElevated = ReadElevation(token) != 0,
                    IsAdministratorRole = principal.IsInRole(WindowsBuiltInRole.Administrator),
                    IntegrityLevelRid = integrityRid,
                    IntegrityLevel = GetIntegrityLevelName(integrityRid),
                    UserInteractive = Environment.UserInteractive,
                    ProcessSessionId = process.SessionId,
                    ActiveConsoleSessionId = WTSGetActiveConsoleSessionId()
                };
            }
        }
        finally
        {
            if (!CloseHandle(token)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not close the queried observer token handle.");
        }
    }
}
'@
}

function Get-P917CurrentExecutionContext {
    $facts = [P917ExecutionContextNative]::Read()
    $context = [ordered]@{
        identity = [string]$facts.Identity
        userSid = [string]$facts.UserSid
        isElevated = [bool]$facts.IsElevated
        isAdministratorRole = [bool]$facts.IsAdministratorRole
        integrityLevelRid = [Int64]$facts.IntegrityLevelRid
        integrityLevel = [string]$facts.IntegrityLevel
        userInteractive = [bool]$facts.UserInteractive
        processSessionId = [Int64]$facts.ProcessSessionId
        activeConsoleSessionId = [Int64]$facts.ActiveConsoleSessionId
        observedAtUtc = Get-P914Utc
    }
    Assert-P917ExecutionContextRecord $context 'observer'
    return $context
}

function Assert-P917ExecutionContextRecord([object]$Context, [string]$Label) {
    if ([string]::IsNullOrWhiteSpace([string]$Context.identity) -or [string]::IsNullOrWhiteSpace([string]$Context.userSid)) { throw "$Label execution context lacks identity or SID evidence." }
    foreach ($field in @('isElevated', 'isAdministratorRole', 'userInteractive')) {
        if ($Context.$field -isnot [bool]) { throw "$Label execution context has a non-Boolean $field value." }
    }
    if ([bool]$Context.isElevated -or [bool]$Context.isAdministratorRole) { throw "$Label execution context is elevated or has the Administrator role." }
    if ([Int64]$Context.integrityLevelRid -ne 8192 -or [string]$Context.integrityLevel -cne 'Medium') { throw "$Label execution context is not actual Medium integrity." }
    if (-not [bool]$Context.userInteractive) { throw "$Label execution context is not UserInteractive." }
    if ([Int64]$Context.processSessionId -lt 1 -or [Int64]$Context.activeConsoleSessionId -ne [Int64]$Context.processSessionId) { throw "$Label process session does not match the active local console session." }
}

function Assert-P917SameExecutionContext([object]$Left, [object]$Right, [string]$Boundary) {
    foreach ($property in @('identity', 'userSid', 'isElevated', 'isAdministratorRole', 'integrityLevelRid', 'integrityLevel', 'userInteractive', 'processSessionId', 'activeConsoleSessionId')) {
        if ([string]$Left.$property -cne [string]$Right.$property) { throw "$Boundary execution context changed at '$property'." }
    }
}

function Assert-P917BuildObserverExecutionContext([object]$BuildContext, [object]$ObserverContext) {
    if ($null -eq $BuildContext -or $null -eq $BuildContext.start -or $null -eq $BuildContext.completion) { throw 'Release receipt omits the actual build execution-context observations.' }
    if ($BuildContext.sameIdentityAndConsoleSession -isnot [bool] -or -not [bool]$BuildContext.sameIdentityAndConsoleSession) { throw 'Release receipt does not verify stable identity and console session across the build.' }
    Assert-P917ExecutionContextRecord $BuildContext.start 'build-start'
    Assert-P917ExecutionContextRecord $BuildContext.completion 'build-completion'
    Assert-P917SameExecutionContext $BuildContext.start $BuildContext.completion 'Build'
    Assert-P917ExecutionContextRecord $ObserverContext 'observer'
    if ([string]$BuildContext.completion.userSid -cne [string]$ObserverContext.userSid) { throw 'Build and observer Windows user SIDs differ.' }
    if ([Int64]$BuildContext.completion.processSessionId -ne [Int64]$ObserverContext.processSessionId -or [Int64]$BuildContext.completion.activeConsoleSessionId -ne [Int64]$ObserverContext.activeConsoleSessionId) { throw 'Build and observer are not in the same active console session.' }
}

if (-not ('P917ScrollNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class P917ScrollNative
{
    private const uint WmMouseWheel = 0x020A;
    private const uint SmtoAbortIfHung = 0x0002;
    [StructLayout(LayoutKind.Sequential)] private struct Point { public int x; public int y; }
    [StructLayout(LayoutKind.Sequential)] private struct Rect { public int left; public int top; public int right; public int bottom; }
    [DllImport("user32.dll", SetLastError=true)] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool ScreenToClient(IntPtr window, ref Point point);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool GetClientRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", SetLastError=true)] private static extern IntPtr SendMessageTimeoutW(IntPtr window, uint message, UIntPtr wParam, IntPtr lParam, uint flags, uint timeoutMilliseconds, out IntPtr result);
    [DllImport("user32.dll", SetLastError=true)] private static extern uint WaitForInputIdle(IntPtr process, uint milliseconds);

    private static IntPtr PackCoordinates(int x, int y)
    {
        int packed = unchecked((int)(((uint)(ushort)x) | ((uint)(ushort)y << 16)));
        return new IntPtr(packed);
    }

    public static long[] ScrollListWindowMessage(IntPtr window, IntPtr process,
        int listLeft, int listTop, int listRight, int listBottom,
        int wheelDelta, uint expectedProcessId)
    {
        uint windowProcessId;
        if (window == IntPtr.Zero || GetWindowThreadProcessId(window, out windowProcessId) == 0)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed for the measured Classes list scroll.");
        if (windowProcessId != expectedProcessId)
            throw new InvalidOperationException("Classes list scroll HWND does not belong to the measured app PID.");
        if (wheelDelta != -120 || listRight <= listLeft || listBottom <= listTop)
            throw new InvalidOperationException("Classes list scroll request has invalid bounds or wheel direction.");
        uint idle = WaitForInputIdle(process, 10000);
        if (idle != 0)
            throw new InvalidOperationException("Packaged app did not reach input-idle before Classes list scrolling.");
        int screenX = listLeft + ((listRight - listLeft) / 2);
        int screenY = listTop + ((listBottom - listTop) / 2);
        Point point = new Point { x = screenX, y = screenY };
        if (!ScreenToClient(window, ref point))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed for the UIA-derived Classes list scroll point.");
        Rect rect;
        if (!GetClientRect(window, out rect))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetClientRect failed for the measured app window.");
        if (point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom)
            throw new InvalidOperationException("UIA-derived Classes list scroll point is outside the measured app client bounds.");
        uint wheelWord = unchecked((uint)(ushort)(short)wheelDelta) << 16;
        IntPtr ignoredResult;
        IntPtr callResult = SendMessageTimeoutW(window, WmMouseWheel, new UIntPtr(wheelWord),
            PackCoordinates(screenX, screenY), SmtoAbortIfHung, 2000, out ignoredResult);
        if (callResult == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "SendMessageTimeout did not deliver the Classes list wheel event.");
        return new long[] { idle, windowProcessId, screenX, screenY, point.x, point.y, rect.left, rect.top, rect.right, rect.bottom, wheelDelta, 1 };
    }
}
'@
}

if (-not ('P917ClickGeometryNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Runtime.ExceptionServices;

public sealed class P917PhysicalWindowGeometry
{
    public long WindowHandle;
    public long ProcessId;
    public bool IsVisible;
    public int Left;
    public int Top;
    public int Width;
    public int Height;
    public long OriginalThreadDpiAwarenessContext;
    public int OriginalThreadDpiAwareness;
    public long WindowDpiAwarenessContext;
    public int WindowDpiAwareness;
    public uint WindowDpi;
    public uint SystemDpi;
    public string BoundsReadDpiAwareness;
}

public sealed class P917WindowFromPointDiagnostic
{
    public long WindowHandle;
    public long ExpectedProcessId;
    public bool ProcessIdAvailable;
    public long ProcessId;
    public bool VisibilityAvailable;
    public bool IsVisible;
    public bool PhysicalBoundsAvailable;
    public int Left;
    public int Top;
    public int Width;
    public int Height;
    public int ScreenX;
    public int ScreenY;
    public string ProcessIdReadError;
    public string VisibilityReadError;
    public string PhysicalBoundsReadError;
    public string ReadContext;
}

public static class P917ClickGeometryNative
{
    private const uint SwpNoSize = 0x0001;
    private const uint SwpNoMove = 0x0002;
    private const uint SwpNoActivate = 0x0010;
    private const uint WmMouseMove = 0x0200;
    private const uint WmLeftButtonDown = 0x0201;
    private const uint WmLeftButtonUp = 0x0202;
    [StructLayout(LayoutKind.Sequential)] private struct NativeRect { public int Left; public int Top; public int Right; public int Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct NativePoint { public int X; public int Y; }
    [DllImport("user32.dll", SetLastError=true)] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool GetWindowRect(IntPtr window, out NativeRect rectangle);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr WindowFromPoint(NativePoint point);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool ScreenToClient(IntPtr window, ref NativePoint point);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr GetThreadDpiAwarenessContext();
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr GetWindowDpiAwarenessContext(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern int GetAwarenessFromDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll", SetLastError=true)] private static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern uint GetDpiForSystem();

    private static uint RequireProcess(IntPtr window, uint expectedProcessId)
    {
        uint actualProcessId;
        if (window == IntPtr.Zero || GetWindowThreadProcessId(window, out actualProcessId) == 0)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed for the P9-17 click owner.");
        if (actualProcessId != expectedProcessId)
            throw new InvalidOperationException("P9-17 click owner HWND does not belong to the measured app PID.");
        return actualProcessId;
    }

    private static P917WindowFromPointDiagnostic CaptureWindowFromPointDiagnostic(IntPtr window, int screenX, int screenY, uint expectedProcessId)
    {
        P917WindowFromPointDiagnostic diagnostic = new P917WindowFromPointDiagnostic {
            WindowHandle = window.ToInt64(), ExpectedProcessId = expectedProcessId,
            ScreenX = screenX, ScreenY = screenY, ReadContext = "PER_MONITOR_AWARE_V2"
        };
        if (window == IntPtr.Zero)
            return diagnostic;

        try
        {
            uint processId;
            if (GetWindowThreadProcessId(window, out processId) == 0)
                diagnostic.ProcessIdReadError = new Win32Exception(Marshal.GetLastWin32Error()).Message;
            else
            {
                diagnostic.ProcessIdAvailable = true;
                diagnostic.ProcessId = processId;
            }
        }
        catch (Exception ex) { diagnostic.ProcessIdReadError = ex.Message; }

        try
        {
            diagnostic.IsVisible = IsWindowVisible(window);
            diagnostic.VisibilityAvailable = true;
        }
        catch (Exception ex) { diagnostic.VisibilityReadError = ex.Message; }

        try
        {
            NativeRect rectangle;
            if (!GetWindowRect(window, out rectangle))
                diagnostic.PhysicalBoundsReadError = new Win32Exception(Marshal.GetLastWin32Error()).Message;
            else
            {
                diagnostic.Left = rectangle.Left;
                diagnostic.Top = rectangle.Top;
                diagnostic.Width = rectangle.Right - rectangle.Left;
                diagnostic.Height = rectangle.Bottom - rectangle.Top;
                diagnostic.PhysicalBoundsAvailable = true;
            }
        }
        catch (Exception ex) { diagnostic.PhysicalBoundsReadError = ex.Message; }
        return diagnostic;
    }

    private static void ThrowWithDpiRestoreFailure(Exception operationFailure, Exception restoreFailure)
    {
        if (operationFailure != null)
        {
            if (restoreFailure != null)
            {
                operationFailure.Data["P917PrimaryOperationFailureMessage"] = operationFailure.Message;
                operationFailure.Data["DpiContextRestoreFailureMessage"] = restoreFailure.Message;
                operationFailure.Data["DpiContextRestoreFailure"] = restoreFailure;
            }
            ExceptionDispatchInfo.Capture(operationFailure).Throw();
            return;
        }
        if (restoreFailure != null)
        {
            restoreFailure.Data["P917PrimaryOperationFailureMessage"] = restoreFailure.Message;
            restoreFailure.Data["DpiContextRestoreFailureMessage"] = restoreFailure.Message;
            ExceptionDispatchInfo.Capture(restoreFailure).Throw();
            return;
        }
    }

    public static long[] ReadDpiAwarenessDetails(IntPtr window)
    {
        IntPtr threadContext = GetThreadDpiAwarenessContext();
        IntPtr windowContext = GetWindowDpiAwarenessContext(window);
        if (threadContext == IntPtr.Zero || windowContext == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not read DPI-awareness diagnostic context.");
        return new long[] { threadContext.ToInt64(), GetAwarenessFromDpiAwarenessContext(threadContext),
            windowContext.ToInt64(), GetAwarenessFromDpiAwarenessContext(windowContext),
            GetDpiForWindow(window), GetDpiForSystem() };
    }

    public static object ReadWithPhysicalCoordinateContext(Func<object> operation)
    {
        if (operation == null)
            throw new ArgumentNullException("operation");
        // UIA bounds and the native owner rectangle must be read in the same physical DPI space.
        IntPtr priorContext = SetThreadDpiAwarenessContext(new IntPtr(-4));
        if (priorContext == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not set Per-Monitor-V2 DPI awareness for a physical UIA coordinate read.");
        object result = null;
        Exception operationFailure = null;
        try
        {
            result = operation();
        }
        catch (Exception ex)
        {
            operationFailure = ex;
        }
        Exception restoreFailure = null;
        try
        {
            if (SetThreadDpiAwarenessContext(priorContext) == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not restore the original thread DPI-awareness context after a physical UIA coordinate read.");
        }
        catch (Exception ex)
        {
            restoreFailure = ex;
        }
        ThrowWithDpiRestoreFailure(operationFailure, restoreFailure);
        return result;
    }

    public static P917WindowFromPointDiagnostic FindVisibleHostAtPhysicalScreenPoint(int screenX, int screenY, uint expectedProcessId)
    {
        IntPtr priorContext = SetThreadDpiAwarenessContext(new IntPtr(-4));
        if (priorContext == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not set Per-Monitor-V2 DPI awareness for handle-0 UIA host correlation.");
        IntPtr host = IntPtr.Zero;
        P917WindowFromPointDiagnostic diagnostic = null;
        Exception operationFailure = null;
        try
        {
            host = WindowFromPoint(new NativePoint { X = screenX, Y = screenY });
            diagnostic = CaptureWindowFromPointDiagnostic(host, screenX, screenY, expectedProcessId);
            RequireProcess(host, expectedProcessId);
            if (!IsWindowVisible(host))
                throw new InvalidOperationException("Physical point for the handle-0 UIA target did not resolve to a visible host HWND.");
            NativeRect rectangle;
            if (!GetWindowRect(host, out rectangle))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed for the handle-0 UIA target host under Per-Monitor-V2 awareness.");
            if (screenX < rectangle.Left || screenX >= rectangle.Right || screenY < rectangle.Top || screenY >= rectangle.Bottom)
                throw new InvalidOperationException("Physical point for the handle-0 UIA target is outside its hit-tested host HWND.");
        }
        catch (Exception ex)
        {
            operationFailure = ex;
            if (diagnostic != null)
            {
                try { ex.Data["P917WindowFromPointDiagnostic"] = diagnostic; } catch { }
            }
        }
        Exception restoreFailure = null;
        try
        {
            if (SetThreadDpiAwarenessContext(priorContext) == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not restore the original thread DPI-awareness context after handle-0 UIA host correlation.");
        }
        catch (Exception ex)
        {
            restoreFailure = ex;
            if (diagnostic != null)
            {
                try { restoreFailure.Data["P917WindowFromPointDiagnostic"] = diagnostic; } catch { }
            }
        }
        ThrowWithDpiRestoreFailure(operationFailure, restoreFailure);
        return diagnostic;
    }

    public static P917PhysicalWindowGeometry ReadPhysicalBounds(IntPtr window, uint expectedProcessId)
    {
        uint actualProcessId = RequireProcess(window, expectedProcessId);
        IntPtr originalContext = GetThreadDpiAwarenessContext();
        IntPtr windowContext = GetWindowDpiAwarenessContext(window);
        if (originalContext == IntPtr.Zero || windowContext == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not read the P9-17 click owner's DPI-awareness context.");
        int originalAwareness = GetAwarenessFromDpiAwarenessContext(originalContext);
        int windowAwareness = GetAwarenessFromDpiAwarenessContext(windowContext);
        IntPtr priorContext = SetThreadDpiAwarenessContext(new IntPtr(-4));
        if (priorContext == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not set Per-Monitor-V2 DPI awareness for physical click geometry.");
        NativeRect rectangle = new NativeRect();
        Exception operationFailure = null;
        try
        {
            if (!GetWindowRect(window, out rectangle))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed under Per-Monitor-V2 awareness.");
        }
        catch (Exception ex)
        {
            operationFailure = ex;
        }
        Exception restoreFailure = null;
        try
        {
            if (SetThreadDpiAwarenessContext(priorContext) == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not restore the original thread DPI-awareness context.");
        }
        catch (Exception ex)
        {
            restoreFailure = ex;
        }
        ThrowWithDpiRestoreFailure(operationFailure, restoreFailure);
        return new P917PhysicalWindowGeometry {
            WindowHandle = window.ToInt64(), ProcessId = actualProcessId, IsVisible = IsWindowVisible(window),
            Left = rectangle.Left, Top = rectangle.Top, Width = rectangle.Right - rectangle.Left, Height = rectangle.Bottom - rectangle.Top,
            OriginalThreadDpiAwarenessContext = originalContext.ToInt64(), OriginalThreadDpiAwareness = originalAwareness,
            WindowDpiAwarenessContext = windowContext.ToInt64(), WindowDpiAwareness = windowAwareness,
            WindowDpi = GetDpiForWindow(window), SystemDpi = GetDpiForSystem(), BoundsReadDpiAwareness = "PER_MONITOR_AWARE_V2"
        };
    }

    public static void ClickPhysicalScreenPoint(IntPtr window, uint expectedProcessId, int screenX, int screenY)
    {
        RequireProcess(window, expectedProcessId);
        IntPtr priorContext = SetThreadDpiAwarenessContext(new IntPtr(-4));
        if (priorContext == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not set Per-Monitor-V2 DPI awareness for the P9-17 click.");
        Exception operationFailure = null;
        try
        {
            NativeRect rectangle;
            if (!GetWindowRect(window, out rectangle))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed before the P9-17 click.");
            if (!IsWindowVisible(window) || screenX < rectangle.Left || screenX >= rectangle.Right || screenY < rectangle.Top || screenY >= rectangle.Bottom)
                throw new InvalidOperationException("P9-17 physical click point is outside its visible owner HWND.");
            if (!SetWindowPos(window, IntPtr.Zero, 0, 0, 0, 0, SwpNoSize | SwpNoMove | SwpNoActivate))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not raise the P9-17 click owner without activation.");
            NativePoint point = new NativePoint { X = screenX, Y = screenY };
            if (!ScreenToClient(window, ref point))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed for the P9-17 physical click point.");
            IntPtr packed = new IntPtr(unchecked((int)(((uint)(ushort)point.Y << 16) | (ushort)point.X)));
            if (!PostMessage(window, WmMouseMove, IntPtr.Zero, packed) ||
                !PostMessage(window, WmLeftButtonDown, new IntPtr(1), packed) ||
                !PostMessage(window, WmLeftButtonUp, IntPtr.Zero, packed))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "P9-17 physical click messages were not delivered.");
        }
        catch (Exception ex)
        {
            operationFailure = ex;
        }
        Exception restoreFailure = null;
        try
        {
            if (SetThreadDpiAwarenessContext(priorContext) == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not restore the original thread DPI-awareness context after clicking.");
        }
        catch (Exception ex)
        {
            restoreFailure = ex;
        }
        ThrowWithDpiRestoreFailure(operationFailure, restoreFailure);
    }
}
'@
}

function Get-P914MainRoot([object]$Context) {
    # Process.MainWindowHandle can switch to Qt's native QMenuBar HWND after
    # startup. Pin the verified QApplication shell handle once and keep using
    # that PID-bound window root for the remainder of this measured route.
    if ([Int64]$Context.mainWindowHandle -gt 0) {
        $handle = [IntPtr][Int64]$Context.mainWindowHandle
        if ([uint32][P914Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id -or -not [P914Native]::IsVisible($handle)) { throw 'Pinned Next main HWND is no longer visible and owned by the measured process.' }
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
        $current = $root.Current
        if ([Int64]$current.ProcessId -ne [Int64]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window' -or [string]$current.AutomationId -cne 'QApplication.classMngrNextApplicationShell' -or [string]$current.ClassName -cne 'QMainWindow') { throw 'Pinned UIA root no longer matches the measured Next application shell.' }
        $Context.mainWindowRoot = $root
        return $root
    }

    $matches = [Collections.Generic.List[object]]::new()
    foreach ($handle in [P914Native]::GetTopLevelWindows([uint32]$Context.process.Id)) {
        if (-not [P914Native]::IsVisible($handle)) { continue }
        if ([uint32][P914Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id) { continue }
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
        $current = $root.Current
        if ([Int64]$current.ProcessId -ne [Int64]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window' -or [string]$current.AutomationId -cne 'QApplication.classMngrNextApplicationShell') { continue }
        if ([string]$current.ClassName -cne 'QMainWindow') { throw 'Measured Next shell has an unexpected native window class.' }
        $matches.Add([ordered]@{ root = $root; handle = [Int64]$handle.ToInt64() })
    }
    if ($matches.Count -gt 1) { throw 'More than one exact Next application shell is visible in the measured process.' }
    if ($matches.Count -eq 0) { return $null }
    $Context.mainWindowHandle = [Int64]$matches[0].handle
    $Context.mainWindowRoot = $matches[0].root
    $Context.ownerHandles[(Get-P914RuntimeKey $Context.mainWindowRoot)] = [Int64]$Context.mainWindowHandle
    return $Context.mainWindowRoot
}

function Get-P917CacheValue([string]$CachePath, [string]$Key) {
    $matches = @(Select-String -LiteralPath $CachePath -Pattern ("^" + [regex]::Escape($Key) + ":[^=]+=") )
    if ($matches.Count -ne 1) { throw "Expected exactly one configured CMake cache entry '$Key'." }
    return ($matches[0].Line -split '=', 2)[1]
}

function Assert-P917V143ReleaseToolchain([object]$Receipt, [string]$CachePath) {
    if ([string]$Receipt.generator -cne $P917ExpectedReleaseGenerator -or [string]$Receipt.generatorPlatform -cne $P917ExpectedReleasePlatform -or [string]$Receipt.generatorToolset -cne $P917ExpectedReleaseToolset) {
        throw 'Release receipt does not prove the required Visual Studio 17 2022 x64 v143 toolchain.'
    }
    if ([string]::IsNullOrWhiteSpace([string]$Receipt.generatorInstance) -or [IO.Path]::GetFullPath([string]$Receipt.generatorInstance) -ine $P917ExpectedReleaseGeneratorInstance) {
        throw 'Release receipt does not identify the installed VS2022 Build Tools instance.'
    }
    if ([string]$Receipt.windowsSdkVersion -cne $P917ExpectedReleaseWindowsSdkVersion) {
        throw 'Release receipt does not prove the required Windows 10.0.26100.0 SDK.'
    }
    $compiler = $Receipt.compiler
    $expectedCompilerPath = [IO.Path]::GetFullPath((Join-Path $P917ExpectedReleaseGeneratorInstance "VC/Tools/MSVC/$P917ExpectedReleaseToolsetVersion/bin/Hostx64/x64/cl.exe"))
    if ($null -eq $compiler -or [IO.Path]::GetFullPath([string]$compiler.path) -ine $expectedCompilerPath -or [string]$compiler.fileVersion -cne $P917ExpectedReleaseCompilerVersion -or [string]$compiler.toolsetVersion -cne $P917ExpectedReleaseToolsetVersion -or [string]$compiler.windowsSdkVersion -cne $P917ExpectedReleaseWindowsSdkVersion -or [string]$compiler.sha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw 'Release receipt compiler identity or version is incomplete or does not match VS2022/v143.'
    }
    $generated = $Receipt.generatedProjectToolchain
    if ($null -eq $generated -or [string]$generated.platformToolset -cne $P917ExpectedReleaseToolset -or [string]$generated.windowsSdkVersion -cne $P917ExpectedReleaseWindowsSdkVersion -or -not (Test-P917JsonInteger -Value $generated.projectFileCount) -or [Int64]$generated.projectFileCount -lt 1) {
        throw 'Release receipt does not prove that generated projects selected v143 and the required Windows SDK.'
    }
    if ([string]$Receipt.build.actualCompilerPath -ine $expectedCompilerPath -or $Receipt.build.compilerObservedInLog -ne $true) {
        throw 'Release receipt does not prove that the verbose build invoked the expected v143 x64 compiler.'
    }
    $cacheGenerator = Get-P917CacheValue $CachePath 'CMAKE_GENERATOR'
    $cachePlatform = Get-P917CacheValue $CachePath 'CMAKE_GENERATOR_PLATFORM'
    $cacheToolset = Get-P917CacheValue $CachePath 'CMAKE_GENERATOR_TOOLSET'
    $cacheInstance = [IO.Path]::GetFullPath((Get-P917CacheValue $CachePath 'CMAKE_GENERATOR_INSTANCE'))
    if ($cacheGenerator -cne $P917ExpectedReleaseGenerator -or $cachePlatform -cne $P917ExpectedReleasePlatform -or $cacheToolset -cne $P917ExpectedReleaseToolset) {
        throw 'Current CMake cache is not configured for the required VS2022/v143 x64 toolchain.'
    }
    if ($cacheInstance -ine $P917ExpectedReleaseGeneratorInstance -or $cacheGenerator -cne [string]$Receipt.generator -or $cachePlatform -cne [string]$Receipt.generatorPlatform -or $cacheToolset -cne [string]$Receipt.generatorToolset -or $cacheInstance -ine [IO.Path]::GetFullPath([string]$Receipt.generatorInstance)) {
        throw 'Current CMake generator, platform, toolset, or instance differs from the exact-build receipt.'
    }
}

function Write-P917File([string]$Path, [object[]]$Lines) {
    $fullPath = [IO.Path]::GetFullPath($Path)
    $parentPath = Split-Path -Parent $fullPath
    if (-not [string]::IsNullOrWhiteSpace($parentPath)) {
        [void][IO.Directory]::CreateDirectory($parentPath)
    }
    $textLines = @($Lines | ForEach-Object { [string]$_ })
    $content = if ($textLines.Count -gt 0) {
        ($textLines -join [Environment]::NewLine) + [Environment]::NewLine
    } else {
        ''
    }
    [IO.File]::WriteAllText($fullPath, $content, [Text.UTF8Encoding]::new($false))
}

function Invoke-P917PackageCommand([string]$Path, [string[]]$Arguments, [string]$LogPath) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = [IO.Path]::GetFullPath($Path)
    $startInfo.Arguments = (@($Arguments | ForEach-Object { '"' + ([string]$_).Replace('"', '\"') + '"' }) -join ' ')
    $startInfo.WorkingDirectory = $RepositoryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) { throw "Package command could not start: $Path" }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
        $stdout = [string]$stdoutTask.Result
        $stderr = [string]$stderrTask.Result
    } finally {
        $process.Dispose()
    }
    $stdoutPath = $LogPath + '.stdout.txt'
    $stderrPath = $LogPath + '.stderr.txt'
    [IO.File]::WriteAllText($stdoutPath, $stdout, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($stderrPath, $stderr, [Text.UTF8Encoding]::new($false))
    Write-P917File $LogPath @("exitCode: $exitCode", "stdoutPath: $stdoutPath", "stderrPath: $stderrPath")
    return [ordered]@{
        exitCode = $exitCode
        logPath = [IO.Path]::GetFullPath($LogPath)
        logSha256 = Get-P914FileSha256 $LogPath
        stdoutPath = [IO.Path]::GetFullPath($stdoutPath)
        stdoutSha256 = Get-P914FileSha256 $stdoutPath
        stderrPath = [IO.Path]::GetFullPath($stderrPath)
        stderrSha256 = Get-P914FileSha256 $stderrPath
        stdout = $stdout
        stderr = $stderr
    }
}

function Write-P917HeapJsonAtomic([string]$Path, [object]$Value) {
    $temporaryPath = $Path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    Write-P914Json $temporaryPath $Value
    [IO.File]::Move($temporaryPath, $Path)
}

function Grant-P917HeapAdministratorDirectoryAccess([string]$Path) {
    $acl = Get-Acl -LiteralPath $Path
    $administrators = [Security.Principal.SecurityIdentifier]::new('S-1-5-32-544')
    $inheritance = [Security.AccessControl.InheritanceFlags]::ContainerInherit -bor [Security.AccessControl.InheritanceFlags]::ObjectInherit
    $rule = [Security.AccessControl.FileSystemAccessRule]::new(
        $administrators,
        [Security.AccessControl.FileSystemRights]::Modify,
        $inheritance,
        [Security.AccessControl.PropagationFlags]::None,
        [Security.AccessControl.AccessControlType]::Allow
    )
    $acl.AddAccessRule($rule)
    Set-Acl -LiteralPath $Path -AclObject $acl
}

function Invoke-P917HeapWorkerRequest([object]$Diagnostic, [ValidateSet('snapshot', 'stop')][string]$Command, [object]$Payload, [int]$TimeoutSeconds) {
    if ($null -eq $Diagnostic.workerProcessId) { throw 'The elevated WPR-only worker has not started.' }
    $sequence = [int]$Diagnostic.nextCommandSequence
    $requestPath = Join-Path ([string]$Diagnostic.workerRequestRoot) ('request-{0:D4}.json' -f $sequence)
    $responsePath = Join-Path ([string]$Diagnostic.workerResponseRoot) ('response-{0:D4}.json' -f $sequence)
    if (Test-Path -LiteralPath $requestPath) { throw "Heap worker request already exists for sequence $sequence." }
    $request = [ordered]@{
        schema = 'classmngr-phase9-p9-17-heap-worker-request-v1'
        runId = [string]$Diagnostic.runId
        nonce = [string]$Diagnostic.nonce
        sequence = $sequence
        command = $Command
        issuedAtUtc = Get-P914Utc
    }
    if ($null -ne $Payload) {
        foreach ($entry in $Payload.GetEnumerator()) { $request[[string]$entry.Key] = $entry.Value }
    }
    $startedAtUtc = Get-P914Utc
    Write-P917HeapJsonAtomic $requestPath $request
    $Diagnostic.nextCommandSequence = $sequence + 1
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $response = $null
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if (Test-Path -LiteralPath $responsePath -PathType Leaf) {
            $response = Read-P914Json $responsePath
            if ([string]$response.schema -cne 'classmngr-phase9-p9-17-heap-worker-response-v1' -or
                [string]$response.runId -cne [string]$Diagnostic.runId -or
                [string]$response.nonce -cne [string]$Diagnostic.nonce -or
                [int]$response.sequence -ne $sequence -or [string]$response.command -cne $Command) {
                throw "Elevated WPR worker response binding failed for sequence $sequence."
            }
            break
        }
        $worker = Get-Process -Id ([int]$Diagnostic.workerProcessId) -ErrorAction SilentlyContinue
        if ($null -eq $worker) {
            $workerFinal = if (Test-Path -LiteralPath ([string]$Diagnostic.workerFinalStatusPath) -PathType Leaf) { Read-P914Json ([string]$Diagnostic.workerFinalStatusPath) } else { $null }
            throw "Elevated WPR worker exited before responding to '$Command'. Final status: $($workerFinal.status); $($workerFinal.failure)"
        }
        Start-Sleep -Milliseconds 100
    }
    $record = [ordered]@{
        command = $Command
        sequence = $sequence
        requestPath = $requestPath
        responsePath = $responsePath
        startedAtUtc = $startedAtUtc
        completedAtUtc = Get-P914Utc
        timeoutSeconds = $TimeoutSeconds
        status = if ($null -ne $response) { [string]$response.status } else { 'timeout' }
        response = $response
    }
    $Diagnostic.commands.Add($record)
    Add-P914JsonLine ([string]$Diagnostic.commandLogPath) $record
    Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
    if ($null -eq $response) { throw "Timed out after $TimeoutSeconds seconds waiting for elevated WPR worker command '$Command' (sequence $sequence)." }
    return $response
}

function New-P917HeapSnapshotDiagnostic([string]$StandardRunId) {
    $evidenceInfo = New-P914EvidenceDirectory $RepositoryRoot $HeapDiagnosticEvidenceRoot
    $evidenceDirectory = [string]$evidenceInfo.evidenceDirectory
    $traceDirectory = Join-Path $evidenceDirectory 'trace'
    [void][IO.Directory]::CreateDirectory($traceDirectory)
    $wpr = Get-Command wpr.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
    $wpa = Get-Command wpaexporter.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $manifestPath = Join-Path $evidenceDirectory 'heap-diagnostic-manifest.json'
    $controlDirectory = Join-Path $evidenceDirectory 'control'
    $requestRoot = Join-Path $controlDirectory 'requests'
    $responseRoot = Join-Path $controlDirectory 'responses'
    $workerScriptPath = Join-Path $PSScriptRoot 'p9_17_heap_snapshot_elevated_worker.ps1'
    $diagnostic = [ordered]@{
        schema = 'classmngr-phase9-p9-17-heap-snapshot-diagnostic-v1'; taskId = $TaskId
        runId = [string]$evidenceInfo.runId; status = 'prepared'; diagnosticOnly = $true
        accepted = $false
        acceptanceUse = 'Excluded: WPR heap tracing can change process memory measurements.'
        repositoryRoot = $RepositoryRoot; repositoryRunId = $StandardRunId; repositoryManifestPath = $null; repositoryManifestSha256 = $null
        evidenceDirectory = $evidenceDirectory; manifestPath = $manifestPath
        processId = $null; processImagePath = $null; processImageSha256 = $null; processSessionId = $null
        wprPath = [string]$wpr.Source; wprFileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo([string]$wpr.Source).FileVersion
        wprSha256 = Get-P914FileSha256 ([string]$wpr.Source)
        wprInstanceName = 'P917Heap_' + ([string]$evidenceInfo.runId -replace '[^A-Za-z0-9]', '')
        profile = 'HeapSnapshot'; snapshotOption = 'Heap'; pidScopedSnapshotConfig = $true; ifeoOrRegistryChanged = $false
        tracePath = Join-Path $traceDirectory 'p9-17-heap-snapshot.etl'
        traceSha256 = $null; traceSizeBytes = $null; commandLogPath = Join-Path (Join-Path $evidenceDirectory 'logs') 'wpr-commands.jsonl'
        workerScriptPath = [IO.Path]::GetFullPath($workerScriptPath); workerScriptSha256 = Get-P914FileSha256 $workerScriptPath
        workerControlDirectory = $controlDirectory; workerConfigPath = Join-Path $controlDirectory 'worker-config.json'
        workerRequestRoot = $requestRoot; workerResponseRoot = $responseRoot
        workerLaunchStatusPath = Join-Path $controlDirectory 'worker-launch.json'
        workerAuthorizationPath = Join-Path $controlDirectory 'worker-start-authorization.json'
        workerReadyStatusPath = Join-Path $controlDirectory 'worker-ready.json'; workerFinalStatusPath = Join-Path $controlDirectory 'worker-final.json'
        workerCommandLogPath = Join-Path (Join-Path $controlDirectory 'logs') 'elevated-wpr-commands.jsonl'
        workerProcessId = $null; workerLauncherProcessId = $null; workerLaunchBrokerProcessId = $null
        workerLaunchBrokerImagePath = $null; workerLaunchBrokerImageSha256 = $null; workerExecutionContext = $null; workerStatus = $null
        workerLaunchStatus = $null; workerLaunchFailure = $null; workerLaunchRecordedAtUtc = $null
        workerAuthorizationSha256 = $null; workerAuthorizedAtUtc = $null
        workerElevationMechanism = 'One user-approved UAC RunAs launch initiated by a hidden standard-integrity PowerShell broker launched through Start-Process/ShellExecute; the non-admin route runner bounds the broker wait to 180 seconds and authorizes only the WPR worker after validating the run, nonce, broker, worker, app, image, and session bindings.'
        workerUacLaunchCount = 0; workerUacRequestedAtUtc = $null; workerReadyAtUtc = $null; workerExitedAtUtc = $null
        workerConfigSha256 = $null; workerExecutablePath = $null; workerExecutableSha256 = $null
        workerLaunchTimeoutSeconds = 180; workerBrokerStopTimeoutSeconds = 5
        workerStartupTimeoutSeconds = 240
        workerCommandResponseTimeoutSeconds = 60; workerStopResponseTimeoutSeconds = 450
        workerWprCommandTimeoutSeconds = 45; workerSessionWatchdogSeconds = 1200; nextCommandSequence = 1
        runnerExecutionContext = $null; applicationExecutionContext = $null
        commands = [Collections.Generic.List[object]]::new(); snapshots = [Collections.Generic.List[object]]::new()
        snapshotConfigEnableAttempted = $false; snapshotConfigEnabled = $false; sessionStartAttempted = $false; sessionStarted = $false
        sessionStopAttempted = $false; sessionStopped = $false; snapshotConfigDisableAttempted = $false; snapshotConfigDisabled = $false
        cleanupCompleted = $false; noActiveWprSessionAtEnd = $null; cleanupErrors = [Collections.Generic.List[string]]::new()
        wpaExporter = [ordered]@{
            path = if ($null -ne $wpa) { [string]$wpa.Source } else { $null }
            fileVersion = if ($null -ne $wpa) { [Diagnostics.FileVersionInfo]::GetVersionInfo([string]$wpa.Source).FileVersion } else { $null }
            availableForExport = $false; exportAttempted = $false
            probe = 'wpaexporter.exe -listplugins and -help Heap'
            limitation = 'Exporter startup reported missing wpaexporter.deps.json and could not create its Windows Performance Analyzer directory under LocalAppData in this sandbox; ETL is retained for WPA GUI analysis.'
        }
        failure = $null; createdAtUtc = Get-P914Utc; completedAtUtc = $null
    }
    [IO.File]::WriteAllText([string]$diagnostic.commandLogPath, '', [Text.UTF8Encoding]::new($false))
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent ([string]$diagnostic.workerCommandLogPath)))
    [IO.File]::WriteAllText([string]$diagnostic.workerCommandLogPath, '', [Text.UTF8Encoding]::new($false))
    Write-P914Json $manifestPath $diagnostic
    return $diagnostic
}

function Assert-P917HeapSnapshotApplicationBinding([object]$RunnerContext, [object]$ApplicationContext, [object]$ProcessEvidence) {
    if ($null -eq $ApplicationContext -or $null -eq $ProcessEvidence) { throw 'Heap snapshot app process or evidence record is missing.' }
    $liveProcess = $ApplicationContext.process
    if ($null -eq $liveProcess) { throw 'Heap snapshot app process object is missing.' }
    $liveProcess.Refresh()
    if ([bool]$liveProcess.HasExited) { throw 'Heap snapshot app process exited before WPR binding.' }

    if (-not (Test-P917JsonInteger -Value $ProcessEvidence.processId) -or [Int64]$ProcessEvidence.processId -lt 1 -or
        [Int64]$liveProcess.Id -ne [Int64]$ProcessEvidence.processId) {
        throw 'Heap snapshot app PID differs from the live app process.'
    }
    if (-not (Test-P917JsonInteger -Value $ProcessEvidence.sessionId) -or [Int64]$ProcessEvidence.sessionId -lt 1 -or
        -not (Test-P917JsonInteger -Value $ApplicationContext.processSessionId) -or
        [Int64]$liveProcess.SessionId -ne [Int64]$ProcessEvidence.sessionId -or
        [Int64]$ApplicationContext.processSessionId -ne [Int64]$ProcessEvidence.sessionId) {
        throw 'Heap snapshot app process session differs from its live process evidence.'
    }

    $evidenceImagePath = [string]$ProcessEvidence.processImagePath
    $contextImagePath = [string]$ApplicationContext.processImagePath
    if ([string]::IsNullOrWhiteSpace($evidenceImagePath) -or [string]::IsNullOrWhiteSpace($contextImagePath) -or
        -not [IO.Path]::IsPathRooted($evidenceImagePath) -or -not [IO.Path]::IsPathRooted($contextImagePath) -or
        [IO.Path]::GetFullPath($evidenceImagePath) -ine [IO.Path]::GetFullPath($contextImagePath)) {
        throw 'Heap snapshot app image path differs from its verified process evidence.'
    }
    $evidenceImageSha256 = [string]$ProcessEvidence.processImageSha256
    $contextImageSha256 = [string]$ApplicationContext.processImageSha256
    if ($evidenceImageSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        $contextImageSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        $evidenceImageSha256 -ine $contextImageSha256) {
        throw 'Heap snapshot app image hash differs from its verified process evidence.'
    }

    $applicationSession = $ApplicationContext.executionContext
    $evidenceSession = $ProcessEvidence.executionContext
    if ($null -eq $applicationSession -or $null -eq $evidenceSession) { throw 'Heap snapshot app session-binding evidence is missing.' }
    foreach ($field in @('buildIdentity', 'buildUserSid', 'observerIdentity', 'observerUserSid', 'buildProcessSessionId', 'observerProcessSessionId', 'measuredProcessSessionId', 'activeConsoleSessionId', 'sharedSessionVerified')) {
        if ([string]$applicationSession.$field -cne [string]$evidenceSession.$field) {
            throw "Heap snapshot app session-binding evidence differs between the live app context and process record at '$field'."
        }
    }
    if ($applicationSession.sharedSessionVerified -isnot [bool] -or -not [bool]$applicationSession.sharedSessionVerified) {
        throw 'Heap snapshot app process lacks verified build/observer session binding.'
    }
    foreach ($field in @('buildProcessSessionId', 'observerProcessSessionId', 'measuredProcessSessionId', 'activeConsoleSessionId')) {
        if (-not (Test-P917JsonInteger -Value $applicationSession.$field) -or [Int64]$applicationSession.$field -lt 1) {
            throw "Heap snapshot app session-binding field '$field' is not a positive observed integer."
        }
    }
    if ([string]$applicationSession.buildIdentity -cne [string]$RunnerContext.identity -or
        [string]$applicationSession.observerIdentity -cne [string]$RunnerContext.identity -or
        [string]$applicationSession.buildUserSid -cne [string]$RunnerContext.userSid -or
        [string]$applicationSession.observerUserSid -cne [string]$RunnerContext.userSid) {
        throw 'Heap snapshot app build/observer identity does not match the verified non-admin runner token.'
    }
    $runnerSessionId = [Int64]$RunnerContext.processSessionId
    $activeConsoleSessionId = [Int64]$RunnerContext.activeConsoleSessionId
    if ([Int64]$applicationSession.buildProcessSessionId -ne $runnerSessionId -or
        [Int64]$applicationSession.observerProcessSessionId -ne $runnerSessionId -or
        [Int64]$applicationSession.measuredProcessSessionId -ne [Int64]$ProcessEvidence.sessionId -or
        [Int64]$applicationSession.measuredProcessSessionId -ne $runnerSessionId -or
        [Int64]$applicationSession.activeConsoleSessionId -ne $activeConsoleSessionId -or
        [Int64]$applicationSession.activeConsoleSessionId -ne [Int64]$ProcessEvidence.sessionId) {
        throw 'Heap snapshot app process session does not match the verified runner and active console sessions.'
    }
}

function New-P917HeapSnapshotBrokerBootstrap([string]$BrokerConfigBase64) {
    if ($BrokerConfigBase64 -notmatch '^[A-Za-z0-9+/]+={0,2}$') { throw 'Heap snapshot broker configuration is not a valid Base64 value.' }
    $template = @'
$ErrorActionPreference = 'Stop'
$brokerConfig = [Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('__P917_BROKER_CONFIG__')) | ConvertFrom-Json
$brokerProcess = [Diagnostics.Process]::GetCurrentProcess()
$brokerProcessId = [Int64]$brokerProcess.Id
$brokerSessionId = [Int64]$brokerProcess.SessionId
$brokerUserSid = [string][Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$runasRequestedAtUtc = [DateTimeOffset]::UtcNow.ToString('o')
$launchStatusPath = [string]$brokerConfig.launchStatusPath
$launchTempPath = $launchStatusPath + '.' + $brokerProcessId + '.tmp'
try {
    $elevatedWorker = Start-Process -FilePath ([string]$brokerConfig.workerPowerShell) -ArgumentList ([string]$brokerConfig.workerArguments) -WorkingDirectory ([string]$brokerConfig.workingDirectory) -Verb RunAs -PassThru -WindowStyle Hidden
    if ($null -eq $elevatedWorker) { throw 'UAC RunAs did not return the elevated WPR worker process.' }
    $status = [ordered]@{
        schema = 'classmngr-phase9-p9-17-heap-worker-launch-v3'
        runId = [string]$brokerConfig.runId; nonce = [string]$brokerConfig.nonce; status = 'started'
        runnerProcessId = [Int64]$brokerConfig.runnerProcessId
        runasAttempted = $true; runasRequestedAtUtc = $runasRequestedAtUtc
        brokerProcessId = $brokerProcessId; brokerSessionId = $brokerSessionId; brokerUserSid = $brokerUserSid
        workerProcessId = [Int64]$elevatedWorker.Id; workerImagePath = [string]$brokerConfig.workerPowerShell
        launchedAtUtc = [DateTimeOffset]::UtcNow.ToString('o'); failure = $null
    }
    $json = ConvertTo-Json -InputObject $status -Depth 8 -Compress
    [IO.File]::WriteAllText($launchTempPath, $json, [Text.UTF8Encoding]::new($false))
    [IO.File]::Move($launchTempPath, $launchStatusPath)
} catch {
    $status = [ordered]@{
        schema = 'classmngr-phase9-p9-17-heap-worker-launch-v3'
        runId = [string]$brokerConfig.runId; nonce = [string]$brokerConfig.nonce; status = 'failed'
        runnerProcessId = [Int64]$brokerConfig.runnerProcessId
        runasAttempted = $true; runasRequestedAtUtc = $runasRequestedAtUtc
        brokerProcessId = $brokerProcessId; brokerSessionId = $brokerSessionId; brokerUserSid = $brokerUserSid
        workerProcessId = $null; workerImagePath = [string]$brokerConfig.workerPowerShell
        launchedAtUtc = [DateTimeOffset]::UtcNow.ToString('o'); failure = [string]$_.Exception.Message
    }
    try {
        $json = ConvertTo-Json -InputObject $status -Depth 8 -Compress
        [IO.File]::WriteAllText($launchTempPath, $json, [Text.UTF8Encoding]::new($false))
        [IO.File]::Move($launchTempPath, $launchStatusPath)
    } catch { }
    exit 1
}
$authorizationDeadline = [DateTime]::UtcNow.AddSeconds([int]$brokerConfig.authorizationWaitSeconds)
while ([DateTime]::UtcNow -lt $authorizationDeadline) {
    if (Test-Path -LiteralPath ([string]$brokerConfig.authorizationPath) -PathType Leaf) {
        try {
            $authorization = Get-Content -LiteralPath ([string]$brokerConfig.authorizationPath) -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$authorization.schema -ceq 'classmngr-phase9-p9-17-heap-worker-authorization-v1' -and
                [string]$authorization.runId -ceq [string]$brokerConfig.runId -and
                [string]$authorization.nonce -ceq [string]$brokerConfig.nonce -and
                [Int64]$authorization.workerProcessId -eq [Int64]$elevatedWorker.Id -and
                [Int64]$authorization.launcherProcessId -eq $brokerProcessId -and
                [string]$authorization.runnerUserSid -ceq $brokerUserSid) { exit 0 }
        } catch { }
    }
    Start-Sleep -Milliseconds 100
}
exit 2
'@
    return $template.Replace('__P917_BROKER_CONFIG__', $BrokerConfigBase64)
}

function Stop-P917HeapSnapshotLaunchBroker([object]$BrokerProcess, [int]$TimeoutMilliseconds) {
    if ($null -eq $BrokerProcess) { return }
    try { $BrokerProcess.Refresh() } catch { return }
    if ([bool]$BrokerProcess.HasExited) { return }
    Stop-Process -Id ([int]$BrokerProcess.Id) -Force -ErrorAction Stop
    if (-not $BrokerProcess.WaitForExit($TimeoutMilliseconds)) {
        throw "Heap snapshot launch broker did not exit within $TimeoutMilliseconds milliseconds after termination."
    }
}

function Start-P917HeapSnapshotTrace([object]$Diagnostic, [object]$ApplicationContext, [object]$ProcessEvidence) {
    $runnerContext = Get-P917CurrentExecutionContext
    Assert-P917ExecutionContextRecord $runnerContext 'heap snapshot route runner'
    Assert-P917HeapSnapshotApplicationBinding -RunnerContext $runnerContext -ApplicationContext $ApplicationContext -ProcessEvidence $ProcessEvidence
    $processId = [Int64]$ProcessEvidence.processId
    $processImagePath = [string]$ProcessEvidence.processImagePath
    $processImageSha256 = [string]$ProcessEvidence.processImageSha256
    $processSessionId = [Int64]$ProcessEvidence.sessionId
    $applicationExecutionContext = $ProcessEvidence.executionContext
    $Diagnostic.processId = $processId
    $Diagnostic.processImagePath = $processImagePath
    $Diagnostic.processImageSha256 = $processImageSha256
    $Diagnostic.processSessionId = $processSessionId
    $Diagnostic.status = 'starting'
    $Diagnostic.runnerExecutionContext = $runnerContext
    $Diagnostic.applicationExecutionContext = $applicationExecutionContext
    $Diagnostic.applicationProcessEvidence = [ordered]@{
        processId = $processId
        processImagePath = [IO.Path]::GetFullPath($processImagePath)
        processImageSha256 = $processImageSha256
        sessionId = $processSessionId
    }

    foreach ($path in @(
        [string]$Diagnostic.workerControlDirectory,
        [string]$Diagnostic.workerRequestRoot,
        [string]$Diagnostic.workerResponseRoot,
        (Split-Path -Parent ([string]$Diagnostic.workerCommandLogPath)),
        (Split-Path -Parent ([string]$Diagnostic.tracePath))
    )) {
        [void][IO.Directory]::CreateDirectory($path)
        Grant-P917HeapAdministratorDirectoryAccess $path
    }
    if ((Get-P914FileSha256 ([string]$Diagnostic.wprPath)) -cne [string]$Diagnostic.wprSha256) { throw 'WPR executable changed after heap diagnostic preparation.' }
    if ((Get-P914FileSha256 ([string]$Diagnostic.workerScriptPath)) -cne [string]$Diagnostic.workerScriptSha256) { throw 'Dedicated elevated WPR worker changed after heap diagnostic preparation.' }

    $nonce = [Guid]::NewGuid().ToString('N')
    $Diagnostic.nonce = $nonce
    $workerPowerShell = [IO.Path]::GetFullPath((Join-Path $PSHOME 'powershell.exe'))
    $workerPowerShellSha256 = Get-P914FileSha256 $workerPowerShell
    $Diagnostic.workerExecutablePath = $workerPowerShell
    $Diagnostic.workerExecutableSha256 = $workerPowerShellSha256
    $config = [ordered]@{
        schema = 'classmngr-phase9-p9-17-heap-worker-config-v1'
        runId = [string]$Diagnostic.runId
        nonce = $nonce
        repositoryRoot = $RepositoryRoot
        evidenceDirectory = [string]$Diagnostic.evidenceDirectory
        controlRoot = [string]$Diagnostic.workerControlDirectory
        requestRoot = [string]$Diagnostic.workerRequestRoot
        responseRoot = [string]$Diagnostic.workerResponseRoot
        readyStatusPath = [string]$Diagnostic.workerReadyStatusPath
        finalStatusPath = [string]$Diagnostic.workerFinalStatusPath
        authorizationPath = [string]$Diagnostic.workerAuthorizationPath
        authorizationTimeoutSeconds = 60
        workerExecutablePath = $workerPowerShell
        workerExecutableSha256 = $workerPowerShellSha256
        commandLogPath = [string]$Diagnostic.workerCommandLogPath
        tracePath = [string]$Diagnostic.tracePath
        wprPath = [string]$Diagnostic.wprPath
        wprSha256 = [string]$Diagnostic.wprSha256
        wprFileVersion = [string]$Diagnostic.wprFileVersion
        wprInstanceName = [string]$Diagnostic.wprInstanceName
        processId = $processId
        runnerUserSid = [string]$runnerContext.userSid
        processSessionId = $processSessionId
        processImagePath = [IO.Path]::GetFullPath($processImagePath)
        processImageSha256 = $processImageSha256
        wprCommandTimeoutSeconds = [int]$Diagnostic.workerWprCommandTimeoutSeconds
        sessionWatchdogSeconds = [int]$Diagnostic.workerSessionWatchdogSeconds
        configExpiresAtUtc = [DateTimeOffset]::UtcNow.AddMinutes(8).ToString('o')
        createdAtUtc = Get-P914Utc
    }
    Write-P917HeapJsonAtomic ([string]$Diagnostic.workerConfigPath) $config
    $Diagnostic.workerConfigSha256 = Get-P914FileSha256 ([string]$Diagnostic.workerConfigPath)
    $Diagnostic.status = 'launch-config-ready'
    Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic

    $workerPathLiteral = "'" + ([string]$Diagnostic.workerScriptPath).Replace("'", "''") + "'"
    $configPathLiteral = "'" + ([string]$Diagnostic.workerConfigPath).Replace("'", "''") + "'"
    $workerBootstrap = "& $workerPathLiteral -ControlConfigPath $configPathLiteral"
    $encodedWorkerBootstrap = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($workerBootstrap))
    $workerArguments = '-NoLogo -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand ' + $encodedWorkerBootstrap
    $runnerProcessId = [Int64][Diagnostics.Process]::GetCurrentProcess().Id
    $brokerConfig = [ordered]@{
        runId = [string]$Diagnostic.runId; nonce = $nonce
        runnerProcessId = $runnerProcessId
        launchStatusPath = [string]$Diagnostic.workerLaunchStatusPath
        authorizationPath = [string]$Diagnostic.workerAuthorizationPath
        authorizationWaitSeconds = 60
        workerPowerShell = [IO.Path]::GetFullPath($workerPowerShell)
        workerArguments = $workerArguments; workingDirectory = $RepositoryRoot
    }
    $brokerConfigJson = ConvertTo-Json -InputObject $brokerConfig -Depth 6 -Compress
    $brokerConfigBase64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($brokerConfigJson))
    $brokerBootstrap = New-P917HeapSnapshotBrokerBootstrap $brokerConfigBase64
    $encodedBrokerBootstrap = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($brokerBootstrap))
    $brokerArguments = '-NoLogo -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand ' + $encodedBrokerBootstrap
    $Diagnostic.workerUacLaunchCount = 1
    $Diagnostic.workerLaunchBrokerStartedAtUtc = Get-P914Utc
    $Diagnostic.status = 'waiting-for-user-approved-uac-worker'
    Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
    $brokerProcess = $null
    try {
        $brokerProcess = Start-Process -FilePath $workerPowerShell -ArgumentList $brokerArguments -WorkingDirectory $RepositoryRoot -PassThru -WindowStyle Hidden
        if ($null -eq $brokerProcess) { throw 'Standard-integrity PowerShell launch broker did not return a process.' }
        $launcherProcessId = [Int64]$brokerProcess.Id
        $Diagnostic.workerLauncherProcessId = $launcherProcessId
        $Diagnostic.workerLaunchBrokerProcessId = $launcherProcessId
        $brokerProcess.Refresh()
        if ([bool]$brokerProcess.HasExited) { throw 'PowerShell launch broker exited before its first status check.' }
        $brokerSessionId = [Int64]$brokerProcess.SessionId
        $brokerImagePath = [IO.Path]::GetFullPath([string]$brokerProcess.MainModule.FileName)
        $brokerImageSha256 = Get-P914FileSha256 $brokerImagePath
        if ($brokerSessionId -ne [Int64]$runnerContext.processSessionId -or
            $brokerImagePath -ine [IO.Path]::GetFullPath($workerPowerShell) -or $brokerImageSha256 -cne $workerPowerShellSha256) {
            throw 'PowerShell launch broker process session, image path, or executable hash differs from the non-admin route runner launch.'
        }
        $Diagnostic.workerLaunchBrokerImagePath = $brokerImagePath
        $Diagnostic.workerLaunchBrokerImageSha256 = $brokerImageSha256
        $launchWatch = [Diagnostics.Stopwatch]::StartNew()
        $launchStatus = $null
        while ($launchWatch.Elapsed.TotalSeconds -lt [int]$Diagnostic.workerLaunchTimeoutSeconds) {
            if (Test-Path -LiteralPath ([string]$Diagnostic.workerLaunchStatusPath) -PathType Leaf) {
                try { $launchStatus = Read-P914Json ([string]$Diagnostic.workerLaunchStatusPath) } catch { $launchStatus = $null }
                if ($null -ne $launchStatus) { break }
            }
            $brokerProcess.Refresh()
            if ([bool]$brokerProcess.HasExited) {
                if (Test-Path -LiteralPath ([string]$Diagnostic.workerLaunchStatusPath) -PathType Leaf) {
                    try { $launchStatus = Read-P914Json ([string]$Diagnostic.workerLaunchStatusPath) } catch { $launchStatus = $null }
                }
                if ($null -eq $launchStatus) { throw 'PowerShell launch broker exited without a valid RunAs status record.' }
                break
            }
            Start-Sleep -Milliseconds 100
        }
        if ($null -eq $launchStatus) {
            Stop-P917HeapSnapshotLaunchBroker $brokerProcess ([int]$Diagnostic.workerBrokerStopTimeoutSeconds * 1000)
            throw "User-approved UAC worker launch did not return a status within $($Diagnostic.workerLaunchTimeoutSeconds) seconds; the standard-integrity broker was terminated."
        }
        $Diagnostic.workerLaunchStatus = $launchStatus
        $Diagnostic.workerUacLaunchCount = if ([bool]$launchStatus.runasAttempted) { 1 } else { 0 }
        $Diagnostic.workerUacRequestedAtUtc = [string]$launchStatus.runasRequestedAtUtc
        if ([string]$launchStatus.schema -cne 'classmngr-phase9-p9-17-heap-worker-launch-v3' -or
            [string]$launchStatus.runId -cne [string]$Diagnostic.runId -or [string]$launchStatus.nonce -cne $nonce -or
            [Int64]$launchStatus.runnerProcessId -ne $runnerProcessId -or
            [Int64]$launchStatus.brokerProcessId -ne $launcherProcessId -or
            [Int64]$launchStatus.brokerSessionId -ne $brokerSessionId -or
            [string]$launchStatus.brokerUserSid -cne [string]$runnerContext.userSid -or
            [string]$launchStatus.status -notin @('started', 'failed') -or -not [bool]$launchStatus.runasAttempted) {
            throw 'UAC broker status failed schema, run, nonce, broker PID/session/SID, or RunAs-attempt binding.'
        }
        if ([string]$launchStatus.status -ceq 'failed') {
            throw "UAC RunAs broker failed before worker authorization: $([string]$launchStatus.failure)"
        }
        if ([Int64]$launchStatus.workerProcessId -lt 1 -or
            [IO.Path]::GetFullPath([string]$launchStatus.workerImagePath) -ine [IO.Path]::GetFullPath($workerPowerShell)) {
            throw 'UAC broker status has an invalid worker PID or image path.'
        }
        $brokerProcess.Refresh()
        if ([bool]$brokerProcess.HasExited -or [Int64]$brokerProcess.SessionId -ne $brokerSessionId) {
            throw 'UAC broker exited or changed session before the runner authorized the WPR worker.'
        }
        $elevatedWorker = Get-Process -Id ([int]$launchStatus.workerProcessId) -ErrorAction Stop
        $elevatedWorker.Refresh()
        if ([bool]$elevatedWorker.HasExited -or [Int64]$elevatedWorker.SessionId -ne $processSessionId) {
            throw 'Elevated WPR worker PID or process session differs from the approved launch configuration.'
        }
        $Diagnostic.workerProcessId = [Int64]$launchStatus.workerProcessId
        $Diagnostic.workerLaunchedAtUtc = [string]$launchStatus.launchedAtUtc
        $authorization = [ordered]@{
            schema = 'classmngr-phase9-p9-17-heap-worker-authorization-v1'
            runId = [string]$Diagnostic.runId; nonce = $nonce
            workerProcessId = [Int64]$Diagnostic.workerProcessId
            runnerProcessId = $runnerProcessId; launcherProcessId = $launcherProcessId; brokerSessionId = $brokerSessionId
            brokerImageSha256 = $brokerImageSha256
            routeProcessId = $processId; routeImageSha256 = $processImageSha256
            runnerUserSid = [string]$runnerContext.userSid
            authorizedAtUtc = Get-P914Utc
        }
        Write-P917HeapJsonAtomic ([string]$Diagnostic.workerAuthorizationPath) $authorization
        $Diagnostic.workerAuthorizationSha256 = Get-P914FileSha256 ([string]$Diagnostic.workerAuthorizationPath)
        $Diagnostic.workerAuthorizedAtUtc = [string]$authorization.authorizedAtUtc
        $Diagnostic.workerLaunchRecordedAtUtc = Get-P914Utc
        $Diagnostic.status = 'waiting-for-elevated-wpr-worker'
        Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
    } catch {
        $Diagnostic.workerLaunchFailure = $_.Exception.Message
        if ($null -ne $brokerProcess) {
            try { Stop-P917HeapSnapshotLaunchBroker $brokerProcess ([int]$Diagnostic.workerBrokerStopTimeoutSeconds * 1000) } catch { }
        }
        if ($null -eq $Diagnostic.workerLaunchStatus) {
            $failedLaunch = [ordered]@{
                schema = 'classmngr-phase9-p9-17-heap-worker-launch-v3'
                runId = [string]$Diagnostic.runId; nonce = $nonce; status = 'failed'; runnerProcessId = $runnerProcessId
                runasAttempted = $false; runasRequestedAtUtc = $null
                brokerProcessId = if ($null -ne $brokerProcess) { [Int64]$brokerProcess.Id } else { $null }
                brokerSessionId = if ($null -ne $brokerProcess) { [Int64]$brokerProcess.SessionId } else { $null }
                brokerUserSid = [string]$runnerContext.userSid
                workerProcessId = $null; workerImagePath = [IO.Path]::GetFullPath($workerPowerShell)
                launchedAtUtc = Get-P914Utc; failure = [string]$_.Exception.Message
            }
            $Diagnostic.workerLaunchStatus = $failedLaunch
            try { Write-P917HeapJsonAtomic ([string]$Diagnostic.workerLaunchStatusPath) $failedLaunch } catch { }
        }
        $Diagnostic.status = 'failed'
        Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
        throw
    }

    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt [int]$Diagnostic.workerStartupTimeoutSeconds) {
        if (Test-Path -LiteralPath ([string]$Diagnostic.workerReadyStatusPath) -PathType Leaf) {
            $ready = Read-P914Json ([string]$Diagnostic.workerReadyStatusPath)
            if ([string]$ready.schema -cne 'classmngr-phase9-p9-17-heap-worker-ready-v1' -or
                [string]$ready.runId -cne [string]$Diagnostic.runId -or [string]$ready.nonce -cne $nonce -or
                [string]$ready.status -cne 'ready' -or [Int64]$ready.processId -ne $processId -or
                [Int64]$ready.workerProcessId -ne [Int64]$Diagnostic.workerProcessId -or
                [IO.Path]::GetFullPath([string]$ready.workerImagePath) -ine [IO.Path]::GetFullPath([string]$Diagnostic.workerExecutablePath) -or
                [string]$ready.workerImageSha256 -cne [string]$Diagnostic.workerExecutableSha256 -or
                [string]$ready.processImageSha256 -cne $processImageSha256 -or
                [string]$ready.executionContext.userSid -cne [string]$runnerContext.userSid -or
                [string]$ready.activationAuthorizationSha256 -cne [string]$Diagnostic.workerAuthorizationSha256 -or
                -not [bool]$ready.executionContext.isAdministratorRole -or
                -not [bool]$ready.executionContext.userInteractive -or
                [Int64]$ready.executionContext.processSessionId -ne $processSessionId -or
                [Int64]$ready.executionContext.activeConsoleSessionId -ne [Int64]$applicationExecutionContext.activeConsoleSessionId) {
                throw 'Elevated WPR worker readiness record failed process, session, or authorization binding.'
            }
            $Diagnostic.workerExecutionContext = $ready.executionContext
            $Diagnostic.workerReadyAtUtc = [string]$ready.readyAtUtc
            $Diagnostic.status = 'recording'
            $Diagnostic.sessionStartAttempted = $true
            $Diagnostic.sessionStarted = $true
            $Diagnostic.snapshotConfigEnableAttempted = $true
            $Diagnostic.snapshotConfigEnabled = $true
            Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
            return
        }
        if (Test-Path -LiteralPath ([string]$Diagnostic.workerFinalStatusPath) -PathType Leaf) {
            $failed = Read-P914Json ([string]$Diagnostic.workerFinalStatusPath)
            $Diagnostic.workerStatus = $failed
            $Diagnostic.status = 'failed'
            Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
            throw "Elevated WPR worker failed before readiness: $($failed.failure)"
        }
        $worker = Get-Process -Id ([int]$Diagnostic.workerProcessId) -ErrorAction SilentlyContinue
        if ($null -eq $worker) { throw 'Elevated WPR worker exited before publishing its readiness record.' }
        Start-Sleep -Milliseconds 100
    }
    throw "Elevated WPR worker did not become ready within $($Diagnostic.workerStartupTimeoutSeconds) seconds."
}

function Add-P917HeapSnapshot([object]$Diagnostic, [string]$Label, [object]$MemoryWindow, [object]$RouteCheckpoint) {
    if ($null -eq $Diagnostic -or [string]$Diagnostic.status -cne 'recording') { throw 'The P9-17 HeapSnapshot diagnostic session is not recording at a requested checkpoint.' }
    $expectedWindows = [ordered]@{
        'post-primer-plateau-baseline' = 'plateau-baseline-05'
        'cycle-01-return' = 'cycle-01-return'
        'cycle-10-return' = 'cycle-10-return'
    }
    $windowId = [string]$MemoryWindow.windowId
    if (-not $expectedWindows.Contains($Label) -or [string]$expectedWindows[$Label] -cne $windowId) { throw "Heap snapshot '$Label' is not bound to its required sampled window." }
    if (@($Diagnostic.snapshots | Where-Object { [string]$_.label -ceq $Label }).Count -ne 0) { throw "Heap snapshot checkpoint '$Label' was already recorded." }
    $triggeredAtUtc = Get-P914Utc
    $checkpoint = [ordered]@{
        label = $Label; windowId = $windowId; memoryWindowCompletedAtUtc = [string]$MemoryWindow.completedAtUtc
        nextUiActionKind = [string]$RouteCheckpoint.nextUiActionKind; nextUiActionExpectedNotStarted = $true
        checkpointObservedAtUtc = $triggeredAtUtc; routeState = $RouteCheckpoint.routeState
    }
    $response = Invoke-P917HeapWorkerRequest $Diagnostic 'snapshot' ([ordered]@{
        label = $Label; windowId = $windowId; routeCheckpoint = $checkpoint
    }) ([int]$Diagnostic.workerCommandResponseTimeoutSeconds)
    $workerResult = $response.result
    $workerSnapshot = if ($null -ne $workerResult) { $workerResult.snapshot } else { $null }
    $snapshot = [ordered]@{
        label = $Label; windowId = $windowId; processId = [Int64]$Diagnostic.processId; snapshotOption = 'Heap'
        commandStartedAtUtc = if ($null -ne $workerSnapshot) { [string]$workerSnapshot.commandStartedAtUtc } else { $null }
        commandCompletedAtUtc = if ($null -ne $workerSnapshot) { [string]$workerSnapshot.commandCompletedAtUtc } else { $null }
        returnCode = if ($null -ne $workerSnapshot) { $workerSnapshot.returnCode } else { $null }
        timedOut = if ($null -ne $workerSnapshot) { [bool]$workerSnapshot.timedOut } else { $null }
        stdout = if ($null -ne $workerSnapshot) { [string]$workerSnapshot.stdout } elseif ($null -ne $workerResult) { [string]$workerResult.failure } else { [string]$response.status }
        stderr = if ($null -ne $workerSnapshot) { [string]$workerSnapshot.stderr } else { $null }
        launchError = if ($null -ne $workerSnapshot) { $workerSnapshot.launchError } else { $null }
        status = if ([string]$response.status -ceq 'captured' -and $null -ne $workerSnapshot -and [string]$workerSnapshot.status -ceq 'captured') { 'captured' } else { 'failed' }
        routeCheckpoint = $checkpoint
        memoryWindow = [ordered]@{
            windowId = $windowId; state = [string]$MemoryWindow.state; phase = [string]$MemoryWindow.phase
            matchedState = $MemoryWindow.matchedState; startedAtUtc = [string]$MemoryWindow.startedAtUtc; completedAtUtc = [string]$MemoryWindow.completedAtUtc
            sampleCount = [int]$MemoryWindow.sampleCount; sampleTimestampsUtc = @($MemoryWindow.samples | ForEach-Object { [string]$_.capturedAtUtc })
            samplesDiagnosticOnly = @($MemoryWindow.samples | ForEach-Object { [ordered]@{ capturedAtUtc = [string]$_.capturedAtUtc; workingSetBytes = $_.workingSetBytes; privateBytes = $_.privateBytes; handles = $_.handles; threadCount = $_.threadCount } })
        }
        diagnosticOnly = $true; accepted = $false; acceptanceUse = 'Excluded: trace overhead may affect process-memory readings.'
    }
    $Diagnostic.snapshots.Add($snapshot)
    Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
    if ([string]$snapshot.status -cne 'captured') { throw "WPR heap snapshot '$Label' failed: $($snapshot.stdout) $($snapshot.stderr)" }
}

function Stop-P917HeapSnapshotTrace([object]$Diagnostic) {
    if ($null -eq $Diagnostic -or [bool]$Diagnostic.cleanupCompleted) { return }
    $Diagnostic.status = 'stopping'
    if ($null -eq $Diagnostic.workerProcessId) {
        $Diagnostic.cleanupErrors.Add('Elevated WPR worker was not launched; no WPR cleanup command was issued.')
        $Diagnostic.cleanupCompleted = $true
        $Diagnostic.status = 'failed'
        $Diagnostic.completedAtUtc = Get-P914Utc
        Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
        return
    }

    $final = $null
    try {
        $response = Invoke-P917HeapWorkerRequest $Diagnostic 'stop' $null ([int]$Diagnostic.workerStopResponseTimeoutSeconds)
        $Diagnostic.workerStatus = $response.result
        if ([string]$response.status -cne 'completed' -or -not [bool]$response.result.cleanupCompleted) {
            $Diagnostic.cleanupErrors.Add('Elevated WPR worker stop response did not confirm completed cleanup.')
        }
        $watch = [Diagnostics.Stopwatch]::StartNew()
        while ($watch.Elapsed.TotalSeconds -lt 15) {
            if (Test-Path -LiteralPath ([string]$Diagnostic.workerFinalStatusPath) -PathType Leaf) {
                $final = Read-P914Json ([string]$Diagnostic.workerFinalStatusPath)
                break
            }
            $worker = Get-Process -Id ([int]$Diagnostic.workerProcessId) -ErrorAction SilentlyContinue
            if ($null -eq $worker) { break }
            Start-Sleep -Milliseconds 100
        }
        if ($null -eq $final) { throw 'Elevated WPR worker did not publish its final status within 15 seconds after its stop response.' }
        if ([string]$final.schema -cne 'classmngr-phase9-p9-17-heap-worker-final-v1' -or
            [string]$final.runId -cne [string]$Diagnostic.runId -or [string]$final.nonce -cne [string]$Diagnostic.nonce -or
            [Int64]$final.workerProcessId -ne [Int64]$Diagnostic.workerProcessId -or
            -not [bool]$final.diagnosticOnly -or [bool]$final.accepted) {
            throw 'Elevated WPR worker final status failed schema, run, nonce, process, or diagnostic-only binding.'
        }
        $Diagnostic.workerStatus = $final
        $Diagnostic.workerExitedAtUtc = [string]$final.completedAtUtc
        $Diagnostic.sessionStopped = [bool]$final.sessionStopped
        $Diagnostic.snapshotConfigDisabled = [bool]$final.snapshotConfigDisabled
        $Diagnostic.noActiveWprSessionAtEnd = $final.noActiveWprSessionAtEnd
        $Diagnostic.cleanupErrors.AddRange([string[]]@($final.cleanupErrors))
        $Diagnostic.traceSha256 = $final.traceSha256
        $Diagnostic.traceSizeBytes = $final.traceSizeBytes
        $Diagnostic.workerFinalSnapshotCount = @($final.snapshots).Count
        foreach ($workerSnapshot in @($final.snapshots)) {
            $parentSnapshot = @($Diagnostic.snapshots | Where-Object { [string]$_.label -ceq [string]$workerSnapshot.label } | Select-Object -First 1)
            if ($parentSnapshot.Count -eq 1) { $parentSnapshot[0].workerSnapshot = $workerSnapshot }
            else { $Diagnostic.cleanupErrors.Add("Worker final snapshot '$($workerSnapshot.label)' has no matching sampled-window record.") }
        }
        if (@($final.snapshots).Count -ne 3 -or @($Diagnostic.snapshots).Count -ne 3) { $Diagnostic.cleanupErrors.Add('Worker and route evidence do not each contain exactly three heap snapshot checkpoints.') }
        if ([string]$final.status -cne 'completed' -or -not [bool]$final.cleanupCompleted) {
            $Diagnostic.cleanupErrors.Add("Elevated WPR worker final status was $($final.status).")
        }
    } catch { $Diagnostic.cleanupErrors.Add('Bounded elevated WPR worker cleanup failed: ' + $_.Exception.Message) }

    $workerProcess = Get-Process -Id ([int]$Diagnostic.workerProcessId) -ErrorAction SilentlyContinue
    if ($null -ne $workerProcess) {
        try {
            $workerProcess.WaitForExit(10000)
            if (-not $workerProcess.HasExited) {
                $workerProcess.Kill()
                if (-not $workerProcess.WaitForExit(5000)) { $Diagnostic.cleanupErrors.Add('Elevated WPR worker remained alive after bounded termination.') }
                else { $Diagnostic.cleanupErrors.Add('Elevated WPR worker required forced termination after bounded cleanup.') }
            }
        } catch { $Diagnostic.cleanupErrors.Add('Elevated WPR worker process exit check failed: ' + $_.Exception.Message) }
        finally { $workerProcess.Dispose() }
    }
    if (Test-Path -LiteralPath ([string]$Diagnostic.tracePath) -PathType Leaf) {
        $Diagnostic.traceSizeBytes = [Int64](Get-Item -LiteralPath ([string]$Diagnostic.tracePath)).Length
        $Diagnostic.traceSha256 = Get-P914FileSha256 ([string]$Diagnostic.tracePath)
    }
    $Diagnostic.cleanupCompleted = $true
    $Diagnostic.completedAtUtc = Get-P914Utc
    $allSnapshotsCaptured = (@($Diagnostic.snapshots | Where-Object { [string]$_.status -cne 'captured' }).Count -eq 0 -and @($Diagnostic.snapshots).Count -eq 3)
    $cleanupOkay = ([bool]$Diagnostic.sessionStopped -and [bool]$Diagnostic.snapshotConfigDisabled -and [bool]$Diagnostic.noActiveWprSessionAtEnd -and $Diagnostic.cleanupErrors.Count -eq 0)
    $Diagnostic.status = if ($allSnapshotsCaptured -and $cleanupOkay -and $null -ne $Diagnostic.traceSha256) { 'completed' } else { 'failed' }
    Write-P914Json ([string]$Diagnostic.manifestPath) $Diagnostic
}

function Complete-P917HeapSnapshotTrace([object]$Diagnostic) {
    if ($null -eq $Diagnostic) { return }
    Stop-P917HeapSnapshotTrace $Diagnostic
    if ([string]$Diagnostic.status -cne 'completed') {
        throw "P9-17 heap snapshot diagnostic did not complete cleanly: $(@($Diagnostic.cleanupErrors) -join ' ')"
    }
}

function Get-P917BlockedBuildEvidence {
    $manifestPath = Join-Path $RepositoryRoot $P917BlockedManifestRelativePath
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Recorded P9-17 exact-tree build blocker is missing: $manifestPath" }
    $blockedManifest = Read-P914Json $manifestPath
    if ([string]$blockedManifest.taskId -cne $TaskId -or [string]$blockedManifest.status -cne 'failed' -or [string]$blockedManifest.failure.message -notmatch 'Fresh exact-tree Release build failed with exit code 1') { throw 'Recorded P9-17 attempt does not prove the exact-tree Release build blocker.' }
    $blockedRunRoot = Split-Path -Parent $manifestPath
    $buildLogPath = Join-Path (Join-Path $blockedRunRoot 'logs') 'local-release-build.log'
    if (-not (Test-Path -LiteralPath $buildLogPath -PathType Leaf)) { throw "Recorded P9-17 build log is missing: $buildLogPath" }
    $buildLog = Get-Content -LiteralPath $buildLogPath -Raw -Encoding UTF8
    if ($buildLog -notmatch 'UnauthorizedAccessException' -or $buildLog -notmatch 'ZERO_CHECK\.vcxproj' -or $buildLog -notmatch 'error MSB4018' -or $buildLog -notmatch 'exitCode=1') { throw 'Recorded P9-17 build log does not prove the expected ZERO_CHECK access failure.' }
    $blockedRunRelativePath = Split-Path -Parent $P917BlockedManifestRelativePath
    $buildLogRelativePath = (Join-Path $blockedRunRelativePath 'logs/local-release-build.log').Replace('\', '/')
    return [ordered]@{
        runId = [string]$blockedManifest.runId; manifestPath = $P917BlockedManifestRelativePath
        manifestSha256 = Get-P914FileSha256 $manifestPath; buildLogPath = $buildLogRelativePath
        buildLogSha256 = Get-P914FileSha256 $buildLogPath; exitCode = 1
        blocker = 'MSBuild ZERO_CHECK GetOutOfDateItems failed with UnauthorizedAccessException before compile.'
    }
}

function Get-P917SourceDeltaProof([string]$EvidenceDirectory, [object]$CurrentSource) {
    $acceptedManifestPath = Join-Path $RepositoryRoot $P916ManifestRelativePath
    if (-not (Test-Path -LiteralPath $acceptedManifestPath -PathType Leaf)) { throw "Accepted P9-16 manifest is missing: $acceptedManifestPath" }
    $acceptedManifest = Read-P914Json $acceptedManifestPath
    if ([string]$acceptedManifest.taskId -cne 'P9_16_PACKAGED_NEXT_WORKSPACE_REPEATED_CYCLES' -or [string]$acceptedManifest.status -cne 'passed') { throw 'P9-17 source-delta ancestry is not the passed accepted P9-16 run.' }
    if ([string]$acceptedManifest.validation.status -cne 'passed' -or [int]$acceptedManifest.validation.exitCode -ne 0) { throw 'P9-16 source-delta ancestry lacks passed evidence validation.' }
    $accepted = $acceptedManifest.packageProvenance
    $acceptedRoot = Split-Path -Parent $acceptedManifestPath
    $acceptedSourcePath = Join-Path $acceptedRoot ([string]$accepted.sourceInventoryPath)
    if (-not (Test-Path -LiteralPath $acceptedSourcePath -PathType Leaf) -or (Get-P914FileSha256 $acceptedSourcePath) -cne [string]$accepted.sourceInventoryFileSha256) { throw 'Accepted P9-16 source inventory changed or is missing.' }
    $acceptedSource = Read-P914Json $acceptedSourcePath
    if ([string]$acceptedSource.sha256 -cne [string]$accepted.currentSourceSha256 -or [string]$acceptedSource.repositoryRoot -ine $RepositoryRoot) { throw 'Accepted P9-16 source inventory fingerprint or root differs from its manifest.' }
    if (($CurrentSource.includedRoots -join "`n") -cne (@($acceptedSource.includedRoots) -join "`n")) { throw 'Current P9-17 and accepted P9-16 source inventories use different roots.' }

    $priorMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    $currentMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $acceptedSource.files) { $priorMap.Add([string]$row.path, $row) }
    foreach ($row in $CurrentSource.files) { $currentMap.Add([string]$row.path, $row) }
    $permitted = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($change in $P917AuthorizedProductSourceChanges) { $permitted.Add([string]$change.path, [ordered]@{ kind = 'product'; change = $change }) }
    foreach ($change in $P917AuthorizedTestOnlySourceChanges) {
        if ($permitted.ContainsKey([string]$change.path)) { throw "P9-17 source delta path is authorized more than once: $($change.path)" }
        $permitted.Add([string]$change.path, [ordered]@{ kind = 'test'; change = $change })
    }
    foreach ($change in $P917AuthorizedHarnessSourceChanges) {
        if ($permitted.ContainsKey([string]$change.path)) { throw "P9-17 source delta path is authorized more than once: $($change.path)" }
        $permitted.Add([string]$change.path, [ordered]@{ kind = 'harness'; change = $change })
    }

    $observed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $productChangeRows = [Collections.Generic.List[object]]::new()
    $testChangeRows = [Collections.Generic.List[object]]::new()
    $harnessChangeRows = [Collections.Generic.List[object]]::new()
    foreach ($path in $priorMap.Keys) {
        if (-not $currentMap.ContainsKey($path)) { throw "Previously inventoried P9-16 source disappeared: $path" }
        $before = $priorMap[$path]; $now = $currentMap[$path]
        if ([Int64]$before.sizeBytes -eq [Int64]$now.sizeBytes -and [string]$before.sha256 -ceq [string]$now.sha256) { continue }
        if (-not $permitted.ContainsKey($path)) { throw "P9-16 source changed outside the exact P9-17 product/test allowlist: $path" }
        $entry = $permitted[$path]; $change = $entry.change
        if ([string]$before.path -cne [string]$change.path -or [string]$now.path -cne [string]$change.path -or
            [Int64]$before.sizeBytes -ne [Int64]$change.beforeSizeBytes -or [string]$before.sha256 -cne [string]$change.beforeSha256 -or
            [Int64]$now.sizeBytes -ne [Int64]$change.afterSizeBytes -or [string]$now.sha256 -cne [string]$change.afterSha256) {
            throw "P9-17 source delta differs from the pinned before/after hashes: $path"
        }
        [void]$observed.Add([string]$change.path)
        if ([string]$entry.kind -ceq 'product') {
            $productChangeRows.Add([ordered]@{
                path = [string]$change.path; beforeSizeBytes = [Int64]$before.sizeBytes; beforeSha256 = [string]$before.sha256
                afterSizeBytes = [Int64]$now.sizeBytes; afterSha256 = [string]$now.sha256
                classification = 'authorized-p9-17-production-source-change-included-in-current-product-fingerprint'
                productFingerprintIncluded = $true
            })
        } elseif ([string]$entry.kind -ceq 'test') {
            $boundaryFinding = if ([string]$change.path -ceq 'cmake/tests/next.cmake') {
                "The pinned CMake delta adds implementation sources only to standalone $($change.target)."
            } else {
                "The changed test source is compiled only into standalone $($change.target)."
            }
            $boundary = @([ordered]@{
                path = 'cmake/tests/next.cmake'; lines = [string]$change.cmakeLines
                finding = $boundaryFinding
            }) + $P917TestOnlySourceBoundaryEvidence
            $testChangeRows.Add([ordered]@{
                path = [string]$change.path; beforeSizeBytes = [Int64]$before.sizeBytes; beforeSha256 = [string]$before.sha256
                afterSizeBytes = [Int64]$now.sizeBytes; afterSha256 = [string]$now.sha256
                classification = 'test-only-source-change-excluded-from-product-fingerprint'
                productFingerprintExcluded = $true; cmakeBoundaryEvidence = $boundary
            })
        } else {
            $harnessChangeRows.Add([ordered]@{
                path = [string]$change.path; beforeSizeBytes = [Int64]$before.sizeBytes; beforeSha256 = [string]$before.sha256
                afterSizeBytes = [Int64]$now.sizeBytes; afterSha256 = [string]$now.sha256
                classification = 'shared-p9-17-harness-dependency-change-excluded-from-product-fingerprint'
                productFingerprintExcluded = $true
            })
        }
    }
    if ($observed.Count -ne $permitted.Count) { throw 'The exact authorized P9-17 product, test-only, and harness source delta set was not observed.' }
    foreach ($path in $permitted.Keys) { if (-not $observed.Contains($path)) { throw "A declared P9-17 source delta is missing: $path" } }
    foreach ($path in $HarnessSources) { if (-not $currentMap.ContainsKey([string]$path)) { throw "Declared P9-17 harness source is missing: $path" } }
    $added = @($currentMap.Keys | Where-Object { -not $priorMap.ContainsKey($_) } | ForEach-Object { ([string]$_).Replace('\', '/') } | Sort-Object)
    if (($added -join "`n") -cne (($OwnedSources | Sort-Object) -join "`n")) { throw "Unexpected P9-17 source additions: $($added -join ', ')" }

    $p913Path = [IO.Path]::GetFullPath([string]$accepted.priorP913ManifestPath)
    if (-not (Test-Path -LiteralPath $p913Path -PathType Leaf) -or (Get-P914FileSha256 $p913Path) -cne [string]$accepted.priorP913ManifestSha256) { throw 'Accepted P9-13 source ancestry manifest changed or is missing.' }
    $p913 = Read-P914Json $p913Path
    if ([string]$p913.taskId -cne 'P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE' -or [string]$p913.status -cne 'passed') { throw 'P9-13 source ancestry is not a passed Release run.' }
    $p913Root = Split-Path -Parent $p913Path
    $derivedPath = Join-Path $p913Root ([string]$p913.source.derivedReceiptPath)
    if (-not (Test-Path -LiteralPath $derivedPath -PathType Leaf) -or (Get-P914FileSha256 $derivedPath) -cne [string]$p913.source.derivedReceiptSha256) { throw 'Accepted P9-13 derivation receipt changed or is missing.' }
    $derived = Read-P914Json $derivedPath
    if ([string]$derived.status -cne 'verified-script-only-derivation' -or [string]$derived.source.productSourceSha256 -cne [string]$accepted.productSourceSha256) { throw 'P9-13 derivation does not support the accepted P9-16 product-source fingerprint.' }
    $legacyExcluded = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $derived.source.allowedPaths) { [void]$legacyExcluded.Add([string]$path) }
    foreach ($path in $P914Sources + $P915Sources + $P916Sources) { [void]$legacyExcluded.Add([string]$path) }
    $acceptedProductRows = @($acceptedSource.files | Where-Object { -not $legacyExcluded.Contains([string]$_.path) })
    if ((Get-P914InventoryFingerprint $acceptedProductRows) -cne [string]$accepted.productSourceSha256) { throw 'Accepted P9-16 source inventory does not reproduce its recorded product fingerprint.' }

    $excluded = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $legacyExcluded) { [void]$excluded.Add([string]$path) }
    foreach ($path in $HarnessSources) { [void]$excluded.Add([string]$path) }
    foreach ($change in $P917AuthorizedTestOnlySourceChanges) { [void]$excluded.Add([string]$change.path) }
    $productDeltaPaths = @($P917AuthorizedProductSourceChanges | ForEach-Object { [string]$_.path })
    foreach ($path in $productDeltaPaths) {
        if (-not $currentMap.ContainsKey($path) -or -not $priorMap.ContainsKey($path)) { throw "Declared P9-17 product source is missing from a full inventory: $path" }
        if ($excluded.Contains($path)) { throw "Authorized production source was excluded from the current product fingerprint: $path" }
    }
    foreach ($path in $excluded) { if (-not $currentMap.ContainsKey([string]$path)) { throw "Declared product-fingerprint exclusion is absent from the current source inventory: $path" } }

    $productRows = @($CurrentSource.files | Where-Object { -not $excluded.Contains([string]$_.path) })
    $productSha = Get-P914InventoryFingerprint $productRows
    $productMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $productRows) { $productMap.Add([string]$row.path, $row) }
    foreach ($path in $productDeltaPaths) { if (-not $productMap.ContainsKey($path)) { throw "Authorized production source is absent from the current product-source inventory: $path" } }

    $commonExcluded = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $excluded) { [void]$commonExcluded.Add([string]$path) }
    foreach ($path in $productDeltaPaths) { [void]$commonExcluded.Add([string]$path) }
    $acceptedCommonRows = @($acceptedSource.files | Where-Object { -not $commonExcluded.Contains([string]$_.path) })
    $currentCommonRows = @($CurrentSource.files | Where-Object { -not $commonExcluded.Contains([string]$_.path) })
    $acceptedCommonMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    $currentCommonMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $acceptedCommonRows) { $acceptedCommonMap.Add([string]$row.path, $row) }
    foreach ($row in $currentCommonRows) { $currentCommonMap.Add([string]$row.path, $row) }
    if ($acceptedCommonMap.Count -ne $currentCommonMap.Count) { throw 'Accepted and current unchanged-common product inventories have different path counts.' }
    foreach ($path in $acceptedCommonMap.Keys) {
        if (-not $currentCommonMap.ContainsKey($path)) { throw "Unchanged-common P9-16 source is missing from P9-17: $path" }
        $before = $acceptedCommonMap[$path]; $now = $currentCommonMap[$path]
        if ([Int64]$before.sizeBytes -ne [Int64]$now.sizeBytes -or [string]$before.sha256 -cne [string]$now.sha256) { throw "A source changed outside the exact P9-17 allowlist: $path" }
    }
    $commonSha = Get-P914InventoryFingerprint $currentCommonRows
    if ($commonSha -cne (Get-P914InventoryFingerprint $acceptedCommonRows)) { throw 'Unchanged-common product-source fingerprints differ.' }

    $currentSourcePath = Join-Path $EvidenceDirectory 'source-files.json'
    $productSourcePath = Join-Path $EvidenceDirectory 'product-source-files.json'
    $harnessSourcePath = Join-Path $EvidenceDirectory 'harness-source-files.json'
    Write-P914Json $currentSourcePath $CurrentSource
    $productInventory = [ordered]@{ repositoryRoot = $RepositoryRoot; includedRoots = @($CurrentSource.includedRoots); sha256 = $productSha; fileCount = $productRows.Count; files = $productRows }
    $harnessRows = @($CurrentSource.files | Where-Object { $HarnessSources -contains [string]$_.path })
    $harnessInventory = [ordered]@{ repositoryRoot = $RepositoryRoot; includedRoots = $HarnessSources; sha256 = (Get-P914InventoryFingerprint $harnessRows); fileCount = $harnessRows.Count; files = $harnessRows }
    Write-P914Json $productSourcePath $productInventory
    Write-P914Json $harnessSourcePath $harnessInventory

    $orderedProductChangeRows = @($productChangeRows.ToArray() | Sort-Object { ([string]$_.path).ToLowerInvariant() })
    $orderedTestChangeRows = @($testChangeRows.ToArray() | Sort-Object { ([string]$_.path).ToLowerInvariant() })
    $orderedHarnessChangeRows = @($harnessChangeRows.ToArray() | Sort-Object { ([string]$_.path).ToLowerInvariant() })
    $orderedProductChangePaths = @($P917AuthorizedProductSourceChanges | ForEach-Object { [string]$_.path } | Sort-Object { ([string]$_).ToLowerInvariant() })
    $orderedTestChangePaths = @($P917AuthorizedTestOnlySourceChanges | ForEach-Object { [string]$_.path } | Sort-Object { ([string]$_).ToLowerInvariant() })
    $orderedHarnessChangePaths = @($P917AuthorizedHarnessSourceChanges | ForEach-Object { [string]$_.path } | Sort-Object { ([string]$_).ToLowerInvariant() })

    $sourceEquivalence = [ordered]@{
        schema = 'classmngr-phase9-p9-17-source-delta-proof-v1'
        classification = 'current-p9-17-product-source-with-pinned-deltas'
        acceptedSourceManifestPath = [IO.Path]::GetFullPath($acceptedManifestPath)
        acceptedSourceManifestSha256 = Get-P914FileSha256 $acceptedManifestPath
        acceptedSourceInventoryPath = [string]$accepted.sourceInventoryPath
        acceptedSourceInventorySha256 = [string]$accepted.sourceInventoryFileSha256
        acceptedSourceTreeSha256 = [string]$accepted.currentSourceSha256
        acceptedP916ProductSourceSha256 = [string]$accepted.productSourceSha256
        currentP917ProductSourceSha256 = $productSha
        unchangedCommonPreviousProductSourceSha256 = $commonSha
        unchangedCommonCurrentProductSourceSha256 = $commonSha
        unchangedCommonProductFileCount = $currentCommonRows.Count
        addedHarnessPaths = $added
        changedPriorProductSourcePaths = $orderedProductChangePaths
        changedPriorTestOnlySourcePaths = $orderedTestChangePaths
        changedPriorHarnessSourcePaths = $orderedHarnessChangePaths
        authorizedProductSourceChanges = $orderedProductChangeRows
        authorizedTestOnlySourceChanges = $orderedTestChangeRows
        authorizedHarnessSourceChanges = $orderedHarnessChangeRows
        productFingerprintExcludedPaths = @($excluded | Sort-Object)
    }
    return [ordered]@{
        acceptedManifest = $acceptedManifest; accepted = $accepted; acceptedSource = $acceptedSource
        priorP913ManifestPath = $p913Path; priorP913ManifestSha256 = Get-P914FileSha256 $p913Path
        currentSourceInventory = [ordered]@{ relativePath = 'source-files.json'; sha256 = Get-P914FileSha256 $currentSourcePath; treeSha256 = [string]$CurrentSource.sha256; fileCount = [int]$CurrentSource.fileCount }
        productSourceInventory = [ordered]@{ relativePath = 'product-source-files.json'; sha256 = Get-P914FileSha256 $productSourcePath; treeSha256 = $productSha; fileCount = [int]$productRows.Count }
        harnessSourceInventory = [ordered]@{ relativePath = 'harness-source-files.json'; sha256 = Get-P914FileSha256 $harnessSourcePath; treeSha256 = [string]$harnessInventory.sha256; fileCount = [int]$harnessInventory.fileCount }
        productSourceSha256 = $productSha; productSourceRows = $productRows; harnessSourceRows = $harnessRows
        sourceEquivalence = $sourceEquivalence; addedHarnessPaths = $added
    }
}

function Get-P917Package([string]$EvidenceDirectory) {
    $blockedBuild = Get-P917BlockedBuildEvidence
    $acceptedManifestPath = Join-Path $RepositoryRoot $P916ManifestRelativePath
    if (-not (Test-Path -LiteralPath $acceptedManifestPath -PathType Leaf)) { throw "Accepted P9-16 manifest is missing: $acceptedManifestPath" }
    $acceptedManifest = Read-P914Json $acceptedManifestPath
    if ([string]$acceptedManifest.taskId -cne 'P9_16_PACKAGED_NEXT_WORKSPACE_REPEATED_CYCLES' -or [string]$acceptedManifest.status -cne 'passed') { throw 'P9-16 fallback ancestry is not a passed accepted run.' }
    if ([string]$acceptedManifest.validation.status -cne 'passed' -or [int]$acceptedManifest.validation.exitCode -ne 0) { throw 'P9-16 accepted run does not have passed evidence validation.' }
    $acceptedRoot = Split-Path -Parent $acceptedManifestPath
    $accepted = $acceptedManifest.packageProvenance
    if (-not [bool]$accepted.reusedPackage -or [bool]$accepted.freshExactTreeRebuild -or [string]$accepted.configuration -cne 'Release' -or [string]$accepted.platform -cne 'x64' -or [string]$accepted.architecture -cne 'x64') { throw 'P9-16 package ancestry does not describe the accepted x64 Release package.' }

    $acceptedSourcePath = Join-Path $acceptedRoot ([string]$accepted.sourceInventoryPath)
    if (-not (Test-Path -LiteralPath $acceptedSourcePath -PathType Leaf) -or (Get-P914FileSha256 $acceptedSourcePath) -cne [string]$accepted.sourceInventoryFileSha256) { throw 'Accepted P9-16 source inventory changed or is missing.' }
    $acceptedSource = Read-P914Json $acceptedSourcePath
    $currentSource = Get-P914SourceInventory $RepositoryRoot @($acceptedSource.includedRoots)
    $sourceDelta = Get-P917SourceDeltaProof $EvidenceDirectory $currentSource
    $acceptedSource = $sourceDelta.acceptedSource
    $p913Path = [string]$sourceDelta.priorP913ManifestPath
    $productSha = [string]$sourceDelta.productSourceSha256
    $sourceEquivalence = $sourceDelta.sourceEquivalence
    $productRows = @($sourceDelta.productSourceRows)
    $harnessRows = @($sourceDelta.harnessSourceRows)
    $currentSourcePath = Join-Path $EvidenceDirectory 'source-files.json'
    $productSourcePath = Join-Path $EvidenceDirectory 'product-source-files.json'
    $harnessSourcePath = Join-Path $EvidenceDirectory 'harness-source-files.json'

    $packageRoot = [IO.Path]::GetFullPath([string]$accepted.packageRoot)
    $package = Get-P914CanonicalInventory $packageRoot
    if ([string]$package.sha256 -cne [string]$accepted.packageSha256 -or [int]$package.fileCount -ne [int]$accepted.packageFileCount) { throw 'Current P9-16 package tree differs from its recorded full inventory.' }
    $acceptedPackagePath = Join-Path $acceptedRoot ([string]$accepted.packageInventoryPath)
    if (-not (Test-Path -LiteralPath $acceptedPackagePath -PathType Leaf) -or (Get-P914FileSha256 $acceptedPackagePath) -cne [string]$accepted.packageInventoryFileSha256) { throw 'Accepted P9-16 package inventory changed or is missing.' }
    $acceptedPackageInventory = Read-P914Json $acceptedPackagePath
    if ((Get-P914InventoryFingerprint @($acceptedPackageInventory.files)) -cne [string]$package.sha256 -or [int]$acceptedPackageInventory.fileCount -ne [int]$package.fileCount) { throw 'Current P9-16 package files differ from the accepted complete package inventory.' }
    $packageInventoryPath = Join-Path $EvidenceDirectory 'package-files.json'
    Write-P914Json $packageInventoryPath $package

    $applicationPath = Join-Path $packageRoot 'ClassMngrNext.exe'
    if ([IO.Path]::GetFullPath($applicationPath) -cne [IO.Path]::GetFullPath([string]$accepted.applicationPath) -or -not (Test-Path -LiteralPath $applicationPath -PathType Leaf)) { throw 'Accepted P9-16 application path is missing or inconsistent with its package root.' }
    $applicationSha = Get-P914FileSha256 $applicationPath
    if ($applicationSha -cne [string]$accepted.applicationSha256 -or (Get-P914PeMachine $applicationPath) -ne 0x8664) { throw 'Accepted P9-16 application hash or x64 architecture is invalid.' }

    $releasePath = [IO.Path]::GetFullPath([string]$accepted.acceptedReleaseReceiptPath)
    if (-not (Test-Path -LiteralPath $releasePath -PathType Leaf) -or (Get-P914FileSha256 $releasePath) -cne [string]$accepted.acceptedReleaseReceiptSha256) { throw 'Accepted P9-16 Release receipt changed or is missing.' }
    $release = Read-P914Json $releasePath
    if ([string]$release.status -cne 'succeeded' -or [string]$release.configuration -cne 'Release' -or [string]$release.platform -cne 'x64' -or [int]$release.build.exitCode -ne 0 -or [string]$release.releaseExecutableSha256 -cne $applicationSha) { throw 'Accepted Release receipt does not prove the P9-16 x64 package executable.' }

    $resourceRoot = [IO.Path]::GetFullPath([string]$accepted.resourceRoot)
    $resource = Get-P914CanonicalInventory $resourceRoot
    if ([string]$resource.sha256 -cne [string]$accepted.resourceInventorySha256 -or [string]$resource.sha256 -cne [string]$release.generatedResources.stagedResourceInputs.sha256) { throw 'P9-16 staged resource tree differs from its accepted Release receipt.' }
    $acceptedResourcePath = Join-Path $acceptedRoot ([string]$accepted.resourceInventoryPath)
    if (-not (Test-Path -LiteralPath $acceptedResourcePath -PathType Leaf) -or (Get-P914FileSha256 $acceptedResourcePath) -cne [string]$accepted.resourceInventoryFileSha256) { throw 'Accepted P9-16 resource inventory changed or is missing.' }
    $acceptedResourceInventory = Read-P914Json $acceptedResourcePath
    if ((Get-P914InventoryFingerprint @($acceptedResourceInventory.files)) -cne [string]$resource.sha256 -or [int]$acceptedResourceInventory.fileCount -ne [int]$resource.fileCount) { throw 'Current staged resources differ from the accepted P9-16 resource inventory.' }
    $resourceInventoryPath = Join-Path $EvidenceDirectory 'resource-files.json'
    Write-P914Json $resourceInventoryPath $resource

    $cachePath = [IO.Path]::GetFullPath([string]$accepted.configuredCachePath)
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf) -or (Get-P914FileSha256 $cachePath) -cne [string]$accepted.configuredCacheSha256 -or (Get-P914FileSha256 $cachePath) -cne [string]$release.cacheSha256After) { throw 'Configured Windows x64 Release CMake cache differs from accepted P9-16 build evidence.' }
    $cacheHome = [IO.Path]::GetFullPath((Get-P917CacheValue $cachePath 'CMAKE_HOME_DIRECTORY'))
    $generator = Get-P917CacheValue $cachePath 'CMAKE_GENERATOR'
    $platform = Get-P917CacheValue $cachePath 'CMAKE_GENERATOR_PLATFORM'
    $releasePackage = Get-P917CacheValue $cachePath 'CLASSMNGR_NEXT_RELEASE_PACKAGE'
    if ($cacheHome -ine $RepositoryRoot -or $generator -notmatch '^Visual Studio' -or $platform -notin @('x64', '') -or $releasePackage -cne 'ON') { throw 'Accepted P9-16 Release cache is not configured for this source tree and Windows x64 package target.' }

    return [ordered]@{
        buildMode = 'verified-accepted-p916-package-fallback'; configuration = 'Release'; platform = 'x64'; architecture = 'x64'
        buildExitCode = $null; freshExactTreeBuild = $false; freshBuildAttemptedInCurrentRun = $false
        fallbackReason = 'Fresh exact-tree Release build is blocked at MSBuild ZERO_CHECK by the previously captured UnauthorizedAccessException.'
        buildBlockEvidence = $blockedBuild
        acceptedRunManifestPath = [IO.Path]::GetFullPath($acceptedManifestPath); acceptedRunManifestSha256 = Get-P914FileSha256 $acceptedManifestPath; acceptedRunId = [string]$acceptedManifest.runId
        priorP913ManifestPath = $p913Path; priorP913ManifestSha256 = Get-P914FileSha256 $p913Path
        productSourceSha256 = $productSha; currentSourceSha256 = [string]$currentSource.sha256
        acceptedP916ProductSourceSha256 = [string]$accepted.productSourceSha256
        sourceEquivalence = $sourceEquivalence
        currentSourceInventory = [ordered]@{ relativePath = 'source-files.json'; sha256 = Get-P914FileSha256 $currentSourcePath; treeSha256 = [string]$currentSource.sha256; fileCount = [int]$currentSource.fileCount }
        productSourceInventory = [ordered]@{ relativePath = 'product-source-files.json'; sha256 = Get-P914FileSha256 $productSourcePath; treeSha256 = $productSha; fileCount = [int]$productRows.Count }
        harnessSourceInventory = [ordered]@{ relativePath = 'harness-source-files.json'; sha256 = Get-P914FileSha256 $harnessSourcePath; treeSha256 = [string]$harnessSource.sha256; fileCount = [int]$harnessSource.fileCount }
        packageRoot = $packageRoot; packageSha256 = [string]$package.sha256; packageFileCount = [int]$package.fileCount
        packageInventory = [ordered]@{ relativePath = 'package-files.json'; sha256 = Get-P914FileSha256 $packageInventoryPath; treeSha256 = [string]$package.sha256; fileCount = [int]$package.fileCount }
        applicationPath = [IO.Path]::GetFullPath($applicationPath); applicationSha256 = $applicationSha; machineType = [int](Get-P914PeMachine $applicationPath)
        acceptedReleaseReceiptPath = $releasePath; acceptedReleaseReceiptSha256 = Get-P914FileSha256 $releasePath
        generator = $generator; generatorPlatform = $platform; cmakeHomeDirectory = $cacheHome; cachePath = $cachePath; cacheSha256 = Get-P914FileSha256 $cachePath
        configuredCachePath = $cachePath; configuredCacheSha256 = Get-P914FileSha256 $cachePath
        resourceRoot = $resourceRoot; resourceInventorySha256 = [string]$resource.sha256
        resourceInventory = [ordered]@{ relativePath = 'resource-files.json'; sha256 = Get-P914FileSha256 $resourceInventoryPath; treeSha256 = [string]$resource.sha256; fileCount = [int]$resource.fileCount }
    }
}

function Get-P917ExactTreePackage([string]$EvidenceDirectory) {
    $observerExecutionContext = Get-P917CurrentExecutionContext
    if ([string]::IsNullOrWhiteSpace($ReleaseBuildReceiptPath)) {
        throw 'P9-17 requires a source-bound exact-current-tree Release build receipt.'
    }
    if ($DefaultProcessHeapDiagnostic) {
        if ([IO.Path]::GetFullPath($BuildDirectory) -ine $P917StandardBuildDirectory -and
            [IO.Path]::GetFullPath($BuildDirectory) -ine $P917SaveStageDiagnosticBuildDirectory) {
            throw "P9-17 save-stage diagnostic must use the standard build directory or the pinned diagnostic directory: $P917SaveStageDiagnosticBuildDirectory"
        }
    } elseif ([IO.Path]::GetFullPath($BuildDirectory) -ine $P917StandardBuildDirectory) {
        throw "P9-17 acceptance must use the standard Windows x64 Release build directory: $P917StandardBuildDirectory"
    }
    $receiptPath = if ([IO.Path]::IsPathRooted($ReleaseBuildReceiptPath)) { [IO.Path]::GetFullPath($ReleaseBuildReceiptPath) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $ReleaseBuildReceiptPath)) }
    if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) { throw "Exact-current-tree Release build receipt is missing: $receiptPath" }
    $receipt = Read-P914Json $receiptPath
    $receiptSha = Get-P914FileSha256 $receiptPath
    if ([string]$receipt.schema -cne 'classmngr-phase9-p9-17-exact-release-build-v3' -or [string]$receipt.status -cne 'succeeded') { throw 'Release build receipt is not a successful P9-17 version-3 exact-current-tree VS2022/v143 receipt.' }
    $buildHelperPath = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'scripts/phase9/p9_17_build_current_source_release.ps1'))
    if ([IO.Path]::GetFullPath([string]$receipt.buildHelperPath) -ine $buildHelperPath -or (Get-P914FileSha256 $buildHelperPath) -cne [string]$receipt.buildHelperSha256) { throw 'Release receipt was not produced by the current tracked P9-17 build helper.' }
    Assert-P917BuildObserverExecutionContext $receipt.executionContext $observerExecutionContext
    if ([IO.Path]::GetFullPath([string]$receipt.repositoryRoot) -ine $RepositoryRoot -or [IO.Path]::GetFullPath([string]$receipt.buildDirectory) -ine $BuildDirectory) { throw 'Release receipt repository or build directory differs from this run.' }
    if ([string]$receipt.configuration -cne 'Release' -or [string]$receipt.platform -cne 'x64' -or [string]$receipt.packageOption -cne 'CLASSMNGR_NEXT_RELEASE_PACKAGE=ON') { throw 'Release receipt does not prove the configured Windows x64 Next Release package.' }
    if (-not (Test-P917JsonInteger -Value $receipt.build.exitCode) -or [Int64]$receipt.build.exitCode -ne 0 -or @($receipt.targets) -notcontains 'ClassMngrNext' -or @($receipt.targets) -notcontains 'ClassMngrNextResourcePackage') { throw 'Release receipt does not prove an exact integer zero exit code and successful current-source app and resource targets.' }

    $cachePath = Join-Path $BuildDirectory 'CMakeCache.txt'
    if ([IO.Path]::GetFullPath([string]$receipt.cachePath) -ine [IO.Path]::GetFullPath($cachePath) -or (Get-P914FileSha256 $cachePath) -cne [string]$receipt.cacheSha256) { throw 'Current configured Release cache differs from the source-bound build receipt.' }
    Assert-P917V143ReleaseToolchain $receipt $cachePath
    $cacheHome = [IO.Path]::GetFullPath((Get-P917CacheValue $cachePath 'CMAKE_HOME_DIRECTORY'))
    $generator = Get-P917CacheValue $cachePath 'CMAKE_GENERATOR'
    $platform = Get-P917CacheValue $cachePath 'CMAKE_GENERATOR_PLATFORM'
    $releasePackage = Get-P917CacheValue $cachePath 'CLASSMNGR_NEXT_RELEASE_PACKAGE'
    if ($cacheHome -ine $RepositoryRoot -or $generator -cne $P917ExpectedReleaseGenerator -or $platform -cne $P917ExpectedReleasePlatform -or $releasePackage -cne 'ON') { throw 'Current build cache is not configured for this repository and VS2022/v143 Windows x64 Release package.' }

    $sourceInventoryPath = [IO.Path]::GetFullPath([string]$receipt.sourceInventoryPath)
    if (-not (Test-Path -LiteralPath $sourceInventoryPath -PathType Leaf) -or (Get-P914FileSha256 $sourceInventoryPath) -cne [string]$receipt.sourceInventoryFileSha256) { throw 'Exact build source inventory is missing or changed.' }
    $sourceInventory = Read-P914Json $sourceInventoryPath
    $sourceRows = @($sourceInventory.files)
    if (-not (Test-P917JsonInteger -Value $sourceInventory.fileCount) -or [Int64]$sourceInventory.fileCount -ne $sourceRows.Count) { throw 'Build source inventory fileCount is not an exact integer equal to its rows.' }
    if (-not (Test-P917JsonInteger -Value $receipt.sourceFileCount) -or [Int64]$receipt.sourceFileCount -ne $sourceRows.Count) { throw 'Release receipt sourceFileCount is not an exact integer equal to the verified build inventory rows.' }
    $currentSource = Get-P914SourceInventory $RepositoryRoot @($sourceInventory.includedRoots)
    if ([string]$currentSource.sha256 -cne [string]$receipt.sourceSha256 -or [int]$currentSource.fileCount -ne [int]$receipt.sourceFileCount) { throw 'Current complete source inventory differs from the build receipt fingerprint.' }
    if ((Get-P914InventoryFingerprint @($sourceInventory.files)) -cne [string]$currentSource.sha256) { throw 'Build receipt source inventory rows do not reproduce the current complete source fingerprint.' }
    $sourceMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    $currentMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $sourceInventory.files) { $sourceMap.Add([string]$row.path, $row) }
    foreach ($row in $currentSource.files) { $currentMap.Add([string]$row.path, $row) }
    if ($sourceMap.Count -ne $currentMap.Count) { throw 'Build receipt source inventory file count differs from current source.' }
    foreach ($path in $sourceMap.Keys) {
        if (-not $currentMap.ContainsKey($path)) { throw "Build receipt source path is absent from current source: $path" }
        if ([Int64]$sourceMap[$path].sizeBytes -ne [Int64]$currentMap[$path].sizeBytes -or [string]$sourceMap[$path].sha256 -cne [string]$currentMap[$path].sha256) { throw "Build receipt source file differs from current source: $path" }
    }
    $buildLogPath = [IO.Path]::GetFullPath([string]$receipt.build.logPath)
    if (-not (Test-Path -LiteralPath $buildLogPath -PathType Leaf) -or (Get-P914FileSha256 $buildLogPath) -cne [string]$receipt.build.logSha256) { throw 'Release build log is missing or changed.' }

    $releaseExecutable = [IO.Path]::GetFullPath([string]$receipt.releaseExecutablePath)
    $expectedExecutable = [IO.Path]::GetFullPath((Join-Path (Join-Path $BuildDirectory 'next-package-build/Release') 'ClassMngr.exe'))
    if ($releaseExecutable -ine $expectedExecutable -or -not (Test-Path -LiteralPath $releaseExecutable -PathType Leaf)) { throw 'Release receipt points to a different app executable or build output path.' }
    $releaseSha = Get-P914FileSha256 $releaseExecutable
    if ($releaseSha -cne [string]$receipt.releaseExecutableSha256 -or (Get-P914PeMachine $releaseExecutable) -ne 0x8664) { throw 'Current Release executable hash or x64 architecture differs from the build receipt.' }

    $generated = $receipt.generatedResources
    $assetRoot = [IO.Path]::GetFullPath([string]$generated.bakedAssetsRoot)
    $translationRoot = [IO.Path]::GetFullPath([string]$generated.translationsRoot)
    $catalogPath = [IO.Path]::GetFullPath([string]$generated.catalogPath)
    $expectedGeneratedRoot = [IO.Path]::GetFullPath((Join-Path $BuildDirectory 'generated/ClassMngrNext'))
    foreach ($path in @($assetRoot, $translationRoot, $catalogPath)) {
        if (-not $path.StartsWith($expectedGeneratedRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -and $path -ine $expectedGeneratedRoot) { throw "Release resource path escapes the standard build output: $path" }
    }
    $assetsInventory = Get-P914CanonicalInventory $assetRoot
    $translationsInventory = Get-P914CanonicalInventory $translationRoot
    if ([string]$assetsInventory.sha256 -cne [string]$generated.bakedAssetsSha256 -or [string]$translationsInventory.sha256 -cne [string]$generated.translationsSha256 -or (Get-P914FileSha256 $catalogPath) -cne [string]$generated.catalogSha256) { throw 'Generated current-source resource outputs differ from the Release build receipt.' }

    $packageRoot = Join-Path (Join-Path $EvidenceDirectory 'package') 'bin'
    if (Test-Path -LiteralPath $packageRoot) { throw "Refusing to overwrite P9-17 package root: $packageRoot" }
    [void][IO.Directory]::CreateDirectory($packageRoot)
    $applicationPath = Join-Path $packageRoot 'ClassMngrNext.exe'
    Copy-Item -LiteralPath $releaseExecutable -Destination $applicationPath
    $resourceRoot = Join-Path $packageRoot 'resources'
    $resourceAssetsRoot = Join-Path $resourceRoot 'assets'
    [void][IO.Directory]::CreateDirectory($resourceAssetsRoot)
    foreach ($item in @(Get-ChildItem -LiteralPath $assetRoot -Force)) { Copy-Item -LiteralPath $item.FullName -Destination $resourceAssetsRoot -Recurse -Force }
    $resourceTranslationsRoot = Join-Path $resourceAssetsRoot 'translations'
    [void][IO.Directory]::CreateDirectory($resourceTranslationsRoot)
    foreach ($item in @(Get-ChildItem -LiteralPath $translationRoot -Force)) { Copy-Item -LiteralPath $item.FullName -Destination $resourceTranslationsRoot -Recurse -Force }
    Copy-Item -LiteralPath $catalogPath -Destination (Join-Path $resourceRoot 'resource-catalog.json')
    foreach ($license in @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'licenses/fonts') -Filter 'LICENSE.txt' -File -Recurse)) {
        $relative = $license.FullName.Substring((Join-Path $RepositoryRoot 'licenses').TrimEnd('\').Length + 1)
        $destination = Join-Path (Join-Path $packageRoot 'licenses') $relative
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
        Copy-Item -LiteralPath $license.FullName -Destination $destination
    }

    $qt6Directory = [IO.Path]::GetFullPath((Get-P917CacheValue $cachePath 'Qt6_DIR'))
    $qtPrefix = [IO.Path]::GetFullPath((Join-Path $qt6Directory '..\..\..'))
    $qtBin = Join-Path $qtPrefix 'bin'
    $windeployqt = Join-Path $qtBin 'windeployqt.exe'
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) { throw 'Cache-bound Qt windeployqt is missing.' }
    $oldPath = [Environment]::GetEnvironmentVariable('PATH', [EnvironmentVariableTarget]::Process)
    try {
        [Environment]::SetEnvironmentVariable('PATH', "$qtBin;$oldPath", [EnvironmentVariableTarget]::Process)
        $deployLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'windeployqt.log'
        $deployResult = Invoke-P917PackageCommand $windeployqt @('--qmldir', (Join-Path $RepositoryRoot 'src/features/calendar/ui/qml'), '--skip-plugin-types', 'qmltooling', '--include-plugins', 'qoffscreen,qsqlite', '--exclude-plugins', 'qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql', '--translations', 'en,ko', '--compiler-runtime', '--dir', $packageRoot, $applicationPath) $deployLog
        $deployExitCode = [int]$deployResult.exitCode
    } finally {
        [Environment]::SetEnvironmentVariable('PATH', $oldPath, [EnvironmentVariableTarget]::Process)
    }
    if ($deployExitCode -ne 0) { throw "windeployqt failed with exit code $deployExitCode. See $deployLog" }
    foreach ($relative in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Sql.dll', 'platforms/qwindows.dll', 'sqldrivers/qsqlite.dll', 'resources/resource-catalog.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $packageRoot $relative) -PathType Leaf)) { throw "Current-source package lacks $relative." }
    }

    $resource = Get-P914CanonicalInventory $resourceRoot
    $resourceInventoryPath = Join-Path $EvidenceDirectory 'resource-files.json'
    Write-P914Json $resourceInventoryPath $resource
    $packageInventory = Get-P914CanonicalInventory $packageRoot
    $packageInventoryPath = Join-Path $EvidenceDirectory 'package-files.json'
    Write-P914Json $packageInventoryPath $packageInventory
    $sourceDelta = Get-P917SourceDeltaProof $EvidenceDirectory $currentSource

    return [ordered]@{
        buildMode = 'exact-current-source-release'; configuration = 'Release'; platform = 'x64'; architecture = 'x64'
        buildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
        buildExitCode = [int]$receipt.build.exitCode; freshExactTreeBuild = $false; exactCurrentSourceBuild = $true; incrementalBuild = [bool]$receipt.incrementalBuild
        currentSourceSha256 = [string]$currentSource.sha256; productSourceSha256 = [string]$sourceDelta.productSourceSha256; cmakeHomeDirectory = $cacheHome
        acceptedRunManifestPath = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $P916ManifestRelativePath)); acceptedRunManifestSha256 = Get-P914FileSha256 (Join-Path $RepositoryRoot $P916ManifestRelativePath)
        acceptedP916ProductSourceSha256 = [string]$sourceDelta.accepted.productSourceSha256
        priorP913ManifestPath = [string]$sourceDelta.priorP913ManifestPath; priorP913ManifestSha256 = [string]$sourceDelta.priorP913ManifestSha256
        sourceEquivalence = $sourceDelta.sourceEquivalence
        currentSourceInventory = $sourceDelta.currentSourceInventory
        productSourceInventory = $sourceDelta.productSourceInventory
        harnessSourceInventory = $sourceDelta.harnessSourceInventory
        packageRoot = [IO.Path]::GetFullPath($packageRoot); packageSha256 = [string]$packageInventory.sha256; packageFileCount = [int]$packageInventory.fileCount
        packageInventory = [ordered]@{ relativePath = 'package-files.json'; sha256 = Get-P914FileSha256 $packageInventoryPath; treeSha256 = [string]$packageInventory.sha256; fileCount = [int]$packageInventory.fileCount }
        applicationPath = [IO.Path]::GetFullPath($applicationPath); applicationSha256 = Get-P914FileSha256 $applicationPath; machineType = [int](Get-P914PeMachine $applicationPath)
        releaseBuildReceiptPath = $receiptPath; releaseBuildReceiptSha256 = $receiptSha
        acceptedReleaseReceiptPath = $receiptPath; acceptedReleaseReceiptSha256 = $receiptSha
        executionContext = [ordered]@{ build = $receipt.executionContext; observer = $observerExecutionContext }
        releaseExecutablePath = $releaseExecutable; releaseExecutableSha256 = $releaseSha
        buildReceiptSourceInventoryPath = $sourceInventoryPath; buildReceiptSourceInventoryFileSha256 = [string]$receipt.sourceInventoryFileSha256
        buildLogPath = $buildLogPath; buildLogSha256 = [string]$receipt.build.logSha256; targets = @($receipt.targets)
        generator = $generator; generatorPlatform = $platform; generatorToolset = [string]$receipt.generatorToolset; generatorInstance = [string]$receipt.generatorInstance
        windowsSdkVersion = [string]$receipt.windowsSdkVersion; compiler = $receipt.compiler
        cachePath = [IO.Path]::GetFullPath($cachePath); cacheSha256 = Get-P914FileSha256 $cachePath
        configuredCachePath = [IO.Path]::GetFullPath($cachePath); configuredCacheSha256 = Get-P914FileSha256 $cachePath
        resourceRoot = [IO.Path]::GetFullPath($resourceRoot); resourceInventorySha256 = [string]$resource.sha256
        resourceInventory = [ordered]@{ relativePath = 'resource-files.json'; sha256 = Get-P914FileSha256 $resourceInventoryPath; treeSha256 = [string]$resource.sha256; fileCount = [int]$resource.fileCount }
        qtDeployment = [ordered]@{ windeployqtPath = [IO.Path]::GetFullPath($windeployqt); windeployqtSha256 = Get-P914FileSha256 $windeployqt; logPath = 'logs/windeployqt.log'; logSha256 = [string]$deployResult.logSha256; stdoutPath = 'logs/windeployqt.log.stdout.txt'; stdoutSha256 = [string]$deployResult.stdoutSha256; stderrPath = 'logs/windeployqt.log.stderr.txt'; stderrSha256 = [string]$deployResult.stderrSha256; exitCode = [int]$deployExitCode }
    }
}

function Invoke-P917Preparer([string]$EvidenceDirectory, [string[]]$Arguments, [string]$LogName) {
    $scriptPath = Join-Path $PSScriptRoot 'prepare_p9_17_large_roster.py'
    $logPath = Join-Path (Join-Path $EvidenceDirectory 'logs') $LogName
    $pythonCommand = Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1
    $pythonPath = [string]$pythonCommand.Source
    $prelaunchPathAudit = [Collections.Generic.List[object]]::new()
    $prelaunchPathAudit.Add((Assert-P917ClassicMaxPath -Path $pythonPath -Label 'fixturePreparerExecutable'))
    $prelaunchPathAudit.Add((Assert-P917ClassicMaxPath -Path $scriptPath -Label 'fixturePreparerScript'))
    for ($argumentIndex = 0; $argumentIndex -lt $Arguments.Count; $argumentIndex++) {
        $argument = [string]$Arguments[$argumentIndex]
        if ($argumentIndex -gt 0 -and $argument.StartsWith('--', [StringComparison]::Ordinal)) { continue }
        if ([IO.Path]::IsPathRooted($argument)) {
            $prelaunchPathAudit.Add((Assert-P917ClassicMaxPath -Path $argument -Label "fixturePreparerArgument$argumentIndex"))
        }
    }
    $pathAuditPath = $logPath + '.path-audit.json'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $pathAuditPath))
    Write-P914Json $pathAuditPath ([ordered]@{ schema = 'classmngr-phase9-p9-17-prelaunch-path-audit-v1'; executable = $pythonPath; script = $scriptPath; arguments = @($Arguments); auditedPaths = $prelaunchPathAudit.ToArray() })
    $lines = @(& python $scriptPath @Arguments 2>&1 | ForEach-Object { [string]$_ })
    $code = $LASTEXITCODE
    Write-P917File $logPath $lines
    if ($code -ne 0) { throw "P9-17 fixture command '$($Arguments[0])' failed with exit code $code. See $logPath" }
    return (($lines | Select-Object -Last 1) | ConvertFrom-Json -ErrorAction Stop)
}

function Get-P917Workspace([string]$EvidenceDirectory, [ValidateSet('filled', 'sparse', '')][string]$PayloadMode = '', [ValidateSet('filled', 'reduced-rows', '')][string]$RecordCountMode = '') {
    if (-not [string]::IsNullOrEmpty($PayloadMode) -and -not [string]::IsNullOrEmpty($RecordCountMode)) { throw 'Payload and roster record-count fixture modes must be prepared separately.' }
    $base = Join-Path $RepositoryRoot $BaseWorkspaceRelativePath
    $seed = Join-Path $RepositoryRoot 'tests/fixtures/workspaces/large_startup.sql'
    $workspace = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'large-roster.tps'
    $receiptPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'preparation-receipt.json'
    $prepareArguments = @('prepare', '--base-workspace', $base, '--seed-sql', $seed, '--output', $workspace, '--receipt', $receiptPath)
    if (-not [string]::IsNullOrEmpty($PayloadMode)) { $prepareArguments += @('--payload-mode', $PayloadMode) }
    if (-not [string]::IsNullOrEmpty($RecordCountMode)) { $prepareArguments += @('--record-count-mode', $RecordCountMode) }
    $prepare = Invoke-P917Preparer $EvidenceDirectory $prepareArguments 'fixture-prepare-output.txt'
    $receipt = Read-P914Json $receiptPath
    if ([string]$prepare.status -cne 'passed' -or [string]$receipt.status -cne 'passed') { throw 'P9-17 fixture preparation did not return a passed receipt.' }
    if (-not [string]::IsNullOrEmpty($PayloadMode) -and (
        [string]$receipt.payloadMode -cne $PayloadMode -or
        [bool]$receipt.payloadDiagnostic.diagnosticOnly -ne $true -or
        [bool]$receipt.payloadDiagnostic.accepted -ne $false
    )) { throw 'P9-17 payload fixture preparation did not return the declared diagnostic-only payload mode.' }
    if (-not [string]::IsNullOrEmpty($RecordCountMode)) {
        $expectedCustomRows = if ($RecordCountMode -ceq 'filled') { 15000 } else { 3 }
        $expectedTargetRows = if ($RecordCountMode -ceq 'filled') { 15200 } else { 203 }
        $expectedDatabaseRows = if ($RecordCountMode -ceq 'filled') { 22325 } else { 7328 }
        if ([string]$receipt.recordCountMode -cne $RecordCountMode -or
            [bool]$receipt.recordCountDiagnostic.diagnosticOnly -ne $true -or
            [bool]$receipt.recordCountDiagnostic.accepted -ne $false -or
            [int]$receipt.recordCountDiagnostic.customCellRecordCount -ne $expectedCustomRows -or
            [int]$receipt.recordCountDiagnostic.customCellValueCount -ne $expectedCustomRows -or
            [int]$receipt.recordCountDiagnostic.blankCustomCellValueCount -ne 0 -or
            [int]$receipt.recordCountDiagnostic.removedCustomCellRecordCount -ne (15000 - $expectedCustomRows) -or
            [int]$receipt.identity.targetRosterCellCount -ne $expectedTargetRows -or
            [int]$receipt.identity.targetCustomRosterCellCount -ne $expectedCustomRows -or
            [int]$receipt.identity.databaseRosterCellCount -ne $expectedDatabaseRows -or
            [int]$receipt.identity.scheduleEntryCount -ne 768 -or
            [int]$receipt.identity.targetRosterRowCount -ne 100 -or
            [int]$receipt.identity.targetStoredRosterColumnCount -ne 156 -or
            [int]$receipt.identity.targetEnglishNameCellCount -ne 100 -or
            [int]$receipt.identity.targetKoreanNameCellCount -ne 100 -or
            @($receipt.recordCountDiagnostic.probeCells).Count -ne 3) {
            throw 'P9-17 roster record-count fixture receipt differs from its declared row-cardinality mode.'
        }
    }
    $beforePath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'before-snapshot.json'
    $beforeSummary = Invoke-P917Preparer $EvidenceDirectory @('snapshot', '--phase', 'before', '--workspace', $workspace, '--preparation-receipt', $receiptPath, '--output', $beforePath) 'before-snapshot-output.txt'
    $beforeSnapshot = Read-P914Json $beforePath
    if ([string]$beforeSummary.status -cne 'passed' -or [string]$beforeSnapshot.status -cne 'passed' -or [string]$beforeSnapshot.phase -cne 'before' -or [string]$beforeSnapshot.integrityCheck -cne 'ok' -or @($beforeSnapshot.foreignKeyErrors).Count -ne 0) { throw 'P9-17 before-snapshot failed its status, integrity, or foreign-key checks.' }
    if ([int]$beforeSnapshot.identity.targetClassId -ne 1 -or [int]$beforeSnapshot.identity.targetRosterRowCount -ne 100 -or [int]$beforeSnapshot.identity.targetFixedBaseRosterColumnCount -ne 6 -or [int]$beforeSnapshot.identity.targetCustomRosterColumnCount -ne 150 -or [int]$beforeSnapshot.identity.targetStoredRosterColumnCount -ne 156 -or [int]$beforeSnapshot.identity.targetDisplayedRosterColumnCount -ne 156 -or [int]$beforeSnapshot.identity.targetEnglishNameCellCount -ne 100 -or [int]$beforeSnapshot.identity.targetKoreanNameCellCount -ne 100) { throw 'P9-17 before-snapshot failed the exact class 1 roster fixture shape.' }
    if (-not [string]::IsNullOrEmpty($RecordCountMode) -and (
        [string]$beforeSnapshot.recordCountMode -cne $RecordCountMode -or
        [int]$beforeSnapshot.workspaceTables.roster_data.rowCount -ne [int]$receipt.identity.databaseRosterCellCount -or
        [int]$beforeSnapshot.recordCountDiagnostic.snapshotCounts.customCellRecordCount -ne [int]$receipt.recordCountDiagnostic.customCellRecordCount -or
        [int]$beforeSnapshot.recordCountDiagnostic.snapshotCounts.removedCustomCellRecordCount -ne [int]$receipt.recordCountDiagnostic.removedCustomCellRecordCount
    )) { throw 'P9-17 before-snapshot failed its declared roster record-count checks.' }
    return [ordered]@{
        workspacePath = [IO.Path]::GetFullPath($workspace); preparationPath = [IO.Path]::GetFullPath($receiptPath); preparation = $receipt
        preparationSha256 = Get-P914FileSha256 $receiptPath; beforePath = [IO.Path]::GetFullPath($beforePath); beforeSha256 = Get-P914FileSha256 $beforePath
        baseWorkspacePath = [IO.Path]::GetFullPath($base); seedPath = [IO.Path]::GetFullPath($seed)
    }
}

function Get-P917Settings([string]$EvidenceDirectory) {
    $settingsRoot = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings'
    $profile = Join-Path (Join-Path $settingsRoot 'PaperCloud') 'ClassMngr.ini'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $profile))
    $text = "[options]`r`nlanguage=1`r`ntheme=1`r`nmaximumRosterRows=100`r`nrosterRowDisplayMode=0`r`nsaveMode=1`r`n"
    [IO.File]::WriteAllText($profile, $text, [Text.UTF8Encoding]::new($false))
    $inventory = Get-P914CanonicalInventory $settingsRoot
    if ([int]$inventory.fileCount -ne 1 -or [IO.File]::ReadAllText($profile, [Text.Encoding]::UTF8) -cne $text) { throw 'Isolated P9-17 settings profile does not encode the English/Light 100-row manual-save fixture preferences.' }
    $inventoryPath = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings-before-launch-inventory.json'
    Write-P914Json $inventoryPath $inventory
    $copyPath = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings-before-launch.ini'
    Copy-Item -LiteralPath $profile -Destination $copyPath
    return [ordered]@{
        root = [IO.Path]::GetFullPath($settingsRoot); preferenceFile = 'runtime/settings/PaperCloud/ClassMngr.ini'
        preferenceFileSha256 = Get-P914FileSha256 $profile; beforeLaunchInventoryPath = 'runtime/settings-before-launch-inventory.json'
        beforeLaunchInventoryFileSha256 = Get-P914FileSha256 $inventoryPath; beforeLaunchInventorySha256 = [string]$inventory.sha256
        initialPreferenceSnapshotPath = 'runtime/settings-before-launch.ini'; initialPreferenceSnapshotSha256 = Get-P914FileSha256 $copyPath
        language = 'English'; theme = 'Light'; maximumRosterRows = 100; rosterRowDisplayMode = 0; saveMode = 'Manual (preference value 1)'
        globalDefaultsChanged = $false; preferenceSource = 'isolated QSettings INI profile in the run evidence directory'
    }
}

function Stop-P917StartedAppAfterContextFailure(
    [Diagnostics.Process]$Process,
    [int]$StartedProcessId,
    [object]$StartedProcessStartTimeUtc,
    [string]$ExpectedApplicationPath
) {
    $cleanupErrors = [Collections.Generic.List[string]]::new()
    $gracefulCloseTimeoutSeconds = 5
    $forceStopTimeoutMilliseconds = 5000
    $record = [ordered]@{
        schema = 'classmngr-phase9-p9-17-pre-context-process-cleanup-v1'
        status = 'cleanup-failed'; processId = $StartedProcessId; processStartTimeUtc = $null
        expectedApplicationPath = $ExpectedApplicationPath
        observedApplicationPath = $null; observedApplicationPathError = $null
        processStartTimeCaptureSource = $null
        closeMethod = $null; gracefulCloseRequested = $false; gracefulCloseAttemptCount = 0
        gracefulCloseTimeoutSeconds = $gracefulCloseTimeoutSeconds; forceStopAttempted = $false
        forceStopIdentityVerified = $false; forceStopTimeoutMilliseconds = $forceStopTimeoutMilliseconds
        forcedTermination = $false; processExited = $false; exitCode = $null; cleanupErrors = @()
        processReference = 'The System.Diagnostics.Process instance returned by Start-Process -PassThru and retained by Start-P917App.'
        forceStopTarget = 'Process.Kill() on that same retained process instance, after PID and UTC start-time revalidation.'
    }
    try {
        if ($null -eq $Process) { throw 'The Start-Process result was null; there is no process instance to clean up.' }
        $Process.Refresh()
        $actualProcessId = [int]$Process.Id
        if ($StartedProcessId -gt 0 -and $actualProcessId -ne $StartedProcessId) {
            throw "Retained process instance PID changed unexpectedly (started=$StartedProcessId; observed=$actualProcessId); no close or kill was attempted."
        }
        $pidCapturedAtLaunch = $StartedProcessId -gt 0
        if ($StartedProcessId -le 0) { $StartedProcessId = $actualProcessId; $record.processId = $actualProcessId }

        $startTimeCapturedAtLaunch = $null -ne $StartedProcessStartTimeUtc
        $verifiedStartTimeUtc = $StartedProcessStartTimeUtc
        if ($null -ne $verifiedStartTimeUtc) { $verifiedStartTimeUtc = ([DateTime]$verifiedStartTimeUtc).ToUniversalTime() }
        $observedStartTimeUtc = $null
        try { $observedStartTimeUtc = $Process.StartTime.ToUniversalTime() }
        catch { $cleanupErrors.Add('Could not read the retained process start time before cleanup: ' + $_.Exception.Message) }
        if ($null -ne $observedStartTimeUtc -and $null -ne $verifiedStartTimeUtc -and $observedStartTimeUtc.Ticks -ne $verifiedStartTimeUtc.Ticks) {
            throw 'Retained process start time differs from the start-time captured immediately after launch; no close or kill was attempted.'
        }
        if ($null -eq $verifiedStartTimeUtc) { $verifiedStartTimeUtc = $observedStartTimeUtc }
        if ($null -ne $verifiedStartTimeUtc) {
            $record.processStartTimeUtc = $verifiedStartTimeUtc.ToString('o')
            $record.processStartTimeCaptureSource = if ($startTimeCapturedAtLaunch) { 'immediately-after-launch' } else { 'cleanup-entry-retained-process-instance' }
        }
        try { $record.observedApplicationPath = [IO.Path]::GetFullPath([string]$Process.MainModule.FileName) }
        catch { $record.observedApplicationPathError = $_.Exception.Message }

        if ($Process.HasExited) {
            $record.status = 'already-exited'
            $record.processExited = $true
            try { $record.exitCode = [int]$Process.ExitCode } catch { }
        } else {
            $gracefulDeadline = [DateTime]::UtcNow.AddSeconds($gracefulCloseTimeoutSeconds)
            while ([DateTime]::UtcNow -lt $gracefulDeadline) {
                $Process.Refresh()
                if ($Process.HasExited) { break }
                if (-not $record.gracefulCloseRequested) {
                    try {
                        $record.gracefulCloseAttemptCount++
                        if ($Process.CloseMainWindow()) {
                            $record.gracefulCloseRequested = $true
                            $record.closeMethod = 'Process.CloseMainWindow'
                        }
                    } catch { $cleanupErrors.Add('Graceful CloseMainWindow attempt failed: ' + $_.Exception.Message) }
                }
                try { $null = $Process.WaitForExit(100) }
                catch { $cleanupErrors.Add('Wait during graceful close failed: ' + $_.Exception.Message) }
                $Process.Refresh()
                if ($Process.HasExited) { break }
            }
            $Process.Refresh()
            if ($Process.HasExited) {
                $record.status = if ($record.gracefulCloseRequested) { 'graceful-close' } else { 'exited-during-cleanup' }
                $record.processExited = $true
                try { $record.exitCode = [int]$Process.ExitCode } catch { }
            } else {
                $Process.Refresh()
                if ($Process.HasExited) {
                    $record.status = 'exited-during-cleanup'
                    $record.processExited = $true
                    try { $record.exitCode = [int]$Process.ExitCode } catch { }
                } else {
                    $forceProcessId = [int]$Process.Id
                    $forceStartTimeUtc = $null
                    try { $forceStartTimeUtc = $Process.StartTime.ToUniversalTime() }
                    catch { $cleanupErrors.Add('Could not re-read the retained process start time before force stop: ' + $_.Exception.Message) }
                    $pidMatches = $forceProcessId -eq $StartedProcessId
                    $startTimeCheckAvailable = $null -ne $verifiedStartTimeUtc -and $null -ne $forceStartTimeUtc
                    $startTimeMatches = $true
                    if ($startTimeCheckAvailable) {
                        $startTimeMatches = $forceStartTimeUtc.Ticks -eq $verifiedStartTimeUtc.Ticks
                    } elseif ($null -eq $verifiedStartTimeUtc -and $null -ne $forceStartTimeUtc) {
                        $verifiedStartTimeUtc = $forceStartTimeUtc
                        $record.processStartTimeUtc = $verifiedStartTimeUtc.ToString('o')
                        $startTimeCheckAvailable = $true
                    } else {
                        $cleanupErrors.Add('Start time could not be revalidated; force-stop identity relies on the same retained Start-Process process instance and its matching PID.')
                    }
                    if (-not $pidMatches -or -not $startTimeMatches) {
                        $expectedStartText = if ($null -ne $verifiedStartTimeUtc) { $verifiedStartTimeUtc.ToString('o') } else { 'unavailable' }
                        $observedStartText = if ($null -ne $forceStartTimeUtc) { $forceStartTimeUtc.ToString('o') } else { 'unavailable' }
                        throw "Force stop refused because retained-process identity could not be revalidated (expectedPid=$StartedProcessId; observedPid=$forceProcessId; expectedStartUtc=$expectedStartText; observedStartUtc=$observedStartText)."
                    }
                    $record.forceStopIdentityVerified = $true
                    $record.forceStopAttempted = $true
                    $record.forceStopVerifiedIdentity = [ordered]@{
                        processId = $forceProcessId; startTimeUtc = if ($null -ne $forceStartTimeUtc) { $forceStartTimeUtc.ToString('o') } else { $null }
                        processReference = 'Same Start-Process -PassThru System.Diagnostics.Process instance retained by Start-P917App.'
                        pidMatchedCapturedLaunch = $pidCapturedAtLaunch; pidMatchedRetainedProcessInstance = $true
                        startTimeCheckAvailable = $startTimeCheckAvailable
                        startTimeMatchedCapturedLaunch = if ($startTimeCapturedAtLaunch -and $startTimeCheckAvailable) { $true } else { $null }
                        processWasStillAliveImmediatelyBeforeKill = $true
                    }
                    $Process.Kill()
                    $record.forcedTermination = $true
                    $null = $Process.WaitForExit($forceStopTimeoutMilliseconds)
                    $Process.Refresh()
                    $record.processExited = [bool]$Process.HasExited
                    if ($record.processExited) {
                        $record.status = 'force-stopped'
                        try { $record.exitCode = [int]$Process.ExitCode } catch { }
                    } else {
                        $record.status = 'force-stop-timeout'
                        $cleanupErrors.Add("The exact launched process remained alive after Process.Kill() and a $forceStopTimeoutMilliseconds ms wait.")
                    }
                }
            }
        }
    } catch {
        $record.status = 'cleanup-failed'
        $cleanupErrors.Add($_.Exception.Message)
        try {
            $Process.Refresh()
            $record.processExited = [bool]$Process.HasExited
            if ($Process.HasExited) { try { $record.exitCode = [int]$Process.ExitCode } catch { } }
        } catch { $cleanupErrors.Add('Could not refresh process state after cleanup failure: ' + $_.Exception.Message) }
    }
    $record.cleanupErrors = $cleanupErrors.ToArray()
    return $record
}

function Assert-P917ClassicMaxPath([string]$Path, [string]$Label) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw "P9-17 $Label path is missing before process launch." }
    $fullPath = [IO.Path]::GetFullPath($Path)
    if ($fullPath.Length -ge 260) {
        throw "P9-17 $Label path has $($fullPath.Length) characters; it must be shorter than the classic 260-character Windows MAX_PATH before process launch: $fullPath"
    }
    return [ordered]@{ label = $Label; path = $fullPath; characterCount = [int]$fullPath.Length; maxPathExclusive = 260 }
}

function Start-P917App([string]$EvidenceDirectory, [string]$ApplicationPath, [object]$ExpectedExecutionContext, [object]$Fixture, [object]$Package, [bool]$EnableDefaultProcessHeapDiagnostic = $false) {
    if ($null -eq $ExpectedExecutionContext) { throw 'P9-17 app launch requires verified build and observer execution-context evidence.' }
    $observerExecutionContextBeforeLaunch = Get-P917CurrentExecutionContext
    Assert-P917BuildObserverExecutionContext $ExpectedExecutionContext.build $observerExecutionContextBeforeLaunch
    Assert-P917SameExecutionContext $ExpectedExecutionContext.observer $observerExecutionContextBeforeLaunch 'Observer before app launch'
    $runtime = Join-Path $EvidenceDirectory 'runtime'
    $traceFileName = if ($EnableDefaultProcessHeapDiagnostic) { 'exit.jsonl' } else { 'application-shutdown.jsonl' }
    $tracePath = Join-Path (Join-Path $runtime 'diagnostics') $traceFileName
    $stdoutPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stdout.log'
    $stderrPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stderr.log'
    $envMap = [ordered]@{
        CLASSMNGR_SETTINGS_ROOT = (Join-Path $runtime 'settings'); CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH = [IO.Path]::GetFullPath($tracePath)
        LOCALAPPDATA = (Join-Path $runtime 'local-app-data'); APPDATA = (Join-Path $runtime 'app-data')
        TEMP = (Join-Path $runtime 'temp'); TMP = (Join-Path $runtime 'temp'); QT_FORCE_STDERR_LOGGING = '1'
        CLASSMNGR_P9_17_DEFAULT_PROCESS_HEAP_DIAGNOSTIC = if ($EnableDefaultProcessHeapDiagnostic) { '1' } else { '' }
        PATH = "$((Split-Path -Parent $ApplicationPath));$env:PATH"
    }
    $settingsProfilePath = Join-Path (Join-Path ([string]$envMap.CLASSMNGR_SETTINGS_ROOT) 'PaperCloud') 'ClassMngr.ini'
    $appDataLocation = Join-Path (Join-Path ([string]$envMap.APPDATA) 'PaperCloud') 'ClassMngrNext'
    $localAppDataLocation = Join-Path (Join-Path ([string]$envMap.LOCALAPPDATA) 'PaperCloud') 'ClassMngrNext'
    $temporaryDirectoryTemplate = Join-Path ([string]$envMap.TEMP) 'qt_temp-XXXXXX'
    $workspacePath = [IO.Path]::GetFullPath([string]$Fixture.workspacePath)
    $appArguments = @()
    $launchPaths = [ordered]@{
        runEvidenceDirectory = $EvidenceDirectory
        runtimeRoot = $runtime
        logsRoot = Join-Path $EvidenceDirectory 'logs'
        applicationImage = $ApplicationPath
        applicationWorkingDirectory = Split-Path -Parent $ApplicationPath
        applicationPathEnvironmentEntry = Split-Path -Parent $ApplicationPath
        packageRoot = [string]$Package.packageRoot
        packageResourceRoot = [string]$Package.resourceRoot
        standardOutput = $stdoutPath
        standardError = $stderrPath
        shutdownTrace = $tracePath
        settingsRoot = [string]$envMap.CLASSMNGR_SETTINGS_ROOT
        settingsProfile = $settingsProfilePath
        settingsAtomicSaveTemporaryFileTemplate = $settingsProfilePath + '.XXXXXX'
        localAppDataRoot = [string]$envMap.LOCALAPPDATA
        localAppDataLocation = $localAppDataLocation
        appDataRoot = [string]$envMap.APPDATA
        appDataLocation = $appDataLocation
        tempRoot = [string]$envMap.TEMP
        tempEnvironmentAlias = [string]$envMap.TMP
        temporaryDirectoryTemplate = $temporaryDirectoryTemplate
        fixtureWorkspaceOpenedThroughUia = $workspacePath
        fixtureWorkspaceSqliteWal = $workspacePath + '-wal'
        fixtureWorkspaceSqliteShm = $workspacePath + '-shm'
        fixtureWorkspaceSqliteJournal = $workspacePath + '-journal'
        fixturePreparationReceipt = [string]$Fixture.preparationPath
        fixtureBeforeSnapshot = [string]$Fixture.beforePath
        fixtureAfterSnapshot = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'after-snapshot.json'
        fixtureBaseWorkspace = [string]$Fixture.baseWorkspacePath
        fixtureSeedSql = [string]$Fixture.seedPath
        launchPathAuditArtifact = Join-Path (Join-Path $runtime 'diagnostics') 'launch-path-audit.json'
    }
    $launchPathAudit = [Collections.Generic.List[object]]::new()
    foreach ($path in $launchPaths.GetEnumerator()) {
        $launchPathAudit.Add((Assert-P917ClassicMaxPath -Path ([string]$path.Value) -Label ([string]$path.Key)))
    }
    $pathEnvironmentEntries = @(([string]$envMap.PATH -split ';') | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    for ($pathIndex = 0; $pathIndex -lt $pathEnvironmentEntries.Count; $pathIndex++) {
        $pathEntry = ([string]$pathEnvironmentEntries[$pathIndex]).Trim().Trim('"')
        $launchPathAudit.Add((Assert-P917ClassicMaxPath -Path $pathEntry -Label "pathEnvironmentEntry$pathIndex"))
    }
    foreach ($argument in $appArguments) {
        if ([string]$argument -match '^[A-Za-z]:[\\/]') {
            $launchPathAudit.Add((Assert-P917ClassicMaxPath -Path ([string]$argument) -Label 'applicationArgument'))
        }
    }
    $launchPathAuditPath = [string]$launchPaths.launchPathAuditArtifact
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $launchPathAuditPath))
    Write-P914Json $launchPathAuditPath ([ordered]@{
        schema = 'classmngr-phase9-p9-17-prelaunch-path-audit-v1'
        applicationPath = [IO.Path]::GetFullPath($ApplicationPath)
        applicationArguments = @($appArguments)
        applicationEnvironmentPathValues = [ordered]@{
            CLASSMNGR_SETTINGS_ROOT = [string]$envMap.CLASSMNGR_SETTINGS_ROOT
            CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH = [string]$envMap.CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH
            LOCALAPPDATA = [string]$envMap.LOCALAPPDATA
            APPDATA = [string]$envMap.APPDATA
            TEMP = [string]$envMap.TEMP
            TMP = [string]$envMap.TMP
        }
        workspaceOpenedThroughUia = $true
        auditedPaths = $launchPathAudit.ToArray()
    })
    [IO.File]::WriteAllText($stdoutPath, '', [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($stderrPath, '', [Text.UTF8Encoding]::new($false))
    $oldEnvironment = [ordered]@{}
    foreach ($pair in $envMap.GetEnumerator()) { $oldEnvironment[[string]$pair.Key] = [Environment]::GetEnvironmentVariable([string]$pair.Key, 'Process'); [Environment]::SetEnvironmentVariable([string]$pair.Key, [string]$pair.Value, 'Process') }
    $process = $null
    $startedProcessId = 0
    $startedProcessStartTimeUtc = $null
    $startupStage = 'start-process'
    try {
    try { $process = Start-Process -FilePath $ApplicationPath -WorkingDirectory (Split-Path -Parent $ApplicationPath) -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru }
    finally { foreach ($pair in $oldEnvironment.GetEnumerator()) { [Environment]::SetEnvironmentVariable([string]$pair.Key, $pair.Value, 'Process') } }
    if ($null -eq $process) { throw 'Unable to start the measured P9-17 x64 Release process.' }
    $startupStage = 'capture-process-identity'
    $startedProcessId = [int]$process.Id
    $startedProcessStartTimeUtc = $process.StartTime.ToUniversalTime()
    $startupStage = 'verify-process-image-path'
    $expectedPath = [IO.Path]::GetFullPath($ApplicationPath)
    $moduleReadinessTimeoutSeconds = 10
    $moduleReadinessDeadline = [DateTime]::UtcNow.AddSeconds($moduleReadinessTimeoutSeconds)
    $observedPath = $null
    $lastModuleReadError = 'Process.MainModule.FileName was not available yet.'
    while ($null -eq $observedPath -and [DateTime]::UtcNow -lt $moduleReadinessDeadline) {
        $process.Refresh()
        if ($process.HasExited) {
            $exitCodeText = 'unavailable'
            try { $exitCodeText = [string][int]$process.ExitCode } catch { }
            throw "Measured P9-17 x64 Release process exited before its executable image became readable (exitCode=$exitCodeText; lastReadError=$lastModuleReadError)."
        }
        try {
            $module = $process.MainModule
            if ($null -eq $module) {
                $lastModuleReadError = 'Process.MainModule was null.'
            } else {
                $fileNameProperty = $module.PSObject.Properties['FileName']
                if ($null -eq $fileNameProperty) {
                    $lastModuleReadError = "Process.MainModule type '$($module.GetType().FullName)' did not expose FileName."
                } elseif ([string]::IsNullOrWhiteSpace([string]$fileNameProperty.Value)) {
                    $lastModuleReadError = 'Process.MainModule.FileName was empty.'
                } else {
                    $observedPath = [IO.Path]::GetFullPath([string]$fileNameProperty.Value)
                }
            }
        } catch [System.ComponentModel.Win32Exception] {
            if ([int]$_.Exception.NativeErrorCode -eq 5) {
                throw "Access denied while reading the launched P9-17 process main module under the current standard Windows token: $($_.Exception.Message)"
            }
            $lastModuleReadError = "$($_.Exception.GetType().FullName): $($_.Exception.Message)"
        } catch [System.UnauthorizedAccessException] {
            throw "Access denied while reading the launched P9-17 process main module under the current standard Windows token: $($_.Exception.Message)"
        } catch {
            $lastModuleReadError = "$($_.Exception.GetType().FullName): $($_.Exception.Message)"
        }
        if ($null -eq $observedPath) {
            $process.Refresh()
            if ($process.HasExited) {
                $exitCodeText = 'unavailable'
                try { $exitCodeText = [string][int]$process.ExitCode } catch { }
                throw "Measured P9-17 x64 Release process exited before its executable image became readable (exitCode=$exitCodeText; lastReadError=$lastModuleReadError)."
            }
            if ([DateTime]::UtcNow -lt $moduleReadinessDeadline) { Start-Sleep -Milliseconds 50 }
        }
    }
    if ($null -eq $observedPath) {
        $process.Refresh()
        if ($process.HasExited) {
            $exitCodeText = 'unavailable'
            try { $exitCodeText = [string][int]$process.ExitCode } catch { }
            throw "Measured P9-17 x64 Release process exited before its executable image became readable (exitCode=$exitCodeText; lastReadError=$lastModuleReadError)."
        }
        throw "Timed out after $moduleReadinessTimeoutSeconds seconds while the P9-17 process remained alive but its main executable path was unreadable. Last read error: $lastModuleReadError"
    }
    $process.Refresh()
    if ($process.HasExited) {
        $exitCodeText = 'unavailable'
        try { $exitCodeText = [string][int]$process.ExitCode } catch { }
        throw "Measured P9-17 x64 Release process exited before its executable image could be verified (exitCode=$exitCodeText; observedPath=$observedPath)."
    }
    if ($observedPath -cne $expectedPath) {
        throw "Live P9-17 process image path differs from the package (expected='$expectedPath'; observed='$observedPath')."
    }
    $startupStage = 'verify-process-image-hash'
    $observedSha = Get-P914FileSha256 $observedPath
    $expectedSha = Get-P914FileSha256 $ApplicationPath
    if ($observedSha -cne $expectedSha) {
        throw "Live P9-17 process image hash differs from the package (expected='$expectedSha'; observed='$observedSha')."
    }
    $startupStage = 'verify-process-architecture'
    $observedMachine = Get-P914PeMachine $observedPath
    if ($observedMachine -ne 0x8664) {
        throw "Live P9-17 process image is not x64 (machineType=$observedMachine)."
    }
    $startupStage = 'verify-build-observer-app-session'
    $process.Refresh()
    $measuredProcessSessionId = [Int64]$process.SessionId
    if ($measuredProcessSessionId -ne [Int64]$observerExecutionContextBeforeLaunch.processSessionId -or $measuredProcessSessionId -ne [Int64]$ExpectedExecutionContext.build.completion.processSessionId) {
        throw "Measured P9-17 app session differs from its build and observer session (app=$measuredProcessSessionId; observer=$($observerExecutionContextBeforeLaunch.processSessionId); build=$($ExpectedExecutionContext.build.completion.processSessionId))."
    }
    $observerExecutionContextAfterLaunch = Get-P917CurrentExecutionContext
    Assert-P917SameExecutionContext $observerExecutionContextBeforeLaunch $observerExecutionContextAfterLaunch 'Observer across app launch'
    $appExecutionContext = [ordered]@{
        buildIdentity = [string]$ExpectedExecutionContext.build.completion.identity
        buildUserSid = [string]$ExpectedExecutionContext.build.completion.userSid
        observerIdentity = [string]$observerExecutionContextAfterLaunch.identity
        observerUserSid = [string]$observerExecutionContextAfterLaunch.userSid
        buildProcessSessionId = [Int64]$ExpectedExecutionContext.build.completion.processSessionId
        observerProcessSessionId = [Int64]$observerExecutionContextAfterLaunch.processSessionId
        measuredProcessSessionId = $measuredProcessSessionId
        activeConsoleSessionId = [Int64]$observerExecutionContextAfterLaunch.activeConsoleSessionId
        sharedSessionVerified = $true
    }
    $startupStage = 'build-process-context'
    return [ordered]@{
        process = $process; stdoutPath = $stdoutPath; stderrPath = $stderrPath; seen = [ordered]@{ stdout = 0; stderr = 0 }; arrival = 0
        mainWindowHandle = 0L; mainWindowRoot = $null; ownerHandles = [Collections.Generic.Dictionary[string, Int64]]::new([StringComparer]::Ordinal)
        routeEvents = [Collections.Generic.List[object]]::new(); lifecycleEvents = [Collections.Generic.List[object]]::new(); outputLines = [Collections.Generic.List[object]]::new()
        forcedTermination = $false; closeAction = $null; processExit = $null; sampler = $null
        tempRoot = (Join-Path $runtime 'temp'); processImagePath = $observedPath; processImageSha256 = Get-P914FileSha256 $observedPath
        executionContext = $appExecutionContext; processSessionId = $measuredProcessSessionId; launchPathAudit = $launchPathAudit.ToArray()
        launchPathAuditRelativePath = 'runtime/diagnostics/launch-path-audit.json'
        workspacePath = ''; dialogRoot = $null; dialogHandle = 0L; dialogWindowHandles = [Collections.Generic.List[Int64]]::new()
    }
    } catch {
        $startupFailure = $_
        if ($null -eq $process) {
            throw "P9-17 app process launch failed during '$startupStage' before Start-Process returned a process instance: $($startupFailure.Exception.Message)"
        }
        try {
            $cleanupRecord = Stop-P917StartedAppAfterContextFailure -Process $process -StartedProcessId $startedProcessId -StartedProcessStartTimeUtc $startedProcessStartTimeUtc -ExpectedApplicationPath $ApplicationPath
        } catch {
            $cleanupRecord = [ordered]@{
                schema = 'classmngr-phase9-p9-17-pre-context-process-cleanup-v1'; status = 'cleanup-failed'
                processId = $startedProcessId; processStartTimeUtc = if ($null -ne $startedProcessStartTimeUtc) { ([DateTime]$startedProcessStartTimeUtc).ToUniversalTime().ToString('o') } else { $null }
                expectedApplicationPath = [IO.Path]::GetFullPath($ApplicationPath); gracefulCloseRequested = $false
                forcedTermination = $false; processExited = $false; cleanupErrors = @($_.Exception.Message)
            }
        }
        $cleanupRecord.failureStage = $startupStage
        $cleanupRecord.originalFailureMessage = [string]$startupFailure.Exception.Message
        $cleanupRelativePath = 'runtime/diagnostics/pre-context-launch-cleanup.json'
        $cleanupPath = Join-Path $EvidenceDirectory ($cleanupRelativePath -replace '/', [IO.Path]::DirectorySeparatorChar)
        $cleanupWriteError = $null
        try {
            [void][IO.Directory]::CreateDirectory((Split-Path -Parent $cleanupPath))
            Write-P914Json $cleanupPath $cleanupRecord
        } catch { $cleanupWriteError = $_.Exception.Message }
        $cleanupSummary = "status=$($cleanupRecord.status); processId=$($cleanupRecord.processId); gracefulCloseRequested=$($cleanupRecord.gracefulCloseRequested); forcedTermination=$($cleanupRecord.forcedTermination); processExited=$($cleanupRecord.processExited); evidence=$cleanupRelativePath"
        if ($null -ne $cleanupWriteError) { $cleanupSummary += "; cleanupEvidenceWriteError=$cleanupWriteError" }
        throw "P9-17 process startup failed during '$startupStage' after launch: $($startupFailure.Exception.Message); pre-context cleanup $cleanupSummary"
    }
}

function Get-P917DialogRoot([object]$Context) {
    $matches = [Collections.Generic.List[object]]::new()
    foreach ($handle in [P914Native]::GetTopLevelWindows([uint32]$Context.process.Id)) {
        if (-not [P914Native]::IsVisible($handle) -or [Int64]$handle.ToInt64() -eq [Int64]$Context.mainWindowHandle) { continue }
        if ([uint32][P914Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id) { throw 'Visible dialog HWND is not owned by the measured process.' }
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
        $current = $root.Current
        $id = [string]$current.AutomationId
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window') { continue }
        if ($id -cne 'classmngrNextFileDialog' -and -not $id.EndsWith('.classmngrNextFileDialog', [StringComparison]::Ordinal)) { continue }
        if ([string]$current.ClassName -cne 'QFileDialog') { throw "P9-17 workspace dialog has unexpected class '$($current.ClassName)'." }
        $Context.ownerHandles[(Get-P914RuntimeKey $root)] = [Int64]$handle.ToInt64()
        $matches.Add([ordered]@{ root = $root; handle = [Int64]$handle.ToInt64() })
    }
    if ($matches.Count -gt 1) { throw 'More than one measured-process P9-17 QFileDialog is visible.' }
    if ($matches.Count -eq 0) { return $null }
    $Context.dialogRoot = $matches[0].root
    $Context.dialogHandle = [Int64]$matches[0].handle
    if (-not $Context.dialogWindowHandles.Contains([Int64]$matches[0].handle)) { $Context.dialogWindowHandles.Add([Int64]$matches[0].handle) }
    return $matches[0].root
}

function Find-P917Suffix([object]$Context, [string]$Suffix, [string]$Container = 'main', [string]$ControlType = '') {
    if ($Container -ceq 'main') { $root = Get-P914MainRoot $Context }
    elseif ($Container -ceq 'dialog') { $root = $Context.dialogRoot; if ($null -eq $root) { $root = Get-P917DialogRoot $Context } }
    else { throw "Unsupported P9-17 UIA search root: $Container" }
    if ($null -eq $root) { return $null }
    $found = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($element in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
        $current = $element.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { continue }
        $id = [string]$current.AutomationId
        $matchesSuffix = $id -ceq $Suffix -or $id.EndsWith('.' + $Suffix, [StringComparison]::Ordinal)
        if (-not $matchesSuffix) { continue }
        if ($ControlType -and [string]$current.ControlType.ProgrammaticName -cne $ControlType) { continue }
        $found.Add($element)
    }
    if ($found.Count -gt 1) { throw "AutomationId suffix '$Suffix' resolved to $($found.Count) UIA descendants." }
    if ($found.Count -eq 0) { return $null }
    $rootKey = Get-P914RuntimeKey $root
    $Context.ownerHandles[(Get-P914RuntimeKey $found[0])] = [Int64]$Context.ownerHandles[$rootKey]
    return $found[0]
}

function Wait-P917Suffix([object]$Context, [string]$EvidenceDirectory, [string]$Suffix, [int]$TimeoutSeconds, [string]$Container = 'main', [string]$ControlType = '') {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory
        $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured process exited while waiting for UIA AutomationId suffix '$Suffix'." }
        $element = Find-P917Suffix $Context $Suffix $Container $ControlType
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for UIA AutomationId suffix '$Suffix'."
}

function Get-P917DpiRestoreFailureMessage([System.Exception]$Exception) {
    $current = $Exception
    while ($null -ne $current) {
        try {
            if ($current.Data.Contains('DpiContextRestoreFailureMessage')) { return [string]$current.Data['DpiContextRestoreFailureMessage'] }
        } catch { }
        $current = $current.InnerException
    }
    return $null
}

function Get-P917PrimaryFailureMessage([System.Exception]$Exception) {
    $current = $Exception
    while ($null -ne $current) {
        try {
            if ($current.Data.Contains('P917PrimaryOperationFailureMessage')) { return [string]$current.Data['P917PrimaryOperationFailureMessage'] }
        } catch { }
        if ($null -eq $current.InnerException) { return [string]$current.Message }
        $current = $current.InnerException
    }
    return [string]$Exception.Message
}

function Get-P917WindowFromPointDiagnostic([System.Exception]$Exception) {
    $current = $Exception
    while ($null -ne $current) {
        try {
            if ($current.Data.Contains('P917WindowFromPointDiagnostic')) { return $current.Data['P917WindowFromPointDiagnostic'] }
        } catch { }
        $current = $current.InnerException
    }
    return $null
}

function Convert-P917WindowFromPointDiagnostic([object]$NativeDiagnostic) {
    if ($null -eq $NativeDiagnostic) { return $null }
    $physicalBounds = $null
    if ([bool]$NativeDiagnostic.PhysicalBoundsAvailable) {
        $physicalBounds = [ordered]@{
            x = [int]$NativeDiagnostic.Left; y = [int]$NativeDiagnostic.Top
            width = [int]$NativeDiagnostic.Width; height = [int]$NativeDiagnostic.Height
            boundsReadContext = [string]$NativeDiagnostic.ReadContext
        }
    }
    return [ordered]@{
        windowHandle = [Int64]$NativeDiagnostic.WindowHandle
        expectedProcessId = [Int64]$NativeDiagnostic.ExpectedProcessId
        processId = if ([bool]$NativeDiagnostic.ProcessIdAvailable) { [Int64]$NativeDiagnostic.ProcessId } else { $null }
        ownerProcessMatchesExpected = [bool]$NativeDiagnostic.ProcessIdAvailable -and [Int64]$NativeDiagnostic.ProcessId -eq [Int64]$NativeDiagnostic.ExpectedProcessId
        isVisible = if ([bool]$NativeDiagnostic.VisibilityAvailable) { [bool]$NativeDiagnostic.IsVisible } else { $null }
        physicalBounds = $physicalBounds
        screenPoint = [ordered]@{ x = [int]$NativeDiagnostic.ScreenX; y = [int]$NativeDiagnostic.ScreenY }
        readContext = [string]$NativeDiagnostic.ReadContext
        processIdReadError = [string]$NativeDiagnostic.ProcessIdReadError
        visibilityReadError = [string]$NativeDiagnostic.VisibilityReadError
        physicalBoundsReadError = [string]$NativeDiagnostic.PhysicalBoundsReadError
    }
}

function Set-P917WindowFromPointDiagnostic([object]$HostCorrelation, [object]$NativeDiagnostic) {
    if ($null -eq $HostCorrelation -or $null -eq $NativeDiagnostic) { return }
    $HostCorrelation.hitTest = Convert-P917WindowFromPointDiagnostic $NativeDiagnostic
    $HostCorrelation.windowHandle = [Int64]$NativeDiagnostic.WindowHandle
    $HostCorrelation.processId = if ([bool]$NativeDiagnostic.ProcessIdAvailable) { [Int64]$NativeDiagnostic.ProcessId } else { $null }
    $HostCorrelation.isVisible = if ([bool]$NativeDiagnostic.VisibilityAvailable) { [bool]$NativeDiagnostic.IsVisible } else { $null }
}

function Get-P917OpenProfileGeometrySample([object]$Context, [System.Windows.Automation.AutomationElement]$Element, [int]$Attempt) {
    $sample = [ordered]@{ attempt = $Attempt; capturedAtUtc = Get-P914Utc; valid = $false; failure = $null; dpiContextRestoreFailure = $null; target = $null; selectedOwner = $null }
    try {
        $sample.target = [P917ClickGeometryNative]::ReadWithPhysicalCoordinateContext([System.Func[object]]{
            $current = $Element.Current
            $rectangle = $current.BoundingRectangle
            return [ordered]@{
                automationId = [string]$current.AutomationId; name = [string]$current.Name
                controlType = [string]$current.ControlType.ProgrammaticName; className = [string]$current.ClassName
                processId = [Int64]$current.ProcessId; isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen
                nativeWindowHandle = [Int64]$current.NativeWindowHandle
                boundingRectangle = [ordered]@{ x = [double]$rectangle.X; y = [double]$rectangle.Y; width = [double]$rectangle.Width; height = [double]$rectangle.Height }
                boundingRectangleReadContext = 'PER_MONITOR_AWARE_V2'
            }
        })
        $rectangle = $sample.target.boundingRectangle
        $nativeWindowHandle = [Int64]$sample.target.nativeWindowHandle
        $runtimeKey = Get-P914RuntimeKey $Element
        $hasMappedOwner = $Context.ownerHandles.ContainsKey($runtimeKey)
        $mappedOwnerHandle = if ($hasMappedOwner) { [Int64]$Context.ownerHandles[$runtimeKey] } else { [Int64]0 }
        $selectedOwnerHandle = [Int64]0
        $ownerSource = $null
        $sample.selectedOwner = [ordered]@{ windowHandle = [Int64]0; source = $null; processId = $null; isVisible = $null; win32Bounds = $null; physicalBounds = $null; dpiAwareness = $null; hostCorrelation = $null; readError = $null; fullyContainsTarget = $false }
        if ($nativeWindowHandle -gt 0) {
            $selectedOwnerHandle = $nativeWindowHandle
            $ownerSource = 'UIA NativeWindowHandle'
        } else {
            if (-not $hasMappedOwner -or $mappedOwnerHandle -le 0) { throw 'Open Teacher Profile handle-0 target has no measured-root UIA provenance for host correlation.' }
            $centerXValue = [double]$rectangle.x + ([double]$rectangle.width / 2.0)
            $centerYValue = [double]$rectangle.y + ([double]$rectangle.height / 2.0)
            if ([double]::IsNaN($centerXValue) -or [double]::IsInfinity($centerXValue) -or [double]::IsNaN($centerYValue) -or [double]::IsInfinity($centerYValue) -or $centerXValue -lt [int]::MinValue -or $centerXValue -gt [int]::MaxValue -or $centerYValue -lt [int]::MinValue -or $centerYValue -gt [int]::MaxValue) { throw 'Open Teacher Profile handle-0 target has no finite physical screen point for host correlation.' }
            $centerX = [int][Math]::Floor($centerXValue)
            $centerY = [int][Math]::Floor($centerYValue)
            $sample.selectedOwner.hostCorrelation = [ordered]@{
                method = 'WindowFromPoint at exact UIA target center under Per-Monitor-V2 awareness'
                targetNativeWindowHandle = [Int64]0; measuredRootWindowHandle = $mappedOwnerHandle
                screenX = $centerX; screenY = $centerY; readContext = 'PER_MONITOR_AWARE_V2'
                windowHandle = $null; processId = $null; isVisible = $null; hitTest = $null
            }
            try {
                $hitTest = [P917ClickGeometryNative]::FindVisibleHostAtPhysicalScreenPoint($centerX, $centerY, [uint32]$Context.process.Id)
                Set-P917WindowFromPointDiagnostic $sample.selectedOwner.hostCorrelation $hitTest
            } catch {
                $hitTestFailure = Get-P917WindowFromPointDiagnostic $_.Exception
                if ($null -ne $hitTestFailure) { Set-P917WindowFromPointDiagnostic $sample.selectedOwner.hostCorrelation $hitTestFailure }
                throw
            }
            $selectedOwnerHandle = [Int64]$hitTest.WindowHandle
            $ownerSource = 'visible measured-process WindowFromPoint host correlated at the handle-0 UIA target center'
            $sample.selectedOwner.hostCorrelation.windowHandle = $selectedOwnerHandle
        }
        $sample.selectedOwner.windowHandle = $selectedOwnerHandle
        $sample.selectedOwner.source = $ownerSource
        if ($selectedOwnerHandle -le 0) { throw 'Open Teacher Profile UIA target has no visible same-process owner HWND correlated to its geometry.' }
        $dpi = [P917ClickGeometryNative]::ReadDpiAwarenessDetails([IntPtr]$selectedOwnerHandle)
        $sample.selectedOwner.dpiAwareness = [ordered]@{
            threadContext = [Int64]$dpi[0]; threadAwareness = [int]$dpi[1]
            ownerContext = [Int64]$dpi[2]; ownerAwareness = [int]$dpi[3]
            ownerDpi = [Int64]$dpi[4]; systemDpi = [Int64]$dpi[5]
            uiAutomationBoundsReadContext = [string]$sample.target.boundingRectangleReadContext
            physicalBoundsReadContext = 'PER_MONITOR_AWARE_V2'
        }
        $geometry = [P917ClickGeometryNative]::ReadPhysicalBounds([IntPtr]$selectedOwnerHandle, [uint32]$Context.process.Id)
        $sample.selectedOwner.processId = [Int64]$geometry.ProcessId
        $sample.selectedOwner.isVisible = [bool]$geometry.IsVisible
        if ($null -ne $sample.selectedOwner.hostCorrelation) {
            $sample.selectedOwner.hostCorrelation.processId = [Int64]$geometry.ProcessId
            $sample.selectedOwner.hostCorrelation.isVisible = [bool]$geometry.IsVisible
        }
        $sample.selectedOwner.win32Bounds = [ordered]@{ x = [int]$geometry.Left; y = [int]$geometry.Top; width = [int]$geometry.Width; height = [int]$geometry.Height; boundsReadContext = [string]$geometry.BoundsReadDpiAwareness }
        $sample.selectedOwner.physicalBounds = [ordered]@{ x = [int]$geometry.Left; y = [int]$geometry.Top; width = [int]$geometry.Width; height = [int]$geometry.Height }
        $sample.selectedOwner.dpiAwareness = [ordered]@{
            threadContext = [Int64]$geometry.OriginalThreadDpiAwarenessContext; threadAwareness = [int]$geometry.OriginalThreadDpiAwareness
            ownerContext = [Int64]$geometry.WindowDpiAwarenessContext; ownerAwareness = [int]$geometry.WindowDpiAwareness
            ownerDpi = [Int64]$geometry.WindowDpi; systemDpi = [Int64]$geometry.SystemDpi
            uiAutomationBoundsReadContext = 'PER_MONITOR_AWARE_V2'
            physicalBoundsReadContext = [string]$geometry.BoundsReadDpiAwareness
        }
        $automationId = [string]$sample.target.automationId
        if ([Int64]$sample.target.processId -ne [Int64]$Context.process.Id) { throw 'Open Teacher Profile target is outside the measured app PID.' }
        if ([string]$sample.target.controlType -cne 'ControlType.MenuItem' -or -not ($automationId -ceq 'classMngrNextOpenTeacherProfile' -or $automationId.EndsWith('.classMngrNextOpenTeacherProfile', [StringComparison]::Ordinal))) { throw 'Open Teacher Profile UIA identity differs from the exact menu target.' }
        if (-not [bool]$sample.target.isEnabled -or [bool]$sample.target.isOffscreen) { throw 'Open Teacher Profile target is disabled or offscreen.' }
        if ([double]::IsNaN([double]$rectangle.X) -or [double]::IsNaN([double]$rectangle.Y) -or [double]::IsInfinity([double]$rectangle.X) -or [double]::IsInfinity([double]$rectangle.Y) -or [double]$rectangle.Width -le 0 -or [double]$rectangle.Height -le 0) { throw 'Open Teacher Profile target has no finite, nonempty UIA rectangle.' }
        if (-not [bool]$geometry.IsVisible) { throw 'Selected click owner HWND is not visible.' }
        if ([Int64]$geometry.ProcessId -ne [Int64]$Context.process.Id) { throw 'Selected click owner HWND does not belong to the measured app PID.' }
        if ([int]$geometry.Width -le 0 -or [int]$geometry.Height -le 0) { throw 'Selected click owner HWND has empty physical bounds.' }
        $contains = [double]$rectangle.X -ge [double]$geometry.Left -and [double]$rectangle.Y -ge [double]$geometry.Top -and
            ([double]$rectangle.X + [double]$rectangle.Width) -le ([double]$geometry.Left + [double]$geometry.Width) -and
            ([double]$rectangle.Y + [double]$rectangle.Height) -le ([double]$geometry.Top + [double]$geometry.Height)
        $sample.selectedOwner.fullyContainsTarget = [bool]$contains
        if (-not $contains) { throw 'Selected visible measured-process owner bounds do not fully contain the UIA target rectangle.' }
        $sample.valid = $true
    } catch {
        $sample.failure = Get-P917PrimaryFailureMessage $_.Exception
        $sample.dpiContextRestoreFailure = Get-P917DpiRestoreFailureMessage $_.Exception
        $hitTestFailure = Get-P917WindowFromPointDiagnostic $_.Exception
        if ($null -ne $hitTestFailure -and $null -ne $sample.selectedOwner -and $null -ne $sample.selectedOwner.hostCorrelation) {
            try { Set-P917WindowFromPointDiagnostic $sample.selectedOwner.hostCorrelation $hitTestFailure } catch { }
        }
        if ($null -ne $sample.dpiContextRestoreFailure) {
            try { $_.Exception.Data['P917OpenProfileGeometrySample'] = $sample } catch { }
            throw
        }
        $hasRawHitTestDiagnostic = $null -ne $sample.selectedOwner -and $null -ne $sample.selectedOwner.hostCorrelation -and $null -ne $sample.selectedOwner.hostCorrelation.hitTest
        if ($null -ne $sample.selectedOwner -and -not $hasRawHitTestDiagnostic) {
            if ($null -eq $sample.selectedOwner.processId) { try { $sample.selectedOwner.processId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$sample.selectedOwner.windowHandle) } catch { $sample.selectedOwner.readError = $_.Exception.Message } }
            if ($null -eq $sample.selectedOwner.isVisible) { try { $sample.selectedOwner.isVisible = [bool][P914Native]::IsVisible([IntPtr]$sample.selectedOwner.windowHandle) } catch { $sample.selectedOwner.readError = $_.Exception.Message } }
            if ($null -eq $sample.selectedOwner.win32Bounds) { try { $fallbackBounds = [P917ClickGeometryNative]::ReadPhysicalBounds([IntPtr]$sample.selectedOwner.windowHandle, [uint32]$Context.process.Id); $sample.selectedOwner.processId = [Int64]$fallbackBounds.ProcessId; $sample.selectedOwner.isVisible = [bool]$fallbackBounds.IsVisible; $sample.selectedOwner.win32Bounds = [ordered]@{ x = [int]$fallbackBounds.Left; y = [int]$fallbackBounds.Top; width = [int]$fallbackBounds.Width; height = [int]$fallbackBounds.Height; boundsReadContext = [string]$fallbackBounds.BoundsReadDpiAwareness }; $sample.selectedOwner.physicalBounds = [ordered]@{ x = [int]$fallbackBounds.Left; y = [int]$fallbackBounds.Top; width = [int]$fallbackBounds.Width; height = [int]$fallbackBounds.Height } } catch { $sample.selectedOwner.readError = $_.Exception.Message } }
            if ($null -eq $sample.selectedOwner.dpiAwareness) { try { $dpi = [P917ClickGeometryNative]::ReadDpiAwarenessDetails([IntPtr]$sample.selectedOwner.windowHandle); $sample.selectedOwner.dpiAwareness = [ordered]@{ threadContext = [Int64]$dpi[0]; threadAwareness = [int]$dpi[1]; ownerContext = [Int64]$dpi[2]; ownerAwareness = [int]$dpi[3]; ownerDpi = [Int64]$dpi[4]; systemDpi = [Int64]$dpi[5]; uiAutomationBoundsReadContext = 'PER_MONITOR_AWARE_V2'; physicalBoundsReadContext = 'PER_MONITOR_AWARE_V2' } } catch { $sample.selectedOwner.readError = $_.Exception.Message } }
        }
    }
    return $sample
}

function Get-P917OpenProfileGeometrySignature([object]$Sample) {
    $rectangle = $Sample.target.boundingRectangle
    $culture = [Globalization.CultureInfo]::InvariantCulture
    return ([string]$Sample.selectedOwner.windowHandle + '|' + ([double]$rectangle.x).ToString('R', $culture) + '|' + ([double]$rectangle.y).ToString('R', $culture) + '|' + ([double]$rectangle.width).ToString('R', $culture) + '|' + ([double]$rectangle.height).ToString('R', $culture))
}

function Save-P917OpenProfileClickFailure([object]$Context, [string]$EvidenceDirectory, [System.Windows.Automation.AutomationElement]$Element, [object[]]$GeometrySamples, [string]$OriginalFailure, [string]$DpiContextRestoreFailure = $null, [object]$Manifest = $null) {
    $failureSamples = [Collections.Generic.List[object]]::new()
    foreach ($sample in $GeometrySamples) { $failureSamples.Add($sample) }
    $diagnosticOnlyReacquisition = $false
    if ($null -eq $Element) {
        try {
            $Element = Find-P917Suffix $Context 'classMngrNextOpenTeacherProfile' 'main' 'ControlType.MenuItem'
            if ($null -ne $Element) { $diagnosticOnlyReacquisition = $true }
        } catch { }
    }
    if ($null -ne $Element -and ($failureSamples.Count -eq 0 -or $diagnosticOnlyReacquisition)) {
        try { $failureSamples.Add((Get-P917OpenProfileGeometrySample $Context $Element ($failureSamples.Count + 1))) } catch { }
    }
    $target = $null
    if ($failureSamples.Count -gt 0) { $target = $failureSamples[$failureSamples.Count - 1].target }
    if ($null -eq $target -and $null -ne $Element) {
        try {
            $target = [P917ClickGeometryNative]::ReadWithPhysicalCoordinateContext([System.Func[object]]{
                $current = $Element.Current; $rect = $current.BoundingRectangle
                return [ordered]@{ automationId = [string]$current.AutomationId; name = [string]$current.Name; controlType = [string]$current.ControlType.ProgrammaticName; className = [string]$current.ClassName; processId = [Int64]$current.ProcessId; isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen; nativeWindowHandle = [Int64]$current.NativeWindowHandle; boundingRectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }; boundingRectangleReadContext = 'PER_MONITOR_AWARE_V2' }
            })
        } catch { $target = [ordered]@{ captureError = $_.Exception.Message; nativeWindowHandle = $null; boundingRectangle = $null } }
    }
    $rootRecord = $null
    $root = $null
    try {
        $root = if ($null -ne $Context.mainWindowRoot) { $Context.mainWindowRoot } else { Get-P914MainRoot $Context }
        if ($null -ne $root) {
            $rootRecord = [P917ClickGeometryNative]::ReadWithPhysicalCoordinateContext([System.Func[object]]{
                $rootCurrent = $root.Current; $rootRect = $rootCurrent.BoundingRectangle
                return [ordered]@{ automationId = [string]$rootCurrent.AutomationId; processId = [Int64]$rootCurrent.ProcessId; nativeWindowHandle = [Int64]$rootCurrent.NativeWindowHandle; mappedWindowHandle = [Int64]$Context.mainWindowHandle; boundingRectangle = [ordered]@{ x = [double]$rootRect.X; y = [double]$rootRect.Y; width = [double]$rootRect.Width; height = [double]$rootRect.Height }; boundingRectangleReadContext = 'PER_MONITOR_AWARE_V2' }
            })
            try {
                $rootGeometry = [P917ClickGeometryNative]::ReadPhysicalBounds([IntPtr]$Context.mainWindowHandle, [uint32]$Context.process.Id)
                $rootRecord.ownerGeometry = [ordered]@{
                    windowHandle = [Int64]$Context.mainWindowHandle; processId = [Int64]$rootGeometry.ProcessId; isVisible = [bool]$rootGeometry.IsVisible
                    physicalBounds = [ordered]@{ x = [int]$rootGeometry.Left; y = [int]$rootGeometry.Top; width = [int]$rootGeometry.Width; height = [int]$rootGeometry.Height; boundsReadContext = [string]$rootGeometry.BoundsReadDpiAwareness }
                    dpiAwareness = [ordered]@{ ownerContext = [Int64]$rootGeometry.WindowDpiAwarenessContext; ownerAwareness = [int]$rootGeometry.WindowDpiAwareness; ownerDpi = [Int64]$rootGeometry.WindowDpi; systemDpi = [Int64]$rootGeometry.SystemDpi }
                }
            } catch { $rootRecord.ownerGeometryReadError = $_.Exception.Message }
        }
    } catch { $rootRecord = [ordered]@{ captureError = $_.Exception.Message; boundingRectangle = $null } }
    $selectedOwner = $null
    if ($failureSamples.Count -gt 0) { $selectedOwner = $failureSamples[$failureSamples.Count - 1].selectedOwner }
    $path = Join-Path (Join-Path $EvidenceDirectory 'uia') 'open-teacher-profile-click-geometry-failure.json'
    if ($null -eq $target) { $target = [ordered]@{ automationId = 'classMngrNextOpenTeacherProfile'; nativeWindowHandle = $null; boundingRectangle = $null; captureError = 'Exact target was not exposed through UIA during the bounded click attempt or diagnostic-only reacquisition.' } }
    $screenshot = $null
    $screenshotCaptureError = $null
    if ($null -ne $Manifest) {
        try {
            if ($null -eq $root) { $root = Get-P914MainRoot $Context }
            if ($null -eq $root) { throw 'Measured app UIA root was unavailable for the failure-time screenshot.' }
            $screenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'open-teacher-profile-click-failure' $root
        } catch { $screenshotCaptureError = $_.Exception.Message }
    }
    $record = [ordered]@{ schema = 'p9-17-open-teacher-profile-click-geometry-failure-v1'; capturedAtUtc = Get-P914Utc; actionKind = 'open-teacher-profile'; originalFailure = $OriginalFailure; dpiContextRestoreFailure = $DpiContextRestoreFailure; diagnosticPath = 'uia/open-teacher-profile-click-geometry-failure.json'; diagnosticOnlyReacquisition = $diagnosticOnlyReacquisition; target = $target; root = $rootRecord; selectedOwner = $selectedOwner; geometrySamples = $failureSamples.ToArray(); failureTimeScreenshot = $screenshot; failureTimeScreenshotCaptureError = $screenshotCaptureError }
    Write-P914Json $path $record
    return $path
}

function Wait-P917StableOpenProfileTarget([object]$Context, [string]$EvidenceDirectory, [int]$TimeoutSeconds, [object]$Manifest = $null) {
    $maximumGeometryReadAttempts = 8
    $consecutiveStableReadsRequired = 2
    $geometrySamples = [Collections.Generic.List[object]]::new()
    $lastElement = $null
    $previousValidSample = $null
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $maximumWaitSeconds = [Math]::Min(3.0, [double]$TimeoutSeconds)
    try {
        for ($attempt = 1; $attempt -le $maximumGeometryReadAttempts -and $watch.Elapsed.TotalSeconds -lt $maximumWaitSeconds; $attempt++) {
            Drain-P914Output $Context $EvidenceDirectory
            $Context.process.Refresh()
            if ($Context.process.HasExited) { throw "Measured process exited while stabilizing the exact Open Teacher Profile UIA target with geometry read $attempt." }
            $element = Find-P917Suffix $Context 'classMngrNextOpenTeacherProfile' 'main' 'ControlType.MenuItem'
            if ($null -eq $element) { $previousValidSample = $null; Start-Sleep -Milliseconds 75; continue }
            $lastElement = $element
            $sample = Get-P917OpenProfileGeometrySample $Context $element $attempt
            $geometrySamples.Add($sample)
            if ($sample.valid) {
                $sample.signature = Get-P917OpenProfileGeometrySignature $sample
                if ($null -ne $previousValidSample -and $sample.signature -ceq [string]$previousValidSample.signature) {
                    return [ordered]@{ element = $element; geometry = $sample; geometrySamples = $geometrySamples.ToArray(); maximumGeometryReadAttempts = $maximumGeometryReadAttempts; consecutiveStableReadsRequired = $consecutiveStableReadsRequired }
                }
                $previousValidSample = $sample
            } else {
                $previousValidSample = $null
            }
            Start-Sleep -Milliseconds 75
        }
        throw "Open Teacher Profile did not produce $consecutiveStableReadsRequired consecutive valid, identical owner/rectangle reads within $maximumGeometryReadAttempts attempts and $maximumWaitSeconds seconds."
    } catch {
        $originalFailure = Get-P917PrimaryFailureMessage $_.Exception
        $restoreFailure = Get-P917DpiRestoreFailureMessage $_.Exception
        try {
            if ($_.Exception.Data.Contains('P917OpenProfileGeometrySample')) { $geometrySamples.Add($_.Exception.Data['P917OpenProfileGeometrySample']) }
        } catch { }
        try { $null = Save-P917OpenProfileClickFailure $Context $EvidenceDirectory $lastElement $geometrySamples.ToArray() $originalFailure $restoreFailure $Manifest } catch { }
        throw
    }
}

function Invoke-P917StableOpenProfileClick([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [object]$StableTarget) {
    $geometrySamples = [Collections.Generic.List[object]]::new()
    foreach ($sample in $StableTarget.geometrySamples) { $geometrySamples.Add($sample) }
    $element = $StableTarget.element
    try {
        $finalSample = Get-P917OpenProfileGeometrySample $Context $element ($geometrySamples.Count + 1)
        $geometrySamples.Add($finalSample)
        if (-not [bool]$finalSample.valid) { throw "Open Teacher Profile final owner/geometry verification failed: $($finalSample.failure)" }
        $finalSample.signature = Get-P917OpenProfileGeometrySignature $finalSample
        if ([string]$finalSample.signature -cne [string]$StableTarget.geometry.signature) { throw 'Open Teacher Profile owner or target rectangle changed after its stable geometry pair.' }
        $action = [P917ClickGeometryNative]::ReadWithPhysicalCoordinateContext([System.Func[object]]{
            return (Add-P917Action $Context $Manifest $EvidenceDirectory 'open-teacher-profile' $element 'UIA-targeted click after two stable Per-Monitor-V2 owner geometry reads')
        })
        $rectangle = $finalSample.target.boundingRectangle
        $ownerBounds = $finalSample.selectedOwner.physicalBounds
        $x = [int][Math]::Round([double]$rectangle.x + ([double]$rectangle.width / 2.0))
        $y = [int][Math]::Round([double]$rectangle.y + ([double]$rectangle.height / 2.0))
        $handle = [Int64]$finalSample.selectedOwner.windowHandle
        [P917ClickGeometryNative]::ClickPhysicalScreenPoint([IntPtr]$handle, [uint32]$Context.process.Id, $x, $y)
        $action.ownerGeometryVerification = [ordered]@{
            source = [string]$finalSample.selectedOwner.source; geometryReadAttempts = [int]$geometrySamples.Count
            consecutiveStableReadsRequired = [int]$StableTarget.consecutiveStableReadsRequired
            nativeWindowHandle = [Int64]$finalSample.target.nativeWindowHandle
            selectedOwnerWindowHandle = $handle; selectedOwnerProcessId = [Int64]$finalSample.selectedOwner.processId
            selectedOwnerPhysicalBounds = $ownerBounds; dpiAwareness = $finalSample.selectedOwner.dpiAwareness
            stableGeometrySignatures = @($StableTarget.geometrySamples | ForEach-Object { [string]$_.signature })
        }
        $action.pointerAction = [ordered]@{ actionType = 'UIA-derived-physical-bounds-window-message-click'; screenX = $x; screenY = $y; windowHandle = $handle; windowProcessId = [Int64]$finalSample.selectedOwner.processId; targetProcessId = [Int64]$Context.process.Id; targetRectangle = $rectangle; ownerWindowRectangle = $ownerBounds }
        $action.pointerInputReturnedAtUtc = Get-P914Utc
        Complete-P917Action $EvidenceDirectory $action
        return $action
    } catch {
        $originalFailure = Get-P917PrimaryFailureMessage $_.Exception
        $restoreFailure = Get-P917DpiRestoreFailureMessage $_.Exception
        try { $null = Save-P917OpenProfileClickFailure $Context $EvidenceDirectory $element $geometrySamples.ToArray() $originalFailure $restoreFailure $Manifest } catch { }
        throw
    }
}

function Find-P917ChildNamed([object]$Context, [System.Windows.Automation.AutomationElement]$Parent, [string]$Name, [string]$ControlType) {
    $condition = [System.Windows.Automation.AndCondition]::new([System.Windows.Automation.Condition[]]@(
        [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, $Name),
        [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::$($ControlType.Replace('ControlType.', '')))
    ))
    $matches = $Parent.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)
    if ($matches.Count -gt 1) { throw "Exact UIA descendant '$Name' ($ControlType) resolved to $($matches.Count) elements." }
    if ($matches.Count -eq 0) { return $null }
    $element = $matches[0]
    $current = $element.Current
    if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.Name -cne $Name -or [string]$current.ControlType.ProgrammaticName -cne $ControlType) { throw "UIA descendant identity differs for '$Name'." }
    $parentKey = Get-P914RuntimeKey $Parent
    $Context.ownerHandles[(Get-P914RuntimeKey $element)] = [Int64]$Context.ownerHandles[$parentKey]
    return $element
}

function Get-P917ClassListItems([object]$Context, [System.Windows.Automation.AutomationElement]$ClassList) {
    $condition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.ControlType]::ListItem)
    $elements = $ClassList.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)
    $rootKey = Get-P914RuntimeKey $Context.mainWindowRoot
    $ownerHandle = [Int64]$Context.ownerHandles[$rootKey]
    $items = [Collections.Generic.List[object]]::new()
    $targets = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($element in $elements) {
        $current = $element.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.ListItem') { throw 'Classes list item is not bound to the measured process and ListItem control type.' }
        $Context.ownerHandles[(Get-P914RuntimeKey $element)] = $ownerHandle
        $selected = $false
        $selectionPatternAvailable = $false
        try {
            $selectionPattern = [System.Windows.Automation.SelectionItemPattern]$element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
            $selected = [bool]$selectionPattern.Current.IsSelected
            $selectionPatternAvailable = $true
        } catch { }
        $rectangle = $current.BoundingRectangle
        $items.Add([ordered]@{
            name = [string]$current.Name; automationId = [string]$current.AutomationId
            controlType = [string]$current.ControlType.ProgrammaticName; className = [string]$current.ClassName
            processId = [Int64]$current.ProcessId; isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen
            helpText = [string]$current.HelpText; itemStatus = [string]$current.ItemStatus; itemType = [string]$current.ItemType
            selectionItemPatternAvailable = $selectionPatternAvailable; isSelected = $selected
            rectangle = [ordered]@{ x = [double]$rectangle.X; y = [double]$rectangle.Y; width = [double]$rectangle.Width; height = [double]$rectangle.Height }
            runtimeId = Get-P914RuntimeKey $element
        })
        $targets.Add($element)
    }
    $scroll = $null
    try {
        $scroll = [System.Windows.Automation.ScrollPattern]$ClassList.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern)
    } catch { }
    $scrollInfo = if ($null -ne $scroll) {
        [ordered]@{ available = $true; horizontallyScrollable = [bool]$scroll.Current.HorizontallyScrollable; verticallyScrollable = [bool]$scroll.Current.VerticallyScrollable; horizontalScrollPercent = [double]$scroll.Current.HorizontalScrollPercent; verticalScrollPercent = [double]$scroll.Current.VerticalScrollPercent; verticalViewSize = [double]$scroll.Current.VerticalViewSize }
    } else { [ordered]@{ available = $false; horizontallyScrollable = $false; verticallyScrollable = $false; horizontalScrollPercent = $null; verticalScrollPercent = $null; verticalViewSize = $null } }
    return [ordered]@{ elements = $targets.ToArray(); record = [ordered]@{ itemCount = $items.Count; items = $items.ToArray(); scrollPattern = $scrollInfo }; scrollPattern = $scroll }
}

function Save-P917ClassListItemsSnapshot([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Name, [System.Windows.Automation.AutomationElement]$ClassList) {
    $listSnapshot = Get-P914ControlSnapshot $Context $ClassList
    $itemSnapshot = Get-P917ClassListItems $Context $ClassList
    $record = [ordered]@{ name = $Name; capturedAtUtc = Get-P914Utc; classList = $listSnapshot; itemCount = [int]$itemSnapshot.record.itemCount; items = $itemSnapshot.record.items; scrollPattern = $itemSnapshot.record.scrollPattern }
    $path = Join-Path (Join-Path $EvidenceDirectory 'uia') ($Name + '.json')
    Write-P914Json $path $record
    $file = Get-Item -LiteralPath $path
    $manifestRecord = [ordered]@{ relativePath = 'uia/' + $Name + '.json'; sha256 = Get-P914FileSha256 $path; sizeBytes = [Int64]$file.Length; capturedAtUtc = $record.capturedAtUtc; processId = [Int64]$Context.process.Id; windowHandle = [Int64]$listSnapshot.ownerWindowHandle }
    $Manifest.route.uiaSnapshots.Add($manifestRecord)
    return [ordered]@{ path = $path; record = $record; manifestRecord = $manifestRecord; elements = $itemSnapshot.elements; scrollPattern = $itemSnapshot.scrollPattern }
}

function Find-P917TargetClassListItem([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [System.Windows.Automation.AutomationElement]$ClassList, [string]$TargetAccessibleName, [int]$MaximumScrolls = 40, [string]$SnapshotPrefix = 'class-list-items') {
    $search = [Collections.Generic.List[object]]::new()
    for ($page = 0; $page -le $MaximumScrolls; $page++) {
        $name = if ($page -eq 0) { $SnapshotPrefix + '-before-target-search' } else { $SnapshotPrefix + '-target-search-' + $page.ToString('D2') }
        $snapshot = Save-P917ClassListItemsSnapshot $Context $Manifest $EvidenceDirectory $name $ClassList
        $matches = @($snapshot.elements | Where-Object { [string]$_.Current.Name -ceq $TargetAccessibleName -and -not [bool]$_.Current.IsOffscreen })
        $search.Add([ordered]@{ page = $page; snapshotRelativePath = [string]$snapshot.manifestRecord.relativePath; itemCount = [int]$snapshot.record.itemCount; matchingAccessibleNameCount = $matches.Count; matchingRuntimeIds = @($matches | ForEach-Object { Get-P914RuntimeKey $_ }); scrollPercent = $snapshot.record.scrollPattern.verticalScrollPercent })
        if ($matches.Count -gt 1) { throw "Target class accessible name '$TargetAccessibleName' resolved to $($matches.Count) visible Classes ListItems." }
        if ($matches.Count -eq 1) { return [ordered]@{ element = $matches[0]; search = $search.ToArray() } }
        if ($page -ge $MaximumScrolls -or ($null -ne $snapshot.scrollPattern -and [bool]$snapshot.record.scrollPattern.verticallyScrollable -and [double]$snapshot.record.scrollPattern.verticalScrollPercent -ge 100)) { break }

        $visibleBefore = @($snapshot.elements | Where-Object { -not [bool]$_.Current.IsOffscreen } | ForEach-Object { [string]$_.Current.Name })
        if ($null -ne $snapshot.scrollPattern -and [bool]$snapshot.record.scrollPattern.verticallyScrollable) {
            $action = Add-P917Action $Context $Manifest $EvidenceDirectory 'scroll-class-list-for-target' $ClassList 'ScrollPattern.Scroll(Vertical=LargeIncrement) with measured list bounds and PID'
            $action.verticalScrollPercentBefore = [double]$snapshot.record.scrollPattern.verticalScrollPercent
            $snapshot.scrollPattern.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount, [System.Windows.Automation.ScrollAmount]::LargeIncrement)
            $action.verticalScrollPercentAfter = [double]$snapshot.scrollPattern.Current.VerticalScrollPercent
            $action.scrollPatternIsVerticallyScrollable = [bool]$snapshot.scrollPattern.Current.VerticallyScrollable
        } else {
            $listCurrent = $ClassList.Current
            if ([uint32]$listCurrent.ProcessId -ne [uint32]$Context.process.Id -or [string]$listCurrent.ControlType.ProgrammaticName -cne 'ControlType.List' -or [bool]$listCurrent.IsOffscreen) { throw 'Classes list changed UIA identity before scroll.' }
            $listRectangle = $listCurrent.BoundingRectangle
            if ([double]$listRectangle.Width -le 0 -or [double]$listRectangle.Height -le 0) { throw 'Classes list has no visible UIA bounds for scrolling.' }
            $windowBounds = [P914Native]::GetBounds([IntPtr][Int64]$Context.mainWindowHandle, [uint32]$Context.process.Id)
            $listLeft = [int][Math]::Floor($listRectangle.X); $listTop = [int][Math]::Floor($listRectangle.Y)
            $listRight = [int][Math]::Ceiling($listRectangle.X + $listRectangle.Width); $listBottom = [int][Math]::Ceiling($listRectangle.Y + $listRectangle.Height)
            if ($listLeft -lt $windowBounds[0] -or $listTop -lt $windowBounds[1] -or $listRight -gt ($windowBounds[0] + $windowBounds[2]) -or $listBottom -gt ($windowBounds[1] + $windowBounds[3])) { throw 'Classes list UIA bounds escape the measured main-window bounds.' }
            $action = Add-P917Action $Context $Manifest $EvidenceDirectory 'scroll-class-list-for-target' $ClassList 'verified-window-message-wheel'
            $wheel = [P917ScrollNative]::ScrollListWindowMessage([IntPtr][Int64]$Context.mainWindowHandle, $Context.process.Handle, $listLeft, $listTop, $listRight, $listBottom, -120, [uint32]$Context.process.Id)
            $action.scrollInput = [ordered]@{
                sequence = @('WaitForInputIdle', 'ScreenToClient', 'GetClientRect', 'WM_MOUSEWHEEL')
                windowHandle = [Int64]$Context.mainWindowHandle; expectedProcessId = [Int64]$Context.process.Id; windowProcessId = [Int64]$wheel[1]
                screenX = [Int64]$wheel[2]; screenY = [Int64]$wheel[3]; clientX = [Int64]$wheel[4]; clientY = [Int64]$wheel[5]
                clientBounds = [ordered]@{ left = [Int64]$wheel[6]; top = [Int64]$wheel[7]; right = [Int64]$wheel[8]; bottom = [Int64]$wheel[9] }
                listBounds = [ordered]@{ left = [Int64]$listLeft; top = [Int64]$listTop; right = [Int64]$listRight; bottom = [Int64]$listBottom }
                wheelDelta = [Int64]$wheel[10]; waitForInputIdleSucceeded = ([Int64]$wheel[0] -eq 0); messageSent = ([Int64]$wheel[11] -eq 1)
                beforeVisibleAccessibleNames = @($visibleBefore)
            }
        }
        Start-Sleep -Milliseconds 150
        $Context.process.Refresh()
        if ($Context.process.HasExited) { throw 'Measured process exited while scrolling the Classes list.' }
        $after = Get-P917ClassListItems $Context $ClassList
        $visibleAfter = @($after.elements | Where-Object { -not [bool]$_.Current.IsOffscreen } | ForEach-Object { [string]$_.Current.Name })
        $beforeSignature = [string]::Join("`n", [string[]]$visibleBefore)
        $afterSignature = [string]::Join("`n", [string[]]$visibleAfter)
        $action.scrollInput.beforeVisibleAccessibleNames = @($visibleBefore)
        $action.scrollInput.afterVisibleAccessibleNames = @($visibleAfter)
        $action.scrollInput.visibleRowsChanged = ($beforeSignature -cne $afterSignature)
        Complete-P917Action $EvidenceDirectory $action
        if ($beforeSignature -ceq $afterSignature) { throw 'Classes list scroll did not change the visible UIA rows.' }
    }
    $Manifest.route.targetClassSearch = [ordered]@{ targetAccessibleName = $TargetAccessibleName; selectionProperty = 'Exact UIA ListItem.Name derived from fixture class id 1 and Classes navigation data'; maximumScrolls = $MaximumScrolls; pages = $search.ToArray(); matched = $false }
    return [ordered]@{ element = $null; search = $search.ToArray() }
}

function Add-P917Action([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [System.Windows.Automation.AutomationElement]$Element, [string]$Method) {
    $snapshot = Get-P914ControlSnapshot $Context $Element
    if ([Int64]$snapshot.processId -ne [Int64]$Context.process.Id -or -not [bool]$snapshot.isEnabled -or [bool]$snapshot.isOffscreen) { throw "UIA action target is not enabled, visible, and PID-bound: $ActionKind" }
    $action = [ordered]@{ actionKind = $ActionKind; method = $Method; target = $snapshot; issuedAtUtc = Get-P914Utc }
    $Manifest.route.actions.Add($action)
    return $action
}

function Complete-P917Action([string]$EvidenceDirectory, [object]$Action) {
    $Action.completedAtUtc = Get-P914Utc
    Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $Action
}

function Invoke-P917BoundedClick([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [System.Windows.Automation.AutomationElement]$Element, [Int64]$ExpectedHandle, [string]$Method = 'UIA-derived bounded window click', [System.Windows.Automation.AutomationElement]$ClipToElement = $null) {
    $action = Add-P917Action $Context $Manifest $EvidenceDirectory $ActionKind $Element $Method
    $current = $Element.Current
    $snapshot = $action.target
    $handle = [Int64]$snapshot.ownerWindowHandle
    $targetProcessId = [uint32]$Context.process.Id
    if ($handle -ne $ExpectedHandle -or [Int64]$snapshot.ownerWindowProcessId -ne [Int64]$targetProcessId -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $targetProcessId) { throw "UIA click owner HWND is not the measured expected window: $ActionKind" }
    $rect = $current.BoundingRectangle
    if ([double]::IsNaN($rect.X) -or [double]::IsNaN($rect.Y) -or $rect.Width -le 0 -or $rect.Height -le 0) { throw "UIA click target has no nonempty rectangle: $ActionKind" }
    $bounds = [P914Native]::GetBounds([IntPtr]$handle, $targetProcessId)
    $targetRectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }
    $ownerRectangle = [ordered]@{ x = [double]$bounds[0]; y = [double]$bounds[1]; width = [double]$bounds[2]; height = [double]$bounds[3] }
    $clickLeft = [Math]::Max([double]$rect.X, [double]$bounds[0]); $clickTop = [Math]::Max([double]$rect.Y, [double]$bounds[1])
    $clickRight = [Math]::Min([double]($rect.X + $rect.Width), [double]($bounds[0] + $bounds[2])); $clickBottom = [Math]::Min([double]($rect.Y + $rect.Height), [double]($bounds[1] + $bounds[3]))
    if ($clickRight -le $clickLeft -or $clickBottom -le $clickTop) { throw "UIA click target does not intersect its owner window: $ActionKind" }
    $clickRegion = [ordered]@{
        strategy = 'intersection-of-target-and-owner-window-bounds'; targetRectangle = $targetRectangle
        ownerWindowRectangle = $ownerRectangle
        clickRectangle = [ordered]@{ x = $clickLeft; y = $clickTop; width = ($clickRight - $clickLeft); height = ($clickBottom - $clickTop) }
    }
    if ($null -ne $ClipToElement) {
        $clipCurrent = $ClipToElement.Current
        if ([uint32]$clipCurrent.ProcessId -ne $targetProcessId -or [string]$clipCurrent.ControlType.ProgrammaticName -cne 'ControlType.List' -or [bool]$clipCurrent.IsOffscreen) { throw "UIA click clipping control is not the visible measured Classes List: $ActionKind" }
        $clipSnapshot = Get-P914ControlSnapshot $Context $ClipToElement
        $clipOwner = [Int64]$clipSnapshot.ownerWindowHandle
        if ($clipOwner -ne $ExpectedHandle) { throw "UIA click clipping control belongs to a different measured window: $ActionKind" }
        $clipRect = $clipCurrent.BoundingRectangle
        if ([double]::IsNaN($clipRect.X) -or [double]::IsNaN($clipRect.Y) -or $clipRect.Width -le 0 -or $clipRect.Height -le 0) { throw "UIA click clipping control has no visible bounds: $ActionKind" }
        $clipRectangle = [ordered]@{ x = [double]$clipRect.X; y = [double]$clipRect.Y; width = [double]$clipRect.Width; height = [double]$clipRect.Height }
        $clickLeft = [Math]::Max($clickLeft, [double]$clipRect.X); $clickTop = [Math]::Max($clickTop, [double]$clipRect.Y)
        $clickRight = [Math]::Min($clickRight, [double]($clipRect.X + $clipRect.Width)); $clickBottom = [Math]::Min($clickBottom, [double]($clipRect.Y + $clipRect.Height))
        if ($clickRight -le $clickLeft -or $clickBottom -le $clickTop) { throw "UIA click target does not intersect its visible Classes List bounds: $ActionKind" }
        $clickRegion = [ordered]@{
            strategy = 'intersection-of-target-and-visible-classes-list-bounds'; targetRectangle = $targetRectangle
            clippingControl = $clipSnapshot; clippingRectangle = $clipRectangle
            clickRectangle = [ordered]@{ x = $clickLeft; y = $clickTop; width = ($clickRight - $clickLeft); height = ($clickBottom - $clickTop) }
        }
    }
    $x = [int][Math]::Round($clickLeft + (($clickRight - $clickLeft) / 2)); $y = [int][Math]::Round($clickTop + (($clickBottom - $clickTop) / 2))
    if ($x -lt $bounds[0] -or $x -ge ($bounds[0] + $bounds[2]) -or $y -lt $bounds[1] -or $y -ge ($bounds[1] + $bounds[3])) { throw "UIA click point is outside its measured owner window: $ActionKind" }
    $action.pointerAction = [ordered]@{ actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'; screenX = $x; screenY = $y; windowHandle = $handle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$handle); targetProcessId = [Int64]$targetProcessId; clickRegion = $clickRegion; screenBounds = [ordered]@{ x = [int]$bounds[0]; y = [int]$bounds[1]; width = [int]$bounds[2]; height = [int]$bounds[3] } }
    [P914Native]::ClickScreenPoint([IntPtr]$handle, $targetProcessId, $x, $y)
    $action.pointerInputReturnedAtUtc = Get-P914Utc
    Complete-P917Action $EvidenceDirectory $action
    return $action
}

function Invoke-P917PatternAction([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [System.Windows.Automation.AutomationElement]$Element, [string]$PatternName) {
    $action = Add-P917Action $Context $Manifest $EvidenceDirectory $ActionKind $Element ($PatternName + '.Invoke')
    $pattern = [System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $pattern.Invoke()
    $action.invocationReturnedAtUtc = Get-P914Utc
    Complete-P917Action $EvidenceDirectory $action
    return $action
}

function Open-P917Workspace([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$WorkspacePath, [int]$TimeoutSeconds) {
    Set-P917SamplingContext $Context 'operation' 'open-workspace-through-ui'
    $fileMenu = Find-P914NamedControl $Context 'main' 'ControlType.MenuItem' 'File'
    if ($null -eq $fileMenu) { throw 'File menu is not exposed through measured-process UIA.' }
    $menuAction = Add-P917Action $Context $Manifest $EvidenceDirectory 'open-file-menu' $fileMenu 'ExpandCollapsePattern.Expand'
    $expanded = $false
    try {
        $expand = [System.Windows.Automation.ExpandCollapsePattern]$fileMenu.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
        if ($expand.Current.ExpandCollapseState -ne [System.Windows.Automation.ExpandCollapseState]::Expanded) { $expand.Expand() }
        $expanded = $true
    } catch {
        $menuAction.method = 'InvokePattern.Invoke'
        ([System.Windows.Automation.InvokePattern]$fileMenu.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)).Invoke()
        $expanded = $true
    }
    if (-not $expanded) { throw 'File menu did not expand through UIA.' }
    $menuAction.expansionReturnedAtUtc = Get-P914Utc
    Complete-P917Action $EvidenceDirectory $menuAction
    $stableOpenTarget = Wait-P917StableOpenProfileTarget $Context $EvidenceDirectory $TimeoutSeconds $Manifest
    $null = Invoke-P917StableOpenProfileClick $Context $Manifest $EvidenceDirectory $stableOpenTarget
    $dialogRoot = $null
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory
        $dialogRoot = Get-P917DialogRoot $Context
        if ($null -ne $dialogRoot) { break }
        Start-Sleep -Milliseconds 100
    }
    if ($null -eq $dialogRoot) { throw 'P9-17 Open Teacher Profile did not open its PID-bound Qt QFileDialog.' }
    $dialogCurrent = $dialogRoot.Current
    if ([string]$dialogCurrent.ClassName -cne 'QFileDialog' -or [string]$dialogCurrent.ControlType.ProgrammaticName -cne 'ControlType.Window') { throw 'P9-17 workspace open dialog is not the measured Qt QFileDialog.' }
    $dialogHandle = [Int64]$Context.dialogHandle
    $dialogSnapshot = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'open-workspace-file-dialog' $dialogRoot
    $null = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'workspace-file-dialog' $dialogRoot
    $fileName = Wait-P917Suffix $Context $EvidenceDirectory 'fileNameEdit' $TimeoutSeconds 'dialog' ''
    if ([string]$fileName.Current.ClassName -cne 'QLineEdit') { throw 'Workspace path field is not a measured Qt QLineEdit.' }
    $setPath = Add-P917Action $Context $Manifest $EvidenceDirectory 'set-workspace-path' $fileName 'ValuePattern.SetValue'
    $path = [IO.Path]::GetFullPath($WorkspacePath)
    $valuePattern = [System.Windows.Automation.ValuePattern]$fileName.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $valuePattern.SetValue($path)
    $setPath.pathReadback = Get-P914EditValue $fileName
    if ([string]$setPath.pathReadback.Trim('"') -cne $path) { throw 'Workspace file dialog did not read back the exact pinned fixture path.' }
    $setPath.dialogWindowHandle = $dialogHandle
    Complete-P917Action $EvidenceDirectory $setPath
    $openButton = Find-P917ChildNamed $Context $dialogRoot 'Open' 'ControlType.Button'
    if ($null -eq $openButton) { throw 'Qt file dialog does not expose the exact Open button.' }
    $confirm = $null
    try { $confirm = Invoke-P917PatternAction $Context $Manifest $EvidenceDirectory 'confirm-workspace-open' $openButton 'InvokePattern' }
    catch {
        $confirm = Invoke-P917BoundedClick $Context $Manifest $EvidenceDirectory 'confirm-workspace-open' $openButton $dialogHandle
    }
    $confirm.dialogWindowHandle = $dialogHandle
    $confirm.selectedWorkspacePath = $path
    $routeArrival = [Int64]$Context.arrival
    $Context.dialogRoot = $null; $Context.dialogHandle = 0L
    $closeWatch = [Diagnostics.Stopwatch]::StartNew()
    while ($closeWatch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory
        if ($null -eq (Get-P917DialogRoot $Context)) { break }
        Start-Sleep -Milliseconds 100
    }
    if ($closeWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Qt file dialog remained open after UIA confirmed the fixture path.' }
    $route = Wait-P914Route $Context $EvidenceDirectory 'my_workspace' $routeArrival $TimeoutSeconds
    return [ordered]@{ routeEvent = $route; selectedPath = $path; dialogWindowHandle = $dialogHandle; dialogTreeRelativePath = [string]$dialogSnapshot.manifestRecord.relativePath }
}

function Set-P917SamplingContext([object]$Context, [string]$Category, [string]$Operation) {
    Set-P914SamplerContext $Context $Category $Operation
    if ($Context.Contains('automationClientSampler') -and $null -ne $Context.automationClientSampler) {
        $Context.automationClientSampler.SetContext($Category, $Operation)
    }
}

function Get-P917OptionalProperty([object]$Value, [string]$Name) {
    if ($null -eq $Value) { return $null }
    if ($Value -is [Collections.IDictionary]) {
        if ($Value.Contains($Name)) { return $Value[$Name] }
        return $null
    }
    $property = $Value.PSObject.Properties[$Name]
    if ($null -ne $property) { return $property.Value }
    return $null
}

function Get-P917MemoryWindow([object]$Context, [object]$Manifest, [string]$Name, [string]$State, [string]$Phase, [int]$SampleCount = 10, [int]$IntervalMilliseconds = 100, [string]$MatchedState = '') {
    Set-P917SamplingContext $Context 'normal-use' $Name
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $samples = [Collections.Generic.List[object]]::new()
    $windowErrors = [Collections.Generic.List[object]]::new()
    $targetExceededSampleIndexes = [Collections.Generic.List[int]]::new()
    $ceilingExceededSampleIndexes = [Collections.Generic.List[int]]::new()
    for ($index = 0; $index -lt $SampleCount; $index++) {
        Start-Sleep -Milliseconds $IntervalMilliseconds
        $sample = [ordered]@{
            sampleIndex = $index; capturedAtUtc = Get-P914Utc; elapsedMilliseconds = [int]$watch.ElapsedMilliseconds
            processState = 'unknown'; workingSetBytes = $null; privateBytes = $null
            peakWorkingSetBytes = $null; handles = $null; threadCount = $null; metricErrors = @()
        }
        $sampleErrors = [Collections.Generic.List[object]]::new()
        try {
            $Context.process.Refresh()
            if ($Context.process.HasExited) { $sample.processState = 'exited' } else { $sample.processState = 'running' }
        } catch {
            $errorRecord = [ordered]@{ sampleIndex = $index; metric = 'processState'; exceptionType = $_.Exception.GetType().FullName; category = [string]$_.CategoryInfo.Category; fullyQualifiedErrorId = [string]$_.FullyQualifiedErrorId; message = $_.Exception.Message }
            $sampleErrors.Add($errorRecord)
        }
        if ([string]$sample.processState -ceq 'running') {
            foreach ($metric in @('workingSetBytes', 'privateBytes', 'peakWorkingSetBytes', 'handles', 'threadCount')) {
                try {
                    switch ($metric) {
                        'workingSetBytes' { $sample[$metric] = [Int64]$Context.process.WorkingSet64 }
                        'privateBytes' { $sample[$metric] = [Int64]$Context.process.PrivateMemorySize64 }
                        'peakWorkingSetBytes' { $sample[$metric] = [Int64]$Context.process.PeakWorkingSet64 }
                        'handles' { $sample[$metric] = [Int64]$Context.process.HandleCount }
                        'threadCount' { $sample[$metric] = [Int64]$Context.process.Threads.Count }
                    }
                } catch {
                    $errorRecord = [ordered]@{ sampleIndex = $index; metric = $metric; exceptionType = $_.Exception.GetType().FullName; category = [string]$_.CategoryInfo.Category; fullyQualifiedErrorId = [string]$_.FullyQualifiedErrorId; message = $_.Exception.Message }
                    $sampleErrors.Add($errorRecord)
                }
            }
        } elseif ([string]$sample.processState -ceq 'exited') {
            $sampleErrors.Add([ordered]@{ sampleIndex = $index; metric = 'processState'; exceptionType = $null; category = 'process-exited'; fullyQualifiedErrorId = $null; message = 'Measured process had exited before this scheduled memory sample.' })
        }
        $sample.metricErrors = $sampleErrors.ToArray()
        $samples.Add($sample)
        foreach ($sampleError in $sampleErrors) { $windowErrors.Add($sampleError) }
        if ($null -ne $sample.workingSetBytes -and [Int64]$sample.workingSetBytes -ge $WorkingSetTarget) {
            $targetExceededSampleIndexes.Add([int]$index)
        }
        if ($null -ne $sample.workingSetBytes -and [Int64]$sample.workingSetBytes -gt $WorkingSetCeiling) {
            $ceilingExceededSampleIndexes.Add([int]$index)
        }
    }
    $allWorkingSetSamplesPresent = @($samples | Where-Object { $null -eq $_.workingSetBytes }).Count -eq 0
    $targetMet = $targetExceededSampleIndexes.Count -eq 0 -and $allWorkingSetSamplesPresent
    $ceilingPassed = $ceilingExceededSampleIndexes.Count -eq 0 -and $allWorkingSetSamplesPresent
    $window = [ordered]@{
        windowId = $Name; state = $State; phase = $Phase; matchedState = if ($MatchedState) { $MatchedState } else { $null }
        processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle
        sampleCount = $samples.Count; sampleIntervalMilliseconds = $IntervalMilliseconds
        nominalDurationMilliseconds = $SampleCount * $IntervalMilliseconds; startedAtUtc = $samples[0].capturedAtUtc
        completedAtUtc = Get-P914Utc; samplingCompleted = ($samples.Count -eq $SampleCount); samplingErrorCount = $windowErrors.Count
        samplingErrors = $windowErrors.ToArray()
        workingSetTargetBytes = $WorkingSetTarget; workingSetTargetExclusive = $true
        workingSetTargetMet = $targetMet; workingSetTargetExceededSampleIndexes = $targetExceededSampleIndexes.ToArray()
        workingSetCeilingBytes = $WorkingSetCeiling; workingSetCeilingExclusive = $false
        workingSetCeilingPassed = $ceilingPassed; workingSetCeilingExceededSampleIndexes = $ceilingExceededSampleIndexes.ToArray()
        samples = $samples.ToArray()
    }
    $Manifest.memory.windows.Add($window)
    return $window
}

function Find-P917RosterTab([object]$Context) {
    $tabs = Find-P917Suffix $Context 'selectedClassEditorSections' 'main'
    if ($null -eq $tabs) { throw 'Selected Class editor tab widget is absent.' }
    $tab = Find-P917ChildNamed $Context $tabs 'Roster' 'ControlType.TabItem'
    if ($null -eq $tab) { throw 'Selected Class editor does not expose the exact Roster tab through UIA.' }
    return $tab
}

function Find-P917DetailsTab([object]$Context) {
    $tabs = Find-P917Suffix $Context 'selectedClassEditorSections' 'main'
    if ($null -eq $tabs) { throw 'Selected Class editor tab widget is absent.' }
    $tab = Find-P917ChildNamed $Context $tabs 'Class Details and Times' 'ControlType.TabItem'
    if ($null -eq $tab) { throw 'Selected Class editor does not expose the exact details tab through UIA.' }
    return $tab
}

function Test-P917TabSelected([System.Windows.Automation.AutomationElement]$Tab) {
    try {
        $selectionPattern = [System.Windows.Automation.SelectionItemPattern]$Tab.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
        return [bool]$selectionPattern.Current.IsSelected
    } catch {
        return $false
    }
}

function Invoke-P917TabClick([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [System.Windows.Automation.AutomationElement]$Tab, [string]$TargetValue) {
    $selectionPattern = $null
    try { $selectionPattern = [System.Windows.Automation.SelectionItemPattern]$Tab.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern) } catch { $selectionPattern = $null }
    if ($null -eq $selectionPattern) {
        $action = Invoke-P917BoundedClick $Context $Manifest $EvidenceDirectory $ActionKind $Tab ([Int64]$Context.mainWindowHandle) 'UIA-targeted measured TabItem click'
        $action.selectedTabName = [string]$Tab.Current.Name
        $action.expectedSection = $TargetValue
        return $action
    }

    $action = Add-P917Action $Context $Manifest $EvidenceDirectory $ActionKind $Tab 'SelectionItemPattern.Select with IsSelected readback'
    $action.selectedTabName = [string]$Tab.Current.Name
    $action.expectedSection = $TargetValue
    $action.selectionItemPatternAvailable = $true
    $selectionPattern.Select()
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $samples = [Collections.Generic.List[object]]::new()
    while ($watch.Elapsed.TotalSeconds -lt 3) {
        $isSelected = [bool]$selectionPattern.Current.IsSelected
        $samples.Add([ordered]@{ capturedAtUtc = Get-P914Utc; isSelected = $isSelected })
        if ($isSelected) { break }
        Start-Sleep -Milliseconds 50
    }
    $action.selectionItemPatternIsSelected = [bool]$selectionPattern.Current.IsSelected
    $action.selectionReadbackSamples = $samples.ToArray()
    if (-not [bool]$action.selectionItemPatternIsSelected) { throw "UIA SelectionItemPattern did not select the exact tab '$TargetValue'." }
    Complete-P917Action $EvidenceDirectory $action
    return $action
}

function Find-P917RosterTable([object]$Context) {
    $root = Get-P914MainRoot $Context
    if ($null -eq $root) { return $null }
    $matches = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($element in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
        $current = $element.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { continue }
        $id = [string]$current.AutomationId
        if (($id -cne 'nextRosterTable' -and -not $id.EndsWith('.nextRosterTable', [StringComparison]::Ordinal)) -or [string]$current.ControlType.ProgrammaticName -notin @('ControlType.Table', 'ControlType.DataGrid')) { continue }
        $matches.Add($element)
    }
    if ($matches.Count -gt 1) { throw "Roster table AutomationId resolved to $($matches.Count) Table/DataGrid UIA elements." }
    if ($matches.Count -eq 0) { return $null }
    $table = $matches[0]
    $rootKey = Get-P914RuntimeKey $root
    $Context.ownerHandles[(Get-P914RuntimeKey $table)] = [Int64]$Context.ownerHandles[$rootKey]
    $current = $table.Current
    if ([string]$current.ControlType.ProgrammaticName -notin @('ControlType.Table', 'ControlType.DataGrid')) { throw "nextRosterTable has unexpected UIA type $($current.ControlType.ProgrammaticName)." }
    return $table
}

function Get-P917SectionRosterAbsenceCheckpoint([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$SnapshotName, [object]$LeaveAction) {
    $detailsTab = Find-P917DetailsTab $Context
    if ($null -eq $detailsTab -or -not (Test-P917TabSelected $detailsTab)) {
        throw 'Roster suspension checkpoint requires a fresh SelectionItemPattern readback of the selected Class Details and Times tab.'
    }
    $selectedTabReadbackAtUtc = Get-P914Utc
    $leaveCompletedAt = [DateTimeOffset]::Parse([string]$LeaveAction.completedAtUtc).ToUniversalTime()
    $selectedTabReadbackAt = [DateTimeOffset]::Parse([string]$selectedTabReadbackAtUtc).ToUniversalTime()
    if (($selectedTabReadbackAt - $leaveCompletedAt).TotalMilliseconds -lt 500) {
        throw 'Fresh Class Details SelectionItemPattern readback occurred less than 500 ms after the section-leave action.'
    }

    # Re-query the active main-window tree after the tab switch. Do not inspect
    # the pre-leave table element: the selected-class editor owns and destroys
    # its roster widget when leaving the section.
    $activeTable = Find-P917RosterTable $Context
    if ($null -ne $activeTable) { throw 'Fresh active-section UIA tree still exposes nextRosterTable after leaving Roster.' }
    $tree = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory $SnapshotName $Context.mainWindowRoot
    $root = $tree.record.root
    if ([Int64]$root.processId -ne [Int64]$Context.process.Id -or
        [Int64]$root.ownerWindowHandle -ne [Int64]$Context.mainWindowHandle -or
        [Int64]$root.ownerWindowProcessId -ne [Int64]$Context.process.Id) {
        throw 'Fresh roster-absence UIA tree is not bound to the measured process and main-window HWND.'
    }
    $treeCapturedAt = [DateTimeOffset]::Parse([string]$tree.record.capturedAtUtc).ToUniversalTime()
    $settledMilliseconds = [int][Math]::Floor(($treeCapturedAt - $leaveCompletedAt).TotalMilliseconds)
    if ($settledMilliseconds -lt 500) {
        throw 'Fresh active-window UIA tree was captured less than 500 ms after the section-leave action.'
    }
    $rosterControls = @($tree.record.descendants | Where-Object {
        $automationId = [string]$_.automationId
        $automationId -ceq 'selectedClassRosterTab' -or
            $automationId.IndexOf('.selectedClassRosterTab.', [StringComparison]::Ordinal) -ge 0 -or
            $automationId -ceq 'selectedClassRosterScrollArea' -or
            $automationId.IndexOf('.selectedClassRosterScrollArea.', [StringComparison]::Ordinal) -ge 0 -or
            $automationId -ceq 'nextRosterTable' -or
            $automationId.EndsWith('.nextRosterTable', [StringComparison]::Ordinal)
    })
    if ($rosterControls.Count -ne 0) { throw "Fresh active-section UIA tree still exposes $($rosterControls.Count) roster control(s)." }

    return [ordered]@{
        selectedSection = 'Class Details and Times'
        selectedTabVerified = $true
        selectedTabReadbackSource = 'fresh SelectionItemPattern.IsSelected readback after section leave'
        selectedTabReadbackAtUtc = $selectedTabReadbackAtUtc
        settledMilliseconds = $settledMilliseconds
        activeUiAutomationRosterTablePresent = $false
        activeTreeRosterControlCount = 0
        freshActiveTreeCaptured = $true
        proofKind = 'fresh-active-tree-roster-absence'
        leaveActionKind = [string]$LeaveAction.actionKind
        leaveActionIndex = [int]$Manifest.route.actions.IndexOf($LeaveAction)
        processId = [Int64]$Context.process.Id
        mainWindowHandle = [Int64]$Context.mainWindowHandle
        activeTreeRootProcessId = [Int64]$root.processId
        activeTreeRootOwnerWindowHandle = [Int64]$root.ownerWindowHandle
        activeTreeCapturedAtUtc = [string]$tree.record.capturedAtUtc
        uiaSnapshotRelativePath = [string]$tree.manifestRecord.relativePath
    }
}

function Invoke-P917MeasuredCycleSectionExit([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$CycleTag) {
    $detailsTab = Find-P917DetailsTab $Context
    $leaveAction = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory "cycle-$CycleTag-leave-roster" $detailsTab 'Class Details and Times'
    $detailsTab = $null
    Start-Sleep -Milliseconds 500
    $checkpoint = Get-P917SectionRosterAbsenceCheckpoint $Context $Manifest $EvidenceDirectory "cycle-$CycleTag-post-suspension-tree" $leaveAction
    $checkpoint.observedAtUtc = Get-P914Utc
    $screenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory "cycle-$CycleTag-post-suspension" $Context.mainWindowRoot
    $checkpoint.screenshotRelativePath = [string]$screenshot.relativePath
    $checkpoint.screenshotCapturedAtUtc = [string]$screenshot.capturedAtUtc
    $leaveAction = $null
    $screenshot = $null
    return [pscustomobject]@{ checkpoint = $checkpoint }
}

function Invoke-P917ClientGcReferenceEvent([object]$Context, [object]$Diagnostic, [object]$AbsenceCheckpoint, [int]$CycleNumber, [string]$PostSuspensionWindowId, [ValidateSet('no-gc', 'client-gc')][string]$Mode) {
    if (-not [bool]$AbsenceCheckpoint.freshActiveTreeCaptured -or
        [bool]$AbsenceCheckpoint.activeUiAutomationRosterTablePresent -or
        [Int64]$AbsenceCheckpoint.activeTreeRosterControlCount -ne 0) {
        throw 'Client GC reference diagnostic requires the fresh active-tree roster-absence proof before its collection boundary.'
    }
    if ($null -ne $Context.mainWindowRoot -or $null -ne $Context.dialogRoot) {
        throw 'Client GC reference diagnostic requires all retained UIA root references to be released before the collection boundary.'
    }
    if ([Int64]$Diagnostic.clientProcessId -ne [Int64]$PID) {
        throw 'Client GC reference diagnostic process identity differs from the active PowerShell client.'
    }

    $counterSnapshot = {
        [ordered]@{
            generation0Collections = [Int64][GC]::CollectionCount(0)
            generation1Collections = [Int64][GC]::CollectionCount(1)
            generation2Collections = [Int64][GC]::CollectionCount(2)
            managedBytesEstimate = [Int64][GC]::GetTotalMemory($false)
        }
    }
    $event = [ordered]@{
        schema = 'classmngr-phase9-p9-17-client-gc-reference-event-v1'
        cycleNumber = $CycleNumber; mode = $Mode
        armOrderSelection = [string]$Diagnostic.armOrderSelection
        armPosition = [Int64]$Diagnostic.armPosition
        clientProcessId = [Int64]$PID; appProcessId = [Int64]$Context.process.Id
        postSuspensionWindowId = $PostSuspensionWindowId
        freshActiveTreeCaptured = $true
        activeTreeCapturedAtUtc = [string]$AbsenceCheckpoint.activeTreeCapturedAtUtc
        activeTreeRosterControlCount = [Int64]$AbsenceCheckpoint.activeTreeRosterControlCount
        activeUiAutomationRosterTablePresent = [bool]$AbsenceCheckpoint.activeUiAutomationRosterTablePresent
        uiAutomationReferenceScopesEnded = $true
        retainedUiAutomationRootsReleased = $true
        collectionTarget = 'PowerShell automation client only'
        appGcRequested = $false; osWorkingSetTrimRequested = $false; heapCompactRequested = $false
        eventStartedAtUtc = Get-P914Utc
        collectionSequence = if ($Mode -ceq 'client-gc') { '[GC]::Collect(); [GC]::WaitForPendingFinalizers(); [GC]::Collect()' } else { $null }
        collectionStartedAtUtc = $null; collectionCompletedAtUtc = $null; collectionDurationMilliseconds = $null
        forcedCollectionRequested = ($Mode -ceq 'client-gc')
        forcedCollectionCallCount = 0
        runtime = [ordered]@{
            frameworkVersion = [Environment]::Version.ToString()
            maxGeneration = [Int64][GC]::MaxGeneration
            before = & $counterSnapshot
            after = $null; delta = $null
        }
        eventCompletedAtUtc = $null
    }
    if ($Mode -ceq 'client-gc') {
        $event.collectionStartedAtUtc = Get-P914Utc
        $collectionWatch = [Diagnostics.Stopwatch]::StartNew()
        [GC]::Collect()
        [GC]::WaitForPendingFinalizers()
        [GC]::Collect()
        $collectionWatch.Stop()
        $event.collectionDurationMilliseconds = [Int64]$collectionWatch.ElapsedMilliseconds
        $event.collectionCompletedAtUtc = Get-P914Utc
        $event.forcedCollectionCallCount = 2
    }
    $event.runtime.after = & $counterSnapshot
    $event.runtime.delta = [ordered]@{
        generation0Collections = [Int64]$event.runtime.after.generation0Collections - [Int64]$event.runtime.before.generation0Collections
        generation1Collections = [Int64]$event.runtime.after.generation1Collections - [Int64]$event.runtime.before.generation1Collections
        generation2Collections = [Int64]$event.runtime.after.generation2Collections - [Int64]$event.runtime.before.generation2Collections
        managedBytesEstimate = [Int64]$event.runtime.after.managedBytesEstimate - [Int64]$event.runtime.before.managedBytesEstimate
    }
    $event.eventCompletedAtUtc = Get-P914Utc
    $Diagnostic.collectionEvents.Add($event)
    return $event
}

function Get-P917RosterTable([object]$Context, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory
        $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured process exited while waiting for the UIA roster table." }
        $table = Find-P917RosterTable $Context
        if ($null -ne $table) { return $table }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for a Table/DataGrid AutomationId suffix 'nextRosterTable'."
}

function Get-P917GridMetrics([System.Windows.Automation.AutomationElement]$Table) {
    $grid = Get-P914Grid $Table
    return [ordered]@{ grid = $grid; rowCount = [int]$grid.Current.RowCount; columnCount = [int]$grid.Current.ColumnCount }
}

function Get-P917GridCell([object]$Context, [System.Windows.Automation.AutomationElement]$Table, [int]$Row, [int]$Column) {
    $metrics = Get-P917GridMetrics $Table
    if ($Row -lt 0 -or $Row -ge $metrics.rowCount -or $Column -lt 0 -or $Column -ge $metrics.columnCount) { throw "UIA roster grid coordinate is outside the exposed model: row=$Row column=$Column." }
    $cell = $metrics.grid.GetItem($Row, $Column)
    $tableKey = Get-P914RuntimeKey $Table
    $Context.ownerHandles[(Get-P914RuntimeKey $cell)] = [Int64]$Context.ownerHandles[$tableKey]
    return [ordered]@{ grid = $metrics.grid; cell = $cell; rowCount = $metrics.rowCount; columnCount = $metrics.columnCount }
}

function Ensure-P917CellVisible([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$Row, [int]$Column) {
    for ($attempt = 0; $attempt -lt 12; $attempt++) {
        $table = Find-P917RosterTable $Context
        if ($null -eq $table) { throw 'Roster table disappeared before a required UIA grid read.' }
        $entry = Get-P917GridCell $Context $table $Row $Column
        $cellCurrent = $entry.cell.Current
        $rect = $cellCurrent.BoundingRectangle
        if (-not [bool]$cellCurrent.IsOffscreen -and $rect.Width -gt 0 -and $rect.Height -gt 0) { return [ordered]@{ table = $table; cell = $entry.cell; rowCount = $entry.rowCount; columnCount = $entry.columnCount } }
        $scrollItem = $null
        try { $scrollItem = [System.Windows.Automation.ScrollItemPattern]$entry.cell.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern) } catch { $scrollItem = $null }
        if ($null -ne $scrollItem) {
            $action = Add-P917Action $Context $Manifest $EvidenceDirectory 'scroll-roster-cell-into-view' $entry.cell 'ScrollItemPattern.ScrollIntoView'
            $action.row = $Row; $action.column = $Column
            $scrollItem.ScrollIntoView()
            $action.scrollReturnedAtUtc = Get-P914Utc
            Complete-P917Action $EvidenceDirectory $action
        } else {
            $scroll = $null
            try { $scroll = [System.Windows.Automation.ScrollPattern]$table.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern) } catch { $scroll = $null }
            if ($null -eq $scroll) { throw "Roster cell row=$Row column=$Column is offscreen and exposes no ScrollItemPattern or table ScrollPattern." }
            $h = if ([bool]$scroll.Current.HorizontallyScrollable) { [Math]::Min(100.0, 100.0 * $Column / [Math]::Max(1, ($entry.columnCount - 1))) } else { [System.Windows.Automation.ScrollPattern]::NoScroll }
            $v = if ([bool]$scroll.Current.VerticallyScrollable) { [Math]::Min(100.0, 100.0 * $Row / [Math]::Max(1, ($entry.rowCount - 1))) } else { [System.Windows.Automation.ScrollPattern]::NoScroll }
            $action = Add-P917Action $Context $Manifest $EvidenceDirectory 'scroll-roster-grid-to-cell' $table 'ScrollPattern.SetScrollPercent'
            $action.row = $Row; $action.column = $Column; $action.horizontalPercent = $h; $action.verticalPercent = $v
            $scroll.SetScrollPercent([double]$h, [double]$v)
            $action.scrollReturnedAtUtc = Get-P914Utc
            Complete-P917Action $EvidenceDirectory $action
        }
        Start-Sleep -Milliseconds 120
    }
    throw "Roster grid cell remained offscreen after bounded UIA scrolling: row=$Row column=$Column."
}

function Get-P917CellText([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$Row, [int]$Column) {
    $entry = Ensure-P917CellVisible $Context $Manifest $EvidenceDirectory $Row $Column
    $text = Get-P914ControlText $entry.cell
    return [ordered]@{ text = $text; cell = $entry.cell; table = $entry.table; rowCount = $entry.rowCount; columnCount = $entry.columnCount }
}

function Get-P917ExpectedHeaders {
    $headers = [Collections.Generic.List[string]]::new()
    foreach ($name in @('English', 'Korean', 'Winter', 'Speech Contest', 'Summer', 'Fall', 'Student', 'School', 'Notes')) { $headers.Add($name) }
    for ($index = 4; $index -le 150; $index++) { $headers.Add(('Stress Field {0:D3}' -f $index)) }
    return $headers.ToArray()
}

function Save-P917RosterContract([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Name, [System.Windows.Automation.AutomationElement]$Table, [object[]]$ObservedValues) {
    $metrics = Get-P917GridMetrics $Table
    $tablePattern = [System.Windows.Automation.TablePattern]$Table.GetCurrentPattern([System.Windows.Automation.TablePattern]::Pattern)
    $headers = @($tablePattern.Current.GetColumnHeaders())
    $headerRows = [Collections.Generic.List[object]]::new()
    foreach ($index in 0..($headers.Count - 1)) {
        $header = $headers[$index]
        if ([uint32]$header.Current.ProcessId -ne [uint32]$Context.process.Id) { throw "Roster header $index belongs to a different PID." }
        $Context.ownerHandles[(Get-P914RuntimeKey $header)] = [Int64]$Context.mainWindowHandle
        $headerRows.Add([ordered]@{ column = $index; text = Get-P914ControlText $header; control = Get-P914ControlSnapshot $Context $header })
    }
    $expectedHeaders = Get-P917ExpectedHeaders
    $actualHeaders = @($headerRows | ForEach-Object { [string]$_.text })
    if ($metrics.rowCount -ne 100 -or $metrics.columnCount -ne 156 -or $headers.Count -ne 156) { throw "Packaged roster UIA grid dimensions differ from fixture: rows=$($metrics.rowCount), columns=$($metrics.columnCount), headers=$($headers.Count)." }
    if (($actualHeaders -join "`n") -cne ($expectedHeaders -join "`n")) { throw "Packaged roster headers differ from the pinned 156-column model. First=$($actualHeaders[0]); last=$($actualHeaders[-1]); count=$($actualHeaders.Count)." }
    $record = [ordered]@{
        schema = 'classmngr-phase9-p9-17-roster-uia-contract-v1'; name = $Name; capturedAtUtc = Get-P914Utc
        processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle
        grid = [ordered]@{ rowCount = $metrics.rowCount; columnCount = $metrics.columnCount; pattern = 'GridPattern'; tablePattern = 'TablePattern' }
        headers = $headerRows.ToArray(); observedValues = @($ObservedValues)
    }
    $path = Join-Path (Join-Path $EvidenceDirectory 'uia') ($Name + '.json')
    Write-P914Json $path $record
    $entry = [ordered]@{ relativePath = 'uia/' + $Name + '.json'; sha256 = Get-P914FileSha256 $path; sizeBytes = [Int64](Get-Item -LiteralPath $path).Length; processId = [Int64]$Context.process.Id; windowHandle = [Int64]$Context.mainWindowHandle; capturedAtUtc = $record.capturedAtUtc }
    $Manifest.route.uiaSnapshots.Add($entry)
    return [ordered]@{ path = $path; record = $record; manifestRecord = $entry }
}

function Read-P917Targets([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [object[]]$Targets, [string]$ExpectedField) {
    $results = [Collections.Generic.List[object]]::new()
    foreach ($target in $Targets) {
        $value = Get-P917CellText $Context $Manifest $EvidenceDirectory ([int]$target.row) ([int]$target.modelColumn)
        $expectedProperty = $target.PSObject.Properties[$ExpectedField]
        if ($null -eq $expectedProperty) { throw "Pinned P9-17 cell target omits expected value field '$ExpectedField'." }
        $expected = [string]$expectedProperty.Value
        $result = [ordered]@{
            name = [string]$target.name; row = [int]$target.row; modelColumn = [int]$target.modelColumn
            storedColumn = [int]$target.storedColumn; expected = $expected; actual = [string]$value.text
            matched = ([string]$value.text -ceq $expected); capturedAtUtc = Get-P914Utc
            control = Get-P914ControlSnapshot $Context $value.cell
        }
        $results.Add($result)
        if (-not [bool]$result.matched) { throw "UIA roster value mismatch at $($target.name): expected '$expected', observed '$($value.text)'." }
    }
    return $results.ToArray()
}

function Set-P917RosterCell([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [object]$Target, [switch]$RequireValuePattern) {
    $entry = Ensure-P917CellVisible $Context $Manifest $EvidenceDirectory ([int]$Target.row) ([int]$Target.modelColumn)
    $cell = $entry.cell
    $newValue = [string]$Target.afterValue
    $action = Add-P917Action $Context $Manifest $EvidenceDirectory ('edit-target-' + [string]$Target.name) $cell 'ValuePattern.SetValue'
    $action.row = [int]$Target.row; $action.modelColumn = [int]$Target.modelColumn; $action.storedColumn = [int]$Target.storedColumn
    $action.expectedBeforeValue = [string]$Target.beforeValue; $action.valueAfter = $newValue
    $usedValuePattern = $false
    try {
        $valuePattern = [System.Windows.Automation.ValuePattern]$cell.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
        $valuePattern.SetValue($newValue)
        Start-Sleep -Milliseconds 120
        $observed = Get-P914ControlText (Get-P917GridCell $Context (Find-P917RosterTable $Context) ([int]$Target.row) ([int]$Target.modelColumn)).cell
        if ([string]$observed -cne $newValue) { throw "ValuePattern.SetValue readback mismatch: '$observed'." }
        $usedValuePattern = $true
        $action.method = 'UIA GridPattern.GetItem + ValuePattern.SetValue with exact UIA readback'
    } catch {
        $action.valuePatternFailure = $_.Exception.Message
        Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
        if ($RequireValuePattern) { throw "Diagnostic observer requires ValuePattern.SetValue at $($Target.name); provider failure: $($_.Exception.Message)" }
        $cell = (Get-P917GridCell $Context (Find-P917RosterTable $Context) ([int]$Target.row) ([int]$Target.modelColumn)).cell
        $action.method = 'UIA GridPattern cell focus + F2/EditKeyPressed and bounded keyboard text entry'
        $cell.SetFocus()
        [System.Windows.Forms.SendKeys]::SendWait('{F2}')
        [System.Windows.Forms.SendKeys]::SendWait('^a')
        [System.Windows.Forms.SendKeys]::SendWait($newValue)
        [System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
        Start-Sleep -Milliseconds 180
        $cell = (Get-P917GridCell $Context (Find-P917RosterTable $Context) ([int]$Target.row) ([int]$Target.modelColumn)).cell
        $observed = Get-P914ControlText $cell
        if ([string]$observed -cne $newValue) { throw "UIA keyboard edit readback mismatch at $($Target.name): '$observed'." }
        $action.keyboardReadback = $observed
    }
    $action.valuePatternSetValueSucceeded = $usedValuePattern
    $action.immediateSetValueReadback = [string]$observed
    $action.immediateSetValueReadbackAtUtc = Get-P914Utc
    $action.immediateSetValueReadbackSource = if ($usedValuePattern) { 'GridPattern.GetItem + ValuePattern.SetValue followed by exact UIA readback' } else { 'bounded keyboard edit followed by exact UIA readback' }
    $action.valueReadback = if ($usedValuePattern) { $newValue } else { [string]$observed }
    Complete-P917Action $EvidenceDirectory $action
    return $action
}

function New-P917ImmediateSetValueReadbacks([object[]]$Targets, [object[]]$EditActions) {
    if ($Targets.Count -ne $EditActions.Count) { throw 'Diagnostic SetValue action count differs from its three pinned target cells.' }
    $results = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $Targets.Count; $index++) {
        $target = $Targets[$index]
        $action = $EditActions[$index]
        if (-not [bool]$action.valuePatternSetValueSucceeded) { throw "Diagnostic edit did not use ValuePattern.SetValue for $($target.name)." }
        $actual = [string]$action.immediateSetValueReadback
        $expected = [string]$target.afterValue
        $matched = $actual -ceq $expected
        if (-not $matched) { throw "Immediate ValuePattern.SetValue readback mismatch at $($target.name): expected '$expected', observed '$actual'." }
        $results.Add([ordered]@{
            name = [string]$target.name; row = [int]$target.row; modelColumn = [int]$target.modelColumn
            storedColumn = [int]$target.storedColumn; expected = $expected; actual = $actual; matched = $matched
            capturedAtUtc = [string]$action.immediateSetValueReadbackAtUtc
            evidenceSource = 'immediate-readback-after-GridPattern-GetItem-and-ValuePattern-SetValue'
            editOrder = $index + 1
        })
    }
    return $results.ToArray()
}

function Get-P917CycleTargets([object[]]$Targets, [int]$CycleNumber) {
    $reverse = ($CycleNumber % 2) -eq 1
    $result = [Collections.Generic.List[object]]::new()
    foreach ($target in $Targets) {
        $expectedBefore = if ($reverse) { [string]$target.afterValue } else { [string]$target.beforeValue }
        $nextValue = if ($reverse) { [string]$target.beforeValue } else { [string]$target.afterValue }
        $result.Add([pscustomobject]@{
            name = [string]$target.name
            row = [int]$target.row
            modelColumn = [int]$target.modelColumn
            storedColumn = [int]$target.storedColumn
            beforeValue = $expectedBefore
            afterValue = $nextValue
        })
    }
    return $result.ToArray()
}

function Save-P917Roster([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $button = Wait-P917Suffix $Context $EvidenceDirectory 'nextRosterSave' $TimeoutSeconds 'main' 'ControlType.Button'
    if (-not [bool]$button.Current.IsEnabled -or [bool]$button.Current.IsOffscreen) { throw 'Manual-save roster button is not enabled and visible after the three deterministic edits.' }
    $action = $null
    try { $action = Invoke-P917PatternAction $Context $Manifest $EvidenceDirectory 'save-roster' $button 'InvokePattern' }
    catch { $action = Invoke-P917BoundedClick $Context $Manifest $EvidenceDirectory 'save-roster' $button ([Int64]$Context.mainWindowHandle) }
    $action.saveButtonAfterInvokeEnabled = [bool]$button.Current.IsEnabled
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory
        $button = Find-P917Suffix $Context 'nextRosterSave' 'main' 'ControlType.Button'
        if ($null -ne $button -and -not [bool]$button.Current.IsEnabled) { $action.savedStateObservedAtUtc = Get-P914Utc; break }
        Start-Sleep -Milliseconds 100
    }
    if ($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Roster Save button remained enabled after the required save action.' }
    $status = Find-P917Suffix $Context 'nextRosterStatus' 'main' ''
    if ($null -ne $status) { $action.statusReadback = Get-P914ControlText $status }
    return $action
}

function Get-P917LifecycleEvidence([object]$Context, [string]$PageKey, [string]$State, [int]$AfterArrival, [int]$TimeoutSeconds) {
    $event = Wait-P914Lifecycle $Context (Split-Path -Parent $Context.stdoutPath | Split-Path -Parent) $PageKey $State $AfterArrival $TimeoutSeconds
    return $event
}

function Add-P917DefaultProcessHeapEvidence([object]$Context, [object]$Manifest, [string]$EvidenceDirectory) {
    $events = [Collections.Generic.List[object]]::new()
    foreach ($observed in @($Context.lifecycleEvents)) {
        $record = Get-P917OptionalProperty $observed 'record'
        if ([string](Get-P917OptionalProperty $record 'event') -cin @(
            'default-process-heap-roster-checkpoint',
            'roster-save-stage-checkpoint'
        )) {
            $events.Add($record)
        }
    }
    if ($events.Count -lt 2) { throw 'Opt-in app output did not contain the classes baseline and Roster checkpoint records.' }

    $validCheckpoints = @(
        'classes-page-baseline-before-roster',
        'roster-model-loaded',
        'roster-cell-data-changed',
        'roster-save-before-snapshot',
        'roster-save-after-snapshot',
        'roster-save-after-preparation',
        'roster-save-before-row-source-setup',
        'roster-save-after-row-source-setup',
        'roster-save-before-row-wise-repository-write',
        'roster-save-after-row-wise-repository-write',
        'roster-save-returned-successfully',
        'roster-widget-destroyed-after-successful-leave',
        'roster-widget-after-delete-returned-after-child-destruction'
    )
    $stageEventCounts = [ordered]@{}
    foreach ($checkpointName in $validCheckpoints) {
        $stageEventCounts[$checkpointName] = 0
    }
    $groups = [Collections.Generic.List[object]]::new()
    $groupsById = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
    $allHeapSamplesAvailable = $true
    $allProcessMemorySamplesAvailable = $true
    $previousSequence = [Int64]0
    $baselineCount = 0
    for ($eventIndex = 0; $eventIndex -lt $events.Count; $eventIndex++) {
        $event = $events[$eventIndex]
        $sequence = Get-P917OptionalProperty $event 'event_sequence'
        if (-not (Test-P917JsonInteger -Value $sequence) -or [Int64]$sequence -ne ($previousSequence + 1)) {
            throw 'Default process heap app checkpoint event_sequence is not a contiguous, monotonically increasing integer sequence.'
        }
        $previousSequence = [Int64]$sequence
        $eventType = [string](Get-P917OptionalProperty $event 'event')
        $isSaveStage = $eventType -ceq 'roster-save-stage-checkpoint'
        $saveSequence = $null
        if ($isSaveStage) {
            if ([string](Get-P917OptionalProperty $event 'schema') -cne 'classmngr-roster-save-stage-diagnostic-v1') {
                throw 'Roster save-stage app checkpoint uses an unknown schema.'
            }
            $stage = [string](Get-P917OptionalProperty $event 'stage')
            $stageToCheckpoint = [ordered]@{
                'before-snapshot' = 'roster-save-before-snapshot'
                'after-snapshot' = 'roster-save-after-snapshot'
                'after-preparation' = 'roster-save-after-preparation'
                'before-row-source-setup' = 'roster-save-before-row-source-setup'
                'after-row-source-setup' = 'roster-save-after-row-source-setup'
                'before-row-wise-repository-write' = 'roster-save-before-row-wise-repository-write'
                'after-row-wise-repository-write' = 'roster-save-after-row-wise-repository-write'
            }
            if (-not $stageToCheckpoint.Contains($stage)) { throw "Unknown roster save-stage checkpoint '$stage'." }
            $checkpoint = [string]$stageToCheckpoint[$stage]
            $expectedOutcome = if ($stage.StartsWith('before-', [StringComparison]::Ordinal)) { 'observed' } else { 'success' }
            if ([string](Get-P917OptionalProperty $event 'outcome') -cne $expectedOutcome) {
                throw "Roster save-stage checkpoint '$stage' did not report expected outcome '$expectedOutcome'."
            }
            if ([string](Get-P917OptionalProperty $event 'platform') -cne 'windows' -or
                (Get-P917OptionalProperty $event 'sample_available') -isnot [bool]) {
                throw "Roster save-stage checkpoint '$stage' does not identify a Windows process-memory sample."
            }
            if (-not [bool](Get-P917OptionalProperty $event 'sample_available')) {
                $allProcessMemorySamplesAvailable = $false
            }
            $capturedAt = [string](Get-P917OptionalProperty $event 'captured_at')
            try { [void][DateTimeOffset]::Parse($capturedAt) }
            catch { throw "Roster save-stage checkpoint '$stage' has an invalid process-memory capture time." }
            $memory = Get-P917OptionalProperty $event 'memory'
            $memoryCounterNames = @(
                'working_set_bytes', 'peak_working_set_bytes', 'private_usage_bytes',
                'pagefile_usage_bytes', 'handle_count', 'thread_count',
                'private_working_set_bytes'
            )
            if ($null -eq $memory) { throw "Roster save-stage checkpoint '$stage' has no process-memory counters." }
            $observedMemoryCounterNames = @($memory.PSObject.Properties.Name | Sort-Object)
            $expectedMemoryCounterNames = @($memoryCounterNames | Sort-Object)
            if ((ConvertTo-Json -InputObject $observedMemoryCounterNames -Compress) -cne
                (ConvertTo-Json -InputObject $expectedMemoryCounterNames -Compress)) {
                throw "Roster save-stage checkpoint '$stage' process-memory counter names differ from the Windows snapshot contract."
            }
            foreach ($counterName in $memoryCounterNames) {
                $counter = Get-P917OptionalProperty $memory $counterName
                if ($null -ne $counter -and -not (Test-P917JsonInteger -Value $counter)) {
                    throw "Roster save-stage checkpoint '$stage' has an invalid process-memory counter '$counterName'."
                }
            }
            $saveSequence = Get-P917OptionalProperty $event 'save_sequence'
            if (-not (Test-P917JsonInteger -Value $saveSequence) -or [Int64]$saveSequence -le 0) {
                throw "Roster save-stage checkpoint '$stage' has no positive save identity."
            }
            foreach ($forbiddenField in @(
                'text', 'cellText', 'cell_text', 'value', 'valueAfter',
                'expectedBeforeValue', 'details', 'roster', 'student', 'header', 'name'
            )) {
                if ($null -ne (Get-P917OptionalProperty $event $forbiddenField)) {
                    throw "Roster save-stage checkpoint '$stage' contains prohibited roster data field '$forbiddenField'."
                }
            }
        } else {
            if ([string](Get-P917OptionalProperty $event 'schema') -cne 'classmngr-default-process-heap-diagnostic-v1') {
                throw 'Default process heap app checkpoint uses an unknown schema.'
            }
            $checkpoint = [string](Get-P917OptionalProperty $event 'checkpoint')
        }
        if ($validCheckpoints -cnotcontains $checkpoint) { throw "Unknown default process heap checkpoint '$checkpoint'." }
        $stageEventCounts[$checkpoint] = [Int64]$stageEventCounts[$checkpoint] + 1
        if (-not $isSaveStage -and [string](Get-P917OptionalProperty $event 'outcome') -cne 'success' -and $checkpoint -cne 'classes-page-baseline-before-roster') {
            throw "Default process heap checkpoint '$checkpoint' did not report success."
        }
        $heap = Get-P917OptionalProperty $event 'default_process_heap'
        if ([string](Get-P917OptionalProperty $heap 'scope') -cne 'default-process-heap') { throw "Checkpoint '$checkpoint' does not identify the default-process-heap scope." }
        $available = Get-P917OptionalProperty $heap 'available'
        if ($available -isnot [bool]) { throw "Checkpoint '$checkpoint' does not identify heap-summary availability as a JSON Boolean." }
        $errorValue = Get-P917OptionalProperty $heap 'error'
        $counterValues = @(
            (Get-P917OptionalProperty $heap 'cbAllocated'),
            (Get-P917OptionalProperty $heap 'cbCommitted'),
            (Get-P917OptionalProperty $heap 'cbReserved'),
            (Get-P917OptionalProperty $heap 'cbMaxReserve')
        )
        $duration = Get-P917OptionalProperty $heap 'callDurationNanoseconds'
        if ($available) {
            if ($null -ne $errorValue -or @($counterValues | Where-Object { -not (Test-P917JsonInteger -Value $_) }).Count -ne 0 -or
                -not (Test-P917JsonInteger -Value $duration)) {
                throw "Available HeapSummary checkpoint '$checkpoint' is missing exact counters or call duration."
            }
        } else {
            $allHeapSamplesAvailable = $false
            if ([string]::IsNullOrWhiteSpace([string]$errorValue) -or
                @($counterValues | Where-Object { $null -ne $_ }).Count -ne 0 -or
                ($null -ne $duration -and -not (Test-P917JsonInteger -Value $duration))) {
                throw "Unavailable HeapSummary checkpoint '$checkpoint' must include an error and null counters, never zero substitutes."
            }
        }

        $instanceId = Get-P917OptionalProperty $event 'roster_widget_instance_id'
        if ($checkpoint -ceq 'classes-page-baseline-before-roster') {
            $baselineCount++
            if ($eventIndex -ne 0 -or $null -ne $instanceId -or
                [string](Get-P917OptionalProperty $event 'outcome') -cne 'observed' -or
                $null -ne (Get-P917OptionalProperty $event 'row') -or
                $null -ne (Get-P917OptionalProperty $event 'column')) {
                throw 'Classes-page baseline must be the first app checkpoint and have no roster widget identity or cell coordinates.'
            }
            continue
        }
        if (-not (Test-P917JsonInteger -Value $instanceId) -or [Int64]$instanceId -le 0) {
            throw "Roster checkpoint '$checkpoint' does not identify a positive integer widget instance."
        }
        foreach ($forbiddenField in @('text', 'cellText', 'cell_text', 'value', 'valueAfter', 'expectedBeforeValue', 'details')) {
            if ($null -ne (Get-P917OptionalProperty $event $forbiddenField)) {
                throw "Roster checkpoint '$checkpoint' contains prohibited cell text or extra details '$forbiddenField'."
            }
        }
        $row = Get-P917OptionalProperty $event 'row'
        $column = Get-P917OptionalProperty $event 'column'
        if ($checkpoint -ceq 'roster-cell-data-changed') {
            if (-not (Test-P917JsonInteger -Value $row) -or -not (Test-P917JsonInteger -Value $column) -or
                [Int64]$row -lt 0 -or [Int64]$column -lt 0) {
                throw 'Successful cell dataChanged event is missing nonnegative zero-based row and column coordinates.'
            }
        } elseif ($null -ne $row -or $null -ne $column) {
            throw "Non-cell checkpoint '$checkpoint' unexpectedly contains row or column coordinates."
        }

        $key = [string][Int64]$instanceId
        if (-not $groupsById.ContainsKey($key)) {
            if ($checkpoint -cne 'roster-model-loaded') { throw "Roster widget $key emitted '$checkpoint' before its loaded checkpoint." }
            $group = [ordered]@{
                instanceId = [Int64]$instanceId
                loadedSequence = [Int64]$sequence
                checkpointEvents = [Collections.Generic.List[string]]::new()
                cells = [Collections.Generic.List[object]]::new()
                saveStageEvents = [Collections.Generic.List[object]]::new()
                saveDiagnosticSequence = $null
                saveCount = 0
                destroyedSequence = $null
                afterDeleteSequence = $null
            }
            $groupsById.Add($key, $group)
            $groups.Add($group)
        } else {
            $group = $groupsById[$key]
            if ($null -ne $group.afterDeleteSequence) { throw "Roster widget $key emitted a checkpoint after its after-delete event." }
            if ($null -ne $group.destroyedSequence -and
                $checkpoint -cne 'roster-widget-after-delete-returned-after-child-destruction') {
                throw "Roster widget $key emitted a checkpoint between its destructor and after-delete events."
            }
            if ($checkpoint -ceq 'roster-model-loaded') { throw "Roster widget $key emitted more than one loaded checkpoint." }
        }
        $group.checkpointEvents.Add($checkpoint)
        if ($isSaveStage) {
            if ($null -ne $group.saveDiagnosticSequence -and
                [Int64]$group.saveDiagnosticSequence -ne [Int64]$saveSequence) {
                throw "Roster widget $key changed save identity during one save operation."
            }
            $group.saveDiagnosticSequence = [Int64]$saveSequence
            $group.saveStageEvents.Add([ordered]@{
                stage = [string](Get-P917OptionalProperty $event 'stage')
                eventSequence = [Int64]$sequence
                saveSequence = [Int64]$saveSequence
                outcome = [string](Get-P917OptionalProperty $event 'outcome')
            })
        }
        switch ($checkpoint) {
            'roster-cell-data-changed' {
                $group.cells.Add([ordered]@{ eventSequence = [Int64]$sequence; row = [Int64]$row; column = [Int64]$column })
            }
            'roster-save-returned-successfully' { $group.saveCount++ }
            'roster-widget-destroyed-after-successful-leave' { $group.destroyedSequence = [Int64]$sequence }
            'roster-widget-after-delete-returned-after-child-destruction' {
                if ($null -eq $group.destroyedSequence -or $null -ne $group.afterDeleteSequence) {
                    throw "Roster widget $key has an after-delete event without exactly one prior destructor event."
                }
                if ([Int64]$sequence -ne ([Int64]$group.destroyedSequence + 1)) {
                    throw "Roster widget $key after-delete event is not immediately ordered after its destructor event."
                }
                $group.afterDeleteSequence = [Int64]$sequence
            }
        }
    }
    if ($baselineCount -ne 1) { throw "Expected exactly one classes-page baseline checkpoint, observed $baselineCount." }
    $incompleteTeardownGroups = @($groups | Where-Object {
        $null -ne $_.destroyedSequence -and $null -eq $_.afterDeleteSequence
    })
    if ($incompleteTeardownGroups.Count -ne 0) {
        throw 'Every successful-leave destructor event must be followed by its owner after-delete event.'
    }
    if ([Int64]$stageEventCounts['roster-widget-destroyed-after-successful-leave'] -ne
        [Int64]$stageEventCounts['roster-widget-after-delete-returned-after-child-destruction']) {
        throw 'Successful-leave destructor and after-delete event counts differ.'
    }
    $countedEventTotal = [Int64]0
    foreach ($checkpointName in $validCheckpoints) {
        $countedEventTotal += [Int64]$stageEventCounts[$checkpointName]
    }
    if ($countedEventTotal -ne $events.Count) {
        throw 'Checkpoint stage event counts do not sum to the observed event count.'
    }
    $openGroups = @($groups | Where-Object { $null -eq $_.afterDeleteSequence })
    if ($openGroups.Count -ne 1 -or $openGroups[0].instanceId -ne $groups[$groups.Count - 1].instanceId) {
        throw 'Checkpoint stream must end with exactly one still-open final Roster widget instance.'
    }

    $cycleRecords = @($Manifest.route.cycleRecords)
    if ($cycleRecords.Count -ne 5 -or [Int64]$Manifest.route.measuredCycleCount -ne 5) {
        throw 'Default process heap diagnostic requires the existing five measured roster cycles.'
    }
    $completeMeasuredGroups = @($groups | Where-Object {
        $_.cells.Count -eq 3 -and $_.saveCount -eq 1 -and
            $_.saveStageEvents.Count -eq 7 -and
            $null -ne $_.destroyedSequence -and $null -ne $_.afterDeleteSequence
    })
    if ($completeMeasuredGroups.Count -lt 5) {
        throw "Expected five complete three-cell edit/save/successful-leave widget cycles, observed $($completeMeasuredGroups.Count)."
    }
    $measuredGroups = @($completeMeasuredGroups | Select-Object -Last 5)
    $measuredInstances = [Collections.Generic.List[object]]::new()
    for ($cycleIndex = 0; $cycleIndex -lt 5; $cycleIndex++) {
        $cycle = $cycleRecords[$cycleIndex]
        $expectedReadbacks = @($cycle.editedReadbacks)
        $observedCells = @($measuredGroups[$cycleIndex].cells)
        $expectedCheckpointEvents = @(
            'roster-model-loaded',
            'roster-cell-data-changed',
            'roster-cell-data-changed',
            'roster-cell-data-changed',
            'roster-save-before-snapshot',
            'roster-save-after-snapshot',
            'roster-save-after-preparation',
            'roster-save-before-row-source-setup',
            'roster-save-after-row-source-setup',
            'roster-save-before-row-wise-repository-write',
            'roster-save-after-row-wise-repository-write',
            'roster-save-returned-successfully',
            'roster-widget-destroyed-after-successful-leave',
            'roster-widget-after-delete-returned-after-child-destruction'
        )
        $observedCheckpointEvents = @($measuredGroups[$cycleIndex].checkpointEvents)
        if ($observedCheckpointEvents.Count -ne $expectedCheckpointEvents.Count) {
            throw "Measured cycle $($cycleIndex + 1) does not have exactly the required ordered fourteen-stage app checkpoint sequence."
        }
        for ($stageIndex = 0; $stageIndex -lt $expectedCheckpointEvents.Count; $stageIndex++) {
            if ([string]$observedCheckpointEvents[$stageIndex] -cne [string]$expectedCheckpointEvents[$stageIndex]) {
                throw "Measured cycle $($cycleIndex + 1) app checkpoint stages are missing, duplicated, or reordered."
            }
        }
        if ($expectedReadbacks.Count -ne 3 -or $observedCells.Count -ne 3) {
            throw "Measured cycle $($cycleIndex + 1) does not have exactly three app cell events and three UIA readbacks."
        }
        $expectedSaveStages = @(
            'before-snapshot', 'after-snapshot', 'after-preparation',
            'before-row-source-setup', 'after-row-source-setup',
            'before-row-wise-repository-write', 'after-row-wise-repository-write'
        )
        $observedSaveStages = @($measuredGroups[$cycleIndex].saveStageEvents)
        if ($observedSaveStages.Count -ne $expectedSaveStages.Count -or
            [Int64]$measuredGroups[$cycleIndex].saveDiagnosticSequence -le 0) {
            throw "Measured cycle $($cycleIndex + 1) does not have one identified seven-boundary save sequence."
        }
        for ($saveStageIndex = 0; $saveStageIndex -lt $expectedSaveStages.Count; $saveStageIndex++) {
            if ([string]$observedSaveStages[$saveStageIndex].stage -cne [string]$expectedSaveStages[$saveStageIndex] -or
                [Int64]$observedSaveStages[$saveStageIndex].saveSequence -ne [Int64]$measuredGroups[$cycleIndex].saveDiagnosticSequence -or
                [string]$observedSaveStages[$saveStageIndex].outcome -cne $(if ($expectedSaveStages[$saveStageIndex].StartsWith('before-', [StringComparison]::Ordinal)) { 'observed' } else { 'success' })) {
                throw "Measured cycle $($cycleIndex + 1) save-stage events are missing, duplicated, reordered, or misattributed."
            }
        }
        for ($cellIndex = 0; $cellIndex -lt 3; $cellIndex++) {
            if ([Int64]$observedCells[$cellIndex].row -ne [Int64]$expectedReadbacks[$cellIndex].row -or
                [Int64]$observedCells[$cellIndex].column -ne [Int64]$expectedReadbacks[$cellIndex].modelColumn) {
                throw "Measured cycle $($cycleIndex + 1) app cell checkpoint coordinates differ from the preserved UIA edit/readback sequence."
            }
        }
        $measuredInstances.Add([ordered]@{
            cycleNumber = [Int64]$cycle.cycleNumber
            rosterWidgetInstanceId = [Int64]$measuredGroups[$cycleIndex].instanceId
            saveSequence = [Int64]$measuredGroups[$cycleIndex].saveDiagnosticSequence
            saveStageEventSequences = @($observedSaveStages | ForEach-Object { [Int64]$_.eventSequence })
            loadedEventSequence = [Int64]$measuredGroups[$cycleIndex].loadedSequence
            cellEventSequences = @($observedCells | ForEach-Object { [Int64]$_.eventSequence })
            destroyedEventSequence = [Int64]$measuredGroups[$cycleIndex].destroyedSequence
            afterDeleteEventSequence = [Int64]$measuredGroups[$cycleIndex].afterDeleteSequence
        })
    }

    $artifact = [ordered]@{
        schema = 'classmngr-default-process-heap-checkpoints-v3'
        processId = [Int64]$Context.process.Id
        eventCount = $events.Count
        checkpointEventCounts = $stageEventCounts
        events = $events.ToArray()
    }
    $artifactPath = Join-Path $EvidenceDirectory 'default-process-heap-checkpoints.json'
    Write-P914Json $artifactPath $artifact
    $Manifest.defaultProcessHeapDiagnostic.events = $events.ToArray()
    $Manifest.defaultProcessHeapDiagnostic.validation = [ordered]@{
        status = 'passed'
        processId = [Int64]$Context.process.Id
        eventCount = $events.Count
        eventSequenceContiguous = $true
        checkpointStageNames = $validCheckpoints
        checkpointEventCounts = $stageEventCounts
        classesPageBaselineCount = $baselineCount
        rosterWidgetInstanceCount = $groups.Count
        measuredCycleCount = $measuredInstances.Count
        measuredCycleInstances = $measuredInstances.ToArray()
        cycleAttribution = 'ordered-app-event-sequence-roster-widget-instance-id-and-save-sequence'
        exactHeapSampleFieldsValidated = $true
        unavailableSamplesPreserveNullCounters = $true
        allSamplesAvailable = $allHeapSamplesAvailable
        processMemoryCounterFieldsValidated = $true
        allProcessMemorySamplesAvailable = $allProcessMemorySamplesAvailable
        exactAppOutputRetained = $true
        appOutputRelativePath = 'application-output.jsonl'
        artifactRelativePath = 'default-process-heap-checkpoints.json'
        artifactSha256 = Get-P914FileSha256 $artifactPath
    }
}

function Get-P917VisibleDialogHandles([object]$Context) {
    $values = [Collections.Generic.List[Int64]]::new()
    foreach ($handle in $Context.dialogWindowHandles) { if (-not $values.Contains([Int64]$handle)) { $values.Add([Int64]$handle) } }
    return $values.ToArray()
}

function Invoke-P917MeasuredRosterCycles([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [object[]]$Targets, [int]$TimeoutSeconds, [int]$MeasuredCycleCount = 10, [ValidateSet('full-observer', 'reduced-observer')][string]$ObserverMode = 'full-observer', [switch]$DiagnosticOnly, [ValidateSet('no-gc', 'client-gc', '')][string]$ClientGcReferenceMode = '', [switch]$PayloadDiagnosticOnly, [switch]$RecordCountDiagnosticOnly, [object]$HeapDiagnostic = $null) {
    $route = $Manifest.route
    $cycleCount = $MeasuredCycleCount
    $baselineCount = 5
    if ($DiagnosticOnly -and $cycleCount -ne 5) { throw 'UIA observer diagnostic mode requires exactly five measured cycles.' }
    if (-not $DiagnosticOnly -and ($cycleCount -ne 10 -or $ObserverMode -cne 'full-observer')) { throw 'P9-17 acceptance mode requires ten measured cycles with the full observer.' }
    if ($null -ne $HeapDiagnostic -and ($DiagnosticOnly -or $cycleCount -ne 10 -or $ObserverMode -cne 'full-observer')) { throw 'Heap snapshot diagnostics require the unmodified full-observer ten-cycle route.' }
    if (-not [string]::IsNullOrEmpty($ClientGcReferenceMode) -and (-not $DiagnosticOnly -or $ObserverMode -cne 'full-observer' -or $cycleCount -ne 5)) {
        throw 'Client GC reference arms require diagnostic-only full-observer mode with exactly five measured cycles.'
    }
    $route.primingCycleCount = 1
    $route.baselineWindowsPerState = $baselineCount
    $route.measuredCycleCount = $cycleCount
    $route.observerMode = $ObserverMode
    $route.observerDiagnosticOnly = [bool]$DiagnosticOnly
    $route.cycleEvidenceSchema = 'classmngr-phase9-p9-17-repeated-roster-cycle-v1'
    $route.recordCountDiagnosticOnly = [bool]$RecordCountDiagnosticOnly
    $route.repeatedCycleProtocol = if ($DiagnosticOnly) { "Diagnostic-only $ObserverMode comparison: one priming edit/save/clear/re-entry precedes five no-action matched baselines and five same-process UIA edit/save/suspend cycles; final full UIA verification follows." } elseif ($PayloadDiagnosticOnly) { 'Diagnostic-only full-observer payload comparison: one priming edit/save/clear/re-entry precedes five no-action matched baselines and ten same-process UIA edit/save/suspend cycles. Each measured cycle samples roster-loaded, post-save, and post-suspension return states.' } elseif ($RecordCountDiagnosticOnly) { 'Diagnostic-only full-observer custom-record cardinality comparison: one priming edit/save/clear/re-entry precedes five no-action matched baselines and ten same-process UIA edit/save/suspend cycles. Each measured cycle samples roster-loaded, post-save, and post-suspension return states.' } else { 'One priming edit/save/clear/re-entry precedes five no-action matched baselines and ten same-process UIA edit/save/suspend cycles. Each measured cycle samples roster-loaded, post-save, and post-suspension return states.' }
    $route.cycleRecords = [Collections.Generic.List[object]]::new()
    $Manifest.memory.plateauBaselineWindowIds = [Collections.Generic.List[string]]::new()
    $Manifest.memory.plateauCycleWindowIds = [Collections.Generic.List[string]]::new()
    $observerDiagnostic = if ($DiagnosticOnly) { $Manifest.observerDiagnostic } else { $null }
    if ($DiagnosticOnly) {
        $observerDiagnostic.measuredCycleObservations = [Collections.Generic.List[object]]::new()
        $observerDiagnostic.measuredCycleObserverTotals = [ordered]@{
            cycleCount = $cycleCount; expectedHeadersPerFullObserverCycle = 468; expectedHeadersPerReducedObserverCycle = 156
            fullObserverContractHeaderElements = 0; reducedObserverContractHeaderElements = 0
            initialTargetReadbacks = 0; immediateSetValueReadbacks = 0; editedPhaseTargetRequeries = 0; postSaveTargetReadbacks = 0
            finalFullVerificationHeaderElements = 0; finalFullVerificationTargetReadbacks = 0
        }
    }

    $detailsTab = Find-P917DetailsTab $Context
    if (Test-P917TabSelected $detailsTab) {
        throw 'Post-priming roster-absence checkpoint must begin from the Roster section so a resolved section-exit action is measured.'
    }
    $baselineLeaveAction = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory 'cycle-baseline-select-class-details' $detailsTab 'Class Details and Times'
    Start-Sleep -Milliseconds 500
    $route.postPrimingClearCheckpoint = Get-P917SectionRosterAbsenceCheckpoint $Context $Manifest $EvidenceDirectory 'post-priming-class-details' $baselineLeaveAction
    $route.postPrimingClearCheckpoint.evidenceRole = 'post-priming plateau baseline; initial cold-baseline uplift remains separately reported'
    $postPrimingScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'post-priming-class-details' $Context.mainWindowRoot
    $route.postPrimingClearCheckpoint.screenshotRelativePath = [string]$postPrimingScreenshot.relativePath
    $route.postPrimingClearCheckpoint.screenshotCapturedAtUtc = [string]$postPrimingScreenshot.capturedAtUtc

    for ($baselineIndex = 1; $baselineIndex -le $baselineCount; $baselineIndex++) {
        Start-Sleep -Seconds 2
        $windowId = 'plateau-baseline-{0:d2}' -f $baselineIndex
        Set-P917SamplingContext $Context 'normal-use' $windowId
        $window = Get-P917MemoryWindow $Context $Manifest $windowId 'classes-details-selected' 'baseline' 10 100 'classes-details-selected-class'
        $window.baselineIndex = $baselineIndex
        $window.settleDelaySeconds = 2
        $window.measurementRole = 'post-priming-no-action-baseline'
        $Manifest.memory.plateauBaselineWindowIds.Add($windowId)
        $detailsTab = Find-P917DetailsTab $Context
        if (-not (Test-P917TabSelected $detailsTab)) { throw "Matched plateau baseline $baselineIndex left the Class Details section." }
    }
    if ($null -ne $HeapDiagnostic) {
        Add-P917HeapSnapshot $HeapDiagnostic 'post-primer-plateau-baseline' $window ([ordered]@{
            nextUiActionKind = 'cycle-01-open-roster'; routeState = 'post-primer-plateau-baseline-complete'
        })
    }
    $detailsTab = $null
    $Context.mainWindowRoot = $null
    $Context.dialogRoot = $null

    for ($cycleNumber = 1; $cycleNumber -le $cycleCount; $cycleNumber++) {
        $cycleTag = '{0:d2}' -f $cycleNumber
        $cycleTargets = @(Get-P917CycleTargets $Targets $cycleNumber)
        $rosterTab = Find-P917RosterTab $Context
        $null = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory "cycle-$cycleTag-open-roster" $rosterTab 'Roster'
        $rosterTab = $null
        $table = Get-P917RosterTable $Context $EvidenceDirectory $TimeoutSeconds
        Start-Sleep -Seconds 2

        $beforeReadbacks = Read-P917Targets $Context $Manifest $EvidenceDirectory $cycleTargets 'beforeValue'
        $loadedContract = Save-P917RosterContract $Context $Manifest $EvidenceDirectory "cycle-$cycleTag-roster-loaded-contract" $table $beforeReadbacks
        $null = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory "cycle-$cycleTag-roster-loaded" $Context.mainWindowRoot
        $gridMetrics = Get-P917GridMetrics $table
        if ($gridMetrics.rowCount -ne 100 -or $gridMetrics.columnCount -ne 156 -or $loadedContract.record.headers.Count -ne 156) {
            throw "Measured cycle $cycleNumber did not restore the authentic 100x156 roster grid with all headers."
        }
        $cycleGridRows = [int]$gridMetrics.rowCount
        $cycleGridColumns = [int]$gridMetrics.columnCount
        $cycleVerifiedHeaderCount = [int]$loadedContract.record.headers.Count
        Set-P917SamplingContext $Context 'normal-use' "cycle-$cycleTag-roster-loaded"
        $loadedWindowId = "cycle-$cycleTag-roster-loaded"
        $loadedWindow = Get-P917MemoryWindow $Context $Manifest $loadedWindowId 'roster-loaded' 'cycle-loaded' 10 100
        $loadedWindow.cycleNumber = $cycleNumber
        $loadedWindow.settleDelaySeconds = 2
        $loadedWindow.measurementRole = 'measured-cycle-roster-loaded'
        $Manifest.memory.plateauCycleWindowIds.Add($loadedWindowId)

        Set-P917SamplingContext $Context 'operation' "cycle-$cycleTag-roster-edit"
        $editActions = [Collections.Generic.List[object]]::new()
        foreach ($target in $cycleTargets) { $editActions.Add((Set-P917RosterCell $Context $Manifest $EvidenceDirectory $target -RequireValuePattern:$DiagnosticOnly)) }
        $editedContract = $null
        $editedContractRelativePath = $null
        if ($ObserverMode -ceq 'reduced-observer') {
            $editedReadbacks = New-P917ImmediateSetValueReadbacks $cycleTargets $editActions.ToArray()
        } else {
            $editedReadbacks = Read-P917Targets $Context $Manifest $EvidenceDirectory $cycleTargets 'afterValue'
            $editedContract = Save-P917RosterContract $Context $Manifest $EvidenceDirectory "cycle-$cycleTag-edited-contract" (Find-P917RosterTable $Context) $editedReadbacks
            $editedContractRelativePath = [string]$editedContract.manifestRecord.relativePath
        }
        $null = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory "cycle-$cycleTag-edited" $Context.mainWindowRoot

        Set-P917SamplingContext $Context 'operation' "cycle-$cycleTag-roster-save"
        $saveAction = Save-P917Roster $Context $Manifest $EvidenceDirectory $TimeoutSeconds
        $saveAction.cycleNumber = $cycleNumber
        $saveAction.measurementRole = 'measured-cycle-save'
        $savedReadbacks = Read-P917Targets $Context $Manifest $EvidenceDirectory $cycleTargets 'afterValue'
        $savedContract = $null
        $savedContractRelativePath = $null
        if ($ObserverMode -ceq 'full-observer') {
            $savedContract = Save-P917RosterContract $Context $Manifest $EvidenceDirectory "cycle-$cycleTag-post-save-contract" (Find-P917RosterTable $Context) $savedReadbacks
            $savedContractRelativePath = [string]$savedContract.manifestRecord.relativePath
        }
        $null = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory "cycle-$cycleTag-post-save" $Context.mainWindowRoot
        Start-Sleep -Seconds 2
        Set-P917SamplingContext $Context 'normal-use' "cycle-$cycleTag-post-save"
        $postSaveWindowId = "cycle-$cycleTag-post-save"
        $postSaveWindow = Get-P917MemoryWindow $Context $Manifest $postSaveWindowId 'roster-saved' 'cycle-post-save' 10 100
        $postSaveWindow.cycleNumber = $cycleNumber
        $postSaveWindow.settleDelaySeconds = 2
        $postSaveWindow.measurementRole = 'measured-cycle-post-save'
        $Manifest.memory.plateauCycleWindowIds.Add($postSaveWindowId)

        $detailsTab = $null
        $table = $null
        $gridMetrics = $null
        $Context.mainWindowRoot = $null
        $Context.dialogRoot = $null
        $sectionExit = Invoke-P917MeasuredCycleSectionExit $Context $Manifest $EvidenceDirectory $cycleTag
        $returnCheckpoint = $sectionExit.checkpoint
        $returnCheckpoint.cycleNumber = $cycleNumber
        $sectionExit = $null
        $Context.mainWindowRoot = $null
        $Context.dialogRoot = $null
        $clientGcEvent = $null
        if (-not [string]::IsNullOrEmpty($ClientGcReferenceMode)) {
            $clientGcEvent = Invoke-P917ClientGcReferenceEvent $Context $Manifest.clientGcReferenceDiagnostic $returnCheckpoint $cycleNumber "cycle-$cycleTag-return" $ClientGcReferenceMode
        }
        Start-Sleep -Seconds 2
        $detailsTab = Find-P917DetailsTab $Context
        if (-not (Test-P917TabSelected $detailsTab)) { throw "Measured cycle $cycleNumber return window is not matched to Class Details and Times." }
        $detailsTab = $null
        $Context.mainWindowRoot = $null
        $Context.dialogRoot = $null
        Set-P917SamplingContext $Context 'normal-use' "cycle-$cycleTag-return"
        $returnWindowId = "cycle-$cycleTag-return"
        $returnWindow = Get-P917MemoryWindow $Context $Manifest $returnWindowId 'classes-details-selected-after-roster-clear' 'cycle' 10 100 'classes-details-selected-class'
        $returnWindow.cycleNumber = $cycleNumber
        $returnWindow.settleDelaySeconds = 2
        $returnWindow.measurementRole = 'measured-cycle-post-suspension-return'
        $Manifest.memory.plateauCycleWindowIds.Add($returnWindowId)
        if ($null -ne $HeapDiagnostic -and $cycleNumber -in @(1, 10)) {
            $nextUiActionKind = if ($cycleNumber -eq 1) { 'cycle-02-open-roster' } else { 'cycle-final-reenter-roster' }
            Add-P917HeapSnapshot $HeapDiagnostic "cycle-$cycleTag-return" $returnWindow ([ordered]@{
                nextUiActionKind = $nextUiActionKind; routeState = "measured-cycle-$cycleTag-return-complete"
                cycleNumber = $cycleNumber; suspensionCheckpoint = $returnCheckpoint
            })
        }

        $Manifest.route.cycleRecords.Add([ordered]@{
            schema = 'classmngr-phase9-p9-17-repeated-roster-cycle-v1'
            cycleNumber = $cycleNumber
            targetClassId = 1
            targetClassName = [string]$route.matchedMemoryState.className
            selectedClassAccessibleName = [string]$route.matchedMemoryState.selectedClassAccessibleName
            classSelectionVerified = [bool]$route.targetClassSelection.selected
            parity = if ($cycleNumber % 2) { 'after-to-before' } else { 'before-to-after' }
            rosterLoadedWindowId = $loadedWindowId
            postSaveWindowId = $postSaveWindowId
            postSuspensionWindowId = $returnWindowId
            initialReadbacks = $beforeReadbacks
            editedReadbacks = $editedReadbacks
            savedReadbacks = $savedReadbacks
            loadedContractRelativePath = [string]$loadedContract.manifestRecord.relativePath
            editedContractRelativePath = $editedContractRelativePath
            savedContractRelativePath = $savedContractRelativePath
            gridRows = $cycleGridRows
            gridColumns = $cycleGridColumns
            verifiedHeaderCount = $cycleVerifiedHeaderCount
            saveActionIndex = [int]($Manifest.route.actions.IndexOf($saveAction))
            suspensionCheckpoint = $returnCheckpoint
        })
        if ($DiagnosticOnly) {
            $headersForCycle = if ($ObserverMode -ceq 'full-observer') { 468 } else { 156 }
            if ($ObserverMode -ceq 'full-observer') { $observerDiagnostic.measuredCycleObserverTotals.fullObserverContractHeaderElements += $headersForCycle }
            else { $observerDiagnostic.measuredCycleObserverTotals.reducedObserverContractHeaderElements += $headersForCycle }
            $observerDiagnostic.measuredCycleObserverTotals.initialTargetReadbacks += $beforeReadbacks.Count
            $observerDiagnostic.measuredCycleObserverTotals.immediateSetValueReadbacks += $editActions.Count
            $observerDiagnostic.measuredCycleObserverTotals.editedPhaseTargetRequeries += if ($ObserverMode -ceq 'full-observer') { $editedReadbacks.Count } else { 0 }
            $observerDiagnostic.measuredCycleObserverTotals.postSaveTargetReadbacks += $savedReadbacks.Count
            $observerDiagnostic.measuredCycleObservations.Add([ordered]@{
                cycleNumber = $cycleNumber; mode = $ObserverMode; headerEnumerationCount = if ($ObserverMode -ceq 'full-observer') { 3 } else { 1 }
                headerElementCount = $headersForCycle; initialTargetReadbacks = $beforeReadbacks.Count
                immediateSetValueReadbacks = $editActions.Count
                editedPhaseTargetRequeries = if ($ObserverMode -ceq 'full-observer') { $editedReadbacks.Count } else { 0 }
                postSaveTargetReadbacks = $savedReadbacks.Count
                editedEvidenceSource = if ($ObserverMode -ceq 'reduced-observer') { 'immediate ValuePattern.SetValue readbacks' } else { 'full GridPattern target reads and roster contract' }
                loadedContractRelativePath = [string]$loadedContract.manifestRecord.relativePath
                editedContractRelativePath = $editedContractRelativePath; savedContractRelativePath = $savedContractRelativePath
            })
        }
    }

    $rosterTab = Find-P917RosterTab $Context
    $null = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory 'cycle-final-reenter-roster' $rosterTab 'Roster'
    $finalTable = Get-P917RosterTable $Context $EvidenceDirectory $TimeoutSeconds
    Start-Sleep -Seconds 2
    $finalTargets = @(Get-P917CycleTargets $Targets $cycleCount)
    $finalReadbacks = Read-P917Targets $Context $Manifest $EvidenceDirectory $finalTargets 'afterValue'
    $finalContract = Save-P917RosterContract $Context $Manifest $EvidenceDirectory 'cycle-final-reentry-contract' $finalTable $finalReadbacks
    $null = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'cycle-final-reentry' $finalTable
    $Manifest.route.finalCycleReadback = $finalReadbacks
    $Manifest.route.finalCycleRosterContractRelativePath = [string]$finalContract.manifestRecord.relativePath
    $Manifest.route.cycleFinalReentryScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'cycle-final-reentry' $Context.mainWindowRoot
    Set-P917SamplingContext $Context 'normal-use' 'cycle-final-reentry'
    $finalWindowId = 'cycle-final-reentry'
    $finalWindow = Get-P917MemoryWindow $Context $Manifest $finalWindowId 'roster-reentered-persisted' 'final-reentry' 10 100
    $finalWindow.settleDelaySeconds = 2
    $finalWindow.measurementRole = 'post-cycle-final-reentry'
    $Manifest.memory.plateauCycleWindowIds.Add($finalWindowId)
    $Manifest.route.readBackChecks.cycles = $route.cycleRecords.ToArray()
    $Manifest.route.readBackChecks.afterCycles = $finalReadbacks
    if ($DiagnosticOnly) {
        $observerDiagnostic.measuredCycleObserverTotals.finalFullVerificationHeaderElements = [int]$finalContract.record.headers.Count
        $observerDiagnostic.measuredCycleObserverTotals.finalFullVerificationTargetReadbacks = $finalReadbacks.Count
        $observerDiagnostic.finalFullVerification = [ordered]@{
            contractRelativePath = [string]$finalContract.manifestRecord.relativePath
            verifiedHeaderCount = [int]$finalContract.record.headers.Count
            targetReadbackCount = $finalReadbacks.Count; targetReadbacks = $finalReadbacks
            verifiesLastCycleParity = $true
            finalValueDirection = if ($cycleCount % 2) { 'before-value' } else { 'after-value' }
        }
        $observerDiagnostic.cycleCallsAvoided = if ($ObserverMode -ceq 'reduced-observer') {
            [ordered]@{ headerElements = ($cycleCount * 312); editedPhaseTargetRequeries = ($cycleCount * $Targets.Count) }
        } else { [ordered]@{ headerElements = 0; editedPhaseTargetRequeries = 0 } }
    }
}

function Invoke-P917Route([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [object]$Fixture, [int]$TimeoutSeconds, [object]$HeapDiagnostic = $null, [string]$ObserverMode = 'full-observer', [switch]$DiagnosticOnly, [ValidateSet('no-gc', 'client-gc', '')][string]$ClientGcReferenceMode = '', [switch]$PayloadDiagnosticOnly, [switch]$RecordCountDiagnosticOnly) {
    $Context.workspacePath = [IO.Path]::GetFullPath([string]$Fixture.workspacePath)
    Start-Sleep -Seconds 2
    $null = Get-P917MemoryWindow $Context $Manifest 'startup-ready' 'initial-setup' 'startup' 10 100
    $open = Open-P917Workspace $Context $Manifest $EvidenceDirectory $Fixture.workspacePath $TimeoutSeconds
    $Manifest.route.workspaceOpen = $open
    Set-P917SamplingContext $Context 'normal-use' 'workspace-entry'
    Start-Sleep -Seconds 2
    $null = Get-P917MemoryWindow $Context $Manifest 'workspace-entry' 'my-workspace' 'entry' 10 100 'my-workspace-open-workspace'

    $classRouteArrival = [Int64]$Context.arrival
    $classRoute = Select-P914Route $Context $Manifest $EvidenceDirectory 'Classes' 'classes'
    $classActivation = Wait-P914Lifecycle $Context $EvidenceDirectory 'classes' 'activated' $classRouteArrival $TimeoutSeconds
    $Manifest.route.classPageActivation = Get-P914PageModelCounts $classActivation
    $Manifest.route.classesPageScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'classes-page-before-target-select' $Context.mainWindowRoot
    $classesPageTree = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'classes-page-before-target-select' $Context.mainWindowRoot
    $Manifest.route.classesPageUiaSnapshotRelativePath = [string]$classesPageTree.manifestRecord.relativePath
    $classList = Wait-P917Suffix $Context $EvidenceDirectory 'classesNavigatorClassList' $TimeoutSeconds 'main' 'ControlType.List'
    $targetClassName = [string]$Fixture.preparation.identity.targetClassName
    $targetAccessibleName = [string]$Fixture.preparation.identity.targetClassListAccessibleName
    if ([string]::IsNullOrWhiteSpace($targetAccessibleName)) { throw 'Fixture receipt does not derive the target class ListItem accessible name.' }
    $targetSearch = Find-P917TargetClassListItem $Context $Manifest $EvidenceDirectory $classList $targetAccessibleName
    $Manifest.route.targetClassSearch = [ordered]@{ className = $targetClassName; targetAccessibleName = $targetAccessibleName; selectionProperty = 'Exact UIA ListItem.Name derived from fixture class id 1 and Classes navigation data'; maximumScrolls = 40; pages = $targetSearch.search; matched = ($null -ne $targetSearch.element) }
    $listItem = $targetSearch.element
    if ($null -eq $listItem) { throw "Exact target class '$targetClassName' accessible name '$targetAccessibleName' was not exposed by the Classes ListItems after the bounded list search." }
    if ([string]$listItem.Current.Name -cne $targetAccessibleName) { throw 'The selected Classes ListItem no longer exposes the exact fixture-derived accessible name.' }
    $selectionAction = Invoke-P917BoundedClick $Context $Manifest $EvidenceDirectory 'select-target-class' $listItem ([Int64]$Context.mainWindowHandle) 'UIA-targeted exact class ListItem click clipped to visible Classes List bounds' $classList
    $selectedPattern = [System.Windows.Automation.SelectionItemPattern]$listItem.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
    $selectionAction.selectedClassName = $targetClassName
    $selectionAction.selectedListItemName = [string]$listItem.Current.Name
    $selectionAction.selectedClassId = 1
    $selectionWatch = [Diagnostics.Stopwatch]::StartNew()
    $selectionReadbackSamples = [Collections.Generic.List[object]]::new()
    while ($selectionWatch.Elapsed.TotalSeconds -lt 3) {
        $isSelected = [bool]$selectedPattern.Current.IsSelected
        $selectionReadbackSamples.Add([ordered]@{ capturedAtUtc = Get-P914Utc; isSelected = $isSelected })
        if ($isSelected) { break }
        Start-Sleep -Milliseconds 50
    }
    $selectionAction.selectionItemPatternIsSelected = [bool]$selectedPattern.Current.IsSelected
    $selectionAction.selectionReadbackSamples = $selectionReadbackSamples.ToArray()
    $selectionAction.selectionReadbackAtUtc = Get-P914Utc
    if (-not [bool]$selectionAction.selectionItemPatternIsSelected) { throw 'The exact target class list item is not selected after the bounded measured UIA click.' }
    $Manifest.route.targetClassSelection = [ordered]@{ classId = 1; className = $targetClassName; listItemName = [string]$listItem.Current.Name; expectedAccessibleName = $targetAccessibleName; selectionProperty = 'UIA ListItem.Name matched the exact fixture-derived summary for the unique prepared class 1 receipt'; selected = $true; selectedBy = 'UIA ListItem click with SelectionItemPattern readback'; actionIndex = [int]($Manifest.route.actions.IndexOf($selectionAction)) }
    Complete-P917Action $EvidenceDirectory $selectionAction
    $null = Wait-P917Suffix $Context $EvidenceDirectory 'selectedClassEditorSections' $TimeoutSeconds 'main' ''
    $detailsTab = Find-P917DetailsTab $Context
    if (-not (Test-P917TabSelected $detailsTab)) { $null = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory 'select-class-details-section' $detailsTab 'Class Details and Times' }
    $Manifest.route.classesPageAfterTargetSelectScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'classes-page-after-target-select' $Context.mainWindowRoot
    $selectedClassTree = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'classes-page-after-target-select' $Context.mainWindowRoot
    $Manifest.route.classesPageAfterTargetSelectUiaSnapshotRelativePath = [string]$selectedClassTree.manifestRecord.relativePath
    Start-Sleep -Seconds 2
    $null = Get-P917MemoryWindow $Context $Manifest 'class-page-entry' 'classes-details-selected' 'entry' 10 100
    $detailsTab = Find-P917DetailsTab $Context
    if (-not [bool]$selectedPattern.Current.IsSelected -or -not (Test-P917TabSelected $detailsTab)) { throw 'Matched baseline sampling requires the fixture target class and its non-Roster Class Information section to remain selected.' }
    $Manifest.route.matchedMemoryState = [ordered]@{
        state = 'classes-details-selected-class'; classId = 1; className = $targetClassName
        selectedClassAccessibleName = $targetAccessibleName; selectedSection = 'Class Details and Times'
        sectionSelectionVerified = [bool](Test-P917TabSelected $detailsTab)
    }
    $baseline = Get-P917MemoryWindow $Context $Manifest 'matched-baseline' 'classes-details-selected' 'baseline' 10 100 'classes-details-selected-class'
    $Manifest.memory.baselineWindowId = [string]$baseline.windowId
    $rosterTab = Find-P917RosterTab $Context
    $beforeEntryArrival = [Int64]$Context.arrival
    $null = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory 'open-roster-tab' $rosterTab 'Roster'
    $table = Get-P917RosterTable $Context $EvidenceDirectory $TimeoutSeconds
    $Manifest.route.rosterTabOpenedAtUtc = Get-P914Utc
    Start-Sleep -Seconds 2
    $targets = @($Fixture.preparation.expectedEditTargets)
    $initialReadbacks = Read-P917Targets $Context $Manifest $EvidenceDirectory $targets 'beforeValue'
    $initialContract = Save-P917RosterContract $Context $Manifest $EvidenceDirectory 'roster-initial-contract' $table $initialReadbacks
    $Manifest.route.initialRosterContractRelativePath = [string]$initialContract.manifestRecord.relativePath
    $Manifest.route.initialScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'roster-initial' $Context.mainWindowRoot
    $null = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'roster-initial-tree' $table
    $gridMetrics = Get-P917GridMetrics $table
    $Manifest.route.modelCounts = [ordered]@{
        rosterRows = $gridMetrics.rowCount; rosterColumns = $gridMetrics.columnCount; verifiedHeaderCount = [int]$initialContract.record.headers.Count
        lifecycleClassesActivationRootRows = if ($null -ne $classActivation.record.page.item_model_root_rows) { [Int64]$classActivation.record.page.item_model_root_rows } else { $null }
        lifecycleClassesActivationItemModelCount = if ($null -ne $classActivation.record.page.item_model_count) { [Int64]$classActivation.record.page.item_model_count } else { $null }
        lifecycleClassesActivationEditorControlCount = if ($null -ne $classActivation.record.page.editor_control_widget_count) { [Int64]$classActivation.record.page.editor_control_widget_count } else { $null }
        preSuspensionRootModelRows = $gridMetrics.rowCount
        preSuspensionViewIndexWidgetCount = if ($null -ne $classActivation.record.page.view_index_widget_count) { [Int64]$classActivation.record.page.view_index_widget_count } else { $null }
        preSuspensionEditorControlCount = if ($null -ne $classActivation.record.page.editor_control_widget_count) { [Int64]$classActivation.record.page.editor_control_widget_count } else { $null }
        suspendedRosterRootRows = $null; suspendedRosterColumnCount = $null; suspendedLifecycleRootRows = $null; suspendedLifecycleEditorControlCount = $null
        reentryRootModelRows = $null; reentryEditorControlCount = $null
    }
    Set-P917SamplingContext $Context 'normal-use' 'roster-entry'
    $null = Get-P917MemoryWindow $Context $Manifest 'roster-entry' 'roster-loaded' 'entry' 10 100

    Set-P917SamplingContext $Context 'operation' 'roster-edit'
    foreach ($target in $targets) { $null = Set-P917RosterCell $Context $Manifest $EvidenceDirectory $target -RequireValuePattern:$DiagnosticOnly }
    $editedReadbacks = Read-P917Targets $Context $Manifest $EvidenceDirectory $targets 'afterValue'
    $editedContract = Save-P917RosterContract $Context $Manifest $EvidenceDirectory 'roster-after-edit-contract' (Find-P917RosterTable $Context) $editedReadbacks
    $Manifest.route.editedRosterContractRelativePath = [string]$editedContract.manifestRecord.relativePath
    $Manifest.route.editedScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'roster-after-edit-before-save' $Context.mainWindowRoot
    Set-P917SamplingContext $Context 'normal-use' 'post-edit'
    $null = Get-P917MemoryWindow $Context $Manifest 'post-edit' 'roster-edited-dirty' 'edit' 10 100

    Set-P917SamplingContext $Context 'operation' 'roster-save'
    $saveAction = Save-P917Roster $Context $Manifest $EvidenceDirectory $TimeoutSeconds
    $Manifest.route.saveActionIndex = [int]($Manifest.route.actions.IndexOf($saveAction))
    $savedReadbacks = Read-P917Targets $Context $Manifest $EvidenceDirectory $targets 'afterValue'
    $savedContract = Save-P917RosterContract $Context $Manifest $EvidenceDirectory 'roster-after-save-contract' (Find-P917RosterTable $Context) $savedReadbacks
    $Manifest.route.savedRosterContractRelativePath = [string]$savedContract.manifestRecord.relativePath
    $Manifest.route.savedScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'roster-after-save' $Context.mainWindowRoot
    Start-Sleep -Seconds 2
    Set-P917SamplingContext $Context 'normal-use' 'post-save'
    $null = Get-P917MemoryWindow $Context $Manifest 'post-save' 'roster-saved' 'save' 10 100

    $detailsTab = Find-P917DetailsTab $Context
    $table = $null
    $leaveAction = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory 'leave-roster-section' $detailsTab 'Class Details and Times'
    Start-Sleep -Milliseconds 500
    $Manifest.route.sectionSuspensionCheckpoint = Get-P917SectionRosterAbsenceCheckpoint $Context $Manifest $EvidenceDirectory 'roster-section-suspended-tree' $leaveAction
    $Manifest.route.sectionSuspensionActionIndex = [int]$Manifest.route.sectionSuspensionCheckpoint.leaveActionIndex
    if ($null -ne $HeapDiagnostic) { $Manifest.route.sectionSuspensionCheckpoint.observedAtUtc = Get-P914Utc }
    $Manifest.route.suspensionScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'roster-section-suspended' $Context.mainWindowRoot
    $Manifest.route.sectionSuspensionCheckpoint.screenshotRelativePath = [string]$Manifest.route.suspensionScreenshot.relativePath
    $Manifest.route.sectionSuspensionCheckpoint.screenshotCapturedAtUtc = [string]$Manifest.route.suspensionScreenshot.capturedAtUtc
    Start-Sleep -Seconds 2
    $detailsTab = Find-P917DetailsTab $Context
    if (-not (Test-P917TabSelected $detailsTab)) { throw 'Matched post-suspension sampling requires the same non-Roster Class Information section used for baseline.' }
    $matchedReturn = Get-P917MemoryWindow $Context $Manifest 'matched-after-suspension' 'classes-details-selected-after-roster-clear' 'suspension-return' 10 100 'classes-details-selected-class'
    $Manifest.memory.matchedReturnWindowId = [string]$matchedReturn.windowId
    $suspendArrival = [Int64]$Context.arrival
    $workspaceRoute = Select-P914Route $Context $Manifest $EvidenceDirectory 'My Workspace' 'my_workspace'
    $classSuspension = Wait-P914Lifecycle $Context $EvidenceDirectory 'classes' 'suspended' $suspendArrival $TimeoutSeconds
    $Manifest.route.classPageSuspension = Get-P914PageModelCounts $classSuspension
    $Manifest.route.modelCounts.suspendedLifecycleRootRows = if ($null -ne $classSuspension.record.page.item_model_root_rows) { [Int64]$classSuspension.record.page.item_model_root_rows } else { $null }
    $Manifest.route.modelCounts.suspendedLifecycleEditorControlCount = if ($null -ne $classSuspension.record.page.editor_control_widget_count) { [Int64]$classSuspension.record.page.editor_control_widget_count } else { $null }
    Start-Sleep -Seconds 2
    $null = Get-P917MemoryWindow $Context $Manifest 'page-suspended' 'my-workspace-after-classes-suspension' 'page-suspension' 10 100 'my-workspace-open-workspace'

    $reentryArrival = [Int64]$Context.arrival
    $reentryRoute = Select-P914Route $Context $Manifest $EvidenceDirectory 'Classes' 'classes'
    $classReactivation = Wait-P914Lifecycle $Context $EvidenceDirectory 'classes' 'activated' $reentryArrival $TimeoutSeconds
    $Manifest.route.classPageReactivation = Get-P914PageModelCounts $classReactivation
    $classList = Wait-P917Suffix $Context $EvidenceDirectory 'classesNavigatorClassList' $TimeoutSeconds 'main' 'ControlType.List'
    $reentryTargetSearch = Find-P917TargetClassListItem $Context $Manifest $EvidenceDirectory $classList $targetAccessibleName 40 'class-list-items-reentry'
    $listItem = $reentryTargetSearch.element
    $Manifest.route.reentryTargetClassSearch = [ordered]@{ targetAccessibleName = $targetAccessibleName; maximumScrolls = 40; pages = $reentryTargetSearch.search; matched = ($null -ne $listItem) }
    if ($null -eq $listItem) { throw 'Exact fixture-derived target class was not exposed by the bounded Classes list search during re-entry.' }
    if ([string]$listItem.Current.Name -cne $targetAccessibleName) { throw 'Re-entry Classes list item differs from the exact fixture-derived accessible name.' }
    $reentrySelectionPattern = [System.Windows.Automation.SelectionItemPattern]$listItem.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
    $wasReentrySelectionAlreadySelected = [bool]$reentrySelectionPattern.Current.IsSelected
    if (-not $wasReentrySelectionAlreadySelected) {
        $reentrySelectionAction = Invoke-P917BoundedClick $Context $Manifest $EvidenceDirectory 'reselect-target-class' $listItem ([Int64]$Context.mainWindowHandle) 'UIA-targeted exact class ListItem click on re-entry'
        $reentrySelectionPattern = [System.Windows.Automation.SelectionItemPattern]$listItem.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
    }
    $Manifest.route.reentryTargetClassSelection = [ordered]@{
        classId = 1; className = $targetClassName; listItemName = [string]$listItem.Current.Name
        expectedAccessibleName = $targetAccessibleName; wasSelectedBeforeReentrySelection = $wasReentrySelectionAlreadySelected
        selected = [bool]$reentrySelectionPattern.Current.IsSelected; selectionProperty = 'UIA SelectionItemPattern.IsSelected after bounded fixture-derived re-entry search'
    }
    if (-not [bool]$Manifest.route.reentryTargetClassSelection.selected) { throw 'Exact target class list item was not selected after the Classes re-entry search.' }
    $rosterTab = Find-P917RosterTab $Context
    $null = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory 'reenter-roster-tab' $rosterTab 'Roster'
    $table = Get-P917RosterTable $Context $EvidenceDirectory $TimeoutSeconds
    Start-Sleep -Seconds 2
    $reentryReadbacks = Read-P917Targets $Context $Manifest $EvidenceDirectory $targets 'afterValue'
    $reentryContract = Save-P917RosterContract $Context $Manifest $EvidenceDirectory 'roster-reentry-contract' $table $reentryReadbacks
    $Manifest.route.reentryRosterContractRelativePath = [string]$reentryContract.manifestRecord.relativePath
    $Manifest.route.reentryScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'roster-reentry-persisted' $Context.mainWindowRoot
    $null = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'roster-reentry-tree' $table
    $reentryMetrics = Get-P917GridMetrics $table
    $Manifest.route.modelCounts.reentryRootModelRows = $reentryMetrics.rowCount
    $Manifest.route.modelCounts.reentryEditorControlCount = if ($null -ne $classReactivation.record.page.editor_control_widget_count) { [Int64]$classReactivation.record.page.editor_control_widget_count } else { $null }
    $Manifest.route.readBackChecks = [ordered]@{ beforeEdit = $initialReadbacks; afterEdit = $editedReadbacks; afterSave = $savedReadbacks; afterReentry = $reentryReadbacks }
    Start-Sleep -Seconds 2
    Set-P917SamplingContext $Context 'normal-use' 're-entry'
    $reentryWindow = Get-P917MemoryWindow $Context $Manifest 're-entry' 'roster-reentered-persisted' 're-entry' 10 100
    $Manifest.route.reentryMemoryWindowId = [string]$reentryWindow.windowId
    $classList = $null
    $targetSearch = $null
    $listItem = $null
    $selectedPattern = $null
    $detailsTab = $null
    $rosterTab = $null
    $table = $null
    $gridMetrics = $null
    $reentryTargetSearch = $null
    $reentrySelectionPattern = $null
    $reentryMetrics = $null
    $Context.mainWindowRoot = $null
    $Context.dialogRoot = $null
    $measuredCycleCount = if ($DiagnosticOnly) { 5 } else { 10 }
    Invoke-P917MeasuredRosterCycles $Context $Manifest $EvidenceDirectory $targets $TimeoutSeconds $measuredCycleCount $ObserverMode -DiagnosticOnly:$DiagnosticOnly -ClientGcReferenceMode $ClientGcReferenceMode -PayloadDiagnosticOnly:$PayloadDiagnosticOnly -RecordCountDiagnosticOnly:$RecordCountDiagnosticOnly -HeapDiagnostic $HeapDiagnostic

    $Manifest.route.memoryWindowSampleCount = 10
    $Manifest.route.memoryWindowIntervalMilliseconds = 100
    $Manifest.route.memoryWindowSamplingPolicy = 'Every settled sample records target misses at Working Set >=268435456 bytes; the inclusive 471859200-byte ceiling is enforced. Operation peaks remain a separate diagnostic.'
    $Manifest.route.workspaceSuspendedThroughClassesPage = $true
    $Manifest.route.workflowCompletedAtUtc = Get-P914Utc
}

function Get-P917ReturnAllowance([object]$Manifest) {
    $baseline = @($Manifest.memory.windows | Where-Object { [string]$_.windowId -ceq 'matched-baseline' })
    $returned = @($Manifest.memory.windows | Where-Object { [string]$_.windowId -ceq 'matched-after-suspension' })
    $evidenceFailureReasons = [Collections.Generic.List[string]]::new()
    if ($baseline.Count -ne 1) { $evidenceFailureReasons.Add("Expected one matched baseline window; observed $($baseline.Count).") }
    if ($returned.Count -ne 1) { $evidenceFailureReasons.Add("Expected one matched post-suspension window; observed $($returned.Count).") }
    $baseSamples = if ($baseline.Count -eq 1) { @($baseline[0].samples) } else { @() }
    $returnSamples = if ($returned.Count -eq 1) { @($returned[0].samples) } else { @() }
    $baseValues = @($baseSamples | Where-Object { $null -ne $_.workingSetBytes } | ForEach-Object { [Int64]$_.workingSetBytes })
    $returnValues = @($returnSamples | Where-Object { $null -ne $_.workingSetBytes } | ForEach-Object { [Int64]$_.workingSetBytes })
    if ($baseSamples.Count -lt 5 -or $returnSamples.Count -lt 5) { $evidenceFailureReasons.Add('Matched baseline and post-suspension windows each require at least five scheduled samples.') }
    if ($baseSamples.Count -ne $returnSamples.Count) { $evidenceFailureReasons.Add('Matched baseline and post-suspension windows have different scheduled sample counts.') }
    if ($baseline.Count -eq 1 -and $returned.Count -eq 1) {
        if ([string]$baseline[0].matchedState -cne [string]$returned[0].matchedState) { $evidenceFailureReasons.Add('Matched baseline and post-suspension windows identify different UI states.') }
        if ([int]$baseline[0].sampleIntervalMilliseconds -ne [int]$returned[0].sampleIntervalMilliseconds) { $evidenceFailureReasons.Add('Matched baseline and post-suspension windows have different sample intervals.') }
    }
    if ($baseValues.Count -ne $baseSamples.Count) { $evidenceFailureReasons.Add('At least one baseline Working Set metric is unavailable.') }
    if ($returnValues.Count -ne $returnSamples.Count) { $evidenceFailureReasons.Add('At least one post-suspension Working Set metric is unavailable.') }
    $baseMedian = $null; $returnMedian = $null; $range = $null; $ceiling = $null
    if ($baseValues.Count -gt 0) {
        $sortedBase = @($baseValues | Sort-Object)
        $middle = [Math]::Floor($sortedBase.Count / 2)
        $baseMedian = if ($sortedBase.Count % 2) { [double]$sortedBase[$middle] } else { ([double]$sortedBase[$middle - 1] + [double]$sortedBase[$middle]) / 2.0 }
        $range = [Int64](($baseValues | Measure-Object -Maximum).Maximum - ($baseValues | Measure-Object -Minimum).Minimum)
        $ceiling = $baseMedian + $range
    }
    if ($returnValues.Count -gt 0) {
        $sortedReturn = @($returnValues | Sort-Object)
        $middle = [Math]::Floor($sortedReturn.Count / 2)
        $returnMedian = if ($sortedReturn.Count % 2) { [double]$sortedReturn[$middle] } else { ([double]$sortedReturn[$middle - 1] + [double]$sortedReturn[$middle]) / 2.0 }
    }
    $comparisonFailureReasons = [Collections.Generic.List[string]]::new()
    $comparisonPassed = $false
    if ($null -eq $baseMedian -or $null -eq $returnMedian -or $null -eq $ceiling) {
        $comparisonFailureReasons.Add('The allowance comparison cannot be derived from the captured Working Set metrics.')
    } else {
        $comparisonPassed = $returnMedian -le $ceiling
        if (-not $comparisonPassed) {
            $comparisonFailureReasons.Add("Post-suspension median Working Set $returnMedian exceeds the baseline-derived ceiling $ceiling.")
        }
    }
    $allowance = [ordered]@{
        rule = 'post-suspension median Working Set <= baseline median Working Set + full observed baseline range'
        gating = $true
        measurementRole = 'required-cold-baseline-to-first-return cleanup gate, evaluated separately from the post-priming plateau and measured cycles'
        matchedState = if ($baseline.Count -eq 1) { [string]$baseline[0].matchedState } else { $null }
        baselineWindowId = if ($baseline.Count -eq 1) { [string]$baseline[0].windowId } else { $null }
        postSuspensionWindowId = if ($returned.Count -eq 1) { [string]$returned[0].windowId } else { $null }
        baselineSampleCount = $baseSamples.Count; postSuspensionSampleCount = $returnSamples.Count
        baselineWorkingSetSampleCount = $baseValues.Count; postSuspensionWorkingSetSampleCount = $returnValues.Count
        sampleIntervalMilliseconds = if ($baseline.Count -eq 1) { [int]$baseline[0].sampleIntervalMilliseconds } else { $null }
        baselineWorkingSetSamplesBytes = $baseValues; postSuspensionWorkingSetSamplesBytes = $returnValues
        baselineMinimumWorkingSetBytes = if ($baseValues.Count -gt 0) { [Int64](($baseValues | Measure-Object -Minimum).Minimum) } else { $null }
        baselineMaximumWorkingSetBytes = if ($baseValues.Count -gt 0) { [Int64](($baseValues | Measure-Object -Maximum).Maximum) } else { $null }
        observedBaselineRangeBytes = $range; baselineMedianWorkingSetBytes = $baseMedian
        postSuspensionMedianWorkingSetBytes = $returnMedian; maximumAllowedPostSuspensionMedianBytes = $ceiling
        passed = $comparisonPassed
        failureReasons = $comparisonFailureReasons.ToArray()
        evidenceFailureReasons = $evidenceFailureReasons.ToArray()
        capturedAtUtc = Get-P914Utc
    }
    $Manifest.memory.initialPrimingReturnAllowance = $allowance
}

function Get-P917OperationPeak([object]$Context, [object]$Manifest) {
    $samples = @($Context.sampler.Snapshot())
    $operation = @($samples | Where-Object { [string]$_.category -ceq 'operation' })
    $peak = [Int64]0
    foreach ($sample in $operation) { $peak = [Math]::Max($peak, [Int64]$sample.workingSetBytes) }
    $Manifest.memory.operationPeakWorkingSetBytes = $peak
    $Manifest.memory.operationPeakAdvisoryLimitBytes = $OperationPeakAdvisoryLimit
    $Manifest.memory.operationPeakAdvisoryOnly = $true
    $Manifest.memory.operationPeakAdvisoryExceeded = ($peak -ge $OperationPeakAdvisoryLimit)
    $Manifest.memory.privateBytesAndOtherProcessMetricsDiagnosticOnly = $true
}

function Get-P917AutomationClientMetrics([object]$Context, [string]$EvidenceDirectory) {
    if (-not $Context.Contains('automationClientSampler') -or $null -eq $Context.automationClientSampler) { return $null }
    $Context.automationClientSampler.Dispose()
    $samples = @($Context.automationClientSampler.Snapshot())
    if ($samples.Count -eq 0) { throw 'No PowerShell automation-client process memory samples were captured.' }
    $path = Join-Path $EvidenceDirectory 'automation-client-metrics.jsonl'
    [IO.File]::WriteAllText($path, '', [Text.UTF8Encoding]::new($false))
    $workingSets = [Collections.Generic.List[Int64]]::new()
    $privateBytes = [Collections.Generic.List[Int64]]::new()
    foreach ($sample in $samples) {
        $workingSet = [Int64]$sample.workingSetBytes
        $private = [Int64]$sample.privateBytesDiagnosticOnly
        $workingSets.Add($workingSet); $privateBytes.Add($private)
        Add-P914JsonLine $path ([ordered]@{
            sampleIndex = [Int64]$sample.sampleIndex; capturedAtUtc = [string]$sample.capturedAtUtc
            category = [string]$sample.category; operation = [string]$sample.operation
            processId = [Int64]$sample.processId; workingSetBytes = $workingSet
            privateBytesDiagnosticOnly = $private; peakWorkingSetBytes = [Int64]$sample.peakWorkingSetBytes
        })
    }
    $process = $Context.automationClientProcess
    $summary = [ordered]@{
        schema = 'classmngr-phase9-p9-17-uia-observer-client-metrics-v1'
        processName = [string]$process.ProcessName; processId = [Int64]$process.Id
        path = 'automation-client-metrics.jsonl'; sha256 = Get-P914FileSha256 $path
        sampleCount = $samples.Count; firstCapturedAtUtc = [string]$samples[0].capturedAtUtc
        lastCapturedAtUtc = [string]$samples[-1].capturedAtUtc
        maximumWorkingSetBytes = [Int64](($workingSets | Measure-Object -Maximum).Maximum)
        maximumPrivateBytesDiagnosticOnly = [Int64](($privateBytes | Measure-Object -Maximum).Maximum)
        privateBytesDiagnosticOnly = $true
    }
    $process.Dispose()
    return $summary
}

function Sort-P917OrdinalInventoryRows([object[]]$Rows) {
    $sortedRows = [object[]]$Rows.Clone()
    $comparison = [System.Comparison[object]]{
        param($left, $right)
        $leftPath = [string]$left.path
        $rightPath = [string]$right.path
        $leftLower = $leftPath.ToLowerInvariant()
        $rightLower = $rightPath.ToLowerInvariant()
        $order = [System.StringComparer]::Ordinal.Compare($leftLower, $rightLower)
        if ($order -ne 0) { return $order }
        return [System.StringComparer]::Ordinal.Compare($leftPath, $rightPath)
    }
    $comparer = [System.Collections.Generic.Comparer[object]]::Create($comparison)
    [Array]::Sort($sortedRows, $comparer)
    return $sortedRows
}

function New-P917EvidenceInventory([string]$EvidenceDirectory) {
    $files = [Collections.Generic.List[object]]::new()
    $prefix = [IO.Path]::GetFullPath($EvidenceDirectory).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    foreach ($file in @(Get-ChildItem -LiteralPath $EvidenceDirectory -File -Recurse)) {
        $relative = [IO.Path]::GetFullPath($file.FullName).Substring($prefix.Length).Replace('\', '/')
        if ($relative -in @('manifest.json', 'evidence-files.json')) { continue }
        $files.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-P914FileSha256 $file.FullName })
    }
    $rows = @(Sort-P917OrdinalInventoryRows -Rows $files.ToArray())
    $inventory = [ordered]@{ schema = 'classmngr-phase9-p9-17-evidence-files-v1'; root = [IO.Path]::GetFullPath($EvidenceDirectory); fileCount = $rows.Count; files = $rows }
    $canonical = @($rows | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    $inventory.sha256 = Get-P914StringSha256 ($canonical + "`n")
    $path = Join-Path $EvidenceDirectory 'evidence-files.json'
    Write-P914Json $path $inventory
    return [ordered]@{ relativePath = 'evidence-files.json'; sha256 = Get-P914FileSha256 $path; fileCount = [int]$inventory.fileCount; treeSha256 = [string]$inventory.sha256 }
}

function Invoke-P917Validator([string]$EvidenceDirectory, [string]$ManifestPath, [string]$LogName, [bool]$PersistOutput = $true) {
    $scriptPath = Join-Path $PSScriptRoot 'validate_packaged_next_p9_17_large_roster.py'
    $stdoutName = $LogName
    $stderrName = $LogName -replace 'output', 'stderr'
    $stdoutPath = if ($PersistOutput) { Join-Path (Join-Path $EvidenceDirectory 'logs') $stdoutName } else { $null }
    $stderrPath = if ($PersistOutput) { Join-Path (Join-Path $EvidenceDirectory 'logs') $stderrName } else { $null }
    $stdout = ''
    $stderr = ''
    $code = $null
    $launchError = $null
    try {
        $pythonCommand = Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1
        $startInfo = [Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = [string]$pythonCommand.Source
        # These paths cannot contain a double quote on Windows. Quoting both
        # keeps spaces in the interpreter, validator, and evidence paths safe.
        $startInfo.Arguments = ('"{0}" --manifest "{1}"' -f $scriptPath, $ManifestPath)
        $startInfo.WorkingDirectory = $RepositoryRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $startInfo.StandardOutputEncoding = [Text.Encoding]::UTF8
        $startInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
        $process = [Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        if (-not $process.Start()) { throw 'Python validator process did not start.' }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        $code = [int]$process.ExitCode
        $process.Dispose()
    } catch {
        $launchError = $_.Exception.ToString()
        if ([string]::IsNullOrEmpty($stderr)) { $stderr = $launchError }
    }
    if ($PersistOutput) {
        [IO.File]::WriteAllText($stdoutPath, $stdout, [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderrPath, $stderr, [Text.UTF8Encoding]::new($false))
    }
    return [ordered]@{
        exitCode = $code; stdout = $stdout; stderr = $stderr; launchError = $launchError
        stdoutPath = $stdoutPath; stderrPath = $stderrPath
    }
}

function Invoke-P917ValidationPersistence([string]$EvidenceDirectory, [string]$ManifestPath, [object]$Manifest) {
    $firstValidation = Invoke-P917Validator $EvidenceDirectory $ManifestPath 'validator-output.txt'
    $validationReport = $null
    if (-not [string]::IsNullOrWhiteSpace([string]$firstValidation.stdout)) {
        try { $validationReport = [string]$firstValidation.stdout | ConvertFrom-Json -ErrorAction Stop } catch { }
    }
    $validationOutputLines = [Collections.Generic.List[string]]::new()
    foreach ($streamText in @([string]$firstValidation.stdout, [string]$firstValidation.stderr)) {
        foreach ($line in ($streamText -split '\r?\n')) { if ($line.Length -gt 0) { $validationOutputLines.Add($line) } }
    }
    $manifest.validation = [ordered]@{
        status = if ($firstValidation.exitCode -eq 0) { 'passed' } else { 'failed' }
        exitCode = $firstValidation.exitCode
        validatorPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'validate_packaged_next_p9_17_large_roster.py'))
        validatorSha256 = Get-P914FileSha256 (Join-Path $PSScriptRoot 'validate_packaged_next_p9_17_large_roster.py')
        logPath = 'logs/validator-output.txt'; logSha256 = Get-P914FileSha256 $firstValidation.stdoutPath; result = $validationReport
        stdoutPath = 'logs/validator-output.txt'; stdoutSha256 = Get-P914FileSha256 $firstValidation.stdoutPath
        stderrPath = 'logs/validator-stderr.txt'; stderrSha256 = Get-P914FileSha256 $firstValidation.stderrPath
        stdout = [string]$firstValidation.stdout; stderr = [string]$firstValidation.stderr
        report = $validationReport; error = [string]$firstValidation.stderr; launchError = $firstValidation.launchError
        capturedAtUtc = Get-P914Utc
    }
    if ($firstValidation.exitCode -ne 0) {
        $manifest.status = 'failed'
        $manifest.failure = [ordered]@{
            message = if ($null -ne $firstValidation.launchError) { 'P9-17 evidence validator could not be started.' } else { 'P9-17 evidence validator rejected the completed packaged route.' }
            capturedAtUtc = Get-P914Utc; category = 'validation'; validatorExitCode = $firstValidation.exitCode
            stdoutPath = 'logs/validator-output.txt'; stderrPath = 'logs/validator-stderr.txt'
            report = $validationReport; error = [string]$firstValidation.stderr; output = $validationOutputLines.ToArray()
        }
    }
    Write-P914Json $ManifestPath $Manifest

    $manifest.evidenceInventory = New-P917EvidenceInventory $EvidenceDirectory
    Write-P914Json $ManifestPath $Manifest
    if ($firstValidation.exitCode -eq 0) {
        $finalValidation = Invoke-P917Validator $EvidenceDirectory $ManifestPath 'validator-final-output.txt' $false
        $manifest.validation.finalInventoryValidationExitCode = $finalValidation.exitCode
        $manifest.validation.finalInventoryValidationStdout = [string]$finalValidation.stdout
        $manifest.validation.finalInventoryValidationStderr = [string]$finalValidation.stderr
        $manifest.validation.finalInventoryValidationLaunchError = $finalValidation.launchError
        $manifest.validation.finalInventoryValidationCapturedAtUtc = Get-P914Utc
        if ($finalValidation.exitCode -ne 0) {
            $manifest.status = 'failed'
            $manifest.failure = [ordered]@{
                message = 'Final evidence inventory validation failed.'; capturedAtUtc = Get-P914Utc; category = 'validation'
                validatorExitCode = $finalValidation.exitCode; stdout = [string]$finalValidation.stdout; error = [string]$finalValidation.stderr
            }
        }
    }
    Write-P914Json $ManifestPath $Manifest
    if ($manifest.status -ceq 'completed') { $manifest.status = 'passed'; Write-P914Json $ManifestPath $Manifest }
    return [ordered]@{ status = [string]$manifest.status; failure = $manifest.failure; validation = $manifest.validation }
}

function Invoke-P917PackagedLifecycle([ValidateSet('full-observer', 'reduced-observer')][string]$ObserverMode = 'full-observer', [switch]$DiagnosticOnly, [string]$RunEvidenceRoot = '', [string]$ComparisonGroupId = '', [string]$ComparisonArmOrderSelection = '', [int]$ComparisonArmPosition = 0, [ValidateSet('no-gc', 'client-gc', '')][string]$ClientGcReferenceArmMode = '', [string]$ClientGcReferenceOrderSelection = '', [int]$ClientGcReferenceArmPosition = 0, [switch]$EnableDefaultProcessHeapDiagnostic, [ValidateSet('filled', 'sparse', '')][string]$PayloadDiagnosticMode = '', [ValidateSet('filled', 'reduced-rows', '')][string]$RecordCountDiagnosticMode = '') {
    $hasClientGcReferenceArm = -not [string]::IsNullOrEmpty($ClientGcReferenceArmMode)
    $hasPayloadDiagnosticArm = -not [string]::IsNullOrEmpty($PayloadDiagnosticMode)
    $hasRecordCountDiagnosticArm = -not [string]::IsNullOrEmpty($RecordCountDiagnosticMode)
    if ($hasPayloadDiagnosticArm -and (
        $DiagnosticOnly -or $ObserverMode -cne 'full-observer' -or $hasClientGcReferenceArm -or
        $EnableDefaultProcessHeapDiagnostic -or $HeapSnapshotDiagnostic
    )) { throw 'Payload diagnostic arms require the unmodified full-observer ten-cycle route without other diagnostic instrumentation.' }
    if ($hasRecordCountDiagnosticArm -and (
        $DiagnosticOnly -or $ObserverMode -cne 'full-observer' -or $hasPayloadDiagnosticArm -or
        $hasClientGcReferenceArm -or $EnableDefaultProcessHeapDiagnostic -or $HeapSnapshotDiagnostic
    )) { throw 'Roster record-count arms require the unmodified full-observer ten-cycle route without other diagnostic instrumentation.' }
    if ($DiagnosticOnly -and -not $hasClientGcReferenceArm -and (
        $ComparisonArmOrderSelection -cnotin @('FullThenReduced', 'ReducedThenFull') -or
        $ComparisonArmPosition -lt 1 -or $ComparisonArmPosition -gt 2
    )) { throw 'UIA observer diagnostic arms require a selected order and a one-based arm position.' }
    if ($hasClientGcReferenceArm -and (
        -not $DiagnosticOnly -or $ObserverMode -cne 'full-observer' -or
        $ClientGcReferenceOrderSelection -cnotin @('NoGcThenGc', 'GcThenNoGc') -or
        $ClientGcReferenceArmPosition -lt 1 -or $ClientGcReferenceArmPosition -gt 2
    )) { throw 'Client GC reference arms require diagnostic-only full-observer mode, a selected pair order, and a one-based arm position.' }
    $runEvidenceRoot = if ([string]::IsNullOrWhiteSpace($RunEvidenceRoot)) { $EvidenceRoot } else { $RunEvidenceRoot }
    $evidenceInfo = New-P914EvidenceDirectory $RepositoryRoot $runEvidenceRoot
    $runId = [string]$evidenceInfo.runId
    $evidenceDirectory = [string]$evidenceInfo.evidenceDirectory
    $manifestPath = Join-Path $evidenceDirectory 'manifest.json'
    [IO.File]::WriteAllText((Join-Path $evidenceDirectory 'uia-actions.jsonl'), '', [Text.UTF8Encoding]::new($false))
    $manifest = [ordered]@{
        schema = $Schema; taskId = $TaskId; runId = $runId; status = 'running'; startedAtUtc = Get-P914Utc; completedAtUtc = $null
        diagnosticOnly = [bool]($DiagnosticOnly -or $hasPayloadDiagnosticArm -or $hasRecordCountDiagnosticArm -or $HeapSnapshotDiagnostic)
        repositoryRoot = $RepositoryRoot; packageProvenance = $null; fixture = $null; settings = $null; process = $null; processExit = $null
        observerDiagnostic = if ($DiagnosticOnly) { [ordered]@{
            schema = 'classmngr-phase9-p9-17-uia-observer-diagnostic-v1'; diagnosticOnly = $true
            comparisonGroupId = $ComparisonGroupId; mode = $ObserverMode
            armOrderSelection = if ($hasClientGcReferenceArm) { $null } else { $ComparisonArmOrderSelection }
            armPosition = if ($hasClientGcReferenceArm) { $null } else { $ComparisonArmPosition }
            measurementPlan = [ordered]@{ primingCycleCount = 1; matchedBaselineWindowCount = 5; measuredCycleCount = 5; samplesPerMemoryWindow = 10; sampleIntervalMilliseconds = 100 }
            perMeasuredCycleContract = if ($ObserverMode -ceq 'full-observer') { 'loaded, edited, and post-save each enumerate 156 headers and read three targets' } else { 'loaded enumerates 156 headers and reads three targets; edited evidence uses immediate SetValue readbacks; post-save reads three targets' }
            finalFullVerificationRequired = $true; automationClientMetrics = $null; measuredCycleObservations = $null
        } } else { $null }
        clientGcReferenceDiagnostic = if ($hasClientGcReferenceArm) { [ordered]@{
            schema = 'classmngr-phase9-p9-17-client-gc-reference-diagnostic-v1'; diagnosticOnly = $true
            comparisonGroupId = $ComparisonGroupId; mode = $ClientGcReferenceArmMode
            armOrderSelection = $ClientGcReferenceOrderSelection; armPosition = $ClientGcReferenceArmPosition
            clientProcessId = $null; collectionEvents = [Collections.Generic.List[object]]::new()
            appGcInvoked = $false; osWorkingSetTrimInvoked = $false; heapCompactInvoked = $false
            collectionTarget = 'PowerShell automation client only'
        } } else { $null }
        route = [ordered]@{
            actions = [Collections.Generic.List[object]]::new(); routeEvents = @(); lifecycleEvents = @(); screenshots = [Collections.Generic.List[object]]::new(); uiaSnapshots = [Collections.Generic.List[object]]::new()
            memoryWindowSampleCount = 10; memoryWindowIntervalMilliseconds = 100; modelCounts = $null; readBackChecks = $null
            observerMode = $ObserverMode; observerDiagnosticOnly = [bool]$DiagnosticOnly
            observerDiagnosticArmOrderSelection = if ($hasClientGcReferenceArm) { $null } else { $ComparisonArmOrderSelection }
            observerDiagnosticArmPosition = if ($hasClientGcReferenceArm) { $null } else { $ComparisonArmPosition }
            clientGcReferenceDiagnosticOnly = $hasClientGcReferenceArm
            clientGcReferenceMode = if ($hasClientGcReferenceArm) { $ClientGcReferenceArmMode } else { $null }
            clientGcReferenceOrderSelection = if ($hasClientGcReferenceArm) { $ClientGcReferenceOrderSelection } else { $null }
            clientGcReferenceArmPosition = if ($hasClientGcReferenceArm) { $ClientGcReferenceArmPosition } else { $null }
            recordCountDiagnosticOnly = $hasRecordCountDiagnosticArm
        }
        memory = [ordered]@{
        windows = [Collections.Generic.List[object]]::new()
        workingSetTargetBytes = $WorkingSetTarget; workingSetTargetExclusive = $true
        workingSetCeilingBytes = $WorkingSetCeiling; workingSetCeilingExclusive = $false
            operationPeakAdvisoryLimitBytes = $OperationPeakAdvisoryLimit; operationPeakAdvisoryOnly = $true
            initialPrimingReturnAllowance = $null; initialPrimingReturnGating = $true
            plateauBaselineWindowIds = @(); plateauCycleWindowIds = @(); plateauCycleAnalysis = $null
        }
        cleanup = $null; failure = $null; validation = $null; evidenceInventory = $null; applicationDiagnostics = $null
    }
    if ($hasPayloadDiagnosticArm) {
        $manifest.accepted = $false
        $manifest.payloadDiagnostic = [ordered]@{
            schema = 'classmngr-phase9-p9-17-payload-diagnostic-v1'; diagnosticOnly = $true; accepted = $false
            comparisonGroupId = $ComparisonGroupId; payloadMode = $PayloadDiagnosticMode
            preparationReceiptSha256 = $null; beforeSnapshotSha256 = $null
            customCellRecordCount = 15000
            customCellValueCount = 15000
            blankCustomCellValueCount = 0
            compactSentinelValue = if ($PayloadDiagnosticMode -ceq 'sparse') { 'x' } else { $null }
            compactSentinelValueCount = if ($PayloadDiagnosticMode -ceq 'sparse') { 14997 } else { 0 }
            preservedProbeCellCount = 3
            measuredCycleCount = 10; primingCycleCount = 1; matchedBaselineWindows = 5
            samplesPerMemoryWindow = 10; sampleIntervalMilliseconds = 100
        }
        $manifest.route.payloadDiagnosticOnly = $true
    }
    if ($hasRecordCountDiagnosticArm) {
        $manifest.accepted = $false
        $expectedCustomRows = if ($RecordCountDiagnosticMode -ceq 'filled') { 15000 } else { 3 }
        $manifest.recordCountDiagnostic = [ordered]@{
            schema = 'classmngr-phase9-p9-17-roster-record-count-diagnostic-v1'
            diagnosticOnly = $true; accepted = $false
            comparisonGroupId = $ComparisonGroupId; recordCountMode = $RecordCountDiagnosticMode
            customCellRecordCount = $expectedCustomRows; customCellValueCount = $expectedCustomRows
            blankCustomCellValueCount = 0; removedCustomCellRecordCount = (15000 - $expectedCustomRows)
            preservedProbeCellCount = 3; preparationReceiptSha256 = $null; beforeSnapshotSha256 = $null
            measuredCycleCount = 10; primingCycleCount = 1; matchedBaselineWindows = 5
            samplesPerMemoryWindow = 10; sampleIntervalMilliseconds = 100
        }
        $manifest.route.recordCountDiagnosticOnly = $true
    }
    if ($HeapSnapshotDiagnostic) {
        $manifest.accepted = $false
        $manifest.heapSnapshotDiagnosticOnly = $true
    }
    if ($EnableDefaultProcessHeapDiagnostic) {
        $manifest.defaultProcessHeapDiagnostic = [ordered]@{
            schema = 'classmngr-default-process-heap-diagnostic-v1'
            enabled = $true
            environmentVariable = 'CLASSMNGR_P9_17_DEFAULT_PROCESS_HEAP_DIAGNOSTIC'
            environmentValue = '1'
            scope = 'default-process-heap'
            outputRelativePath = 'default-process-heap-checkpoints.json'
            events = @()
            validation = $null
        }
    }
    $context = $null; $fixture = $null; $completed = $false; $heapDiagnostic = $null
    try {
        if ($HeapSnapshotDiagnostic) {
            $heapDiagnostic = New-P917HeapSnapshotDiagnostic $runId
            $heapDiagnostic.repositoryManifestPath = $manifestPath
        }
        $package = Get-P917ExactTreePackage $evidenceDirectory
        $manifest.packageProvenance = $package
        $fixture = Get-P917Workspace $evidenceDirectory $PayloadDiagnosticMode $RecordCountDiagnosticMode
        $manifest.fixture = [ordered]@{
            kind = 'large-roster-editing'; workspacePath = [string]$fixture.workspacePath; preparationSourceBaseWorkspacePath = [string]$fixture.baseWorkspacePath
            preparationSourceSeedSqlPath = [string]$fixture.seedPath
            preparationReceipt = [ordered]@{ relativePath = 'workspace/preparation-receipt.json'; sha256 = [string]$fixture.preparationSha256; workspaceSha256 = [string]$fixture.preparation.workspaceSha256; identity = $fixture.preparation.identity }
            beforeSnapshot = [ordered]@{ relativePath = 'workspace/before-snapshot.json'; sha256 = [string]$fixture.beforeSha256; phase = 'before' }
            afterSnapshot = $null; fixturePreparerSha256 = [string]$fixture.preparation.preparerSha256
        }
        if ($hasPayloadDiagnosticArm) {
            $manifest.fixture.payloadMode = $PayloadDiagnosticMode
            $manifest.fixture.payloadDiagnostic = $fixture.preparation.payloadDiagnostic
            $manifest.payloadDiagnostic.preparationReceiptSha256 = [string]$fixture.preparationSha256
            $manifest.payloadDiagnostic.beforeSnapshotSha256 = [string]$fixture.beforeSha256
        }
        if ($hasRecordCountDiagnosticArm) {
            $manifest.fixture.recordCountMode = $RecordCountDiagnosticMode
            $manifest.fixture.recordCountDiagnostic = $fixture.preparation.recordCountDiagnostic
            $manifest.recordCountDiagnostic.preparationReceiptSha256 = [string]$fixture.preparationSha256
            $manifest.recordCountDiagnostic.beforeSnapshotSha256 = [string]$fixture.beforeSha256
        }
        $settings = Get-P917Settings $evidenceDirectory
        $manifest.settings = $settings
        Write-P914Json $manifestPath $manifest

        $context = Start-P917App $evidenceDirectory ([string]$package.applicationPath) $package.executionContext $fixture $package ([bool]$EnableDefaultProcessHeapDiagnostic)
        $context.workspacePath = [IO.Path]::GetFullPath([string]$fixture.workspacePath)
        $context.sampler = [P914ProcessSampler]::new($context.process)
        if ($DiagnosticOnly) {
            $context.automationClientProcess = [Diagnostics.Process]::GetCurrentProcess()
            $context.automationClientSampler = [P914ProcessSampler]::new($context.automationClientProcess)
            if ($hasClientGcReferenceArm) { $manifest.clientGcReferenceDiagnostic.clientProcessId = [Int64]$context.automationClientProcess.Id }
        }
        $null = Wait-P914MainRoot $context $evidenceDirectory $ReadinessTimeoutSeconds
        Drain-P914Output $context $evidenceDirectory
        $context.process.Refresh()
        $manifest.process = [ordered]@{
            processId = [Int64]$context.process.Id; processImagePath = [string]$context.processImagePath; processImageSha256 = [string]$context.processImageSha256
            sessionId = [Int64]$context.processSessionId; executionContext = $context.executionContext
            architecture = 'x64'; startedAtUtc = $context.process.StartTime.ToUniversalTime().ToString('o'); workspacePath = [string]$fixture.workspacePath
            startedWithoutWorkspaceArgument = $true; commandLine = [ordered]@{ executable = [string]$package.applicationPath; workspaceArgument = $null }
            mainWindowHandle = [Int64]$context.mainWindowHandle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle)
            dialogWindowHandles = @(); stdoutPath = 'logs/process-stdout.log'; stderrPath = 'logs/process-stderr.log'; metrics = $null
            environmentIsolation = [ordered]@{ settingsRootFromEnvironment = $true; globalDefaultsChanged = $false; workspaceOpenedOnlyThroughUIA = $true }
        }
        if ($EnableDefaultProcessHeapDiagnostic) {
            $manifest.process.launchPathAuditRelativePath = [string]$context.launchPathAuditRelativePath
            $manifest.process.launchPathAudit = @($context.launchPathAudit)
        }
        if ([Int64]$manifest.process.windowProcessId -ne [Int64]$context.process.Id) { throw 'Measured main HWND PID differs from the single application PID.' }
        if ($null -ne $heapDiagnostic) {
            Start-P917HeapSnapshotTrace $heapDiagnostic $context $manifest.process
        }
        Set-P917SamplingContext $context 'normal-use' 'startup-ready'
        Invoke-P917Route $context $manifest $evidenceDirectory $fixture $ReadinessTimeoutSeconds $heapDiagnostic $ObserverMode -DiagnosticOnly:$DiagnosticOnly -ClientGcReferenceMode $ClientGcReferenceArmMode -PayloadDiagnosticOnly:$hasPayloadDiagnosticArm -RecordCountDiagnosticOnly:$hasRecordCountDiagnosticArm
        if ($EnableDefaultProcessHeapDiagnostic) {
            Drain-P914Output $context $evidenceDirectory
            Add-P917DefaultProcessHeapEvidence $context $manifest $evidenceDirectory
        }
        Get-P917ReturnAllowance $manifest
        if ($null -ne $heapDiagnostic) { Complete-P917HeapSnapshotTrace $heapDiagnostic }
        Set-P917SamplingContext $context 'operation' 'natural-process-exit'
        $exit = Invoke-P914NaturalClose $context $manifest $evidenceDirectory $ExitTimeoutSeconds
        $manifest.processExit = $exit
        $manifest.process.closeMethod = [string]$exit.closeMethod
        $manifest.process.forcedTermination = $false
        $cleanup = Get-P914RuntimeCleanup $context $evidenceDirectory
        if (@($cleanup.workspaceSidecarFilesAfterExit).Count -ne 0) { throw 'Workspace SQLite sidecar files remain after natural process exit.' }
        $manifest.cleanup = $cleanup
        $afterPath = Join-Path (Join-Path $evidenceDirectory 'workspace') 'after-snapshot.json'
        $afterSummary = Invoke-P917Preparer $evidenceDirectory @('snapshot', '--phase', 'after', '--workspace', $fixture.workspacePath, '--preparation-receipt', $fixture.preparationPath, '--output', $afterPath) 'after-snapshot-output.txt'
        $afterSnapshot = Read-P914Json $afterPath
        if ([string]$afterSummary.status -cne 'passed' -or [string]$afterSnapshot.status -cne 'passed' -or [string]$afterSnapshot.phase -cne 'after' -or [string]$afterSnapshot.integrityCheck -cne 'ok' -or @($afterSnapshot.foreignKeyErrors).Count -ne 0) { throw 'Final workspace snapshot failed its status, integrity, or foreign-key checks.' }
        $manifest.fixture.afterSnapshot = [ordered]@{ relativePath = 'workspace/after-snapshot.json'; sha256 = Get-P914FileSha256 $afterPath; phase = 'after'; workspaceSha256 = [string]$afterSnapshot.workspaceSha256; identity = $afterSnapshot.identity }
        $completed = $true
    } catch {
        $failureRecord = [ordered]@{ message = $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = [string]$_.CategoryInfo.Category; fullyQualifiedErrorId = [string]$_.FullyQualifiedErrorId; position = [string]$_.InvocationInfo.PositionMessage; scriptStackTrace = [string]$_.ScriptStackTrace }
        $preContextCleanupPath = Join-Path (Join-Path (Join-Path $evidenceDirectory 'runtime') 'diagnostics') 'pre-context-launch-cleanup.json'
        if (Test-Path -LiteralPath $preContextCleanupPath -PathType Leaf) {
            try { $failureRecord.preContextLaunchCleanup = Read-P914Json $preContextCleanupPath }
            catch { $failureRecord.preContextLaunchCleanupReadError = $_.Exception.Message }
        }
        $manifest.failure = $failureRecord
    } finally {
        if ($null -ne $heapDiagnostic -and -not [bool]$heapDiagnostic.cleanupCompleted) {
            try { Stop-P917HeapSnapshotTrace $heapDiagnostic }
            catch { $heapDiagnostic.cleanupErrors.Add('Final heap trace cleanup failed: ' + $_.Exception.Message) }
        }
        if ($null -ne $context) {
            try { Drain-P914Output $context $evidenceDirectory } catch { }
            if ($null -ne $context.sampler) {
                try {
                    $context.process.Refresh()
                    if (-not $context.process.HasExited) {
                        try {
                            $null = Invoke-P914NaturalClose $context $manifest $evidenceDirectory $ExitTimeoutSeconds
                            $manifest.processExit = $context.processExit
                            if ($null -ne $manifest.process) { $manifest.process.closeMethod = [string]$context.processExit.closeMethod; $manifest.process.forcedTermination = $false }
                        } catch {
                            if ($null -eq $manifest.failure) { $manifest.failure = [ordered]@{ message = 'Natural close during cleanup failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } }
                        }
                    }
                    Get-P917OperationPeak $context $manifest
                    $context.sampler.Dispose()
                    $metrics = Get-P914SamplerSummary $context $evidenceDirectory
                    if ($metrics.Contains('stableCheckpointLimitBytesExclusive')) {
                        $metrics['inheritedSamplerStableCheckpointLimitBytesExclusive'] = [Int64]$metrics['stableCheckpointLimitBytesExclusive']
                        $metrics.Remove('stableCheckpointLimitBytesExclusive')
                    }
                    $metrics['stableCheckpointLimitMeaning'] = 'Inherited P9-14 sampler metadata only; it is independent of P9-17 acceptance memory gates.'
                    $metrics['p917WorkingSetAcceptance'] = [ordered]@{ targetBytesExclusive = 268435456; ceilingBytesInclusive = 471859200; operationPeakReportedSeparately = $true }
                    if ($null -ne $manifest.process) { $manifest.process.metrics = $metrics }
                    if ($null -eq $manifest.cleanup -and $context.process.HasExited) { $manifest.cleanup = Get-P914RuntimeCleanup $context $evidenceDirectory }
                } catch {
                    if ($null -eq $manifest.failure) { $manifest.failure = [ordered]@{ message = 'Final process evidence collection failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } }
                }
            }
            if ($DiagnosticOnly -and $null -ne $context.automationClientSampler) {
                try {
                    $clientMetrics = Get-P917AutomationClientMetrics $context $evidenceDirectory
                    $manifest.observerDiagnostic.automationClientMetrics = $clientMetrics
                    if ($hasClientGcReferenceArm) { $manifest.clientGcReferenceDiagnostic.automationClientMetrics = $clientMetrics }
                }
                catch { if ($null -eq $manifest.failure) { $manifest.failure = [ordered]@{ message = 'PowerShell automation-client memory evidence collection failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'diagnostic-evidence' } } }
            }
            $manifest.process.dialogWindowHandles = @(Get-P917VisibleDialogHandles $context)
            $manifest.route.routeEvents = @(Get-P914RouteSummary $context)
            $manifest.route.lifecycleEvents = @(Get-P914LifecycleSummary $context)
            $manifest.applicationDiagnostics = [ordered]@{ outputPath = 'application-output.jsonl'; stdoutPath = 'logs/process-stdout.log'; stderrPath = 'logs/process-stderr.log'; outputLineCount = @($context.outputLines).Count; lifecycleEventCount = @($context.lifecycleEvents).Count; routeEventCount = @($context.routeEvents).Count }
            try { Save-P914RouteManifest $manifest $evidenceDirectory } catch { if ($null -eq $manifest.failure) { $manifest.failure = [ordered]@{ message = 'Could not save route manifest: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'evidence' } } }
        }
        $manifest.completedAtUtc = Get-P914Utc
        $manifest.status = if ($completed -and $null -eq $manifest.failure) { 'completed' } else { 'failed' }
        Write-P914Json $manifestPath $manifest
    }

    if ($manifest.status -ceq 'completed') {
        if ($DiagnosticOnly -or $hasPayloadDiagnosticArm -or $hasRecordCountDiagnosticArm) {
            $manifest.evidenceInventory = New-P917EvidenceInventory $evidenceDirectory
            $manifest.status = 'diagnostic-only'
            if ($hasPayloadDiagnosticArm -or $hasRecordCountDiagnosticArm) { $manifest.accepted = $false }
            $manifest.validation = [ordered]@{
                status = 'skipped'; diagnosticOnly = $true
                reason = if ($hasPayloadDiagnosticArm) { 'Filled/sparse custom-cell payload comparison is diagnostic-only and excluded from P9-17 acceptance.' } elseif ($hasRecordCountDiagnosticArm) { 'Filled/reduced-row custom-record cardinality comparison is diagnostic-only and excluded from P9-17 acceptance.' } elseif ($hasClientGcReferenceArm) { 'Client GC reference arms are diagnostic-only full-observer runs and are excluded from P9-17 acceptance.' } else { 'UIA observer arms intentionally use a reduced or comparison observer contract and are excluded from P9-17 acceptance.' }
                capturedAtUtc = Get-P914Utc
            }
            Write-P914Json $manifestPath $manifest
        } elseif ($null -ne $heapDiagnostic) {
            $manifest.evidenceInventory = New-P917EvidenceInventory $evidenceDirectory
            $manifest.status = 'diagnostic-only'
            $manifest.validation = [ordered]@{ status = 'skipped'; diagnosticOnly = $true; reason = 'Heap snapshot tracing changes process memory; this run is excluded from P9-17 acceptance.'; capturedAtUtc = Get-P914Utc }
            Write-P914Json $manifestPath $manifest
        } else {
            $null = Invoke-P917ValidationPersistence $evidenceDirectory $manifestPath $manifest
        }
    }
    if ($null -ne $heapDiagnostic) {
        $heapDiagnostic.repositoryManifestPath = $manifestPath
        if (Test-Path -LiteralPath $manifestPath -PathType Leaf) { $heapDiagnostic.repositoryManifestSha256 = Get-P914FileSha256 $manifestPath }
        if ([string]$heapDiagnostic.status -cne 'completed' -and $null -eq $heapDiagnostic.failure) {
            $heapDiagnostic.failure = if ($heapDiagnostic.cleanupErrors.Count -gt 0) { @($heapDiagnostic.cleanupErrors) -join ' ' } else { 'The diagnostic route ended before all three heap snapshots and cleanup completed.' }
        }
        Write-P914Json ([string]$heapDiagnostic.manifestPath) $heapDiagnostic
    }
    $armSummary = [ordered]@{
        runId = $runId; evidenceDirectory = $evidenceDirectory; manifestPath = $manifestPath
        status = [string]$manifest.status; diagnosticOnly = [bool]($DiagnosticOnly -or $hasPayloadDiagnosticArm -or $hasRecordCountDiagnosticArm -or $HeapSnapshotDiagnostic); observerMode = $ObserverMode; failure = $manifest.failure; validation = $manifest.validation
        heapSnapshotDiagnostic = if ($null -ne $heapDiagnostic) { [ordered]@{ status = [string]$heapDiagnostic.status; runId = [string]$heapDiagnostic.runId; evidenceDirectory = [string]$heapDiagnostic.evidenceDirectory; manifestPath = [string]$heapDiagnostic.manifestPath; tracePath = [string]$heapDiagnostic.tracePath; traceSha256 = $heapDiagnostic.traceSha256; noActiveWprSessionAtEnd = $heapDiagnostic.noActiveWprSessionAtEnd } } else { $null }
    }
    if ($hasPayloadDiagnosticArm) { $armSummary.payloadMode = $PayloadDiagnosticMode }
    if ($hasRecordCountDiagnosticArm) { $armSummary.recordCountMode = $RecordCountDiagnosticMode }
    return $armSummary
}

function Test-P917ClientGcReferenceArmMetadata([object]$ArmManifest, [string]$ExpectedMode, [string]$ExpectedOrderSelection, [int]$ExpectedPosition) {
    $failures = [Collections.Generic.List[string]]::new()
    $diagnostic = Get-P917OptionalProperty $ArmManifest 'clientGcReferenceDiagnostic'
    $route = Get-P917OptionalProperty $ArmManifest 'route'
    $process = Get-P917OptionalProperty $ArmManifest 'process'
    $observer = Get-P917OptionalProperty $ArmManifest 'observerDiagnostic'
    if ($null -eq $diagnostic -or $null -eq $route -or $null -eq $process -or $null -eq $observer) {
        return [pscustomobject]@{ passed = $false; failureReasons = @('Client GC reference arm manifest is missing diagnostic, route, process, or observer data.') }
    }
    if ([string](Get-P917OptionalProperty $ArmManifest 'status') -cne 'diagnostic-only' -or -not [bool](Get-P917OptionalProperty $ArmManifest 'diagnosticOnly')) {
        $failures.Add('Client GC reference arm is not marked diagnostic-only.')
    }
    if ([string](Get-P917OptionalProperty $diagnostic 'schema') -cne 'classmngr-phase9-p9-17-client-gc-reference-diagnostic-v1' -or
        -not [bool](Get-P917OptionalProperty $diagnostic 'diagnosticOnly') -or
        [string](Get-P917OptionalProperty $diagnostic 'mode') -cne $ExpectedMode -or
        [string](Get-P917OptionalProperty $diagnostic 'armOrderSelection') -cne $ExpectedOrderSelection -or
        [Int64](Get-P917OptionalProperty $diagnostic 'armPosition') -ne $ExpectedPosition -or
        [bool](Get-P917OptionalProperty $diagnostic 'appGcInvoked') -or
        [bool](Get-P917OptionalProperty $diagnostic 'osWorkingSetTrimInvoked') -or
        [bool](Get-P917OptionalProperty $diagnostic 'heapCompactInvoked') -or
        [string](Get-P917OptionalProperty $diagnostic 'collectionTarget') -cne 'PowerShell automation client only') {
        $failures.Add('Client GC reference arm order or mode metadata is inconsistent.')
    }
    if ([string](Get-P917OptionalProperty $route 'observerMode') -cne 'full-observer' -or
        [Int64](Get-P917OptionalProperty $route 'measuredCycleCount') -ne 5 -or
        [bool](Get-P917OptionalProperty $route 'clientGcReferenceDiagnosticOnly') -ne $true -or
        [string](Get-P917OptionalProperty $route 'clientGcReferenceMode') -cne $ExpectedMode -or
        [string](Get-P917OptionalProperty $route 'clientGcReferenceOrderSelection') -cne $ExpectedOrderSelection -or
        [Int64](Get-P917OptionalProperty $route 'clientGcReferenceArmPosition') -ne $ExpectedPosition) {
        $failures.Add('Client GC reference route is not a five-cycle full-observer arm with matching order metadata.')
    }
    $modelCounts = Get-P917OptionalProperty $route 'modelCounts'
    if ([Int64](Get-P917OptionalProperty $modelCounts 'rosterRows') -ne 100 -or
        [Int64](Get-P917OptionalProperty $modelCounts 'rosterColumns') -ne 156 -or
        [Int64](Get-P917OptionalProperty $modelCounts 'verifiedHeaderCount') -ne 156) {
        $failures.Add('Client GC reference arm did not verify the 100x156 full-observer roster.')
    }
    if ([string](Get-P917OptionalProperty $observer 'mode') -cne 'full-observer') {
        $failures.Add('Client GC reference arm observer metadata is not full-observer.')
    }
    $cycleRecords = @(Get-P917OptionalProperty $route 'cycleRecords')
    $events = @(Get-P917OptionalProperty $diagnostic 'collectionEvents')
    if ($cycleRecords.Count -ne 5 -or $events.Count -ne 5) {
        $failures.Add('Client GC reference arm must contain exactly five measured cycles and five collection-boundary events.')
    }
    $clientMetrics = Get-P917OptionalProperty $diagnostic 'automationClientMetrics'
    $clientProcessId = [Int64](Get-P917OptionalProperty $diagnostic 'clientProcessId')
    if ($null -eq $clientMetrics -or $clientProcessId -le 0 -or
        [Int64](Get-P917OptionalProperty $clientMetrics 'processId') -ne $clientProcessId -or
        $clientProcessId -eq [Int64](Get-P917OptionalProperty $process 'processId')) {
        $failures.Add('Client GC reference counters are not bound to the live automation-client process.')
    }
    $windowsById = @{}
    foreach ($window in @(Get-P917OptionalProperty (Get-P917OptionalProperty $ArmManifest 'memory') 'windows')) {
        $windowsById[[string](Get-P917OptionalProperty $window 'windowId')] = $window
    }
    $expectedSequence = '[GC]::Collect(); [GC]::WaitForPendingFinalizers(); [GC]::Collect()'
    $cycleLimit = [Math]::Min(5, [Math]::Min($cycleRecords.Count, $events.Count))
    for ($index = 0; $index -lt $cycleLimit; $index++) {
        $expectedCycle = $index + 1
        $cycle = $cycleRecords[$index]
        $event = $events[$index]
        $checkpoint = Get-P917OptionalProperty $cycle 'suspensionCheckpoint'
        $windowId = [string](Get-P917OptionalProperty $cycle 'postSuspensionWindowId')
        $window = if ($windowsById.ContainsKey($windowId)) { $windowsById[$windowId] } else { $null }
        $label = "Client GC reference cycle $expectedCycle"
        if ([Int64](Get-P917OptionalProperty $cycle 'cycleNumber') -ne $expectedCycle -or
            [Int64](Get-P917OptionalProperty $event 'cycleNumber') -ne $expectedCycle -or
            [Int64](Get-P917OptionalProperty $event 'armPosition') -ne $ExpectedPosition -or
            [string](Get-P917OptionalProperty $event 'mode') -cne $ExpectedMode -or
            [string](Get-P917OptionalProperty $event 'armOrderSelection') -cne $ExpectedOrderSelection -or
            [string](Get-P917OptionalProperty $event 'postSuspensionWindowId') -cne $windowId) {
            $failures.Add("$label has mismatched cycle, arm, order, or return-window metadata.")
        }
        if ([string](Get-P917OptionalProperty $event 'schema') -cne 'classmngr-phase9-p9-17-client-gc-reference-event-v1' -or
            [Int64](Get-P917OptionalProperty $event 'clientProcessId') -ne $clientProcessId -or
            [Int64](Get-P917OptionalProperty $event 'appProcessId') -ne [Int64](Get-P917OptionalProperty $process 'processId')) {
            $failures.Add("$label process identities do not match the client and measured app.")
        }
        if (-not [bool](Get-P917OptionalProperty $checkpoint 'freshActiveTreeCaptured') -or
            [bool](Get-P917OptionalProperty $checkpoint 'activeUiAutomationRosterTablePresent') -or
            [Int64](Get-P917OptionalProperty $checkpoint 'activeTreeRosterControlCount') -ne 0 -or
            -not [bool](Get-P917OptionalProperty $event 'freshActiveTreeCaptured') -or
            [bool](Get-P917OptionalProperty $event 'activeUiAutomationRosterTablePresent') -or
            [Int64](Get-P917OptionalProperty $event 'activeTreeRosterControlCount') -ne 0 -or
            -not [bool](Get-P917OptionalProperty $event 'uiAutomationReferenceScopesEnded') -or
            -not [bool](Get-P917OptionalProperty $event 'retainedUiAutomationRootsReleased')) {
            $failures.Add("$label lacks the fresh roster-absence proof or reference-release evidence.")
        }
        if ([string](Get-P917OptionalProperty $event 'collectionTarget') -cne 'PowerShell automation client only' -or
            [bool](Get-P917OptionalProperty $event 'appGcRequested') -or
            [bool](Get-P917OptionalProperty $event 'osWorkingSetTrimRequested') -or
            [bool](Get-P917OptionalProperty $event 'heapCompactRequested')) {
            $failures.Add("$label metadata reports a disallowed collection target or memory operation.")
        }
        $forced = [bool](Get-P917OptionalProperty $event 'forcedCollectionRequested')
        if ($ExpectedMode -ceq 'client-gc') {
            if (-not $forced -or [Int64](Get-P917OptionalProperty $event 'forcedCollectionCallCount') -ne 2 -or
                [string](Get-P917OptionalProperty $event 'collectionSequence') -cne $expectedSequence) {
                $failures.Add("$label is missing the exact client GC sequence.")
            }
        } elseif ($forced -or [Int64](Get-P917OptionalProperty $event 'forcedCollectionCallCount') -ne 0 -or
            $null -ne (Get-P917OptionalProperty $event 'collectionStartedAtUtc') -or
            $null -ne (Get-P917OptionalProperty $event 'collectionCompletedAtUtc') -or
            $null -ne (Get-P917OptionalProperty $event 'collectionDurationMilliseconds') -or
            $null -ne (Get-P917OptionalProperty $event 'collectionSequence')) {
            $failures.Add("$label no-GC arm records an explicit forced collection.")
        }
        $runtime = Get-P917OptionalProperty $event 'runtime'
        $before = Get-P917OptionalProperty $runtime 'before'
        $after = Get-P917OptionalProperty $runtime 'after'
        $delta = Get-P917OptionalProperty $runtime 'delta'
        if ([string]::IsNullOrWhiteSpace([string](Get-P917OptionalProperty $runtime 'frameworkVersion')) -or
            [Int64](Get-P917OptionalProperty $runtime 'maxGeneration') -lt 2) {
            $failures.Add("$label is missing the PowerShell runtime identity or generation range.")
        }
        foreach ($generation in @(0, 1, 2)) {
            $counterName = 'generation{0}Collections' -f $generation
            if ($null -eq (Get-P917OptionalProperty $before $counterName) -or
                $null -eq (Get-P917OptionalProperty $after $counterName) -or
                $null -eq (Get-P917OptionalProperty $delta $counterName) -or
                [Int64](Get-P917OptionalProperty $before $counterName) -lt 0 -or
                [Int64](Get-P917OptionalProperty $after $counterName) -lt [Int64](Get-P917OptionalProperty $before $counterName) -or
                ([Int64](Get-P917OptionalProperty $after $counterName) - [Int64](Get-P917OptionalProperty $before $counterName)) -ne [Int64](Get-P917OptionalProperty $delta $counterName)) {
                $failures.Add("$label has missing or decreasing runtime GC counter $counterName.")
            }
        }
        if ($null -eq (Get-P917OptionalProperty $before 'managedBytesEstimate') -or
            $null -eq (Get-P917OptionalProperty $after 'managedBytesEstimate') -or
            $null -eq (Get-P917OptionalProperty $delta 'managedBytesEstimate') -or
            ([Int64](Get-P917OptionalProperty $after 'managedBytesEstimate') - [Int64](Get-P917OptionalProperty $before 'managedBytesEstimate')) -ne [Int64](Get-P917OptionalProperty $delta 'managedBytesEstimate')) {
            $failures.Add("$label has missing or inconsistent managed-memory estimates.")
        }
        if ($ExpectedMode -ceq 'client-gc' -and (
            [Int64](Get-P917OptionalProperty $runtime 'maxGeneration') -lt 2 -or
            [Int64](Get-P917OptionalProperty (Get-P917OptionalProperty $runtime 'delta') 'generation2Collections') -lt 2
        )) { $failures.Add("$label did not record two completed full-generation collections.") }
        if ($null -eq $window) {
            $failures.Add("$label has no matching post-suspension sample window.")
        } else {
            try {
                $proofAt = [DateTimeOffset]::Parse([string](Get-P917OptionalProperty $checkpoint 'activeTreeCapturedAtUtc')).ToUniversalTime()
                $eventStart = [DateTimeOffset]::Parse([string](Get-P917OptionalProperty $event 'eventStartedAtUtc')).ToUniversalTime()
                $eventEnd = [DateTimeOffset]::Parse([string](Get-P917OptionalProperty $event 'eventCompletedAtUtc')).ToUniversalTime()
                $sampleStart = [DateTimeOffset]::Parse([string](Get-P917OptionalProperty $window 'startedAtUtc')).ToUniversalTime()
                $eventProofAt = [DateTimeOffset]::Parse([string](Get-P917OptionalProperty $event 'activeTreeCapturedAtUtc')).ToUniversalTime()
                if ($proofAt -ne $eventProofAt -or $proofAt -gt $eventStart -or $eventStart -gt $eventEnd -or $eventEnd -gt $sampleStart) {
                    $failures.Add("$label timestamps do not place the client boundary after the absence proof and before app sampling.")
                }
                if ($ExpectedMode -ceq 'client-gc') {
                    $collectionStart = [DateTimeOffset]::Parse([string](Get-P917OptionalProperty $event 'collectionStartedAtUtc')).ToUniversalTime()
                    $collectionEnd = [DateTimeOffset]::Parse([string](Get-P917OptionalProperty $event 'collectionCompletedAtUtc')).ToUniversalTime()
                    if ([Int64](Get-P917OptionalProperty $event 'collectionDurationMilliseconds') -lt 0 -or
                        $eventStart -gt $collectionStart -or $collectionStart -gt $collectionEnd -or $collectionEnd -gt $eventEnd) {
                        $failures.Add("$label collection timestamps are inconsistent with the event boundary.")
                    }
                }
            } catch {
                $failures.Add("$label has invalid or missing proof, collection, or sample timestamps.")
            }
        }
    }
    return [pscustomobject]@{ passed = ($failures.Count -eq 0); failureReasons = $failures.ToArray() }
}

function Get-P917ClientGcReferenceUiActionSignature([object]$ArmManifest) {
    $signature = [Collections.Generic.List[object]]::new()
    foreach ($action in @(Get-P917OptionalProperty (Get-P917OptionalProperty $ArmManifest 'route') 'actions')) {
        $target = Get-P917OptionalProperty $action 'target'
        $signature.Add([ordered]@{
            actionKind = [string](Get-P917OptionalProperty $action 'actionKind')
            method = [string](Get-P917OptionalProperty $action 'method')
            cycleNumber = Get-P917OptionalProperty $action 'cycleNumber'
            measurementRole = Get-P917OptionalProperty $action 'measurementRole'
            row = Get-P917OptionalProperty $action 'row'
            column = Get-P917OptionalProperty $action 'column'
            modelColumn = Get-P917OptionalProperty $action 'modelColumn'
            storedColumn = Get-P917OptionalProperty $action 'storedColumn'
            expectedBeforeValue = Get-P917OptionalProperty $action 'expectedBeforeValue'
            valueAfter = Get-P917OptionalProperty $action 'valueAfter'
            selectedTabName = Get-P917OptionalProperty $action 'selectedTabName'
            expectedSection = Get-P917OptionalProperty $action 'expectedSection'
            horizontalPercent = Get-P917OptionalProperty $action 'horizontalPercent'
            verticalPercent = Get-P917OptionalProperty $action 'verticalPercent'
            targetName = Get-P917OptionalProperty $target 'name'
            targetControlType = Get-P917OptionalProperty $target 'controlType'
            targetAutomationId = Get-P917OptionalProperty $target 'automationId'
        })
    }
    return $signature.ToArray()
}

function Get-P917DefaultHeapDiagnosticReadbackSignature([object]$ArmManifest) {
    $route = Get-P917OptionalProperty $ArmManifest 'route'
    $cycles = [Collections.Generic.List[object]]::new()
    foreach ($cycle in @(Get-P917OptionalProperty $route 'cycleRecords')) {
        $cycleSignature = [ordered]@{ cycleNumber = [Int64](Get-P917OptionalProperty $cycle 'cycleNumber') }
        foreach ($phase in @('initialReadbacks', 'editedReadbacks', 'savedReadbacks')) {
            $rows = [Collections.Generic.List[object]]::new()
            foreach ($readback in @(Get-P917OptionalProperty $cycle $phase)) {
                $rows.Add([ordered]@{
                    name = [string](Get-P917OptionalProperty $readback 'name')
                    row = Get-P917OptionalProperty $readback 'row'
                    modelColumn = Get-P917OptionalProperty $readback 'modelColumn'
                    storedColumn = Get-P917OptionalProperty $readback 'storedColumn'
                    expected = Get-P917OptionalProperty $readback 'expected'
                    actual = Get-P917OptionalProperty $readback 'actual'
                    matched = Get-P917OptionalProperty $readback 'matched'
                })
            }
            $cycleSignature[$phase] = $rows.ToArray()
        }
        $cycles.Add($cycleSignature)
    }
    $finalReadbacks = [Collections.Generic.List[object]]::new()
    foreach ($readback in @(Get-P917OptionalProperty (Get-P917OptionalProperty $route 'readBackChecks') 'afterCycles')) {
        $finalReadbacks.Add([ordered]@{
            name = [string](Get-P917OptionalProperty $readback 'name')
            row = Get-P917OptionalProperty $readback 'row'
            modelColumn = Get-P917OptionalProperty $readback 'modelColumn'
            storedColumn = Get-P917OptionalProperty $readback 'storedColumn'
            expected = Get-P917OptionalProperty $readback 'expected'
            actual = Get-P917OptionalProperty $readback 'actual'
            matched = Get-P917OptionalProperty $readback 'matched'
        })
    }
    return [ordered]@{ cycles = $cycles.ToArray(); finalReadbacks = $finalReadbacks.ToArray() }
}

function Get-P917DefaultHeapCheckpointSignature([object]$ArmManifest) {
    $diagnostic = Get-P917OptionalProperty $ArmManifest 'defaultProcessHeapDiagnostic'
    $signature = [Collections.Generic.List[object]]::new()
    foreach ($event in @(Get-P917OptionalProperty $diagnostic 'events')) {
        $heap = Get-P917OptionalProperty $event 'default_process_heap'
        $eventType = [string](Get-P917OptionalProperty $event 'event')
        $signature.Add([ordered]@{
            event = $eventType
            eventSequence = Get-P917OptionalProperty $event 'event_sequence'
            checkpoint = [string](Get-P917OptionalProperty $event 'checkpoint')
            stage = [string](Get-P917OptionalProperty $event 'stage')
            rosterWidgetInstanceId = Get-P917OptionalProperty $event 'roster_widget_instance_id'
            saveSequence = Get-P917OptionalProperty $event 'save_sequence'
            row = Get-P917OptionalProperty $event 'row'
            column = Get-P917OptionalProperty $event 'column'
            available = Get-P917OptionalProperty $heap 'available'
            error = Get-P917OptionalProperty $heap 'error'
        })
    }
    return $signature.ToArray()
}

function Invoke-P917ClientGcReferenceComparison {
    if (-not $ClientGcReferenceDiagnostic) { throw 'Client GC reference comparison requires -ClientGcReferenceDiagnostic.' }
    if ($HeapSnapshotDiagnostic) { throw 'Client GC reference comparison cannot be combined with the heap snapshot diagnostic.' }
    if (-not [string]::IsNullOrWhiteSpace($PostValidateManifestPath)) { throw 'Client GC reference comparison cannot be combined with post-validation mode.' }
    if ($null -eq $ClientGcReferenceDiagnosticOrderPlan) { throw 'Client GC reference comparison has no validated arm order plan.' }

    $armModes = @($ClientGcReferenceDiagnosticOrderPlan.modes)
    $evidenceRoot = if ([IO.Path]::IsPathRooted($ClientGcReferenceDiagnosticEvidenceRoot)) { [IO.Path]::GetFullPath($ClientGcReferenceDiagnosticEvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $ClientGcReferenceDiagnosticEvidenceRoot)) }
    $repositoryPrefix = $RepositoryRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    if (-not $evidenceRoot.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Client GC reference diagnostic evidence root must be inside the repository.' }
    $comparisonId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
    $comparisonRoot = Join-Path $evidenceRoot $comparisonId
    if (Test-Path -LiteralPath $comparisonRoot) { throw "Refusing to overwrite client GC reference comparison evidence: $comparisonRoot" }
    [void][IO.Directory]::CreateDirectory($comparisonRoot)
    $manifestPath = Join-Path $comparisonRoot 'comparison-manifest.json'
    $comparison = [ordered]@{
        schema = 'classmngr-phase9-p9-17-client-gc-reference-comparison-v1'; taskId = $TaskId
        comparisonId = $comparisonId; status = 'running'; diagnosticOnly = $true; accepted = $false
        acceptanceUse = 'Diagnostic comparison only; both full-observer arms are excluded from P9-17 acceptance.'
        collectionTarget = 'PowerShell automation client only'; appGcInvoked = $false
        osWorkingSetTrimInvoked = $false; heapCompactInvoked = $false
        clientProcessId = [Int64]$PID
        armOrderSelection = [string]$ClientGcReferenceDiagnosticOrder; armExecutionOrder = $armModes
        observerMode = 'full-observer'; rosterRows = 100; rosterColumns = 156
        measuredCyclesPerArm = 5; primingCyclesPerArm = 1; matchedBaselineWindowsPerArm = 5
        arms = [Collections.Generic.List[object]]::new(); comparisonChecks = $null; failure = $null
        createdAtUtc = Get-P914Utc; completedAtUtc = $null
        validation = [ordered]@{ status = 'skipped'; diagnosticOnly = $true; reason = 'The client-reference comparison is diagnostic-only and is not a P9-17 acceptance result.' }
    }
    Write-P914Json $manifestPath $comparison

    $armSummaries = [Collections.Generic.List[object]]::new()
    for ($armIndex = 0; $armIndex -lt $armModes.Count; $armIndex++) {
        $mode = [string]$armModes[$armIndex]
        $position = $armIndex + 1
        $runEvidenceRoot = Join-Path $comparisonRoot $mode
        $armResult = $null
        try {
            $armResult = Invoke-P917PackagedLifecycle -ObserverMode 'full-observer' -DiagnosticOnly -RunEvidenceRoot $runEvidenceRoot -ComparisonGroupId $comparisonId -ClientGcReferenceArmMode $mode -ClientGcReferenceOrderSelection $ClientGcReferenceDiagnosticOrder -ClientGcReferenceArmPosition $position
        } catch {
            $armResult = [ordered]@{ status = 'failed'; diagnosticOnly = $true; observerMode = 'full-observer'; failure = [ordered]@{ message = $_.Exception.Message } }
        }

        $armManifestPath = Get-P917OptionalProperty $armResult 'manifestPath'
        $armManifestSha = $null
        $armManifest = $null
        if ($null -ne $armManifestPath -and (Test-Path -LiteralPath ([string]$armManifestPath) -PathType Leaf)) {
            $armManifestSha = Get-P914FileSha256 ([string]$armManifestPath)
            $armManifest = Read-P914Json ([string]$armManifestPath)
        }
        $metadataValidation = if ($null -ne $armManifest) {
            Test-P917ClientGcReferenceArmMetadata $armManifest $mode $ClientGcReferenceDiagnosticOrder $position
        } else {
            [pscustomobject]@{ passed = $false; failureReasons = @('Client GC reference arm manifest was not written.') }
        }
        $package = Get-P917OptionalProperty $armManifest 'packageProvenance'
        $fixture = Get-P917OptionalProperty $armManifest 'fixture'
        $preparation = Get-P917OptionalProperty $fixture 'preparationReceipt'
        $process = Get-P917OptionalProperty $armManifest 'process'
        $route = Get-P917OptionalProperty $armManifest 'route'
        $diagnostic = Get-P917OptionalProperty $armManifest 'clientGcReferenceDiagnostic'
        $actionSignature = if ($null -ne $armManifest) { Get-P917ClientGcReferenceUiActionSignature $armManifest } else { @() }
        $actionSignatureJson = ConvertTo-Json -InputObject @($actionSignature) -Depth 16 -Compress
        $fixtureIdentity = Get-P917OptionalProperty $preparation 'identity'
        $fixtureIdentityJson = if ($null -ne $fixtureIdentity) { ConvertTo-Json -InputObject $fixtureIdentity -Depth 20 -Compress } else { '' }
        $manifestStatus = Get-P917OptionalProperty $armManifest 'status'
        $manifestDiagnosticOnly = Get-P917OptionalProperty $armManifest 'diagnosticOnly'
        $armRecord = [ordered]@{
            mode = $mode; armPosition = $position; armOrderSelection = $ClientGcReferenceDiagnosticOrder
            status = if ($null -ne $manifestStatus) { [string]$manifestStatus } else { [string](Get-P917OptionalProperty $armResult 'status') }
            diagnosticOnly = if ($null -ne $manifestDiagnosticOnly) { [bool]$manifestDiagnosticOnly } else { [bool](Get-P917OptionalProperty $armResult 'diagnosticOnly') }
            runId = Get-P917OptionalProperty $armResult 'runId'
            evidenceDirectory = Get-P917OptionalProperty $armResult 'evidenceDirectory'
            manifestPath = if ($null -ne $armManifestPath) { [string]$armManifestPath } else { $null }
            manifestSha256 = $armManifestSha
            processId = if ($null -ne $process) { [Int64](Get-P917OptionalProperty $process 'processId') } else { $null }
            clientProcessId = if ($null -ne $diagnostic) { [Int64](Get-P917OptionalProperty $diagnostic 'clientProcessId') } else { $null }
            comparisonGroupId = Get-P917OptionalProperty $diagnostic 'comparisonGroupId'
            applicationSha256 = Get-P917OptionalProperty $package 'applicationSha256'
            packageSha256 = Get-P917OptionalProperty $package 'packageSha256'
            productSourceSha256 = Get-P917OptionalProperty $package 'productSourceSha256'
            currentSourceSha256 = Get-P917OptionalProperty $package 'currentSourceSha256'
            fixtureWorkspaceSha256 = Get-P917OptionalProperty $preparation 'workspaceSha256'
            fixtureIdentity = $fixtureIdentity
            fixturePreparerSha256 = Get-P917OptionalProperty $fixture 'fixturePreparerSha256'
            automationClientMetrics = Get-P917OptionalProperty $diagnostic 'automationClientMetrics'
            collectionEvents = @(Get-P917OptionalProperty $diagnostic 'collectionEvents')
            measuredCycleCount = Get-P917OptionalProperty $route 'measuredCycleCount'
            observerMode = Get-P917OptionalProperty $route 'observerMode'
            rosterRows = Get-P917OptionalProperty (Get-P917OptionalProperty $route 'modelCounts') 'rosterRows'
            rosterColumns = Get-P917OptionalProperty (Get-P917OptionalProperty $route 'modelCounts') 'rosterColumns'
            actionSignature = @($actionSignature)
            metadataValidation = [ordered]@{ passed = [bool]$metadataValidation.passed; failureReasons = @($metadataValidation.failureReasons) }
            failure = Get-P917OptionalProperty $armResult 'failure'
        }
        $comparison.arms.Add($armRecord)
        $armSummaries.Add([pscustomobject]@{
            mode = $mode; position = $position; status = [string]$armRecord.status
            processId = $armRecord.processId; clientProcessId = $armRecord.clientProcessId
            comparisonGroupId = [string]$armRecord.comparisonGroupId
            applicationSha256 = [string]$armRecord.applicationSha256; packageSha256 = [string]$armRecord.packageSha256
            productSourceSha256 = [string]$armRecord.productSourceSha256; currentSourceSha256 = [string]$armRecord.currentSourceSha256
            fixtureWorkspaceSha256 = [string]$armRecord.fixtureWorkspaceSha256; fixtureIdentityJson = $fixtureIdentityJson
            fixturePreparerSha256 = [string]$armRecord.fixturePreparerSha256; actionSignatureJson = $actionSignatureJson
            measuredCycleCount = $armRecord.measuredCycleCount; observerMode = [string]$armRecord.observerMode
            rosterRows = $armRecord.rosterRows; rosterColumns = $armRecord.rosterColumns
            diagnosticOnly = [bool]$armRecord.diagnosticOnly; metadataValid = [bool]$metadataValidation.passed
            eventCount = @($armRecord.collectionEvents).Count; metricsCaptured = ($null -ne $armRecord.automationClientMetrics)
        })
        $armManifest = $null; $package = $null; $fixture = $null; $preparation = $null; $process = $null; $route = $null; $diagnostic = $null
        $actionSignature = $null; $actionSignatureJson = $null; $fixtureIdentity = $null; $fixtureIdentityJson = $null
        Write-P914Json $manifestPath $comparison
    }

    $comparisonPassed = $false
    if ($armSummaries.Count -eq 2) {
        $left = $armSummaries[0]; $right = $armSummaries[1]
        $observedModesJson = ConvertTo-Json -InputObject @($comparison.arms | ForEach-Object { [string]$_.mode }) -Compress
        $expectedModesJson = ConvertTo-Json -InputObject @($armModes | ForEach-Object { [string]$_ }) -Compress
        $checks = [ordered]@{
            selectedArmOrderMatches = [string]$comparison.armOrderSelection -ceq $ClientGcReferenceDiagnosticOrder -and
                [Int64]$comparison.arms[0].armPosition -eq 1 -and [Int64]$comparison.arms[1].armPosition -eq 2 -and
                $observedModesJson -ceq $expectedModesJson
            expectedNoGcAndClientGcArms = @($armSummaries | Where-Object { $_.mode -ceq 'no-gc' }).Count -eq 1 -and @($armSummaries | Where-Object { $_.mode -ceq 'client-gc' }).Count -eq 1
            bothArmsDiagnosticOnly = $left.diagnosticOnly -and $right.diagnosticOnly -and $left.status -ceq 'diagnostic-only' -and $right.status -ceq 'diagnostic-only'
            sameComparisonGroupId = $left.comparisonGroupId -ceq $comparisonId -and $right.comparisonGroupId -ceq $comparisonId
            distinctPackagedProcesses = [Int64]$left.processId -gt 0 -and [Int64]$right.processId -gt 0 -and [Int64]$left.processId -ne [Int64]$right.processId
            samePowerShellClientProcess = [Int64]$left.clientProcessId -eq [Int64]$PID -and [Int64]$right.clientProcessId -eq [Int64]$PID -and [Int64]$left.clientProcessId -eq [Int64]$right.clientProcessId
            sameCurrentSource = -not [string]::IsNullOrEmpty($left.currentSourceSha256) -and $left.currentSourceSha256 -ceq $right.currentSourceSha256
            sameProductSource = -not [string]::IsNullOrEmpty($left.productSourceSha256) -and $left.productSourceSha256 -ceq $right.productSourceSha256
            sameExecutable = -not [string]::IsNullOrEmpty($left.applicationSha256) -and $left.applicationSha256 -ceq $right.applicationSha256
            samePackageTree = -not [string]::IsNullOrEmpty($left.packageSha256) -and $left.packageSha256 -ceq $right.packageSha256
            sameFixtureWorkspace = -not [string]::IsNullOrEmpty($left.fixtureWorkspaceSha256) -and $left.fixtureWorkspaceSha256 -ceq $right.fixtureWorkspaceSha256
            sameFixtureIdentity = -not [string]::IsNullOrEmpty($left.fixtureIdentityJson) -and $left.fixtureIdentityJson -ceq $right.fixtureIdentityJson
            sameFixturePreparer = -not [string]::IsNullOrEmpty($left.fixturePreparerSha256) -and $left.fixturePreparerSha256 -ceq $right.fixturePreparerSha256
            sameUiAutomationActions = -not [string]::IsNullOrEmpty($left.actionSignatureJson) -and $left.actionSignatureJson -ceq $right.actionSignatureJson
            sameFullObserverRosterContract = $left.observerMode -ceq 'full-observer' -and $right.observerMode -ceq 'full-observer' -and [Int64]$left.measuredCycleCount -eq 5 -and [Int64]$right.measuredCycleCount -eq 5 -and [Int64]$left.rosterRows -eq 100 -and [Int64]$right.rosterRows -eq 100 -and [Int64]$left.rosterColumns -eq 156 -and [Int64]$right.rosterColumns -eq 156
            bothArmMetadataValid = $left.metadataValid -and $right.metadataValid
            bothClientMetricsCaptured = $left.metricsCaptured -and $right.metricsCaptured
            bothArmsHaveFiveBoundaryEvents = $left.eventCount -eq 5 -and $right.eventCount -eq 5
            noDisallowedCollectionRequested = -not [bool]$comparison.appGcInvoked -and -not [bool]$comparison.osWorkingSetTrimInvoked -and -not [bool]$comparison.heapCompactInvoked
        }
        $comparison.comparisonChecks = $checks
        $comparisonPassed = $true
        foreach ($check in $checks.GetEnumerator()) { if (-not [bool]$check.Value) { $comparisonPassed = $false } }
        if (-not $comparisonPassed) {
            $failedChecks = [Collections.Generic.List[string]]::new()
            foreach ($check in $checks.GetEnumerator()) { if (-not [bool]$check.Value) { $failedChecks.Add([string]$check.Key) } }
            $comparison.failure = [ordered]@{
                message = 'Client GC reference arms did not share the required exact product, fixture, action sequence, and diagnostic metadata.'
                failedChecks = $failedChecks.ToArray()
                armMetadataFailureReasons = @($comparison.arms | ForEach-Object { $_.metadataValidation.failureReasons })
            }
        }
    } else {
        $comparison.failure = [ordered]@{ message = 'Both client GC reference arm summaries were not available for paired comparison.' }
    }
    $comparison.status = if ($comparisonPassed) { 'diagnostic-only' } else { 'failed' }
    $comparison.completedAtUtc = Get-P914Utc
    Write-P914Json $manifestPath $comparison
    return [ordered]@{
        comparisonId = $comparisonId; evidenceDirectory = $comparisonRoot; manifestPath = $manifestPath
        armOrderSelection = $ClientGcReferenceDiagnosticOrder; armExecutionOrder = $armModes
        status = [string]$comparison.status; diagnosticOnly = $true; validation = $comparison.validation; failure = $comparison.failure
        arms = $comparison.arms.ToArray()
    }
}

function Invoke-P917UiaObserverComparison {
    if ($HeapSnapshotDiagnostic) { throw 'UIA observer comparison cannot be combined with the heap snapshot diagnostic.' }
    if (-not [string]::IsNullOrWhiteSpace($PostValidateManifestPath)) { throw 'UIA observer comparison cannot be combined with post-validation mode.' }
    if ($null -eq $UiaObserverDiagnosticOrderPlan) { throw 'UIA observer comparison has no validated arm order plan.' }
    $armModes = @($UiaObserverDiagnosticOrderPlan.modes)
    $evidenceRoot = if ([IO.Path]::IsPathRooted($UiaObserverDiagnosticEvidenceRoot)) { [IO.Path]::GetFullPath($UiaObserverDiagnosticEvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $UiaObserverDiagnosticEvidenceRoot)) }
    $repositoryPrefix = $RepositoryRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    if (-not $evidenceRoot.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'UIA observer diagnostic evidence root must be inside the repository.' }
    $comparisonId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
    $comparisonRoot = if ($DefaultProcessHeapDiagnostic) { $evidenceRoot } else { Join-Path $evidenceRoot $comparisonId }
    $manifestPath = Join-Path $comparisonRoot 'comparison-manifest.json'
    if (Test-Path -LiteralPath $manifestPath) { throw "Refusing to overwrite UIA observer comparison evidence: $manifestPath" }
    [void][IO.Directory]::CreateDirectory($comparisonRoot)
    $comparison = [ordered]@{
        schema = if ($DefaultProcessHeapDiagnostic) { 'classmngr-phase9-p9-17-default-process-heap-pair-v1' } else { 'classmngr-phase9-p9-17-uia-observer-comparison-v1' }; taskId = $TaskId
        comparisonId = $comparisonId; status = 'running'; diagnosticOnly = $true; accepted = $false
        acceptanceUse = if ($DefaultProcessHeapDiagnostic) { 'Default process heap summary diagnostic only; both arms are excluded from P9-17 acceptance.' } else { 'Diagnostic comparison only; both arms are excluded from P9-17 acceptance.' }
        sameExactCurrentProductAndFixtureRequired = $true
        armOrderSelection = $UiaObserverDiagnosticOrder; armExecutionOrder = $armModes
        observerModes = @('full-observer', 'reduced-observer')
        defaultProcessHeapDiagnostic = [bool]$DefaultProcessHeapDiagnostic
        measuredCyclesPerArm = 5; primingCyclesPerArm = 1; matchedBaselineWindowsPerArm = 5
        arms = [Collections.Generic.List[object]]::new(); comparisonChecks = $null; failure = $null
        createdAtUtc = Get-P914Utc; completedAtUtc = $null; validation = [ordered]@{ status = 'skipped'; diagnosticOnly = $true }
    }
    Write-P914Json $manifestPath $comparison
    $armManifests = [Collections.Generic.List[object]]::new()
    for ($armIndex = 0; $armIndex -lt $armModes.Count; $armIndex++) {
        $mode = [string]$armModes[$armIndex]
        $runEvidenceRoot = Join-Path $comparisonRoot $mode
        $armResult = $null
        try {
            $armResult = Invoke-P917PackagedLifecycle -ObserverMode $mode -DiagnosticOnly -RunEvidenceRoot $runEvidenceRoot -ComparisonGroupId $comparisonId -ComparisonArmOrderSelection $UiaObserverDiagnosticOrder -ComparisonArmPosition ($armIndex + 1) -EnableDefaultProcessHeapDiagnostic:$DefaultProcessHeapDiagnostic
        } catch {
            $armResult = [ordered]@{ status = 'failed'; diagnosticOnly = $true; observerMode = $mode; failure = [ordered]@{ message = $_.Exception.Message } }
        }
        $armManifest = $null
        $armManifestSha = $null
        $armManifestPath = Get-P917OptionalProperty $armResult 'manifestPath'
        if ($null -ne $armManifestPath -and (Test-Path -LiteralPath ([string]$armManifestPath) -PathType Leaf)) {
            $armManifestSha = Get-P914FileSha256 ([string]$armManifestPath)
            $armManifest = Read-P914Json ([string]$armManifestPath)
            $armManifests.Add($armManifest)
        }
        $armPackage = Get-P917OptionalProperty $armManifest 'packageProvenance'
        $armFixture = Get-P917OptionalProperty $armManifest 'fixture'
        $armProcess = Get-P917OptionalProperty $armManifest 'process'
        $armObserver = Get-P917OptionalProperty $armManifest 'observerDiagnostic'
        $armHeapDiagnostic = Get-P917OptionalProperty $armManifest 'defaultProcessHeapDiagnostic'
        $armPreparation = Get-P917OptionalProperty $armFixture 'preparationReceipt'
        $armRunId = Get-P917OptionalProperty $armResult 'runId'
        $armDirectory = Get-P917OptionalProperty $armResult 'evidenceDirectory'
        $armFailure = Get-P917OptionalProperty $armResult 'failure'
        $armStatus = Get-P917OptionalProperty $armResult 'status'
        $armDiagnosticOnly = Get-P917OptionalProperty $armResult 'diagnosticOnly'
        $armRecord = [ordered]@{
            mode = $mode; armPosition = ($armIndex + 1); armOrderSelection = $UiaObserverDiagnosticOrder
            status = [string]$armStatus; diagnosticOnly = [bool]$armDiagnosticOnly
            runId = if ($null -ne $armRunId) { [string]$armRunId } else { $null }
            evidenceDirectory = if ($null -ne $armDirectory) { [string]$armDirectory } else { $null }
            manifestPath = if ($null -ne $armManifestPath) { [string]$armManifestPath } else { $null }
            manifestSha256 = $armManifestSha; processId = if ($null -ne $armProcess) { [Int64]$armProcess.processId } else { $null }
            applicationSha256 = Get-P917OptionalProperty $armPackage 'applicationSha256'
            packageSha256 = Get-P917OptionalProperty $armPackage 'packageSha256'
            productSourceSha256 = Get-P917OptionalProperty $armPackage 'productSourceSha256'
            currentSourceSha256 = Get-P917OptionalProperty $armPackage 'currentSourceSha256'
            fixtureWorkspaceSha256 = Get-P917OptionalProperty $armPreparation 'workspaceSha256'
            fixturePreparerSha256 = Get-P917OptionalProperty $armFixture 'fixturePreparerSha256'
            automationClientMetrics = Get-P917OptionalProperty $armObserver 'automationClientMetrics'
            defaultProcessHeapEventCount = @(Get-P917OptionalProperty $armHeapDiagnostic 'events').Count
            defaultProcessHeapValidationStatus = Get-P917OptionalProperty (Get-P917OptionalProperty $armHeapDiagnostic 'validation') 'status'
            failure = $armFailure
        }
        $comparison.arms.Add($armRecord)
        Write-P914Json $manifestPath $comparison
    }

    $comparisonPassed = $false
    if ($armManifests.Count -eq 2) {
        $left = $armManifests[0]; $right = $armManifests[1]
        $leftPackage = Get-P917OptionalProperty $left 'packageProvenance'; $rightPackage = Get-P917OptionalProperty $right 'packageProvenance'
        $leftFixture = Get-P917OptionalProperty $left 'fixture'; $rightFixture = Get-P917OptionalProperty $right 'fixture'
        $leftPreparation = Get-P917OptionalProperty $leftFixture 'preparationReceipt'; $rightPreparation = Get-P917OptionalProperty $rightFixture 'preparationReceipt'
        $leftIdentityValue = Get-P917OptionalProperty $leftPreparation 'identity'; $rightIdentityValue = Get-P917OptionalProperty $rightPreparation 'identity'
        $leftIdentity = ConvertTo-Json -InputObject $leftIdentityValue -Depth 20 -Compress
        $rightIdentity = ConvertTo-Json -InputObject $rightIdentityValue -Depth 20 -Compress
        $leftObserver = Get-P917OptionalProperty $left 'observerDiagnostic'; $rightObserver = Get-P917OptionalProperty $right 'observerDiagnostic'
        $leftHeapDiagnostic = Get-P917OptionalProperty $left 'defaultProcessHeapDiagnostic'; $rightHeapDiagnostic = Get-P917OptionalProperty $right 'defaultProcessHeapDiagnostic'
        $leftProcess = Get-P917OptionalProperty $left 'process'; $rightProcess = Get-P917OptionalProperty $right 'process'
        $leftCurrentSource = [string](Get-P917OptionalProperty $leftPackage 'currentSourceSha256'); $rightCurrentSource = [string](Get-P917OptionalProperty $rightPackage 'currentSourceSha256')
        $leftProductSource = [string](Get-P917OptionalProperty $leftPackage 'productSourceSha256'); $rightProductSource = [string](Get-P917OptionalProperty $rightPackage 'productSourceSha256')
        $leftExecutable = [string](Get-P917OptionalProperty $leftPackage 'applicationSha256'); $rightExecutable = [string](Get-P917OptionalProperty $rightPackage 'applicationSha256')
        $leftPackageTree = [string](Get-P917OptionalProperty $leftPackage 'packageSha256'); $rightPackageTree = [string](Get-P917OptionalProperty $rightPackage 'packageSha256')
        $leftWorkspace = [string](Get-P917OptionalProperty $leftPreparation 'workspaceSha256'); $rightWorkspace = [string](Get-P917OptionalProperty $rightPreparation 'workspaceSha256')
        $leftPreparer = [string](Get-P917OptionalProperty $leftFixture 'fixturePreparerSha256'); $rightPreparer = [string](Get-P917OptionalProperty $rightFixture 'fixturePreparerSha256')
        $leftClientMetrics = Get-P917OptionalProperty $leftObserver 'automationClientMetrics'; $rightClientMetrics = Get-P917OptionalProperty $rightObserver 'automationClientMetrics'
        $leftActionsJson = ConvertTo-Json -InputObject (Get-P917ClientGcReferenceUiActionSignature $left) -Depth 40 -Compress
        $rightActionsJson = ConvertTo-Json -InputObject (Get-P917ClientGcReferenceUiActionSignature $right) -Depth 40 -Compress
        $leftReadbacksJson = ConvertTo-Json -InputObject (Get-P917DefaultHeapDiagnosticReadbackSignature $left) -Depth 40 -Compress
        $rightReadbacksJson = ConvertTo-Json -InputObject (Get-P917DefaultHeapDiagnosticReadbackSignature $right) -Depth 40 -Compress
        $leftHeapEventsJson = ConvertTo-Json -InputObject (Get-P917DefaultHeapCheckpointSignature $left) -Depth 40 -Compress
        $rightHeapEventsJson = ConvertTo-Json -InputObject (Get-P917DefaultHeapCheckpointSignature $right) -Depth 40 -Compress
        $leftProcessId = Get-P917OptionalProperty $leftProcess 'processId'; $rightProcessId = Get-P917OptionalProperty $rightProcess 'processId'
        $observedArmModes = @($comparison.arms | ForEach-Object { [string]$_.mode })
        $observedArmModesJson = ConvertTo-Json -InputObject $observedArmModes -Compress
        $expectedArmModesJson = ConvertTo-Json -InputObject $armModes -Compress
        $checks = [ordered]@{
            selectedArmOrderMatches = [string]$comparison.armOrderSelection -ceq $UiaObserverDiagnosticOrder -and
                [string]$leftObserver.armOrderSelection -ceq $UiaObserverDiagnosticOrder -and
                [string]$rightObserver.armOrderSelection -ceq $UiaObserverDiagnosticOrder -and
                [Int64]$leftObserver.armPosition -eq 1 -and [Int64]$rightObserver.armPosition -eq 2 -and
                $expectedArmModesJson -ceq $observedArmModesJson -and
                [string]$comparison.arms[0].mode -ceq [string]$armModes[0] -and
                [string]$comparison.arms[1].mode -ceq [string]$armModes[1]
            bothArmsDiagnosticOnly = [bool]$left.diagnosticOnly -and [bool]$right.diagnosticOnly -and [string]$left.status -ceq 'diagnostic-only' -and [string]$right.status -ceq 'diagnostic-only'
            distinctPackagedProcesses = [Int64]$leftProcessId -gt 0 -and [Int64]$rightProcessId -gt 0 -and [Int64]$leftProcessId -ne [Int64]$rightProcessId
            sameCurrentSource = -not [string]::IsNullOrEmpty($leftCurrentSource) -and $leftCurrentSource -ceq $rightCurrentSource
            sameProductSource = -not [string]::IsNullOrEmpty($leftProductSource) -and $leftProductSource -ceq $rightProductSource
            sameExecutable = -not [string]::IsNullOrEmpty($leftExecutable) -and $leftExecutable -ceq $rightExecutable
            samePackageTree = -not [string]::IsNullOrEmpty($leftPackageTree) -and $leftPackageTree -ceq $rightPackageTree
            sameFixtureWorkspace = -not [string]::IsNullOrEmpty($leftWorkspace) -and $leftWorkspace -ceq $rightWorkspace
            sameFixtureIdentity = $null -ne $leftIdentityValue -and $leftIdentity -ceq $rightIdentity
            sameFixturePreparer = -not [string]::IsNullOrEmpty($leftPreparer) -and $leftPreparer -ceq $rightPreparer
            bothAutomationClientMetricsCaptured = $null -ne $leftClientMetrics -and $null -ne $rightClientMetrics
        }
        if ($DefaultProcessHeapDiagnostic) {
            $checks.sameUiActionSequence = $leftActionsJson -ceq $rightActionsJson
            $checks.sameRosterReadbacks = $leftReadbacksJson -ceq $rightReadbacksJson
            $checks.bothHeapCheckpointStreamsValidated =
                [string](Get-P917OptionalProperty (Get-P917OptionalProperty $leftHeapDiagnostic 'validation') 'status') -ceq 'passed' -and
                [string](Get-P917OptionalProperty (Get-P917OptionalProperty $rightHeapDiagnostic 'validation') 'status') -ceq 'passed'
            $checks.sameHeapCheckpointSequenceAndCellCoordinates =
                -not [string]::IsNullOrEmpty($leftHeapEventsJson) -and $leftHeapEventsJson -ceq $rightHeapEventsJson
        }
        $comparison.comparisonChecks = $checks
        $comparisonPassed = $true
        foreach ($check in $checks.GetEnumerator()) { if (-not [bool]$check.Value) { $comparisonPassed = $false } }
        if (-not $comparisonPassed) {
            $failedChecks = [Collections.Generic.List[string]]::new()
            foreach ($check in $checks.GetEnumerator()) { if (-not [bool]$check.Value) { $failedChecks.Add([string]$check.Key) } }
            $comparison.failure = [ordered]@{ message = 'Observer arms did not share the exact product/fixture or diagnostic evidence was incomplete.'; failedChecks = $failedChecks.ToArray() }
        }
    } else {
        $comparison.failure = [ordered]@{ message = 'Both observer arm manifests were not available for paired comparison.' }
    }
    $comparison.status = if ($comparisonPassed) { 'diagnostic-only' } else { 'failed' }
    $comparison.completedAtUtc = Get-P914Utc
    Write-P914Json $manifestPath $comparison
    return [ordered]@{
        comparisonId = $comparisonId; evidenceDirectory = $comparisonRoot; manifestPath = $manifestPath
        armOrderSelection = $UiaObserverDiagnosticOrder; armExecutionOrder = $armModes
        status = [string]$comparison.status; diagnosticOnly = $true; validation = $comparison.validation; failure = $comparison.failure
        arms = $comparison.arms.ToArray()
    }
}

function Invoke-P917PayloadManifestComparator([string]$EvidenceDirectory, [string]$FilledManifestPath, [string]$SparseManifestPath) {
    $scriptPath = Join-Path $PSScriptRoot 'validate_packaged_next_p9_17_large_roster.py'
    $outputPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'payload-comparator-output.txt'
    $lines = @(& python $scriptPath --compare-payload-diagnostic --filled-manifest $FilledManifestPath --sparse-manifest $SparseManifestPath 2>&1 | ForEach-Object { [string]$_ })
    $exitCode = $LASTEXITCODE
    Write-P917File $outputPath $lines
    if ($exitCode -ne 0) { throw "P9-17 payload comparator failed with exit code $exitCode. See $outputPath" }
    if ($lines.Count -eq 0) { throw "P9-17 payload comparator produced no report. See $outputPath" }
    return [ordered]@{
        report = (($lines | Select-Object -Last 1) | ConvertFrom-Json -ErrorAction Stop)
        outputPath = 'logs/payload-comparator-output.txt'
        outputSha256 = Get-P914FileSha256 $outputPath
        comparatorPath = [IO.Path]::GetFullPath($scriptPath)
        comparatorSha256 = Get-P914FileSha256 $scriptPath
    }
}

function Invoke-P917RosterRecordCountManifestComparator([string]$EvidenceDirectory, [string]$FilledManifestPath, [string]$ReducedRowsManifestPath) {
    $scriptPath = Join-Path $PSScriptRoot 'validate_packaged_next_p9_17_large_roster.py'
    $outputPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'roster-record-count-comparator-output.txt'
    $lines = @(& python $scriptPath --compare-roster-record-count-diagnostic --filled-manifest $FilledManifestPath --reduced-rows-manifest $ReducedRowsManifestPath 2>&1 | ForEach-Object { [string]$_ })
    $exitCode = $LASTEXITCODE
    Write-P917File $outputPath $lines
    if ($exitCode -ne 0) { throw "P9-17 roster record-count comparator failed with exit code $exitCode. See $outputPath" }
    if ($lines.Count -eq 0) { throw "P9-17 roster record-count comparator produced no report. See $outputPath" }
    return [ordered]@{
        report = (($lines | Select-Object -Last 1) | ConvertFrom-Json -ErrorAction Stop)
        outputPath = 'logs/roster-record-count-comparator-output.txt'
        outputSha256 = Get-P914FileSha256 $outputPath
        comparatorPath = [IO.Path]::GetFullPath($scriptPath)
        comparatorSha256 = Get-P914FileSha256 $scriptPath
    }
}

function Invoke-P917RosterRecordCountDiagnosticComparison {
    if (-not $RosterRecordCountDiagnostic) { throw 'Roster record-count comparison requires -RosterRecordCountDiagnostic.' }
    if ($null -eq $RosterRecordCountDiagnosticOrderPlan) { throw 'Roster record-count comparison has no fixed filled-then-reduced-rows order.' }
    $modes = @($RosterRecordCountDiagnosticOrderPlan.modes)
    $evidenceRoot = if ([IO.Path]::IsPathRooted($RosterRecordCountDiagnosticEvidenceRoot)) {
        [IO.Path]::GetFullPath($RosterRecordCountDiagnosticEvidenceRoot)
    } else {
        [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $RosterRecordCountDiagnosticEvidenceRoot))
    }
    $repositoryPrefix = $RepositoryRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    if (-not $evidenceRoot.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Roster record-count diagnostic evidence root must be inside the repository.'
    }
    $comparisonId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
    $comparisonRoot = Join-Path $evidenceRoot $comparisonId
    $manifestPath = Join-Path $comparisonRoot 'comparison-manifest.json'
    if (Test-Path -LiteralPath $comparisonRoot) { throw "Refusing to overwrite roster record-count evidence: $comparisonRoot" }
    [void][IO.Directory]::CreateDirectory($comparisonRoot)
    [void][IO.Directory]::CreateDirectory((Join-Path $comparisonRoot 'logs'))
    $comparison = [ordered]@{
        schema = 'classmngr-phase9-p9-17-roster-record-count-comparison-v1'; taskId = $TaskId
        comparisonId = $comparisonId; status = 'running'; diagnosticOnly = $true; accepted = $false
        acceptanceUse = 'Filled versus reduced custom-record cardinality comparison only; both arms and this comparison are excluded from P9-17 acceptance.'
        armOrderSelection = 'FilledThenReducedRows'; armExecutionOrder = $modes
        observerMode = 'full-observer'; measuredCyclesPerArm = 10; primingCyclesPerArm = 1
        matchedBaselineWindowsPerArm = 5; samplesPerMemoryWindow = 10; sampleIntervalMilliseconds = 100
        arms = [Collections.Generic.List[object]]::new(); comparisonChecks = $null; memoryComparison = $null; failure = $null
        createdAtUtc = Get-P914Utc; completedAtUtc = $null
    }
    Write-P914Json $manifestPath $comparison
    $armManifests = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
    $armManifestPaths = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::Ordinal)
    for ($armIndex = 0; $armIndex -lt $modes.Count; $armIndex++) {
        $mode = [string]$modes[$armIndex]
        $runEvidenceRoot = Join-Path $comparisonRoot ('arm-' + $mode)
        $armResult = $null
        try {
            $armResult = Invoke-P917PackagedLifecycle -ObserverMode 'full-observer' -RunEvidenceRoot $runEvidenceRoot -ComparisonGroupId $comparisonId -RecordCountDiagnosticMode $mode
        } catch {
            $armResult = [ordered]@{ status = 'failed'; diagnosticOnly = $true; observerMode = 'full-observer'; recordCountMode = $mode; failure = [ordered]@{ message = $_.Exception.Message } }
        }
        $armManifestPath = Get-P917OptionalProperty $armResult 'manifestPath'
        $armManifest = $null
        $armManifestSha = $null
        if ($null -ne $armManifestPath -and (Test-Path -LiteralPath ([string]$armManifestPath) -PathType Leaf)) {
            $armManifestSha = Get-P914FileSha256 ([string]$armManifestPath)
            $armManifest = Read-P914Json ([string]$armManifestPath)
            $armManifests[$mode] = $armManifest
            $armManifestPaths[$mode] = [string]$armManifestPath
        }
        $package = Get-P917OptionalProperty $armManifest 'packageProvenance'
        $fixture = Get-P917OptionalProperty $armManifest 'fixture'
        $preparation = Get-P917OptionalProperty $fixture 'preparationReceipt'
        $diagnostic = Get-P917OptionalProperty $armManifest 'recordCountDiagnostic'
        $process = Get-P917OptionalProperty $armManifest 'process'
        $comparison.arms.Add([ordered]@{
            mode = $mode; armPosition = ($armIndex + 1); status = [string](Get-P917OptionalProperty $armResult 'status')
            diagnosticOnly = [bool](Get-P917OptionalProperty $armResult 'diagnosticOnly')
            accepted = if ($null -ne $armManifest) { [bool](Get-P917OptionalProperty $armManifest 'accepted') } else { $false }
            runId = Get-P917OptionalProperty $armResult 'runId'
            evidenceDirectory = Get-P917OptionalProperty $armResult 'evidenceDirectory'
            manifestPath = $armManifestPath; manifestSha256 = $armManifestSha
            processId = Get-P917OptionalProperty $process 'processId'
            applicationSha256 = Get-P917OptionalProperty $package 'applicationSha256'
            productSourceSha256 = Get-P917OptionalProperty $package 'productSourceSha256'
            currentSourceSha256 = Get-P917OptionalProperty $package 'currentSourceSha256'
            packageSha256 = Get-P917OptionalProperty $package 'packageSha256'
            resourceInventorySha256 = Get-P917OptionalProperty $package 'resourceInventorySha256'
            fixtureWorkspaceSha256 = Get-P917OptionalProperty $preparation 'workspaceSha256'
            fixturePreparerSha256 = Get-P917OptionalProperty $fixture 'fixturePreparerSha256'
            recordCountMode = Get-P917OptionalProperty $diagnostic 'recordCountMode'
            customCellRecordCount = Get-P917OptionalProperty $diagnostic 'customCellRecordCount'
            removedCustomCellRecordCount = Get-P917OptionalProperty $diagnostic 'removedCustomCellRecordCount'
            failure = Get-P917OptionalProperty $armResult 'failure'
        })
        Write-P914Json $manifestPath $comparison
    }
    if ($armManifests.ContainsKey('filled') -and $armManifests.ContainsKey('reduced-rows')) {
        try {
            $comparator = Invoke-P917RosterRecordCountManifestComparator $comparisonRoot $armManifestPaths['filled'] $armManifestPaths['reduced-rows']
            $report = $comparator.report
            $comparison.comparisonChecks = $report.comparisonChecks
            $comparison.memoryComparison = $report.memoryComparison
            $comparison.fixture = $report.fixture
            $comparison.comparatorEvidence = [ordered]@{
                outputPath = $comparator.outputPath; outputSha256 = $comparator.outputSha256
                comparatorPath = $comparator.comparatorPath; comparatorSha256 = $comparator.comparatorSha256
            }
            if ([string]$report.status -ceq 'diagnostic-only') {
                $comparison.status = 'diagnostic-only'
            } else {
                $comparison.status = 'failed'; $comparison.failure = $report.failure
            }
        } catch {
            $comparison.status = 'failed'
            $comparison.failure = [ordered]@{ message = $_.Exception.Message }
        }
    } else {
        $comparison.status = 'failed'
        $comparison.failure = [ordered]@{ message = 'Both filled and reduced-rows arm manifests are required for comparison.' }
    }
    $comparison.completedAtUtc = Get-P914Utc
    Write-P914Json $manifestPath $comparison
    return [ordered]@{
        comparisonId = $comparisonId; evidenceDirectory = $comparisonRoot; manifestPath = $manifestPath
        status = [string]$comparison.status; diagnosticOnly = $true; accepted = $false
        comparisonChecks = $comparison.comparisonChecks; failure = $comparison.failure; arms = $comparison.arms.ToArray()
    }
}

function Invoke-P917PayloadDiagnosticComparison {
    if (-not $PayloadDiagnostic) { throw 'Payload comparison requires -PayloadDiagnostic.' }
    if ($null -eq $PayloadDiagnosticOrderPlan) { throw 'Payload comparison has no fixed filled-then-sparse arm order.' }
    $modes = @($PayloadDiagnosticOrderPlan.modes)
    $evidenceRoot = if ([IO.Path]::IsPathRooted($PayloadDiagnosticEvidenceRoot)) {
        [IO.Path]::GetFullPath($PayloadDiagnosticEvidenceRoot)
    } else {
        [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PayloadDiagnosticEvidenceRoot))
    }
    $repositoryPrefix = $RepositoryRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    if (-not $evidenceRoot.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Payload diagnostic evidence root must be inside the repository.'
    }
    $comparisonId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
    $comparisonRoot = Join-Path $evidenceRoot $comparisonId
    $manifestPath = Join-Path $comparisonRoot 'comparison-manifest.json'
    if (Test-Path -LiteralPath $comparisonRoot) { throw "Refusing to overwrite payload diagnostic evidence: $comparisonRoot" }
    [void][IO.Directory]::CreateDirectory($comparisonRoot)
    [void][IO.Directory]::CreateDirectory((Join-Path $comparisonRoot 'logs'))
    $comparison = [ordered]@{
        schema = 'classmngr-phase9-p9-17-payload-diagnostic-comparison-v1'; taskId = $TaskId
        comparisonId = $comparisonId; status = 'running'; diagnosticOnly = $true; accepted = $false
        acceptanceUse = 'Filled versus sparse custom-cell text comparison only; both arms and this comparison are excluded from P9-17 acceptance.'
        armOrderSelection = 'FilledThenSparse'; armExecutionOrder = $modes
        observerMode = 'full-observer'; measuredCyclesPerArm = 10; primingCyclesPerArm = 1
        matchedBaselineWindowsPerArm = 5; samplesPerMemoryWindow = 10; sampleIntervalMilliseconds = 100
        arms = [Collections.Generic.List[object]]::new(); comparisonChecks = $null; memoryComparison = $null; failure = $null
        createdAtUtc = Get-P914Utc; completedAtUtc = $null
    }
    Write-P914Json $manifestPath $comparison
    $armManifests = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
    $armManifestPaths = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::Ordinal)
    for ($armIndex = 0; $armIndex -lt $modes.Count; $armIndex++) {
        $mode = [string]$modes[$armIndex]
        $runEvidenceRoot = Join-Path $comparisonRoot ('arm-' + $mode)
        $armResult = $null
        try {
            $armResult = Invoke-P917PackagedLifecycle -ObserverMode 'full-observer' -RunEvidenceRoot $runEvidenceRoot -ComparisonGroupId $comparisonId -PayloadDiagnosticMode $mode
        } catch {
            $armResult = [ordered]@{ status = 'failed'; diagnosticOnly = $true; observerMode = 'full-observer'; payloadMode = $mode; failure = [ordered]@{ message = $_.Exception.Message } }
        }
        $armManifestPath = Get-P917OptionalProperty $armResult 'manifestPath'
        $armManifest = $null
        $armManifestSha = $null
        if ($null -ne $armManifestPath -and (Test-Path -LiteralPath ([string]$armManifestPath) -PathType Leaf)) {
            $armManifestSha = Get-P914FileSha256 ([string]$armManifestPath)
            $armManifest = Read-P914Json ([string]$armManifestPath)
            $armManifests[$mode] = $armManifest
            $armManifestPaths[$mode] = [string]$armManifestPath
        }
        $package = Get-P917OptionalProperty $armManifest 'packageProvenance'
        $fixture = Get-P917OptionalProperty $armManifest 'fixture'
        $preparation = Get-P917OptionalProperty $fixture 'preparationReceipt'
        $diagnostic = Get-P917OptionalProperty $armManifest 'payloadDiagnostic'
        $process = Get-P917OptionalProperty $armManifest 'process'
        $comparison.arms.Add([ordered]@{
            mode = $mode; armPosition = ($armIndex + 1); status = [string](Get-P917OptionalProperty $armResult 'status')
            diagnosticOnly = [bool](Get-P917OptionalProperty $armResult 'diagnosticOnly')
            accepted = if ($null -ne $armManifest) { [bool](Get-P917OptionalProperty $armManifest 'accepted') } else { $false }
            runId = Get-P917OptionalProperty $armResult 'runId'
            evidenceDirectory = Get-P917OptionalProperty $armResult 'evidenceDirectory'
            manifestPath = $armManifestPath; manifestSha256 = $armManifestSha
            processId = Get-P917OptionalProperty $process 'processId'
            applicationSha256 = Get-P917OptionalProperty $package 'applicationSha256'
            productSourceSha256 = Get-P917OptionalProperty $package 'productSourceSha256'
            currentSourceSha256 = Get-P917OptionalProperty $package 'currentSourceSha256'
            packageSha256 = Get-P917OptionalProperty $package 'packageSha256'
            resourceInventorySha256 = Get-P917OptionalProperty $package 'resourceInventorySha256'
            fixtureWorkspaceSha256 = Get-P917OptionalProperty $preparation 'workspaceSha256'
            fixturePreparerSha256 = Get-P917OptionalProperty $fixture 'fixturePreparerSha256'
            payloadMode = Get-P917OptionalProperty $diagnostic 'payloadMode'
            customCellRecordCount = Get-P917OptionalProperty $diagnostic 'customCellRecordCount'
            customCellValueCount = Get-P917OptionalProperty $diagnostic 'customCellValueCount'
            blankCustomCellValueCount = Get-P917OptionalProperty $diagnostic 'blankCustomCellValueCount'
            compactSentinelValue = Get-P917OptionalProperty $diagnostic 'compactSentinelValue'
            compactSentinelValueCount = Get-P917OptionalProperty $diagnostic 'compactSentinelValueCount'
            failure = Get-P917OptionalProperty $armResult 'failure'
        })
        Write-P914Json $manifestPath $comparison
    }

    if ($armManifests.ContainsKey('filled') -and $armManifests.ContainsKey('sparse')) {
        try {
            $comparator = Invoke-P917PayloadManifestComparator $comparisonRoot $armManifestPaths['filled'] $armManifestPaths['sparse']
            $report = $comparator.report
            $comparison.comparisonChecks = $report.comparisonChecks
            $comparison.memoryComparison = $report.memoryComparison
            $comparison.fixture = $report.fixture
            $comparison.comparatorEvidence = [ordered]@{
                outputPath = $comparator.outputPath; outputSha256 = $comparator.outputSha256
                comparatorPath = $comparator.comparatorPath; comparatorSha256 = $comparator.comparatorSha256
            }
            if ([string]$report.status -ceq 'diagnostic-only') {
                $comparison.status = 'diagnostic-only'
            } else {
                $comparison.status = 'failed'
                $comparison.failure = $report.failure
            }
        } catch {
            $comparison.status = 'failed'
            $comparison.failure = [ordered]@{ message = $_.Exception.Message }
        }
    } else {
        $comparison.status = 'failed'
        $comparison.failure = [ordered]@{ message = 'Both filled and sparse arm manifests are required for comparison.' }
    }
    $comparison.completedAtUtc = Get-P914Utc
    Write-P914Json $manifestPath $comparison
    return [ordered]@{
        comparisonId = $comparisonId; evidenceDirectory = $comparisonRoot; manifestPath = $manifestPath
        status = [string]$comparison.status; diagnosticOnly = $true; accepted = $false
        comparisonChecks = $comparison.comparisonChecks; failure = $comparison.failure; arms = $comparison.arms.ToArray()
    }
}

function Invoke-P917DefaultProcessHeapDiagnosticComparison {
    if (-not $DefaultProcessHeapDiagnostic) { throw 'Default process heap comparison requires -DefaultProcessHeapDiagnostic.' }
    if ($HeapSnapshotDiagnostic -or $UiaObserverDiagnostic -or $ClientGcReferenceDiagnostic -or
        -not [string]::IsNullOrWhiteSpace($PostValidateManifestPath)) {
        throw 'Default process heap comparison cannot be combined with another diagnostic or post-validation mode.'
    }
    $evidenceRoot = if ([IO.Path]::IsPathRooted($DefaultProcessHeapDiagnosticEvidenceRoot)) {
        [IO.Path]::GetFullPath($DefaultProcessHeapDiagnosticEvidenceRoot)
    } else {
        [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $DefaultProcessHeapDiagnosticEvidenceRoot))
    }
    $repositoryPrefix = $RepositoryRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    if (-not $evidenceRoot.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Default process heap diagnostic evidence root must be inside the repository.'
    }
    $comparisonId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
    $comparisonRoot = Join-Path $evidenceRoot $comparisonId
    if (Test-Path -LiteralPath $comparisonRoot) { throw "Refusing to overwrite default process heap evidence: $comparisonRoot" }
    [void][IO.Directory]::CreateDirectory($comparisonRoot)
    $manifestPath = Join-Path $comparisonRoot 'comparison-manifest.json'
    $orders = @('FullThenReduced', 'ReducedThenFull')
    $comparison = [ordered]@{
        schema = 'classmngr-phase9-p9-17-default-process-heap-four-arm-comparison-v1'
        taskId = $TaskId
        comparisonId = $comparisonId
        status = 'running'
        diagnosticOnly = $true
        accepted = $false
        acceptanceUse = 'In-process default-heap diagnostic only; all four full/reduced observer arms are excluded from P9-17 acceptance.'
        applicationHeapScope = 'default-process-heap'
        applicationEnvironmentVariable = 'CLASSMNGR_P9_17_DEFAULT_PROCESS_HEAP_DIAGNOSTIC'
        counterbalancedOrders = $orders
        observerModes = @('full-observer', 'reduced-observer')
        measuredCyclesPerArm = 5
        primingCyclesPerArm = 1
        matchedBaselineWindowsPerArm = 5
        sameExactCurrentProductFixtureActionsAndReadbacksRequired = $true
        pairComparisons = [Collections.Generic.List[object]]::new()
        arms = [Collections.Generic.List[object]]::new()
        comparisonChecks = $null
        failure = $null
        createdAtUtc = Get-P914Utc
        completedAtUtc = $null
    }
    Write-P914Json $manifestPath $comparison

    foreach ($order in $orders) {
        $UiaObserverDiagnosticOrderPlan = Get-P917UiaObserverOrderPlan $order
        $UiaObserverDiagnosticOrder = [string]$UiaObserverDiagnosticOrderPlan.selection
        $orderEvidenceRoot = Join-Path $comparisonRoot $order
        $UiaObserverDiagnosticEvidenceRoot = $orderEvidenceRoot
        $pairResult = $null
        try {
            $pairResult = Invoke-P917UiaObserverComparison
        } catch {
            $pairResult = [ordered]@{
                status = 'failed'
                diagnosticOnly = $true
                armOrderSelection = $order
                failure = [ordered]@{ message = $_.Exception.Message }
                arms = @()
            }
        }
        $comparison.pairComparisons.Add([ordered]@{
            armOrderSelection = $order
            armExecutionOrder = $UiaObserverDiagnosticOrderPlan.modes
            comparisonId = Get-P917OptionalProperty $pairResult 'comparisonId'
            status = [string](Get-P917OptionalProperty $pairResult 'status')
            manifestPath = Get-P917OptionalProperty $pairResult 'manifestPath'
            failure = Get-P917OptionalProperty $pairResult 'failure'
        })
        foreach ($pairArm in @(Get-P917OptionalProperty $pairResult 'arms')) {
            $armManifestPath = Get-P917OptionalProperty $pairArm 'manifestPath'
            $armManifest = $null
            if ($null -ne $armManifestPath -and (Test-Path -LiteralPath ([string]$armManifestPath) -PathType Leaf)) {
                $armManifest = Read-P914Json ([string]$armManifestPath)
            }
            $package = Get-P917OptionalProperty $armManifest 'packageProvenance'
            $fixture = Get-P917OptionalProperty $armManifest 'fixture'
            $preparation = Get-P917OptionalProperty $fixture 'preparationReceipt'
            $process = Get-P917OptionalProperty $armManifest 'process'
            $heapDiagnostic = Get-P917OptionalProperty $armManifest 'defaultProcessHeapDiagnostic'
            $heapValidation = Get-P917OptionalProperty $heapDiagnostic 'validation'
            $comparison.arms.Add([ordered]@{
                armOrderSelection = $order
                mode = [string](Get-P917OptionalProperty $pairArm 'mode')
                armPosition = Get-P917OptionalProperty $pairArm 'armPosition'
                status = [string](Get-P917OptionalProperty $pairArm 'status')
                diagnosticOnly = [bool](Get-P917OptionalProperty $pairArm 'diagnosticOnly')
                runId = Get-P917OptionalProperty $pairArm 'runId'
                evidenceDirectory = Get-P917OptionalProperty $pairArm 'evidenceDirectory'
                manifestPath = $armManifestPath
                manifestSha256 = if ($null -ne $armManifestPath -and (Test-Path -LiteralPath ([string]$armManifestPath) -PathType Leaf)) { Get-P914FileSha256 ([string]$armManifestPath) } else { $null }
                processId = Get-P917OptionalProperty $process 'processId'
                applicationSha256 = Get-P917OptionalProperty $package 'applicationSha256'
                packageSha256 = Get-P917OptionalProperty $package 'packageSha256'
                productSourceSha256 = Get-P917OptionalProperty $package 'productSourceSha256'
                currentSourceSha256 = Get-P917OptionalProperty $package 'currentSourceSha256'
                fixtureWorkspaceSha256 = Get-P917OptionalProperty $preparation 'workspaceSha256'
                fixturePreparerSha256 = Get-P917OptionalProperty $fixture 'fixturePreparerSha256'
                defaultProcessHeapEventCount = @(Get-P917OptionalProperty $heapDiagnostic 'events').Count
                defaultProcessHeapValidationStatus = Get-P917OptionalProperty $heapValidation 'status'
                failure = Get-P917OptionalProperty $pairArm 'failure'
            })
        }
        Write-P914Json $manifestPath $comparison
    }

    $armManifests = [Collections.Generic.List[object]]::new()
    $armPaths = [Collections.Generic.List[string]]::new()
    foreach ($arm in $comparison.arms) {
        $path = [string]$arm.manifestPath
        if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path -LiteralPath $path -PathType Leaf)) {
            $armPaths.Add($path)
            $armManifests.Add((Read-P914Json $path))
        }
    }
    $comparisonPassed = $false
    if ($armManifests.Count -eq 4) {
        $actionSignatures = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $readbackSignatures = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $heapCheckpointSignatures = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $currentSourceHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $productSourceHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $applicationHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $packageHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $workspaceHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $preparerHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $processIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $heapStreamsValidated = $true
        $fiveCycleFullRoster = $true
        $allDiagnosticOnly = $true
        for ($armIndex = 0; $armIndex -lt 4; $armIndex++) {
            $armManifest = $armManifests[$armIndex]
            $package = Get-P917OptionalProperty $armManifest 'packageProvenance'
            $fixture = Get-P917OptionalProperty $armManifest 'fixture'
            $preparation = Get-P917OptionalProperty $fixture 'preparationReceipt'
            $process = Get-P917OptionalProperty $armManifest 'process'
            $route = Get-P917OptionalProperty $armManifest 'route'
            $modelCounts = Get-P917OptionalProperty $route 'modelCounts'
            $heapDiagnostic = Get-P917OptionalProperty $armManifest 'defaultProcessHeapDiagnostic'
            $heapValidation = Get-P917OptionalProperty $heapDiagnostic 'validation'
            $actionJson = ConvertTo-Json -InputObject (Get-P917ClientGcReferenceUiActionSignature $armManifest) -Depth 40 -Compress
            $readbackJson = ConvertTo-Json -InputObject (Get-P917DefaultHeapDiagnosticReadbackSignature $armManifest) -Depth 40 -Compress
            $heapEventJson = ConvertTo-Json -InputObject (Get-P917DefaultHeapCheckpointSignature $armManifest) -Depth 40 -Compress
            [void]$actionSignatures.Add($actionJson)
            [void]$readbackSignatures.Add($readbackJson)
            [void]$heapCheckpointSignatures.Add($heapEventJson)
            [void]$currentSourceHashes.Add([string](Get-P917OptionalProperty $package 'currentSourceSha256'))
            [void]$productSourceHashes.Add([string](Get-P917OptionalProperty $package 'productSourceSha256'))
            [void]$applicationHashes.Add([string](Get-P917OptionalProperty $package 'applicationSha256'))
            [void]$packageHashes.Add([string](Get-P917OptionalProperty $package 'packageSha256'))
            [void]$workspaceHashes.Add([string](Get-P917OptionalProperty $preparation 'workspaceSha256'))
            [void]$preparerHashes.Add([string](Get-P917OptionalProperty $fixture 'fixturePreparerSha256'))
            [void]$processIds.Add([string](Get-P917OptionalProperty $process 'processId'))
            $heapStreamsValidated = $heapStreamsValidated -and
                [string](Get-P917OptionalProperty $heapValidation 'status') -ceq 'passed' -and
                [bool](Get-P917OptionalProperty $heapDiagnostic 'enabled') -and
                [string](Get-P917OptionalProperty $heapDiagnostic 'environmentValue') -ceq '1'
            $fiveCycleFullRoster = $fiveCycleFullRoster -and
                [Int64](Get-P917OptionalProperty $route 'measuredCycleCount') -eq 5 -and
                [Int64](Get-P917OptionalProperty $modelCounts 'rosterRows') -eq 100 -and
                [Int64](Get-P917OptionalProperty $modelCounts 'rosterColumns') -eq 156
            $allDiagnosticOnly = $allDiagnosticOnly -and
                [bool](Get-P917OptionalProperty $armManifest 'diagnosticOnly') -and
                [string](Get-P917OptionalProperty $armManifest 'status') -ceq 'diagnostic-only'
        }
        $orderList = @($comparison.arms | ForEach-Object { [string]$_.armOrderSelection } | Select-Object -Unique)
        $modeOrderValid = @($comparison.arms | Where-Object { [string]$_.armOrderSelection -ceq 'FullThenReduced' -and [string]$_.mode -ceq 'full-observer' -and [Int64]$_.armPosition -eq 1 }).Count -eq 1 -and
            @($comparison.arms | Where-Object { [string]$_.armOrderSelection -ceq 'FullThenReduced' -and [string]$_.mode -ceq 'reduced-observer' -and [Int64]$_.armPosition -eq 2 }).Count -eq 1 -and
            @($comparison.arms | Where-Object { [string]$_.armOrderSelection -ceq 'ReducedThenFull' -and [string]$_.mode -ceq 'reduced-observer' -and [Int64]$_.armPosition -eq 1 }).Count -eq 1 -and
            @($comparison.arms | Where-Object { [string]$_.armOrderSelection -ceq 'ReducedThenFull' -and [string]$_.mode -ceq 'full-observer' -and [Int64]$_.armPosition -eq 2 }).Count -eq 1
        $checks = [ordered]@{
            fourArmCount = $armManifests.Count -eq 4 -and $comparison.arms.Count -eq 4
            bothCounterbalancedOrdersPresent = $orderList.Count -eq 2 -and $orderList -contains 'FullThenReduced' -and $orderList -contains 'ReducedThenFull' -and $modeOrderValid
            allArmsDiagnosticOnly = $allDiagnosticOnly
            distinctPackagedProcesses = $processIds.Count -eq 4 -and -not $processIds.Contains('')
            sameCurrentSource = $currentSourceHashes.Count -eq 1 -and -not $currentSourceHashes.Contains('')
            sameProductSource = $productSourceHashes.Count -eq 1 -and -not $productSourceHashes.Contains('')
            sameExecutable = $applicationHashes.Count -eq 1 -and -not $applicationHashes.Contains('')
            samePackageTree = $packageHashes.Count -eq 1 -and -not $packageHashes.Contains('')
            sameFixtureWorkspace = $workspaceHashes.Count -eq 1 -and -not $workspaceHashes.Contains('')
            sameFixturePreparer = $preparerHashes.Count -eq 1 -and -not $preparerHashes.Contains('')
            allHeapEventsAndSamplesValidated = $heapStreamsValidated
            sameActionsAcrossArms = $actionSignatures.Count -eq 1
            sameReadbacksAcrossArms = $readbackSignatures.Count -eq 1
            sameCheckpointOrderAndCoordinatesAcrossArms = $heapCheckpointSignatures.Count -eq 1
            fiveMeasuredCyclesPerArmAt100By156 = $fiveCycleFullRoster
        }
        $comparison.comparisonChecks = $checks
        $comparisonPassed = $true
        foreach ($check in $checks.GetEnumerator()) { if (-not [bool]$check.Value) { $comparisonPassed = $false } }
        if (-not $comparisonPassed) {
            $failedChecks = [Collections.Generic.List[string]]::new()
            foreach ($check in $checks.GetEnumerator()) { if (-not [bool]$check.Value) { $failedChecks.Add([string]$check.Key) } }
            $comparison.failure = [ordered]@{
                message = 'Four default-heap diagnostic arms did not preserve exact action/readback/source/package/fixture parity and complete app checkpoint evidence.'
                failedChecks = $failedChecks.ToArray()
            }
        }
    } else {
        $comparison.failure = [ordered]@{ message = 'All four counterbalanced arm manifests were not available.' }
    }
    $comparison.status = if ($comparisonPassed) { 'diagnostic-only' } else { 'failed' }
    $comparison.completedAtUtc = Get-P914Utc
    Write-P914Json $manifestPath $comparison
    return [ordered]@{
        comparisonId = $comparisonId
        evidenceDirectory = $comparisonRoot
        manifestPath = $manifestPath
        status = [string]$comparison.status
        diagnosticOnly = $true
        counterbalancedOrders = $orders
        arms = $comparison.arms.ToArray()
        comparisonChecks = $comparison.comparisonChecks
        failure = $comparison.failure
    }
}

if ($PayloadDiagnostic) {
    $result = Invoke-P917PayloadDiagnosticComparison
} elseif ($ClientGcReferenceDiagnostic) {
    $result = Invoke-P917ClientGcReferenceComparison
} elseif ($DefaultProcessHeapDiagnostic) {
    $result = Invoke-P917DefaultProcessHeapDiagnosticComparison
} elseif ($UiaObserverDiagnostic) {
    $result = Invoke-P917UiaObserverComparison
} elseif (-not [string]::IsNullOrWhiteSpace($PostValidateManifestPath)) {
    if ($HeapSnapshotDiagnostic) { throw 'Heap snapshot diagnostic mode cannot be combined with post-validation mode.' }
    $postValidationManifestPath = [IO.Path]::GetFullPath($PostValidateManifestPath)
    $repositoryPrefix = $RepositoryRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    if (-not $postValidationManifestPath.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Post-validation manifest must be inside the repository root.' }
    if (-not (Test-Path -LiteralPath $postValidationManifestPath -PathType Leaf)) { throw "Post-validation manifest is missing: $postValidationManifestPath" }
    $postValidationManifest = Read-P914Json $postValidationManifestPath
    if ([string]$postValidationManifest.taskId -cne $TaskId) { throw 'Post-validation manifest does not belong to P9-17.' }
    if ([string]$postValidationManifest.status -cne 'completed' -and -not ([string]$postValidationManifest.status -ceq 'failed' -and [string]$postValidationManifest.failure.category -ceq 'validation')) { throw 'Post-validation accepts only a completed route or a prior validation failure.' }
    if ([string]$postValidationManifest.status -ceq 'failed') {
        # A prior validation attempt may have changed the manifest status and
        # inventory. Restore the completed-route input expected by the Python
        # validator before replaying the validation-only stage.
        $postValidationManifest.status = 'completed'
        $postValidationManifest.failure = $null
        $postValidationManifest.validation = $null
        $postValidationManifest.evidenceInventory = $null
        Write-P914Json $postValidationManifestPath $postValidationManifest
    }
    $postValidationResult = Invoke-P917ValidationPersistence (Split-Path -Parent $postValidationManifestPath) $postValidationManifestPath $postValidationManifest
    $result = [ordered]@{
        runId = [string]$postValidationManifest.runId; evidenceDirectory = Split-Path -Parent $postValidationManifestPath
        manifestPath = $postValidationManifestPath; status = [string]$postValidationResult.status
        failure = $postValidationResult.failure; validation = $postValidationResult.validation
    }
} else {
    $result = Invoke-P917PackagedLifecycle
}
$result | ConvertTo-Json -Depth 20
if ([string]$result.status -cne 'passed' -and -not ($HeapSnapshotDiagnostic -and [string]$result.status -ceq 'diagnostic-only') -and -not ($DefaultProcessHeapDiagnostic -and [string]$result.status -ceq 'diagnostic-only') -and -not ($UiaObserverDiagnostic -and [string]$result.status -ceq 'diagnostic-only') -and -not ($ClientGcReferenceDiagnostic -and [string]$result.status -ceq 'diagnostic-only') -and -not ($PayloadDiagnostic -and [string]$result.status -ceq 'diagnostic-only') -and -not ($RosterRecordCountDiagnostic -and [string]$result.status -ceq 'diagnostic-only')) { exit 1 }
