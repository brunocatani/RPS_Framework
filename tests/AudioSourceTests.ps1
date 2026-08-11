param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/Audio.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/Audio.cpp')
$combined = $header + $source

foreach ($needle in @(
    'Audio_GetListenerDistanceSquared, 0x1B4F8F0',
    'Audio_SetSoundVolume, 0x1B4ADD0',
    'Audio_FadeInPlay, 0x1B4B370',
    'Audio_FadeOutAndRelease, 0x1B4B3E0',
    'Audio_PlayFollowingDescriptor, 0x02C7D70',
    'Audio_ManagerSingleton, 0x5B6DB90'
)) {
    if (-not $catalog.Contains($needle)) {
        throw "Audio catalog lost '$needle'."
    }
}

foreach ($needle in @(
    'SoundHandleSize = 0x08',
    'SoundHandle_Id = 0x00',
    'SoundHandle_AssumeSuccess = 0x04',
    'SoundHandle_State = 0x05',
    'InvalidSoundId = 0xFFFFFFFF'
)) {
    if (-not $layouts.Contains($needle)) {
        throw "Audio layout contract lost '$needle'."
    }
}

foreach ($needle in @(
    'currentThreadPhysicsStepState',
    'Memory::Access::Execute',
    'HandleAlreadyActive',
    'NativeSoundHandle(const NativeSoundHandle&) = delete',
    'handle.invalidate()',
    'the consumer must',
    'call fadeOutAndRelease',
    'framework never silently replaces an active handle'
)) {
    if (-not $combined.Contains($needle)) {
        throw "Audio runtime contract lost '$needle'."
    }
}

if ($combined -match 'CommonLib|REL::|F4SE') {
    throw 'Core audio helpers must remain dependency-free.'
}

Write-Host 'RPS native audio source contracts valid.'
