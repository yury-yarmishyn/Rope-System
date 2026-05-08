#include "RayRopeRelaxSolver.h"

#include "Helpers/RayRopeNodeSynchronizer.h"
#include "Helpers/RayRopeTransitionValidator.h"

namespace
{
enum class ERelaxNodeResult
{
	Unchanged,
	Collapsed,
	Removed
};

struct FRelaxSolveContext
{
	const FRayRopeTraceContext& TraceContext;
	FRayRopeTransitionValidationSettings TransitionValidationSettings;
	float SolverTolerance = 0.f;

	FRelaxSolveContext(
		const FRayRopeTraceContext& InTraceContext,
		const FRayRopeRelaxSettings& InRelaxSettings)
		: TraceContext(InTraceContext)
	{
		SolverTolerance = FMath::Max(0.f, InRelaxSettings.RelaxSolverTolerance);
		TransitionValidationSettings.SolverTolerance = SolverTolerance;
		TransitionValidationSettings.MaxTransitionValidationIterations =
			FMath::Max(0, InRelaxSettings.MaxRelaxCollapseIterations);
	}
};

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

struct FRelaxNodeWindow
{
	const FRayRopeNode& PrevNode;
	FRayRopeNode& CurrentNode;
	const FRayRopeNode& NextNode;
	FVector CollapseTarget = FVector::ZeroVector;

	FRelaxNodeWindow(
		const FRayRopeNode& InPrevNode,
		FRayRopeNode& InCurrentNode,
		const FRayRopeNode& InNextNode)
		: PrevNode(InPrevNode)
		, CurrentNode(InCurrentNode)
		, NextNode(InNextNode)
		, CollapseTarget(CalculateCollapseTargetOnShortcutLine(
			InPrevNode,
			InCurrentNode,
			InNextNode))
	{
	}

	bool IsCurrentPointFree(const FRelaxSolveContext& SolveContext) const
	{
		return FRayRopeTrace::IsValidFreePoint(
			SolveContext.TraceContext,
			CurrentNode.WorldLocation);
	}

	bool IsCurrentAtCollapseTarget(const FRelaxSolveContext& SolveContext) const
	{
		return CurrentNode.WorldLocation.Equals(
			CollapseTarget,
			SolveContext.SolverTolerance);
	}
};

bool IsShortcutClear(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow)
{
	const FRayRopeSpan ShortcutSpan{&NodeWindow.PrevNode, &NodeWindow.NextNode};
	return !FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, ShortcutSpan);
}

bool CanMoveCurrentNodeToTarget(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow)
{
	const FRayRopeNodeTransition Transition{
		&NodeWindow.PrevNode,
		&NodeWindow.CurrentNode,
		&NodeWindow.NextNode,
		NodeWindow.CollapseTarget
	};
	return FRayRopeTransitionValidator::IsNodeTransitionClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		Transition);
}

bool TryCollapseNode(
	const FRelaxSolveContext& SolveContext,
	FRelaxNodeWindow& NodeWindow)
{
	if (NodeWindow.IsCurrentAtCollapseTarget(SolveContext))
	{
		return false;
	}

	if (!CanMoveCurrentNodeToTarget(SolveContext, NodeWindow))
	{
		return false;
	}

	NodeWindow.CurrentNode.WorldLocation = NodeWindow.CollapseTarget;
	FRayRopeNodeSynchronizer::CacheAttachedActorOffset(NodeWindow.CurrentNode);
	return true;
}

bool CanRemoveCollapsedNode(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow)
{
	return NodeWindow.IsCurrentAtCollapseTarget(SolveContext) &&
		IsShortcutClear(SolveContext, NodeWindow);
}

bool CanRemoveRecoverableNode(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow)
{
	if (!IsShortcutClear(SolveContext, NodeWindow))
	{
		return false;
	}

	if (NodeWindow.IsCurrentAtCollapseTarget(SolveContext))
	{
		return true;
	}

	return CanMoveCurrentNodeToTarget(SolveContext, NodeWindow);
}

bool TryRemoveNode(
	const FRelaxSolveContext& SolveContext,
	FRayRopeSegment& Segment,
	int32 NodeIndex,
	const FRelaxNodeWindow& NodeWindow,
	bool bAllowRecoverableUncollapsedNode)
{
	const bool bCanRemove = bAllowRecoverableUncollapsedNode
		? CanRemoveRecoverableNode(SolveContext, NodeWindow)
		: CanRemoveCollapsedNode(SolveContext, NodeWindow);
	if (!bCanRemove)
	{
		return false;
	}

	Segment.Nodes.RemoveAt(NodeIndex, 1, EAllowShrinking::No);
	return true;
}

ERelaxNodeResult RelaxNode(
	const FRelaxSolveContext& SolveContext,
	FRayRopeSegment& Segment,
	int32 NodeIndex)
{
	const FRayRopeNode& PrevNode = Segment.Nodes[NodeIndex - 1];
	FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
	const FRayRopeNode& NextNode = Segment.Nodes[NodeIndex + 1];
	FRelaxNodeWindow NodeWindow(PrevNode, CurrentNode, NextNode);

	if (!NodeWindow.IsCurrentPointFree(SolveContext))
	{
		return TryRemoveNode(
			SolveContext,
			Segment,
			NodeIndex,
			NodeWindow,
			true)
			? ERelaxNodeResult::Removed
			: ERelaxNodeResult::Unchanged;
	}

	if (NodeWindow.IsCurrentAtCollapseTarget(SolveContext))
	{
		return TryRemoveNode(
			SolveContext,
			Segment,
			NodeIndex,
			NodeWindow,
			false)
			? ERelaxNodeResult::Removed
			: ERelaxNodeResult::Unchanged;
	}

	if (!TryCollapseNode(SolveContext, NodeWindow))
	{
		return ERelaxNodeResult::Unchanged;
	}

	return TryRemoveNode(
		SolveContext,
		Segment,
		NodeIndex,
		NodeWindow,
		false)
		? ERelaxNodeResult::Removed
		: ERelaxNodeResult::Collapsed;
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
	const FRelaxSolveContext SolveContext(TraceContext, RelaxSettings);

	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num() - 1;)
	{
		if (Segment.Nodes[NodeIndex].NodeType != ERayRopeNodeType::Redirect)
		{
			++NodeIndex;
			continue;
		}

		if (RelaxNode(SolveContext, Segment, NodeIndex) == ERelaxNodeResult::Removed)
		{
			continue;
		}

		++NodeIndex;
	}
}
