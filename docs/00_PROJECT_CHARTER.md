# RPS Framework project charter

Date: 2026-08-11

RPS Framework is the FO4VR-native engine binding and mechanics library for the
ROCK/PAPER/SCISSORS ecosystem. It exists so new FO4VR plugins can consume the
engine knowledge proven by the local projects without embedding RVAs, repeating
reverse engineering, or inheriting flat-Fallout assumptions.

The framework has two public layers:

1. `RPS::Addresses`: dependency-free FO4VR 1.2.72 symbol and layout catalogs.
2. `RPS::Runtime`: checked native invocation, memory guards, ownership helpers,
   and reusable engine mechanics.

RPS_SDK remains separate. It describes the dynamic provider ABI exported by
ROCK.dll. RPS Framework describes the FO4VR executable and reusable native
mechanics. A plugin may consume either or both.

Source authority is current local ROCK/PAPER/PAPER_Toolkit/SCISSORS/add-on code
and proven in-game behavior. Existing in-code addresses and contracts are
accepted as verified by explicit user direction. New engine claims still require
FO4VR binary verification before admission.

Non-goals:

- moving ROCK hand/grab/weapon policy into a generic library;
- moving PAPER reload policy or PAPER_Toolkit authoring policy;
- moving SCISSORS active-ragdoll ownership/repair state machines;
- reproducing F4VR-CommonFramework as a monolith;
- exposing an unchecked patch-any-address utility.

