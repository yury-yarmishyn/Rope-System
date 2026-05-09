#pragma once

#include "Trace/RayRopeTrace.h"

/** Small buffer used because a single blocked span can produce at most two redirect nodes. */
using FRayRopeBuiltNodeBuffer = TArray<FRayRopeNode, TInlineAllocator<2>>;

/** Per-pass insertion queue keyed by the segment index where each node should be inserted. */
using FRayRopePendingNodeInsertionBuffer = TArray<TPair<int32, FRayRopeNode>, TInlineAllocator<8>>;

struct FRayRopeNodeBuildSettings;

/**
 * Validates and applies deferred node insertions.
 *
 * Insertions are queued during solver sweeps so node indexes remain stable until the pass finishes.
 */
struct FRayRopeNodeInsertionQueue
{
	/**
	 * Checks whether candidate nodes can be inserted between two neighbors.
	 *
	 * Rejects duplicates against neighbors, other candidates, and nearby pending insertions.
	 */
	static bool CanInsertNodes(
		const FRayRopeNodeBuildSettings& Settings,
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& NextNode,
		int32 InsertIndex,
		TConstArrayView<FRayRopeNode> Candidates,
		const FRayRopePendingNodeInsertionBuffer& PendingInsertions);

	static bool CanInsertNodesInSegment(
		const FRayRopeNodeBuildSettings& Settings,
		int32 InsertIndex,
		const FRayRopeSegment& Segment,
		TConstArrayView<FRayRopeNode> Candidates,
		const FRayRopePendingNodeInsertionBuffer& PendingInsertions);

	/**
	 * Moves built nodes into the pending queue at the requested insert index.
	 */
	static void AppendPendingInsertions(
		int32 InsertIndex,
		FRayRopeBuiltNodeBuffer& Nodes,
		FRayRopePendingNodeInsertionBuffer& PendingInsertions);

	/**
	 * Applies queued insertions from back to front and clears the queue.
	 */
	static void ApplyPendingInsertions(
		FRayRopeSegment& Segment,
		FRayRopePendingNodeInsertionBuffer& PendingInsertions);
};
