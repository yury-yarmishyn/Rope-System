#pragma once

#include "Types/RayRopeTypes.h"
#include "Component/RayRopeComponentSettings.h"

/**
 * Orchestrates the per-segment solve passes in their required order.
 */
struct FRayRopeSolvePipeline
{
	/**
	 * Solves one segment in place.
	 *
	 * ReferenceNodes is caller-owned scratch storage used to compare the current topology against
	 * a stable pre-pass snapshot. The pipeline may insert, move, collapse, or remove redirect nodes.
	 */
	static void SolveSegment(
		const FRayRopeComponentSolveSettings& SolveSettings,
		FRayRopeSegment& Segment,
		int32 SegmentIndex,
		TArray<FRayRopeNode>& ReferenceNodes,
		bool bLogNodeCountChanges);
};
