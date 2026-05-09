#include "RayRopeRelaxSolverInternal.h"

#include "Nodes/RayRopeNodeSynchronizer.h"

namespace RayRopeRelaxSolverPrivate
{
namespace
{
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

