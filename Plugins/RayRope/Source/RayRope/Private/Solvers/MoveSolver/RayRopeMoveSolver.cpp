#include "RayRopeMoveSolver.h"

#include "Debug/RayRopeDebugConfig.h"
#include "Nodes/RayRopeNodeSynchronizer.h"
#include "RayRopeMoveSolverInternal.h"

#if RAYROPE_WITH_DEBUG
#include "Debug/RayRopeDebugContext.h"
#endif

using namespace RayRopeMoveSolverPrivate;

namespace
{
#if RAYROPE_WITH_DEBUG
const TCHAR* GetGlobalMoveStatusName(EGlobalMoveSolveStatus Status)
{
	switch (Status)
	{
	case EGlobalMoveSolveStatus::NotApplicable:
		return TEXT("NotApplicable");

	case EGlobalMoveSolveStatus::Converged:
		return TEXT("Converged");

	case EGlobalMoveSolveStatus::Applied:
		return TEXT("Applied");

	case EGlobalMoveSolveStatus::Failed:
		return TEXT("Failed");

	default:
		return TEXT("Unknown");
	}
}
#endif

struct FInsertionGroup
{
	int32 InsertIndex = INDEX_NONE;
	int32 Count = 0;
};

void BuildInsertionGroups(
	const FRayRopePendingNodeInsertionBuffer& PendingInsertions,
	TArray<FInsertionGroup, TInlineAllocator<8>>& OutGroups)
{
	OutGroups.Reset();
	for (const TPair<int32, FRayRopeNode>& PendingInsertion : PendingInsertions)
	{
		if (OutGroups.Num() > 0 &&
			OutGroups.Last().InsertIndex == PendingInsertion.Key)
		{
			++OutGroups.Last().Count;
			continue;
		}

		OutGroups.Add(FInsertionGroup{PendingInsertion.Key, 1});
	}
}

void AddInsertionAffectedSpanRanges(
	TConstArrayView<FInsertionGroup> InsertionGroups,
	FRayRopeSolveResult& Result)
{
	int32 AppliedOffset = 0;
	for (const FInsertionGroup& Group : InsertionGroups)
	{
		const int32 AdjustedInsertIndex = Group.InsertIndex + AppliedOffset;
		Result.AddAffectedSpanRange(
			AdjustedInsertIndex - 2,
			AdjustedInsertIndex + Group.Count + 2);
		AppliedOffset += Group.Count;
	}
}

int32 ApplyPendingInsertionsAndRecordRanges(
	FRayRopeSegment& Segment,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions,
	FRayRopeSolveResult& Result)
{
	TArray<FInsertionGroup, TInlineAllocator<8>> InsertionGroups;
	FRayRopeNodeInsertionQueue::SortPendingInsertions(PendingInsertions);
	BuildInsertionGroups(PendingInsertions, InsertionGroups);

	const int32 AppliedInsertionCount =
		FRayRopeNodeInsertionQueue::ApplyPendingInsertions(
			Segment,
			PendingInsertions,
			true);
	if (AppliedInsertionCount > 0)
	{
		Result.MarkTopologyChanged();
		AddInsertionAffectedSpanRanges(InsertionGroups, Result);
	}

	return AppliedInsertionCount;
}

FMovePassResult RepairCurrentTopology(
	const FMoveSolveContext& SolveContext,
	FRayRopeSegment& Segment)
{
	FMovePassResult Result;
	if (Segment.Nodes.Num() < 3 ||
		SolveContext.MaxTopologyRepairIterations <= 0)
	{
		return Result;
	}

	FRayRopePendingNodeInsertionBuffer PendingInsertions;
	PendingInsertions.Reserve(Segment.Nodes.Num() * 2);

	for (int32 RepairIteration = 0;
		RepairIteration < SolveContext.MaxTopologyRepairIterations;
		++RepairIteration)
	{
		PendingInsertions.Reset();
		PendingInsertions.Reserve(Segment.Nodes.Num() * 2);

		for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num() - 1; ++NodeIndex)
		{
			FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
			if (CurrentNode.NodeType == ERayRopeNodeType::Anchor)
			{
				continue;
			}

			const FMoveNodeWindow NodeWindow(
				Segment.Nodes[NodeIndex - 1],
				CurrentNode,
				Segment.Nodes[NodeIndex + 1],
				NodeIndex);
			const FMoveValidation Validation = EvaluateCurrentSpans(
				SolveContext,
				NodeWindow);
			if (!Validation.NeedsInsertions())
			{
				continue;
			}

			FMoveCommand Command;
			if (!TryBuildMoveCommand(
					SolveContext,
					NodeWindow,
					Validation,
					Command) ||
				!TryQueueMoveInsertions(
					SolveContext,
					NodeWindow,
					Command,
					PendingInsertions))
			{
				Result.bHadFailure = true;
				Result.bNeedsLocalFallback = true;
				Result.bFullyHandled = false;
#if RAYROPE_WITH_DEBUG
				if (SolveContext.TraceContext.DebugContext != nullptr)
				{
					SolveContext.TraceContext.DebugContext->RecordSolverEvent(
						TEXT("MoveRepair"),
						FString::Printf(
							TEXT("Node[%d] rejected: topology repair insertion failed"),
							NodeIndex));
				}
#endif
				continue;
			}
		}

		if (PendingInsertions.Num() == 0)
		{
			break;
		}

		const int32 AppliedInsertionCount = ApplyPendingInsertionsAndRecordRanges(
			Segment,
			PendingInsertions,
			Result.SolveResult);
		Result.bAppliedAnyMove |= AppliedInsertionCount > 0;
		if (RepairIteration + 1 >= SolveContext.MaxTopologyRepairIterations)
		{
			Result.bFullyHandled = false;
			Result.bNeedsLocalFallback = true;
		}
	}

	return Result;
}

FMovePassResult MoveSegmentLocal(
	const FMoveSolveContext& SolveContext,
	FRayRopeSegment& Segment)
{
	FMovePassResult Result;
	if (Segment.Nodes.Num() < 3 ||
		SolveContext.MaxMoveIterations <= 0)
	{
		return Result;
	}

	FRayRopePendingNodeInsertionBuffer PendingInsertions;
	PendingInsertions.Reserve(Segment.Nodes.Num() * 2);
	bool bConverged = false;

	for (int32 MoveIteration = 0; MoveIteration < SolveContext.MaxMoveIterations; ++MoveIteration)
	{
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveLocal"),
				FString::Printf(
					TEXT("Iteration %d started Nodes=%d"),
					MoveIteration,
					Segment.Nodes.Num()));
		}
#endif

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

			FMoveCommand MoveCommand;
			if (!TryFindEffectiveMove(
					SolveContext,
					NodeWindow,
					MoveCommand))
			{
#if RAYROPE_WITH_DEBUG
				if (SolveContext.TraceContext.DebugContext != nullptr)
				{
					SolveContext.TraceContext.DebugContext->RecordSolverEvent(
						TEXT("MoveLocal"),
						FString::Printf(
							TEXT("Node[%d] rejected: no effective move"),
							NodeIndex));
				}
#endif
				continue;
			}

			if (!TryQueueMoveInsertions(
				SolveContext,
				NodeWindow,
				MoveCommand,
				PendingInsertions))
			{
#if RAYROPE_WITH_DEBUG
				if (SolveContext.TraceContext.DebugContext != nullptr)
				{
					SolveContext.TraceContext.DebugContext->DrawSolverPoint(
						ERayRopeDebugDrawFlags::MoveCandidates,
						MoveCommand.TargetPoint,
						SolveContext.TraceContext.DebugContext->GetSettings().DebugBlockedTraceColor,
						TEXT("MoveInsertReject"));
					SolveContext.TraceContext.DebugContext->RecordSolverEvent(
						TEXT("MoveLocal"),
						FString::Printf(
							TEXT("Node[%d] rejected: move insertions invalid Target=%s"),
							NodeIndex,
							*MoveCommand.TargetPoint.ToCompactString()));
				}
#endif
				continue;
			}

#if RAYROPE_WITH_DEBUG
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				if (MoveCommand.ShouldMoveNode())
				{
					SolveContext.TraceContext.DebugContext->DrawSolverLine(
						ERayRopeDebugDrawFlags::MoveCandidates,
						CurrentNode.WorldLocation,
						MoveCommand.TargetPoint,
						SolveContext.TraceContext.DebugContext->GetSettings().DebugAcceptedColor,
						TEXT("Move"));
					SolveContext.TraceContext.DebugContext->DrawSolverPoint(
						ERayRopeDebugDrawFlags::MoveCandidates,
						MoveCommand.TargetPoint,
						SolveContext.TraceContext.DebugContext->GetSettings().DebugAcceptedColor,
						TEXT("MoveTarget"));
				}
				else if (MoveCommand.HasInsertions())
				{
					SolveContext.TraceContext.DebugContext->DrawSolverPoint(
						ERayRopeDebugDrawFlags::MoveCandidates,
						MoveCommand.TargetPoint,
						SolveContext.TraceContext.DebugContext->GetSettings().DebugAcceptedColor,
						TEXT("TopologyRepair"));
				}

				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveLocal"),
					FString::Printf(
						TEXT("Node[%d] accepted Move=%s Target=%s InsertionsBefore=%d InsertionsAfter=%d"),
						NodeIndex,
						MoveCommand.ShouldMoveNode() ? TEXT("true") : TEXT("false"),
						*MoveCommand.TargetPoint.ToCompactString(),
						MoveCommand.BeforeCurrentNodes.Num(),
						MoveCommand.AfterCurrentNodes.Num()));
			}
#endif

			if (MoveCommand.ShouldMoveNode())
			{
				FRayRopeNode MovedNode = FRayRopeNodeFactory::CreateNodeAtLocation(
					CurrentNode,
					MoveCommand.TargetPoint);
				FRayRopeNodeSynchronizer::CacheAttachedActorOffset(MovedNode);

				CurrentNode = MoveTemp(MovedNode);
				bChangedThisIteration = true;
				Result.bAppliedAnyMove = true;
				Result.SolveResult.MarkNodeLocationsChanged();
				Result.SolveResult.AddAffectedSpanRange(NodeIndex - 1, NodeIndex);
			}
		}

		const bool bHasPendingInsertions = PendingInsertions.Num() > 0;
		const int32 AppliedInsertionCount = ApplyPendingInsertionsAndRecordRanges(
			Segment,
			PendingInsertions,
			Result.SolveResult);
		Result.bAppliedAnyMove |= AppliedInsertionCount > 0;

		if (!bChangedThisIteration && !bHasPendingInsertions)
		{
			bConverged = true;
#if RAYROPE_WITH_DEBUG
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveLocal"),
					FString::Printf(
						TEXT("Iteration %d converged"),
						MoveIteration));
			}
#endif
			break;
		}
	}

	Result.bFullyHandled = bConverged;
	Result.bNeedsLocalFallback = !bConverged;
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
		const FGlobalMoveResult GlobalMoveResult = TryMoveSegmentGlobal(
			SolveContext,
			Segment);
		Result.Merge(GlobalMoveResult.PassResult.SolveResult);
#if RAYROPE_WITH_DEBUG
		if (TraceContext.DebugContext != nullptr)
		{
			TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveGlobal"),
				FString::Printf(
					TEXT("Status=%s Fallback=%s Applied=%s HadFailure=%s SkippedRedirect=%s ResultTopology=%s ResultLocations=%s"),
					GetGlobalMoveStatusName(GlobalMoveResult.Status),
					MoveSettings.bFallbackToLocalMoveSolver ? TEXT("enabled") : TEXT("disabled"),
					GlobalMoveResult.PassResult.bAppliedAnyMove ? TEXT("true") : TEXT("false"),
					GlobalMoveResult.PassResult.bHadFailure ? TEXT("true") : TEXT("false"),
					GlobalMoveResult.bSkippedAnyRedirect ? TEXT("true") : TEXT("false"),
					Result.bTopologyChanged ? TEXT("true") : TEXT("false"),
					Result.bNodeLocationsChanged ? TEXT("true") : TEXT("false")));
		}
#endif

		if (GlobalMoveResult.Status == EGlobalMoveSolveStatus::Applied ||
			GlobalMoveResult.Status == EGlobalMoveSolveStatus::Converged)
		{
#if RAYROPE_WITH_DEBUG
			if (TraceContext.DebugContext != nullptr)
			{
				TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveRepair"),
					TEXT("Running topology repair after global move"));
			}
#endif

			FMovePassResult RepairResult = RepairCurrentTopology(SolveContext, Segment);
			Result.Merge(RepairResult.SolveResult);

			const bool bNeedsFullLocal =
				GlobalMoveResult.PassResult.bNeedsLocalFallback ||
				RepairResult.bNeedsLocalFallback ||
				!RepairResult.bFullyHandled;
			if (!bNeedsFullLocal || !MoveSettings.bFallbackToLocalMoveSolver)
			{
				return Result;
			}
		}

		if (!MoveSettings.bFallbackToLocalMoveSolver)
		{
			return Result;
		}
	}

#if RAYROPE_WITH_DEBUG
	if (TraceContext.DebugContext != nullptr)
	{
		TraceContext.DebugContext->RecordSolverEvent(
			TEXT("MoveLocal"),
			TEXT("Running local fallback"));
	}
#endif

	FMovePassResult LocalResult = MoveSegmentLocal(SolveContext, Segment);
	Result.Merge(LocalResult.SolveResult);
	return Result;
}
