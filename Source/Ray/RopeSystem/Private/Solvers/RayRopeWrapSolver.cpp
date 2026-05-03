#include "RayRopeWrapSolver.h"

void FRayRopeWrapSolver::WrapSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeWrapSettings& WrapSettings,
	FRayRopeSegment& Segment,
	TConstArrayView<FRayRopeNode> ReferenceNodes)
{
	ensureMsgf(
		Segment.Nodes.Num() == ReferenceNodes.Num(),
		TEXT("WrapSegment expects current and reference segments with matching node counts."));

	const int32 ComparableNodeCount =
		FMath::Min(Segment.Nodes.Num(), ReferenceNodes.Num());

	if (ComparableNodeCount < 2)
	{
		return;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeTrace)));

	FRayRopePendingNodeInsertionBuffer PendingInsertions;
	PendingInsertions.Reserve((ComparableNodeCount - 1) * 2);

	for (int32 NodeIndex = 0; NodeIndex < ComparableNodeCount - 1; ++NodeIndex)
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
		if (!FRayRopeNodeBuilder::CanInsertNodesInSegment(
			WrapSettings,
			InsertIndex,
			Segment,
			NewNodes,
			PendingInsertions))
		{
			continue;
		}

		FRayRopeNodeBuilder::AppendPendingInsertions(
			InsertIndex,
			NewNodes,
			PendingInsertions);
	}

	FRayRopeNodeBuilder::ApplyPendingInsertions(Segment, PendingInsertions);
}
