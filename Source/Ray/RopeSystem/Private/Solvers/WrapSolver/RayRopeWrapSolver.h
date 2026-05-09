#pragma once

#include "Nodes/RayRopeNodeBuilder.h"

struct FRayRopeWrapSolver
{
	static void WrapSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeNodeBuildSettings& WrapSettings,
		FRayRopeSegment& Segment,
		TConstArrayView<FRayRopeNode> ReferenceNodes);
};

