#pragma once

#include "CoreMinimal.h"
#include "Types/RayRopeTypes.h"

class AActor;

/**
 * Synchronizes node world locations from their owning actors.
 */
struct FRayRopeNodeSynchronizer
{
	/**
	 * Refreshes every actor-backed node in a segment.
	 *
	 * Anchor nodes query IRayRopeInterface when available. Redirect nodes use cached actor-local
	 * offsets when attached to moving geometry.
	 */
	static void SyncSegmentNodes(FRayRopeSegment& Segment);

	/**
	 * Captures the node's current world location as an actor-local offset.
	 *
	 * Clears offset state when the node has no valid attached actor.
	 */
	static void CacheAttachedActorOffset(FRayRopeNode& Node);

	/**
	 * Creates an anchor node for the actor and immediately synchronizes its component/socket target.
	 */
	static FRayRopeNode CreateAnchorNode(AActor* AnchorActor);
};
