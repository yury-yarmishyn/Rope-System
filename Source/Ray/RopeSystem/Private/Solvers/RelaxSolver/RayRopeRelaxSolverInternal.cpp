#include "RayRopeRelaxSolverInternal.h"

namespace RayRopeRelaxSolverPrivate
{
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
}

FRelaxSolveContext::FRelaxSolveContext(
	const FRayRopeTraceContext& InTraceContext,
	const FRayRopeRelaxSettings& InRelaxSettings)
	: TraceContext(InTraceContext)
{
	SolverTolerance = FMath::Max(0.f, InRelaxSettings.RelaxSolverTolerance);
	TransitionValidationSettings.SolverTolerance = SolverTolerance;
	TransitionValidationSettings.MaxTransitionValidationIterations =
		FMath::Max(0, InRelaxSettings.MaxRelaxCollapseIterations);
}

FRelaxNodeWindow::FRelaxNodeWindow(
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

bool FRelaxNodeWindow::IsCurrentPointFree(const FRelaxSolveContext& SolveContext) const
{
	return FRayRopeTrace::IsValidFreePoint(
		SolveContext.TraceContext,
		CurrentNode.WorldLocation);
}

bool FRelaxNodeWindow::IsCurrentAtCollapseTarget(const FRelaxSolveContext& SolveContext) const
{
	return CurrentNode.WorldLocation.Equals(
		CollapseTarget,
		SolveContext.SolverTolerance);
}
}

