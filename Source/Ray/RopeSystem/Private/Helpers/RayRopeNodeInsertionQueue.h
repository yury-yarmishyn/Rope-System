#pragma once

#include "RayRopeTrace.h"

using FRayRopeBuiltNodeBuffer = TArray<FRayRopeNode, TInlineAllocator<2>>;
using FRayRopePendingNodeInsertionBuffer = TArray<TPair<int32, FRayRopeNode>, TInlineAllocator<8>>;

struct FRayRopeNodeBuildSettings;

struct FRayRopeNodeInsertionQueue
{
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

private:
	static bool AreEquivalentNodes(
		const FRayRopeNodeBuildSettings& Settings,
		const FRayRopeNode& FirstNode,
		const FRayRopeNode& SecondNode);
};
