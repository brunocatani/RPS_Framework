param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$viewHeader = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/GeneratorOutput.h')
$viewSource = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/GeneratorOutput.cpp')
$poseHeader = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/AnimationPose.h')
$poseSource = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/AnimationPose.cpp')
$motorHeader = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/AnimationMotor.h')
$motorSource = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/AnimationMotor.cpp')

$required = @(
    @($viewHeader, 'TrackAccess'),
    @($viewHeader, 'valid only for the synchronous'),
    @($viewSource, 'Memory::Access::Write'),
    @($viewSource, 'access == TrackAccess::MutableStorage'),
    @($poseHeader, 'std::span<HkQsTransform>'),
    @($poseSource, 'if (dot < 0.0f)'),
    @($poseSource, 'Memory::copyTo'),
    @($motorHeader, 'sanitizeMotorControlSettings'),
    @($motorHeader, 'suppressPoweredControlsForOneFrame'),
    @($motorSource, 'paletteUsable'),
    @($motorSource, 'TrackAccess::MutableStorage'),
    @($motorSource, 'WorldFromModelMode_Value')
)

foreach ($contract in $required) {
    if (-not $contract[0].Contains($contract[1])) {
        throw "Animation output contract missing: $($contract[1])"
    }
}
if ($poseHeader.Contains('std::vector') -or $motorHeader.Contains('std::vector')) {
    throw 'Animation callback APIs must remain allocation-free span/value interfaces.'
}

Write-Host 'RPS synchronous animation pose and motor contracts valid.'
