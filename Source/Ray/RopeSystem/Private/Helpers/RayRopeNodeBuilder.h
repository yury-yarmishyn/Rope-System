#pragma once

#include "RayRopeTrace.h"

struct FRayRopeNodeBuildSettings
{
	bool bAllowWrapOnMovableObjects = false;
	int32 MaxWrapBinarySearchIterations = 0;
	float WrapSolverTolerance = 0.f;
	float GeometryCollinearityTolerance = 0.f;
	float WrapSurfaceOffset = 0.f;
};

using FRayRopeBuiltNodeBuffer = TArray<FRayRopeNode, TInlineAllocator<2>>;
using FRayRopePendingNodeInsertionBuffer = TArray<TPair<int32, FRayRopeNode>, TInlineAllocator<8>>;

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

	static bool CanInsertNodes(
		const FRayRopeNodeBuildSettings& Settings,
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& NextNode,
		int32 InsertIndex,
		TConstArrayView<FRayRopeNode> Candidates,
		const FRayRopePendingNodeInsertionBuffer& PendingInsertions);

	static bool CanInsertNodesInSegment(
		const FRayRopeNodeBuildSettings& Settings,
		int32 InsertIndex,
		const FRayRopeSegment& Segment,
		TConstArrayView<FRayRopeNode> Candidates,
		const FRayRopePendingNodeInsertionBuffer& PendingInsertions);

	static void AppendPendingInsertions(
		int32 InsertIndex,
		FRayRopeBuiltNodeBuffer& Nodes,
		FRayRopePendingNodeInsertionBuffer& PendingInsertions);

	static void ApplyPendingInsertions(
		FRayRopeSegment& Segment,
		FRayRopePendingNodeInsertionBuffer& PendingInsertions);

	static bool AreEquivalentNodes(
		const FRayRopeNodeBuildSettings& Settings,
		const FRayRopeNode& FirstNode,
		const FRayRopeNode& SecondNode);
};
