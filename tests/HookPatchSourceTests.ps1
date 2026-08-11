param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/HookPatch.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/HookPatch.cpp')

$requiredHeader = @(
    'caller owns thread quiescence',
    'patchDirectCallsTransactional',
    'patchVtableSlotsTransactional',
    'inspectExecutableSite',
    'RollbackFailed'
)
foreach ($needle in $requiredHeader) {
    if (-not $header.Contains($needle)) {
        throw "Hook patch public contract lost '$needle'."
    }
}

$requiredSource = @(
    'FlushInstructionCache',
    'VirtualProtect',
    'InterlockedExchangePointer',
    'std::byte{ 0xE8 }',
    'rollbackPatch.replacementTarget',
    'Memory::Access::Execute'
)
foreach ($needle in $requiredSource) {
    if (-not $source.Contains($needle)) {
        throw "Hook patch safety implementation lost '$needle'."
    }
}

if ($source -match 'std::vector|new\s|make_unique|make_shared') {
    throw 'Hook patch transactions must not allocate.'
}

Write-Host 'RPS hook patch source contracts valid.'
