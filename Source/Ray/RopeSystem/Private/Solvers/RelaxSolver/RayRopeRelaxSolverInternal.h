#pragma once

#include "RayRopeRelaxSolver.h"

#include "Validation/RayRopeTransitionValidator.h"

namespace RayRopeRelaxSolverPrivate
{
/** Outcome of one redirect relaxation attempt. */
enum class ERelaxNodeResult
{
	/** No topology or location change was accepted. */
	Unchanged,

	/** Redirect moved to its shortcut projection but remains in the segment. */
	Collapsed,

	/** Redirect was removed from the segment. */
	Removed
};

/**
 * Normalized per-pass relax settings.
 */
struct FRelaxSolveContext
{
	const FRayRopeTraceContext& TraceContext;
	FRayRopeTransitionValidationSettings TransitionValidationSettings;
	float SolverTolerance = 0.f;

	FRelaxSolveContext(
		const FRayRopeTraceContext& InTraceContext,
		const FRayRopeRelaxSettings& InRelaxSettings);
};

/**
 * Three-node view used while relaxing one redirect.
 */
struct FRelaxNodeWindow
{
	/** Previous fixed node in the segment. */
	const FRayRopeNode& PrevNode;

	/** Redirect node being collapsed or removed. */
	FRayRopeNode& CurrentNode;

	/** Next fixed node in the segment. */
	const FRayRopeNode& NextNode;

	/** Projection of CurrentNode onto the PrevNode-to-NextNode shortcut. */
	FVector CollapseTarget = FVector::ZeroVector;

	FRelaxNodeWindow(
		const FRayRopeNode& InPrevNode,
		FRayRopeNode& InCurrentNode,
		const FRayRopeNode& InNextNode);

	bool IsCurrentPointFree(const FRelaxSolveContext& SolveContext) const;

	bool IsCurrentAtCollapseTarget(const FRelaxSolveContext& SolveContext) const;
};

/**
 * Returns true when removing CurrentNode would leave a clear PrevNode-to-NextNode shortcut.
 */
bool IsShortcutClear(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow);

/**
 * Returns true when CurrentNode can move to CollapseTarget without sweeping through geometry.
 */
bool CanMoveCurrentNodeToTarget(
	const FRelaxSolveContext& SolveContext,
	const FRelaxNodeWindow& NodeWindow);

/**
 * Moves CurrentNode to CollapseTarget when the transition is valid.
 */
bool TryCollapseNode(
	const FRelaxSolveContext& SolveContext,
	FRelaxNodeWindow& NodeWindow);

/**
 * Removes CurrentNode when shortcut and recovery rules allow it.
 */
bool TryRemoveNode(
	const FRelaxSolveContext& SolveContext,
	FRayRopeSegment& Segment,
	int32 NodeIndex,
	const FRelaxNodeWindow& NodeWindow,
	bool bAllowRecoverableUncollapsedNode);

/**
 * Attempts the full relax decision for one redirect.
 */
ERelaxNodeResult RelaxNode(
	const FRelaxSolveContext& SolveContext,
	FRayRopeSegment& Segment,
	int32 NodeIndex);
}

