#pragma once

#include "RayRopeInternalTypes.h"

struct FRayRopeMoveSettings
{
	float MoveSolverTolerance = KINDA_SMALL_NUMBER;
	float PlaneParallelTolerance = KINDA_SMALL_NUMBER;
	float EffectivePointSearchTolerance = KINDA_SMALL_NUMBER;
	int32 MaxMoveIterations = 4;
	int32 MaxEffectivePointSearchIterations = 8;
};

struct FRayRopeMoveSolver
{
	static void MoveSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeMoveSettings& MoveSettings,
		FRayRopeSegment& Segment);

	static bool TryFindEffectiveMovePoint(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeMoveSettings& MoveSettings,
		const FVector& PrevLocation,
		const FVector& CurrentLocation,
		const FVector& NextLocation,
		FVector& OutEffectivePoint);

private:
	static bool TryFindEffectiveMovePoint(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeMoveSettings& MoveSettings,
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		FVector& OutEffectivePoint);

	static bool TryBuildMoveRailDirection(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeMoveSettings& MoveSettings,
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		FVector& OutRailDirection);

	static bool TryFindRailDirectionSurfaceHits(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeMoveSettings& MoveSettings,
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		FHitResult& OutPrevCurrentToCurrentNextHit,
		FHitResult& OutCurrentNextToPrevCurrentHit);

	static bool TryTraceMoveHit(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeMoveSettings& MoveSettings,
		const FRayRopeNode& StartNode,
		const FRayRopeNode& EndNode,
		FHitResult& OutHit);

	static bool TryFindPlaneIntersectionRailDirection(
		const FRayRopeMoveSettings& MoveSettings,
		const FHitResult& FirstSurfaceHit,
		const FHitResult& SecondSurfaceHit,
		FVector& OutRailDirection);

	static bool TryFindEffectivePointOnRail(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeMoveSettings& MoveSettings,
		const FVector& RailDirection,
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		FVector& OutEffectivePoint);

	static FRayRopeNode CreateMovePointNode(const FVector& WorldLocation);
};
