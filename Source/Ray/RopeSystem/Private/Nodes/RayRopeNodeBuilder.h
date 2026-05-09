#pragma once

#include "RayRopeNodeInsertionQueue.h"
#include "Trace/RayRopeTrace.h"

struct FRayRopeNodeBuildSettings
{
	bool bAllowWrapOnMovableObjects = false;
	int32 MaxWrapBinarySearchIterations = 0;
	float WrapSolverTolerance = 0.f;
	float GeometryCollinearityTolerance = 0.f;
	float WrapSurfaceOffset = 0.f;
};

struct FRayRopeNodeBuilder
{
	static bool BuildNodes(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNodeBuildSettings& Settings,
		int32 NodeIndex,
		TConstArrayView<FRayRopeNode> CurrentNodes,
		TConstArrayView<FRayRopeNode> ReferenceNodes,
		FRayRopeBuiltNodeBuffer& OutNodes);

	static bool BuildNodesForSpanTransition(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNodeBuildSettings& Settings,
		const FRayRopeSpan& CurrentSpan,
		const FRayRopeSpan& ReferenceSpan,
		FRayRopeBuiltNodeBuffer& OutNodes);
};
