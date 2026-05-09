#pragma once

#include "Trace/RayRopeTrace.h"

struct FRayRopeSegmentTopology
{
	static bool TryGetSegmentSpan(
		const FRayRopeSegment& Segment,
		int32 NodeIndex,
		FRayRopeSpan& OutSpan);

	static bool TryGetNodeSpan(
		TConstArrayView<FRayRopeNode> Nodes,
		int32 NodeIndex,
		FRayRopeSpan& OutSpan);

	static float CalculateSegmentLength(const FRayRopeSegment& Segment);

	static float CalculateRopeLength(const TArray<FRayRopeSegment>& Segments);

	static void SplitSegmentsOnAnchors(TArray<FRayRopeSegment>& Segments);
};
