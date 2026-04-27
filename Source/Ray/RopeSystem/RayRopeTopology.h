#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.h"

struct FRayRopeTopology
{
	static bool TryGetSegmentSpan(
		const FRayRopeSegment& Segment,
		int32 NodeIndex,
		FRayRopeSpan& OutSpan);

	static void SplitSegmentsOnAnchors(TArray<FRayRopeSegment>& Segments);
	static void SplitSegmentOnAnchors(TArray<FRayRopeSegment>& Segments, int32 SegmentIndex);
};
