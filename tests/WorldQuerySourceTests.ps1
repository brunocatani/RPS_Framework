param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/WorldQuery.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/WorldQuery.cpp')

$requiredHeader = @(
    'const WorldReadGuard& guard',
    'std::span<QueryHit> output',
    'droppedHitCount',
    'sortQueryHitsByFraction'
)
foreach ($needle in $requiredHeader) {
    if (-not $header.Contains($needle)) {
        throw "World query public contract lost '$needle'."
    }
}

$requiredSource = @(
    'Addresses::Symbol::World_PickObject',
    'Addresses::Symbol::World_CastShape',
    'guard.owns(_hknpWorld)',
    'FixedHitCollector',
    'Layout::MaximumCollectedHits',
    'static_assert(sizeof(NativeCollectorBase)',
    'collector.dropped()',
    'QueryFilterUnavailable',
    'scale.runtimeBacked'
)
foreach ($needle in $requiredSource) {
    if (-not $source.Contains($needle)) {
        throw "World query safety implementation lost '$needle'."
    }
}

if ($source -match 'std::vector|new\s|make_unique|make_shared|WorldWriteGuard') {
    throw 'World queries must remain allocation-free and read-only.'
}
if ($source -match 'WorldReadGuard\s+\w+\s*\{[^}]*_bhkWorld') {
    throw 'Bethesda PickObject owns its synchronization and must not be double-locked.'
}

Write-Host 'RPS world query source contracts valid.'
