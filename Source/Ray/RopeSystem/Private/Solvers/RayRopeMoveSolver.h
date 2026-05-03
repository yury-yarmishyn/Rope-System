#pragma once

#include "Helpers/RayRopeNodeBuilder.h"

struct FRayRopeMoveSettings
{
	FRayRopeNodeBuildSettings NodeBuildSettings;
	float MoveSolverTolerance = KINDA_SMALL_NUMBER;
	float PlaneParallelTolerance = KINDA_SMALL_NUMBER;
	float EffectivePointSearchTolerance = KINDA_SMALL_NUMBER;
	float SurfaceOffset = 0.f;
	int32 MaxMoveIterations = 4;
	int32 MaxEffectivePointSearchIterations = 8;
};

struct FRayRopeMoveSolver
{
	static void MoveSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeMoveSettings& MoveSettings,
		FRayRopeSegment& Segment);
};
