#include "RayRopeRelaxSolverInternal.h"

#include "Debug/RayRopeDebugContext.h"
#include "Nodes/RayRopeNodeSynchronizer.h"

namespace RayRopeRelaxSolverPrivate
{
namespace
{
void RecordRelaxEvent(
	const FRelaxSolveContext& SolveContext,
	int32 NodeIndex,
	const FString& Message)
{
	if (SolveContext.TraceContext.DebugContext == nullptr)
	{
		return;
	}

	SolveContext.TraceContext.DebugContext->RecordSolverEvent(
		TEXT("Relax"),
		FString::Printf(
			TEXT("Node[%d] %s"),
			NodeIndex,
			*Message));
}

void DrawRelaxShortcut(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow)
{
	if (SolveContext.TraceContext.DebugContext == nullptr)
	{
		return;
	}

	SolveContext.TraceContext.DebugContext->DrawSolverLine(
		ERayRopeDebugDrawFlags::Relax,
		NodeWindow.PrevNode.WorldLocation,
		NodeWindow.NextNode.WorldLocation,
		SolveContext.TraceContext.DebugContext->GetSettings().DebugSolverGuideColor,
		TEXT("RelaxShortcut"));
	SolveContext.TraceContext.DebugContext->DrawSolverPoint(
		ERayRopeDebugDrawFlags::Relax,
		NodeWindow.CollapseTarget,
		SolveContext.TraceContext.DebugContext->GetSettings().DebugCandidateColor,
		TEXT("CollapseTarget"));
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

	// A penetrating redirect can be removed if it could have legally collapsed to the clear shortcut.
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
		RecordRelaxEvent(
			SolveContext,
			NodeIndex,
			bAllowRecoverableUncollapsedNode
				? TEXT("remove rejected: shortcut is blocked or node cannot recover to collapse target")
				: TEXT("remove rejected: node is not collapsed or shortcut is blocked"));
		return false;
	}

	RecordRelaxEvent(SolveContext, NodeIndex, TEXT("remove accepted"));
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
	DrawRelaxShortcut(SolveContext, NodeWindow);

	if (!NodeWindow.IsCurrentPointFree(SolveContext))
	{
		RecordRelaxEvent(
			SolveContext,
			NodeIndex,
			TEXT("current point is inside geometry; trying recoverable removal"));
		// Prefer removing invalid redirects when the shortcut proves recoverable; moving a point that
		// starts in geometry can otherwise create persistent jitter around collision boundaries.
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
		RecordRelaxEvent(
			SolveContext,
			NodeIndex,
			TEXT("already at collapse target; trying removal"));
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
		RecordRelaxEvent(
			SolveContext,
			NodeIndex,
			TEXT("collapse rejected: transition to target is blocked"));
		return ERelaxNodeResult::Unchanged;
	}

	RecordRelaxEvent(
		SolveContext,
		NodeIndex,
		FString::Printf(
			TEXT("collapse accepted Target=%s"),
			*NodeWindow.CollapseTarget.ToCompactString()));

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

