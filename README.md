# Ray Rope System

This project contains a custom rope logic component for Unreal Engine. The rope is modeled as a chain of logical nodes grouped into segments and treated as a piecewise-linear path in world space. The component does not render the rope by itself and it does not run a full physics simulation. Its job is to keep rope topology consistent while anchors move and while the rope wraps or unwraps around world geometry. At the moment, the system can stably process up to 100 rope points simultaneously without a major performance drop.

## Problem Statement

For gameplay rope systems in Unreal Engine, two popular starting points are `CableComponent` and chains built from `PhysicsConstraint`.

- `CableComponent` is useful for drawing and simulating a cable-like shape, but for this task it does not maintain explicit rope topology at geometry contact. It does not produce stable logical wrap points, deterministic unwrapping, or a persistent "the rope now bends around this obstacle" state.
- `PhysicsConstraint` can approximate a rope as rigid bodies connected by joints, but for this task it still delegates geometry contact to the physics solver frame by frame. It does not naturally expose persistent bend nodes, deterministic routing, or clean anchor insertion and segment splitting when the rope touches gameplay geometry.
- In both cases the core problem remains unsolved: correct rope behavior at contact with level geometry is hard to keep stable and reproducible.
- This project solves a different problem. Instead of asking the engine to infer rope routing implicitly, it stores the routing explicitly as anchors and redirect nodes and updates that topology deterministically every tick.

## Practical Gameplay Use

This system is gameplay-first. Its main purpose is to provide reliable rope topology for interactive mechanics, not to simulate a fully physical rope.

- It can be used as the gameplay backbone for a grappling hook style mechanic.
- It can be used for tripwire mechanics and other trigger-driven rope interactions.
- It can be used for pseudo-physical interactions where the rope shape matters more than strict physics accuracy.
- It can be used for pulling-style interactions, such as dragging an enemy toward the player, or for puzzle setups that rely on rope routing and tension logic. This part is still work in progress.

## Current Limitations

The current implementation can attach redirect nodes to moving actors by caching a local offset and reconstructing the node position every tick.

This is a temporary solution. It exists to keep moving-object support usable during gameplay prototyping, but it is expected to be redesigned in a later iteration.

## Core Concepts

### Rope Node
`FRayRopeNode` is the smallest rope unit.

- `Anchor` nodes represent rope attachment points.
- `Redirect` nodes represent temporary bend points created when the rope hits geometry.
- Every node stores a `WorldLocation`.
- A node can also store an `AttachActor` and a cached local-space offset so it can follow a moving actor.

### Rope Segment
`FRayRopeSegment` is an ordered array of rope nodes.

- A valid segment is expected to have at least two nodes.
- The first and last nodes are typically anchors.
- Redirect nodes can exist between anchors when the rope bends around surfaces.
- If the rope hits another valid rope anchor actor, the current segment can be split into multiple segments.

### Rope Anchor Interface
Actors that should behave as rope anchors can implement `URayRopeInterface`.

- `GetAnchorComponent()` returns the scene component used as the anchor source.
- `GetAnchorSocketName()` optionally returns a socket on that component.
- If the interface is not implemented or returns invalid data, the actor location is used as fallback.

## Mathematical Model

The solver represents each rope segment as a polyline

`P = {x0, x1, ..., xn}`

where every `xi` is a rope node in world space.

- Each neighboring pair defines one straight rope span
  `Li(t) = xi + t * (xi+1 - xi), t in [0, 1]`.
- `Anchor` nodes are fixed by gameplay attachment data.
- `Redirect` nodes are solver-generated bend points that appear only when geometry requires the rope to change direction.
- If a redirect is attached to a moving actor, the solver stores a local offset
  `x_local = T_actor^-1 * x_world`
  and reconstructs the node every tick as
  `x_world = T_actor * x_local`.
- The full rope state is therefore not a particle simulation. It is a routed polyline whose topology changes only when visibility or collision tests prove that a new bend is necessary or an old bend is no longer needed.

## Component Flow

`URayRopeComponent` ticks every frame in `TG_PostPhysics` and runs `SolveRope()`. For every segment, it first captures a reference copy of the segment before mutation, then updates the current segment against that reference.

For each segment, the solver currently performs these steps:

1. `SyncSegmentAnchors`
   Updates the first and last anchor positions from their attached actors or anchor sockets.
2. `SyncAttachedRedirectNodes`
   Rebuilds redirect node world positions from the cached local offset if the redirect is attached to a moving actor.
3. `MoveSegment`
   Placeholder hook for future movement or constraint logic. It is currently a no-op.
4. `WrapSegment`
   Detects when a straight rope line now intersects geometry and inserts new nodes.
5. `RelaxSegment`
   Removes redirect nodes that are no longer needed.
6. `SplitSegmentOnAnchors`
   Splits a segment into smaller segments if a new anchor node appeared inside it.

After solving, the component broadcasts `OnSegmentsSet` so Blueprint or other systems can react to the latest rope layout.

## Wrapping Logic

Wrapping is evaluated on each neighboring node pair `(A, B)` of the current polyline.

- The solver traces the current span `A -> B`.
- If that trace has no blocking hit, the span remains valid and no topology change is needed.
- If the hit actor implements `URayRopeInterface`, the solver inserts a new `Anchor` node instead of a redirect.
- Otherwise, the solver inserts one or more `Redirect` nodes so that one blocked straight span becomes several valid spans.
- The current span is always compared against the corresponding reference span `(A_ref, B_ref)` captured before the tick. This lets the solver distinguish "this rope was already wrapped" from "this rope became wrapped during this frame".

### Boundary Search
If the reference span was not colliding but the current span is, the solver searches for the transition between a valid line and an invalid line.

- The number of iterations is controlled by `MaxBinarySearchIteration`.
- The early stop threshold is controlled by `WrapSolverEpsilon`.
- The midpoint line is built by interpolating both endpoints independently:
  `A_mid = lerp(A_valid, A_invalid, 0.5)`
  `B_mid = lerp(B_valid, B_invalid, 0.5)`.
- If `Trace(A_mid, B_mid)` hits geometry, the midpoint becomes the new invalid line.
- Otherwise, it becomes the new valid line.
- After the search converges, the solver traces the final invalid line again and uses that blocking hit as the collision boundary used for redirect generation.

This gives a stable approximation of the moment when the rope span first starts touching geometry, without requiring a continuous simulation over substeps.

### Redirect Placement
Redirect nodes are placed from collision geometry rather than from accumulated physics impulses.

For a single surface hit, the solver projects the valid rope span onto the hit plane. With

`L(t) = A + t * (B - A)`

and plane equation

`n . (x - p) = 0`

it computes

`t = dot(p - A, n) / dot(B - A, n)`

then clamps `t` to `[0, 1]` and places the redirect at

`R = A + t * (B - A)`.

- If the denominator is nearly zero, the line is almost parallel to the plane and the solver falls back to the closer rope endpoint.
- The final redirect is offset by `normalize(n) * RopePhysicalRadius` so the rope stays outside the surface.

For front and back hits, the solver tries to build one corner node from the intersection of two hit planes.

- Let `n1` and `n2` be the normalized impact normals.
- The intersection direction is `dir = n1 x n2`.
- The plane scalars are `w1 = dot(n1, p1)` and `w2 = dot(n2, p2)`.
- One point on the intersection line is
  `x0 = ((w1 * n2 - w2 * n1) x dir) / |dir|^2`.
- The solver then finds the point on the rope span closest to that line by minimizing
  `|(A + s * u) - (x0 + t * dir)|^2`
  where `u = B - A` and `s` is clamped to `[0, 1]`.
- If the closest distance to the intersection line is larger than `WrapSolverEpsilon`, the corner fit is rejected and the solver falls back to the single-plane solution.
- If the normals are nearly parallel, the solver creates two redirect nodes instead of one corner node.
- For the two-plane case, the outward offset direction is `normalize(n1 + n2) * RopePhysicalRadius`. If `n1 + n2` is nearly zero, it falls back to `n1`.

When a redirect attaches to an actor, its local offset is cached. On later ticks the node is reconstructed from that local-space offset, which lets the rope follow moving geometry.

## Relaxation Logic

`RelaxSegment` removes redirect nodes that are no longer required.

A redirect node `P1` between neighbors `P0` and `P2` is considered removable only if the direct shortcut `P0 -> P2` is not blocked. After that visibility test passes, the solver applies geometric simplification rules.

- If `|P1 - P0|` or `|P2 - P1|` is smaller than `RelaxSolverEpsilon`, the bend is degenerate and the node is removed.
- Let `v1 = normalize(P1 - P0)` and `v2 = normalize(P2 - P1)`. If `|v1 x v2|^2 <= RelaxCollinearEpsilon^2`, the bend is almost collinear and the node is removed.
- The closest point on the supporting line through `P0` and `P2` is computed as
  `Q = P0 + t * (P2 - P0)`
  where
  `t = dot(P1 - P0, P2 - P0) / |P2 - P0|^2`.
- If `|P1 - Q| <= RelaxSolverEpsilon`, the redirect is already close enough to the straight line and is removed.
- Otherwise the solver traces `P1 -> Q`. If that path is unobstructed, the redirect can collapse safely back toward the straight line and is removed.

This keeps the rope from accumulating stale bend points after geometry is cleared or after anchors move.

## Segment Splitting

If `WrapSegment` inserts an anchor actor into the middle of a segment, `SplitSegmentOnAnchors` converts the single segment into multiple segments.

Example:

- Before split: `Anchor A -> Redirect -> Anchor B -> Redirect -> Anchor C`
- After split:
  - `Anchor A -> Redirect -> Anchor B`
  - `Anchor B -> Redirect -> Anchor C`

This ensures every segment stays local to a single anchor-to-anchor path.

## Collision Rules

All rope traces use `ECC_Visibility`.

- The rope component owner is ignored.
- Anchor actors already present in the segment are ignored.
- Initial penetration hits are retried from the opposite direction to reduce false positives.

## Practical Summary

The current rope system is a topology solver:

- Anchors define the endpoints.
- Redirect nodes appear when geometry forces the rope to bend.
- Redirect nodes disappear when a straight path becomes valid again.
- New anchor hits can break one rope path into multiple segments.

This makes the rope deterministic and easy to drive from gameplay code or Blueprint, while leaving rendering and advanced physical motion open for later work. The key design choice is that geometry contact is solved explicitly as a routing problem, rather than left implicit inside `CableComponent` or `PhysicsConstraint`.
