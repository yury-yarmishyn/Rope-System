#include "RayRopeMoveSolverGlobalInternal.h"

#include "Debug/RayRopeDebugConfig.h"
#include "Nodes/RayRopeNodeSynchronizer.h"
#include "Topology/RayRopeSegmentTopology.h"

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
}

EGlobalMoveSolveStatus TryMoveSegmentGlobal(
	const FMoveSolveContext& SolveContext,
	FRayRopeSegment& Segment,
	FRayRopeSolveResult& OutResult)
{
	OutResult = FRayRopeSolveResult();

	if (SolveContext.MaxGlobalMoveIterations <= 0 ||
		Segment.Nodes.Num() < 5 ||
		FRayRopeSegmentTopology::CountRedirectNodes(Segment) < GMinGlobalMoveRedirects)
	{
		return EGlobalMoveSolveStatus::NotApplicable;
	}

	bool bAppliedAnyMove = false;
	for (int32 Iteration = 0; Iteration < SolveContext.MaxGlobalMoveIterations; ++Iteration)
	{
		TArray<FGlobalMoveNodeState, TInlineAllocator<32>> States;
		TArray<int32, TInlineAllocator<64>> NodeToStateIndex;
		BuildGlobalMoveNodeStates(
			SolveContext,
			Segment,
			States,
			NodeToStateIndex);
		if (States.Num() < GMinGlobalMoveRedirects)
		{
			return bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::NotApplicable;
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
			return bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::Converged;
		}

		if (StepStatus == EGlobalMoveStepStatus::Failed)
		{
			return bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::NoValidStep;
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
		int32 FirstSpanIndex = INDEX_NONE;
		int32 LastSpanIndex = INDEX_NONE;
		if (!TryFindAcceptedAlpha(
				SolveContext,
				Segment,
				States,
				NodeToStateIndex,
				InitialAlpha,
				CurrentLength,
				AcceptedAlpha,
				FirstSpanIndex,
				LastSpanIndex))
		{
			return bAppliedAnyMove
				? EGlobalMoveSolveStatus::Applied
				: EGlobalMoveSolveStatus::NoValidStep;
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

		OutResult.MarkNodeLocationsChanged();
		OutResult.AddAffectedSpanRange(FirstSpanIndex, LastSpanIndex);
		bAppliedAnyMove = true;

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

	return bAppliedAnyMove
		? EGlobalMoveSolveStatus::Applied
		: EGlobalMoveSolveStatus::Converged;
}
}
