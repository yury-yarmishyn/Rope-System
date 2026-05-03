#pragma once

#include "Helpers/RayRopeNodeBuilder.h"

struct FRayRopeWrapSettings : FRayRopeNodeBuildSettings
{
};

struct FRayRopeWrapSolver
{
	static void WrapSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeWrapSettings& WrapSettings,
		FRayRopeSegment& Segment,
		TConstArrayView<FRayRopeNode> ReferenceNodes);
};
