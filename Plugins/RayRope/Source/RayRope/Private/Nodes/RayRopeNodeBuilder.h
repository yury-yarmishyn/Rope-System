#pragma once

#include "RayRopeNodeInsertionQueue.h"
#include "Trace/RayRopeTrace.h"

/**
 * Settings used when building anchors or redirects for newly blocked spans.
 */
struct FRayRopeNodeBuildSettings
{
	/** Whether movable or simulating hit objects can receive redirect nodes. */
	bool bAllowWrapOnMovableObjects = false;

	/** Iteration budget for finding the clear-to-blocked transition span. */
	int32 MaxWrapBinarySearchIterations = 0;

	/** World-space convergence tolerance for wrap boundary search. */
	float WrapSolverTolerance = 0.f;

	/** Normal collinearity tolerance used to choose one redirect versus two redirects. */
	float GeometryCollinearityTolerance = 0.f;

	/** Distance used to push new redirect nodes away from hit surfaces. */
	float WrapSurfaceOffset = 0.f;
};

/**
 * Builds new nodes required when a span transitions from clear to blocked.
 */
struct FRayRopeNodeBuilder
{
	/**
	 * Builds nodes for the span at NodeIndex by comparing current topology with a reference snapshot.
	 *
	 * Returns candidate nodes only; the caller owns duplicate checks and insertion.
	 */
	static bool BuildNodes(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNodeBuildSettings& Settings,
		int32 NodeIndex,
		TConstArrayView<FRayRopeNode> CurrentNodes,
		TConstArrayView<FRayRopeNode> ReferenceNodes,
		FRayRopeBuiltNodeBuffer& OutNodes);

	/**
	 * Builds anchor or redirect nodes for one blocked current span.
	 *
	 * ReferenceSpan is the previous clear span used to recover a stable wrap boundary.
	 * Does not mutate either span.
	 */
	static bool BuildNodesForSpanTransition(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNodeBuildSettings& Settings,
		const FRayRopeSpan& CurrentSpan,
		const FRayRopeSpan& ReferenceSpan,
		FRayRopeBuiltNodeBuffer& OutNodes);
};
