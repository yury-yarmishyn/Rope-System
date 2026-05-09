#include "RayRopeMoveSolverInternal.h"

namespace RayRopeMoveSolverPrivate
{
bool TryBuildMoveWithNewNodes(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& TargetPoint,
	FMoveResult& OutResult)
{
	OutResult = FMoveResult();

	if (!IsReachableMoveTarget(
		SolveContext,
		NodeWindow,
		TargetPoint))
	{
		return false;
	}

	const FRayRopeNode CandidateNode = FRayRopeNodeFactory::CreateNodeAtLocation(
		NodeWindow.CurrentNode,
		TargetPoint);
	const FRayRopeSpan PrevCurrentSpan{&NodeWindow.PrevNode, &NodeWindow.CurrentNode};
	const FRayRopeSpan CurrentNextSpan{&NodeWindow.CurrentNode, &NodeWindow.NextNode};
	const FRayRopeSpan PrevCandidateSpan{&NodeWindow.PrevNode, &CandidateNode};
	const FRayRopeSpan CandidateNextSpan{&CandidateNode, &NodeWindow.NextNode};

	const bool bPrevCandidateBlocked =
		FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, PrevCandidateSpan);
	const bool bCandidateNextBlocked =
		FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, CandidateNextSpan);
	if (!bPrevCandidateBlocked && !bCandidateNextBlocked)
	{
		// Direct moves with clear final spans are handled by TryFindValidEffectivePoint.
		return false;
	}

	if (bPrevCandidateBlocked &&
		!FRayRopeNodeBuilder::BuildNodesForSpanTransition(
			SolveContext.TraceContext,
			SolveContext.MoveSettings.NodeBuildSettings,
			PrevCandidateSpan,
			PrevCurrentSpan,
			OutResult.BeforeCurrentNodes))
	{
		return false;
	}

	if (bCandidateNextBlocked &&
		!FRayRopeNodeBuilder::BuildNodesForSpanTransition(
			SolveContext.TraceContext,
			SolveContext.MoveSettings.NodeBuildSettings,
			CandidateNextSpan,
			CurrentNextSpan,
			OutResult.AfterCurrentNodes))
	{
		return false;
	}

	if (OutResult.BeforeCurrentNodes.Num() == 0 &&
		OutResult.AfterCurrentNodes.Num() == 0)
	{
		return false;
	}

	OutResult.EffectivePoint = TargetPoint;
	return true;
}

bool TryQueueMoveInsertions(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FRayRopeNode& MovedNode,
	FMoveResult& MoveResult,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	if (MoveResult.BeforeCurrentNodes.Num() == 0 &&
		MoveResult.AfterCurrentNodes.Num() == 0)
	{
		return true;
	}

	const int32 OriginalPendingInsertionCount = PendingInsertions.Num();
	if (MoveResult.BeforeCurrentNodes.Num() > 0 &&
		!FRayRopeNodeInsertionQueue::CanInsertNodes(
			SolveContext.MoveSettings.NodeBuildSettings,
			NodeWindow.PrevNode,
			MovedNode,
			NodeWindow.NodeIndex,
			MoveResult.BeforeCurrentNodes,
			PendingInsertions))
	{
		return false;
	}

	FRayRopeNodeInsertionQueue::AppendPendingInsertions(
		NodeWindow.NodeIndex,
		MoveResult.BeforeCurrentNodes,
		PendingInsertions);

	if (MoveResult.AfterCurrentNodes.Num() > 0 &&
		!FRayRopeNodeInsertionQueue::CanInsertNodes(
			SolveContext.MoveSettings.NodeBuildSettings,
			MovedNode,
			NodeWindow.NextNode,
			NodeWindow.NodeIndex + 1,
			MoveResult.AfterCurrentNodes,
			PendingInsertions))
	{
		const int32 InsertedPendingInsertionCount =
			PendingInsertions.Num() - OriginalPendingInsertionCount;
		if (InsertedPendingInsertionCount > 0)
		{
			// Roll back the before-current insertions so a partially queued move cannot affect the pass.
			PendingInsertions.RemoveAt(
				OriginalPendingInsertionCount,
				InsertedPendingInsertionCount,
				EAllowShrinking::No);
		}

		return false;
	}

	FRayRopeNodeInsertionQueue::AppendPendingInsertions(
		NodeWindow.NodeIndex + 1,
		MoveResult.AfterCurrentNodes,
		PendingInsertions);
	return true;
}
}
