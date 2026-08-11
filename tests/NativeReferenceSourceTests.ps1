param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/NativeReference.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/NativeReference.cpp')
$combined = $header + $source

$requiredContracts = @(
    'Full-width BSIntrusiveRefCounted contract used by native path requests',
    'NativeIntrusiveReferenceSnapshot',
    'NativeIntrusiveReferenceOperation',
    'NativeIntrusivePtr',
    'adoptRetained',
    'detach() transfers',
    'MaximumReasonableIntrusiveReferenceCount',
    '_InterlockedIncrement',
    '_InterlockedDecrement',
    'invokeNativeIntrusiveDestructor',
    'destructor(object, 1)',
    'NativeIntrusivePtr::~NativeIntrusivePtr',
    '(void)releaseNow()',
    '_object = other.detach()'
)
foreach ($needle in $requiredContracts) {
    if (-not $combined.Contains($needle)) {
        throw "Native intrusive-reference contract lost '$needle'."
    }
}

if ($source -notmatch 'inspectNativeIntrusiveReference[\s\S]*std::int32_t' -or
    $source -notmatch 'releaseNativeIntrusiveReference[\s\S]*referenceCountAfter == 0[\s\S]*invokeNativeIntrusiveDestructor') {
    throw 'Native request ownership must keep a full-width signed count and destroy exactly at zero.'
}
if ($header -match 'NativeIntrusivePtr\s*\(\s*const NativeIntrusivePtr&\s*\)\s*=\s*default') {
    throw 'Native intrusive ownership must remain move-only.'
}

Write-Host 'RPS native intrusive-reference source contracts valid.'
