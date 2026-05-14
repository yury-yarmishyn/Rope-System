#pragma once

#include "RayRopeMoveSolver.h"

#include "Debug/RayRopeDebugConfig.h"
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
	const FRayRopeNodeBuildSettings& NodeBuildSettings;
	FRayRopeTransitionValidationSettings TransitionValidationSettings;
	float GeometryTolerance = KINDA_SMALL_NUMBER;
	float GeometryToleranceSquared = FMath::Square(KINDA_SMALL_NUMBER);
	float EffectivePointSearchTolerance = KINDA_SMALL_NUMBER;
	float MinNodeSeparationSquared = FMath::Square(KINDA_SMALL_NUMBER);
	float MinMoveDistance = KINDA_SMALL_NUMBER;
	float MinLengthImprovement = KINDA_SMALL_NUMBER;
	float PlaneParallelToleranceSquared = FMath::Square(KINDA_SMALL_NUMBER);
	float SurfaceOffset = KINDA_SMALL_NUMBER;
	float GlobalMoveDamping = KINDA_SMALL_NUMBER;
	int32 MaxMoveIterations = 0;
	int32 MaxTopologyRepairIterations = 0;
	int32 MaxRailSurfaceSearchIterations = 0;
	int32 MaxRailPointSearchIterations = 0;
	int32 MaxCandidateBacktrackIterations = 0;
	int32 MaxTransitionValidationIterations = 0;
	int32 MaxGlobalValidationSamples = 0;
	int32 MaxGlobalMoveIterations = 0;
	int32 MaxGlobalMoveLineSearchSteps = 0;

	explicit FMoveSolveContext(
		const FRayRopeTraceContext& InTraceContext,
		const FRayRopeMoveSettings& InMoveSettings,
		const FRayRopeNodeBuildSettings& InNodeBuildSettings);
};

enum class EGlobalMoveSolveStatus : uint8
{
	/** Segment or settings are outside the batch solver's supported scope. */
	NotApplicable,

	/** A valid batch solve found no movement. */
	Converged,

	/** At least one direct-only batch step was accepted. */
	Applied,

	/** The batch solver failed before it could produce a clean step. */
	Failed
};

enum class EMoveDecisionType : uint8
{
	None,
	Move,
	Insert,
	MoveAndInsert
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

struct FMoveValidation
{
	EMoveDecisionType Type = EMoveDecisionType::None;
	FVector TargetPoint = FVector::ZeroVector;
	bool bPrevSpanBlocked = false;
	bool bNextSpanBlocked = false;
#if RAYROPE_WITH_DEBUG
	const TCHAR* DebugReason = TEXT("Rejected");
#endif

	bool ShouldMoveNode() const
	{
		return Type == EMoveDecisionType::Move || Type == EMoveDecisionType::MoveAndInsert;
	}

	bool NeedsInsertions() const
	{
		return Type == EMoveDecisionType::Insert || Type == EMoveDecisionType::MoveAndInsert;
	}

	bool IsAccepted() const
	{
		return Type != EMoveDecisionType::None;
	}
};

struct FMovePassResult
{
	FRayRopeSolveResult SolveResult;
	bool bAppliedAnyMove = false;
	bool bFullyHandled = true;
	bool bHadFailure = false;
	bool bNeedsLocalFallback = false;
};

struct FMoveCommand
{
	EMoveDecisionType Type = EMoveDecisionType::None;
	FVector TargetPoint = FVector::ZeroVector;
	FRayRopeBuiltNodeBuffer BeforeCurrentNodes;
	FRayRopeBuiltNodeBuffer AfterCurrentNodes;

	bool ShouldMoveNode() const
	{
		return Type == EMoveDecisionType::Move || Type == EMoveDecisionType::MoveAndInsert;
	}

	bool NeedsInsertions() const
	{
		return Type == EMoveDecisionType::Insert || Type == EMoveDecisionType::MoveAndInsert;
	}

	bool HasInsertions() const
	{
		return BeforeCurrentNodes.Num() > 0 || AfterCurrentNodes.Num() > 0;
	}

	bool IsAccepted() const
	{
		return Type != EMoveDecisionType::None;
	}
};

bool TryFindEffectiveMove(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveCommand& OutCommand);

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
 * Resolves the ideal target into either a direct move or move/topology insertion.
 */
bool TryResolveCandidateOnPath(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& TargetPoint,
	FMoveCommand& OutCommand);

/**
 * Converts a move validation result into an executable command.
 */
bool TryBuildMoveCommand(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FMoveValidation& Validation,
	FMoveCommand& OutCommand);

/**
 * Evaluates whether the current adjacent spans already require topology repair.
 */
FMoveValidation EvaluateCurrentSpans(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow);

FMoveValidation EvaluateMoveCandidate(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint);

/**
 * Validates and queues any insertions produced by a move result.
 */
bool TryQueueMoveInsertions(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveCommand& Command,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions);

/**
 * Runs a batch rail-coordinate move over multiple redirects.
 *
 * Rails are cached only inside one global iteration and rebuilt after accepted movement.
 */
struct FGlobalMoveResult
{
	EGlobalMoveSolveStatus Status = EGlobalMoveSolveStatus::NotApplicable;
	FMovePassResult PassResult;
	bool bSkippedAnyRedirect = false;
};

FGlobalMoveResult TryMoveSegmentGlobal(
	const FMoveSolveContext& SolveContext,
	FRayRopeSegment& Segment);

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
