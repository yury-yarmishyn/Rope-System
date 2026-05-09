#pragma once

#include "Types/RayRopeTypes.h"
#include "Component/RayRopeComponentSettings.h"

struct FRayRopeSolvePipeline
{
	static void SolveSegment(
		const FRayRopeComponentSolveSettings& SolveSettings,
		FRayRopeSegment& Segment,
		int32 SegmentIndex,
		TArray<FRayRopeNode>& ReferenceNodes,
		bool bLogNodeCountChanges);
};
