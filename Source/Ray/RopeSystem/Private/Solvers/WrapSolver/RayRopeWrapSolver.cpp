#include "RayRopeWrapSolver.h"

void FRayRopeWrapSolver::WrapSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeNodeBuildSettings& WrapSettings,
	FRayRopeSegment& Segment,
	TConstArrayView<FRayRopeNode> ReferenceNodes)
{
	const int32 NodeCount = Segment.Nodes.Num();
	if (NodeCount != ReferenceNodes.Num() || NodeCount < 2)
	{
		return;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeTrace)));

	FRayRopePendingNodeInsertionBuffer PendingInsertions;
	PendingInsertions.Reserve((NodeCount - 1) * 2);

	for (int32 NodeIndex = 0; NodeIndex < NodeCount - 1; ++NodeIndex)
	{
		FRayRopeBuiltNodeBuffer NewNodes;
		if (!FRayRopeNodeBuilder::BuildNodes(
			TraceContext,
			WrapSettings,
			NodeIndex,
			Segment.Nodes,
			ReferenceNodes,
			NewNodes))
		{
			continue;
		}

		const int32 InsertIndex = NodeIndex + 1;
		if (!FRayRopeNodeInsertionQueue::CanInsertNodesInSegment(
			WrapSettings,
			InsertIndex,
			Segment,
			NewNodes,
			PendingInsertions))
		{
			continue;
		}

		FRayRopeNodeInsertionQueue::AppendPendingInsertions(
			InsertIndex,
			NewNodes,
			PendingInsertions);
	}

	FRayRopeNodeInsertionQueue::ApplyPendingInsertions(Segment, PendingInsertions);
}

