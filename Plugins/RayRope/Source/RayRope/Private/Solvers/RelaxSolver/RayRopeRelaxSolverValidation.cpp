#include "RayRopeRelaxSolverInternal.h"

namespace RayRopeRelaxSolverPrivate
{
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
	return FRayRopeTransitionValidator::IsNodeTransitionClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		FRayRopeNodeTransition::Make(
			NodeWindow.PrevNode,
			NodeWindow.CurrentNode,
			NodeWindow.NextNode,
			NodeWindow.CollapseTarget));
}
}

