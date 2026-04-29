#pragma once

#include "RayRopeInternalTypes.h"

struct FRayRopeMoveSettings
{
};

struct FRayRopeMoveSolver
{
	static void MoveSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeMoveSettings& MoveSettings,
		FRayRopeSegment& Segment);
};
