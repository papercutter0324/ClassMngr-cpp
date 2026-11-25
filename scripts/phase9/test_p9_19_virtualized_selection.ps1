$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'p9_19_virtualized_selection.ps1')

function New-TestNames { @(96..1 | ForEach-Object { 'Heavy Fixture Class {0:D3}' -f $_ }) }
function New-TestViewport([object]$State, [string[]]$Names, [int]$Step = 12) {
    $first = $State.page * $Step
    $last = [Math]::Min($Names.Count, $first + $Step)
    $items = [Collections.Generic.List[object]]::new()
    for ($i = $first; $i -lt $last; $i++) {
        $items.Add([ordered]@{ name = $Names[$i]; isVisible = $true; isEnabled = $true; controlType = 'ControlType.ListItem'; processId = 9876; ownerWindowHandle = [Int64]12345; ownerWindowProcessId = 9876; rectangle = [ordered]@{ x = 10; y = 10 + (($i - $first) * 10); width = 100; height = 9 } })
    }
    [ordered]@{ items = $items.ToArray(); visibleNames = @($items.ToArray() | ForEach-Object { [string]$_.name }); listBounds = [ordered]@{ x = 0; y = 0; width = 200; height = 200 }; verticalScrollPercent = [double]($State.page * 100 / 8); canScroll = ($State.page -lt 8); scrollPattern = [object]::new() }
}
function New-TestCallbacks([object]$State, [string[]]$Names, [switch]$NoProgress, [switch]$DuplicateVisible, [switch]$SkipClass13) {
    $read = { New-TestViewport $State $Names }.GetNewClosure()
    $scroll = {
        param($Viewport)
        if (-not $NoProgress) { $State.page += $(if ($SkipClass13 -and $State.page -eq 0) { 2 } else { 1 }) }
    }.GetNewClosure()
    $select = {
        param($Viewport, $Item, $ExpectedName)
        $State.selected.Add($ExpectedName)
        $id = [int]$ExpectedName.Substring($ExpectedName.Length - 3)
        [ordered]@{ selectedListItemName = $ExpectedName; classId = $id; selectionItemPatternIsSelected = $true }
    }.GetNewClosure()
    if ($DuplicateVisible) {
        $read = {
            $snapshot = New-TestViewport $State $Names
            $duplicate = [ordered]@{}
            foreach ($key in $snapshot.items[0].Keys) { $duplicate[$key] = $snapshot.items[0][$key] }
            $snapshot.items = @($snapshot.items) + @($duplicate)
            $snapshot.visibleNames = @($snapshot.visibleNames) + @([string]$duplicate.name)
            $snapshot
        }.GetNewClosure()
    }
    [ordered]@{ read = $read; scroll = $scroll; select = $select }
}

$names = New-TestNames
$state = [pscustomobject]@{ page = 0; selected = [Collections.Generic.List[string]]::new() }
$callbacks = New-TestCallbacks $state $names
$idByName = [Collections.Generic.Dictionary[string,int]]::new([StringComparer]::Ordinal)
for ($i = 0; $i -lt 96; $i++) { $idByName[$names[$i]] = [int]$names[$i].Substring($names[$i].Length - 3) }
$order = Get-P919VirtualizedDisplayOrder -ExpectedNames $names -ClassIdByName $idByName -ExpectedProcessId 9876 -ExpectedWindowHandle 12345 -ReadViewport $callbacks.read -ScrollViewport $callbacks.scroll
if ($order.classIds.Count -ne 96 -or [string]::Join(',', [string[]]$order.classIds) -cne [string]::Join(',', [string[]](96..1)) -or $order.scrollEvents.Count -ne 7 -or [string]::Join([char]10,[string[]]$order.accessibleNames) -cne [string]::Join([char]10,[string[]]$names)) { throw 'Viewport order discovery did not retain every class ID in deterministic display order.' }
$state.page = 0
$callbacks = New-TestCallbacks $state $names
$expectedIds = @($names | ForEach-Object { [int]$_.Substring($_.Length - 3) })
$result = Invoke-P919VirtualizedSelectionSweep -ExpectedNames $names -ExpectedClassIds $expectedIds -ExpectedProcessId 9876 -ExpectedWindowHandle 12345 -ReadViewport $callbacks.read -ScrollViewport $callbacks.scroll -SelectAndReadback $callbacks.select
if ($result.selectionCount -ne 96 -or $state.selected.Count -ne 96 -or @($state.selected | Select-Object -Unique).Count -ne 96 -or $result.scrollEvents.Count -ne 7) { throw 'Virtualized 96-class test did not traverse and select the full roster in bounded viewports.' }

$noProgressState = [pscustomobject]@{ page = 0; selected = [Collections.Generic.List[string]]::new() }
$noProgress = New-TestCallbacks $noProgressState $names -NoProgress
$threw = $false
try { Invoke-P919VirtualizedSelectionSweep -ExpectedNames $names -ExpectedClassIds $expectedIds -ExpectedProcessId 9876 -ExpectedWindowHandle 12345 -ReadViewport $noProgress.read -ScrollViewport $noProgress.scroll -SelectAndReadback $noProgress.select | Out-Null } catch { $threw = $_.Exception.Message -match 'no measurable scroll progress' }
if (-not $threw) { throw 'Virtualized UIA test did not fail closed when scrolling stopped.' }

$duplicateState = [pscustomobject]@{ page = 0; selected = [Collections.Generic.List[string]]::new() }
$duplicate = New-TestCallbacks $duplicateState $names -DuplicateVisible
$threw = $false
try { Invoke-P919VirtualizedSelectionSweep -ExpectedNames $names -ExpectedClassIds $expectedIds -ExpectedProcessId 9876 -ExpectedWindowHandle 12345 -ReadViewport $duplicate.read -ScrollViewport $duplicate.scroll -SelectAndReadback $duplicate.select | Out-Null } catch { $threw = $_.Exception.Message -match 'duplicate realized class name' }
if (-not $threw) { throw 'Virtualized UIA test did not fail closed for duplicate visible class rows.' }

$skipState = [pscustomobject]@{ page = 0; selected = [Collections.Generic.List[string]]::new() }
$skip = New-TestCallbacks $skipState $names -SkipClass13
$threw = $false
try { Invoke-P919VirtualizedSelectionSweep -ExpectedNames $names -ExpectedClassIds $expectedIds -ExpectedProcessId 9876 -ExpectedWindowHandle 12345 -ReadViewport $skip.read -ScrollViewport $skip.scroll -SelectAndReadback $skip.select | Out-Null } catch { $threw = $_.Exception.Message -match 'expected class|visible' }
if (-not $threw) { throw 'Virtualized UIA test did not fail closed for a missed fixture class.' }

Write-Output 'P9-19 virtualized selection tests passed (96 ordered readbacks, bounded progress, duplicate and missed row rejection).'
