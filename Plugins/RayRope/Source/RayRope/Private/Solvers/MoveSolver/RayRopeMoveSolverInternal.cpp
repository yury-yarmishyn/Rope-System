#include "RayRopeMoveSolverInternal.h"

namespace RayRopeMoveSolverPrivate
{
FMoveSolveContext::FMoveSolveContext(
	const FRayRopeTraceContext& InTraceContext,
	const FRayRopeMoveSettings& InMoveSettings,
	const FRayRopeNodeBuildSettings& InNodeBuildSettings)
	: TraceContext(InTraceContext)
	, MoveSettings(InMoveSettings)
	, NodeBuildSettings(InNodeBuildSettings)
{
	GeometryTolerance = FMath::Max(
		FMath::Min(MoveSettings.MoveSolverTolerance, MoveSettings.MinMoveDistance),
		KINDA_SMALL_NUMBER);
	GeometryToleranceSquared = FMath::Square(GeometryTolerance);
	EffectivePointSearchTolerance = FMath::Max(
		MoveSettings.EffectivePointSearchTolerance,
		KINDA_SMALL_NUMBER);
	MinNodeSeparationSquared =
		FMath::Square(FMath::Max(MoveSettings.MinNodeSeparation, KINDA_SMALL_NUMBER));
	MinMoveDistance = FMath::Max(MoveSettings.MinMoveDistance, KINDA_SMALL_NUMBER);
	MinLengthImprovement = FMath::Max(MoveSettings.MinLengthImprovement, KINDA_SMALL_NUMBER);
	PlaneParallelToleranceSquared = FMath::Square(MoveSettings.PlaneParallelTolerance);
	SurfaceOffset = FMath::Max(MoveSettings.SurfaceOffset, KINDA_SMALL_NUMBER);
	GlobalMoveDamping = FMath::Max(MoveSettings.GlobalMoveDamping, KINDA_SMALL_NUMBER);
	MaxMoveIterations = FMath::Max(0, MoveSettings.MaxMoveIterations);
	MaxEffectivePointSearchIterations =
		FMath::Max(0, MoveSettings.MaxEffectivePointSearchIterations);
	MaxGlobalMoveIterations = FMath::Max(0, MoveSettings.MaxGlobalMoveIterations);
	MaxGlobalMoveLineSearchSteps = FMath::Max(0, MoveSettings.MaxGlobalMoveLineSearchSteps);

	TransitionValidationSettings.SolverTolerance = GeometryTolerance;
	TransitionValidationSettings.MaxTransitionValidationIterations =
		MaxEffectivePointSearchIterations;
}

FMoveNodeWindow::FMoveNodeWindow(
	const FRayRopeNode& InPrevNode,
	const FRayRopeNode& InCurrentNode,
	const FRayRopeNode& InNextNode,
	int32 InNodeIndex)
	: PrevNode(InPrevNode)
	, CurrentNode(InCurrentNode)
	, NextNode(InNextNode)
	, NodeIndex(InNodeIndex)
{
}

bool FMoveNodeWindow::IsCurrentPointFree(const FRayRopeTraceContext& TraceContext) const
{
	if (!bHasCachedCurrentPointFree)
	{
		bCachedCurrentPointFree = FRayRopeTrace::IsValidFreePoint(
			TraceContext,
			CurrentNode.WorldLocation);
		bHasCachedCurrentPointFree = true;
	}

	return bCachedCurrentPointFree;
}

bool TryFindEffectiveMove(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveResult& OutResult)
{
	OutResult = FMoveResult();

	FMoveRail Rail;
	if (!TryBuildMoveRail(
		SolveContext,
		NodeWindow,
		Rail))
	{
		return false;
	}

	FVector TargetPoint = FVector::ZeroVector;
	if (!TryFindEffectivePointOnRail(
		SolveContext,
		NodeWindow,
		Rail,
		TargetPoint))
	{
		return false;
	}

	TargetPoint = ClampMoveTarget(
		SolveContext.MoveSettings,
		NodeWindow.CurrentNode.WorldLocation,
		TargetPoint);
	if (!TryFindValidEffectivePoint(
		SolveContext,
		NodeWindow,
		TargetPoint,
		OutResult.EffectivePoint))
	{
		return TryBuildMoveWithNewNodes(
			SolveContext,
			NodeWindow,
			TargetPoint,
			OutResult);
	}

	return true;
}

float CalculateMoveDistanceSum(
	const FRayRopeNode& PrevNode,
	const FVector& MiddleLocation,
	const FRayRopeNode& NextNode)
{
	return FVector::Dist(PrevNode.WorldLocation, MiddleLocation) +
		FVector::Dist(MiddleLocation, NextNode.WorldLocation);
}

FVector ClampMoveTarget(
	const FRayRopeMoveSettings& MoveSettings,
	const FVector& CurrentPoint,
	const FVector& TargetPoint)
{
	if (CurrentPoint.ContainsNaN() || TargetPoint.ContainsNaN())
	{
		return TargetPoint;
	}

	const float MaxMoveDistance = MoveSettings.MaxMoveDistancePerIteration;
	if (MaxMoveDistance <= 0.f)
	{
		return TargetPoint;
	}

	const FVector MoveDelta = TargetPoint - CurrentPoint;
	if (MoveDelta.SizeSquared() <= FMath::Square(MaxMoveDistance))
	{
		return TargetPoint;
	}

	const FVector MoveDirection = MoveDelta.GetSafeNormal();
	return MoveDirection.IsNearlyZero()
		? TargetPoint
		: CurrentPoint + MoveDirection * MaxMoveDistance;
}

}
