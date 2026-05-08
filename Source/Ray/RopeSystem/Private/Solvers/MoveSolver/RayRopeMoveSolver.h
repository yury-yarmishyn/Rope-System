#pragma once

#include "Helpers/RayRopeNodeBuilder.h"

struct FRayRopeMoveSettings
{
	FRayRopeNodeBuildSettings NodeBuildSettings;
	float MoveSolverTolerance = KINDA_SMALL_NUMBER;
	float PlaneParallelTolerance = KINDA_SMALL_NUMBER;
	float EffectivePointSearchTolerance = KINDA_SMALL_NUMBER;
	float MinMoveDistance = 0.05f;
	float MinNodeSeparation = 0.05f;
	float MinLengthImprovement = 0.01f;
	float MaxMoveDistancePerIteration = 2.f;
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
