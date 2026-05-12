#include "RayRopeWrapSolver.h"

namespace
{
void QueueWrapInsertionsForSpan(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& WrapSettings,
	FRayRopeSegment& Segment,
	TConstArrayView<FRayRopeNode> ReferenceNodes,
	int32 NodeIndex,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions)
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
		return;
	}

	const int32 InsertIndex = NodeIndex + 1;
	if (!FRayRopeNodeInsertionQueue::CanInsertNodesInSegment(
		WrapSettings,
		InsertIndex,
		Segment,
		NewNodes,
		PendingInsertions))
	{
		return;
	}

	FRayRopeNodeInsertionQueue::AppendPendingInsertions(
		InsertIndex,
		NewNodes,
		PendingInsertions);
}
}

FRayRopeSolveResult FRayRopeWrapSolver::WrapSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeNodeBuildSettings& WrapSettings,
	FRayRopeSegment& Segment,
	TConstArrayView<FRayRopeNode> ReferenceNodes)
{
	FRayRopeSolveResult Result;
	const int32 NodeCount = Segment.Nodes.Num();
	if (NodeCount != ReferenceNodes.Num() || NodeCount < 2)
	{
		return Result;
	}

	Result.AddAffectedSpanRange(0, NodeCount - 2);
	return WrapSegmentRanges(
		TraceSettings,
		WrapSettings,
		Segment,
		ReferenceNodes,
		Result.AffectedSpanRanges);
}

FRayRopeSolveResult FRayRopeWrapSolver::WrapSegmentRanges(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeNodeBuildSettings& WrapSettings,
	FRayRopeSegment& Segment,
	TConstArrayView<FRayRopeNode> ReferenceNodes,
	TConstArrayView<FRayRopeSpanIndexRange> SpanRanges)
{
	FRayRopeSolveResult Result;
	const int32 NodeCount = Segment.Nodes.Num();
	if (NodeCount != ReferenceNodes.Num() || NodeCount < 2 || SpanRanges.Num() == 0)
	{
		return Result;
	}

	const FRayRopeAffectedSpanRangeBuffer MergedRanges =
		FRayRopeSpanIndexRangeUtils::BuildMergedRanges(SpanRanges, NodeCount - 2);
	if (MergedRanges.Num() == 0)
	{
		return Result;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeTrace)));

	FRayRopePendingNodeInsertionBuffer PendingInsertions;
	PendingInsertions.Reserve((NodeCount - 1) * 2);

	for (const FRayRopeSpanIndexRange& Range : MergedRanges)
	{
		for (int32 NodeIndex = Range.FirstSpanIndex;
			NodeIndex <= Range.LastSpanIndex;
			++NodeIndex)
		{
			QueueWrapInsertionsForSpan(
				TraceContext,
				WrapSettings,
				Segment,
				ReferenceNodes,
				NodeIndex,
				PendingInsertions);
		}
	}

	const int32 AppliedInsertionCount =
		FRayRopeNodeInsertionQueue::ApplyPendingInsertions(Segment, PendingInsertions);
	if (AppliedInsertionCount > 0)
	{
		Result.MarkTopologyChanged();
		for (const FRayRopeSpanIndexRange& Range : MergedRanges)
		{
			Result.AddAffectedSpanRange(
				Range.FirstSpanIndex,
				Range.LastSpanIndex + AppliedInsertionCount);
		}
	}

	return Result;
}
