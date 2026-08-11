param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$worldAccess = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/WorldAccess.cpp')
$sceneHeader = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/ScenePhysics.h')
$sceneSource = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/ScenePhysics.cpp')
$gravityHeader = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/BodyGravity.h')
$gravitySource = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/BodyGravity.cpp')

$requiredCatalog = @(
    'World_SetMotionRecursive, 0x1DF95B0',
    'World_EnableCollisionRecursive, 0x1DF9940',
    'Character_SetWaterGravityFactor, 0x07C73F0'
)
foreach ($needle in $requiredCatalog) {
    if (-not $catalog.Contains($needle)) {
        throw "Recursive physics address catalog lost '$needle'."
    }
}

$requiredLayouts = @(
    'HknpWorld_MotionPropertiesLibrary = 0x5D0',
    'MotionPropertiesLibrary_Data = 0x28',
    'MotionProperties_Stride = 0x40',
    'MotionProperties_GravityFactor = 0x08'
)
foreach ($needle in $requiredLayouts) {
    if (-not $layouts.Contains($needle)) {
        throw "Body gravity layout catalog lost '$needle'."
    }
}

$requiredSceneContracts = @(
    'using SetMotionRecursiveFunction = std::uint8_t (*)(void*, std::uint32_t, bool, bool, bool)',
    'using EnableCollisionRecursiveFunction = std::uint8_t (*)(void*, bool, bool, bool)',
    'currentThreadPhysicsStepState',
    'PhysicsStepState::Outside',
    'InvalidMotionPreset',
    'multi-child props',
    'scene root is borrowed'
)
$sceneCombined = $sceneHeader + $sceneSource
foreach ($needle in $requiredSceneContracts) {
    if (-not $sceneCombined.Contains($needle)) {
        throw "Recursive scene physics contract lost '$needle'."
    }
}

$requiredGravityContracts = @(
    'using SetBodyGravityFactorFunction = void (*)(void*, std::uint32_t, float)',
    'snapshotBodyGravityDuringSafeEpoch',
    'currentThreadPhysicsStepState',
    'PhysicsStepState::Unknown',
    'SetWaterGravityFactor acquires the world',
    'separate read-lock epochs',
    'VerificationFailed'
)
$gravityCombined = $gravityHeader + $gravitySource
foreach ($needle in $requiredGravityContracts) {
    if (-not $gravityCombined.Contains($needle)) {
        throw "Body gravity contract lost '$needle'."
    }
}

if ($gravitySource -match 'WorldWriteGuard|Memory::write' -or
    $gravityHeader -match 'setBodyFactor\s*\(\s*const\s+WorldWriteGuard') {
    throw 'The self-locking gravity native must never be wrapped in an external world write guard or direct field write.'
}
if ($sceneCombined -match 'WorldWriteGuard|SetMotionType') {
    throw 'Recursive scene commands must stay on the engine subtree wrappers without an external world-write path.'
}
if ($worldAccess -notmatch 'currentThreadPhysicsStepState[\s\S]*PhysicsStepState::Unknown' -or
    $worldAccess -notmatch 'WorldWriteGuard::WorldWriteGuard[\s\S]*WriteAccessMode::PhysicsStepOwned[\s\S]*PhysicsStepState::Outside') {
    throw 'World access must preserve tri-state TLS ownership and fail closed before MarkForWrite.'
}

Write-Host 'RPS recursive scene and body gravity contracts valid.'
