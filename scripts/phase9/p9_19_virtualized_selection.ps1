Set-StrictMode -Version Latest

function Get-P919ViewportSignature([object]$Viewport) {
    $names = @($Viewport.visibleNames | ForEach-Object { [string]$_ })
    return ([string]::Join([char]10, [string[]]$names) + "|" + ([double]$Viewport.verticalScrollPercent).ToString("F4", [Globalization.CultureInfo]::InvariantCulture))
}

function Assert-P919ViewportSnapshot([object]$Viewport, [string[]]$ExpectedNames, [int]$ExpectedProcessId, [Int64]$ExpectedWindowHandle) {
    if ($null -eq $Viewport -or $null -eq $Viewport.items -or $null -eq $Viewport.visibleNames) { throw 'P9-19 viewport adapter returned an incomplete UIA snapshot.' }
    $expected = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in $ExpectedNames) { [void]$expected.Add([string]$name) }
    $allNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $visibleNames = [Collections.Generic.List[string]]::new()
    foreach ($item in @($Viewport.items)) {
        $name = [string]$item.name
        if (-not $expected.Contains($name)) { throw "P9-19 realized UIA item is outside the pinned class roster: $name" }
        if (-not $allNames.Add($name)) { throw "P9-19 UIA viewport contains duplicate realized class name: $name" }
        if ([string]$item.controlType -cne 'ControlType.ListItem' -or [Int64]$item.processId -ne [Int64]$ExpectedProcessId -or [Int64]$item.ownerWindowHandle -ne $ExpectedWindowHandle -or [Int64]$item.ownerWindowProcessId -ne [Int64]$ExpectedProcessId) { throw "P9-19 UIA item identity escaped the measured PID/HWND: $name" }
        if ([bool]$item.isVisible) {
            $rectangle = $item.rectangle
            if (-not [bool]$item.isEnabled -or [double]$rectangle.width -le 0 -or [double]$rectangle.height -le 0) { throw "P9-19 visible class item has no enabled visible bounds: $name" }
            $bounds = $Viewport.listBounds
            $intersectionLeft = [Math]::Max([double]$rectangle.x,[double]$bounds.x)
            $intersectionTop = [Math]::Max([double]$rectangle.y,[double]$bounds.y)
            $intersectionRight = [Math]::Min(([double]$rectangle.x + [double]$rectangle.width),([double]$bounds.x + [double]$bounds.width))
            $intersectionBottom = [Math]::Min(([double]$rectangle.y + [double]$rectangle.height),([double]$bounds.y + [double]$bounds.height))
            if ($intersectionRight -le $intersectionLeft -or $intersectionBottom -le $intersectionTop) { throw "P9-19 visible class item bounds do not intersect the exact Classes List: $name" }
            $visibleNames.Add($name)
        }
    }
    if ($visibleNames.Count -eq 0) { throw 'P9-19 UIA Classes List has no visible realized ListItems.' }
    if (@($visibleNames.ToArray() | Select-Object -Unique).Count -ne $visibleNames.Count) { throw 'P9-19 UIA Classes viewport repeats a visible class name.' }
    $recordedVisibleNames = @($Viewport.visibleNames | ForEach-Object { [string]$_ })
    if ([string]::Join([char]10, [string[]]$recordedVisibleNames) -cne [string]::Join([char]10, [string[]]$visibleNames.ToArray())) { throw 'P9-19 viewport adapter visible-name summary differs from the UIA items.' }
    if ([double]$Viewport.verticalScrollPercent -lt 0 -or [double]$Viewport.verticalScrollPercent -gt 100) { throw 'P9-19 UIA vertical scroll percent is outside [0,100].' }
}


function Get-P919VirtualizedDisplayOrder {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string[]]$ExpectedNames,
        [Parameter(Mandatory=$true)][object]$ClassIdByName,
        [Parameter(Mandatory=$true)][int]$ExpectedProcessId,
        [Parameter(Mandatory=$true)][Int64]$ExpectedWindowHandle,
        [Parameter(Mandatory=$true)][scriptblock]$ReadViewport,
        [Parameter(Mandatory=$true)][scriptblock]$ScrollViewport,
        [ValidateRange(1,100)][int]$MaximumScrolls = 40
    )
    if ($ExpectedNames.Count -ne 96 -or @($ExpectedNames | Select-Object -Unique).Count -ne 96) { throw 'P9-19 viewport discovery requires exactly 96 unique DB-derived accessible names.' }
    $expectedIds = [Collections.Generic.HashSet[int]]::new()
    foreach ($name in $ExpectedNames) {
        if (-not $ClassIdByName.ContainsKey([string]$name)) { throw "P9-19 DB-derived accessible-name map is missing $name" }
        [void]$expectedIds.Add([int]$ClassIdByName[[string]$name])
    }
    $observedIds = [string]::Join(',', [string[]]@($expectedIds | Sort-Object))
    $requiredIds = [string]::Join(',', [string[]](1..96))
    if ($expectedIds.Count -ne 96 -or $observedIds -cne $requiredIds) { throw 'P9-19 DB-derived accessible-name map does not bind exactly class IDs 1 through 96.' }
    $seenNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $seenIds = [Collections.Generic.HashSet[int]]::new()
    $orderedNames = [Collections.Generic.List[string]]::new()
    $orderedIds = [Collections.Generic.List[int]]::new()
    $scrollEvents = [Collections.Generic.List[object]]::new()
    $viewport = & $ReadViewport
    $scrollCount = 0
    $seenSignatures = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    while ($true) {
        Assert-P919ViewportSnapshot $viewport $ExpectedNames $ExpectedProcessId $ExpectedWindowHandle
        $signature = Get-P919ViewportSignature $viewport
        [void]$seenSignatures.Add($signature)
        foreach ($item in @($viewport.items | Where-Object { [bool]$_.isVisible })) {
            $name = [string]$item.name
            $id = [int]$ClassIdByName[$name]
            if ($seenNames.Add($name)) {
                if (-not $seenIds.Add($id)) { throw "P9-19 two distinct UIA names map to duplicate fixture class ID $id." }
                $orderedNames.Add($name)
                $orderedIds.Add($id)
            }
        }
        if ($seenIds.Count -eq 96) { break }
        if ($scrollCount -ge $MaximumScrolls) { throw "P9-19 bounded viewport discovery found only $($seenIds.Count) of 96 fixture classes after $MaximumScrolls scrolls." }
        if (-not [bool]$viewport.canScroll) { throw "P9-19 Classes viewport reached its boundary after discovering only $($seenIds.Count) fixture classes." }
        if ($null -ne $viewport.scrollPattern -and [double]$viewport.verticalScrollPercent -ge 100) { throw "P9-19 Classes scroll percent reached 100 before all fixture classes were discovered." }
        $beforePercent = [double]$viewport.verticalScrollPercent
        $beforeNames = @($viewport.visibleNames)
        $scrollAction = & $ScrollViewport $viewport
        $nextViewport = & $ReadViewport
        Assert-P919ViewportSnapshot $nextViewport $ExpectedNames $ExpectedProcessId $ExpectedWindowHandle
        $afterSignature = Get-P919ViewportSignature $nextViewport
        $afterPercent = [double]$nextViewport.verticalScrollPercent
        $afterNames = @($nextViewport.visibleNames)
        if ($afterPercent -lt ($beforePercent - 0.001) -or ($afterSignature -ceq $signature)) { throw 'P9-19 viewport discovery scroll did not make bounded forward progress.' }
        if ($seenSignatures.Contains($afterSignature)) { throw 'P9-19 viewport discovery repeated a previous visible position.' }
        $scrollEvents.Add([ordered]@{ scrollIndex = $scrollCount + 1; verticalScrollPercentBefore = $beforePercent; verticalScrollPercentAfter = $afterPercent; visibleNamesBefore = $beforeNames; visibleNamesAfter = $afterNames; progressSignatureChanged = ($afterSignature -cne $signature); classIdsDiscoveredThroughCurrentViewport = @($orderedIds.ToArray()); scrollAction = $scrollAction })
        $viewport = $nextViewport
        $scrollCount++
    }
    $discoveredIds = [string]::Join(',', [string[]]@($orderedIds.ToArray() | Sort-Object -Unique))
    if ($orderedIds.Count -ne 96 -or $discoveredIds -cne $requiredIds) { throw 'P9-19 viewport discovery missed or duplicated an expected fixture class.' }
    [ordered]@{ classIds = $orderedIds.ToArray(); accessibleNames = $orderedNames.ToArray(); scrollEvents = $scrollEvents.ToArray(); viewportScrollCount = $scrollEvents.Count }
}

function Invoke-P919VirtualizedSelectionSweep {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string[]]$ExpectedNames,
        [Parameter(Mandatory=$true)][int[]]$ExpectedClassIds,
        [Parameter(Mandatory=$true)][int]$ExpectedProcessId,
        [Parameter(Mandatory=$true)][Int64]$ExpectedWindowHandle,
        [Parameter(Mandatory=$true)][scriptblock]$ReadViewport,
        [Parameter(Mandatory=$true)][scriptblock]$ScrollViewport,
        [Parameter(Mandatory=$true)][scriptblock]$SelectAndReadback,
        [ValidateRange(1,100)][int]$MaximumScrollsPerClass = 40
    )
    $observedIds = [string]::Join(',', [string[]]@($ExpectedClassIds | Sort-Object -Unique))
    $requiredIds = [string]::Join(',', [string[]](1..96))
    if ($ExpectedNames.Count -ne 96 -or @($ExpectedNames | Select-Object -Unique).Count -ne 96 -or $ExpectedClassIds.Count -ne 96 -or @($ExpectedClassIds | Select-Object -Unique).Count -ne 96 -or $observedIds -cne $requiredIds) { throw 'P9-19 virtualized sweep requires exactly 96 unique names and class IDs.' }
    $selectedNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $readbacks = [Collections.Generic.List[object]]::new()
    $scrollEvents = [Collections.Generic.List[object]]::new()
    foreach ($index in 0..($ExpectedNames.Count - 1)) {
        $expectedName = [string]$ExpectedNames[$index]
        $expectedId = [int]$ExpectedClassIds[$index]
        $viewport = & $ReadViewport
        $scrollCount = 0
        $seenSignatures = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        while ($true) {
            Assert-P919ViewportSnapshot $viewport $ExpectedNames $ExpectedProcessId $ExpectedWindowHandle
            $signature = Get-P919ViewportSignature $viewport
            [void]$seenSignatures.Add($signature)
            $matches = @($viewport.items | Where-Object { [bool]$_.isVisible -and [string]$_.name -ceq $expectedName })
            if ($matches.Count -gt 1) { throw "P9-19 expected class appears more than once in a visible viewport: $expectedName" }
            if ($matches.Count -eq 1) { break }
            if ($scrollCount -ge $MaximumScrollsPerClass) { throw "P9-19 bounded viewport scan missed expected class after $MaximumScrollsPerClass scrolls: $expectedName" }
            if (-not [bool]$viewport.canScroll) { throw "P9-19 expected class is absent and the current UIA viewport cannot scroll: $expectedName" }
            if ([double]$viewport.verticalScrollPercent -ge 100) { throw "P9-19 reached the UIA scroll boundary before expected class: $expectedName" }
            $beforeSignature = $signature
            $beforePercent = [double]$viewport.verticalScrollPercent
            $scrollAction = & $ScrollViewport $viewport
            $nextViewport = & $ReadViewport
            Assert-P919ViewportSnapshot $nextViewport $ExpectedNames $ExpectedProcessId $ExpectedWindowHandle
            $afterSignature = Get-P919ViewportSignature $nextViewport
            $afterPercent = [double]$nextViewport.verticalScrollPercent
            if ($afterPercent -lt ($beforePercent - 0.001)) { throw "P9-19 forward UIA scroll moved backward while locating $expectedName." }
            if ($afterSignature -ceq $beforeSignature -or ($afterPercent -le ($beforePercent + 0.001) -and [string]::Join([char]10, [string[]]$nextViewport.visibleNames) -ceq [string]::Join([char]10, [string[]]$viewport.visibleNames))) { throw "P9-19 UIA viewport made no measurable scroll progress while locating $expectedName." }
            if ($seenSignatures.Contains($afterSignature)) { throw "P9-19 UIA viewport repeated a previous position while locating $expectedName." }
            $scrollEvents.Add([ordered]@{ expectedClassName = $expectedName; scrollIndex = $scrollCount + 1; verticalScrollPercentBefore = $beforePercent; verticalScrollPercentAfter = $afterPercent; visibleNamesBefore = @($viewport.visibleNames); visibleNamesAfter = @($nextViewport.visibleNames); progressSignatureChanged = ($afterSignature -cne $beforeSignature); scrollAction = $scrollAction })
            $viewport = $nextViewport
            $scrollCount++
        }
        if (-not $selectedNames.Add($expectedName)) { throw "P9-19 selection sweep attempted a duplicate class selection: $expectedName" }
        $readback = & $SelectAndReadback $viewport $matches[0] $expectedName
        if ($null -eq $readback -or [string]$readback.selectedListItemName -cne $expectedName -or [int]$readback.classId -ne $expectedId -or -not [bool]$readback.selectionItemPatternIsSelected) { throw "P9-19 fresh UIA selection readback failed for $expectedName." }
        $readbacks.Add($readback)
    }
    if ($readbacks.Count -ne 96 -or $selectedNames.Count -ne 96) { throw 'P9-19 virtualized UIA traversal did not select exactly 96 distinct classes.' }
    [ordered]@{ readbacks = $readbacks.ToArray(); scrollEvents = $scrollEvents.ToArray(); selectionCount = $readbacks.Count; distinctSelectionCount = $selectedNames.Count }
}
