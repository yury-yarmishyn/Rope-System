#pragma once

#include "RayRopeMoveSolverInternal.h"

namespace RayRopeMoveSolverPrivate
{
constexpr int32 GMinGlobalMoveRedirects = 3;

/**
 * One redirect participating in the batch rail-coordinate solve.
 *
 * DeltaParameter is the proposed displacement along Rail.Direction before the shared alpha is applied.
 */
struct FGlobalMoveNodeState
{
	int32 NodeIndex = INDEX_NONE;
	FVector StartLocation = FVector::ZeroVector;
	FMoveRail Rail;
	float DeltaParameter = 0.f;
};

/**
 * Tridiagonal approximation of segment length in per-redirect rail coordinates.
 */
struct FGlobalMoveSystem
{
	TArray<float, TInlineAllocator<32>> Gradient;
	TArray<float, TInlineAllocator<32>> Diagonal;
	TArray<float, TInlineAllocator<32>> Upper;
	TArray<float, TInlineAllocator<32>> Delta;
};

struct FGlobalMoveStateBuildResult
{
	int32 RedirectCount = 0;
	int32 StateCount = 0;
	bool bSkippedAnyRedirect = false;
};

enum class EGlobalMoveStepStatus : uint8
{
	Failed,
	Converged,
	Ready
};

template <typename ElementType>
bool IsValidViewIndex(TConstArrayView<ElementType> View, int32 Index)
{
	return Index >= 0 && Index < View.Num();
}

FGlobalMoveStateBuildResult BuildGlobalMoveNodeStates(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TArray<FGlobalMoveNodeState, TInlineAllocator<32>>& OutStates,
	TArray<int32, TInlineAllocator<64>>& OutNodeToStateIndex);

FVector GetNodeLocationAtAlpha(
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	int32 NodeIndex,
	float Alpha);

float CalculateSegmentLengthAtAlpha(
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	float Alpha);

/**
 * Builds the local length objective approximation for the current rail states.
 */
bool BuildGlobalMoveSystem(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	FGlobalMoveSystem& OutSystem);

/**
 * Solves the tridiagonal system with the Thomas algorithm.
 */
bool SolveTridiagonalSystem(FGlobalMoveSystem& System);

float CopyDeltaToStates(
	const FGlobalMoveSystem& System,
	TArray<FGlobalMoveNodeState, TInlineAllocator<32>>& States);

float CalculateInitialLineSearchAlpha(
	const FRayRopeMoveSettings& MoveSettings,
	float MaxAbsDelta);

bool BuildAffectedSpanRanges(
	TConstArrayView<FGlobalMoveNodeState> States,
	float Alpha,
	float MoveThreshold,
	int32 MaxSpanIndex,
	FRayRopeAffectedSpanRangeBuffer& OutRanges);

/**
 * Validates final and sampled intermediate locations for a shared-alpha batch move.
 */
bool IsBatchMoveClearAtAlpha(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	TConstArrayView<FRayRopeSpanIndexRange> AffectedSpanRanges,
	float Alpha);

/**
 * Runs backtracking line search over the shared batch alpha.
 */
bool TryFindAcceptedAlpha(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	float InitialAlpha,
	float MaxAbsDelta,
	float CurrentLength,
	float& OutAcceptedAlpha,
	FRayRopeAffectedSpanRangeBuffer& OutAffectedSpanRanges);
}
