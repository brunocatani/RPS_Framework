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
    'NiAVObject_UpdateWorldBoundVtableIndex = 0x36',
    'NiNode_AttachChildVtableIndex = 0x3D',
    'NiNode_DetachChildVtableIndex = 0x40',
    'NiAVObject_Parent = 0x28'
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
    'addBethesdaReference(parent)',
    'addBethesdaReference(child)',
    'releaseBethesdaReference(child)',
    'releaseBethesdaReference(parent)',
    'result.parentAfter != result.parentAddress',
    'result.parentAfter != 0',
    'desiredWorld.rotate.entry[row][0] * parentWorld.rotate.entry[column][0]',
    'local.rotate.entry[row][0] * parentWorld.rotate.entry[0][column]',
    'long-lived reference ownership remain consumer responsibilities'
)
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Scene runtime contract lost '$needle'."
    }
}

if ($combined -match 'CommonLib|REL::|F4SE|NiPointer') {
    throw 'Core scene helpers must remain CommonLib-free and must not introduce NiPointer ownership.'
}

Write-Host 'RPS native scene source contracts valid.'
