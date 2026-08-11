param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$header = Get-Content -Raw -LiteralPath (Join-Path $Root 'include/RPS/Runtime/GeneratedBody.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/GeneratedBody.cpp')

function Require-Pattern([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) {
        throw $Message
    }
}

Require-Pattern $header 'GeneratedBodyRetirementGraceSteps\s*=\s*8' 'Generated bodies must retain ROCK''s eight-step broadphase grace window.'
Require-Pattern $header 'GeneratedBodyRetirementCapacity\s*=\s*512' 'Generated-body retirement must remain bounded.'
Require-Pattern $source 'Body_MotionCinfoCtor' 'Non-static bodies must use the proven dynamic-safe MotionCinfo constructor.'
Require-Pattern $source 'Physics_SetBodyKeyframed' 'Keyframed bodies must use Bethesda''s live body transition.'
Require-Pattern $source 'Body_CollisionObjectSetMotionType' 'Static and dynamic bodies must use the Bethesda collision-object transition.'
Require-Pattern $source 'Body_CollisionObjectAddToWorld' 'Generated colliders must enter the world through Bethesda wrapper ownership.'
Require-Pattern $source 'addHavokReference\(shape\)' 'System-data shape ownership must retain its shape reference.'
Require-Pattern $source 'snapshot\.body\.userDataAddress\s*!=\s*reinterpret_cast<std::uintptr_t>\(resources\.collisionObject\)' 'Creation must verify the live body back-pointer.'
Require-Pattern $source 'advanceGeneratedBodyRetirement' 'Deferred release must advance only by completed physics steps.'
Require-Pattern $source 'PendingRemoval[\s\S]*GraceWindow' 'Cross-thread destruction must pass through removal before the grace window.'

if ($source -match 'InitializeAsKeyframed|registerContactSignal|setPointVelocity') {
    throw 'Unverified or superseded generated-body paths must not enter the framework.'
}

Write-Host 'Generated-body lifecycle source contracts valid.'
