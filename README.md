# Ray Rope System

This project contains a custom rope logic component for Unreal Engine. The rope is modeled as a chain of logical nodes grouped into segments. The component does not render the rope by itself and it does not run a full physics simulation. Its job is to keep rope topology consistent while anchors move and while the rope wraps or unwraps around world geometry.

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

## Component Flow

`URayRopeComponent` ticks every frame in `TG_PostPhysics` and runs `SolveRope()`.

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

Wrapping is based on line traces between neighboring nodes.

- The component compares the current segment against a reference copy captured before the solver mutates it.
- If the current node pair now hits geometry, the solver examines what was hit.
- If the hit actor implements `URayRopeInterface`, the solver inserts a new `Anchor` node.
- Otherwise, the solver creates one or two `Redirect` nodes to bend the rope around blocking geometry.

### Boundary Search
If the reference segment was not colliding but the current one is, the component uses a binary search to find the transition between the valid line and the invalid line.

- The number of iterations is controlled by `MaxBinarySearchIteration`.
- The early stop threshold is controlled by `WrapSolverEpsilon`.

This gives a stable approximation of the point where the rope starts touching geometry.

### Redirect Placement
Redirect nodes are placed using the collision data:

- For a single surface hit, the solver projects the valid rope line onto the hit plane.
- For front and back hits, it tries to use the intersection line of the two planes.
- If the planes are nearly parallel, it creates two redirect nodes instead of one combined corner node.
- The final redirect position is pushed away from the surface by `RopePhysicalRadius` so the rope does not sit directly inside geometry.

When a redirect attaches to an actor, its local offset is cached. On later ticks the node is reconstructed from that local-space offset, which lets the rope follow moving geometry.

## Relaxation Logic

`RelaxSegment` removes redirect nodes that are no longer required.

A redirect can be removed when one of these conditions is true:

- The previous and next nodes can see each other directly with no blocking hit.
- One of the neighboring directions is effectively zero-length.
- The redirect lies on an almost straight line, based on `RelaxCollinearEpsilon`.
- The redirect is already close enough to the straight segment according to `RelaxSolverEpsilon`.
- A direct path from the redirect to the closest point on the straight segment is unobstructed.

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

This makes the rope deterministic and easy to drive from gameplay code or Blueprint, while leaving rendering and advanced physical motion open for later work.
