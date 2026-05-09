#pragma once

#include "RayRopeMoveSolver.h"

#include "Nodes/RayRopeNodeFactory.h"
#include "Validation/RayRopeTransitionValidator.h"

namespace RayRopeMoveSolverPrivate
{
struct FMoveRail
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
};

struct FMoveSolveContext
{
	const FRayRopeTraceContext& TraceContext;
	const FRayRopeMoveSettings& MoveSettings;
	FRayRopeTransitionValidationSettings TransitionValidationSettings;
	float GeometryTolerance = KINDA_SMALL_NUMBER;
	float GeometryToleranceSquared = FMath::Square(KINDA_SMALL_NUMBER);
	float EffectivePointSearchTolerance = KINDA_SMALL_NUMBER;
	float MinNodeSeparationSquared = FMath::Square(KINDA_SMALL_NUMBER);
	float MinMoveDistance = KINDA_SMALL_NUMBER;
	float MinLengthImprovement = KINDA_SMALL_NUMBER;
	float PlaneParallelToleranceSquared = FMath::Square(KINDA_SMALL_NUMBER);
	float SurfaceOffset = KINDA_SMALL_NUMBER;
	int32 MaxMoveIterations = 0;
	int32 MaxEffectivePointSearchIterations = 0;

	explicit FMoveSolveContext(
		const FRayRopeTraceContext& InTraceContext,
		const FRayRopeMoveSettings& InMoveSettings);
};

struct FMoveNodeWindow
{
	const FRayRopeNode& PrevNode;
	const FRayRopeNode& CurrentNode;
	const FRayRopeNode& NextNode;
	int32 NodeIndex = INDEX_NONE;
	mutable bool bHasCachedCurrentPointFree = false;
	mutable bool bCachedCurrentPointFree = false;

	FMoveNodeWindow(
		const FRayRopeNode& InPrevNode,
		const FRayRopeNode& InCurrentNode,
		const FRayRopeNode& InNextNode,
		int32 InNodeIndex);

	bool IsCurrentPointFree(const FRayRopeTraceContext& TraceContext) const;
};

struct FMoveResult
{
	FVector EffectivePoint = FVector::ZeroVector;
	FRayRopeBuiltNodeBuffer BeforeCurrentNodes;
	FRayRopeBuiltNodeBuffer AfterCurrentNodes;
};

bool TryFindEffectiveMove(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveResult& OutResult);

bool TryBuildMoveRail(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveRail& OutRail);

bool TryFindEffectivePointOnRail(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FMoveRail& Rail,
	FVector& OutEffectivePoint);

bool TryFindValidEffectivePoint(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& TargetPoint,
	FVector& OutEffectivePoint);

bool TryBuildMoveWithNewNodes(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& TargetPoint,
	FMoveResult& OutResult);

bool IsReachableMoveTarget(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint);

bool IsValidMovePoint(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint);

bool TryQueueMoveInsertions(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FRayRopeNode& MovedNode,
	FMoveResult& MoveResult,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions);

float CalculateMoveDistanceSum(
	const FRayRopeNode& PrevNode,
	const FVector& MiddleLocation,
	const FRayRopeNode& NextNode);

bool IsMoveImprovementSignificant(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint);

FVector ClampMoveTarget(
	const FRayRopeMoveSettings& MoveSettings,
	const FVector& CurrentPoint,
	const FVector& TargetPoint);
}
