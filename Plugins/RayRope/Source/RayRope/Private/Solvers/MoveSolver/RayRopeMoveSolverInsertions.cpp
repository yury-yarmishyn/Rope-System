#include "RayRopeMoveSolverInternal.h"

#include "Nodes/RayRopeNodeSynchronizer.h"

namespace RayRopeMoveSolverPrivate
{
namespace
{
bool TryBuildInsertionNodesForValidation(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FMoveValidation& Validation,
	FMoveCommand& OutCommand)
{
	if (!Validation.NeedsInsertions() ||
		(!Validation.bPrevSpanBlocked && !Validation.bNextSpanBlocked))
	{
		return false;
	}

	const FRayRopeNode CandidateNode = FRayRopeNodeFactory::CreateNodeAtLocation(
		NodeWindow.CurrentNode,
		Validation.TargetPoint);
	const FRayRopeSpan PrevCurrentSpan{&NodeWindow.PrevNode, &NodeWindow.CurrentNode};
	const FRayRopeSpan CurrentNextSpan{&NodeWindow.CurrentNode, &NodeWindow.NextNode};
	const FRayRopeSpan PrevCandidateSpan{&NodeWindow.PrevNode, &CandidateNode};
	const FRayRopeSpan CandidateNextSpan{&CandidateNode, &NodeWindow.NextNode};
	const bool bMoveThenInsert = Validation.Type == EMoveDecisionType::MoveAndInsert;
	const FRayRopeSpan& BeforeBlockedSpan = bMoveThenInsert
		? PrevCandidateSpan
		: PrevCurrentSpan;
	const FRayRopeSpan& AfterBlockedSpan = bMoveThenInsert
		? CandidateNextSpan
		: CurrentNextSpan;

	FRayRopeBuiltNodeBuffer BeforeCurrentNodes;
	FRayRopeBuiltNodeBuffer AfterCurrentNodes;
	if (Validation.bPrevSpanBlocked &&
		!FRayRopeNodeBuilder::BuildNodesForSpanTransition(
			SolveContext.TraceContext,
			SolveContext.NodeBuildSettings,
			BeforeBlockedSpan,
			PrevCurrentSpan,
			BeforeCurrentNodes))
	{
		return false;
	}

	if (Validation.bNextSpanBlocked &&
		!FRayRopeNodeBuilder::BuildNodesForSpanTransition(
			SolveContext.TraceContext,
			SolveContext.NodeBuildSettings,
			AfterBlockedSpan,
			CurrentNextSpan,
			AfterCurrentNodes))
	{
		return false;
	}

	if (BeforeCurrentNodes.Num() == 0 && AfterCurrentNodes.Num() == 0)
	{
		return false;
	}

	OutCommand.Type = Validation.Type;
	OutCommand.TargetPoint = Validation.TargetPoint;
	OutCommand.BeforeCurrentNodes = MoveTemp(BeforeCurrentNodes);
	OutCommand.AfterCurrentNodes = MoveTemp(AfterCurrentNodes);
	return OutCommand.HasInsertions();
}
}

bool TryBuildMoveCommand(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FMoveValidation& Validation,
	FMoveCommand& OutCommand)
{
	OutCommand = FMoveCommand();
	if (!Validation.IsAccepted())
	{
		return false;
	}

	if (!Validation.NeedsInsertions())
	{
		OutCommand.Type = Validation.Type;
		OutCommand.TargetPoint = Validation.TargetPoint;
		return true;
	}

	return TryBuildInsertionNodesForValidation(
		SolveContext,
		NodeWindow,
		Validation,
		OutCommand);
}

bool TryResolveCandidateOnPath(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& TargetPoint,
	FMoveCommand& OutCommand)
{
	OutCommand = FMoveCommand();

	const FMoveValidation TargetValidation = EvaluateMoveCandidate(
		SolveContext,
		NodeWindow,
		TargetPoint);
	if (TryBuildMoveCommand(
		SolveContext,
		NodeWindow,
		TargetValidation,
		OutCommand))
	{
		if (!OutCommand.NeedsInsertions())
		{
			return true;
		}
	}

	if (TargetPoint.ContainsNaN() ||
		!NodeWindow.IsCurrentPointFree(SolveContext.TraceContext))
	{
		return OutCommand.NeedsInsertions();
	}

	FVector LastValidPoint = NodeWindow.CurrentNode.WorldLocation;
	FVector LastInvalidPoint = TargetPoint;
	FMoveCommand BestMoveCommand;
	bool bHasBestMoveCommand = false;
	FMoveCommand BestInsertionCommand = MoveTemp(OutCommand);
	bool bHasBestInsertionCommand = BestInsertionCommand.NeedsInsertions();
	const int32 MaxSearchIterations = SolveContext.MaxCandidateBacktrackIterations;

	for (int32 Iteration = 0; Iteration < MaxSearchIterations; ++Iteration)
	{
		if (FVector::DistSquared(LastValidPoint, LastInvalidPoint) <=
			SolveContext.GeometryToleranceSquared)
		{
			break;
		}

		const FVector CandidatePoint = (LastValidPoint + LastInvalidPoint) * 0.5f;
		const FMoveValidation CandidateValidation = EvaluateMoveCandidate(
			SolveContext,
			NodeWindow,
			CandidatePoint);
		FMoveCommand CandidateCommand;
		if (TryBuildMoveCommand(
			SolveContext,
			NodeWindow,
			CandidateValidation,
			CandidateCommand))
		{
			if (CandidateCommand.NeedsInsertions())
			{
				BestInsertionCommand = MoveTemp(CandidateCommand);
				bHasBestInsertionCommand = true;
				LastInvalidPoint = CandidatePoint;
				continue;
			}

			BestMoveCommand = MoveTemp(CandidateCommand);
			bHasBestMoveCommand = true;
			LastValidPoint = CandidatePoint;
			continue;
		}

		LastInvalidPoint = CandidatePoint;
	}

	if (bHasBestMoveCommand)
	{
		OutCommand = MoveTemp(BestMoveCommand);
		return true;
	}

	if (bHasBestInsertionCommand)
	{
		OutCommand = MoveTemp(BestInsertionCommand);
		return true;
	}

	return false;
}

bool TryQueueMoveInsertions(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveCommand& Command,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	if (!Command.HasInsertions())
	{
		return true;
	}

	FRayRopeNode EffectiveCurrentNode = Command.ShouldMoveNode()
		? FRayRopeNodeFactory::CreateNodeAtLocation(
			NodeWindow.CurrentNode,
			Command.TargetPoint)
		: NodeWindow.CurrentNode;
	if (Command.ShouldMoveNode())
	{
		FRayRopeNodeSynchronizer::CacheAttachedActorOffset(EffectiveCurrentNode);
	}

	const int32 OriginalPendingInsertionCount = PendingInsertions.Num();
	if (Command.BeforeCurrentNodes.Num() > 0 &&
		!FRayRopeNodeInsertionQueue::CanInsertNodes(
			SolveContext.NodeBuildSettings,
			NodeWindow.PrevNode,
			EffectiveCurrentNode,
			NodeWindow.NodeIndex,
			Command.BeforeCurrentNodes,
			PendingInsertions))
	{
		return false;
	}

	FRayRopeNodeInsertionQueue::AppendPendingInsertions(
		NodeWindow.NodeIndex,
		Command.BeforeCurrentNodes,
		PendingInsertions);

	if (Command.AfterCurrentNodes.Num() > 0 &&
		!FRayRopeNodeInsertionQueue::CanInsertNodes(
			SolveContext.NodeBuildSettings,
			EffectiveCurrentNode,
			NodeWindow.NextNode,
			NodeWindow.NodeIndex + 1,
			Command.AfterCurrentNodes,
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
		Command.AfterCurrentNodes,
		PendingInsertions);
	return true;
}
}
