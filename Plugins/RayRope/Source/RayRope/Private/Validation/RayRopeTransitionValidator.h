#pragma once

#include "Trace/RayRopeTrace.h"

/**
 * Tolerances and iteration limits for checking whether a node can move without cutting through geometry.
 */
struct FRayRopeTransitionValidationSettings
{
	/** World-space tolerance used for coincident points and transition convergence. */
	float SolverTolerance = KINDA_SMALL_NUMBER;

	/** Sampling budget for validating the continuous fan of spans swept by a moving node. */
	int32 MaxTransitionValidationIterations = 8;
};

/**
 * Non-owning description of moving CurrentNode between fixed neighbor nodes.
 */
struct FRayRopeNodeTransition
{
	/** Previous fixed node in the segment. */
	const FRayRopeNode* PrevNode = nullptr;

	/** Node being moved by the transition. */
	const FRayRopeNode* CurrentNode = nullptr;

	/** Next fixed node in the segment. */
	const FRayRopeNode* NextNode = nullptr;

	/** Desired world-space location for CurrentNode. */
	FVector TargetLocation = FVector::ZeroVector;

	static FRayRopeNodeTransition Make(
		const FRayRopeNode& InPrevNode,
		const FRayRopeNode& InCurrentNode,
		const FRayRopeNode& InNextNode,
		const FVector& InTargetLocation)
	{
		return FRayRopeNodeTransition{
			&InPrevNode,
			&InCurrentNode,
			&InNextNode,
			InTargetLocation
		};
	}
};

/**
 * Validates redirect movement and collapse transitions.
 */
struct FRayRopeTransitionValidator
{
	/**
	 * Returns true only if the target is free, both final spans are clear, the node path is clear,
	 * and sampled intermediate span fans do not intersect blocking geometry.
	 */
	static bool IsNodeTransitionClear(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeTransitionValidationSettings& Settings,
		const FRayRopeNodeTransition& Transition);

	/**
	 * Checks only the direct path swept by the moving node.
	 *
	 * Use this for coarse reachability before building extra wrap nodes around blocked final spans.
	 */
	static bool IsTransitionNodePathClear(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeTransitionValidationSettings& Settings,
		const FRayRopeNodeTransition& Transition);
};
