param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/Impact.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/Impact.cpp')

$requiredCatalog = @(
    'Impact_HitDataCtor, 0x1042460',
    'Impact_HitDataDtor, 0x1042530',
    'Impact_HitDataInitialize, 0x1043740',
    'Impact_ActorHitMe, 0x0E51760',
    'Impact_GetCollisionObjectForBody, 0x1D7F2F0',
    'Impact_GetActorHandle, 0x0000AAC0'
)
foreach ($needle in $requiredCatalog) {
    if (-not $catalog.Contains($needle)) {
        throw "Impact address catalog lost '$needle'."
    }
}

$requiredContracts = @(
    'DamageImpactDataSize = 0x40',
    'HitDataSize = 0xE0',
    'HitData_AggressorHandle = 0x40',
    'owning game',
    'addBethesdaReference(collisionObject)',
    'releaseBethesdaReference(collisionObject)',
    'preparePhysicsImpactGeometry',
    'ActorDispatchFailed'
)
$combined = $layouts + $header + $source
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Impact runtime contract lost '$needle'."
    }
}

if ($source -match 'ActorDoDamage|DoDamage|RE::|REL::|F4SE::') {
    throw 'Impact delivery must remain native, CommonLib-free, and route through Actor::HitMe semantics.'
}

Write-Host 'RPS impact source contracts valid.'
