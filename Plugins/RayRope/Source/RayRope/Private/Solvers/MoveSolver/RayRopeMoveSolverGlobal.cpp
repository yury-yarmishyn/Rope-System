#include "RayRopeMoveSolverGlobalInternal.h"

#include "Debug/RayRopeDebugConfig.h"
#include "Nodes/RayRopeNodeSynchronizer.h"

namespace RayRopeMoveSolverPrivate
{
namespace
{
EGlobalMoveStepStatus PrepareGlobalMoveStep(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TArray<FGlobalMoveNodeState, TInlineAllocator<32>>& States,
	TConstArrayView<int32> NodeToStateIndex,
	float& OutMaxAbsDelta)
{
	OutMaxAbsDelta = 0.f;

	FGlobalMoveSystem System;
	if (!BuildGlobalMoveSystem(
			SolveContext,
			Segment,
			States,
			NodeToStateIndex,
			System) ||
		!SolveTridiagonalSystem(System))
	{
		return EGlobalMoveStepStatus::Failed;
	}

	OutMaxAbsDelta = CopyDeltaToStates(System, States);
	if (OutMaxAbsDelta < SolveContext.MinMoveDistance)
	{
		return EGlobalMoveStepStatus::Converged;
	}

	return EGlobalMoveStepStatus::Ready;
}

void ApplyGlobalMove(
	const FRayRopeSegment& ReferenceSegment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	float Alpha,
	float MoveThreshold,
	FRayRopeSegment& Segment)
{
	for (const FGlobalMoveNodeState& State : States)
	{
		if (FMath::Abs(State.DeltaParameter * Alpha) < MoveThreshold)
		{
			continue;
		}

		FRayRopeNode MovedNode = FRayRopeNodeFactory::CreateNodeAtLocation(
			Segment.Nodes[State.NodeIndex],
			GetNodeLocationAtAlpha(
				ReferenceSegment,
				States,
				NodeToStateIndex,
				State.NodeIndex,
				Alpha));
		FRayRopeNodeSynchronizer::CacheAttachedActorOffset(MovedNode);
		Segment.Nodes[State.NodeIndex] = MoveTemp(MovedNode);
	}
}

void AddAffectedSpanRanges(
	TConstArrayView<FRayRopeSpanIndexRange> AffectedSpanRanges,
	FRayRopeSolveResult& Result)
{
	for (const FRayRopeSpanIndexRange& Range : AffectedSpanRanges)
	{
		Result.AddAffectedSpanRange(Range.FirstSpanIndex, Range.LastSpanIndex);
	}
}
}

FGlobalMoveResult TryMoveSegmentGlobal(
	const FMoveSolveContext& SolveContext,
	FRayRopeSegment& Segment)
{
	FGlobalMoveResult Result;

	if (SolveContext.MaxGlobalMoveIterations <= 0 ||
		Segment.Nodes.Num() < 5)
	{
		Result.Status = EGlobalMoveSolveStatus::NotApplicable;
		return Result;
	}

	for (int32 Iteration = 0; Iteration < SolveContext.MaxGlobalMoveIterations; ++Iteration)
	{
		TArray<FGlobalMoveNodeState, TInlineAllocator<32>> States;
		TArray<int32, TInlineAllocator<64>> NodeToStateIndex;
		const FGlobalMoveStateBuildResult BuildResult = BuildGlobalMoveNodeStates(
			SolveContext,
			Segment,
			States,
			NodeToStateIndex);
		Result.bSkippedAnyRedirect |= BuildResult.bSkippedAnyRedirect;
		if (BuildResult.RedirectCount < GMinGlobalMoveRedirects)
		{
			Result.Status = Result.PassResult.bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::NotApplicable;
			return Result;
		}

		if (BuildResult.StateCount < GMinGlobalMoveRedirects)
		{
			Result.Status = Result.PassResult.bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::NotApplicable;
			Result.PassResult.bNeedsLocalFallback = BuildResult.bSkippedAnyRedirect;
			Result.PassResult.bFullyHandled = !Result.PassResult.bNeedsLocalFallback;
			return Result;
		}

		float MaxAbsDelta = 0.f;
		const EGlobalMoveStepStatus StepStatus = PrepareGlobalMoveStep(
			SolveContext,
			Segment,
			States,
			NodeToStateIndex,
			MaxAbsDelta);
		if (StepStatus == EGlobalMoveStepStatus::Converged)
		{
			Result.Status = Result.PassResult.bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::Converged;
			Result.PassResult.bNeedsLocalFallback = Result.bSkippedAnyRedirect;
			Result.PassResult.bFullyHandled = !Result.PassResult.bNeedsLocalFallback;
			return Result;
		}

		if (StepStatus == EGlobalMoveStepStatus::Failed)
		{
			Result.PassResult.bHadFailure = true;
			Result.PassResult.bNeedsLocalFallback = true;
			Result.PassResult.bFullyHandled = false;
			Result.Status = Result.PassResult.bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::Failed;
			return Result;
		}

		const float CurrentLength = CalculateSegmentLengthAtAlpha(
			Segment,
			States,
			NodeToStateIndex,
			0.f);
		const float InitialAlpha = CalculateInitialLineSearchAlpha(
			SolveContext.MoveSettings,
			MaxAbsDelta);

		float AcceptedAlpha = 0.f;
		FRayRopeAffectedSpanRangeBuffer AcceptedSpanRanges;
		if (!TryFindAcceptedAlpha(
				SolveContext,
				Segment,
				States,
				NodeToStateIndex,
				InitialAlpha,
				MaxAbsDelta,
				CurrentLength,
				AcceptedAlpha,
				AcceptedSpanRanges))
		{
			Result.PassResult.bHadFailure = true;
			Result.PassResult.bNeedsLocalFallback = true;
			Result.PassResult.bFullyHandled = false;
			Result.Status = Result.PassResult.bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::Failed;
			return Result;
		}

		const FRayRopeSegment ReferenceSegment = Segment;
		const float AcceptedLength = CalculateSegmentLengthAtAlpha(
			ReferenceSegment,
			States,
			NodeToStateIndex,
			AcceptedAlpha);
		ApplyGlobalMove(
			ReferenceSegment,
			States,
			NodeToStateIndex,
			AcceptedAlpha,
			SolveContext.GeometryTolerance,
			Segment);

		Result.PassResult.SolveResult.MarkNodeLocationsChanged();
		AddAffectedSpanRanges(AcceptedSpanRanges, Result.PassResult.SolveResult);
		Result.PassResult.bAppliedAnyMove = true;

#if RAYROPE_WITH_DEBUG
		UE_LOG(
			LogRayRope,
			VeryVerbose,
			TEXT("[MoveSolver] Global iteration %d accepted alpha %.3f length %.3f -> %.3f rails %d"),
			Iteration,
			AcceptedAlpha,
			CurrentLength,
			AcceptedLength,
			States.Num());
#endif
	}

	Result.PassResult.bNeedsLocalFallback = Result.bSkippedAnyRedirect;
	Result.PassResult.bFullyHandled = !Result.PassResult.bNeedsLocalFallback;
	Result.Status = Result.PassResult.bAppliedAnyMove
		? EGlobalMoveSolveStatus::Applied
		: EGlobalMoveSolveStatus::Converged;
	return Result;
}
}
