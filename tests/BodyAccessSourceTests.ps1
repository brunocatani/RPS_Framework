param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/BodyAccess.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/BodyAccess.cpp')
$combined = $header + $source

$requiredLayouts = @(
    'PhysicsSystem_Instance = 0x18',
    'PhysicsSystemInstance_World = 0x18',
    'PhysicsSystemInstance_BodyIds = 0x20',
    'PhysicsSystemInstance_BodyCount = 0x28',
    'PhysicsSystemInstance_MinimumReadableSize = 0x30',
    'CollisionObject_OwnerNode = 0x10',
    'CollisionObject_PhysicsSystem = 0x20',
    'CollisionObject_BodyIndex = 0x28',
    'MaximumPhysicsSystemBodyCount = 4096'
)
foreach ($needle in $requiredLayouts) {
    if (-not $layouts.Contains($needle)) {
        throw "Collision-body layout catalog lost '$needle'."
    }
}

$requiredContracts = @(
    'sizeof(NativeCollisionObject) == Bethesda::CollisionObjectSize',
    'sizeof(NativePhysicsSystem) == Bethesda::PhysicsSystemSize',
    'sizeof(NativePhysicsInstance) == Bethesda::PhysicsSystemInstance_MinimumReadableSize',
    'collision.ownerNode != reinterpret_cast<std::uintptr_t>(expectedSceneOwner)',
    'instance.world != reinterpret_cast<std::uintptr_t>(expectedHknpWorld)',
    'collision.bodyIndex >= static_cast<std::uint32_t>(instance.bodyCount)',
    'bodyTableBytes',
    'result.bodyId.value > Havok::MaxReadableBodyIndex',
    'verifiedCollision.ownerNode != collision.ownerNode',
    'verifiedSystem.instance != system.instance',
    'verifiedInstance.bodyIds != instance.bodyIds',
    'verifiedBodyId != result.bodyId.value',
    'The caller must keep the scene object'
)
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Collision-body resolver contract lost '$needle'."
    }
}

if ($source -match 'reinterpret_cast<[^>]+\(\*\)' -or $source -match 'Getbhk|virtual') {
    throw 'Collision-body resolution must use copied records and never dispatch through the borrowed collision object.'
}

Write-Host 'RPS collision-body resolver source contracts valid.'
