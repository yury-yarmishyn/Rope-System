#pragma once

#include "Nodes/RayRopeNodeBuilder.h"

/**
 * Inserts anchor or redirect nodes where spans became blocked since the reference snapshot.
 */
struct FRayRopeWrapSolver
{
	/**
	 * Examines each current span against ReferenceNodes and queues any required insertions.
	 *
	 * ReferenceNodes must represent the same topology size as Segment at pass start. Insertions are
	 * deferred until after scanning so span indexes remain stable during the pass.
	 */
	static void WrapSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeNodeBuildSettings& WrapSettings,
		FRayRopeSegment& Segment,
		TConstArrayView<FRayRopeNode> ReferenceNodes);
};

