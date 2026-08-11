param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/ActorState.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/ActorState.cpp')
$combined = $header + $source

$requiredLayouts = @(
    'Actor_StateSubobject = 0x128',
    'Actor_LifeFlags = 0x130',
    'Actor_KnockFlags = 0x134',
    'Actor_AIProcess = 0x300',
    'Actor_RagdollMovementFlags = 0x43C',
    'ActorState_SetKnockStateVtableSlot = 0x120',
    'ActorState_GetKnockStateVtableSlot = 0x128',
    'KnockCodeShift = 19',
    'KnockCodeMask = 0x3',
    'LifeStateShift = 18',
    'LifeStateMask = 0xF',
    'RagdollMovementFlagMask = 0x100',
    'AIProcess_KnockData = 0x8',
    'AIProcess_HighData = 0x10',
    'KnockData_CurrentHandle = 0x3B0',
    'KnockData_RagdollFlag = 0x4BF',
    'HighProcess_FullRagdollFlagA = 0x58E',
    'HighProcess_FullRagdollFlagB = 0x58F'
)
foreach ($needle in $requiredLayouts) {
    if (-not $layouts.Contains($needle)) {
        throw "Actor-state layout catalog lost '$needle'."
    }
}

$requiredContracts = @(
    'using GetKnockStateFunction = std::uint32_t (*)(void*)',
    'using SetKnockStateFunction = bool (*)(void*, std::uint32_t)',
    'resolveActorStateVirtual',
    'Memory::Access::Execute',
    'currentThreadPhysicsStepState',
    'Layout::MaximumKnockState',
    'Layout::NormalKnockState',
    'const auto preflight = inspect()',
    'preflight.snapshot.rawMovementFlags != result.before.rawMovementFlags',
    'result.after.processAddress != result.before.processAddress',
    'result.after.highProcessAddress != result.before.highProcessAddress',
    'The narrow',
    'consumers decide when they own those fields'
)
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Actor-state runtime contract lost '$needle'."
    }
}

if ($combined -match 'CommonLib|REL::|F4SE' -or $source -match 'WorldWriteGuard') {
    throw 'Corrected actor-state access must remain CommonLib-free and outside the hknp world-write domain.'
}
if ($source -match 'Memory::write\([^\r\n]+Actor_LifeFlags|Memory::write\([^\r\n]+Actor_KnockFlags') {
    throw 'Life/knock storage must not be patched directly; knock transitions use the live ActorState virtual.'
}

Write-Host 'RPS corrected actor-state source contracts valid.'
