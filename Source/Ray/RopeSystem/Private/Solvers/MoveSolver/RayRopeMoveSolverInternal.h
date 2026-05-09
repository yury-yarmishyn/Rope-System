#pragma once

#include "RayRopeMoveSolver.h"

#include "Nodes/RayRopeNodeFactory.h"
#include "Validation/RayRopeTransitionValidator.h"

namespace RayRopeMoveSolverPrivate
{
/** Collision-derived line along which a redirect can slide. */
struct FMoveRail
{
	/** World-space point on the rail after applying surface offset. */
	FVector Origin = FVector::ZeroVector;

	/** Normalized rail direction. */
	FVector Direction = FVector::ZeroVector;
};

/**
 * Normalized per-pass move settings.
 *
 * Constructor clamps tolerances and caches squared values used by inner loops.
 */
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

/**
 * Three-node view used when moving one redirect between fixed neighbors.
 */
struct FMoveNodeWindow
{
	/** Previous fixed node in the segment. */
	const FRayRopeNode& PrevNode;

	/** Redirect node being evaluated for movement. */
	const FRayRopeNode& CurrentNode;

	/** Next fixed node in the segment. */
	const FRayRopeNode& NextNode;

	/** CurrentNode index in the owning segment. */
	int32 NodeIndex = INDEX_NONE;

	mutable bool bHasCachedCurrentPointFree = false;
	mutable bool bCachedCurrentPointFree = false;

	FMoveNodeWindow(
		const FRayRopeNode& InPrevNode,
		const FRayRopeNode& InCurrentNode,
		const FRayRopeNode& InNextNode,
		int32 InNodeIndex);

	/**
	 * Caches whether the current redirect is already free of blocking geometry.
	 */
	bool IsCurrentPointFree(const FRayRopeTraceContext& TraceContext) const;
};

/**
 * Result of a redirect move, including any redirects needed around the moved node.
 */
struct FMoveResult
{
	/** Final world location for CurrentNode if the move is accepted. */
	FVector EffectivePoint = FVector::ZeroVector;

	/** Nodes to insert between PrevNode and the moved CurrentNode. */
	FRayRopeBuiltNodeBuffer BeforeCurrentNodes;

	/** Nodes to insert between the moved CurrentNode and NextNode. */
	FRayRopeBuiltNodeBuffer AfterCurrentNodes;
};

bool TryFindEffectiveMove(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveResult& OutResult);

/**
 * Builds a rail from the two contact surfaces around CurrentNode.
 */
bool TryBuildMoveRail(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveRail& OutRail);

/**
 * Searches along a rail for the point that best reduces local rope length.
 */
bool TryFindEffectivePointOnRail(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FMoveRail& Rail,
	FVector& OutEffectivePoint);

/**
 * Clamps an ideal target back to the nearest valid reachable point when the ideal move is blocked.
 */
bool TryFindValidEffectivePoint(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& TargetPoint,
	FVector& OutEffectivePoint);

/**
 * Builds extra redirects around a reachable target when moving directly would block adjacent spans.
 */
bool TryBuildMoveWithNewNodes(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& TargetPoint,
	FMoveResult& OutResult);

/**
 * Checks coarse move reachability without requiring final adjacent spans to be clear.
 */
bool IsReachableMoveTarget(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint);

/**
 * Validates that a candidate point can replace CurrentNode without adding new redirects.
 */
bool IsValidMovePoint(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint);

/**
 * Validates and queues any insertions produced by a move result.
 */
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

/**
 * Returns whether CandidatePoint improves enough to justify moving the redirect.
 *
 * Penetrating redirects are allowed to move even when the local length does not improve.
 */
bool IsMoveImprovementSignificant(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint);

/**
 * Applies the per-iteration movement cap without changing the final direction.
 */
FVector ClampMoveTarget(
	const FRayRopeMoveSettings& MoveSettings,
	const FVector& CurrentPoint,
	const FVector& TargetPoint);
}
