param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/ActorPathing.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/ActorPathing.cpp')
$combined = $header + $source

foreach ($needle in @(
    'Character_ActorNativePackageLoopGuard, 0x03DD040',
    'Character_ActorCanSubmitPathingGoal, 0x0E004B0',
    'Character_ActorIsPathing, 0x0E004F0',
    'Character_ActorQueryPathingState, 0x0E00550',
    'Character_ActorQueryCurrentPathRequest, 0x0DFF5D0',
    'Character_ActorQueryDirectMovementState, 0x0E00940',
    'Character_ActorQueryDirectMovementTargetOffset, 0x0E009B0',
    'Character_ActorQueryDirectMovementTargetAngle, 0x0E00AB0'
)) {
    if (-not $catalog.Contains($needle)) {
        throw "Actor pathing catalog lost '$needle'."
    }
}

foreach ($needle in @(
    'currentThreadPhysicsStepState',
    'Memory::Access::Execute',
    'NativeIntrusivePtr::adoptRetained',
    'inspectNativeIntrusiveReference',
    'InvalidRetainedRequest',
    'one',
    'retained native reference',
    'engine-owning thread'
)) {
    if (-not $combined.Contains($needle)) {
        throw "Actor pathing runtime contract lost '$needle'."
    }
}

if ($combined -match 'CommonLib|REL::|F4SE' -or $source -match 'PathingFlags.*=') {
    throw 'Actor pathing query core must stay dependency-free and read-only.'
}

Write-Host 'RPS actor pathing source contracts valid.'
