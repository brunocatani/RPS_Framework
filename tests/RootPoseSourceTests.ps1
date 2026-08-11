param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/RootPose.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/RootPose.cpp')
$combined = $header + $source

foreach ($needle in @(
    'Interface_GetBodyHandleVtableSlot = 0x38',
    'Interface_GetLowSkeletonVtableSlot = 0x50',
    'PhysicsInterface_GetBodyOffsetVtableSlot = 0x60',
    'PhysicsInterface_GetBodyTransformVtableSlot = 0x70',
    'LowSkeleton_ParentIndices = 0x18',
    'LowSkeleton_BoneCount = 0x30',
    'BodyHandle_BodyId = 0x10'
)) {
    if (-not $layouts.Contains($needle)) {
        throw "Root-pose layout catalog lost '$needle'."
    }
}

foreach ($needle in @(
    'Ragdoll_MapHighToLowPose',
    'Ragdoll_CopyAndApplyScaleToPose',
    'Ragdoll_CopyAndScaleTransform',
    'Ragdoll_PoseLocalToWorld',
    'Ragdoll_ResolvePhysicsInterface',
    'sameSkeleton',
    'confirmation',
    'GenerationChanged',
    'Memory::Access::Execute',
    'No pointer is',
    'retained and no world-from-model track is mutated',
    'applyBodyAnchorOffset',
    'computeRootTranslationBias',
    'sampleRootDelta'
)) {
    if (-not $combined.Contains($needle)) {
        throw "Root-pose contract lost '$needle'."
    }
}

if ($combined -match 'CommonLib|REL::|F4SE' -or $combined -match 'applyWorldFromModelCompensation') {
    throw 'Root-pose core must stay dependency-free and must not promote live WFM mutation.'
}

Write-Host 'RPS root-pose source contracts valid.'
