#pragma once

#include "Solvers/SolvePipeline/RayRopeSolveTypes.h"
#include "Trace/RayRopeTrace.h"

/**
 * Settings controlling redirect collapse and removal.
 */
struct FRayRopeRelaxSettings
{
	/** World-space tolerance for collapse target comparisons. */
	float RelaxSolverTolerance = 0.f;

	/** Iteration budget for validating swept collapse transitions. */
	int32 MaxRelaxCollapseIterations = 8;
};

/**
 * Collapses or removes redirect nodes that no longer need to preserve a wrap.
 */
struct FRayRopeRelaxSolver
{
	/**
	 * Runs the relax pass in place.
	 *
	 * Only redirect nodes are eligible. Anchors are preserved and can split segments later.
	 */
	static FRayRopeSolveResult RelaxSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeRelaxSettings& RelaxSettings,
		FRayRopeSegment& Segment);
};

