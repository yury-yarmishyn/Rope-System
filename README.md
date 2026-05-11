# Ray Rope System

## Video Demo

[![Ray Rope System video demo](https://img.youtube.com/vi/fA-1VRwdwPM/maxresdefault.jpg)](https://youtu.be/fA-1VRwdwPM)

Watch the demo: https://youtu.be/fA-1VRwdwPM

This project contains a custom gameplay rope topology system for Unreal Engine. The rope is represented as a set of logical nodes grouped into ordered segments and solved as a piecewise-linear path in world space.

`URayRopeComponent` does not render the rope and it is not a full rope physics simulation. Its responsibility is to keep a stable, explicit rope route while anchors move and while the rope wraps, moves, unwraps, or splits around collision and anchor actors.

## Project Snapshot

- Unreal Engine `5.7` C++ project.
- The rope code lives in `Source/Ray/RopeSystem`.
- The main gameplay-facing class is `URayRopeComponent` in `Source/Ray/RopeSystem/Public/Component/RayRopeComponent.h`.
- Public rope types are defined in `Source/Ray/RopeSystem/Public/Types/RayRopeTypes.h`.
- Anchor actors can implement `URayRopeInterface` from `Source/Ray/RopeSystem/Public/Interfaces/RayRopeInterface.h`.
- Prototype content lives under `/Game/Ray`, including `/Game/Ray/Levels/Lvl_FirstPerson`, `/Game/Ray/Characters/Player/BPC_RopeComponent`, and `/Game/Ray/MovingActor`.
- The runtime module currently depends on `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `EnhancedInput`, `StateTreeModule`, and `CommonUI`.

## Getting Started

1. Open `Ray.uproject` in Unreal Engine `5.7`.
2. Let Unreal build the `Ray` module, or generate project files and build from your IDE.
3. Open `/Game/Ray/Levels/Lvl_FirstPerson` as the prototype map.
4. Inspect `/Game/Ray/Characters/Player/BPC_RopeComponent` and `/Game/Ray/MovingActor` for Blueprint-side usage examples.

## What The System Solves

For this project, the important problem is not cable rendering. It is deterministic rope routing against gameplay geometry.

`CableComponent` can draw and simulate a cable-like object, but it does not expose persistent logical bend nodes, deterministic unwrap decisions, or anchor insertion when a rope hits gameplay objects. A chain of `PhysicsConstraint` bodies can approximate rope motion, but routing is still implicit in the physics solver and hard to use as stable gameplay state.

This rope system stores routing explicitly:

- `Anchor` nodes define attachment points.
- `Redirect` nodes define temporary bend points created by collision.
- Segments store ordered node chains between anchors.
- Solver passes mutate this topology only when traces and transition validation prove the change is valid.

The result is a gameplay-first rope route that can drive a spline, mesh, Niagara ribbon, tripwire logic, grappling logic, puzzles, or other mechanics.

## Current Limitations

- Rendering is external. The component does not create a mesh, spline, cable, Niagara ribbon, or material setup by itself.
- This is not a full physical rope simulation. There is no sag solver, stretch model, rope mass, friction model, or constraint-body chain.
- `TryStartRopeSolve` starts only from direct, unobstructed spans between consecutive anchors. It does not bootstrap a rope that is already wrapped around geometry.
- `MaxAllowedRopeLength` can clamp only the component owner, and only when the owner actor is the first anchor of the first segment or the last anchor of the last segment.
- Redirects attached to moving actors currently follow the actor through a cached actor-local offset. This works for prototyping, but it is still a simple attachment model.
- The component does not implement a dedicated network replication layer for rope topology.

## Integration Workflow

1. Add `URayRopeComponent` to the actor that owns the rope state.
2. Include the component owner as a terminal anchor if `MaxAllowedRopeLength` should constrain that actor.
3. Make anchor actors implement `URayRopeInterface` when the rope should attach to a specific component or socket. Without the interface, the actor location is used.
4. Call `TryStartRopeSolve` with an ordered list of anchor actors.
5. Use `OnRopeSegmentsUpdated` or `GetSegments()` to drive your own visual or gameplay representation.
6. Use `EndRopeSolve`, `BreakRopeOnSegment`, `BreakRope`, or `SetSegments` to stop or replace runtime topology.
7. Use `SetRopeDebugEnabled` and `RopeDebugSettings` when inspecting node placement and segment state in the editor.

## Runtime API

`URayRopeComponent` exposes these main Blueprint-facing operations:

- `TryStartRopeSolve(const TArray<AActor*>& AnchorActors)`
  Builds one direct base segment per consecutive anchor pair and enters active solving. It fails if there are fewer than two anchors, invalid anchors, duplicated neighboring anchors, degenerate spans, or initially blocked spans.
- `EndRopeSolve()`
  Stops active wrap, move, and relax solving after synchronizing the current topology. Existing segments remain available.
- `BreakRopeOnSegment(int32 SegmentIndex)`
  Removes one segment. If no segments remain, solving is ended and the full rope-broken event is emitted.
- `BreakRope()`
  Clears all rope topology and ends solving.
- `GetSegments()`
  Returns the component-owned current topology.
- `SetSegments(TArray<FRayRopeSegment> NewSegments)`
  Replaces topology, synchronizes attached nodes, refreshes length, and broadcasts an update.
- `SetRopeDebugEnabled(bool bEnabled)` and `IsRopeDebugEnabled()`
  Toggle and read the debug master gate.

Important runtime properties:

- `bIsRopeSolving`
  True while the component runs solver passes each tick.
- `CurrentRopeLength`
  Total length of all current segment polylines.
- `MaxAllowedRopeLength`
  Non-positive values disable owner clamping. Positive values allow `FRayRopePhysicsSolver` to pull the owner back toward the adjacent terminal rope node when the solved rope length exceeds the limit.

Events:

- `OnRopeSegmentsUpdated`
- `OnRopeSolveStarted`
- `OnRopeSolveEnded`
- `OnRopeSegmentBroken`
- `OnRopeBroken`

## Editor Settings

Trace settings:

- `TraceChannel`
  Collision channel used for all rope traces and overlap probes. Default: `ECC_Visibility`.
- `bTraceComplex`
  Passed into `FCollisionQueryParams` for rope queries.

Wrap settings:

- `bAllowWrapOnMovableObjects`
  Allows redirect nodes to attach to movable or physics-simulating hit objects.
- `MaxWrapBinarySearchIterations`
  Iteration budget for locating the clear-to-blocked boundary during wrapping.
- `WrapSolverTolerance`
  World-space tolerance for wrap convergence and duplicate redirect filtering.
- `WrapSurfaceOffset`
  Distance used to push redirect nodes away from hit surfaces.
- `GeometryCollinearityTolerance`
  Cross-product tolerance used to decide whether two surface normals should be treated as collinear.

Move settings:

- `MoveSolverTolerance`
  Geometry tolerance used by move validation.
- `MovePlaneParallelTolerance`
  Tolerance for treating two contact planes as parallel while constructing a move rail.
- `MoveEffectivePointSearchTolerance`
  Search convergence tolerance while selecting a usable rail point.
- `MoveMinMoveDistance`
  Minimum redirect displacement required before a move is worth applying.
- `MoveMinNodeSeparation`
  Minimum separation from neighboring nodes.
- `MoveMinLengthImprovement`
  Required local path-length improvement for moving a free redirect.
- `MoveMaxDistancePerIteration`
  Per-iteration move cap. `0` or less disables the cap.
- `MaxMoveIterations`
  Number of alternating forward/backward sweeps over redirect nodes.
- `MaxEffectivePointSearchIterations`
  Shared iteration budget for rail hit search, rail optimization, and transition validation.

Relax settings:

- `RelaxSolverTolerance`
  World-space tolerance for collapse target comparisons.
- `MaxRelaxCollapseIterations`
  Iteration budget for validating swept collapse transitions.

Length constraint:

- `MaxAllowedRopeLength`
  Enables owner-side length clamping when greater than zero.

Debug settings:

- `RopeDebugSettings.bDebugEnabled`
  Master gate for debug output.
- `bDrawDebugRope`
  Draws owner marker, rope spans, node spheres, labels, and optional attachment links.
- `bLogDebugState`
  Emits throttled topology snapshots to `LogRayRope`.
- `DebugLogIntervalSeconds`, draw sizes, colors, labels, and attachment-link options are configured through `FRayRopeDebugSettings`.

## Core Types

### Rope Node

`FRayRopeNode` is the smallest unit of topology.

- `NodeType` is `Anchor` or `Redirect`.
- `WorldLocation` is the synchronized location used by solvers.
- `AttachedActor` stores the anchor actor or the actor that owns a redirect offset.
- `bUseAttachedActorOffset` and `AttachedActorOffset` allow redirects on moving actors to be reconstructed from actor-local space.
- `CachedAnchorComponent` and `CachedAnchorSocketName` are transient anchor data resolved from `URayRopeInterface`.

### Rope Segment

`FRayRopeSegment` is an ordered `TArray<FRayRopeNode>`.

- Active segments are expected to contain at least two nodes.
- Segment endpoints are normally anchors.
- Redirect nodes may appear between anchors.
- If wrapping inserts an internal anchor, the topology finalization pass splits the segment around that anchor.

### Rope Anchor Interface

Actors that should behave as rope anchors can implement `URayRopeInterface`.

- `GetAnchorComponent()` returns the scene component used as the anchor source.
- `GetAnchorSocketName()` optionally returns a socket on that component.
- If the interface is absent, returns null, or provides a missing socket, the system falls back to the component location or actor location.

## Component Tick Flow

`URayRopeComponent` ticks in `TG_PostPhysics`.

If there are no segments, it only runs debug output if debug is enabled.

When `bIsRopeSolving` is false, the component:

1. Synchronizes attached nodes.
2. Refreshes `CurrentRopeLength`.
3. Applies `MaxAllowedRopeLength` owner clamping if needed.
4. Broadcasts `OnRopeSegmentsUpdated` only if the runtime clamp moved the owner.
5. Runs debug output if enabled.

When `bIsRopeSolving` is true, the component:

1. Runs `SolveRope()`.
2. Applies owner-side runtime length clamping.
3. Re-synchronizes and refreshes length if the clamp moved the owner.
4. Broadcasts `OnRopeSegmentsUpdated`.
5. Runs debug output if enabled.

## Solve Pipeline

Each segment is solved by `FRayRopeSolvePipeline` in this order:

1. Cache a reference copy of the segment nodes.
2. `SyncSegmentNodes`
   Updates anchors from their interface component/socket or actor location, and updates actor-relative redirects from cached local offsets.
3. `WrapSegment`
   Inserts anchors or redirects for spans that are blocked relative to the reference snapshot.
4. Cache a new reference copy after wrapping.
5. `MoveSegment`
   Moves existing redirects along collision-derived rails. The pass alternates forward and backward sweeps and may queue extra redirects if a moved node exposes blocked adjacent spans.
6. If movement changed the node count, refresh the reference snapshot.
7. `WrapSegment`
   Runs again so the post-move topology cannot keep newly blocked spans.
8. `RelaxSegment`
   Collapses or removes redirects that are no longer needed.
9. `SplitSegmentsOnAnchors`
   Splits any segment containing internal anchor nodes.
10. Refresh `CurrentRopeLength`.

## Wrapping Logic

Wrapping is evaluated for each neighboring node pair in the current segment.

The wrap pass follows these rules:

1. If the current span is clear, no nodes are inserted.
2. If the blocking hit actor implements `URayRopeInterface`, one `Anchor` node is inserted.
3. If redirect creation is disabled for movable geometry and the hit is on a movable or simulating object, no redirect is inserted.
4. If the reference span was already blocked, redirects are built from the current front hit and an optional reverse hit.
5. If the reference span was clear and the current span is blocked, the builder binary-searches between the reference span and current span to find a stable boundary hit. It does this in both directions when possible.

Redirect insertion then uses the surface hits:

- One hit, or two hits whose normals are not nearly collinear, produces one redirect.
- Two nearly collinear hit normals produce two redirects ordered along the base span.
- Redirects are attached to a hit actor only when the relevant hit data resolves to the same valid actor.
- Candidate insertions are deferred until the sweep finishes, then applied from back to front so span indexes stay stable.
- Duplicate insertion checks compare anchors by actor and redirects by actor-local offset or world location within `WrapSolverTolerance`.

## Redirect Placement

For a single surface hit, the solver projects the base span onto the hit plane:

`L(t) = A + t * (B - A)`

`t = dot(P - A, N) / dot(B - A, N)`

`t` is clamped to `[0, 1]`. If the span is almost parallel to the plane, the solver uses the endpoint closest to the impact point.

For two valid hit planes, the solver tries to use the closest point on the planes' intersection line to the clamped rope span. If the planes are degenerate or the result is invalid, it falls back to the front-plane projection.

The final redirect is offset by `WrapSurfaceOffset` along the surface normal for one hit, or along the normalized sum of both normals for two hits. If the summed normal is invalid, it falls back to the front normal.

## Move Logic

`MoveSegment` tries to shorten existing redirect bends while preserving collision correctness.

For each redirect, it:

1. Searches from the redirect toward its neighboring spans to recover forward and reverse surface hits.
2. Builds a movement rail from the cross product of the two hit normals.
3. If the planes are nearly parallel, falls back to the rope direction projected onto the first surface.
4. Searches along the rail between the current, previous, and next node rail parameters for a lower local path length.
5. Applies `MoveMaxDistancePerIteration`.
6. Validates the candidate with free-point checks, neighbor separation, improvement thresholds, node-path tracing, final-span tracing, and swept span-fan samples.
7. If the direct moved result is reachable but its final adjacent spans are blocked, builds extra redirects around the moved node.

Penetrating redirects are allowed to move even if the local length does not improve, because escaping invalid geometry is more important than shortening the path in that case.

## Relaxation Logic

`RelaxSegment` only operates on redirect nodes. Anchors are preserved.

For a redirect `P1` between `P0` and `P2`, the collapse target is the projection of `P1` onto the `P0 -> P2` shortcut, clamped to the shortcut segment.

The relax pass then applies these rules:

1. If `P1` is inside blocking geometry, it can be removed only when the shortcut is clear and the node could legally collapse to the shortcut.
2. If `P1` is already at the collapse target, it is removed when the shortcut is clear.
3. Otherwise, the solver tries to move `P1` to the collapse target.
4. A collapse is accepted only when the target is free, the node path is clear, the final adjacent spans are clear, and sampled intermediate span fans are clear.
5. After a successful collapse, the node is removed if the shortcut is clear.

`MaxRelaxCollapseIterations` controls the sampled transition validation budget.

## Length Constraint

`FRayRopePhysicsSolver` applies the `MaxAllowedRopeLength` runtime effect.

The clamp runs only when:

- `MaxAllowedRopeLength > 0`
- `CurrentRopeLength > MaxAllowedRopeLength`
- the component owner is a terminal anchor at the start of the first segment or the end of the last segment

When active, the solver:

1. Finds the adjacent node next to the owner anchor.
2. Removes owner velocity that points away from that adjacent node.
3. Moves the owner actor inward by the excess rope length, capped by the terminal span length.
4. Uses swept `SetActorLocation` so the clamp respects world collision.

Velocity removal supports `ACharacter` movement, simulating primitive roots, and non-simulating primitive component velocity.

## Collision Rules

- All rope queries use `TraceChannel` and `bTraceComplex`.
- The component owner is ignored by default.
- Span traces ignore valid anchor endpoint actors so the rope does not immediately collide with its own anchors.
- Redirect nodes attached to geometry are not ignored as endpoints; they still need to collide with their attached surface.
- Initial hits are accepted only when the trace is entering the surface. If a trace starts in penetration and appears to be exiting a surface, the system attempts a reverse trace to recover a usable hit.
- Free-point validation uses a small blocking overlap probe.

## Practical Summary

The current rope system is an explicit topology solver:

- It starts from direct anchor-to-anchor segments.
- It inserts anchors or redirects when traces prove a span is blocked.
- It moves redirects along collision-derived rails when that shortens the route safely.
- It removes redirects when validated shortcuts become available.
- It splits segments around newly inserted anchors.
- It optionally constrains the component owner by total solved rope length.

Rendering, advanced rope physics, replication, and final gameplay-specific behavior remain responsibilities of systems built on top of `URayRopeComponent`.
