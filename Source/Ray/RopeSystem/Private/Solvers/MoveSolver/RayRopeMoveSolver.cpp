#include "RayRopeMoveSolver.h"

#include "Nodes/RayRopeNodeSynchronizer.h"
#include "RayRopeMoveSolverInternal.h"

using namespace RayRopeMoveSolverPrivate;

void FRayRopeMoveSolver::MoveSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeMoveSettings& MoveSettings,
	FRayRopeSegment& Segment)
{
	if (Segment.Nodes.Num() < 3)
	{
		return;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeMoveTrace)));
	const FMoveSolveContext SolveContext(TraceContext, MoveSettings);

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
		}

		const bool bHasPendingInsertions = PendingInsertions.Num() > 0;
		FRayRopeNodeInsertionQueue::ApplyPendingInsertions(Segment, PendingInsertions);
		if (!bChangedThisIteration && !bHasPendingInsertions)
		{
			break;
		}
	}
}
