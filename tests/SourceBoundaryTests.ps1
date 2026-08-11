param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$includeRoot = Join-Path $Root 'include'
$sourceRoot = Join-Path $Root 'src'
$catalogPath = Join-Path $includeRoot 'RPS/Addresses/Fallout4Vr_1_2_72.inc'
$layoutsPath = Join-Path $includeRoot 'RPS/Addresses/Layouts.h'

if (-not (Test-Path -LiteralPath $catalogPath) -or -not (Test-Path -LiteralPath $layoutsPath)) {
    throw 'The versioned address and layout catalogs are mandatory.'
}

$productionFiles = @(
    Get-ChildItem -LiteralPath $includeRoot, $sourceRoot -Recurse -File -Include *.h,*.hpp,*.cpp,*.inc
)
$forbiddenCommonLib = '(?m)^\s*#\s*include\s*[<"](?:RE|REL|F4SE)/|\bREL::(?:ID|Offset|Relocation)\b'
$forbiddenLoaderDomain = 'RuntimeVersion\s*\(\s*\)\s*(?:==|!=|<|>|<=|>=)\s*F4SE::RUNTIME_(?:LATEST_)?VR'

foreach ($file in $productionFiles) {
    $text = Get-Content -Raw -LiteralPath $file.FullName
    if ($text -match $forbiddenCommonLib) {
        throw "CommonLib boundary violation in $($file.FullName)"
    }
    if ($text -match $forbiddenLoaderDomain) {
        throw "F4SE query/executable version-domain violation in $($file.FullName)"
    }

    if ($file.FullName -ne $catalogPath -and $file.FullName -ne $layoutsPath) {
        $matches = [regex]::Matches($text, '0x[0-9A-Fa-f]{6,}')
        if ($matches.Count -ne 0) {
            $values = ($matches | ForEach-Object Value | Sort-Object -Unique) -join ', '
            throw "Raw engine-sized constants outside catalogs in $($file.FullName): $values"
        }
    }
}

$catalog = Get-Content -Raw -LiteralPath $catalogPath
$symbolCount = ([regex]::Matches($catalog, '(?m)^RPS_SYMBOL\(')).Count
if ($symbolCount -lt 150) {
    throw "Address catalog unexpectedly shrank to $symbolCount symbols."
}

Write-Host "RPS source boundaries valid; $symbolCount FO4VR symbols are cataloged."
