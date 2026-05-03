#include "RayRopeRelaxSolver.h"

#include "Helpers/RayRopeNodeSynchronizer.h"
#include "Helpers/RayRopeTransitionValidator.h"

namespace
{
FVector CalculateCollapseTargetOnShortcutLine(
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode)
{
	const FVector Dir = NextNode.WorldLocation - PrevNode.WorldLocation;
	const float LenSq = Dir.SizeSquared();

	if (LenSq <= KINDA_SMALL_NUMBER)
	{
		return PrevNode.WorldLocation;
	}

	const float T = FVector::DotProduct(CurrentNode.WorldLocation - PrevNode.WorldLocation, Dir) / LenSq;

	return PrevNode.WorldLocation + Dir * FMath::Clamp(T, 0.f, 1.f);
}

FRayRopeTransitionValidationSettings BuildRelaxTransitionValidationSettings(
	const FRayRopeRelaxSettings& RelaxSettings)
{
	FRayRopeTransitionValidationSettings ValidationSettings;
	ValidationSettings.SolverTolerance = RelaxSettings.RelaxSolverTolerance;
	ValidationSettings.MaxTransitionValidationIterations =
		FMath::Max(0, RelaxSettings.MaxRelaxCollapseIterations);
	return ValidationSettings;
}

bool IsShortcutSpanClear(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& NextNode)
{
	const FRayRopeSpan ShortcutSpan{&PrevNode, &NextNode};
	return !FRayRopeTrace::HasBlockingSpanHit(TraceContext, ShortcutSpan);
}

bool ValidateSegmentSpans(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeSegment& Segment)
{
	for (const FRayRopeNode& Node : Segment.Nodes)
	{
		if (!FRayRopeTrace::IsValidFreeNode(TraceContext, Node))
		{
			return false;
		}
	}

	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
	{
		const FRayRopeSpan Span{&Segment.Nodes[NodeIndex - 1], &Segment.Nodes[NodeIndex]};
		if (FRayRopeTrace::HasBlockingSpanHit(TraceContext, Span))
		{
			return false;
		}
	}

	return true;
}

bool CanCollapseRelaxNodeToTarget(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeRelaxSettings& RelaxSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CollapseTarget)
{
	const FRayRopeNodeTransition Transition{
		&PrevNode,
		&CurrentNode,
		&NextNode,
		CollapseTarget
	};
	return FRayRopeTransitionValidator::IsNodeTransitionClear(
		TraceContext,
		BuildRelaxTransitionValidationSettings(RelaxSettings),
		Transition);
}

bool TryCollapseRelaxNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeRelaxSettings& RelaxSettings,
	const FRayRopeNode& PrevNode,
	FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CollapseTarget)
{
	if (CurrentNode.WorldLocation.Equals(
		CollapseTarget,
		RelaxSettings.RelaxSolverTolerance))
	{
		return false;
	}

	if (!CanCollapseRelaxNodeToTarget(
		TraceContext,
		RelaxSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		CollapseTarget))
	{
		return false;
	}

	CurrentNode.WorldLocation = CollapseTarget;
	FRayRopeNodeSynchronizer::CacheAttachedActorOffset(CurrentNode);
	return true;
}

bool CanRemoveCollapsedRelaxNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeRelaxSettings& RelaxSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CollapseTarget)
{
	return CurrentNode.WorldLocation.Equals(
			CollapseTarget,
			RelaxSettings.RelaxSolverTolerance) &&
		IsShortcutSpanClear(TraceContext, PrevNode, NextNode);
}

bool CanRemoveRecoverableRelaxNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeRelaxSettings& RelaxSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CollapseTarget)
{
	if (!IsShortcutSpanClear(TraceContext, PrevNode, NextNode))
	{
		return false;
	}

	if (CurrentNode.WorldLocation.Equals(
		CollapseTarget,
		RelaxSettings.RelaxSolverTolerance))
	{
		return true;
	}

	return CanCollapseRelaxNodeToTarget(
		TraceContext,
		RelaxSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		CollapseTarget);
}

bool TryRemoveRelaxNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeRelaxSettings& RelaxSettings,
	FRayRopeSegment& Segment,
	int32 NodeIndex,
	bool bAllowRecoverableUncollapsedNode)
{
	if (!Segment.Nodes.IsValidIndex(NodeIndex - 1) ||
		!Segment.Nodes.IsValidIndex(NodeIndex) ||
		!Segment.Nodes.IsValidIndex(NodeIndex + 1))
	{
		return false;
	}

	const FRayRopeNode& PrevNode = Segment.Nodes[NodeIndex - 1];
	const FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
	const FRayRopeNode& NextNode = Segment.Nodes[NodeIndex + 1];
	const FVector CollapseTarget = CalculateCollapseTargetOnShortcutLine(
		PrevNode,
		CurrentNode,
		NextNode);

	const bool bCanRemove = bAllowRecoverableUncollapsedNode
		? CanRemoveRecoverableRelaxNode(
			TraceContext,
			RelaxSettings,
			PrevNode,
			CurrentNode,
			NextNode,
			CollapseTarget)
		: CanRemoveCollapsedRelaxNode(
			TraceContext,
			RelaxSettings,
			PrevNode,
			CurrentNode,
			NextNode,
			CollapseTarget);
	if (!bCanRemove)
	{
		return false;
	}

	FRayRopeNode RemovedNode = Segment.Nodes[NodeIndex];
	Segment.Nodes.RemoveAt(NodeIndex, 1, EAllowShrinking::No);
	if (ValidateSegmentSpans(TraceContext, Segment))
	{
		return true;
	}

	Segment.Nodes.Insert(MoveTemp(RemovedNode), NodeIndex);
	return false;
}
}

void FRayRopeRelaxSolver::RelaxSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeRelaxSettings& RelaxSettings,
	FRayRopeSegment& Segment)
{
	if (Segment.Nodes.Num() < 3)
	{
		return;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeRelaxTrace)));

	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num() - 1;)
	{
		FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
		if (CurrentNode.NodeType != ERayRopeNodeType::Redirect)
		{
			++NodeIndex;
			continue;
		}

		const FRayRopeNode& PrevNode = Segment.Nodes[NodeIndex - 1];
		const FRayRopeNode& NextNode = Segment.Nodes[NodeIndex + 1];
		const bool bCurrentNodeFree = FRayRopeTrace::IsValidFreePoint(
			TraceContext,
			CurrentNode.WorldLocation);
		if (!bCurrentNodeFree)
		{
			if (TryRemoveRelaxNode(
				TraceContext,
				RelaxSettings,
				Segment,
				NodeIndex,
				true))
			{
				continue;
			}

			++NodeIndex;
			continue;
		}

		const FVector CollapseTarget = CalculateCollapseTargetOnShortcutLine(
			PrevNode,
			CurrentNode,
			NextNode);
		if (CurrentNode.WorldLocation.Equals(
			CollapseTarget,
			RelaxSettings.RelaxSolverTolerance))
		{
			if (TryRemoveRelaxNode(
				TraceContext,
				RelaxSettings,
				Segment,
				NodeIndex,
				false))
			{
				continue;
			}

			++NodeIndex;
			continue;
		}

		if (TryCollapseRelaxNode(
			TraceContext,
			RelaxSettings,
			PrevNode,
			CurrentNode,
			NextNode,
			CollapseTarget))
		{
			if (TryRemoveRelaxNode(
				TraceContext,
				RelaxSettings,
				Segment,
				NodeIndex,
				false))
			{
				continue;
			}

			++NodeIndex;
			continue;
		}

		++NodeIndex;
	}
}
