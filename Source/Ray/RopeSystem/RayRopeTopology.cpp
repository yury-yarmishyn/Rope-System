#include "RayRopeTopology.h"

bool FRayRopeTopology::TryGetSegmentSpan(
	const FRayRopeSegment& Segment,
	int32 NodeIndex,
	FRayRopeSpan& OutSpan)
{
	OutSpan = FRayRopeSpan();

	if (!Segment.Nodes.IsValidIndex(NodeIndex) || !Segment.Nodes.IsValidIndex(NodeIndex + 1))
	{
		return false;
	}

	OutSpan = FRayRopeSpan{
		&Segment.Nodes[NodeIndex],
		&Segment.Nodes[NodeIndex + 1]
	};
	return true;
}

void FRayRopeTopology::SplitSegmentsOnAnchors(TArray<FRayRopeSegment>& Segments)
{
	for (int32 SegmentIndex = Segments.Num() - 1; SegmentIndex >= 0; --SegmentIndex)
	{
		SplitSegmentOnAnchors(Segments, SegmentIndex);
	}
}

void FRayRopeTopology::SplitSegmentOnAnchors(TArray<FRayRopeSegment>& Segments, int32 SegmentIndex)
{
	if (!Segments.IsValidIndex(SegmentIndex))
	{
		return;
	}

	const TArray<FRayRopeNode>& Nodes = Segments[SegmentIndex].Nodes;
	if (Nodes.Num() < 3)
	{
		return;
	}

	TArray<FRayRopeSegment> SplitSegments;
	int32 StartIndex = 0;
	for (int32 NodeIndex = 1; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		const bool bShouldSplit =
			NodeIndex == Nodes.Num() - 1 ||
			Nodes[NodeIndex].NodeType == ENodeType::Anchor;

		if (!bShouldSplit)
		{
			continue;
		}

		const int32 NewNodeCount = NodeIndex - StartIndex + 1;
		FRayRopeSegment NewSegment;
		NewSegment.Nodes.Reserve(NewNodeCount);
		NewSegment.Nodes.Append(&Nodes[StartIndex], NewNodeCount);
		SplitSegments.Add(MoveTemp(NewSegment));
		StartIndex = NodeIndex;
	}

	if (SplitSegments.Num() <= 1)
	{
		return;
	}

	Segments.RemoveAt(SegmentIndex, 1, EAllowShrinking::No);
	Segments.Reserve(Segments.Num() + SplitSegments.Num());

	for (int32 SplitIndex = 0; SplitIndex < SplitSegments.Num(); ++SplitIndex)
	{
		Segments.Insert(MoveTemp(SplitSegments[SplitIndex]), SegmentIndex + SplitIndex);
	}
}
