param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$catalog = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Fallout4Vr_1_2_72.inc')
$layouts = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Addresses/Layouts.h')
$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/PointLight.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/PointLight.cpp')
$combined = $header + $source

foreach ($needle in @(
    'Scene_PointLightCreate, 0x1C27DB0',
    'Scene_PointLightRegister, 0x27E9C30',
    'Scene_PointLightUnregister, 0x27EA6F0',
    'Scene_PointLightManager, 0x6879520',
    'Scene_PointLightVtable, 0x2E58BC8'
)) {
    if (-not $catalog.Contains($needle)) {
        throw "Point-light catalog lost '$needle'."
    }
}

foreach ($needle in @(
    'ObjectSize = 0x1D0',
    'Ambient = 0x160',
    'Diffuse = 0x16C',
    'Specular = 0x178',
    'Dimmer = 0x184',
    'ModelBound = 0x190',
    'RendererData = 0x1A0',
    'ConstantAttenuation = 0x1B0',
    'LinearAttenuation = 0x1B4',
    'QuadraticAttenuation = 0x1B8'
)) {
    if (-not $layouts.Contains($needle)) {
        throw "Point-light layout lost '$needle'."
    }
}

foreach ($needle in @(
    'currentThreadPhysicsStepState',
    'GetCurrentThreadId() != ownerThreadId',
    'pointLightIdentityValid',
    'addBethesdaReference(light)',
    'addBethesdaReference(proxy)',
    'hasLiveBethesdaReference(parent)',
    'addBethesdaReference(parent)',
    'hierarchy.detachChild(parent, _light)',
    'invokeUnregister(unregister, _manager, _light)',
    'releaseBethesdaReference(proxy)',
    'releaseBethesdaReference(light)',
    'clearWithoutRelease();',
    '_registrationStateKnown = false',
    'ownershipAbandoned = true',
    'detach -> unregister -> proxy release -> light release'
)) {
    if (-not $combined.Contains($needle)) {
        throw "Point-light ownership contract lost '$needle'."
    }
}

if ($combined -match 'CommonLib|REL::|F4SE|NiPointer') {
    throw 'Point-light runtime must remain CommonLib-free and own references explicitly.'
}

Write-Host 'RPS point-light source contracts valid.'
