param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/MovementController.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/MovementController.cpp')

$requiredCatalog = @(
    'Character_MotionDrivenControlUpdate, 0x0FA5EE0',
    'Character_SetPlannerTargetAngle, 0x0FA7A70',
    'Character_ClearPlannerDirectControl, 0x0FA7AF0',
    'Character_RequestPathFollowing, 0x0FA7C40',
    'Character_SetAnimationDriven, 0x0FA7CA0',
    'Character_SetAnimationDrivenAllowPlannerRotation, 0x0FA7EA0',
    'Character_SetPlannerDirectControl, 0x0FA7FA0'
)
foreach ($needle in $requiredCatalog) {
    if (-not $catalog.Contains($needle)) {
        throw "Movement-controller address catalog lost '$needle'."
    }
}

$requiredLayouts = @(
    'Actor_ControllerSmartPointer = 0x318',
    'Controller_MotionDrivenInterface = 0x128',
    'Controller_PlannerDirectInterface = 0x140',
    'Controller_Mode = 0x198',
    'Controller_PathingFlags = 0x1A0',
    'Controller_PathingMasterFlagIndex = 5',
    'Controller_MinimumReadableSize = 0x1A8'
)
foreach ($needle in $requiredLayouts) {
    if (-not $layouts.Contains($needle)) {
        throw "Movement-controller layout catalog lost '$needle'."
    }
}

$combined = $header + $source
$requiredContracts = @(
    'using MovementControllerModeFunction = void (*)(void*)',
    'using MovementPlannerSetTargetAngleFunction = void (*)(void*, void*)',
    'snapshotMovementController',
    'currentThreadPhysicsStepState',
    'sameController',
    'validMovementTransition',
    'WrongMode',
    'never writes Controller_Mode directly'
)
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Movement-controller contract lost '$needle'."
    }
}

if ($source -match 'Memory::write|Memory::copyTo' -or
    $source -match 'controller\s*\+\s*(?:0x198|408)') {
    throw 'Movement-controller modes and flags must change only through the proven native transitions.'
}
if ($source -match 'std::vector|\bnew\s|make_unique|make_shared|WorldWriteGuard') {
    throw 'Movement-controller commands must stay synchronous, allocation-free, and outside world-write ownership.'
}

Write-Host 'RPS movement-controller source contracts valid.'
