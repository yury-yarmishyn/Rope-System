#include "RayRopeSegmentTopology.h"

namespace
{
void SplitSegmentOnAnchors(TArray<FRayRopeSegment>& Segments, int32 SegmentIndex)
{
	if (!Segments.IsValidIndex(SegmentIndex))
	{
		return;
	}

	const TArray<FRayRopeNode>& Nodes = Segments[SegmentIndex].Nodes;
	const int32 NodeCount = Nodes.Num();
	if (NodeCount < 3)
	{
		return;
	}

	int32 InternalAnchorCount = 0;
	for (int32 NodeIndex = 1; NodeIndex < NodeCount - 1; ++NodeIndex)
	{
		if (Nodes[NodeIndex].NodeType == ERayRopeNodeType::Anchor)
		{
			++InternalAnchorCount;
		}
	}

	if (InternalAnchorCount == 0)
	{
		return;
	}

	TArray<FRayRopeSegment> SplitSegments;
	SplitSegments.Reserve(InternalAnchorCount + 1);
	int32 StartIndex = 0;
	for (int32 NodeIndex = 1; NodeIndex < NodeCount; ++NodeIndex)
	{
		// Each internal anchor becomes both the end of the previous segment and the start of the next.
		const bool bShouldSplit =
			NodeIndex == NodeCount - 1 ||
			Nodes[NodeIndex].NodeType == ERayRopeNodeType::Anchor;

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

	Segments.RemoveAt(SegmentIndex, 1, EAllowShrinking::No);
	Segments.Reserve(Segments.Num() + SplitSegments.Num());
	Segments.InsertDefaulted(SegmentIndex, SplitSegments.Num());
	for (int32 SplitIndex = 0; SplitIndex < SplitSegments.Num(); ++SplitIndex)
	{
		Segments[SegmentIndex + SplitIndex] = MoveTemp(SplitSegments[SplitIndex]);
	}
}
}

bool FRayRopeSegmentTopology::TryGetSegmentSpan(
	const FRayRopeSegment& Segment,
	int32 NodeIndex,
	FRayRopeSpan& OutSpan)
{
	return TryGetNodeSpan(Segment.Nodes, NodeIndex, OutSpan);
}

bool FRayRopeSegmentTopology::TryGetNodeSpan(
	TConstArrayView<FRayRopeNode> Nodes,
	int32 NodeIndex,
	FRayRopeSpan& OutSpan)
{
	OutSpan = FRayRopeSpan();

	if (NodeIndex < 0 || NodeIndex + 1 >= Nodes.Num())
	{
		return false;
	}

	OutSpan = FRayRopeSpan{
		&Nodes[NodeIndex],
		&Nodes[NodeIndex + 1]
	};
	return true;
}

bool FRayRopeSegmentTopology::HasNodeType(
	const FRayRopeSegment& Segment,
	ERayRopeNodeType NodeType)
{
	for (const FRayRopeNode& Node : Segment.Nodes)
	{
		if (Node.NodeType == NodeType)
		{
			return true;
		}
	}

	return false;
}

int32 FRayRopeSegmentTopology::CountNodesOfType(
	const FRayRopeSegment& Segment,
	ERayRopeNodeType NodeType)
{
	int32 NodeCount = 0;
	for (const FRayRopeNode& Node : Segment.Nodes)
	{
		if (Node.NodeType == NodeType)
		{
			++NodeCount;
		}
	}

	return NodeCount;
}

bool FRayRopeSegmentTopology::HasRedirectNodes(const FRayRopeSegment& Segment)
{
	return HasNodeType(Segment, ERayRopeNodeType::Redirect);
}

int32 FRayRopeSegmentTopology::CountRedirectNodes(const FRayRopeSegment& Segment)
{
	return CountNodesOfType(Segment, ERayRopeNodeType::Redirect);
}

float FRayRopeSegmentTopology::CalculateSegmentLength(const FRayRopeSegment& Segment)
{
	float SegmentLength = 0.f;

	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
	{
		SegmentLength += FVector::Dist(
			Segment.Nodes[NodeIndex - 1].WorldLocation,
			Segment.Nodes[NodeIndex].WorldLocation);
	}

	return SegmentLength;
}

float FRayRopeSegmentTopology::CalculateRopeLength(const TArray<FRayRopeSegment>& Segments)
{
	float TotalLength = 0.f;

	for (const FRayRopeSegment& Segment : Segments)
	{
		TotalLength += CalculateSegmentLength(Segment);
	}

	return TotalLength;
}

void FRayRopeSegmentTopology::SplitSegmentsOnAnchors(TArray<FRayRopeSegment>& Segments)
{
	for (int32 SegmentIndex = Segments.Num() - 1; SegmentIndex >= 0; --SegmentIndex)
	{
		SplitSegmentOnAnchors(Segments, SegmentIndex);
	}
}
