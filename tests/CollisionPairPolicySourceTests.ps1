param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/CollisionPairPolicy.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/CollisionPairPolicy.cpp')
$combined = $header + $source

if (-not $catalog.Contains('Collision_CompareFilterInfo, 0x1E115B0')) {
    throw 'Collision-pair compare address was lost from the versioned catalog.'
}

$requiredContracts = @(
    'MaximumSuppressionRules = 256',
    'CompareFilterInfoExpectedPrefix',
    '0x8B',
    '0x7F',
    'std::atomic<std::uint64_t>::is_always_lock_free',
    'std::scoped_lock lock(_writeMutex)',
    'std::memory_order_seq_cst',
    'stableSequence(sequenceBefore, sequenceAfter)',
    'result.collides = vanillaCollides',
    'if (!vanillaCollides)',
    'result.collides = false',
    'This object does not install a hook',
    'the original first',
    'keep this object alive'
)
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Collision-pair snapshot contract lost '$needle'."
    }
}

if ($source -match 'std::function|VirtualAlloc|VirtualProtect|safe_write|writeAbsoluteJump|install.*Hook') {
    throw 'Collision-pair snapshots must not own callbacks, allocate trampolines, or install hooks.'
}
if ($source -match 'new\s|delete\s|push_back|emplace_back') {
    throw 'Collision-pair publication and evaluation must remain fixed-capacity and allocation-free.'
}

Write-Host 'RPS collision-pair snapshot source contracts valid.'
