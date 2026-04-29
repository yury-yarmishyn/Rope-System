#include "RayRopeTopology.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Solvers/RayRopeNodeResolver.h"
#include "RayRopeTrace.h"
#include "RayRopeWrapGeometry.h"

bool FRayRopeTopology::TryBuildBaseSegments(
	const FRayRopeTraceSettings& TraceSettings,
	const TArray<AActor*>& AnchorActors,
	TArray<FRayRopeSegment>& OutSegments)
{
	OutSegments.Reset();

	if (!IsValid(TraceSettings.World) || AnchorActors.Num() < 2)
	{
		return false;
	}

	OutSegments.Reserve(AnchorActors.Num() - 1);

	for (int32 AnchorIndex = 0; AnchorIndex < AnchorActors.Num() - 1; ++AnchorIndex)
	{
		AActor* StartAnchorActor = AnchorActors[AnchorIndex];
		AActor* EndAnchorActor = AnchorActors[AnchorIndex + 1];
		if (!IsValid(StartAnchorActor) ||
			!IsValid(EndAnchorActor) ||
			StartAnchorActor == EndAnchorActor)
		{
			OutSegments.Reset();
			return false;
		}

		FRayRopeSegment BaseSegment;
		BaseSegment.Nodes.Reserve(2);
		BaseSegment.Nodes.Add(FRayRopeNodeResolver::CreateAnchorNode(StartAnchorActor));
		BaseSegment.Nodes.Add(FRayRopeNodeResolver::CreateAnchorNode(EndAnchorActor));

		FRayRopeSpan BaseSpan;
		if (!TryGetSegmentSpan(BaseSegment, 0, BaseSpan) ||
			BaseSpan.IsDegenerate(KINDA_SMALL_NUMBER))
		{
			OutSegments.Reset();
			return false;
		}

		const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
			TraceSettings,
			FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeStartTrace)));

		FHitResult SurfaceHit;
		if (FRayRopeTrace::TryTraceSpan(TraceContext, BaseSpan, SurfaceHit))
		{
			OutSegments.Reset();
			return false;
		}

		OutSegments.Add(MoveTemp(BaseSegment));
	}

	return OutSegments.Num() > 0;
}

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

float FRayRopeTopology::CalculateRopeLength(const TArray<FRayRopeSegment>& Segments)
{
	float TotalLength = 0.f;

	for (const FRayRopeSegment& Segment : Segments)
	{
		for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
		{
			TotalLength += FVector::Dist(
				Segment.Nodes[NodeIndex - 1].WorldLocation,
				Segment.Nodes[NodeIndex].WorldLocation);
		}
	}

	return TotalLength;
}

void FRayRopeTopology::RelaxSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeRelaxSettings& RelaxSettings,
	FRayRopeSegment& Segment)
{
	if (Segment.Nodes.Num() < 3)
	{
		return;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeRelaxTrace)));

	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num() - 1;)
	{
		const FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
		if (CurrentNode.NodeType != ENodeType::Redirect)
		{
			++NodeIndex;
			continue;
		}

		const FRayRopeNode& PrevNode = Segment.Nodes[NodeIndex - 1];
		const FRayRopeNode& NextNode = Segment.Nodes[NodeIndex + 1];
		if (CanRemoveRelaxNode(
			TraceContext,
			RelaxSettings,
			PrevNode,
			CurrentNode,
			NextNode))
		{
			Segment.Nodes.RemoveAt(NodeIndex, 1, EAllowShrinking::No);
			continue;
		}

		++NodeIndex;
	}
}

bool FRayRopeTopology::CanRemoveRelaxNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeRelaxSettings& RelaxSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode)
{
	FHitResult SurfaceHit;
	const FRayRopeSpan ShortcutSpan{&PrevNode, &NextNode};
	if (FRayRopeTrace::TryTraceSpan(TraceContext, ShortcutSpan, SurfaceHit))
	{
		return false;
	}

	const FVector FirstDirection = CurrentNode.WorldLocation - PrevNode.WorldLocation;
	const FVector SecondDirection = NextNode.WorldLocation - CurrentNode.WorldLocation;
	const bool bHasDegenerateBend =
		FirstDirection.IsNearlyZero(RelaxSettings.RelaxSolverEpsilon) ||
		SecondDirection.IsNearlyZero(RelaxSettings.RelaxSolverEpsilon);

	const bool bHasCollinearBend =
		FRayRopeWrapGeometry::AreDirectionsNearlyCollinear(
			FirstDirection,
			SecondDirection,
			RelaxSettings.RelaxCollinearEpsilon);

	if (bHasDegenerateBend || bHasCollinearBend)
	{
		return true;
	}

	const FVector ShortcutDirection = NextNode.WorldLocation - PrevNode.WorldLocation;
	const float ShortcutLengthSquared = ShortcutDirection.SizeSquared();
	const float T = ShortcutLengthSquared <= KINDA_SMALL_NUMBER
		? 0.f
		: FVector::DotProduct(
			CurrentNode.WorldLocation - PrevNode.WorldLocation,
			ShortcutDirection) / ShortcutLengthSquared;

	const FVector ClosestPoint = PrevNode.WorldLocation + ShortcutDirection * T;
	return CurrentNode.WorldLocation.Equals(ClosestPoint, RelaxSettings.RelaxSolverEpsilon) ||
		!FRayRopeTrace::TryTraceBlockingHit(
			TraceContext,
			CurrentNode.WorldLocation,
			ClosestPoint,
			SurfaceHit);
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
