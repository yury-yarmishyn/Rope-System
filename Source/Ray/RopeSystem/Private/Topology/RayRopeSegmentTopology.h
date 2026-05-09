#pragma once

#include "Trace/RayRopeTrace.h"

/**
 * Topology queries and maintenance for rope segments.
 */
struct FRayRopeSegmentTopology
{
	/**
	 * Returns a non-owning span for Segment.Nodes[NodeIndex] to Segment.Nodes[NodeIndex + 1].
	 */
	static bool TryGetSegmentSpan(
		const FRayRopeSegment& Segment,
		int32 NodeIndex,
		FRayRopeSpan& OutSpan);

	/**
	 * Returns a non-owning span for adjacent nodes in any node view.
	 */
	static bool TryGetNodeSpan(
		TConstArrayView<FRayRopeNode> Nodes,
		int32 NodeIndex,
		FRayRopeSpan& OutSpan);

	static float CalculateSegmentLength(const FRayRopeSegment& Segment);

	static float CalculateRopeLength(const TArray<FRayRopeSegment>& Segments);

	/**
	 * Splits segments at internal anchor nodes so every resulting segment has anchor endpoints.
	 */
	static void SplitSegmentsOnAnchors(TArray<FRayRopeSegment>& Segments);
};
