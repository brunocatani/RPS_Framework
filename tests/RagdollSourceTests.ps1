param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/Ragdoll.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/Ragdoll.cpp')

$requiredHeader = @(
    'GraphManagerLease',
    'GraphManagerLock',
    'borrowed and remain valid only',
    'GraphManagerLease&& managerLease',
    'const GraphManagerLease& managerLease()',
    'const WorldWriteGuard& guard',
    'copyRagdollBodyIds'
)
foreach ($needle in $requiredHeader) {
    if (-not $header.Contains($needle)) {
        throw "Ragdoll public contract lost '$needle'."
    }
}

$requiredSource = @(
    'Layout::GraphLockAttemptLimit',
    'Layout::GraphArrayInlineFlag',
    'Layout::MaximumBodyCount',
    'guard.owns(_hknpWorld)',
    'pointers.ownerManager != lease.get()',
    'Ragdoll_RemoveAttachments',
    'Ragdoll_UpdateConstraints',
    'rather than destroying'
)
foreach ($needle in $requiredSource) {
    if (-not $source.Contains($needle)) {
        throw "Ragdoll safety implementation lost '$needle'."
    }
}

if ($source -match 'std::vector|new\s|make_unique|make_shared|GraphManager_ActiveGraphIndex') {
    throw 'Core ragdoll access must remain allocation-free and must not promote the diagnostic active-index field.'
}

Write-Host 'RPS ragdoll source contracts valid.'
