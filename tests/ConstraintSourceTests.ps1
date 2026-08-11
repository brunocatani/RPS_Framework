param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/Constraint.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/Constraint.cpp')
$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')

$requiredHeader = @(
    'class OwnedConstraint',
    'class ConstraintService',
    'ConstraintRetirementCapacity = 512',
    'servicePendingRetirements',
    'shutdownAfterWorldLoss'
)
$requiredSource = @(
    'guard.owns(_world)',
    'snapshotBodyDuringSafeEpoch(_world, bodyA)',
    'releaseHavokReference(constraintData)',
    '_retirements[_retirementCount++] = constraint._id',
    'Constraint_PositionMotorVtable'
)
$requiredCatalog = @(
    'RPS_SYMBOL(Constraint_Create, 0x15469B0, Function, Constraint, Rock)',
    'RPS_SYMBOL(Constraint_Destroy, 0x1546B40, Function, Constraint, Rock)'
)
$requiredLayouts = @(
    'BallAndSocketDataSize = 0x70',
    'LimitedHingeDataSize = 0x130',
    'PrismaticDataSize = 0x120',
    'PositionMotorSize = 0x30'
)

foreach ($needle in $requiredHeader) {
    if (-not $header.Contains($needle)) {
        throw "Constraint ownership contract missing from public header: $needle"
    }
}
foreach ($needle in $requiredSource) {
    if (-not $source.Contains($needle)) {
        throw "Constraint runtime contract missing from source: $needle"
    }
}
foreach ($needle in $requiredCatalog) {
    if (-not $catalog.Contains($needle)) {
        throw "Constraint address missing from catalog: $needle"
    }
}
foreach ($needle in $requiredLayouts) {
    if (-not $layouts.Contains($needle)) {
        throw "Constraint layout missing from catalog: $needle"
    }
}

Write-Host 'RPS constraint construction, ownership, and motor contracts valid.'
