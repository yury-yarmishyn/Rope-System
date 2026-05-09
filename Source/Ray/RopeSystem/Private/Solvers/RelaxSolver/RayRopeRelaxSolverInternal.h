#pragma once

#include "RayRopeRelaxSolver.h"

#include "Validation/RayRopeTransitionValidator.h"

namespace RayRopeRelaxSolverPrivate
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
		const FRayRopeRelaxSettings& InRelaxSettings);
};

struct FRelaxNodeWindow
{
	const FRayRopeNode& PrevNode;
	FRayRopeNode& CurrentNode;
	const FRayRopeNode& NextNode;
	FVector CollapseTarget = FVector::ZeroVector;

	FRelaxNodeWindow(
		const FRayRopeNode& InPrevNode,
		FRayRopeNode& InCurrentNode,
		const FRayRopeNode& InNextNode);

	bool IsCurrentPointFree(const FRelaxSolveContext& SolveContext) const;
	bool IsCurrentAtCollapseTarget(const FRelaxSolveContext& SolveContext) const;
};

bool IsShortcutClear(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow);

bool CanMoveCurrentNodeToTarget(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow);

bool TryCollapseNode(
	const FRelaxSolveContext& SolveContext,
	FRelaxNodeWindow& NodeWindow);

bool TryRemoveNode(
	const FRelaxSolveContext& SolveContext,
	FRayRopeSegment& Segment,
	int32 NodeIndex,
	const FRelaxNodeWindow& NodeWindow,
	bool bAllowRecoverableUncollapsedNode);

ERelaxNodeResult RelaxNode(
	const FRelaxSolveContext& SolveContext,
	FRayRopeSegment& Segment,
	int32 NodeIndex);
}

