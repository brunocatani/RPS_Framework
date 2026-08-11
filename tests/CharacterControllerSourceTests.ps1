param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/CharacterController.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/CharacterController.cpp')
$combined = $header + $source

foreach ($needle in @('Character_GetController, 0x0DC42E0', 'Character_AddController, 0x1DFA9C0')) {
    if (-not $catalog.Contains($needle)) {
        throw "Character-controller address catalog lost '$needle'."
    }
}

$requiredLayouts = @(
    'BhkWorld_RigidBodyManager = 0xD8',
    'BhkWorld_MinimumReadableSize = 0x148',
    'RigidBodyManager_MinimumReadableSize = 0x70',
    'RigidBodyManager_ControllerList = 0x10',
    'RigidBodyManager_ControllerCount = 0x20',
    'Controller_RigidBody = 0x470',
    'RigidBody_StepGate = 0xA8',
    'MaximumManagerControllerScan = 8192'
)
foreach ($needle in $requiredLayouts) {
    if (-not $layouts.Contains($needle)) {
        throw "Character-controller layout catalog lost '$needle'."
    }
}

$requiredContracts = @(
    'using ActorGetCharacterControllerFunction = void* (*)(void*)',
    'using AddCharacterControllerFunction = bool (*)(void*, void*)',
    'inspectCharacterControllerPointer',
    'currentThreadPhysicsStepState',
    'NoFreshInsertion',
    'already present or rejected',
    'native locks bhkWorld internally',
    'inspectExecutableVtable',
    'Layout::BhkWorld_MinimumReadableSize',
    'Layout::RigidBodyManager_MinimumReadableSize',
    'result.after.controllerAddress != result.before.controllerAddress'
)
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Character-controller runtime contract lost '$needle'."
    }
}

if ($source -match 'WorldWriteGuard|Memory::write|Memory::copyTo' -or
    $header -match 'addToWorld\s*\(\s*const\s+WorldWriteGuard') {
    throw 'Character-controller access must use the self-locking native add wrapper and keep step-gate policy read-only.'
}
if ($source -match 'RigidBodyManager_ControllerList|RigidBodyManager_ControllerCount') {
    throw 'Core character-controller access must not race the live manager list without its native lock.'
}

Write-Host 'RPS character-controller source contracts valid.'
