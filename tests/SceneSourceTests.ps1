param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/Scene.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/Scene.cpp')
$combined = $header + $source

foreach ($needle in @(
    'NiAVObject_SetMaterialNeedsUpdateVtableIndex = 0x2E',
    'NiAVObject_SetAppCulledVtableIndex = 0x30',
    'NiAVObject_UpdateWorldBoundVtableIndex = 0x36'
)) {
    if (-not $layouts.Contains($needle)) {
        throw "Scene virtual catalog lost '$needle'."
    }
}

$requiredContracts = @(
    'using BooleanObjectCommand = void (*)(void*, bool)',
    'using UnaryObjectCommand = void (*)(void*)',
    'currentThreadPhysicsStepState',
    'Memory::Access::Execute',
    'properRotation',
    'determinant - 1.0f',
    'NiAVObject_SetMaterialNeedsUpdateVtableIndex',
    'NiAVObject_SetAppCulledVtableIndex',
    'NiAVObject_UpdateWorldBoundVtableIndex',
    'desiredWorld.rotate.entry[row][0] * parentWorld.rotate.entry[column][0]',
    'local.rotate.entry[row][0] * parentWorld.rotate.entry[0][column]',
    'attachment,',
    'renderer-proxy, and NiPointer lifetime remain consumer responsibilities'
)
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Scene runtime contract lost '$needle'."
    }
}

if ($combined -match 'CommonLib|REL::|F4SE' -or $source -match 'AttachChild|DetachChild|NiPointer') {
    throw 'Core scene helpers must remain CommonLib-free and must not claim scene attachment or reference ownership.'
}

Write-Host 'RPS native scene source contracts valid.'
