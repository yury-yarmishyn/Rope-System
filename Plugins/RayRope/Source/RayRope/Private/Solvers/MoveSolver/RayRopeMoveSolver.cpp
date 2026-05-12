#include "RayRopeMoveSolver.h"

#include "Nodes/RayRopeNodeSynchronizer.h"
#include "RayRopeMoveSolverInternal.h"

using namespace RayRopeMoveSolverPrivate;

namespace
{
FRayRopeSolveResult MoveSegmentLocal(
	const FMoveSolveContext& SolveContext,
	FRayRopeSegment& Segment)
{
	FRayRopeSolveResult Result;
	if (Segment.Nodes.Num() < 3)
	{
		return Result;
	}

	FRayRopePendingNodeInsertionBuffer PendingInsertions;
	PendingInsertions.Reserve(Segment.Nodes.Num() * 2);

	for (int32 MoveIteration = 0; MoveIteration < SolveContext.MaxMoveIterations; ++MoveIteration)
	{
		// Alternate sweep direction so redirects are not biased toward the start of the segment.
		const bool bMoveForward = MoveIteration % 2 == 0;
		const int32 FirstNodeIndex = bMoveForward ? 1 : Segment.Nodes.Num() - 2;
		const int32 LastNodeIndex = bMoveForward ? Segment.Nodes.Num() - 1 : 0;
		const int32 NodeIndexStep = bMoveForward ? 1 : -1;

		bool bChangedThisIteration = false;
		PendingInsertions.Reset();
		PendingInsertions.Reserve(Segment.Nodes.Num() * 2);

		for (int32 NodeIndex = FirstNodeIndex; NodeIndex != LastNodeIndex; NodeIndex += NodeIndexStep)
		{
			FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
			if (CurrentNode.NodeType == ERayRopeNodeType::Anchor)
			{
				continue;
			}

			const FRayRopeNode& PrevNode = Segment.Nodes[NodeIndex - 1];
			const FRayRopeNode& NextNode = Segment.Nodes[NodeIndex + 1];
			const FMoveNodeWindow NodeWindow(PrevNode, CurrentNode, NextNode, NodeIndex);

			FMoveResult MoveResult;
			if (!TryFindEffectiveMove(
				SolveContext,
				NodeWindow,
				MoveResult))
			{
				continue;
			}

			FRayRopeNode MovedNode = FRayRopeNodeFactory::CreateNodeAtLocation(
				CurrentNode,
				MoveResult.EffectivePoint);
			FRayRopeNodeSynchronizer::CacheAttachedActorOffset(MovedNode);
			if (!TryQueueMoveInsertions(
				SolveContext,
				NodeWindow,
				MovedNode,
				MoveResult,
				PendingInsertions))
			{
				continue;
			}

			CurrentNode = MoveTemp(MovedNode);
			bChangedThisIteration = true;
			Result.MarkNodeLocationsChanged();
			Result.AddAffectedSpanRange(NodeIndex - 1, NodeIndex);
		}

		const bool bHasPendingInsertions = PendingInsertions.Num() > 0;
		int32 FirstInsertIndex = INDEX_NONE;
		int32 LastInsertIndex = INDEX_NONE;
		FRayRopeNodeInsertionQueue::TryGetInsertionBounds(
			PendingInsertions,
			FirstInsertIndex,
			LastInsertIndex);
		const int32 AppliedInsertionCount =
			FRayRopeNodeInsertionQueue::ApplyPendingInsertions(Segment, PendingInsertions);
		if (AppliedInsertionCount > 0)
		{
			Result.MarkTopologyChanged();
			Result.AddAffectedSpanRange(
				FirstInsertIndex - 2,
				LastInsertIndex + AppliedInsertionCount + 2);
		}

		if (!bChangedThisIteration && !bHasPendingInsertions)
		{
			break;
		}
	}

	return Result;
}
}

FRayRopeSolveResult FRayRopeMoveSolver::MoveSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNodeBuildSettings& NodeBuildSettings,
	FRayRopeSegment& Segment)
{
	FRayRopeSolveResult Result;
	if (Segment.Nodes.Num() < 3)
	{
		return Result;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeMoveTrace)));
	const FMoveSolveContext SolveContext(TraceContext, MoveSettings, NodeBuildSettings);

	if (MoveSettings.bUseGlobalMoveSolver)
	{
		const EGlobalMoveSolveStatus GlobalMoveStatus = TryMoveSegmentGlobal(
			SolveContext,
			Segment,
			Result);
		if (GlobalMoveStatus == EGlobalMoveSolveStatus::Applied ||
			GlobalMoveStatus == EGlobalMoveSolveStatus::Converged ||
			!MoveSettings.bFallbackToLocalMoveSolver)
		{
			return Result;
		}
	}

	return MoveSegmentLocal(SolveContext, Segment);
}
