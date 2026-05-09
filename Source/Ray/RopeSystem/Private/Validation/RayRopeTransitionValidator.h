#pragma once

#include "Trace/RayRopeTrace.h"

struct FRayRopeTransitionValidationSettings
{
	float SolverTolerance = KINDA_SMALL_NUMBER;
	int32 MaxTransitionValidationIterations = 8;
};

struct FRayRopeNodeTransition
{
	const FRayRopeNode* PrevNode = nullptr;
	const FRayRopeNode* CurrentNode = nullptr;
	const FRayRopeNode* NextNode = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
};

struct FRayRopeTransitionValidator
{
	static bool IsNodeTransitionClear(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeTransitionValidationSettings& Settings,
		const FRayRopeNodeTransition& Transition);

	static bool IsTransitionNodePathClear(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeTransitionValidationSettings& Settings,
		const FRayRopeNodeTransition& Transition);
};
