#pragma once

#include "Helpers/RayRopeTrace.h"

struct FRayRopeRelaxSettings
{
	float RelaxSolverTolerance = 0.f;
	int32 MaxRelaxCollapseIterations = 8;
};

struct FRayRopeRelaxSolver
{
	static void RelaxSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeRelaxSettings& RelaxSettings,
		FRayRopeSegment& Segment);
};
